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

#include <mint/falcon.h>
#include <mint/mintbind.h>
#include <signal.h>
#include <usound.h>	// https://github.com/mikrosk/usound

#include "backends/platform/atari/atari-debug.h"
#include "common/config-manager.h"

extern "C" uint32 _stksize;
extern "C" void _setstack(void *newsp);
extern "C" int usleep (__useconds_t __useconds);	// #include <unistd.h>

#define DEFAULT_OUTPUT_RATE 24585
#define DEFAULT_OUTPUT_CHANNELS 2
#define DEFAULT_SAMPLES 2048	// 83ms

void AtariAudioShutdown() {
	Jdisint(MFP_TIMERA);
	AtariSoundSetupDeinitXbios();
}

AtariMixerManager::AtariMixerManager() : MixerManager() {
	atari_debug("AtariMixerManager()");

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

	g_system->getEventManager()->getEventDispatcher()->registerObserver(this, 10, false);
}

AtariMixerManager::~AtariMixerManager() {
	atari_debug("~AtariMixerManager()");

	g_system->getEventManager()->getEventDispatcher()->unregisterObserver(this);

	AtariAudioShutdown();

	Mfree(_atariSampleBuffer);
	_atariSampleBuffer = _atariPhysicalSampleBuffer = _atariLogicalSampleBuffer = nullptr;

	delete[] _samplesBuf;
	_samplesBuf = nullptr;
}

void AtariMixerManager::init() {
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
	atari_debug("phys: %p, log: %p", _atariPhysicalSampleBuffer, _atariLogicalSampleBuffer);

	Setinterrupt(SI_TIMERA, SI_PLAY);
	Xbtimer(XB_TIMERA, 1<<3, 1, timerA);	// event count mode, count to '1'
	Jenabint(MFP_TIMERA);

	_samplesBuf = new uint8[_samples * _outputChannels * 2];	// always 16-bit

	_mixer = new Audio::MixerImpl(_outputRate, _outputChannels == 2, _samples);
	_mixer->setReady(true);

	{
		BASEPAGE *bp = (BASEPAGE *)Pexec(PE_CBASEPAGE_FLAGS, 0L, "", 0L);
		Mshrink(bp, 256 + _stksize);

		bp->p_tbase = (char *)audioThread;
		bp->p_dbase = (char *)this;
		bp->p_hitpa = (char *)bp + 256 + _stksize;

		if (Pexec(PE_ASYNC_GO, 0L, bp, 0L) == -ENOSYS) {
			// TODO: silently ignore for non-MiNT
			error("Pexec() failed");
		}

		Mfree(bp);
	}

	// set initial buffer (silence)
	Setbuffer(SR_PLAY, _atariPhysicalSampleBuffer, _atariLogicalSampleBuffer);
	// fire first interrupt (at the beginning of the sample)
	atari_debug("initial play start");
	Buffoper(SB_PLA_ENA | SB_PLA_RPT);
}

void AtariMixerManager::suspendAudio() {
	atari_debug("suspendAudio");

	// TODO
	_audioSuspended = true;
}

int AtariMixerManager::resumeAudio() {
	atari_debug("resumeAudio");

	_audioSuspended = false;
	// TODO
	return 0;
}

bool AtariMixerManager::notifyEvent(const Common::Event &event) {
	switch (event.type) {
	case Common::EVENT_QUIT:
		Buffoper(0x00);
		_quit = true;
		while (!_quitAcknowledged) {
			Syield();
		}
		// fall through
	case Common::EVENT_RETURN_TO_LAUNCHER:
		return false;
	default:
		break;
	}

	return false;
}

// TODO: Aranym misses the first trigger
volatile bool AtariMixerManager::_timerATriggered = false;
volatile static uint32 timerAMillis = 0;

// Signal handler function
volatile static int audioPid = -1;
void signal_handler(long sig) {
	if (sig == SIGUSR1) {
		// ...
	}
}

void __attribute__((interrupt)) AtariMixerManager::timerA(void) {
	_timerATriggered = true;

	//atari_debug("triggered");
	timerAMillis = g_system->getMillis();
	if (audioPid != -1)
		Pkill((int)audioPid, SIGUSR1);

	*((volatile byte *)0xFFFFFA0FL) = ~(1<<5);	// clear in service bit
}

void AtariMixerManager::audioThread(BASEPAGE *bp) {
	_setstack((char *)bp + _stksize);
	{
		// new stack in place
		AtariMixerManager *manager = (AtariMixerManager *)bp->p_dbase;

		Mfree(bp->p_env);

		bp->p_env   = _base->p_env;
		bp->p_tbase = _base->p_tbase;
		bp->p_tlen  = _base->p_tlen;
		bp->p_dbase = _base->p_dbase;
		bp->p_dlen  = _base->p_dlen;
		bp->p_bbase = _base->p_bbase;
		bp->p_blen  = _base->p_blen;

		Pnice(-20);

		audioPid = Pgetpid();
		Psignal(SIGUSR1, (long)signal_handler);

		while (!manager->_quit) {
			// // while (!_timerATriggered && !manager->_quit) {
			// // 	Syield();
			// // }
			// _timerATriggered = false;
			Pause();

			uint32 noticed = g_system->getMillis();

			atari_debug("timer a: %d, noticed: %d", timerAMillis, noticed);

			long oldssp = Super(0L);
			int t1 = (int)g_system->getMillis();
			manager->update();
			int t2 = (int)g_system->getMillis();
			SuperToUser(oldssp);

			if (t2 - t1 > manager->_samples * 1000 / manager->_outputRate) {
				atari_warning("WARNING: mixing takes too long! (%d > %d); %d %d",
					t2 - t1, manager->_samples * 1000 / manager->_outputRate,
					t1, t2);
			} else {
				atari_warning("WARNING: mixing is quick enough (%d <= %d); %d %d",
							  t1 - t2, manager->_samples * 1000 / manager->_outputRate,
							  t1, t2);
			}
		}
		manager->_quitAcknowledged = true;

		Pterm0();
	}
}

void AtariMixerManager::update() {
	// _atariPhysicalSampleBuffer is still playing
	byte *buf = _atariLogicalSampleBuffer;
	size_t bufLen = _samples * _outputChannels * (_downsample ? 1 : 2);
	//atari_debug("setting buffer to %p", buf);
	Setbuffer(SR_PLAY, buf, buf + bufLen);

	byte* tmp = _atariPhysicalSampleBuffer;
	_atariPhysicalSampleBuffer = _atariLogicalSampleBuffer;
	_atariLogicalSampleBuffer = tmp;

	int processed = _mixer->mixCallback(_samplesBuf, _samples * _outputChannels * 2);
	if (_downsample) {
		// use the trick with move.b (a7)+,dx which skips two bytes at once
		// basically supplying move.w (src)+,dx; asr.w #8,dx; move.b dx,(dst)+
		if (processed > 0) {
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
			: // outputs
			: "g"(_samplesBuf), "a"(buf), "d"(processed * _outputChannels * 1) // inputs
			: "d0", "d1", "cc" AND_MEMORY
			);
		}
		memset(buf + processed * _outputChannels * 1, 0, (_samples - processed) * _outputChannels * 1);
	} else {
		memcpy(buf, _samplesBuf, processed * _outputChannels * 2);
		memset(buf + processed * _outputChannels * 2, 0, (_samples - processed) * _outputChannels * 2);
	}
}
