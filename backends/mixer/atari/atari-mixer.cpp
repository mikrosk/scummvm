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

#define FORCE_TEXT_CONSOLE

#include "backends/mixer/atari/atari-mixer.h"

#include <mint/falcon.h>
#include <mint/osbind.h>
#include <mint/ostruct.h>
#include <usound.h>

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/textconsole.h"

#ifdef DISABLE_FANCY_THEMES
#define DEFAULT_OUTPUT_RATE			11025
#define DEFAULT_OUTPUT_CHANNELS		1
#else
#define DEFAULT_OUTPUT_RATE			22050
#define DEFAULT_OUTPUT_CHANNELS		2
#endif

#define DEFAULT_SAMPLES 2048	// 83ms

void AtariAudioShutdown() {
	AtariSoundSetupDeinitXbios();
}

AtariMixerManager::AtariMixerManager() : MixerManager() {
	debug("AtariMixerManager()");

	suspendAudio();

	ConfMan.registerDefault("output_rate", DEFAULT_OUTPUT_RATE);
	ConfMan.registerDefault("output_channels", DEFAULT_OUTPUT_CHANNELS);
	ConfMan.registerDefault("audio_buffer_size", DEFAULT_SAMPLES);

	g_system->getEventManager()->getEventDispatcher()->registerObserver(this, 10, false);
}

AtariMixerManager::~AtariMixerManager() {
	debug("~AtariMixerManager()");

	g_system->getEventManager()->getEventDispatcher()->unregisterObserver(this);

	deinit();
}

void AtariMixerManager::init() {
	debug("audio init");

	assert(!_mixer);

	// read either from game domain or from defaults
	// but never write back so 22050 Hz will stay even on TT/stock Falcon
	_outputRate = ConfMan.getInt("output_rate");
	if (_outputRate <= 0)
		_outputRate = DEFAULT_OUTPUT_RATE;

	_outputChannels = ConfMan.getInt("output_channels");
	if (_outputChannels <= 0 || _outputChannels > 2)
		_outputChannels = DEFAULT_OUTPUT_CHANNELS;

	_samples = ConfMan.getInt("audio_buffer_size");
	if (_samples <= 0)
		_samples = DEFAULT_SAMPLES;

	AudioSpec desired, obtained;

	desired.frequency = _outputRate;
	desired.channels = _outputChannels;
	desired.format = AudioFormatSigned16MSB;
	desired.samples = _samples;

	if (!AtariSoundSetupInitXbios(&desired, &obtained)) {
		error("Sound system is not available");
	}

	if (obtained.format != AudioFormatSigned8 && obtained.format != AudioFormatSigned16MSB) {
		error("Sound system currently supports only 8/16-bit signed big endian samples");
	}

	// don't use the recommended number of samples
	obtained.size = obtained.size * desired.samples / obtained.samples;
	obtained.samples = desired.samples;

	_outputRate = obtained.frequency;
	if (desired.channels == 1 && obtained.channels == 2 && obtained.format == AudioFormatSigned16MSB) {
		_outputChannels = 1;
		_emulated16bitMono = true;
	} else {
		_outputChannels = obtained.channels;
		_emulated16bitMono = false;
	}
	_downsample = (obtained.format == AudioFormatSigned8);
	_samples = obtained.samples;

	debug("setting %d Hz mixing frequency, %d-bit, %s",
		_outputRate,
		obtained.format == AudioFormatSigned8 ? 8 : 16,
		_outputChannels == 2
			? "stereo"
			: _emulated16bitMono
				? "mono (emulated)"
				: "mono");
	debug("audio buffer size: %d", _samples);

	_atariSampleBufferSize = obtained.size * 2;	// two buffers
	_atariSampleBuffer = (byte*)Mxalloc(_atariSampleBufferSize, MX_STRAM);
	if (!_atariSampleBuffer) {
		_atariSampleBufferSize = 0;
		error("Failed to allocate memory in ST RAM");
	}

	_sampleBufferSize = _samples * _outputChannels * 4;	// always 32-bit
	_sampleBuffer = new uint8[_sampleBufferSize];

	_mixer = new Audio::MixerImpl(_outputRate, _outputChannels == 2, _samples, 4, false);
	_mixer->setReady(true);

	resumeAudio();
}

void AtariMixerManager::deinit() {
	debug("audio deinit");

	suspendAudio();

	AtariAudioShutdown();

	delete _mixer;
	_mixer = nullptr;

	Mfree(_atariSampleBuffer);
	_atariSampleBuffer = nullptr;
	_atariSampleBufferSize = 0;

	delete[] _sampleBuffer;
	_sampleBuffer = nullptr;
	_sampleBufferSize = 0;
	_samples = 0;
}

void AtariMixerManager::suspendAudio() {
	debug("suspendAudio");

	Buffoper(0x00);
	_playbackState = kPlaybackStopped;
	_audioSuspended = true;
}

int AtariMixerManager::resumeAudio() {
	debug("resumeAudio");

	_audioSuspended = false;
	update();
	return 0;
}

bool AtariMixerManager::notifyEvent(const Common::Event &event) {
	switch (event.type) {
	case Common::EVENT_QUIT:
	case Common::EVENT_RETURN_TO_LAUNCHER:
		if (_playbackState != kPlaybackStopped) {
			Buffoper(0x00);
			_playbackState = kPlaybackStopped;
			debug("silencing the mixer");
		}
		return false;
	default:
		break;
	}

	return false;
}

