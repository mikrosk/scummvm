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

#include "common/rect.h"
#include "common/str.h"

#define SCREEN_WIDTH	320
#define SCREEN_HEIGHT	240

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
			_chunkyBuffer = (byte*)Mxalloc(SCREEN_WIDTH * SCREEN_HEIGHT + 15, MX_PREFTTRAM);
			if (!_chunkyBuffer)
				return OSystem::TransactionError::kTransactionSizeChangeFailed;

			_chunkyBufferAligned = (byte*)(((unsigned long)_chunkyBuffer + 15) & 0xfffffff0);
			memset(_chunkyBufferAligned, 0, SCREEN_WIDTH * SCREEN_HEIGHT);

			_chunkySurface.init(_width, _height, _width, _chunkyBufferAligned, _format);

			_screen = (byte*)Mxalloc(SCREEN_WIDTH * SCREEN_HEIGHT + 15, MX_STRAM);
			if (!_screen)
				return OSystem::TransactionError::kTransactionSizeChangeFailed;

			_screenAligned = (byte*)(((unsigned long)_screen + 15) & 0xfffffff0);
			memset(_screenAligned, 0, SCREEN_WIDTH * SCREEN_HEIGHT);
#ifdef SCREEN_ACTIVE
			VsetScreen(SCR_NOCHANGE, _screenAligned, SCR_NOCHANGE, SCR_NOCHANGE);
#endif
			_overlayBuffer = (uint16*)Mxalloc(getOverlayWidth() * getOverlayHeight() * getOverlayFormat().bytesPerPixel, MX_STRAM);
			if (!_overlayBuffer)
				return OSystem::TransactionError::kTransactionSizeChangeFailed;

			memset(_overlayBuffer, 0, getOverlayWidth() * getOverlayHeight() * getOverlayFormat().bytesPerPixel);

			_overlaySurface.init(getOverlayWidth(),
				getOverlayHeight(),
				getOverlayWidth() * getOverlayFormat().bytesPerPixel,
				_overlayBuffer,
				getOverlayFormat());
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
	if (_cursorModified) {
		_cursorModified = false;

		if (_mouseX != -1 && _mouseY != -1) {
			if (isOverlayVisible()) {
				updateOverlayCursor();
				return;
			}

			// TODO: clip
			_chunkySurface.copyRectToSurface(_cursorSurface,
				_mouseX - _cursorHotspotX,
				_mouseY - _cursorHotspotY,
				Common::Rect(_cursorWidth, _cursorHeight));

			_screenModified = true;
		}
	}

	if (_screenModified) {
		Common::String str = Common::String::format("updateScreen\n");
		g_system->logMessage(LogMessageType::kDebug, str.c_str());

		// TODO: c2p (maybe remember updated rects from copyRectToScreen? esp. useful for cursor updates)
		_screenModified = false;
	}
}

