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

#include <mint/falcon.h>
#include <mint/osbind.h>

#include "common/str.h"

// max(screen, overlay)
#define SCREEN_WIDTH	320
#define SCREEN_HEIGHT	240
#define SCREEN_DEPTH	2

#define SCREEN_ACTIVE

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
		int16 old_mode = VsetMode(VM_INQUIRE);
		VsetMode(VERTFLAG | (old_mode&PAL) | (old_mode&VGA) | COL40 | BPS8);
#endif
		_oldFormat = _format;
	}

	if (_width > SCREEN_WIDTH || _height > SCREEN_HEIGHT)
		return OSystem::TransactionError::kTransactionSizeChangeFailed;

	if (_oldWidth != _width || _oldHeight != _height) {
		if (_screen == nullptr) {
			// no need to realloc each time

			// TODO: Mfree() in the destructor
			_chunkyBuffer = (byte*)Mxalloc(SCREEN_WIDTH * SCREEN_HEIGHT + 15, MX_PREFTTRAM);
			if (!_chunkyBuffer)
				return OSystem::TransactionError::kTransactionSizeChangeFailed;

			byte* chunkyBufferAligned = (byte*)(((unsigned long)_chunkyBuffer + 15) & 0xfffffff0);
			memset(chunkyBufferAligned, 0, SCREEN_WIDTH * SCREEN_HEIGHT);

			_chunkySurface.init(_width, _height, _width, chunkyBufferAligned, _format);

			// TODO: Mfree() in the destructor
			_screen = (byte*)Mxalloc(SCREEN_WIDTH * SCREEN_HEIGHT * SCREEN_DEPTH + 15, MX_STRAM);
			if (!_screen)
				return OSystem::TransactionError::kTransactionSizeChangeFailed;

			byte* screenAligned = (byte*)(((unsigned long)_screen + 15) & 0xfffffff0);
			memset(screenAligned, 0, SCREEN_WIDTH * SCREEN_HEIGHT * SCREEN_DEPTH);

			_screenSurface8.init(_width, _height, _width * _format.bytesPerPixel, screenAligned, _format);
#ifdef SCREEN_ACTIVE
			VsetScreen(SCR_NOCHANGE, _screenSurface8.getPixels(), SCR_NOCHANGE, SCR_NOCHANGE);
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
	Common::String str = Common::String::format("setPalette: %d, %d\n", start, num);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());
}

void AtariGraphicsManager::grabPalette(byte *colors, uint start, uint num) const {
	Common::String str = Common::String::format("grabPalette: %d, %d\n", start, num);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());
}

void AtariGraphicsManager::copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) {
	Common::String str = Common::String::format("copyRectToScreen: %d, %d, %d, %d, %d\n", pitch, x, y, w, h);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	_chunkySurface.copyRectToSurface(buf, pitch, x, y, w, h);

	_screenModified = true;
}

