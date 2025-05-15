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

#include "atari-surface.h"
#include "graphics/surface.h"

#include <mint/cookie.h>
#include <mint/falcon.h>
#include <mint/trap14.h>
#define ct60_vm(mode, value) (long)trap_14_wwl((short)0xc60e, (short)(mode), (long)(value))
#define ct60_vmalloc(value)  ct60_vm(0, value)
#define ct60_vmfree(value)   ct60_vm(1, value)

#include "backends/graphics/atari/atari-c2p-asm.h"
#include "backends/graphics/atari/atari-graphics-asm.h"
#include "backends/graphics/atari/atari-supervidel.h"
#include "backends/platform/atari/atari-debug.h"
#include "backends/platform/atari/dlmalloc.h"
#include "common/textconsole.h"	// error()

static struct MemoryPool {
	void create() {
		if (base)
			_mspace = create_mspace_with_base((void *)base, size, 0);

		if (_mspace)
			atari_debug("Allocated mspace at 0x%08lx (%ld bytes)", base, size);
		else
			error("mspace allocation failed at 0x%08lx (%ld bytes)", base, size);
	}

	void destroy() {
		if (_mspace) {
			destroy_mspace(_mspace);
			_mspace = nullptr;
		}
	}

	void *malloc(size_t bytes) {
		assert(_mspace);
		return mspace_malloc(_mspace, bytes);
	}

	void *calloc(size_t n_elements, size_t elem_size) {
		assert(_mspace);
		return mspace_calloc(_mspace, n_elements, elem_size);
	}

	void free(void *mem) {
		assert(_mspace);
		mspace_free(_mspace, mem);
	}

	long base;
	long size;

private:
	mspace _mspace;
} s_videoRamPool, s_blitterPool;

static MemoryPool *s_currentPool;

namespace Graphics {

void Surface::create(int16 width, int16 height, const PixelFormat &f) {
	assert(width >= 0 && height >= 0);
	free();

	w = width;
	h = height;
	format = f;
	pitch = w * format.bytesPerPixel;

	if (width && height) {
		if (s_currentPool) {
			pixels = s_currentPool->calloc(height * pitch, format.bytesPerPixel);
			if (!pixels)
				error("Not enough VRAM to allocate a surface");

			if (s_currentPool == &s_blitterPool) {
				assert(pixels >= (void *)0xA1000000);
			} else if (s_currentPool == &s_videoRamPool) {
#ifdef USE_SUPERVIDEL
				if (g_hasSuperVidel)
					assert(pixels >= (void *)0xA0000000 && pixels < (void *)0xA1000000);
				else
#endif
					assert(pixels < (void *)0x01000000);
			}
		} else {
			pixels = ::calloc(height * pitch, format.bytesPerPixel);
			if (!pixels)
				error("Not enough RAM to allocate a surface");
		}

		assert(((uintptr)pixels & (MALLOC_ALIGNMENT - 1)) == 0);
	}
}

void Surface::free() {
	if (pixels) {
		if (s_currentPool)
			s_currentPool->free(pixels);
		else
			::free(pixels);

		pixels = nullptr;
	}

	w = h = pitch = 0;
	format = PixelFormat();
}

} // End of namespace Graphics

///////////////////////////////////////////////////////////////////////////////

AtariSurface::AtariSurface(int bitsPerPixel)
	: _bitsPerPixel(bitsPerPixel) {
}

AtariSurface::AtariSurface(int16 width, int16 height, const Graphics::PixelFormat &pixelFormat, int bitsPerPixel)
	: _bitsPerPixel(bitsPerPixel) {
	create(width, height, pixelFormat);
}

AtariSurface::~AtariSurface() {
	free();
}

void AtariSurface::create(int16 width, int16 height, const Graphics::PixelFormat &pixelFormat) {
	MemoryPool *oldPool = s_currentPool;
	s_currentPool = &s_videoRamPool;

	Graphics::ManagedSurface::create(width * _bitsPerPixel / 8, height, pixelFormat);
	w = width;

	s_currentPool = oldPool;
}

void AtariSurface::free() {
	MemoryPool *oldPool = s_currentPool;
	s_currentPool = &s_videoRamPool;

	Graphics::ManagedSurface::free();

	s_currentPool = oldPool;
}

