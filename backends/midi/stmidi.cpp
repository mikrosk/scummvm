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
 * directly.
 */

// Disable symbol overrides so that we can use system headers.
#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "common/scummsys.h"

#if defined(__MINT__)

#include <osbind.h>
#include "audio/mpu401.h"
#include "common/error.h"
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

private:
	bool _isOpen;
};

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

void MidiDriver_STMIDI::send(uint32 b) {
	midiDriverCommonSend(b);

	byte buf[3];
	buf[0] = (b & 0x000000FF);
	buf[1] = (b & 0x0000FF00) >> 8;
	buf[2] = (b & 0x00FF0000) >> 16;

//	warning("ST MIDI Packet sent");

	switch (b & 0xF0) {
	case 0x80:	// Note Off
	case 0x90:	// Note On
	case 0xA0:	// Polyphonic Key Pressure
	case 0xB0:	// Controller
	case 0xE0:	// Pitch Bend
		Midiws(3-1, buf);
		break;
	case 0xC0:	// Program Change
	case 0xD0:	// Aftertouch
		Midiws(2-1, buf);
		break;
	default:
		fprintf(stderr, "Unknown : %08x\n", (int)b);
		break;
	}
}

void MidiDriver_STMIDI::sysEx (const byte *msg, uint16 length) {
	midiDriverCommonSysEx(msg, length);

	warning("Sending SysEx Message (%d bytes)", length);

	byte buf[1 + 268 + 1];
	assert(length + 2 <= ARRAYSIZE(buf));

	buf[0] = 0xF0;
	memcpy(buf + 1, msg, length);
	buf[1 + length] = 0xF7;

	Midiws(length + 2 - 1, buf);
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
