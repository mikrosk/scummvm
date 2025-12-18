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

#include <mint/mintbind.h>

#include "backends/platform/atari/atari-debug.h"

/**
 * Atari mutex implementation
 */
enum PsemaphoreMode {
	SEM_CREATE,
	SEM_DESTROY,
	SEM_LOCK,
	SEM_UNLOCK
};

static uint32_t GLOBAL_LOCK_SEM = 0;
static uint32_t SEM_BASE = 0;
static void GLOBAL_LOCK() {
	Psemaphore(SEM_LOCK, GLOBAL_LOCK_SEM, -1);
}
static void GLOBAL_UNLOCK() {
	Psemaphore(SEM_UNLOCK, GLOBAL_LOCK_SEM, 0);
}

bool AtariMutexInit() {
	int32_t pid = Pgetpid();

	GLOBAL_LOCK_SEM = 0x4C4B0000 | (uint16_t)(pid & 0xFFFF);	// 'LK' + PID

	// Format: 'L' (0x4C) + 16-bit PID + 8-bit index
	SEM_BASE = 0x4C000000UL | ((uint32_t)(pid & 0xFFFF) << 8);

	// Create global lock semaphore
	return Psemaphore(SEM_CREATE, GLOBAL_LOCK_SEM, 0) == 0	// implicitly owned & locked
		&& Psemaphore(SEM_UNLOCK, GLOBAL_LOCK_SEM, 0) == 0;
}
bool AtariMutexDeinit() {
	return Psemaphore(SEM_LOCK, GLOBAL_LOCK_SEM, -1) == 0	// must own semaphore to destroy it
		&& Psemaphore(SEM_DESTROY, GLOBAL_LOCK_SEM, 0) == 0;
}

class AtariMutexInternal final : public Common::MutexInternal {
private:
	static const int SEM_COUNT = 256;
	static bool _semMap[SEM_COUNT];

	int32_t _semId = -1;	// MiNT semaphore ID for this mutex
	int32_t _ownerPid = -1;	// PID of owning thread (-1 = unlocked)
	int _lockDepth = 0;

public:
	AtariMutexInternal() {
		GLOBAL_LOCK();

		// Find a free semaphore ID to recycle
		int index = -1;
		for (int i = 0; i < SEM_COUNT; ++i) {
			if (!_semMap[i]) {
				_semMap[i] = true;
				index = i;
				break;
			}
		}

		if (index == -1) {
			GLOBAL_UNLOCK();
			error("Maximum number of mutexes (256) reached for this process!");
			return;
		}

		_semId = SEM_BASE | (uint8_t)index;
		atari_debug("Creating mutex %08x", _semId);

		GLOBAL_UNLOCK();

		// Create semaphore and grant ownership to caller
		if (Psemaphore(SEM_CREATE, _semId, 0) == 0) {
			// Release it immediately so it starts unlocked
			Psemaphore(SEM_UNLOCK, _semId, 0);
		} else {
			GLOBAL_LOCK();
			_semMap[index] = false;
			GLOBAL_UNLOCK();

			error("Mutex %08x failed to create!", _semId);
			_semId = -1;
		}
	}

	~AtariMutexInternal() override {
		if (_semId != -1) {
			atari_debug("Destroying mutex %08x", _semId);

			Psemaphore(SEM_LOCK, _semId, -1);

			GLOBAL_LOCK();

			int index = (int)(_semId & 0xFF);
			if (index >= 0 && index < SEM_COUNT) {
				_semMap[index] = false;
			}

			Psemaphore(SEM_DESTROY, _semId, 0);

			GLOBAL_UNLOCK();
		}
	}

	bool lock() override {
		if (_semId == -1)
			return false;

		int32_t myPid = Pgetpid();

		//atari_debug("Locking mutex %08x in %d (depth %d)", _semId, myPid, _lockDepth);

		GLOBAL_LOCK();

		bool alreadyOwned = (_ownerPid == myPid);
		if (alreadyOwned) {
			_lockDepth++;
		}

		GLOBAL_UNLOCK();

		if (alreadyOwned) {
			return true;
		}

		if (Psemaphore(SEM_LOCK, _semId, -1) != 0) {
			warning("Mutex %08x failed to lock!", _semId);
			return false;
		}

		// We got it - claim ownership
		GLOBAL_LOCK();
		_ownerPid = myPid;
		_lockDepth = 1;
		GLOBAL_UNLOCK();
		return true;
	}

	bool unlock() override {
		if (_semId == -1)
			return false;

		int32_t myPid = Pgetpid();

		//atari_debug("Unlocking mutex %08x in %d (depth %d)", _semId, myPid, _lockDepth);

		GLOBAL_LOCK();

		if (_ownerPid != myPid) {
			warning("Mutex %08x failed to unlock!", _semId);
			GLOBAL_UNLOCK();
			return false;
		}

		_lockDepth--;

		if (_lockDepth > 0) {
			GLOBAL_UNLOCK();
			return true;
		}

		_ownerPid = -1;

		GLOBAL_UNLOCK();

		return Psemaphore(SEM_UNLOCK, _semId, 0) == 0;
	}
};

bool AtariMutexInternal::_semMap[AtariMutexInternal::SEM_COUNT];

Common::MutexInternal *createAtariMutexInternal() {
	return new AtariMutexInternal();
}
