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

#ifndef ATARI_THREAD_H
#define ATARI_THREAD_H

#include "common/mutex.h"

extern "C" long atari_200hz_init(void);					/* call in super */
extern "C" long atari_200hz_shutdown(void);				/* call in super */
extern "C" volatile uint32 atari_200hz_counter;

/* tpaLo/tpaHi: bounds of the memory our process actually owns (basepage,
 * top of the region kept by Mshrink); the scheduler preempts only user-mode
 * contexts whose USP lies within them so that foreign processes are left alone.
 */
extern "C" long atari_thread_init(void (*func)(void),
								  const void *tpaLo, const void *tpaHi);	/* call in super */
extern "C" long atari_thread_shutdown(void);			/* call in super */
extern "C" void atari_thread_yield(void);

extern "C" void atari_mutex_lock(volatile uint32 *m);
extern "C" void atari_mutex_unlock(volatile uint32 *m);

class AtariMutex : public Common::MutexInternal {
private:
	// byte 0: lock flag, byte 1: owner thread id (0xff = none), bytes 2-3: recursion counter
	volatile uint32 _lock;

public:
	AtariMutex() { _lock = 0x00ff0000; }
	~AtariMutex() { }

	bool lock() override { atari_mutex_lock(&_lock); return true; }
	bool unlock() override { atari_mutex_unlock(&_lock); return true; }
};

#endif