void AtariGraphicsManager::showOverlay() {
	Common::String str = Common::String::format("showOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (_overlayVisible || !_overlayBuffer)
		return;

#ifdef SCREEN_ACTIVE
	int16 old_mode = VsetMode(VM_INQUIRE);
	VsetMode(VERTFLAG | (old_mode&PAL) | (old_mode&VGA) | COL40 | BPS16);
	VsetScreen(SCR_NOCHANGE, _overlayBuffer, SCR_NOCHANGE, SCR_NOCHANGE);
#endif
	_overlayVisible = true;
}

void AtariGraphicsManager::hideOverlay() {
	Common::String str = Common::String::format("hideOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (!_overlayVisible || !_screenAligned)
		return;

#ifdef SCREEN_ACTIVE
	int16 old_mode = VsetMode(VM_INQUIRE);
	VsetMode(VERTFLAG | (old_mode&PAL) | (old_mode&VGA) | COL40 | BPS8);
	VsetScreen(SCR_NOCHANGE, _screenAligned, SCR_NOCHANGE, SCR_NOCHANGE);
#endif
	_overlayVisible = false;
}

void AtariGraphicsManager::clearOverlay() {
	Common::String str = Common::String::format("clearOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	//memset(_overlayBuffer, 0, getOverlayWidth() * getOverlayHeight() * getOverlayFormat().bytesPerPixel);
}

void AtariGraphicsManager::grabOverlay(Graphics::Surface &surface) const {
	Common::String str = Common::String::format("grabOverlay: %d, %d, %d\n", surface.pitch, surface.w, surface.h);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	surface.copyFrom(_overlaySurface);
}

void AtariGraphicsManager::copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) {
	Common::String str = Common::String::format("copyRectToOverlay: %d, %d, %d, %d, %d\n", pitch, x, y, w, h);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	_overlaySurface.copyRectToSurface(buf, pitch, x, y, w, h);
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

	if (_mouseX == -1 || _mouseY == -1) {
		if (isOverlayVisible()) {
			_mouseX = getOverlayWidth() / 2;
			_mouseY = getOverlayHeight() / 2;
		} else {
			_mouseX = getWidth() / 2;
			_mouseY = getHeight() / 2;
		}
	}

	_cursorModified = true;

	bool last = _mouseVisible;
	_mouseVisible = visible;
	return last;
}

void AtariGraphicsManager::warpMouse(int x, int y) {
	Common::String str = Common::String::format("warpMouse: %d, %d\n", x, y);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (_mouseX == x && _mouseY == y)
		return;

	_mouseX = x;
	_mouseY = y;

	_cursorModified = true;
}

void AtariGraphicsManager::setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor, bool dontScale, const Graphics::PixelFormat *format) {
	Common::String str = Common::String::format("setMouseCursor: %d, %d, %d, %d, %d, %p\n", w, h, hotspotX, hotspotY, keycolor, (const void*)format);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (format != nullptr && *format != _format)
		return;

	if (w == 0 || h == 0) {
		_cursorSurface.free();
		return;
	}

	if (_cursorWidth != w && _cursorHeight != h) {
		_cursorSurface.free();
		_cursorSurface.create(w, h, _format);
	}

	if (_cursorKeycolor != keycolor)
		_cursorSurface.fillRect(Common::Rect(_cursorSurface.w, _cursorSurface.h), keycolor);

	_cursorSurface.copyRectToSurfaceWithKey(buf, w, 0, 0, w, h, keycolor);

	_cursorWidth = w;
	_cursorHeight = h;
	_cursorHotspotX = hotspotX;
	_cursorHotspotY = hotspotY;
	_cursorKeycolor = keycolor;

	_cursorModified = true;
}

void AtariGraphicsManager::updateOverlayCursor()
{
	static byte palette[256*3] = {};

	// TODO: system palette?
	static bool paletteInitialised;
	if (!paletteInitialised) {
		for (int i = 0; i < 4; ++i) {
			palette[i * 3 + 0] = (byte)(63 + i * 64);
			palette[i * 3 + 1] = (byte)(63 + i * 64);
			palette[i * 3 + 2] = (byte)(63 + i * 64);
		}
		paletteInitialised = true;
	}

	const Graphics::PixelFormat dstFormat = getOverlayFormat();

	// TODO: clipping
	for (int y = 0; y < _cursorSurface.h; y++) {
		const byte *srcRow = (const byte *)_cursorSurface.getBasePtr(0, y);
		byte *dstRow = (byte *)_overlaySurface.getBasePtr(_mouseX - _cursorHotspotX, _mouseY - _cursorHotspotY + y);

		for (int x = 0; x < _cursorSurface.w; x++) {
			byte index = *srcRow++;
			byte r = palette[index * 3];
			byte g = palette[index * 3 + 1];
			byte b = palette[index * 3 + 2];

			*((uint16 *)dstRow) = dstFormat.RGBToColor(r, g, b);

			dstRow += dstFormat.bytesPerPixel;
		}
	}
}
