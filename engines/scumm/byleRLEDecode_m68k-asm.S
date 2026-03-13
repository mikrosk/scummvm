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

| kate: tab-width 8; replace-tabs off; hl Motorola 68k (VASM/Devpac);

#include "../../backends/platform/atari/symbols.h"

	.globl	SYM(ByleRLEDecode_m68k_Mode0)

	.struct	0
compData.x:
	.space	4	// int x;
compData.y:
	.space	4	// int y;
compData.scaleTable:
	.space	4	// const byte *scaleTable;
//compData.height:
//	.space	4	// int height;
//compData.width:
//	.space	4	// int width;

	.space	2
compData.skipWidth:
	.space	2	// int skipWidth; (used as uint16)

compData.destPtr:
	.space	4	// byte *destPtr;
compData.maskPtr:
	.space	4	// const byte *maskPtr;
compData.scaleXStep:
	.space	4	// int scaleXStep;
compData.mask:
	.space	1	// byte mask;
compData.shr:
	.space	1	// byte shr;
compData.repColor:
	.space	1	// byte repColor;
compData.repLen:
	.space	1	// byte repLen;
compData.boundsRect:
	.space	8	// Common::Rect boundsRect; (4 x int16)
compData.scaleXIndex:
	.space	4	// int scaleXIndex;
compData.scaleYIndex:
	.space	4	// int scaleYIndex;
compData.scaleIndexMask:
	.space	4	// int scaleIndexMask;

	.set	sizeof_local_vars,0
	.macro	add_var name,size
	.set	sizeof_local_vars,sizeof_local_vars+\size
	\name = -sizeof_local_vars
	.endm

	add_var	len,2	// uint16 len;

	.struct 0
arg_fp:
	.space	4
arg_rts:
	.space	4
arg_pcompData:
	.space	4	// BaseCostumeRenderer::ByleRLEData *pcompData

	.space	3
arg_scaleX:
	.space	1	// const byte _scaleX

	.space	3
arg_scaleY:
	.space	1	// const byte _scaleY

	.space	2
arg_height:
	.space	2	// const int _height (used as uint16)

arg_pitch:
	.space	4	// const int pitch
arg_numStrips:
	.space	4	// const int _numStrips
arg_srcPtr:
	.space	4	// const byte *_srcPtr
arg_shadowTable:
	.space	4	// const byte *_shadowTable
arg_palette:
	.space	4	// const uint16 *_palette


	.text

| void ByleRLEDecode_m68k_Mode0(
|	BaseCostumeRenderer::ByleRLEData *pcompData,
|	const byte _scaleX, /* unused */
|	const byte _scaleY, /* unused */
|	const int _height,
|	const int pitch,
|	const int _numStrips,
|	const byte *_srcPtr,
|	const byte *_shadowTable, /* unused */
|	const uint16 *_palette);
SYM(ByleRLEDecode_m68k_Mode0):
// TODO: __FASTCALL__
	link	a6,#sizeof_local_vars		| a6: frame pointer
	movem.l	d2-d7/a2-a5,-(sp)

	// registers taken so far:
	// - d6(color), d5(height), d4(maskbit), d3(pitch), d2(_numStrips)
	// - a0(src), a1(dst), a2(mask), a3(_palette), a5(pcompData), a6(frame pointer)

	// BaseCostumeRenderer::ByleRLEData &compData = *pcompData;
	move.l	(arg_pcompData,a6),a5		| a5: pcompData

	// const byte *src = _srcPtr;
	move.l	(arg_srcPtr,a6),a0		| a0: src

	// uint16 len = compData.repLen;
	clr.w	(len,a6)
	move.b	(compData.repLen,a5),(len+1,a6)
	// uint16 color = compData.repColor;
	clr.l	d6
	move.b	(compData.repColor,a5),d6	| d6.b: color (used as long, too)

	// byte *dst = compData.destPtr;
	move.l	(compData.destPtr,a5),a1	| a1: dst
	// uint16 height = _height;
	move.w	(arg_height,a6),d5		| d5.w: height
	// byte maskbit = revBitMask(compData.x & 7);
	// #define revBitMask(x) (0x80 >> (x))
	move.l	(compData.x,a5),d4
	not.b	d4
	and.b	#0b00000111,d4			| d4.b: bit to test
	// const byte *mask = compData.maskPtr + compData.x / 8;
	move.l	(compData.x,a5),d0
	asr.l	#3,d0
	move.l	(compData.maskPtr,a5),a2
	add.l	d0,a2				| a2: mask

	move.l	(arg_pitch,a6),d3		| d3.w: pitch (used as long, too)
	move.l	(arg_numStrips,a6),d2		| d2.w: _numStrips (used as long, too)
	move.l	(arg_palette,a6),a3		| a3: _palette

	move.w	(a3,d6.l*2),d7			| d7.w: _palette[color]

	// if (len) {
	// 	--len;
	// 	goto StartPos;
	// }
	// len   --len action
	// 0     -1    jump into main_loop
	// 1      0    jump into main_loop = jump into len_loop (0x)
	// 2      1    jump into len_loop (1x)
	// 3      2    jump into len_loop (2x)
	subq.w	#1,(len,a6)
	bgt.b	len_loop
