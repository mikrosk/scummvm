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
 *	IKBD 6301 interrupt routine
 *
 *	Patrice Mandin
 */

	.globl	_atari_ikbd_init
	.globl	_atari_ikbd_shutdown

	.globl	_g_atari_ikbd_mouse_buttons_state
	.globl	_g_atari_ikbd_mouse_delta_x
	.globl	_g_atari_ikbd_mouse_delta_y


	.text

_atari_ikbd_init:
	| Disable interrupts

	movew	sr,d1
	movew	#0x2700,sr

	| Save MFP registers used for keyboard

	lea	0xfffffa00:w,a0
	btst	#6,a0@(0x09)
	sne	ikbd_ierb
	btst	#6,a0@(0x15)
	sne	ikbd_imrb

	| Set our routine

	movel	0x118:w,old_ikbd
	movel	#ikbd,0x118:w
	bset	#6,0xfffffa09:w	| IERB
	bset	#6,0xfffffa15:w	| IMRB

	| Set mouse relative mode

	moveb	#8,0xfffffc02:w

	| Reenable interrupts

	movew	d1,sr
	rts

_atari_ikbd_shutdown:
	| Disable interrupts

	movew	sr,d1
	movew	#0x2700,sr

	| Restore previous MFP registers

	lea	0xfffffa00:w,a0

	bclr	#6,a0@(0x09)
	tstb	ikbd_ierb
	beqs	ikbd_restoreierb
	bset	#6,a0@(0x09)
ikbd_restoreierb:

	bclr	#6,a0@(0x15)
	tstb	ikbd_imrb
	beqs	ikbd_restoreimrb
	bset	#6,a0@(0x15)
ikbd_restoreimrb:

	movel	old_ikbd,0x118:w

	| Clear keyboard buffer

	lea	0xfffffc00:w,a0
ikbd_videbuffer:
	btst	#0,a0@
	beqs	ikbd_finbuffer
	tstb	a0@(0x02)
	bras	ikbd_videbuffer
ikbd_finbuffer:

	| Reenable interrupts

	movew	d1,sr
	rts

	.bss

	.even
ikbd_ierb:
	.ds.b	1
ikbd_imrb:
	.ds.b	1

/*--- Our custom IKBD vector ---*/

	.text
	.even
	.ascii	"XBRA"
	.ascii	"SCUM"
old_ikbd:
	.dc.l	0
ikbd:
	moveml	d0-d1/a0,sp@-

	| Check if source is IKBD or MIDI

	btst	#0,0xfffffc00.w
	beqs	ikbd_oldmidi

	moveb	0xfffffc02:w,d0

	| Mouse packet ?

	cmpb	#0xf8,d0
	bmis	ikbd_no_mouse
	cmpb	#0xfc,d0
	bpls	ikbd_no_mouse

	| Mouse packet, byte #1

ikbd_yes_mouse:
	andw	#3,d0
	moveb	d0,_g_atari_ikbd_mouse_buttons_state

	movel	#ikbd_mousex,0x118:w
	bras	ikbd_endit_stack

	| Keyboard press/release

ikbd_no_mouse:
	moveb	d0,d1
#if 0	// TODO
	lea	_SDL_AtariIkbd_keyboard,a0
	andw	#0x7f,d1
	btst	#7,d0
	seq	a0@(0,d1:w)
#endif

	| End of interrupt

ikbd_endit_stack:
	moveml	sp@+,d0-d1/a0

	bclr	#6,0xfffffa11:w
	rte

	| Call old MIDI interrupt

ikbd_oldmidi:
	moveml	sp@+,d0-d1/a0

	movel	old_ikbd,sp@-
	rts

	| Mouse packet, byte #2

ikbd_mousex:
	moveml	d0-d1/a0,sp@-

	| Check if source is IKBD or MIDI
	btst	#0,0xfffffc00.w
	beqs	ikbd_oldmidi

	moveb	0xfffffc02:w,d0
	extw	d0
	addw	d0,_g_atari_ikbd_mouse_delta_x

	movel	#ikbd_mousey,0x118:w
	bras	ikbd_endit_stack

	| Mouse packet, byte #3

ikbd_mousey:
	moveml	d0-d1/a0,sp@-

	| Check if source is IKBD or MIDI

	btst	#0,0xfffffc00.w
	beqs	ikbd_oldmidi

	moveb	0xfffffc02:w,d0
	extw	d0

	addw	d0,_g_atari_ikbd_mouse_delta_y

	movel	#ikbd,0x118:w
	bras	ikbd_endit_stack




#if 0
	;.globl	_g_scancodeBuffer
	;.globl	_g_scancodeBufferHead
	;.globl	_g_scancodeShiftDepressed

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
#endif
