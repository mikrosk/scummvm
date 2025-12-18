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

#ifndef PLATFORM_ATARI_INTERRUPTS_H
#define PLATFORM_ATARI_INTERRUPTS_H

#include "common/scummsys.h"

#include <mint/osbind.h>

inline uint16 AtariDisableInterrupts(void) {
	long oldSSP = -1;

	if (Super(SUP_INQUIRE) == SUP_USER)
		oldSSP = Super(SUP_SET);

	uint16 oldSR;
	__asm__ __volatile__(
		"	move	%%sr,%0\n"
		"	ori 	#0x700,%%sr\n"
		: "=g"(oldSR) // outputs
		: // inputs
		: "cc"
	);

	if (oldSSP != -1)
		SuperToUser(oldSSP);

	return oldSR;
}

inline void AtariEnableInterrupts(uint16 oldSR) {
	long oldSSP = -1;

	if (Super(SUP_INQUIRE) == SUP_USER)
		oldSSP = Super(SUP_SET);

	__asm__ __volatile__(
		"	move	%0,%%sr\n"
		: // outputs
		: "g"(oldSR) // inputs
		: "cc"
	);

	if (oldSSP != -1)
		SuperToUser(oldSSP);
}

#endif // PLATFORM_ATARI_INTERRUPTS_H