main_loop:
	// do {
	// len = *src++;
	// color = len >> compData.shr;
	// len &= compData.mask;
	// if (!len)
	// 	len = *src++;
	clr.w	d7
	move.b	(a0)+,d7
	move.b	d7,d6
	move.b	(compData.shr,a5),d0
	lsr.b	d0,d6				| d6.b: color (used as long, too)
	and.b	(compData.mask,a5),d7
	bne.b	1f
	move.b	(a0)+,d7
1:	move.w	d7,(len,a6)

	move.w	(a3,d6.l*2),d7			| d7.w: _palette[color]
len_loop:
	// do {
	// 	batch = height < len ? height : len;
	move.w	(len,a6),d0
	move.w	d5,d1				| d1.w: batch
	cmp.w	d0,d1
	blt.b	1f
	move.w	d0,d1
1:	// 	len -= batch;
	// 	height -= batch;
	sub.w	d1,d0
	sub.w	d1,d5
	move.w	d0,(len,a6)
	// 	if (color) {
	tst.b	d6
	beq.b	2f

	subq.w	#1,d1				| dbra
batch_loop:
	// 		do {
	// 			if (!(*mask & maskbit))
	btst	d4,(a2)
	bne.b	1f
	// 				*dst = _palette[color];
	move.b	d7,(a1)
1:	// 			dst  += pitch;
	// 			mask += _numStrips;
	add.l	d3,a1
	add.l	d2,a2
	// 		} while (--batch);
	dbra	d1,batch_loop
	bra.b	3f
	// 	} else {
2:	// 		dst  += batch * pitch;
	move.w	d1,d0
	mulu.w	d3,d0
	add.l	d0,a1
	// 		mask += batch * _numStrips;
	mulu.w	d2,d1
	add.l	d1,a2
	// 	}

3:	// 	if (height == 0) {
	tst.w	d5
	bne.b	5f
	// 		if (--compData.skipWidth == 0)
	subq.w	#1,(compData.skipWidth,a5)
	bne.b	4f
	// 			return;
	movem.l	(sp)+,d2-d7/a2-a5
	unlk	a6
	rts

4:	// 		height = _height;
	move.w	(arg_height,a6),d5		| d5.w: height
	// 		compData.x += compData.scaleXStep;
	move.l	(compData.x,a5),d0
	add.l	(compData.scaleXStep,a5),d0
	move.l	d0,(compData.x,a5)		| d0.l: compData.x
	move.l	d0,d4
	// 		maskbit = revBitMask(compData.x & 7);
	not.b	d4
	and.b	#0b00000111,d4			| d4.b: bit to test
	// 		compData.destPtr += compData.scaleXStep;
	move.l	(compData.destPtr,a5),d1
	add.l	(compData.scaleXStep,a5),d1
	move.l	d1,(compData.destPtr,a5)
	//
	// 		dst = compData.destPtr;
	move.l	d1,a1				| a1: dst
	// 		mask = compData.maskPtr + compData.x / 8;
	asr.l	#3,d0
	move.l	(compData.maskPtr,a5),a2
	add.l	d0,a2				| a2: mask
	// 	}
5:	// StartPos:
	// } while (len > 0);
	tst.w	(len,a6)
	bne.b	len_loop
	// } while (true);
	bra.w	main_loop