void AtariSurface::copyRectToSurface(const void *buffer, int srcPitch, int destX, int destY, int width, int height) {
	assert(width % 16 == 0);
	assert(destX % 16 == 0);

	// 'pChunkyEnd' is a delicate parameter: the c2p routine compares it to the address register
	// used for pixel reading; two common mistakes:
	// 1. (subRect.left, subRect.bottom) = beginning of the next line *including the offset*
	// 2. (subRect.right, subRect.bottom) = even worse, end of the *next* line, not current one
	const byte *pChunky    = (const byte *)buffer;
	const byte *pChunkyEnd = pChunky + (height - 1) * srcPitch + width * format.bytesPerPixel;

	byte *pScreen = (byte *)getPixels() + destY * pitch + destX * _bitsPerPixel/8;

	if (_bitsPerPixel == 8) {
		if (srcPitch == width) {
			if (srcPitch == pitch) {
				asm_c2p1x1_8(pChunky, pChunkyEnd, pScreen);
				return;
			} else if (srcPitch == pitch/2) {
				asm_c2p1x1_8_tt(pChunky, pChunkyEnd, pScreen, pitch);
				return;
			}
		}

		asm_c2p1x1_8_rect(
			pChunky, pChunkyEnd,
			width,
			srcPitch,
			pScreen,
			pitch);
	} else {
		if (srcPitch == width && srcPitch/2 == pitch) {
			asm_c2p1x1_4(pChunky, pChunkyEnd, pScreen);
			return;
		}

		asm_c2p1x1_4_rect(
			pChunky, pChunkyEnd,
			width,
			srcPitch,
			pScreen,
			pitch);
	}
}

void AtariSurface::drawMaskedSprite(
	const Graphics::Surface &srcSurface, const Graphics::Surface &srcMask,
	int destX, int destY,
	const Common::Rect &subRect) {
	assert(subRect.width() % 16 == 0);
	assert(subRect.width() == srcSurface.w);
	assert(srcSurface.format == format);
	assert(((uintptr)srcSurface.getPixels() & 0xFF000000) == 0);
	assert(((uintptr)srcMask.getPixels() & 0xFF000000) == 0);

	if (_bitsPerPixel == 4) {
		long oldssp = Super(0L);

		#define BLITTER_HALFTONE ((volatile uint16 *)0xFFFF8A00)
		#define BLITTER_SRC_XINC ((volatile uint16 *)0xFFFF8A20)	// word aligned
		#define BLITTER_SRC_YINC ((volatile uint16 *)0xFFFF8A22)	// word aligned
		#define BLITTER_SRC_ADDR ((volatile uint16 *)0xFFFF8A24)	// 24-bit, word aligned
		#define BLITTER_ENDMASK1 ((volatile uint16 *)0xFFFF8A28)
		#define BLITTER_ENDMASK2 ((volatile uint16 *)0xFFFF8A2A)
		#define BLITTER_ENDMASK3 ((volatile uint16 *)0xFFFF8A2C)
		#define BLITTER_DST_XINC ((volatile uint16 *)0xFFFF8A2E)	// word aligned
		#define BLITTER_DST_YINC ((volatile uint16 *)0xFFFF8A30)	// word aligned
		#define BLITTER_DST_ADDR ((volatile uint16 *)0xFFFF8A32)	// 24-bit, word aligned
		#define BLITTER_X_COUNT  ((volatile uint16 *)0xFFFF8A36)
		#define BLITTER_Y_COUNT  ((volatile uint16 *)0xFFFF8A38)
		#define BLITTER_HOP      ((volatile uint8  *)0xFFFF8A3A)
		#define BLITTER_OP       ((volatile uint8  *)0xFFFF8A3B)
		#define BLITTER_LINE_NUM ((volatile uint8  *)0xFFFF8A3C)
		#define BLITTER_SKEW     ((volatile uint8  *)0xFFFF8A3D)

		// All bits taken from halftone patterns
		#define BLITTER_HOP_HALFTONE	(1<<0)
		// All bits taken from source
		#define BLITTER_HOP_SOURCE		(1<<1)

		// Smudge Mode (Write: Smudge/Clean mode: Read Status)
		#define BLITTER_LINE_SMUDGE		(1<<5)
		// HOG Mode (Write: HOG/BLiT mode, Read: Status)
		#define BLITTER_LINE_HOG		(1<<6)
		// BUSY Bit (Write: Start/Stop, Read: Status Busy/Idle)
		#define BLITTER_LINE_BUSY		(1<<7)

		// No Final Source Read (NFSR)
		#define BLITTER_SKEW_NFSR		(1<<6)
		// Force eXtra Source Read
		#define BLITTER_SKEW_FXSR		(1<<7)

		SuperToUser(oldssp);

		asm_draw_4bpl_sprite(
			(uint16 *)getPixels(), (const uint16 *)srcSurface.getBasePtr(subRect.left, subRect.top),
			(const uint16 *)srcMask.getBasePtr(subRect.left, subRect.top),
			destX, destY,
			pitch, subRect.width(), subRect.height());
	} else if (_bitsPerPixel == 8) {
		asm_draw_8bpl_sprite(
			(uint16 *)getPixels(), (const uint16 *)srcSurface.getBasePtr(subRect.left, subRect.top),
			(const uint16 *)srcMask.getBasePtr(subRect.left, subRect.top),
			destX, destY,
			pitch, subRect.width(), subRect.height());
	}
}

