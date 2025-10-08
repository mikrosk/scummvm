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

#include "backends/mixer/atari/atari-mixer.h"

#include <math.h>
#include <mint/falcon.h>
#include <mint/osbind.h>
#include <mint/ostruct.h>
#include <usound.h>	// https://github.com/mikrosk/usound

#include "backends/platform/atari/atari-debug.h"
#include "common/config-manager.h"

#define DEFAULT_OUTPUT_RATE 24585
#define DEFAULT_OUTPUT_CHANNELS 2
#define DEFAULT_SAMPLES 2048	// 83ms

void AtariAudioShutdown() {
	Jdisint(MFP_TIMERA);
	AtariSoundSetupDeinitXbios();
}

static void SetPlayBuffer(byte *start, byte *end) {
	const uint32 startl = (uint32)start;
	const uint32 endl   = (uint32)end;

	// select replay registers
	*(volatile byte *)0xFF8901 &= ~(1 << 7);

	*(volatile byte *)0xFF8903 = startl >> 16;
	*(volatile byte *)0xFF8905 = startl >> 8;
	*(volatile byte *)0xFF8907 = startl;

	*(volatile byte *)0xFF890F = endl >> 16;
	*(volatile byte *)0xFF8911 = endl >> 8;
	*(volatile byte *)0xFF8913 = endl;
}

int MixCallbackWrapper(AtariMixerManager *manager) {
	return manager->_mixer->mixCallback(manager->_samplesBuf, manager->_samples * manager->_outputChannels * 2);
}

static volatile AtariMixerManager *manager;
void __attribute__((interrupt)) AtariAudioTimerA(void)
{
	// TODO: save/restore FPU state
	if (!manager)
		return;

#if 1
	register int processed __asm__("d0");
	static uint32 oldsp;
	__asm__ __volatile__
	(
		"	move.l	%%sp,%1\n"
		"	move.l	%4,%%sp\n"

		// processed = _mixer->mixCallback(manager->_samplesBuf, manager->_samples * manager->_outputChannels * 2);
		"	move.l	%3,%%sp@-\n"
		"	jsr		(%2)\n"
		"	addq.l	#4,%%sp\n"

		"	move.l	%1,%%sp\n"

		: "=r"(processed), "=g"(oldsp)	// outputs
		: "a"(MixCallbackWrapper), "g"(manager),
		  "g"(manager->_stack + manager->_stackSize)	// inputs
		: __CLOBBER_RETURN("d0") "d1", "d2", "a0", "a1", "a2", "fp0", "fp1", "cc"	// clobbered regs
		  AND_MEMORY
	);
#else
	int processed = manager->_mixer->mixCallback(manager->_samplesBuf, manager->_samples * manager->_outputChannels * 2);
#endif

#if 1
	byte *tmp = manager->_atariPhysicalSampleBuffer;
	manager->_atariPhysicalSampleBuffer = manager->_atariLogicalSampleBuffer;
	manager->_atariLogicalSampleBuffer = tmp;

	if (manager->_downsample) {
		// TODO: test whether this really works in the interrupt handler

		// use the trick with move.b (a7)+,dx which skips two bytes at once
		// basically supplying move.w (src)+,dx; asr.w #8,dx; move.b dx,(dst)+
		__asm__ volatile(
			"	move.l	%%a7,%%d0\n"
			"	move.l	%0,%%a7\n"
			"	moveq	#0x0f,%%d1\n"
			"	and.l	%2,%%d1\n"
			"	neg.l	%%d1\n"
			"	lsr.l	#4,%2\n"
			"	jmp		(2f,%%pc,%%d1.l*2)\n"
			"1:	move.b	(%%a7)+,(%1)+\n"
			"	move.b	(%%a7)+,(%1)+\n"
			"	move.b	(%%a7)+,(%1)+\n"
			"	move.b	(%%a7)+,(%1)+\n"
			"	move.b	(%%a7)+,(%1)+\n"
			"	move.b	(%%a7)+,(%1)+\n"
			"	move.b	(%%a7)+,(%1)+\n"
			"	move.b	(%%a7)+,(%1)+\n"
			"	move.b	(%%a7)+,(%1)+\n"
			"	move.b	(%%a7)+,(%1)+\n"
			"	move.b	(%%a7)+,(%1)+\n"
			"	move.b	(%%a7)+,(%1)+\n"
			"	move.b	(%%a7)+,(%1)+\n"
			"	move.b	(%%a7)+,(%1)+\n"
			"	move.b	(%%a7)+,(%1)+\n"
			"	move.b	(%%a7)+,(%1)+\n"
			"2:	dbra	%2,1b\n"
			"	move.l	%%d0,%%a7\n"
			:                                                                                                                        // outputs
			: "g"(manager->_samplesBuf), "a"(manager->_atariPhysicalSampleBuffer), "d"(processed * manager->_outputChannels * 2 / 2) // inputs
			: "d0", "d1", "cc" AND_MEMORY);
		memset(manager->_atariPhysicalSampleBuffer + processed * manager->_outputChannels * 2 / 2, 0, (manager->_samples - processed) * manager->_outputChannels * 2 / 2);
		SetPlayBuffer(manager->_atariPhysicalSampleBuffer, manager->_atariPhysicalSampleBuffer + manager->_samples * manager->_outputChannels * 2 / 2);
	} else {
		memcpy(manager->_atariPhysicalSampleBuffer, manager->_samplesBuf, processed * manager->_outputChannels * 2);
		memset(manager->_atariPhysicalSampleBuffer + processed * manager->_outputChannels * 2, 0, (manager->_samples - processed) * manager->_outputChannels * 2);
		SetPlayBuffer(manager->_atariPhysicalSampleBuffer, manager->_atariPhysicalSampleBuffer + manager->_samples * manager->_outputChannels * 2);
	}
#endif

	// clear pending bit; if the callback is too CPU heavy, we don't want to flood the system with endless pending interrupts...
	*((volatile byte *)0xFFFFFA0BL) &= ~(1<<5);

	// clear in service bit
	*((volatile byte *)0xFFFFFA0FL) &= ~(1<<5);
}

