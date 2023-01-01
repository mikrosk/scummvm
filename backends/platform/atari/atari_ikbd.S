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

	.globl	_atari_ikbd_init
	.globl	_atari_ikbd_shutdown

	;.globl	_g_scancodeBuffer
	;.globl	_g_scancodeBufferHead
	;.globl	_g_scancodeShiftDepressed

	.globl	_g_atari_ikbd_mouse_buttons_state
	.globl	_g_atari_ikbd_mouse_delta_x
	.globl	_g_atari_ikbd_mouse_delta_y

	.text

// called via Supexec(), no need to store d2/a2
_atari_ikbd_init:
	move.w	#0x22,-(sp)			| Kbdvbase()
	trap	#14
	addq.l	#2,sp

	move.l	d0,a0

wait_for_packet1:
	tst.b	(36.w,a0)			| ikbdstate
	bne.b	wait_for_packet1

	move	sr,-(sp)
	or	#0x700,sr

	lea	(32.w,a0),a0			| get adress to ikbdsys
	move.l	a0,ikbdsys_pointer
	move.l	(a0),old_ikbdsys
	move.l	#new_ikbdsys,(a0)

	move	(sp)+,sr
	rts


// called via Supexec(), no need to store d2/a2
_atari_ikbd_shutdown:
	move.w	#0x22,-(sp)			| Kbdvbase()
	trap	#14
	addq.l	#2,sp

	move.l	d0,a0

wait_for_packet2:
	tst.b	(36.w,a0)			| ikbdstate
	bne.b	wait_for_packet2

	move	sr,-(sp)
	or	#0x700,sr

	move.l	old_ikbdsys,(32.w,a0)

	move	(sp)+,sr
	rts


new_ikbdsys:
	move.b	0xfffffc02.w,d0
	cmp.b	#0xf6,d0
	blo.b	not_ikbd_packet

	cmp.b	#0xf8,d0
	blo.b	not_mouse

	cmp.b	#0xfb,d0
	bhi.b	not_mouse

mouse:	move.b	d0,_g_atari_ikbd_mouse_buttons_state

	move.l	ikbdsys_pointer,a0
	move.l	#ikbdsys_mouse_x,(a0)

not_mouse:
not_ikbd_packet:
	rts

/*
not_mouse:
not_ikbd_packet:
	cmp.b	#0x2a,d1
	bne.b	shift_not_pressed
	move.l	#1,_g_scancodeShiftDepressed
	bra.b	shift_skip

shift_not_pressed:
	cmp.b	#0x36,d1
	bne.b	shift_skip
	move.l	#1,_g_scancodeShiftDepressed

shift_skip:
	lea	_g_scancodeBuffer,a0
	move.l	_g_scancodeBufferHead,d0

	move.b	d1,(0.b,a0,d0.l)		| g_scancodeBuffer[g_scancodeBufferHead] = scancode

	addq.l	#1,d0
	and.l	#256-1,d0			| SCANCODE_BUFFER_SIZE-1
	move.l	d0,_g_scancodeBufferHead

	movem.l	(sp)+,d0-d1/a0	| TODO
	jmp	([old_ikbdsys])
*/

ikbdsys_mouse_x:
	move.b	0xfffffc02.w,d0
	extb.w	d0

	add.w	d0,_g_atari_ikbd_mouse_delta_x

	move.l	ikbdsys_pointer,a0
	move.l	#ikbdsys_mouse_y,(a0)
	rts

ikbdsys_mouse_y:
	move.b	0xfffffc02.w,d0
	extb.w	d0

	add.w	d0,_g_atari_ikbd_mouse_delta_y

skip_my:
	move.l	ikbdsys_pointer,a0
	move.l	#new_ikbdsys,(a0)		| set original pointer
	rts


	.bss

old_ikbdsys:
	ds.l	1
ikbdsys_pointer:
	ds.l	1
