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

#include "backends/mutex/atari/atari-mutex.h"

#include <mint/osbind.h>

#include "backends/mixer/atari/atari-mixer-asm.h"
#include "backends/platform/atari/atari-debug.h"

/**
 * Atari mutex implementation (aimed at the audio handler in an interrupt)
 */
class AtariMutexInternal final : public Common::MutexInternal {
public:
	AtariMutexInternal();
	~AtariMutexInternal() override;

	bool lock() override;
	bool unlock() override;

private:
	static volatile AtariMutexInternal *_blockingMutex;
	static volatile bool _interruptEnabled;

	int _lockDepth = 0;

	enum Owner {
		OWNER_NONE,
		OWNER_MAIN,
		OWNER_INTERRUPT
	};
	volatile Owner _currentOwner = OWNER_NONE;
};

volatile AtariMutexInternal *AtariMutexInternal::_blockingMutex = nullptr;
volatile bool AtariMutexInternal::_interruptEnabled = true;

AtariMutexInternal::AtariMutexInternal() {
}

AtariMutexInternal::~AtariMutexInternal() {
}

bool AtariMutexInternal::lock() {
	if (g_asm_atari_in_interrupt) {
		switch (_currentOwner) {
		case OWNER_MAIN:
			if (_blockingMutex) {
				atari_warning("WARNING: _blockingMutex is not null!");
				return false;
			}
			_blockingMutex = this;
			// there is no point in letting Timer A trigger before the mutex is unlocked
			Jdisint(MFP_TIMERA);
			_interruptEnabled = false;
			asm_atari_audio_handler_switch_to_main();
			// this is where asm_atari_audio_handler_switch_to_interrupt() jumps
			// as user code (no Timer A anymore)

			// fall through
		case OWNER_NONE:
			_currentOwner = OWNER_INTERRUPT;
			// fall through
		case OWNER_INTERRUPT:
			_lockDepth++;
			return true;
		}

	} else {
		switch (_currentOwner) {
		case OWNER_INTERRUPT:
			atari_warning("WARNING: Main thread found mutex owned by interrupt!");
			// fall through
		case OWNER_NONE:
			if (_interruptEnabled)
				Jdisint(MFP_TIMERA);

			_currentOwner = OWNER_MAIN;
			_lockDepth++;

			if (_interruptEnabled)
				Jenabint(MFP_TIMERA);
			return true;

		case OWNER_MAIN:
			_lockDepth++;
			return true;
		}
	}

	return false;
}

bool AtariMutexInternal::unlock() {
	if (_lockDepth > 0) {
		_lockDepth--;

		if (_lockDepth == 0) {
			if (g_asm_atari_in_interrupt) {
				_currentOwner = OWNER_NONE;
			} else {
				if (_interruptEnabled)
					Jdisint(MFP_TIMERA);

				_currentOwner = OWNER_NONE;

				if (_blockingMutex == this) {
					_blockingMutex = nullptr;
					asm_atari_audio_handler_switch_to_interrupt();
					// this is where asm_atari_audio_handler_switch_to_interrupt() jumps
					// when finishes AtariAudioCallback() code

					// re-enable again, hopefully we managed to process samples in time
					_interruptEnabled = true;
				}

				// it's ok if TimerA fires now but TODO:
				// - it has to be the proper next one (i.e. code above filled
				//   the log buffer while SDMA finished playing the phys one
				// - it must handle if code above took more than one sample frame:
				//	- two: log/phys are out of sync
				//	- three: log/phys are in sync
				// - we must also handle if TimerA *would* fire JUST NOW but didn't
				//   allow it yet... that's basically missing one frame above
				if (_interruptEnabled)
					Jenabint(MFP_TIMERA);
			}
		}

		return true;
	}

	return false;
}

Common::MutexInternal *createAtariMutexInternal() {
	return new AtariMutexInternal();
}
