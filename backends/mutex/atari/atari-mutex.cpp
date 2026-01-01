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

#include <uthread.h>	// https://github.com/mikrosk/uthread

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
	uthread_mutex_t _mutex;
};


AtariMutexInternal::AtariMutexInternal() {
	if (!uthread_mutex_init(&_mutex)) {
		warning("uthread_mutex_init() failed");
	}
}

AtariMutexInternal::~AtariMutexInternal() {
}

bool AtariMutexInternal::lock() {
	if (!uthread_mutex_lock(&_mutex)) {
		warning("uthread_mutex_lock() failed");
		return false;
	} else {
		return true;
	}
}

bool AtariMutexInternal::unlock() {
	if (!uthread_mutex_unlock(&_mutex)) {
		warning("uthread_mutex_unlock() failed");
		return false;
	} else {
		return true;
	}
}

Common::MutexInternal *createAtariMutexInternal() {
	return new AtariMutexInternal();
}