void AtariMixerManager::update() {
	if (_audioSuspended) {
		return;
	}

	assert(_mixer);

	if (_playbackState == kPlaybackStopped) {
		memset(_atariSampleBuffer, 0, _atariSampleBufferSize);
		Setbuffer(SR_PLAY, _atariSampleBuffer, _atariSampleBuffer + _atariSampleBufferSize);
		Buffoper(SB_PLA_ENA | SB_PLA_RPT);
		_playbackState = kWriteTo2ndHalf;
	}

	SndBufPtr sPtr;
	if (Buffptr(&sPtr) != 0) {
		warning("Buffptr() failed");
		return;
	}

	byte *atariSampleBuffer1stHalf = _atariSampleBuffer;
	byte *atariSampleBuffer2ndHalf = _atariSampleBuffer + _atariSampleBufferSize/2;
	byte *buf = nullptr;
	if (_playbackState == kWriteTo2ndHalf) {
		if ((byte *)sPtr.play < atariSampleBuffer2ndHalf) {
			buf = atariSampleBuffer2ndHalf;
			_playbackState = kWriteTo1stHalf;
		}
	} else if (_playbackState == kWriteTo1stHalf) {
		if ((byte *)sPtr.play >= atariSampleBuffer2ndHalf) {
			buf = atariSampleBuffer1stHalf;
			_playbackState = kWriteTo2ndHalf;
		}
	}
	if (!buf)
		return;

	int processed = _mixer->mixCallback(_sampleBuffer, _sampleBufferSize);

	// WARNING: loopCount, src and dst are modified by the asm code
	int loopCount = processed * _outputChannels;
	const byte *src = _sampleBuffer;
	byte *dst = buf;

	if (_downsample) {
		__asm__ volatile(
			"	subq.l	#1,%0\n"
			"	bmi.b	4f\n"
			"	move.l	#32768,%%d2\n"
			"	move.l	#65535,%%d3\n"
			"1:	move.l	(%1)+,%%d0\n"
			"	move.l	%%d0,%%d1\n"
			"	add.l	%%d2,%%d1\n"
			"	cmp.l	%%d3,%%d1\n"
			"	bhi.b	3f\n"
			"2:	asr.l	#8,%%d0\n"	// TODO: tweak (there were reports that >> 8 is too quiet)
			"	move.b	%%d0,(%2)+\n"
			"	dbra	%0,1b\n"
			"	bra.b	4f\n"
			"3:	tst.l	%%d0\n"
			"	spl		%%d0\n"
			"	ext.w	%%d0\n"
			"	add.w	%%d2,%%d0\n"
			"	bra.b	2b\n"
			"4:\n"
			: "+d"(loopCount), "+a"(src), "+a"(dst) // outputs
			: // inputs
			: "d0", "d1", "d2", "d3", "cc" AND_MEMORY
		);
		memset(buf + processed * _outputChannels * 2, 0, (_samples - processed) * _outputChannels * 2/2);
	} else {
		int bufferSize = processed * _outputChannels * 2;

		if (!_emulated16bitMono) {
			__asm__ volatile(
				"	subq.l	#1,%0\n"
				"	bmi.b	4f\n"
				"	move.l	#32768,%%d2\n"
				"	move.l	#65535,%%d3\n"
				"1:	move.l	(%1)+,%%d0\n"
				"	move.l	%%d0,%%d1\n"
				"	add.l	%%d2,%%d1\n"
				"	cmp.l	%%d3,%%d1\n"
				"	bhi.b	3f\n"
				"2:	move.w	%%d0,(%2)+\n"
				"	dbra	%0,1b\n"
				"	bra.b	4f\n"
				"3:	tst.l	%%d0\n"
				"	spl		%%d0\n"
				"	ext.w	%%d0\n"
				"	add.w	%%d2,%%d0\n"
				"	bra.b	2b\n"
				"4:\n"
				: "+d"(loopCount), "+a"(src), "+a"(dst) // outputs
				: // inputs
				: "d0", "d1", "d2", "d3", "cc" AND_MEMORY
			);
		} else {
			bufferSize *= 2;

			__asm__ volatile(
				"	subq.l	#1,%0\n"
				"	bmi.b	4f\n"
				"	move.l	#32768,%%d2\n"
				"	move.l	#65535,%%d3\n"
				"1:	move.l	(%1)+,%%d0\n"
				"	move.l	%%d0,%%d1\n"
				"	add.l	%%d2,%%d1\n"
				"	cmp.l	%%d3,%%d1\n"
				"	bhi.b	3f\n"
				"2:	move.w	%%d0,(%2)+\n"
				"	move.w	%%d0,(%2)+\n"
				"	dbra	%0,1b\n"
				"	bra.b	4f\n"
				"3:	tst.l	%%d0\n"
				"	spl		%%d0\n"
				"	ext.w	%%d0\n"
				"	add.w	%%d2,%%d0\n"
				"	bra.b	2b\n"
				"4:\n"
				: "+d"(loopCount), "+a"(src), "+a"(dst) // outputs
				: // inputs
				: "d0", "d1", "d2", "d3", "cc" AND_MEMORY
			);
		}
		memset(buf + processed * _outputChannels * 2, 0, (_samples - processed) * _outputChannels * 2);
	}

	if (processed > 0 && processed != _samples) {
		warning("processed: %d, _samples: %d", processed, _samples);
	}
}