///////////////////////////////////////////////////////////////////////////////

#ifdef USE_SUPERVIDEL
static long hasSvRamBoosted() {
	register long ret __asm__ ("d0") = 0;

	__asm__ volatile(
		"\tmovec	%%itt0,%%d1\n"
		"\tcmp.l	#0xA007E060,%%d1\n"
		"\tbne.s	1f\n"

		"\tmovec	%%dtt0,%%d1\n"
		"\tcmp.l	#0xA007E060,%%d1\n"
		"\tbne.s	1f\n"

		"\tmoveq	#1,%%d0\n"

		"1:\n"
		: "=g"(ret)	/* outputs */
		:			/* inputs  */
		: __CLOBBER_RETURN("d0") "d1", "cc"
		);

	return ret;
}
#endif	// USE_SUPERVIDEL

void AtariSurfaceInit() {
#ifdef USE_SUPERVIDEL
	g_hasSuperVidel = Getcookie(C_SupV, NULL) == C_FOUND && VgetMonitor() == MON_VGA;

	if (g_hasSuperVidel) {
#ifdef USE_SV_BLITTER
		g_superVidelFwVersion = *SV_VERSION & 0x01ff;

		atari_debug("SuperVidel FW Revision: %d, using %s", g_superVidelFwVersion,
			g_superVidelFwVersion >= 9 ? "fast async FIFO" : "slower sync blitting");
#else
		atari_debug("SuperVidel FW Revision: %d, SuperBlitter not used", *SV_VERSION & 0x01ff);
#endif
		if (Supexec(hasSvRamBoosted))
			atari_debug("SV_XBIOS has the pmmu boost enabled");
		else
			atari_warning("SV_XBIOS has the pmmu boost disabled, set 'pmmu_boost = true' in C:\\SV.INF");

#ifdef USE_SV_BLITTER
		s_blitterPool.size = ct60_vmalloc(-1) - (16 * 1024 * 1024);	// SV XBIOS seems to forget the initial 16 MB ST RAM mirror
		s_blitterPool.base = s_blitterPool.size > 0 ? ct60_vmalloc(s_blitterPool.size) : 0;
		s_blitterPool.create();
		// default pool is either null or blitter
		s_currentPool = &s_blitterPool;
#endif
	}
#endif	// USE_SUPERVIDEL

	s_videoRamPool.size = 2 * 1024 * 1024;	// allocate 2 MiB, leave the rest for SDMA / Blitter usage
	s_videoRamPool.base = s_videoRamPool.size > 0 ? Mxalloc(s_videoRamPool.size, MX_STRAM) : 0;
#ifdef USE_SUPERVIDEL
	if (g_hasSuperVidel && s_videoRamPool.base)
		s_videoRamPool.base |= 0xA0000000;
#endif
	s_videoRamPool.create();
}

void AtariSurfaceDeinit() {
	s_videoRamPool.destroy();
	if (s_videoRamPool.base) {
#ifdef USE_SUPERVIDEL
		if (g_hasSuperVidel)
			s_videoRamPool.base &= 0x00FFFFFF;
#endif
		Mfree(s_videoRamPool.base);
		s_videoRamPool.base = 0;
		s_videoRamPool.size = 0;
	}

#ifdef USE_SV_BLITTER
	s_blitterPool.destroy();
	if (s_blitterPool.base) {
		ct60_vmfree(s_blitterPool.base);
		s_blitterPool.base = 0;
		s_blitterPool.size = 0;
	}
#endif
}

