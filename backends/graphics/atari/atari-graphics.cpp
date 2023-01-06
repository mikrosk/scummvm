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

#include "backends/graphics/atari/atari-graphics.h"

#include <mint/osbind.h>

#include "backends/graphics/atari/320x240x8_vga.h"
#include "backends/graphics/atari/320x240x16_vga.h"
#include "backends/graphics/atari/atari_c2p-asm.h"
#include "backends/graphics/atari/atari-graphics-asm.h"
#include "common/foreach.h"
#include "common/str.h"

// max(screen, overlay)
#define SCREEN_WIDTH	320
#define SCREEN_HEIGHT	240
#define SCREEN_DEPTH	2

#define SCREEN_ACTIVE

AtariGraphicsManager::~AtariGraphicsManager() {
	Mfree(_chunkyBuffer);
	_chunkyBuffer = nullptr;

	Mfree(_screen);
	_screen = nullptr;
}

bool AtariGraphicsManager::setGraphicsMode(int mode, uint flags) {
	Common::String str = Common::String::format("setGraphicsMode: %d, %d\n", mode, flags);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	return (mode == 0);
}

void AtariGraphicsManager::initSize(uint width, uint height, const Graphics::PixelFormat *format) {
	Common::String str = Common::String::format("initSize: %d, %d\n", width, height);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	_width = width;
	_height = height;
	_format = format ? *format : Graphics::PixelFormat::createFormatCLUT8();
}

