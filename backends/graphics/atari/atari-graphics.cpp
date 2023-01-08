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
#include "backends/graphics/atari/640x480x8_vga.h"
#include "backends/graphics/atari/atari_c2p-asm.h"
#include "backends/graphics/atari/atari-graphics-asm.h"
#include "common/foreach.h"
#include "common/str.h"

#define OVERLAY_WIDTH	320
#define OVERLAY_HEIGHT	240

// maximum screen dimensions
#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480

#define SCREEN_ACTIVE

AtariGraphicsManager::~AtariGraphicsManager() {
	Mfree(_chunkyBuffer);
	_chunkyBuffer = nullptr;

	Mfree(_screen);
	_screen = nullptr;
}

bool AtariGraphicsManager::hasFeature(OSystem::Feature f) const {
	switch (f) {
	case OSystem::Feature::kFeatureCursorPalette:
		{Common::String str = Common::String::format("hasFeature(kFeatureCursorPalette): %d\n", isOverlayVisible());
		g_system->logMessage(LogMessageType::kDebug, str.c_str());}
		return true;
	default:
		return false;
	}
}

bool AtariGraphicsManager::setGraphicsMode(int mode, uint flags) {
	Common::String str = Common::String::format("setGraphicsMode: %d, %d\n", mode, flags);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	return (mode == 0);
}

void AtariGraphicsManager::initSize(uint width, uint height, const Graphics::PixelFormat *format) {
	Common::String str = Common::String::format("initSize: %d, %d, %d\n", width, height, format ? format->bytesPerPixel : 1);
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

	if ((_width != 320 || (_height != 200 && _height != 240))
		&& (_width != 640 || (_height != 400 && _height != 480)))
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

			_screen = (byte*)Mxalloc(SCREEN_WIDTH * SCREEN_HEIGHT + 15, MX_STRAM);
			if (!_screen)
				return OSystem::TransactionError::kTransactionSizeChangeFailed;

			byte* screenAligned = (byte*)(((unsigned long)_screen + 15) & 0xfffffff0);
			memset(screenAligned, 0, SCREEN_WIDTH * SCREEN_HEIGHT);

			_screenSurface8.init(_width, _height, _width * _format.bytesPerPixel, screenAligned, _format);

			_overlaySurface.create(getOverlayWidth(), getOverlayHeight(), getOverlayFormat());
			_screenSurface16.init(_overlaySurface.w, _overlaySurface.h, _overlaySurface.pitch, screenAligned, _overlaySurface.format);
		} else {
			_chunkySurface.init(_width, _height, _width, _chunkySurface.getPixels(), _format);
			_screenSurface8.init(_width, _height, _width, _screenSurface8.getPixels(), _format);
		}

#ifdef SCREEN_ACTIVE
		if (_width == 320)
			asm_screen_set_scp_res(scp_320x240x8_vga);
		else
			asm_screen_set_scp_res(scp_640x480x8_vga);

		asm_screen_set_vram(_screenSurface8.getPixels());
#endif
		if (_height == 200)
			_screenCorrection = (240 - _height) / 2;
		else if (_height == 400)
			_screenCorrection = (480 - _height) / 2;
		else
			_screenCorrection = 0;

		_mouseX = _width / 2;
		_mouseY = _height / 2;

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
	//Common::String str = Common::String::format("lockScreen\n");
	//g_system->logMessage(LogMessageType::kDebug, str.c_str());

	return &_chunkySurface;
}

void AtariGraphicsManager::fillScreen(uint32 col) {
	{Common::String str = Common::String::format("fillScreen: %d\n", col);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());}

	const Common::Rect rect = Common::Rect(_chunkySurface.w, _chunkySurface.h);

	_chunkySurface.fillRect(rect, col);

	handleModifiedRect(rect, _modifiedChunkyRects, _chunkySurface);
}