/*
 * 	;used when demosequencer is disabled
txt_bitplanes=4
txt_screenx=320
txt_oneplane=6
		include	"debugbp.asm"
SPRX=128
SPRY=139

key_left=$4b
key_right=$4d
key_up=$48
key_down=$50

copyscreen:
		lea	backpic+34,a0
		move.l	screen_buf2,a1
		move.w	#1000-1,d7
.lp:
		movem.l	(a0)+,d0-d6/a2
		movem.l	d0-d6/a2,(a1)
		lea	32(a1),a1
		dbra	d7,.lp
		rts

initfx:
		movem.l	spritepal,d0-d7
		movem.l	d0-d7,io_pal_st.w
		bsr	copyscreen
		bsr	switchdbl
		bsr	copyscreen

		lea	spritepic,a0
		lea	spritemsk,a1
		move.w	#((spritepicend-spritepic)/4)-1,d7
.msklp:
		move.w	(a0)+,d0
		or.w	(a0)+,d0
		or.w	(a0)+,d0
		or.w	(a0)+,d0
		move.w	d0,(a1)+
		move.w	d0,(a1)+
		move.w	d0,(a1)+
		move.w	d0,(a1)+
		dbra	d7,.msklp

		move.w	#192,sprposx
		rts

mainfx:
		bsr	switchdbl
	ifne	showcpu
		eor.w	#$421,io_pal_st.w
	endc
		cmp.b	#key_left,io_kbd_data.w
		bne	.skipl
		sub.w	#1,sprposx
		bsr	copyscreen
		bsr	switchdbl
		bsr	copyscreen
.skipl:
		cmp.b	#key_right,io_kbd_data.w
		bne	.skipr
		add.w	#1,sprposx
		bsr	copyscreen
		bsr	switchdbl
		bsr	copyscreen
.skipr:

.skipkey:
	ifne	showcpu
		eor.w	#$421,io_pal_st.w
	endc
		move.w	sprposx,d0	; in pixels!
		bsr	blit_spritemsk

		move.l	screen_buf2,a0
		move.w	sprposx,d0
		ext.l	d0
		bsr	debugwd
		rts

blit_spritemsk:
;11223344
;012345678
		lea	spritemsk,a0
		lea	spritepic,a1

		move.l	screen_buf2,a3
		add.l	#(200-SPRY)*160,a3

		move.l	#$00080008,d3 ;src inc
		move.l	#$00080008,d4 ;dst inc
		move.w	d0,d5
		and.w	#15,d5
		move.w	#SPRX,d1
		and.w	#$1f0,d0
		cmp.w	#320-SPRX,d0
		bls	.skipcrop
		move.w	#320,d1
		sub.w	d0,d1
		bgt	.skipcrop
		rts
.skipcrop:
		move.w	#320,d7
		sub.w	d1,d7
		lsr.w	#1,d7
		add.w	d7,d4				;dstinc+=((320-SPRX)/2)

		move.w	#SPRX,d7
		sub.w	d1,d7
		lsr.w	#1,d7
		add.w	d7,d3				;srcinc+=((SPRXMAX-SPRX)/2)

		move.l	#SPRY<<16,d2			;count x y
		move.w	d1,d2
		lsr.w	#4,d2
		swap	d2				;countx=planes*sprx/16

		lsr.w	#1,d0				;pixels -> bytes
		add.w	d0,a3
		moveq	#-1,d7

		lea.l   io_blit_endmsk1.w,a2
		move.l	d3,-8(a2)			;source increment
;ffff=0
;7fff=1
		moveq.l	#-1,d7
		lsr.w	d5,d7
		move.w	d7,(a2)+	;endmask 1
		moveq.l	#-1,d7
		move.l	d7,(a2)+			;endmask 2+3
		move.l	d4,(a2)+			;dest increment

		move.l	#$0204c000,d6
		or.w	d5,d6

		move.w	#3,d7
.clearlp:
; m_waitblit	macro
; .\@lp	tst.b	io_blit_mode.w
;	bmi.s	.\@lp
;	endm
		m_waitblit
		lea	io_blit_dstaddr.w,a2
		move.l	a0,-14(a2)			; io_blit_srcaddr
		move.l	a3,(a2)+			; io_blit_dstaddr
		move.l	d2,(a2)+			; io_blit_count
		move.l	d6,(a2)
		addq.l	#2,a0
		addq.l	#2,a3

		dbra	d7,.clearlp

		lea	-8(a3),a3

		move.w	#3,d7
		or.l	#$00070000,d6
.blitlp:
		lea	io_blit_dstaddr.w,a2
		move.l	a1,-14(a2)			; io_blit_srcaddr
		move.l	a3,(a2)+			; io_blit_dstaddr
		move.l	d2,(a2)+			; io_blit_count
		move.l	d6,(a2)
		addq.l	#2,a1
		addq.l	#2,a3

		dbra	d7,.blitlp

		rts

tmpl:		dc.l	$00020062
*/
