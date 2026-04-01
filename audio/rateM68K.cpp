/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * This is a copy of RateConverter_Atari optimised with inline m68k assembly.
 * I found it the lesser evil in comparison to polluting the original file with
 * many #ifdefs.
 */

#include "audio/audiostream.h"
#include "audio/rate.h"
#include "audio/mixer.h"
#include "backends/platform/atari/atari-debug.h"
#include "common/config-manager.h"
#include "common/frac.h"
#include "common/util.h"

namespace Audio {

template<bool inStereo, bool outStereo, bool reverseStereo>
class RateConverter_Atari : public RateConverter {
private:
	/** Input and output rates */
	st_rate_t _inRate, _outRate;

	/**
	 * The intermediate input cache. Bigger values may increase performance,
	 * but only until some point (depends largely on cache size, target
	 * processor and various other factors), at which it will decrease again.
	 */
	int16 _buffer[512];

	/** Current position inside the buffer */
	const int16 *_bufferPos;

	/** Size of data currently loaded into the buffer */
	int _bufferSize;

	/** How far output is ahead of input when doing simple conversion */
	frac_t _outPos;

	/** Fractional position of the output stream in input stream unit */
	frac_t _outPosFrac;

	/** Last sample(s) in the input stream (left/right channel) */
	int16 _inLastL, _inLastR;

	/** Current sample(s) in the input stream (left/right channel) */
	int16 _inCurL, _inCurR;

	template<int maxOutputSamples, st_volume_t volL, st_volume_t volR>
	int commonConvert(AudioStream &input, int32 *outBuffer, st_size_t numSamples, st_volume_t volL_dm, st_volume_t volR_dm, int outputSamples);

	template<st_volume_t volL = static_cast<st_volume_t>(-1), st_volume_t volR = static_cast<st_volume_t>(-1)>
	int copyConvert(AudioStream &input, int32 *outBuffer, st_size_t numSamples, st_volume_t volL_dm, st_volume_t volR_dm);
	template<st_volume_t volL = static_cast<st_volume_t>(-1), st_volume_t volR = static_cast<st_volume_t>(-1)>
	int downsampleConvert(AudioStream &input, int32 *outBuffer, st_size_t numSamples, st_volume_t volL_dm, st_volume_t volR_dm);
	template<st_volume_t volL = static_cast<st_volume_t>(-1), st_volume_t volR = static_cast<st_volume_t>(-1)>
	int upsampleConvert(AudioStream &input, int32 *outBuffer, st_size_t numSamples, st_volume_t volL_dm, st_volume_t volR_dm);
	template<st_volume_t volL = static_cast<st_volume_t>(-1), st_volume_t volR = static_cast<st_volume_t>(-1)>
	int interpolateConvert(AudioStream &input, int32 *outBuffer, st_size_t numSamples, st_volume_t volL_dm, st_volume_t volR_dm);

	void printConvertType(const Common::String &name) {
		const Common::ConfigManager::Domain *activeDomain = ConfMan.getActiveDomain();
		if (activeDomain && ConfMan.getBool("print_rate")) {
			static st_rate_t previousInRate, previousOutRate;
			if (previousInRate != _inRate || previousOutRate != _outRate) {
				previousInRate = _inRate;
				previousOutRate = _outRate;
				atari_debug("RateConverter_Atari::%s[%s]: inRate %d Hz (%s) => outRate %d Hz (%s)",
							name.c_str(), activeDomain->getValOrDefault("gameid").c_str(),
							_inRate, inStereo ? "stereo" : "mono", _outRate, outStereo ? "stereo" : "mono");
			}
		}
	}

public:
	RateConverter_Atari(st_rate_t inputRate, st_rate_t outputRate);
	virtual ~RateConverter_Atari() {}

	int convert(AudioStream &input, byte *outBuffer, uint outBytesPerSample, st_size_t numSamples, st_volume_t vol_l, st_volume_t vol_r, MixMode mixMode) override;

	void setInputRate(st_rate_t inputRate) override { _inRate = inputRate; }
	void setOutputRate(st_rate_t outputRate) override { _outRate = outputRate; }

	st_rate_t getInputRate() const override { return _inRate; }
	st_rate_t getOutputRate() const override { return _outRate; }