void AtariGraphicsManager::beginGFXTransaction() {
	Common::String str = Common::String::format("beginGFXTransaction\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());
}

OSystem::TransactionError AtariGraphicsManager::endGFXTransaction() {
	Common::String str = Common::String::format("endGFXTransaction\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (_format != Graphics::PixelFormat::createFormatCLUT8())
		return OSystem::TransactionError::kTransactionFormatNotSupported;

	if (_oldFormat != _format) {
#ifdef SCREEN_ACTIVE
		asm_screen_set_scp_res(scp_320x240x8_vga);
#endif
		_oldFormat = _format;
	}

	if (_width > SCREEN_WIDTH || _height > SCREEN_HEIGHT)
		return OSystem::TransactionError::kTransactionSizeChangeFailed;

	if (_oldWidth != _width || _oldHeight != _height) {
		if (_screen == nullptr) {
			// no need to realloc each time

			_chunkyBuffer = (byte*)Mxalloc(SCREEN_WIDTH * SCREEN_HEIGHT + 15, MX_PREFTTRAM);
			if (!_chunkyBuffer)
				return OSystem::TransactionError::kTransactionSizeChangeFailed;

			byte* chunkyBufferAligned = (byte*)(((unsigned long)_chunkyBuffer + 15) & 0xfffffff0);
			memset(chunkyBufferAligned, 0, SCREEN_WIDTH * SCREEN_HEIGHT);

			_chunkySurface.init(_width, _height, _width, chunkyBufferAligned, _format);

			_screen = (byte*)Mxalloc(SCREEN_WIDTH * SCREEN_HEIGHT * SCREEN_DEPTH + 15, MX_STRAM);
			if (!_screen)
				return OSystem::TransactionError::kTransactionSizeChangeFailed;

			byte* screenAligned = (byte*)(((unsigned long)_screen + 15) & 0xfffffff0);
			memset(screenAligned, 0, SCREEN_WIDTH * SCREEN_HEIGHT * SCREEN_DEPTH);

			_screenSurface8.init(_width, _height, _width * _format.bytesPerPixel, screenAligned, _format);
#ifdef SCREEN_ACTIVE
			asm_screen_set_vram(_screenSurface8.getPixels());
#endif
			_overlaySurface.create(getOverlayWidth(), getOverlayHeight(), getOverlayFormat());
			_screenSurface16.init(_overlaySurface.w, _overlaySurface.h, _overlaySurface.pitch, screenAligned, _overlaySurface.format);
		}

		_oldWidth = _width;
		_oldHeight = _height;
	}

	return OSystem::kTransactionSuccess;
}

void AtariGraphicsManager::setPalette(const byte *colors, uint start, uint num) {
	//Common::String str = Common::String::format("setPalette: %d, %d\n", start, num);
	//g_system->logMessage(LogMessageType::kDebug, str.c_str());

	uint *pal = &_palette[start];

	for (uint i = 0; i < num; ++i) {
		// RRRRRRRR GGGGGGGG BBBBBBBB -> RRRRRRrr GGGGGGgg 00000000 BBBBBBbb
		pal[i] = (colors[i * 3 + 0] << 24) | (colors[i * 3 + 1] << 16) | colors[i * 3 + 2];
	}

#ifdef SCREEN_ACTIVE
	asm_screen_set_falcon_palette(_palette);
#endif
}

void AtariGraphicsManager::grabPalette(byte *colors, uint start, uint num) const {
	//Common::String str = Common::String::format("grabPalette: %d, %d\n", start, num);
	//g_system->logMessage(LogMessageType::kDebug, str.c_str());

	const uint *pal = &_palette[start];

	for (uint i = 0; i < num; ++i) {
		// RRRRRRrr GGGGGGgg 00000000 BBBBBBbb -> RRRRRRRR GGGGGGGG BBBBBBBB
		colors[i * 3 + 0] = pal[i] >> 24;
		colors[i * 3 + 1] = pal[i] >> 16;
		colors[i * 3 + 2] = pal[i];
	}
}

void AtariGraphicsManager::copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) {
	//Common::String str = Common::String::format("copyRectToScreen: %d, %d, %d, %d, %d\n", pitch, x, y, w, h);
	//g_system->logMessage(LogMessageType::kDebug, str.c_str());

	_chunkySurface.copyRectToSurface(buf, pitch, x, y, w, h);

	handleModifiedRect(Common::Rect(x, y, x + w, y + h), _modifiedChunkyRects, _chunkySurface);
}

Graphics::Surface *AtariGraphicsManager::lockScreen() {
	Common::String str = Common::String::format("lockScreen\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	return &_chunkySurface;
}

void AtariGraphicsManager::fillScreen(uint32 col) {
	Common::String str = Common::String::format("fillScreen: %d\n", col);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	const Common::Rect rect = Common::Rect(_width, _height);

	_chunkySurface.fillRect(rect, col);

	handleModifiedRect(rect, _modifiedChunkyRects, _chunkySurface);
}

// TODO: double buffering & avoid _overlaySurface / _chunkyBufferSurface
//       when in a native format (16bpp, SuperVidel+8bpp). This requires
//       special cursor handling as we'd need to store the rect underneath it (or better, use a mask).
void AtariGraphicsManager::updateScreen() {
	//Common::String str = Common::String::format("updateScreen\n");
	//g_system->logMessage(LogMessageType::kDebug, str.c_str());

	const int screenCorrection = isOverlayVisible()
		? (SCREEN_HEIGHT - _overlaySurface.h) / 2
		: (SCREEN_HEIGHT - _chunkySurface.h) / 2;

	while (!_modifiedOverlayRects.empty()) {
		const Common::Rect &rect = _modifiedOverlayRects.back();

		_screenSurface16.copyRectToSurface(
			_overlaySurface.getBasePtr(rect.left, rect.top),
			_overlaySurface.pitch,
			rect.left, rect.top + screenCorrection,
			rect.width(), rect.height());

		_modifiedOverlayRects.pop_back();
	}

	while (!_modifiedChunkyRects.empty()) {
		const Common::Rect &rect = _modifiedChunkyRects.back();

		c2p1x1_8_falcon(
			(char*)_chunkySurface.getBasePtr(rect.left, rect.top),
			(char*)_chunkySurface.getBasePtr(rect.right, rect.bottom),
			rect.width(),
			_chunkySurface.pitch,
			(char*)_screenSurface8.getBasePtr(rect.left, rect.top + screenCorrection),
			_screenSurface8.pitch);

		_modifiedChunkyRects.pop_back();
	}

	if (_mouseVisible) {
		updateCursorRect();
	} else if (!_cursorRect.isEmpty()) {
		// force cursor background restore
		_oldCursorRect = _cursorRect;
		_cursorRect = Common::Rect();
	}

	if (_mouseOutOfScreen)
		return;

	if (_mouseVisible /*&& _cursorModified*/ && !isOverlayVisible()) {
		// updates _cursorRect
		prepareCursorSurface8();
	}

	if (_oldCursorRect != _cursorRect) {
		if (!_oldCursorRect.isEmpty()) {
			if (isOverlayVisible()) {
				_screenSurface16.copyRectToSurface(
					_overlaySurface.getBasePtr(_oldCursorRect.left, _oldCursorRect.top),
					_overlaySurface.pitch,
					_oldCursorRect.left, _oldCursorRect.top + screenCorrection,
					_oldCursorRect.width(), _oldCursorRect.height());
			} else {
				c2p1x1_8_falcon(
					(char*)_chunkySurface.getBasePtr(_oldCursorRect.left, _oldCursorRect.top),
					(char*)_chunkySurface.getBasePtr(_oldCursorRect.right, _oldCursorRect.bottom),
					_oldCursorRect.width(),
					_chunkySurface.w,
					(char*)_screenSurface8.getBasePtr(_oldCursorRect.left, _oldCursorRect.top + screenCorrection),
					_screenSurface8.pitch);
			}
		}

		_oldCursorRect = _cursorRect;
	}

	// TODO: we can't use _cursorModified yet because we'd need to detect whether
	//       the cursor shouldn't be updated if rect underneath has changed.
	if (_mouseVisible /*&& _cursorModified*/) {
		if (isOverlayVisible()) {
			copyCursorSurface16(screenCorrection);
		} else {
			c2p1x1_8_falcon(
				(char*)_cursorSurface8.getPixels(),
				(char*)_cursorSurface8.getBasePtr(0, _cursorSurface8.h),
				_cursorSurface8.w,
				_cursorSurface8.pitch,
				(char*)_screenSurface8.getBasePtr(_cursorRect.left, _cursorRect.top + screenCorrection),
				_screenSurface8.pitch);
		}

		_cursorModified = false;
	}
}

void AtariGraphicsManager::showOverlay() {
	Common::String str = Common::String::format("showOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (_overlayVisible)
		return;

#ifdef SCREEN_ACTIVE
	asm_screen_set_scp_res(scp_320x240x16_vga);
#endif

	_modifiedChunkyRects.clear();

	_overlayVisible = true;
}

void AtariGraphicsManager::hideOverlay() {
	Common::String str = Common::String::format("hideOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (!_overlayVisible)
		return;

#ifdef SCREEN_ACTIVE
	memset(_screenSurface16.getPixels(), 0, _screenSurface16.pitch * _screenSurface16.h);
	asm_screen_set_scp_res(scp_320x240x8_vga);
#endif

	_modifiedOverlayRects.clear();

	_overlayVisible = false;
}

void AtariGraphicsManager::clearOverlay() {
	Common::String str = Common::String::format("clearOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());
}

void AtariGraphicsManager::grabOverlay(Graphics::Surface &surface) const {
	Common::String str = Common::String::format("grabOverlay: %d, %d, %d\n", surface.pitch, surface.w, surface.h);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	// not really needed as copyRectToOverlay() overwrites the surface again
	//surface.copyRectToSurface(_overlaySurface, 0, 0, Common::Rect(_overlaySurface.w, _overlaySurface.h));
}

void AtariGraphicsManager::copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) {
	//Common::String str = Common::String::format("copyRectToOverlay: %d, %d, %d, %d, %d\n", pitch, x, y, w, h);
	//g_system->logMessage(LogMessageType::kDebug, str.c_str());

	_overlaySurface.copyRectToSurface(buf, pitch, x, y, w, h);

	handleModifiedRect(Common::Rect(x, y, x + w, y + h), _modifiedOverlayRects, _overlaySurface);
}

int16 AtariGraphicsManager::getOverlayHeight() const {
	return SCREEN_HEIGHT;
}

int16 AtariGraphicsManager::getOverlayWidth() const {
	return SCREEN_WIDTH;
}

bool AtariGraphicsManager::showMouse(bool visible) {
	//Common::String str = Common::String::format("showMouse: %d\n", visible);
	//g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (_mouseVisible == visible) {
		return visible;
	}

	bool last = _mouseVisible;
	_mouseVisible = visible;
	return last;
}

void AtariGraphicsManager::warpMouse(int x, int y) {
	//Common::String str = Common::String::format("warpMouse: %d, %d\n", x, y);
	//g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (_mouseX == x && _mouseY == y)
		return;

	_mouseX = x;
	_mouseY = y;
}

void AtariGraphicsManager::setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor, bool dontScale, const Graphics::PixelFormat *format) {
	//Common::String str = Common::String::format("setMouseCursor: %d, %d, %d, %d, %d, %p\n", w, h, hotspotX, hotspotY, keycolor, (const void*)format);
	//g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (w == 0 || h == 0 || buf == nullptr) {
		if (_cursorSurface.getPixels())
			_cursorSurface.free();
		return;
	}

	const Graphics::PixelFormat cursorFormat = format ? *format : Graphics::PixelFormat::createFormatCLUT8();

	if ((uint)_cursorSurface.w != w || (uint)_cursorSurface.h != h || _cursorSurface.format != cursorFormat)
		_cursorSurface.create(w, h, cursorFormat);

	_cursorSurface.copyRectToSurface(buf, _cursorSurface.pitch, 0, 0, w, h);

	_cursorHotspotX = hotspotX;
	_cursorHotspotY = hotspotY;
	_cursorKeycolor = keycolor;

	_cursorModified = true;
}

void AtariGraphicsManager::handleModifiedRect(Common::Rect rect, Common::Array<Common::Rect> &rects, const Graphics::Surface &surface)
{
	// align on 16px
	rect.left &= 0xfff0;
	rect.right = (rect.right + 15) & 0xfff0;
	if (rect.right > surface.w)
		rect.right = surface.w;

	if (rect.width() == surface.w && rect.height() == surface.h) {
		Common::String str = Common::String::format("handleModifiedRect: purge\n");
		g_system->logMessage(LogMessageType::kDebug, str.c_str());

		rects.clear();
		rects.push_back(rect);
		return;
	}

	for (const Common::Rect &r : rects) {
		if (r.contains(rect))
			return;
	}

	rects.push_back(rect);
}

void AtariGraphicsManager::updateCursorRect() {
	Common::Rect cursorSrcBounds(_cursorSurface.w, _cursorSurface.h);
	Common::Rect cursorDstBounds(
		_mouseX - _cursorHotspotX,	// left
		_mouseY - _cursorHotspotY,	// top
		_mouseX - _cursorHotspotX + _cursorSurface.w,	// right
		_mouseY - _cursorHotspotY + _cursorSurface.h);	// bottom

	if (isOverlayVisible())
		_mouseOutOfScreen = !_screenSurface16.clip(cursorSrcBounds, cursorDstBounds);
	else
		_mouseOutOfScreen = !_screenSurface8.clip(cursorSrcBounds, cursorDstBounds);

	if (_mouseOutOfScreen)
		return;

	_clippedCursorSurface = _cursorSurface.getSubArea(cursorSrcBounds);

	_cursorRect = cursorDstBounds;
}

void AtariGraphicsManager::prepareCursorSurface8() {
	Common::Rect backgroundCursorRect = _cursorRect;

	// ensure that background's left and right lie on a 16px boundary and double the width if needed
	backgroundCursorRect.moveTo(backgroundCursorRect.left & 0xfff0, backgroundCursorRect.top);

	const int cursorDeltaX = _cursorRect.left - backgroundCursorRect.left;

	backgroundCursorRect.right = (backgroundCursorRect.right + cursorDeltaX + 15) & 0xfff0;
	if (backgroundCursorRect.right > _chunkySurface.w)
		backgroundCursorRect.right = _chunkySurface.w;

	if (_cursorSurface8.w != backgroundCursorRect.width() || _cursorSurface8.h != backgroundCursorRect.height()) {
		_cursorSurface8.create(
			backgroundCursorRect.width(),
			backgroundCursorRect.height(),
			_chunkySurface.format);
	}

	// copy background
	// TODO: mask out old cursor and mask in new one?
	_cursorSurface8.copyRectToSurface(
		_chunkySurface.getBasePtr(backgroundCursorRect.left, backgroundCursorRect.top),
		_chunkySurface.pitch,
		0, 0,
		backgroundCursorRect.width(), backgroundCursorRect.height());

	_cursorSurface8.copyRectToSurfaceWithKey(
		_clippedCursorSurface.getPixels(),
		_clippedCursorSurface.pitch,
		cursorDeltaX, 0,
		_clippedCursorSurface.w, _clippedCursorSurface.h,
		_cursorKeycolor);

	_cursorRect = backgroundCursorRect;
}

void AtariGraphicsManager::copyCursorSurface16(int screenCorrection) {
	static byte palette[256*3] = {};
	{
		// TODO: system palette?
		static bool paletteInitialized;
		if (!paletteInitialized) {
			for (int i = 0; i < 4; ++i) {
				palette[i * 3 + 0] = (byte)(63 + i * 64);
				palette[i * 3 + 1] = (byte)(63 + i * 64);
				palette[i * 3 + 2] = (byte)(63 + i * 64);
			}
			paletteInitialized = true;
		}
	}

	// TODO: mask out old cursor and mask in new one?
	if (_cursorSurface.format == _screenSurface16.format) {
		_screenSurface16.copyRectToSurfaceWithKey(
			_clippedCursorSurface,
			_cursorRect.left, _cursorRect.top + screenCorrection,
			_cursorRect,
			_cursorKeycolor);
	} else {
		// faster (no memory allocation) version of Surface::convertTo()
		const int w = _cursorRect.width();
		const int h = _cursorRect.height();
		const Graphics::PixelFormat dstFormat = _screenSurface16.format;

		const byte *srcRow = (const byte *)_clippedCursorSurface.getPixels();
		uint16 *dstRow = (uint16 *)_screenSurface16.getBasePtr(_cursorRect.left, _cursorRect.top + screenCorrection);

		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				byte index = *srcRow++;
				if (index != _cursorKeycolor) {
					byte r = palette[index * 3];
					byte g = palette[index * 3 + 1];
					byte b = palette[index * 3 + 2];

					*dstRow++ = dstFormat.RGBToColor(r, g, b);
				} else {
					dstRow++;
				}
			}

			srcRow += _cursorSurface.w - w;
			dstRow += _screenSurface16.w - w;
		}
	}
}