Graphics::Surface *AtariGraphicsManager::lockScreen() {
	Common::String str = Common::String::format("lockScreen\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	return &_chunkySurface;
}

void AtariGraphicsManager::fillScreen(uint32 col) {
	Common::String str = Common::String::format("fillScreen: %d\n", col);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	Graphics::Surface *screen = lockScreen();
	if (screen)
		screen->fillRect(Common::Rect(_width, _height), col);
	unlockScreen();

	_screenModified = true;
}

void AtariGraphicsManager::updateScreen() {
	if (_screenModified) {
		Common::String str = Common::String::format("updateScreen\n");
		g_system->logMessage(LogMessageType::kDebug, str.c_str());

		// TODO: maybe remember updated rects from copyRectToScreen / copyRectToOverlay?
		if (isOverlayVisible()) {
			//_screenSurface16.copyRectToSurface(_overlaySurface, 0, 0, Common::rect(_overlaySurface.w, _overlaySurface.h));
			memcpy(_screenSurface16.getPixels(), _overlaySurface.getPixels(), _overlaySurface.pitch * _overlaySurface.h);
		} else {
			// TODO: c2p
		}

		_screenModified = false;
	}

	if (_mouseVisible) {
		updateCursorRect();
	} else if (!_cursorRect.isEmpty()) {
		// force cursor background restore
		_oldCursorRect = _cursorRect;
		_cursorRect = Common::Rect();
	}

	if (!_oldCursorRect.isEmpty() && _oldCursorRect != _cursorRect) {
		if (isOverlayVisible()) {
			_screenSurface16.copyRectToSurface(
				_overlaySurface.getSubArea(_oldCursorRect),
				_oldCursorRect.left, _oldCursorRect.top,
				Common::Rect(_oldCursorRect.width(), _oldCursorRect.height()));
		} else {
			// TODO: c2p
		}

		_oldCursorRect = _cursorRect;
	}

	// TODO: mask & detect movement
	// this is the simplest approach, we don't have to worry whether the overlay didn't overwrite the (non-moving) cursor
	if (_mouseVisible) {
		if (isOverlayVisible()) {
			updateOverlayCursor();
		} else {
			// TODO: c2p
		}
	}
}

void AtariGraphicsManager::showOverlay() {
	Common::String str = Common::String::format("showOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (_overlayVisible)
		return;

#ifdef SCREEN_ACTIVE
	int16 old_mode = VsetMode(VM_INQUIRE);
	VsetMode(VERTFLAG | (old_mode&PAL) | (old_mode&VGA) | COL40 | BPS16);
#endif
	_overlayVisible = true;
}

void AtariGraphicsManager::hideOverlay() {
	Common::String str = Common::String::format("hideOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (!_overlayVisible)
		return;

#ifdef SCREEN_ACTIVE
	int16 old_mode = VsetMode(VM_INQUIRE);
	VsetMode(VERTFLAG | (old_mode&PAL) | (old_mode&VGA) | COL40 | BPS8);
#endif
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
	Common::String str = Common::String::format("copyRectToOverlay: %d, %d, %d, %d, %d\n", pitch, x, y, w, h);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	_overlaySurface.copyRectToSurface(buf, pitch, x, y, w, h);

	_screenModified = true;
}

int16 AtariGraphicsManager::getOverlayHeight() const {
	return SCREEN_HEIGHT;
}

int16 AtariGraphicsManager::getOverlayWidth() const {
	return SCREEN_WIDTH;
}

bool AtariGraphicsManager::showMouse(bool visible) {
	Common::String str = Common::String::format("showMouse: %d\n", visible);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

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

	if (format != nullptr && *format != _format)
		return;

	if (w == 0 || h == 0 || buf == nullptr) {
		_cursorWidth = _cursorHeight = 0;
		_cursorHotspotX = _cursorHotspotY = 0;
		_cursorKeycolor = 0;

		_cursorSurface.free();
		return;
	}

	if (_cursorWidth != w && _cursorHeight != h) {
		_cursorSurface.free();
		_cursorSurface.create(w, h, _format);
	}

	_cursorSurface.copyRectToSurface(buf, w, 0, 0, w, h);

	_cursorWidth = w;
	_cursorHeight = h;
	_cursorHotspotX = hotspotX;
	_cursorHotspotY = hotspotY;
	_cursorKeycolor = keycolor;
}

void AtariGraphicsManager::updateCursorRect()
{
	_oldCursorRect = _cursorRect;

	Common::Rect cursorSrcBounds(_cursorSurface.w, _cursorSurface.h);
	Common::Rect cursorDstBounds(
		_mouseX - _cursorHotspotX,	// left
		_mouseY - _cursorHotspotY,	// top
		_mouseX - _cursorHotspotX + _cursorSurface.w,	// right
		_mouseY - _cursorHotspotY + _cursorSurface.h);	// bottom

#if 0
	{Common::String str = Common::String::format("cursorSrcBounds 1: %d, %d, %d, %d\n",
		cursorSrcBounds.left, cursorSrcBounds.top, cursorSrcBounds.width(), cursorSrcBounds.height());
	g_system->logMessage(LogMessageType::kDebug, str.c_str());}
	{Common::String str = Common::String::format("cursorDstBounds 1: %d, %d, %d, %d\n",
		cursorDstBounds.left, cursorDstBounds.top, cursorDstBounds.width(), cursorDstBounds.height());
	g_system->logMessage(LogMessageType::kDebug, str.c_str());}
#endif

	_screenSurface16.clip(cursorSrcBounds, cursorDstBounds);

#if 0
	{Common::String str = Common::String::format("cursorSrcBounds 2: %d, %d, %d, %d\n",
		cursorSrcBounds.left, cursorSrcBounds.top, cursorSrcBounds.width(), cursorSrcBounds.height());
	g_system->logMessage(LogMessageType::kDebug, str.c_str());}
	{Common::String str = Common::String::format("cursorDstBounds 2: %d, %d, %d, %d\n",
		cursorDstBounds.left, cursorDstBounds.top, cursorDstBounds.width(), cursorDstBounds.height());
	g_system->logMessage(LogMessageType::kDebug, str.c_str());}
#endif

	_clippedCursorSurface = _cursorSurface.getSubArea(cursorSrcBounds);

	_cursorRect.left = cursorDstBounds.left;
	_cursorRect.top = cursorDstBounds.top;
	_cursorRect.setWidth(_clippedCursorSurface.w);
	_cursorRect.setHeight(_clippedCursorSurface.h);

#if 0
	{Common::String str = Common::String::format("_cursorRect 2: %d, %d, %d, %d\n",
		_cursorRect.left, _cursorRect.top, _cursorRect.width(), _cursorRect.height());
	g_system->logMessage(LogMessageType::kDebug, str.c_str());}
#endif
}

void AtariGraphicsManager::updateOverlayCursor()
{
	static byte palette[256*3] = {};

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

	const Graphics::PixelFormat dstFormat = getOverlayFormat();

	// faster (no memory allocation) version of Surface::convertTo()
	for (int y = 0; y < _clippedCursorSurface.h; y++) {
		const byte *srcRow = (const byte *)_clippedCursorSurface.getBasePtr(0, y);
		byte *dstRow = (byte *)_screenSurface16.getBasePtr(_cursorRect.left, _cursorRect.top + y);

		for (int x = 0; x < _clippedCursorSurface.w; x++) {
			byte index = *srcRow++;
			if (index != _cursorKeycolor) {
				byte r = palette[index * 3];
				byte g = palette[index * 3 + 1];
				byte b = palette[index * 3 + 2];

				*((uint16 *)dstRow) = dstFormat.RGBToColor(r, g, b);
			}

			dstRow += dstFormat.bytesPerPixel;
		}
	}
}