// TODO: double buffering & avoid _overlaySurface / _chunkyBufferSurface
//       when in a native format (16bpp, SuperVidel+8bpp). This requires
//       special cursor handling as we'd need to store the rect underneath it (or better, use a mask).
void AtariGraphicsManager::updateScreen() {
	//Common::String str = Common::String::format("updateScreen\n");
	//g_system->logMessage(LogMessageType::kDebug, str.c_str());

	// prepare _cursorRect first
	if (_mouseVisible) {
		updateCursorRect();
	} else if (!_cursorRect.isEmpty()) {
		// force cursor background restore
		_oldCursorRect = _cursorRect;
		_cursorRect = Common::Rect();
	}

	const bool updateCursor = !_mouseOutOfScreen && _mouseVisible && !_cursorRect.isEmpty();

	while (!_modifiedOverlayRects.empty()) {
		const Common::Rect &rect = _modifiedOverlayRects.back();

		if (!_cursorModified && updateCursor)
			_cursorModified = rect.intersects(_cursorRect);

		_screenSurface16.copyRectToSurface(
			_overlaySurface.getBasePtr(rect.left, rect.top),
			_overlaySurface.pitch,
			rect.left, rect.top,
			rect.width(), rect.height());

		_modifiedOverlayRects.pop_back();
	}

	while (!_modifiedChunkyRects.empty()) {
		const Common::Rect &rect = _modifiedChunkyRects.back();

		if (!_cursorModified && updateCursor)
			_cursorModified = rect.intersects(_cursorRect);

		c2p1x1_8_falcon(
			(char*)_chunkySurface.getBasePtr(rect.left, rect.top),
			(char*)_chunkySurface.getBasePtr(rect.right, rect.bottom),
			rect.width(),
			_chunkySurface.pitch,
			(char*)_screenSurface8.getBasePtr(rect.left, rect.top + _screenCorrection),
			_screenSurface8.pitch);

		_modifiedChunkyRects.pop_back();
	}

	if (_mouseOutOfScreen)
		return;

	if (_oldCursorRect != _cursorRect) {
		if (!_oldCursorRect.isEmpty()) {
			if (isOverlayVisible()) {
				_screenSurface16.copyRectToSurface(
					_overlaySurface.getBasePtr(_oldCursorRect.left, _oldCursorRect.top),
					_overlaySurface.pitch,
					_oldCursorRect.left, _oldCursorRect.top,
					_oldCursorRect.width(), _oldCursorRect.height());
			} else {
				c2p1x1_8_falcon(
					(char*)_chunkySurface.getBasePtr(_oldCursorRect.left, _oldCursorRect.top),
					(char*)_chunkySurface.getBasePtr(_oldCursorRect.right, _oldCursorRect.bottom),
					_oldCursorRect.width(),
					_chunkySurface.w,
					(char*)_screenSurface8.getBasePtr(_oldCursorRect.left, _oldCursorRect.top + _screenCorrection),
					_screenSurface8.pitch);
			}
		}

		_oldCursorRect = _cursorRect;
	}

	if (updateCursor && _cursorModified) {
		//Common::String str = Common::String::format("Redraw cursor: %d %d %d %d\n",
		//											_cursorRect.left, _cursorRect.top, _cursorRect.width(), _cursorRect.height());
		//g_system->logMessage(LogMessageType::kDebug, str.c_str());

		if (isOverlayVisible()) {
			copyCursorSurface16();
		} else {
			c2p1x1_8_falcon(
				(char*)_cursorSurface8.getPixels(),
				(char*)_cursorSurface8.getBasePtr(0, _cursorSurface8.h),
				_cursorSurface8.w,
				_cursorSurface8.pitch,
				(char*)_screenSurface8.getBasePtr(_cursorRect.left, _cursorRect.top + _screenCorrection),
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

	_oldCursorRect = Common::Rect();
	_modifiedChunkyRects.clear();
	handleModifiedRect(Common::Rect(_overlaySurface.w, _overlaySurface.h), _modifiedOverlayRects, _overlaySurface);

	_overlayVisible = true;
}

void AtariGraphicsManager::hideOverlay() {
	Common::String str = Common::String::format("hideOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (!_overlayVisible)
		return;

#ifdef SCREEN_ACTIVE
	memset(_screenSurface16.getPixels(), 0, _screenSurface16.pitch * _screenSurface16.h);

	if (_width == 320)
		asm_screen_set_scp_res(scp_320x240x8_vga);
	else
		asm_screen_set_scp_res(scp_640x480x8_vga);
#endif

	_oldCursorRect = Common::Rect();
	_modifiedOverlayRects.clear();
	handleModifiedRect(Common::Rect(_chunkySurface.w, _chunkySurface.h), _modifiedChunkyRects, _chunkySurface);

	_overlayVisible = false;
}

void AtariGraphicsManager::clearOverlay() {
	Common::String str = Common::String::format("clearOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	memset(_overlaySurface.getPixels(), 0, _overlaySurface.pitch * _overlaySurface.h);
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
	return OVERLAY_HEIGHT;
}

int16 AtariGraphicsManager::getOverlayWidth() const {
	return OVERLAY_WIDTH;
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

	_cursorModified = true;
}

void AtariGraphicsManager::setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor, bool dontScale, const Graphics::PixelFormat *format) {
	//Common::String str = Common::String::format("setMouseCursor: %d, %d, %d, %d, %d, %d\n", w, h, hotspotX, hotspotY, keycolor, format ? format->bytesPerPixel : 1);
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

void AtariGraphicsManager::setCursorPalette(const byte *colors, uint start, uint num) {
	Common::String str = Common::String::format("setCursorPalette: %d, %d\n", start, num);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	byte *pal = &_overlayCursorPalette[start];

	for (uint i = 0; i < num; ++i) {
		pal[i * 3 + 0] = colors[i * 3 + 0];
		pal[i * 3 + 1] = colors[i * 3 + 1];
		pal[i * 3 + 2] = colors[i * 3 + 2];
	}
}

void AtariGraphicsManager::updateMousePosition(int deltaX, int deltaY)
{
	_mouseX += deltaX;
	_mouseY += deltaY;

	const int maxX = isOverlayVisible() ? getOverlayWidth() : getWidth();
	const int maxY = isOverlayVisible() ? getOverlayHeight() : getHeight();

	if (_mouseX < 0)
		_mouseX = 0;
	else if (_mouseX >= maxX)
		_mouseX = maxX - 1;

	if (_mouseY < 0)
		_mouseY = 0;
	else if (_mouseY >= maxY)
		_mouseY = maxY - 1;

	_cursorModified = true;
}

void AtariGraphicsManager::handleModifiedRect(Common::Rect rect, Common::Array<Common::Rect> &rects, const Graphics::Surface &surface)
{
	if (surface.format.bytesPerPixel == 1) {
		// align on 16px
		rect.left &= 0xfff0;
		rect.right = (rect.right + 15) & 0xfff0;
		if (rect.right > surface.w)
			rect.right = surface.w;
	}

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

	if (cursorSrcBounds.isEmpty()) {
		_cursorRect = Common::Rect();
		return;
	}

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

	if (!isOverlayVisible())
		prepareCursorSurface8();
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

void AtariGraphicsManager::copyCursorSurface16() {
	// TODO: mask out old cursor and mask in new one?
	if (_cursorSurface.format == _screenSurface16.format) {
		_screenSurface16.copyRectToSurfaceWithKey(
			_clippedCursorSurface,
			_cursorRect.left, _cursorRect.top,
			_cursorRect,
			_cursorKeycolor);
	} else {
		// faster (no memory allocation) version of Surface::convertTo()
		const int w = _cursorRect.width();
		const int h = _cursorRect.height();
		const Graphics::PixelFormat dstFormat = _screenSurface16.format;

		const byte *srcRow = (const byte *)_clippedCursorSurface.getPixels();
		uint16 *dstRow = (uint16 *)_screenSurface16.getBasePtr(_cursorRect.left, _cursorRect.top);

		const uint16 cursorKeyColor = dstFormat.RGBToColor(
			_overlayCursorPalette[_cursorKeycolor * 3 + 0],
			_overlayCursorPalette[_cursorKeycolor * 3 + 1],
			_overlayCursorPalette[_cursorKeycolor * 3 + 2]);

		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				const byte index = *srcRow++;

				uint16 pixel = dstFormat.RGBToColor(
					_overlayCursorPalette[index * 3 + 0],
					_overlayCursorPalette[index * 3 + 1],
					_overlayCursorPalette[index * 3 + 2]);

				if (pixel != cursorKeyColor) {
					*dstRow++ = pixel;
				} else {
					dstRow++;
				}
			}

			srcRow += _cursorSurface.w - w;
			dstRow += _screenSurface16.w - w;
		}
	}
}