extern "C" uint32 _stksize;
AtariMixerManager::AtariMixerManager() : MixerManager() {
	atari_debug("AtariMixerManager()");

	suspendAudio();

	ConfMan.registerDefault("output_rate", DEFAULT_OUTPUT_RATE);
	_outputRate = ConfMan.getInt("output_rate");
	if (_outputRate <= 0)
		_outputRate = DEFAULT_OUTPUT_RATE;

	ConfMan.registerDefault("output_channels", DEFAULT_OUTPUT_CHANNELS);
	_outputChannels = ConfMan.getInt("output_channels");
	if (_outputChannels <= 0 || _outputChannels > 2)
		_outputChannels = DEFAULT_OUTPUT_CHANNELS;

	ConfMan.registerDefault("audio_buffer_size", DEFAULT_SAMPLES);
	_samples = ConfMan.getInt("audio_buffer_size");
	if (_samples <= 0)
		_samples = DEFAULT_SAMPLES;

	// hacky way to ensure that the audio stack can be altered from outside
	_stack = _samplesBuf = new byte[_stksize];
	_stackSize = _stksize;
}

AtariMixerManager::~AtariMixerManager() {
	atari_debug("~AtariMixerManager()");

	AtariAudioShutdown();

	Mfree(_atariSampleBuffer);
	_atariSampleBuffer = _atariPhysicalSampleBuffer = _atariLogicalSampleBuffer = nullptr;

	delete[] _samplesBuf;
	_samplesBuf = nullptr;

	delete[] _stack;
	_stackSize = 0;
}

void AtariMixerManager::init() {
	AudioSpec desired, obtained;

	desired.frequency = _outputRate;
	desired.channels = _outputChannels;
	desired.format = AudioFormatSigned16MSB;
	// this is allocates enough samples for 20ms (roughly one 50 Hz frame)
	desired.samples = ((desired.frequency * 20 / 1000) + 1) & -2;

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
	_outputChannels = obtained.channels;
	_samples = obtained.samples;
	_downsample = (obtained.format == AudioFormatSigned8);

	ConfMan.setInt("output_rate", _outputRate);
	ConfMan.setInt("output_channels", _outputChannels);
	ConfMan.setInt("audio_buffer_size", _samples);

	atari_debug("setting %d Hz mixing frequency (%d-bit, %s)",
		  _outputRate, obtained.format == AudioFormatSigned8 ? 8 : 16, _outputChannels == 1 ? "mono" : "stereo");
	atari_debug("sample buffer size: %d", _samples);

	ConfMan.flushToDisk();

	_atariSampleBuffer = (byte*)Mxalloc(obtained.size * 2, MX_STRAM);
	if (!_atariSampleBuffer)
		error("Failed to allocate memory in ST RAM");
	memset(_atariSampleBuffer, 0, obtained.size * 2);

	_atariPhysicalSampleBuffer = _atariSampleBuffer;
	_atariLogicalSampleBuffer = _atariSampleBuffer + obtained.size;

	manager = this;

	Setbuffer(SR_PLAY, _atariPhysicalSampleBuffer, _atariPhysicalSampleBuffer + _samples * _outputChannels * 2);
	Setinterrupt(SI_TIMERA, SI_PLAY);
	Xbtimer(XB_TIMERA, 1<<3, 1, AtariAudioTimerA);	// event count mode, count to '1'
	Jenabint(MFP_TIMERA);

	_samplesBuf = new uint8[_samples * _outputChannels * 2];	// always 16-bit

	_mixer = new Audio::MixerImpl(_outputRate, _outputChannels == 2, _samples);
	_mixer->setReady(true);

	resumeAudio();
}

void AtariMixerManager::suspendAudio() {
	atari_debug("suspendAudio");

	Buffoper(0x00);
	_audioSuspended = true;
}

int AtariMixerManager::resumeAudio() {
	atari_debug("resumeAudio");

	Buffoper(SB_PLA_ENA | SB_PLA_RPT);
	_audioSuspended = false;
	return 0;
}