	bool needsDraining() const override { return _bufferSize != 0; }
};

template<bool inStereo, bool outStereo, bool reverseStereo>
template<int maxOutputSamples, st_volume_t volL, st_volume_t volR>
int RateConverter_Atari<inStereo, outStereo, reverseStereo>::commonConvert(AudioStream &input, int32 *outBuffer, st_size_t numSamples, st_volume_t volL_dm, st_volume_t volR_dm, int outputSamples) {
	const int32 *outStart = outBuffer;
	const int32 *outEnd = outBuffer + numSamples * (outStereo ? 2 : 1);

	while (outBuffer < outEnd) {
		// Check if we have to refill the buffer
		if (_bufferSize == 0) {
			_bufferPos = _buffer;
			_bufferSize = input.readBuffer(_buffer, ARRAYSIZE(_buffer));

			if (_bufferSize <= 0)
				return (outBuffer - outStart) / (outStereo ? 2 : 1);
		}

		// Process as many samples as we can from the current buffer
		int count = MIN(
			_bufferSize / (inStereo ? 2 : 1),
			(int)(outEnd - outBuffer) / (outStereo ? 2 : 1) / outputSamples);
		_bufferSize -= count * (inStereo ? 2 : 1);

		if (volL | volR) {
			__asm__ volatile(
				"	subq.l	#1,%[count]\n"
				"1:\n"
				// inL = *_bufferPos++;
				// inR = (inStereo ? *_bufferPos++ : inL);
				".if %c[inStereo]\n"
				"	.if %c[volL] != 0\n"
				"		move.w	(%[_bufferPos])+,%%d0\n"
				"	.else\n"
				"		addq.l	#2,%[_bufferPos]\n"
				"	.endif\n"
				"	.if %c[volR] != 0\n"
				"		move.w	(%[_bufferPos])+,%%d1\n"
				"	.else\n"
				"		addq.l	#2,%[_bufferPos]\n"
				"	.endif\n"
				".else\n"
				"	.if %c[volL] != 0\n"
				"		move.w	(%[_bufferPos])+,%%d0\n"
				"		.if %c[volR] != 0\n"
				"			move.w	%%d0,%%d1\n"
				"		.endif\n"
				"	.else\n"
				"		move.w	(%[_bufferPos])+,%%d1\n"
				"	.endif\n"
				".endif\n"

				// outL = (inL * (int)volL) / Audio::Mixer::kMaxMixerVolume
				".if %c[volL] != 0\n"
				"	.if %c[volL] != %c[kMaxMixerVolume]\n"
				"		muls.w	%[volL_dm],%%d0\n"
				"		asr.l	#8,%%d0\n"
				"	.else\n"
				"		ext.l	%%d0\n"
				"	.endif\n"
				".endif\n"

				// outR = (inR * (int)volR) / Audio::Mixer::kMaxMixerVolume
				".if %c[volR] != 0\n"
				"	.if %c[volR] != %c[kMaxMixerVolume]\n"
				"		muls.w	%[volR_dm],%%d1\n"
				"		asr.l	#8,%%d1\n"
				"	.else\n"
				"		ext.l	%%d1\n"
				"	.endif\n"
				".endif\n"

				// Duplicate the sample outputSamples times
				".if %c[outStereo]\n"
				"	.if %c[maxOutputSamples] > 1\n"
				"		jmp	(2,%%pc,%[jumpOff].l*4)\n"
				"		.if %c[reverseStereo]\n"
				"			.if %c[volR] != 0 && %c[volL] != 0\n"
				"				add.l	%%d1,(%[outBuffer])+\n"	// case 8
				"				add.l	%%d0,(%[outBuffer])+\n"
				"				add.l	%%d1,(%[outBuffer])+\n"	// case 7
				"				add.l	%%d0,(%[outBuffer])+\n"
				"				add.l	%%d1,(%[outBuffer])+\n"	// case 6
				"				add.l	%%d0,(%[outBuffer])+\n"
				"				add.l	%%d1,(%[outBuffer])+\n"	// case 5
				"				add.l	%%d0,(%[outBuffer])+\n"
				"				add.l	%%d1,(%[outBuffer])+\n"	// case 4
				"				add.l	%%d0,(%[outBuffer])+\n"
				"				add.l	%%d1,(%[outBuffer])+\n"	// case 3
				"				add.l	%%d0,(%[outBuffer])+\n"
				"				add.l	%%d1,(%[outBuffer])+\n"	// case 2
				"				add.l	%%d0,(%[outBuffer])+\n"
				"			.elseif %c[volR] != 0\n"
				"				add.l	%%d1,(%[outBuffer])+\n"	// case 8
				"				addq.l	#4,%[outBuffer]\n"
				"				add.l	%%d1,(%[outBuffer])+\n"	// case 7
				"				addq.l	#4,%[outBuffer]\n"
				"				add.l	%%d1,(%[outBuffer])+\n"	// case 6
				"				addq.l	#4,%[outBuffer]\n"
				"				add.l	%%d1,(%[outBuffer])+\n"	// case 5
				"				addq.l	#4,%[outBuffer]\n"
				"				add.l	%%d1,(%[outBuffer])+\n"	// case 4
				"				addq.l	#4,%[outBuffer]\n"
				"				add.l	%%d1,(%[outBuffer])+\n"	// case 3
				"				addq.l	#4,%[outBuffer]\n"
				"				add.l	%%d1,(%[outBuffer])+\n"	// case 2
				"				addq.l	#4,%[outBuffer]\n"
				"			.elseif %c[volL] != 0\n"
				"				addq.l	#4,%[outBuffer]\n"		// case 8
				"				add.l	%%d0,(%[outBuffer])+\n"
				"				addq.l	#4,%[outBuffer]\n"		// case 7
				"				add.l	%%d0,(%[outBuffer])+\n"
				"				addq.l	#4,%[outBuffer]\n"		// case 6
				"				add.l	%%d0,(%[outBuffer])+\n"
				"				addq.l	#4,%[outBuffer]\n"		// case 5
				"				add.l	%%d0,(%[outBuffer])+\n"
				"				addq.l	#4,%[outBuffer]\n"		// case 4
				"				add.l	%%d0,(%[outBuffer])+\n"
				"				addq.l	#4,%[outBuffer]\n"		// case 3
				"				add.l	%%d0,(%[outBuffer])+\n"
				"				addq.l	#4,%[outBuffer]\n"		// case 2
				"				add.l	%%d0,(%[outBuffer])+\n"
				"			.endif\n"
				"		.else\n"
				"			.if %c[volL] != 0 && %c[volR] != 0\n"
				"				add.l	%%d0,(%[outBuffer])+\n"	// case 8
				"				add.l	%%d1,(%[outBuffer])+\n"
				"				add.l	%%d0,(%[outBuffer])+\n"	// case 7
				"				add.l	%%d1,(%[outBuffer])+\n"
				"				add.l	%%d0,(%[outBuffer])+\n"	// case 6
				"				add.l	%%d1,(%[outBuffer])+\n"
				"				add.l	%%d0,(%[outBuffer])+\n"	// case 5
				"				add.l	%%d1,(%[outBuffer])+\n"
				"				add.l	%%d0,(%[outBuffer])+\n"	// case 4
				"				add.l	%%d1,(%[outBuffer])+\n"
				"				add.l	%%d0,(%[outBuffer])+\n"	// case 3
				"				add.l	%%d1,(%[outBuffer])+\n"
				"				add.l	%%d0,(%[outBuffer])+\n"	// case 2
				"				add.l	%%d1,(%[outBuffer])+\n"
				"			.elseif %c[volL] != 0\n"
				"				add.l	%%d0,(%[outBuffer])+\n"	// case 8
				"				addq.l	#4,%[outBuffer]\n"
				"				add.l	%%d0,(%[outBuffer])+\n"	// case 7
				"				addq.l	#4,%[outBuffer]\n"
				"				add.l	%%d0,(%[outBuffer])+\n"	// case 6
				"				addq.l	#4,%[outBuffer]\n"
				"				add.l	%%d0,(%[outBuffer])+\n"	// case 5
				"				addq.l	#4,%[outBuffer]\n"
				"				add.l	%%d0,(%[outBuffer])+\n"	// case 4
				"				addq.l	#4,%[outBuffer]\n"
				"				add.l	%%d0,(%[outBuffer])+\n"	// case 3
				"				addq.l	#4,%[outBuffer]\n"
				"				add.l	%%d0,(%[outBuffer])+\n"	// case 2
				"				addq.l	#4,%[outBuffer]\n"
				"			.elseif %c[volR] != 0\n"
				"				addq.l	#4,%[outBuffer]\n"		// case 8
				"				add.l	%%d1,(%[outBuffer])+\n"
				"				addq.l	#4,%[outBuffer]\n"		// case 7
				"				add.l	%%d1,(%[outBuffer])+\n"
				"				addq.l	#4,%[outBuffer]\n"		// case 6
				"				add.l	%%d1,(%[outBuffer])+\n"
				"				addq.l	#4,%[outBuffer]\n"		// case 5
				"				add.l	%%d1,(%[outBuffer])+\n"
				"				addq.l	#4,%[outBuffer]\n"		// case 4
				"				add.l	%%d1,(%[outBuffer])+\n"
				"				addq.l	#4,%[outBuffer]\n"		// case 3
				"				add.l	%%d1,(%[outBuffer])+\n"
				"				addq.l	#4,%[outBuffer]\n"		// case 2
				"				add.l	%%d1,(%[outBuffer])+\n"
				"			.endif\n"
				"		.endif\n"
				"	.endif\n"
				// case 1 (always present)
				"	.if %c[reverseStereo]\n"
				// Output left channel (from right input when reversed)
				"		.if %c[volR] != 0\n"
				"			add.l	%%d1,(%[outBuffer])+\n"
				"		.else\n"
				"			addq.l	#4,%[outBuffer]\n"
				"		.endif\n"
				// Output right channel (from left input when reversed)
				"		.if %c[volL] != 0\n"
				"			add.l	%%d0,(%[outBuffer])+\n"
				"		.else\n"
				"			addq.l	#4,%[outBuffer]\n"
				"		.endif\n"
				"	.else\n"
				// Output left channel
				"		.if %c[volL] != 0\n"
				"			add.l	%%d0,(%[outBuffer])+\n"
				"		.else\n"
				"			addq.l	#4,%[outBuffer]\n"
				"		.endif\n"
				// Output right channel
				"		.if %c[volR] != 0\n"
				"			add.l	%%d1,(%[outBuffer])+\n"
				"		.else\n"
				"			addq.l	#4,%[outBuffer]\n"
				"		.endif\n"
				"	.endif\n"
				".else\n"
				// Output mono channel
				"	.if %c[volL] != 0 && %c[volR] != 0\n"
				"		add.l	%%d1,%%d0\n"
				"		asr.l	#1,%%d0\n"
				"	.elseif %c[volL] != 0\n"
				"		asr.l	#1,%%d0\n"
				"	.elseif %c[volR] != 0\n"
				"		move.l	%%d1,%%d0\n"
				"		asr.l	#1,%%d0\n"
				"	.endif\n"
				"	.if %c[maxOutputSamples] > 1\n"
				"		jmp	(2,%%pc,%[jumpOff].l*2)\n"
				"		add.l	%%d0,(%[outBuffer])+\n"	// case 8
				"		add.l	%%d0,(%[outBuffer])+\n"	// case 7
				"		add.l	%%d0,(%[outBuffer])+\n"	// case 6
				"		add.l	%%d0,(%[outBuffer])+\n"	// case 5
				"		add.l	%%d0,(%[outBuffer])+\n"	// case 4
				"		add.l	%%d0,(%[outBuffer])+\n"	// case 3
				"		add.l	%%d0,(%[outBuffer])+\n"	// case 2
				"	.endif\n"
				"	add.l	%%d0,(%[outBuffer])+\n"	// case 1
				".endif\n"

				"	dbra	%[count],1b\n"

				// outputs
				: [_bufferPos] "+a" (_bufferPos),
				  [outBuffer] "+a" (outBuffer),
				  [count] "+d" (count)
				// inputs
				: [volL] "i" (volL),
				  [volR] "i" (volR),
				  [volL_dm] "dm" ((int16)volL_dm),
				  [volR_dm] "dm" ((int16)volR_dm),
				  [jumpOff] "d" (maxOutputSamples - outputSamples),
				  [inStereo] "i" (inStereo ? 1 : 0),
				  [outStereo] "i" (outStereo ? 1 : 0),
				  [reverseStereo] "i" (reverseStereo ? 1 : 0),
				  [maxOutputSamples] "i" (maxOutputSamples),
				  [kMaxMixerVolume] "i" (Audio::Mixer::kMaxMixerVolume)
				// clobbered
				: "d0", "d1", "cc" AND_MEMORY
			);
		} else {
			_bufferPos += count * (inStereo ? 2 : 1);
			outBuffer += count * outputSamples * (outStereo ? 2 : 1);
		}
	}
	return (outBuffer - outStart) / (outStereo ? 2 : 1);
}

template<bool inStereo, bool outStereo, bool reverseStereo>
template<st_volume_t volL, st_volume_t volR>
int RateConverter_Atari<inStereo, outStereo, reverseStereo>::copyConvert(AudioStream &input, int32 *outBuffer, st_size_t numSamples, st_volume_t volL_dm, st_volume_t volR_dm) {
	printConvertType("copyConvert");

	return commonConvert<1, volL, volR>(input, outBuffer, numSamples, volL_dm, volR_dm, 1);
}

template<bool inStereo, bool outStereo, bool reverseStereo>
template<st_volume_t volL, st_volume_t volR>
int RateConverter_Atari<inStereo, outStereo, reverseStereo>::downsampleConvert(AudioStream &input, int32 *outBuffer, st_size_t numSamples, st_volume_t volL_dm, st_volume_t volR_dm) {
	printConvertType("downsampleConvert");

	// How much to increment _outPos by
	const frac_t outPos_inc = _inRate / _outRate;

	const int32 *outStart = outBuffer;
	const int32 *outEnd = outBuffer + numSamples * (outStereo ? 2 : 1);

	while (outBuffer < outEnd) {
		// Read enough input samples so that _outPos >= 0
		while (_outPos >= 0) {
			const int skip = MIN((int)_outPos + 1, _bufferSize / (inStereo ? 2 : 1));
			_bufferPos += skip * (inStereo ? 2 : 1);
			_bufferSize -= skip * (inStereo ? 2 : 1);
			_outPos -= skip;

			// Check if we have to refill the buffer
			if (_bufferSize == 0) {
				_bufferPos = _buffer;
				_bufferSize = input.readBuffer(_buffer, ARRAYSIZE(_buffer));

				if (_bufferSize <= 0)
					return (outBuffer - outStart) / (outStereo ? 2 : 1);
			}
		}

		// Batch process: each output sample reads 1 frame and skips outPos_inc-1
		int count = MIN(
			_bufferSize / (inStereo ? 2 : 1) / outPos_inc,
			(int)(outEnd - outBuffer) / (outStereo ? 2 : 1));

		_bufferSize -= count * outPos_inc * (inStereo ? 2 : 1);
		_outPos = outPos_inc - 1;

		// Byte stride remaining after reading one frame with auto-increment
		const int32 stride = (outPos_inc - 1) * (inStereo ? 2 : 1) * sizeof(int16);

		if (volL | volR) {
			__asm__ volatile(
				"	subq.l	#1,%[count]\n"
				"1:\n"
				// inL = *_bufferPos++;
				// inR = (inStereo ? *_bufferPos++ : inL);
				".if %c[inStereo]\n"
				"	.if %c[volL] != 0\n"
				"		move.w	(%[_bufferPos])+,%%d0\n"
				"	.else\n"
				"		addq.l	#2,%[_bufferPos]\n"
				"	.endif\n"
				"	.if %c[volR] != 0\n"
				"		move.w	(%[_bufferPos])+,%%d1\n"
				"	.else\n"
				"		addq.l	#2,%[_bufferPos]\n"
				"	.endif\n"
				".else\n"
				"	.if %c[volL] != 0\n"
				"		move.w	(%[_bufferPos])+,%%d0\n"
				"		.if %c[volR] != 0\n"
				"			move.w	%%d0,%%d1\n"
				"		.endif\n"
				"	.else\n"
				"		move.w	(%[_bufferPos])+,%%d1\n"
				"	.endif\n"
				".endif\n"

				// _bufferPos += (outPos_inc - 1) * (inStereo ? 2 : 1);
				"	adda.l	%[stride],%[_bufferPos]\n"

				// outL = (inL * (int)volL) / Audio::Mixer::kMaxMixerVolume
				".if %c[volL] != 0\n"
				"	.if %c[volL] != %c[kMaxMixerVolume]\n"
				"		muls.w	%[volL_dm],%%d0\n"
				"		asr.l	#8,%%d0\n"
				"	.else\n"
				"		ext.l	%%d0\n"
				"	.endif\n"
				".endif\n"

				// outR = (inR * (int)volR) / Audio::Mixer::kMaxMixerVolume
				".if %c[volR] != 0\n"
				"	.if %c[volR] != %c[kMaxMixerVolume]\n"
				"		muls.w	%[volR_dm],%%d1\n"
				"		asr.l	#8,%%d1\n"
				"	.else\n"
				"		ext.l	%%d1\n"
				"	.endif\n"
				".endif\n"

				".if %c[outStereo]\n"
				"	.if	%c[reverseStereo]\n"
				// Output left channel (from right input when reversed)
				"		.if %c[volR] != 0\n"
				"			add.l	%%d1,(%[outBuffer])+\n"
				"		.else\n"
				"			addq.l	#4,%[outBuffer]\n"
				"		.endif\n"
				// Output right channel (from left input when reversed)
				"		.if %c[volL] != 0\n"
				"			add.l	%%d0,(%[outBuffer])+\n"
				"		.else\n"
				"			addq.l	#4,%[outBuffer]\n"
				"		.endif\n"
				"	.else\n"
				// Output left channel
				"		.if %c[volL] != 0\n"
				"			add.l	%%d0,(%[outBuffer])+\n"
				"		.else\n"
				"			addq.l	#4,%[outBuffer]\n"
				"		.endif\n"
				// Output right channel
				"		.if %c[volR] != 0\n"
				"			add.l	%%d1,(%[outBuffer])+\n"
				"		.else\n"
				"			addq.l	#4,%[outBuffer]\n"
				"		.endif\n"
				"	.endif\n"
				".else\n"
				// Output mono channel
				"	.if %c[volL] != 0 && %c[volR] != 0\n"
				"		add.l	%%d1,%%d0\n"
				"		asr.l	#1,%%d0\n"
				"		add.l	%%d0,(%[outBuffer])+\n"
				"	.elseif %c[volL] != 0\n"
				"		asr.l	#1,%%d0\n"
				"		add.l	%%d0,(%[outBuffer])+\n"
				"	.elseif %c[volR] != 0\n"
				"		asr.l	#1,%%d1\n"
				"		add.l	%%d1,(%[outBuffer])+\n"
				"	.endif\n"
				".endif\n"

				"	dbra	%[count],1b\n"

				// outputs
				: [_bufferPos] "+a" (_bufferPos),
				  [outBuffer] "+a" (outBuffer),
				  [count] "+d" (count)
				// inputs
				: [volL] "i" (volL),
				  [volR] "i" (volR),
				  [volL_dm] "dm" ((int16)volL_dm),
				  [volR_dm] "dm" ((int16)volR_dm),
				  [stride] "a" (stride),
				  [inStereo] "i" (inStereo ? 1 : 0),
				  [outStereo] "i" (outStereo ? 1 : 0),
				  [reverseStereo] "i" (reverseStereo ? 1 : 0),
				  [kMaxMixerVolume] "i" (Audio::Mixer::kMaxMixerVolume)
				// clobbered
				: "d0", "d1", "cc" AND_MEMORY
			);
		} else {
			_bufferPos += count * outPos_inc * (inStereo ? 2 : 1);
			outBuffer += count * (outStereo ? 2 : 1);
		}
	}
	return (outBuffer - outStart) / (outStereo ? 2 : 1);
}

template<bool inStereo, bool outStereo, bool reverseStereo>
template<st_volume_t volL, st_volume_t volR>
int RateConverter_Atari<inStereo, outStereo, reverseStereo>::upsampleConvert(AudioStream &input, int32 *outBuffer, st_size_t numSamples, st_volume_t volL_dm, st_volume_t volR_dm) {
	printConvertType("upsampleConvert");

	return commonConvert<8, volL, volR>(input, outBuffer, numSamples, volL_dm, volR_dm, _outRate / _inRate);
}

template<bool inStereo, bool outStereo, bool reverseStereo>
template<st_volume_t volL, st_volume_t volR>
int RateConverter_Atari<inStereo, outStereo, reverseStereo>::interpolateConvert(AudioStream &input, int32 *outBuffer, st_size_t numSamples, st_volume_t volL_dm, st_volume_t volR_dm) {
	printConvertType("interpolateConvert");

	// How much to increment _outPosFrac by
	const frac_t outPos_inc = (_inRate << FRAC_BITS) / _outRate;

	const int32 *outStart = outBuffer;
	const int32 *outEnd = outBuffer + numSamples * (outStereo ? 2 : 1);

	while (outBuffer < outEnd) {
		// Read enough input samples so that _outPosFrac < 0
		while ((frac_t)FRAC_ONE <= _outPosFrac) {
			// Check if we have to refill the buffer
			if (_bufferSize == 0) {
				_bufferPos = _buffer;
				_bufferSize = input.readBuffer(_buffer, ARRAYSIZE(_buffer));

				if (_bufferSize <= 0)
					return (outBuffer - outStart) / (outStereo ? 2 : 1);
			}

			_bufferSize -= (inStereo ? 2 : 1);

			if (volL || (!inStereo && volR)) {
				_inLastL = _inCurL;
				_inCurL = *_bufferPos++;
			} else {
				_bufferPos++;
			}

			if (inStereo) {
				if (volR) {
					_inLastR = _inCurR;
					_inCurR = *_bufferPos++;
				} else {
					_bufferPos++;
				}
			}

			_outPosFrac -= FRAC_ONE;
		}

		// Loop as long as the _outPos trails behind, and as long as there is
		// still space in the output buffer.
		while (_outPosFrac < (frac_t)FRAC_ONE && outBuffer < outEnd) {
			if (volL | volR) {
				// Interpolate
				__asm__ volatile(
					".if %c[volL] != 0\n"
					// inL = (int16)(_inLastL + (((_inCurL - _inLastL) * _outPosFrac + FRAC_HALF) >> FRAC_BITS))
					"	move.l	%[_inCurL],%%d0\n"
					"	sub.l	%[_inLastL],%%d0\n"
					"	muls.l	%[_outPosFrac],%%d0\n"
					"	add.l	%[FRAC_HALF],%%d0\n"
					"	swap	%%d0\n"
					"	add.l	%[_inLastL],%%d0\n"
					".endif\n"

					".if %c[volR] != 0\n"
					// inR = (inStereo
					//			? (int16)(_inLastR + (((_inCurR - _inLastR) * _outPosFrac + FRAC_HALF) >> FRAC_BITS))
					//			: inL)
					"	.if %c[inStereo]\n"
					"		move.l	%[_inCurR],%%d1\n"
					"		sub.l	%[_inLastR],%%d1\n"
					"		muls.l	%[_outPosFrac],%%d1\n"
					"		add.l	%[FRAC_HALF],%%d1\n"
					"		swap	%%d1\n"
					"		add.l	%[_inLastR],%%d1\n"
					"	.else\n"
					"		.if %c[volL] != 0\n"
					"			move.w	%%d0,%%d1\n"
					"		.else\n"
					"			move.l	%[_inCurL],%%d1\n"
					"			sub.l	%[_inLastL],%%d1\n"
					"			muls.l	%[_outPosFrac],%%d1\n"
					"			add.l	%[FRAC_HALF],%%d1\n"
					"			swap	%%d1\n"
					"			add.l	%[_inLastL],%%d1\n"
					"		.endif\n"
					"	.endif\n"
					".endif\n"

					// outL = (inL * (int)volL) / Audio::Mixer::kMaxMixerVolume
					".if %c[volL] != 0\n"
					"	.if %c[volL] != %c[kMaxMixerVolume]\n"
					"		muls.w	%[volL_dm],%%d0\n"
					"		asr.l	#8,%%d0\n"
					"	.else\n"
					"		ext.l	%%d0\n"
					"	.endif\n"
					".endif\n"

					// outR = (inR * (int)volR) / Audio::Mixer::kMaxMixerVolume
					".if %c[volR] != 0\n"
					"	.if %c[volR] != %c[kMaxMixerVolume]\n"
					"		muls.w	%[volR_dm],%%d1\n"
					"		asr.l	#8,%%d1\n"
					"	.else\n"
					"		ext.l	%%d1\n"
					"	.endif\n"
					".endif\n"

					".if %c[outStereo]\n"
					"	.if %c[reverseStereo]\n"
					// Output left channel (from right input when reversed)
					"		.if %c[volR] != 0\n"
					"			add.l	%%d1,(%[outBuffer])+\n"
					"		.else\n"
					"			addq.l	#4,%[outBuffer]\n"
					"		.endif\n"
					// Output right channel (from left input when reversed)
					"		.if %c[volL] != 0\n"
					"			add.l	%%d0,(%[outBuffer])+\n"
					"		.else\n"
					"			addq.l	#4,%[outBuffer]\n"
					"		.endif\n"
					"	.else\n"
					// Output left channel
					"		.if %c[volL] != 0\n"
					"			add.l	%%d0,(%[outBuffer])+\n"
					"		.else\n"
					"			addq.l	#4,%[outBuffer]\n"
					"		.endif\n"
					// Output right channel
					"		.if %c[volR] != 0\n"
					"			add.l	%%d1,(%[outBuffer])+\n"
					"		.else\n"
					"			addq.l	#4,%[outBuffer]\n"
					"		.endif\n"
					"	.endif\n"
					".else\n"
					// Output mono channel
					"	.if %c[volL] != 0 && %c[volR] != 0\n"
					"		add.l	%%d1,%%d0\n"
					"		asr.l	#1,%%d0\n"
					"		add.l	%%d0,(%[outBuffer])+\n"
					"	.elseif %c[volL] != 0\n"
					"		asr.l	#1,%%d0\n"
					"		add.l	%%d0,(%[outBuffer])+\n"
					"	.else\n"
					"		asr.l	#1,%%d1\n"
					"		add.l	%%d1,(%[outBuffer])+\n"
					"	.endif\n"
					".endif\n"

					// outputs
					: [outBuffer] "+a" (outBuffer)
					// inputs
					: [volL] "i" (volL),
					  [volR] "i" (volR),
					  [volL_dm] "dm" ((int16)volL_dm),
					  [volR_dm] "dm" ((int16)volR_dm),
					  [_outPosFrac] "dm" (_outPosFrac),
					  [_inCurL] "dam" ((int32)_inCurL),
					  [_inLastL] "dam" ((int32)_inLastL),
					  [_inCurR] "dam" ((int32)_inCurR),
					  [_inLastR] "dam" ((int32)_inLastR),
					  [inStereo] "i" (inStereo ? 1 : 0),
					  [outStereo] "i" (outStereo ? 1 : 0),
					  [reverseStereo] "i" (reverseStereo ? 1 : 0),
					  [FRAC_HALF] "i" ((frac_t)FRAC_HALF),
					  [kMaxMixerVolume] "i" (Audio::Mixer::kMaxMixerVolume)
					// clobbered
					: "d0", "d1", "cc" AND_MEMORY
				);
			} else {
				outBuffer += (outStereo ? 2 : 1);
			}

			// Increment output position
			_outPosFrac += outPos_inc;
		}
	}
	return (outBuffer - outStart) / (outStereo ? 2 : 1);
}

template<bool inStereo, bool outStereo, bool reverseStereo>
RateConverter_Atari<inStereo, outStereo, reverseStereo>::RateConverter_Atari(st_rate_t inputRate, st_rate_t outputRate) :
	_inRate(inputRate),
	_outRate(outputRate),
	_outPos(1),
	_outPosFrac(FRAC_ONE),
	_inLastL(0),
	_inLastR(0),
	_inCurL(0),
	_inCurR(0),
	_bufferSize(0),
	_bufferPos(nullptr) {}

template<bool inStereo, bool outStereo, bool reverseStereo>
int RateConverter_Atari<inStereo, outStereo, reverseStereo>::convert(AudioStream &input, byte *outBuffer, uint outBytesPerSample, st_size_t numSamples, st_volume_t volL, st_volume_t volR, MixMode mixMode) {
	assert(mixMode == MIX_ADD);
	assert(outBytesPerSample == 4);
	assert(input.isStereo() == inStereo);
	assert(_inRate < 65536);
	assert(_outRate < 65536);

	constexpr auto kMax = Audio::Mixer::kMaxMixerVolume;

	if (_inRate == _outRate) {
		if (volL == 0 && volR == 0)
			return copyConvert<0, 0>(input, (int32 *)outBuffer, numSamples, volL, volR);
		else if (volL == 0 && volR == kMax)
			return copyConvert<0, kMax>(input, (int32 *)outBuffer, numSamples, volL, volR);
		else if (volL == kMax && volR == 0)
			return copyConvert<kMax, 0>(input, (int32 *)outBuffer, numSamples, volL, volR);
		else if (volL == kMax && volR == kMax)
			return copyConvert<kMax, kMax>(input, (int32 *)outBuffer, numSamples, volL, volR);
		else
			return copyConvert(input, (int32 *)outBuffer, numSamples, volL, volR);
	} else if ((_inRate % _outRate) == 0) {
		if (volL == 0 && volR == 0)
			return downsampleConvert<0, 0>(input, (int32 *)outBuffer, numSamples, volL, volR);
		else if (volL == 0 && volR == kMax)
			return downsampleConvert<0, kMax>(input, (int32 *)outBuffer, numSamples, volL, volR);
		else if (volL == kMax && volR == 0)
			return downsampleConvert<kMax, 0>(input, (int32 *)outBuffer, numSamples, volL, volR);
		else if (volL == kMax && volR == kMax)
			return downsampleConvert<kMax, kMax>(input, (int32 *)outBuffer, numSamples, volL, volR);
		else
			return downsampleConvert(input, (int32 *)outBuffer, numSamples, volL, volR);
	} else if ((_outRate % _inRate) == 0 && (_outRate / _inRate) <= 8) {
		if (volL == 0 && volR == 0)
			return upsampleConvert<0, 0>(input, (int32 *)outBuffer, numSamples, volL, volR);
		else if (volL == 0 && volR == kMax)
			return upsampleConvert<0, kMax>(input, (int32 *)outBuffer, numSamples, volL, volR);
		else if (volL == kMax && volR == 0)
			return upsampleConvert<kMax, 0>(input, (int32 *)outBuffer, numSamples, volL, volR);
		else if (volL == kMax && volR == kMax)
			return upsampleConvert<kMax, kMax>(input, (int32 *)outBuffer, numSamples, volL, volR);
		else
			return upsampleConvert(input, (int32 *)outBuffer, numSamples, volL, volR);
	} else {
		if (volL == 0 && volR == 0)
			return interpolateConvert<0, 0>(input, (int32 *)outBuffer, numSamples, volL, volR);
		else if (volL == 0 && volR == kMax)
			return interpolateConvert<0, kMax>(input, (int32 *)outBuffer, numSamples, volL, volR);
		else if (volL == kMax && volR == 0)
			return interpolateConvert<kMax, 0>(input, (int32 *)outBuffer, numSamples, volL, volR);
		else if (volL == kMax && volR == kMax)
			return interpolateConvert<kMax, kMax>(input, (int32 *)outBuffer, numSamples, volL, volR);
		else
			return interpolateConvert(input, (int32 *)outBuffer, numSamples, volL, volR);
	}
}

RateConverter *makeRateConverter(st_rate_t inRate, st_rate_t outRate, bool inStereo, bool outStereo, bool reverseStereo) {
	if (inStereo) {
		if (outStereo) {
			if (reverseStereo)
				return new RateConverter_Atari<true, true, true>(inRate, outRate);
			else
				return new RateConverter_Atari<true, true, false>(inRate, outRate);
		} else
			return new RateConverter_Atari<true, false, false>(inRate, outRate);
	} else {
		if (outStereo) {
			return new RateConverter_Atari<false, true, false>(inRate, outRate);
		} else
			return new RateConverter_Atari<false, false, false>(inRate, outRate);
	}
}

} // End of namespace Audio
