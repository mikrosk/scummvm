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

 #include "../../platform/atari/symbols.h"

	.global	SYM(asm_atari_audio_timer_a)
	.global	SYM(g_asm_atari_in_interrupt)
	.global	SYM(g_asm_atari_isp)
	.global	SYM(g_asm_atari_isp_size)

	.extern	SYM(AtariAudioHandler)


	.text

// TODO: block interrupts before calling (X)BIOS handler
SYM(asm_atari_audio_timer_a):
	// gcc's __attribute__((interrupt)) doesn't store FPCR/FPSR/FPIAR
	fsave		-(sp)
	fmovem.l	fpcr/fpsr/fpiar,-(sp)
	fmovem.x	fp0-fp1,-(sp)
	movem.l		d0-d1,-(sp)

	move.l		sp,old_isp
	move.l		g_asm_atari_isp,d0
	add.l		g_asm_atari_isp_size,d0
	move.l		d0,sp

	not.l		g_asm_atari_in_interrupt
	jsr		SYM(AtariAudioHandler)
	not.l		g_asm_atari_in_interrupt

	move.l		old_isp,sp

	movem.l		(sp)+,d0-d1
	fmovem.x	(sp)+,fp0-fp1
	fmovem.l	(sp)+,fpcr/fpsr/fpiar
	frestore	(sp)+

	// clear pending bit; if the callback is too CPU heavy, we don't want to flood the system with endless pending interrupts...
	andi.b		#~(1<<5),0xFFFFFA0B

	// clear in service bit
	andi.b		#~(1<<5),0xFFFFFA0F
	rte


	.bss

SYM(g_asm_atari_in_interrupt):
	ds.l		1
SYM(g_asm_atari_isp):
	ds.l		1
SYM(g_asm_atari_isp_size):
	ds.l		1
old_isp:
	ds.l		1
