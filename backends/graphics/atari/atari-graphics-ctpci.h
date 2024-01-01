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

#ifndef BACKENDS_GRAPHICS_ATARI_CTPCI_H
#define BACKENDS_GRAPHICS_ATARI_CTPCI_H

#ifdef USE_CTPCI

#include "backends/graphics/atari/atari-graphics.h"

#include <mint/osbind.h>

#include "common/scummsys.h"

class AtariCtpciManager : public AtariGraphicsManager {
public:
	AtariCtpciManager() {
		// using virtual methods so must be done here
		allocateSurfaces();
	}

	~AtariCtpciManager() {
		// using virtual methods so must be done here
		freeSurfaces();
	}

private:
	void drawMaskedSprite(Graphics::Surface &dstSurface, int dstBitsPerPixel,
						  const Graphics::Surface &srcSurface, const Graphics::Surface &srcMask,
						  int destX, int destY,
						  const Common::Rect &subRect) override {
		assert(dstBitsPerPixel == 8);
		assert(subRect.width() % 16 == 0);
		assert(subRect.width() == srcSurface.w);

		const byte *src = (const byte *)srcSurface.getBasePtr(subRect.left, subRect.top);
		const uint16 *mask = (const uint16 *)srcMask.getBasePtr(subRect.left, subRect.top);
		byte *dst = (byte *)dstSurface.getBasePtr(destX, destY);

		const int h = subRect.height();
		const int w = subRect.width();
		const int dstOffset = dstSurface.pitch - w;

		for (int j = 0; j < h; ++j) {
			for (int i = 0; i < w; i += 16, mask++) {
				const uint16 m = *mask;

				if (m == 0xFFFF) {
					// all 16 pixels transparentm6
					src += 16;
					dst += 16;
					continue;
				}

				for (int k = 0; k < 16; ++k) {
					const uint16 bit = 1 << (15 - k);

					if (m & bit) {
						// transparent
						src++;
						dst++;
					} else {
						*dst++ = *src++;
					}
				}
			}

			dst += dstOffset;
		}
	}

	Common::Rect alignRect(int x, int y, int w, int h) const override {
		return Common::Rect(x, y, x + w, y + h);
	}
};

#endif	// USE_CTPCI

#endif
