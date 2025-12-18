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

#include <exception>
#include <mint/osbind.h>

#include "backends/mixer/atari/atari-mixer.h"
#include "backends/mixer/atari/atari-mixer-asm.h"
#include "backends/platform/atari/atari-debug.h"
#include "backends/platform/atari/atari-interrupts.h"

/**
 * Atari mutex implementation (aimed at the audio handler in an interrupt)
 */
class InterruptLockFailedException : public std::exception {
public:
	const char* what() const noexcept override {
		return "Interrupt thread failed to acquire mutex";
	}
};

class AtariMutexInternal final : public Common::MutexInternal {
public:
	AtariMutexInternal();
	~AtariMutexInternal() override;

	bool lock() override;
	bool unlock() override;

private:
	static int _mainThreadLockCount;
	static volatile bool _handlerDeferred;

	int _lockDepth = 0;
	uint16 _savedSR;

	enum Owner {
		OWNER_NONE,
		OWNER_MAIN,
		OWNER_INTERRUPT
	};
	volatile Owner _currentOwner = OWNER_NONE;
};

int  AtariMutexInternal::_mainThreadLockCount = 0;
volatile bool AtariMutexInternal::_handlerDeferred = false;

AtariMutexInternal::AtariMutexInternal() {
}

AtariMutexInternal::~AtariMutexInternal() {
}

bool AtariMutexInternal::lock() {
	if (g_asm_atari_in_interrupt) {
		switch (_currentOwner) {
		case OWNER_MAIN:
			_handlerDeferred = true;
			throw InterruptLockFailedException();

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
			assert(false && "Main thread found mutex owned by interrupt");
			// fall through
		case OWNER_NONE: {
			uint16 oldSR = AtariDisableInterrupts();

			_currentOwner = OWNER_MAIN;
			_lockDepth++;
			_mainThreadLockCount++;

			AtariEnableInterrupts(oldSR);
			return true;
		}

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
				uint16 oldSR = AtariDisableInterrupts();

				_currentOwner = OWNER_NONE;
				_mainThreadLockCount--;

				if (_mainThreadLockCount == 0 && _handlerDeferred) {
					_handlerDeferred = false;
                    AtariAudioHandler();
				}

				AtariEnableInterrupts(oldSR);
			}
		}

		return true;
	}

	return false;
}

Common::MutexInternal *createAtariMutexInternal() {
	return new AtariMutexInternal();
}
