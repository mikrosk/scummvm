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

AtariMixerManager::AtariMixerManager() : MixerManager() {
	_outputRate = 24585;
	_samples = 8192;
	while (_samples * 16 > _outputRate * 2)
		_samples >>= 1;
	_samplesBuf = new uint8[_samples * 4];
}

AtariMixerManager::~AtariMixerManager() {
	Buffoper(0x00);

	Mfree(_atariSampleBuffer);
	_atariSampleBuffer = _atariPhysicalSampleBuffer = _atariLogicalSampleBuffer = nullptr;

	Unlocksnd();

	_atariInitialized = false;

	delete[] _samplesBuf;
}

void AtariMixerManager::init() {
	_mixer = new Audio::MixerImpl(_outputRate, _samples);
	_mixer->setReady(true);

	_atariSampleBufferSize = _samples * 4;

	_atariSampleBuffer = (byte*)Mxalloc(_atariSampleBufferSize * 2, MX_STRAM);
	if (!_atariSampleBuffer)
		return;

	_atariPhysicalSampleBuffer = _atariSampleBuffer;
	_atariLogicalSampleBuffer = _atariSampleBuffer + _atariSampleBufferSize;

	memset(_atariSampleBuffer, 0, 2 * _atariSampleBufferSize);

	if (Locksnd() < 0)
		return;

	Sndstatus(SND_RESET);
	Setmode(MODE_STEREO16);
	Devconnect(DMAPLAY, DAC, CLK25M, CLK25K, NO_SHAKE);
	Soundcmd(ADDERIN, MATIN);
	Setbuffer(SR_PLAY, _atariSampleBuffer, _atariSampleBuffer + 2 * _atariSampleBufferSize);
	Buffoper(SB_PLA_ENA | SB_PLA_RPT);

	_atariInitialized = true;
}

void AtariMixerManager::suspendAudio() {
	{Common::String str = Common::String::format("suspendAudio\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());}

	Buffoper(0x00);

	_audioSuspended = true;
}

int AtariMixerManager::resumeAudio() {
	{Common::String str = Common::String::format("resumeAudio 1\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());}

	if (!_audioSuspended || !_atariInitialized) {
		return -2;
	}

	{Common::String str = Common::String::format("resumeAudio 2\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());}

	Buffoper(SB_PLA_ENA | SB_PLA_RPT);

	_audioSuspended = false;
	return 0;
}

void AtariMixerManager::update() {
	if (_audioSuspended) {
		return;
	}

	static bool loadSampleFlag = true;
	byte *buf = nullptr;

	SndBufPtr sPtr;
	if (Buffptr(&sPtr) != 0)
		return;

	if (!loadSampleFlag)
	{
		// we play from _atariPhysicalSampleBuffer (1st buffer)
		if ((byte*)sPtr.play < _atariLogicalSampleBuffer)
		{
			buf = _atariLogicalSampleBuffer;
			loadSampleFlag = !loadSampleFlag;
		}
	}
	else
	{
		// we play from _atariLogicalSampleBuffer (2nd buffer)
		if ((byte*)sPtr.play >= _atariLogicalSampleBuffer)
		{
			buf = _atariPhysicalSampleBuffer;
			loadSampleFlag = !loadSampleFlag;
		}
	}

	if (_atariInitialized && buf != nullptr) {
		assert(_mixer);
		// generates stereo 16-bit samples
		int processed = _mixer->mixCallback(_samplesBuf, _samples * 4);
		if (processed > 0) {
			memcpy(buf, _samplesBuf, processed * 4);
		} else {
			memset(buf, 0, _atariSampleBufferSize);
		}
	}
}
