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
 * Raw MIDI output for the Atari ST line of computers.
 * Based on the ScummVM SEQ & CoreMIDI drivers.
 * Atari code by Keith Scroggins
 * We, unfortunately, could not use the SEQ driver because the /dev/midi under
 * FreeMiNT (and hence in libc) is considered to be a serial port for machine
 * access.  So, we just use OS calls then to send the data to the MIDI ports
 * directly.  The current implementation is sending 1 byte at a time because
 * in most cases we are only sending up to 3 bytes, I believe this saves a few
 * cycles.  I might change so sysex messages are sent the other way later.
 */

// Disable symbol overrides so that we can use system headers.
#define FORBIDDEN_SYMBOL_ALLOW_ALL
#define FORCE_TEXT_CONSOLE

#include "common/scummsys.h"

#if defined(__MINT__)

#include <osbind.h>
#include "audio/mpu401.h"
#include "common/error.h"
#include "common/system.h"
#include "common/util.h"
#include "audio/musicplugin.h"

class MidiDriver_STMIDI : public MidiDriver_MPU401 {
public:
	MidiDriver_STMIDI() : _isOpen (false) { }
	int open();
	bool isOpen() const { return _isOpen; }
	void close();
	void send(uint32 b) override;
	void sysEx(const byte *msg, uint16 length);
	void setTimerCallback(void *timer_param, Common::TimerManager::TimerProc timer_proc) override;

private:
	bool _isOpen;
};

// --- Instrumentation (temporary) -----------------------------------------
// Measures two things:
//   1. Inter-tick spacing of the installed MPU401 timer callback; anything
//      noticeably above the 10ms base tempo means the cooperative update()
//      pump is starving.
//   2. Wall-time of MidiDriver_STMIDI::send() and number of send() calls per
//      tick, so we can see how much main-thread time Bconout's busy-wait is
//      actually costing on dense ticks.
static Common::TimerManager::TimerProc g_stmidiRealTimerProc = nullptr;
static uint32 g_stmidiLastTickMillis = 0;
static uint32 g_stmidiTickStartMillis = 0;
static uint32 g_stmidiEventsThisTick = 0;

static void stmidiTimerTrampoline(void *refCon) {
	const uint32 now = g_system->getMillis(true);

	if (g_stmidiLastTickMillis != 0) {
		const uint32 delta = now - g_stmidiLastTickMillis;
		if (delta > 15)
			warning("STMIDI: tick delta=%u ms (prev tick: %u events, %u ms)",
				delta, g_stmidiEventsThisTick,
				g_stmidiLastTickMillis - g_stmidiTickStartMillis);
	}

	g_stmidiLastTickMillis = now;
	g_stmidiTickStartMillis = now;
	g_stmidiEventsThisTick = 0;

	if (g_stmidiRealTimerProc)
		g_stmidiRealTimerProc(refCon);

	const uint32 tickMs = g_system->getMillis(true) - g_stmidiTickStartMillis;
	if (tickMs >= 3)
		warning("STMIDI: tick ran %u ms, %u send() calls",
			tickMs, g_stmidiEventsThisTick);
}
// -------------------------------------------------------------------------

int MidiDriver_STMIDI::open() {
	if (_isOpen && (!Bcostat(4)))
		return MERR_ALREADY_OPEN;
	warning("ST Midi Port Open");
	_isOpen = true;
	return 0;
}

void MidiDriver_STMIDI::close() {
	MidiDriver_MPU401::close();
	_isOpen = false;
}

void MidiDriver_STMIDI::setTimerCallback(void *timer_param, Common::TimerManager::TimerProc timer_proc) {
	g_stmidiRealTimerProc = timer_proc;
	g_stmidiLastTickMillis = 0;
	g_stmidiTickStartMillis = 0;
	g_stmidiEventsThisTick = 0;
	MidiDriver_MPU401::setTimerCallback(timer_param,
		timer_proc ? stmidiTimerTrampoline : nullptr);
}

void MidiDriver_STMIDI::send(uint32 b) {
	const uint32 sendStart = g_system->getMillis(true);
	++g_stmidiEventsThisTick;

	midiDriverCommonSend(b);

	byte status_byte = (b & 0x000000FF);
	byte first_byte = (b & 0x0000FF00) >> 8;
	byte second_byte = (b & 0x00FF0000) >> 16;

//	warning("ST MIDI Packet sent");

	switch (b & 0xF0) {
	case 0x80:	// Note Off
	case 0x90:	// Note On
	case 0xA0:	// Polyphonic Key Pressure
	case 0xB0:	// Controller
	case 0xE0:	// Pitch Bend
		Bconout(DEV_MIDI, status_byte);
		Bconout(DEV_MIDI, first_byte);
		Bconout(DEV_MIDI, second_byte);
		break;
	case 0xC0:	// Program Change
	case 0xD0:	// Aftertouch
		Bconout(DEV_MIDI, status_byte);
		Bconout(DEV_MIDI, first_byte);
		break;
	default:
		fprintf(stderr, "Unknown : %08x\n", (int)b);
		break;
	}

	const uint32 sendMs = g_system->getMillis(true) - sendStart;
	if (sendMs >= 2)
		warning("STMIDI: send(%08x) took %u ms", (unsigned)b, sendMs);
}

void MidiDriver_STMIDI::sysEx (const byte *msg, uint16 length) {
	midiDriverCommonSysEx(msg, length);

	warning("Sending SysEx Message (%d bytes)", length);

	Bconout(DEV_MIDI, 0xF0);
	Midiws(length-1, msg);
	Bconout(DEV_MIDI, 0xF7);
}

// Plugin interface

class StMidiMusicPlugin : public MusicPluginObject {
public:
	const char *getName() const {
		return "STMIDI";
	}

	const char *getId() const {
		return "stmidi";
	}

	MusicDevices getDevices() const;
	Common::Error createInstance(MidiDriver **mididriver, MidiDriver::DeviceHandle = 0) const;
};

MusicDevices StMidiMusicPlugin::getDevices() const {
	MusicDevices devices;
	// TODO: Return a different music type depending on the configuration
	// TODO: List the available devices
	devices.push_back(MusicDevice(this, "", MT_GM));
	return devices;
}

Common::Error StMidiMusicPlugin::createInstance(MidiDriver **mididriver, MidiDriver::DeviceHandle) const {
	*mididriver = new MidiDriver_STMIDI();

	return Common::kNoError;
}

//#if PLUGIN_ENABLED_DYNAMIC(STMIDI)
	//REGISTER_PLUGIN_DYNAMIC(STMIDI, PLUGIN_TYPE_MUSIC, StMidiMusicPlugin);
//#else
	REGISTER_PLUGIN_STATIC(STMIDI, PLUGIN_TYPE_MUSIC, StMidiMusicPlugin);
//#endif

#endif
