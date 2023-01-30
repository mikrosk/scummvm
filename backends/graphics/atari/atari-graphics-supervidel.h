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

#ifndef BACKENDS_GRAPHICS_ATARI_SUPERVIDEL_H
#define BACKENDS_GRAPHICS_ATARI_SUPERVIDEL_H

#define USE_SV_BLITTER

#include "backends/graphics/atari/atari-graphics.h"

#include <cstring>
#include <mint/osbind.h>
#ifdef USE_SV_BLITTER
#define ct60_vm(mode, value) (long)trap_14_wwl((short)0xc60e, (short)(mode), (long)(value))
#define ct60_vmalloc(value) ct60_vm(0, value)
#define ct60_vmfree(value)  ct60_vm(1, value)

#define SV_BLITTER_SRC1  (volatile long*)0x80010058
#define SV_BLITTER_SRC2  (volatile long*)0x8001005C
#define SV_BLITTER_DST   (volatile long*)0x80010060
#define SV_BLITTER_COUNT (volatile long*)0x80010064
#endif

#include "backends/graphics/atari/videl-resolutions.h"
#include "common/scummsys.h"
#include "common/textconsole.h"	// for error()

class AtariSuperVidelManager : public AtariGraphicsManager {
public:
	AtariSuperVidelManager() {
		for (int i = 0; i < SCREENS; ++i) {
			if (!allocateAtariSurface(_screen[i], _screenSurface,
					SCREEN_WIDTH, SCREEN_HEIGHT, PIXELFORMAT8,
					MX_STRAM, 0xA0000000))
				error("Failed to allocate screen memory in ST RAM");
			_screenAligned[i] = (byte*)_screenSurface.getPixels();
		}
		_screenSurface.setPixels(_screenAligned[getDefaultGraphicsMode() <= 1 ? FRONT_BUFFER : BACK_BUFFER1]);

		if (!allocateAtariSurface(_chunkyBuffer, _chunkySurface,
				SCREEN_WIDTH, SCREEN_HEIGHT, PIXELFORMAT8,
				MX_PREFTTRAM))
			error("Failed to allocate chunky buffer memory in ST/TT RAM");

		if (!allocateAtariSurface(_overlayScreen, _screenOverlaySurface,
				getOverlayWidth(), getOverlayHeight(), getOverlayFormat(),
				MX_STRAM, 0xA0000000))
			error("Failed to allocate overlay memory in ST RAM");

		if (!allocateAtariSurface(_overlayBuffer, _overlaySurface,
				getOverlayWidth(), getOverlayHeight(), getOverlayFormat(),
				MX_PREFTTRAM))
			error("Failed to allocate overlay buffer memory in ST/TT RAM");

		// patch SPSHIFT for SuperVidel's BPS8C
		for (unsigned char *p : {scp_320x200x8_vga, scp_320x240x8_vga, scp_640x400x8_vga, scp_640x480x8_vga}) {
			uint16 *p16 = (uint16*)(p + 122 + 30);
			*p16 |= 0x1000;
		}
	}

	~AtariSuperVidelManager() {
#ifdef USE_SV_BLITTER
		ct60_vmfree(_chunkyBuffer);
#else
		Mfree(_chunkyBuffer);
#endif
		_chunkyBuffer = nullptr;

#ifdef USE_SV_BLITTER
		ct60_vmfree(_overlayBuffer);
#else
		Mfree(_overlayBuffer);
#endif
		_overlayBuffer = nullptr;
	}

	virtual const OSystem::GraphicsMode *getSupportedGraphicsModes() const override {
		static const OSystem::GraphicsMode graphicsModes[] = {
			{"direct", "Direct rendering", 0},
			{"single", "Single buffering", 1},
			{"double", "Double buffering", 2},
			{"triple", "Triple buffering", 3},
			{nullptr, nullptr, 0 }
		};
		return graphicsModes;
	}

	int16 getOverlayHeight() const override { return 2 * OVERLAY_HEIGHT; }
	int16 getOverlayWidth() const override { return 2 * OVERLAY_WIDTH; }

private:
	virtual void* allocFast(size_t bytes) const override {
#ifdef USE_SV_BLITTER
		return (void*)ct60_vmalloc(bytes);
#else
		return (void*)Mxalloc(bytes, MX_PREFTTRAM);
#endif
	}

	void copySurfaceToSurface(const Graphics::Surface &srcSurface, Graphics::Surface &dstSurface) const override {
		memcpy(dstSurface.getPixels(), srcSurface.getPixels(), srcSurface.h * srcSurface.pitch);
	}

	void copyRectToSurface(const Graphics::Surface &srcSurface, int destX, int destY, Graphics::Surface &dstSurface,
						   const Common::Rect &subRect) const override {
		dstSurface.copyRectToSurface(srcSurface, destX, destY, subRect);
	}

	void copyRectToSurfaceWithKey(const Graphics::Surface &srcSurface, int destX, int destY, Graphics::Surface &dstSurface,
								  const Common::Rect &subRect, uint32 key) const override {
		dstSurface.copyRectToSurfaceWithKey(srcSurface, destX, destY, subRect, key);
	}
};

#endif
