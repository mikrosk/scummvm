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

	.global	SYM(asm_atari_timer_a)
	.global	SYM(asm_atari_audio_handler)
	.global	SYM(asm_atari_audio_handler_switch_to_main)
	.global	SYM(asm_atari_audio_handler_switch_to_interrupt)

	.global	SYM(g_asm_atari_in_interrupt)
	.global	SYM(g_asm_atari_isp)
	.global	SYM(g_asm_atari_isp_size)

	.extern	SYM(AtariAudioCallback)


	.text

// TODO: block interrupts before calling (X)BIOS handler to avoid re-entrancy errors
SYM(asm_atari_timer_a):
	// gcc's __attribute__((interrupt)) doesn't store FPCR/FPSR/FPIAR
	fsave		-(sp)
	fmovem.l	fpcr/fpsr/fpiar,-(sp)
	fmovem.x	fp0-fp1,-(sp)
	movem.l		d0-d1/a0-a1,-(sp)

	// TODO: lower the interrupt mask to allow ACIA interrupts
	//       after setting new SP

	bsr.b		SYM(asm_atari_audio_handler)

	movem.l		(sp)+,d0-d1/a0-a1
	fmovem.x	(sp)+,fp0-fp1
	fmovem.l	(sp)+,fpcr/fpsr/fpiar
	frestore	(sp)+

	// clear pending bit: if the callback is too CPU heavy,
	// we don't want to flood the system with endless pending interrupts
	andi.b		#~(1<<5),0xFFFFFA0B

	// clear in service bit
	andi.b		#~(1<<5),0xFFFFFA0F
	rte

SYM(asm_atari_audio_handler):
	move.l		sp,old_isp
	move.l		g_asm_atari_isp,d0
	add.l		g_asm_atari_isp_size,d0
	move.l		d0,sp

	not.l		g_asm_atari_in_interrupt
	jsr			SYM(AtariAudioCallback)
	not.l		g_asm_atari_in_interrupt

	move.l		old_isp,sp
	rts

// Called from AtariMutexInternal::lock() when interrupt is blocked
// on a mutex held by main. Saves interrupt context and returns to main.
SYM(asm_atari_audio_handler_switch_to_main):
	move.l		a0,-(sp)
	lea			saved_regs_from_int_end,a0

	move.l		(sp)+,-(a0)						| a0
	move.l		(sp)+,-(a0)						| pc
	movem.l		d0-d7/a1-a7,-(a0)
	fmovem.x	fp0-fp7,-(a0)

	not.l		SYM(g_asm_atari_in_interrupt)	| clear

	move.l		old_isp,sp
	rts

// Called from AtariMutexInternal::unlock() when main releases
// a mutex the interrupt was waiting for. Saves main context,
// restores interrupt context.
SYM(asm_atari_audio_handler_switch_to_interrupt):
	move.l		a0,-(sp)
	lea			saved_regs_from_main_end,a0

	move.l		(sp)+,-(a0)						| a0
	move.l		(sp)+,-(a0)						| pc
	movem.l		d0-d7/a1-a7,-(a0)
	fmovem.x	fp0-fp7,-(a0)

	// asm_atari_audio_handler will return there and not to
	// asm_atari_timer_a (RTE, Supervisor code etc)
	pea			(return_to_main,pc)
	move.l		sp,old_isp

	// restore interrupt context
	lea			saved_regs_from_int,a0

	fmovem.x	(a0)+,fp0-fp7
	movem.l		(a0)+,d0-d7/a1-a7
	move.l		(a0)+,-(sp)						| pc for rts below (new stack already)
	move.l		(a0),a0

	not.l		SYM(g_asm_atari_in_interrupt)	| set

	// return to AtariMutexInternal::lock()
	rts

return_to_main:
	lea			saved_regs_from_main,a0

	fmovem.x	(a0)+,fp0-fp7
	movem.l		(a0)+,d0-d7/a1-a7
	move.l		(a0)+,-(sp)						| pc for rts below (new stack already)
	move.l		(a0),a0

	// return to AtariMutexInternal::unlock()
	rts


	.bss

	.macro ds.x count
	.space \count * 12
	.endm

SYM(g_asm_atari_in_interrupt):
	ds.l		1
SYM(g_asm_atari_isp):
	ds.l		1
SYM(g_asm_atari_isp_size):
	ds.l		1
old_isp:
	ds.l		1

saved_regs_from_int:
	ds.x		8								| fp0-fp7
	ds.l		15								| d0-d7/a1-a7
	ds.l		1								| address to return
	ds.l		1								| a0
saved_regs_from_int_end:

saved_regs_from_main:
	ds.x		8								| fp0-fp7
	ds.l		15								| d0-d7/a1-a7
	ds.l		1								| address to return
	ds.l		1								| a0
saved_regs_from_main_end:
