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

#include "backends/graphics/atari/320x200x8_rgb.h"
#include "backends/graphics/atari/320x200x8_rgb60.h"
#include "backends/graphics/atari/320x200x8_vga.h"
#include "backends/graphics/atari/320x240x8_rgb.h"
#include "backends/graphics/atari/320x240x8_vga.h"
#include "backends/graphics/atari/320x240x16_rgb.h"
#include "backends/graphics/atari/320x240x16_vga.h"
#include "backends/graphics/atari/640x400x8_rgb.h"
#include "backends/graphics/atari/640x400x8_rgb60.h"
#include "backends/graphics/atari/640x400x8_vga.h"
#include "backends/graphics/atari/640x480x8_rgb.h"
#include "backends/graphics/atari/640x480x8_vga.h"

#include "backends/graphics/atari/atari_c2p-asm.h"
#include "backends/graphics/atari/atari-graphics-asm.h"
#include "backends/keymapper/action.h"
#include "backends/keymapper/keymap.h"

#include "common/foreach.h"
#include "common/str.h"
#include "common/translation.h"

#define OVERLAY_WIDTH	320
#define OVERLAY_HEIGHT	240

// maximum screen dimensions
#define SCREEN_SIZE	(640*480*1)

#define SCREEN_ACTIVE

AtariGraphicsManager::AtariGraphicsManager() {
	_vgaMonitor = VgetMonitor() == MON_VGA;

	g_system->getEventManager()->getEventDispatcher()->registerObserver(this, 10, false);
}

AtariGraphicsManager::~AtariGraphicsManager() {
	g_system->getEventManager()->getEventDispatcher()->unregisterObserver(this);

	Mfree(_chunkyBuffer);
	_chunkyBuffer = nullptr;

	Mfree(_overlayBuffer);
	_overlayBuffer = nullptr;

	Mfree(_screen);
	_screen = nullptr;

	Mfree(_overlay);
	_overlay = nullptr;
}

bool AtariGraphicsManager::hasFeature(OSystem::Feature f) const {
	switch (f) {
	case OSystem::Feature::kFeatureCursorPalette:
		{Common::String str = Common::String::format("hasFeature(kFeatureCursorPalette): %d\n", isOverlayVisible());
		g_system->logMessage(LogMessageType::kDebug, str.c_str());}
		return true;
	case OSystem::Feature::kFeatureAspectRatioCorrection:
		{Common::String str = Common::String::format("hasFeature(kFeatureAspectRatioCorrection): %d\n", !_vgaMonitor);
		g_system->logMessage(LogMessageType::kDebug, str.c_str());}
		return !_vgaMonitor;
	default:
		return false;
	}
}

void AtariGraphicsManager::setFeatureState(OSystem::Feature f, bool enable) {
	switch (f) {
	case OSystem::Feature::kFeatureAspectRatioCorrection:
		{Common::String str = Common::String::format("setFeatureState(kFeatureAspectRatioCorrection): %d\n", enable);
		g_system->logMessage(LogMessageType::kDebug, str.c_str());}
		_oldAspectRatioCorrection = _aspectRatioCorrection;
		_aspectRatioCorrection = enable;
		break;
	default:
		[[fallthrough]];
	}
}

bool AtariGraphicsManager::getFeatureState(OSystem::Feature f) const {
	switch (f) {
	case OSystem::Feature::kFeatureCursorPalette:
		return true;
	case OSystem::Feature::kFeatureAspectRatioCorrection:
		return _aspectRatioCorrection;
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
		bool firstRun = false;

		if (_screen == nullptr) {
			// no need to realloc each time

			if (!allocateAtariSurface(_chunkyBuffer, _chunkySurface, _width, _height, _format, MX_PREFTTRAM, SCREEN_SIZE))
				return OSystem::TransactionError::kTransactionSizeChangeFailed;

			if (!allocateAtariSurface(_overlayBuffer, _overlaySurface, getOverlayWidth(), getOverlayHeight(), getOverlayFormat(), MX_PREFTTRAM))
				return OSystem::TransactionError::kTransactionSizeChangeFailed;

			if (!allocateAtariSurface(_screen, _screenSurface8, _width, _height, _format, MX_STRAM, SCREEN_SIZE))
				return OSystem::TransactionError::kTransactionSizeChangeFailed;

			if (!allocateAtariSurface(_overlay, _screenSurface16, getOverlayWidth(), getOverlayHeight(), getOverlayFormat(), MX_STRAM))
				return OSystem::TransactionError::kTransactionSizeChangeFailed;

			firstRun = true;
		} else {
			_chunkySurface.init(_width, _height, _width, _chunkySurface.getPixels(), _format);
			_screenSurface8.init(_width, _height, _width, _screenSurface8.getPixels(), _format);
		}

		// some games do not initialize their viewport entirely
		// (sword1 sets 640x480 but always copies only 640x400...)
		_chunkySurface.fillRect(Common::Rect(_chunkySurface.w, _chunkySurface.h), 0);
		handleModifiedRect(Common::Rect(_chunkySurface.w, _chunkySurface.h), _modifiedChunkyRects, _chunkySurface);

#ifdef SCREEN_ACTIVE
		if (firstRun)
			asm_screen_set_falcon_palette(_palette);
		_pendingResolutionChange = PendingResolutionChange::Screen;
#endif
		warpMouse(_width / 2, _height / 2);
		if (firstRun) {
			_oldMouseX = getOverlayWidth() / 2;
			_oldMouseY = getOverlayHeight() / 2;
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

#ifdef SCREEN_ACTIVE
	bool resolutionChanged = false;

	switch (_pendingResolutionChange) {
	case PendingResolutionChange::Overlay:
		if (_vgaMonitor)
			asm_screen_set_scp_res(scp_320x240x16_vga);
		else
			asm_screen_set_scp_res(scp_320x240x16_rgb);

		asm_screen_set_vram(_screenSurface16.getPixels());

		_pendingResolutionChange = PendingResolutionChange::None;
		resolutionChanged = true;
		break;

	case PendingResolutionChange::Screen:
		setVidelResolution();
		asm_screen_set_vram(_screenSurface8.getPixels());

		_pendingResolutionChange = PendingResolutionChange::None;
		resolutionChanged = true;
		break;

	case PendingResolutionChange::None:
		[[fallthrough]];
	}

	if (_oldAspectRatioCorrection != _aspectRatioCorrection) {
		if (!isOverlayVisible() && !resolutionChanged) {
			setVidelResolution();
		}
		_oldAspectRatioCorrection = _aspectRatioCorrection;
	}
#endif

	// prepare _cursorRect first
	if (_mouseVisible) {
		updateCursorRect();
	} else if (!_cursorDstRect.isEmpty()) {
		// force cursor background restore
		_oldCursorRect = _cursorDstRect;
		_cursorDstRect = Common::Rect();
	}

	const bool updateCursor = !_mouseOutOfScreen && _mouseVisible && !_cursorDstRect.isEmpty();

	while (!_modifiedOverlayRects.empty()) {
		const Common::Rect &rect = _modifiedOverlayRects.back();

		if (!_cursorModified && isOverlayVisible() && updateCursor)
			_cursorModified = rect.intersects(_cursorDstRect);

		_screenSurface16.copyRectToSurface(_overlaySurface, rect.left, rect.top, rect);

		_modifiedOverlayRects.pop_back();
	}

	while (!_modifiedChunkyRects.empty()) {
		const Common::Rect &rect = _modifiedChunkyRects.back();

		if (!_cursorModified && !isOverlayVisible() && updateCursor)
			_cursorModified = rect.intersects(_cursorDstRect);

		c2p1x1_8_falcon(
			(char*)_chunkySurface.getBasePtr(rect.left, rect.top),
			(char*)_chunkySurface.getBasePtr(rect.right, rect.bottom),
			rect.width(),
			_chunkySurface.pitch,
			(char*)_screenSurface8.getBasePtr(rect.left, rect.top),
			_screenSurface8.pitch);

		_modifiedChunkyRects.pop_back();
	}

	if (_mouseOutOfScreen)
		return;

	if (_oldCursorRect != _cursorDstRect) {
		if (!_oldCursorRect.isEmpty()) {
			if (isOverlayVisible()) {
				_screenSurface16.copyRectToSurface(_overlaySurface, _oldCursorRect.left, _oldCursorRect.top, _oldCursorRect);
			} else {
				c2p1x1_8_falcon(
					(char*)_chunkySurface.getBasePtr(_oldCursorRect.left, _oldCursorRect.top),
					(char*)_chunkySurface.getBasePtr(_oldCursorRect.right, _oldCursorRect.bottom),
					_oldCursorRect.width(),
					_chunkySurface.w,
					(char*)_screenSurface8.getBasePtr(_oldCursorRect.left, _oldCursorRect.top),
					_screenSurface8.pitch);
			}
		}

		_oldCursorRect = _cursorDstRect;
	}

	if (updateCursor && _cursorModified) {
		//Common::String str = Common::String::format("Redraw cursor: %d %d %d %d\n",
		//											_cursorRect.left, _cursorRect.top, _cursorRect.width(), _cursorRect.height());
		//g_system->logMessage(LogMessageType::kDebug, str.c_str());

		if (isOverlayVisible()) {
			copySurface8ToSurface16WithKey(
				_cursorSurface,
				_overlayCursorPalette,
				_screenSurface16,
				_cursorDstRect.left, _cursorDstRect.top,
				_cursorSrcRect,
				_cursorKeycolor);
		} else {
			c2p1x1_8_falcon(
				(char*)_cursorSurface8.getPixels(),
				(char*)_cursorSurface8.getBasePtr(0, _cursorSurface8.h),
				_cursorSurface8.w,
				_cursorSurface8.pitch,
				(char*)_screenSurface8.getBasePtr(_cursorDstRect.left, _cursorDstRect.top),
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

	_pendingResolutionChange = PendingResolutionChange::Overlay;

	int oldMouseX = _oldMouseX;
	int oldMouseY = _oldMouseY;

	_oldMouseX = _mouseX;
	_oldMouseY = _mouseY;

	warpMouse(oldMouseX, oldMouseY);

	// _cursorRect may get used if _mouseVisible = false
	_cursorDstRect = _oldCursorRect = Common::Rect();

	_overlayVisible = true;
}

void AtariGraphicsManager::hideOverlay() {
	Common::String str = Common::String::format("hideOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (!_overlayVisible)
		return;

	_pendingResolutionChange = PendingResolutionChange::Screen;

	int oldMouseX = _oldMouseX;
	int oldMouseY = _oldMouseY;

	_oldMouseX = _mouseX;
	_oldMouseY = _mouseY;

	warpMouse(oldMouseX, oldMouseY);

	// _cursorRect may get used if _mouseVisible = false
	_cursorDstRect = _oldCursorRect = Common::Rect();

	_overlayVisible = false;
}

void AtariGraphicsManager::clearOverlay() {
	Common::String str = Common::String::format("clearOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (!_overlayVisible)
		return;

	int vOffset = 0;
	int height = _height;

	if (height < getOverlayHeight()) {
		vOffset = (getOverlayHeight() - height) / 2;
	} else if (height > getOverlayHeight()) {
		height /= 2;
		vOffset = (getOverlayHeight() - height) / 2;
	}

	static byte palette[256 * 3];
	grabPalette(palette, 0, 256);

	memset(_overlaySurface.getBasePtr(0, 0), 0, _overlaySurface.pitch * vOffset);
	copySurface8ToSurface16(_chunkySurface, palette, _overlaySurface, 0, vOffset, Common::Rect(_width, _height));
	memset(_overlaySurface.getBasePtr(0, vOffset + height), 0, _overlaySurface.pitch * vOffset);

	handleModifiedRect(Common::Rect(_overlaySurface.w, _overlaySurface.h), _modifiedOverlayRects, _overlaySurface);
}

void AtariGraphicsManager::grabOverlay(Graphics::Surface &surface) const {
	Common::String str = Common::String::format("grabOverlay: %d, %d, %d\n", surface.pitch, surface.w, surface.h);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	memcpy(surface.getPixels(), _overlaySurface.getPixels(), surface.pitch * surface.h);
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

void AtariGraphicsManager::updateMousePosition(int deltaX, int deltaY) {
	if (!_mouseVisible)
		return;

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

bool AtariGraphicsManager::notifyEvent(const Common::Event &event) {
	switch ((CustomEventAction) event.customType) {
	case kActionToggleAspectRatioCorrection:
		_aspectRatioCorrection = !_aspectRatioCorrection;
		return true;
	}

	return false;
}

Common::Keymap *AtariGraphicsManager::getKeymap() const {
	Common::Keymap *keymap = new Common::Keymap(Common::Keymap::kKeymapTypeGlobal, "atari-graphics", _("Graphics"));
	Common::Action *act;

	if (hasFeature(OSystem::kFeatureAspectRatioCorrection)) {
		act = new Common::Action("ASPT", _("Toggle aspect ratio correction"));
		act->addDefaultInputMapping("C+A+a");
		act->setCustomBackendActionEvent(kActionToggleAspectRatioCorrection);
		keymap->addAction(act);
	}

	return keymap;
}

bool AtariGraphicsManager::allocateAtariSurface(
		byte *&buf, Graphics::Surface &surface,
		int width, int height,
		const Graphics::PixelFormat &format, int mode,
		size_t forcedAllocationSize) {
	buf = (byte*)Mxalloc(forcedAllocationSize ? forcedAllocationSize : width * height * format.bytesPerPixel + 15, mode);
	if (!buf)
		return false;

	byte *bufAligned = (byte*)(((uint32)buf + 15) & 0xfffffff0);
	memset(bufAligned, 0, forcedAllocationSize ? forcedAllocationSize : width * height * format.bytesPerPixel);

	surface.init(width, height, width * format.bytesPerPixel, bufAligned, format);
	return true;
}

void AtariGraphicsManager::setVidelResolution() const
{
	if (_vgaMonitor) {
		// TODO: aspect ratio correction
		if (_width == 320) {
			if (_height == 200)
				asm_screen_set_scp_res(scp_320x200x8_vga);
			else
				asm_screen_set_scp_res(scp_320x240x8_vga);
		} else {
			if (_height == 400)
				asm_screen_set_scp_res(scp_640x400x8_vga);
			else
				asm_screen_set_scp_res(scp_640x480x8_vga);
		}
	} else {
		if (_width == 320) {
			if (_height == 240)
				asm_screen_set_scp_res(scp_320x240x8_rgb);
			else if (_height == 200 && _aspectRatioCorrection)
				asm_screen_set_scp_res(scp_320x200x8_rgb60);
			else
				asm_screen_set_scp_res(scp_320x200x8_rgb);
		} else {
			if (_height == 480)
				asm_screen_set_scp_res(scp_640x480x8_rgb);
			else if (_height == 400 && _aspectRatioCorrection)
				asm_screen_set_scp_res(scp_640x400x8_rgb60);
			else
				asm_screen_set_scp_res(scp_640x400x8_rgb);
		}
	}
}

void AtariGraphicsManager::waitForVbl() const
{
	extern volatile uint32 vbl_counter;
	uint32 counter = vbl_counter;

	while (counter == vbl_counter);
}

void AtariGraphicsManager::copySurface8ToSurface16(
		const Graphics::Surface &srcSurface, const byte *srcPalette,
		Graphics::Surface &dstSurface, int destX, int destY,
		const Common::Rect subRect) {
	assert(srcSurface.format.bytesPerPixel == 1);
	assert(dstSurface.format.bytesPerPixel == 2);

	// faster (no memory (re-)allocation) version of Surface::convertTo()
	const int w = subRect.width();
	const int h = subRect.height();
	const Graphics::PixelFormat dstFormat = dstSurface.format;

	int hzScale = 1;
	int scaledWidth = w;
	if (srcSurface.w > dstSurface.w) {
		hzScale = 2;
		scaledWidth /= 2;
	}

	int vScale = 1;
	int scaledHeight = h;
	if (srcSurface.h > dstSurface.h) {
		vScale = 2;
		scaledHeight /= 2;
	}

	const byte *srcRow = (const byte *)srcSurface.getBasePtr(subRect.left * hzScale, subRect.top * vScale);
	uint16 *dstRow = (uint16 *)dstSurface.getBasePtr(destX, destY);

	for (int y = 0; y < scaledHeight; y++) {
		for (int x = 0; x < scaledWidth; x++) {
			const byte index = *srcRow;
			srcRow += hzScale;

			*dstRow++ = dstFormat.RGBToColor(
				srcPalette[index * 3 + 0],
				srcPalette[index * 3 + 1],
				srcPalette[index * 3 + 2]);
		}

		srcRow += (srcSurface.w - w) + (vScale - 1) * srcSurface.w;
		dstRow += dstSurface.w - scaledWidth;
	}
}

void AtariGraphicsManager::copySurface8ToSurface16WithKey(
		const Graphics::Surface &srcSurface, const byte *srcPalette,
		Graphics::Surface &dstSurface, int destX, int destY,
		const Common::Rect subRect, uint32 key) {
	assert(srcSurface.format.bytesPerPixel == 1);
	assert(dstSurface.format.bytesPerPixel == 2);

	// faster (no memory (re-)allocation) version of Surface::convertTo()
	const int w = subRect.width();
	const int h = subRect.height();
	const Graphics::PixelFormat dstFormat = dstSurface.format;

	const byte *srcRow = (const byte *)srcSurface.getBasePtr(subRect.left, subRect.top);
	uint16 *dstRow = (uint16 *)dstSurface.getBasePtr(destX, destY);

	const uint16 keyColor = dstFormat.RGBToColor(
		srcPalette[key * 3 + 0],
		srcPalette[key * 3 + 1],
		srcPalette[key * 3 + 2]);

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			const byte index = *srcRow++;

			uint16 pixel = dstFormat.RGBToColor(
				srcPalette[index * 3 + 0],
				srcPalette[index * 3 + 1],
				srcPalette[index * 3 + 2]);

			if (pixel != keyColor) {
				*dstRow++ = pixel;
			} else {
				dstRow++;
			}
		}

		srcRow += srcSurface.w - w;
		dstRow += dstSurface.w - w;
	}
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
		//Common::String str = Common::String::format("handleModifiedRect: purge\n");
		//g_system->logMessage(LogMessageType::kDebug, str.c_str());

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
	_cursorSrcRect = Common::Rect(_cursorSurface.w, _cursorSurface.h);

	if (_cursorSrcRect.isEmpty()) {
		_cursorDstRect = Common::Rect();
		return;
	}

	_cursorDstRect = Common::Rect(
		_mouseX - _cursorHotspotX,	// left
		_mouseY - _cursorHotspotY,	// top
		_mouseX - _cursorHotspotX + _cursorSurface.w,	// right
		_mouseY - _cursorHotspotY + _cursorSurface.h);	// bottom

	if (isOverlayVisible())
		_mouseOutOfScreen = !_screenSurface16.clip(_cursorSrcRect, _cursorDstRect);
	else
		_mouseOutOfScreen = !_screenSurface8.clip(_cursorSrcRect, _cursorDstRect);

	if (_mouseOutOfScreen)
		return;

	if (!isOverlayVisible())
		prepareCursorSurface8();
}

void AtariGraphicsManager::prepareCursorSurface8() {
	Common::Rect backgroundCursorRect = _cursorDstRect;

	// ensure that background's left and right lie on a 16px boundary and double the width if needed
	backgroundCursorRect.moveTo(backgroundCursorRect.left & 0xfff0, backgroundCursorRect.top);

	const int cursorDeltaX = _cursorDstRect.left - backgroundCursorRect.left;

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
	_cursorSurface8.copyRectToSurface(_chunkySurface, 0, 0, backgroundCursorRect);
	// copy cursor
	_cursorSurface8.copyRectToSurfaceWithKey(_cursorSurface, cursorDeltaX, 0, _cursorSrcRect, _cursorKeycolor);

	_cursorDstRect = backgroundCursorRect;
}
