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

#include <mint/cookie.h>
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
#include "common/textconsole.h"
#include "common/translation.h"

#define OVERLAY_WIDTH	320
#define OVERLAY_HEIGHT	240

// maximum screen dimensions
#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480
#define SCREEN_SIZE		(SCREEN_WIDTH*SCREEN_HEIGHT*1)

#define SCREEN_ACTIVE

AtariGraphicsManager::AtariGraphicsManager() {
	_vgaMonitor = VgetMonitor() == MON_VGA;
	_superVidel = _vgaMonitor && Getcookie(C_SupV, NULL) == C_FOUND;

	for (int i = 0; i < SCREENS; ++i) {
		if (!allocateAtariSurface(_screen[i], _screenSurface8, SCREEN_WIDTH, SCREEN_HEIGHT,
				Graphics::PixelFormat::createFormatCLUT8(), MX_STRAM))
			error("Failed to allocate screen memory in ST RAM");
		_screenAligned[i] = (byte*)_screenSurface8.getPixels();
	}
	_screenSurface8.setPixels(_screenAligned[getDefaultGraphicsMode() <= 1 ? FRONT_BUFFER : BACK_BUFFER1]);

	if (!allocateAtariSurface(_chunkyBuffer, _chunkySurface, SCREEN_WIDTH, SCREEN_HEIGHT,
			Graphics::PixelFormat::createFormatCLUT8(), MX_PREFTTRAM))
		error("Failed to allocate chunky buffer memory in ST/TT RAM");

	if (!allocateAtariSurface(_overlay, _screenSurface16, getOverlayWidth(), getOverlayHeight(),
			getOverlayFormat(), MX_STRAM))
		error("Failed to allocate overlay memory in ST RAM");

	if (!allocateAtariSurface(_overlayBuffer, _overlaySurface, getOverlayWidth(), getOverlayHeight(),
			getOverlayFormat(), MX_PREFTTRAM))
		error("Failed to allocate overlay buffer memory in ST/TT RAM");

	if (_superVidel) {
		// patch SPSHIFT for SuperVidel's BPS8C
		for (unsigned char *p : {scp_320x200x8_vga, scp_320x240x8_vga, scp_640x400x8_vga, scp_640x480x8_vga}) {
			uint16 *p16 = (uint16*)(p + 122 + 30);
			*p16 |= 0x1000;
		}
	}

	g_system->getEventManager()->getEventDispatcher()->registerObserver(this, 10, false);
}

AtariGraphicsManager::~AtariGraphicsManager() {
	g_system->getEventManager()->getEventDispatcher()->unregisterObserver(this);

	for (int i = 0; i < SCREENS; ++i) {
		Mfree(_screen[i]);
		_screen[i] = _screenAligned[i] = nullptr;
	}

	Mfree(_chunkyBuffer);
	_chunkyBuffer = nullptr;

	Mfree(_overlayBuffer);
	_overlayBuffer = nullptr;

	Mfree(_overlay);
	_overlay = nullptr;
}

bool AtariGraphicsManager::hasFeature(OSystem::Feature f) const {
	switch (f) {
	case OSystem::Feature::kFeatureAspectRatioCorrection:
		debug("hasFeature(kFeatureAspectRatioCorrection): %d", !_vgaMonitor);
		return !_vgaMonitor;
	case OSystem::Feature::kFeatureCursorPalette:
		debug("hasFeature(kFeatureCursorPalette): %d", isOverlayVisible());
		return true;
	case OSystem::Feature::kFeatureVSync:
		debug("hasFeature(kFeatureVSync): %d", _vsync);
		return true;
	default:
		return false;
	}
}

void AtariGraphicsManager::setFeatureState(OSystem::Feature f, bool enable) {
	switch (f) {
	case OSystem::Feature::kFeatureAspectRatioCorrection:
		debug("setFeatureState(kFeatureAspectRatioCorrection): %d", enable);
		_oldAspectRatioCorrection = _aspectRatioCorrection;
		_aspectRatioCorrection = enable;
		break;
	case OSystem::Feature::kFeatureVSync:
		debug("setFeatureState(kFeatureVSync): %d", enable);
		_vsync = enable;
		break;
	default:
		[[fallthrough]];
	}
}

bool AtariGraphicsManager::getFeatureState(OSystem::Feature f) const {
	switch (f) {
	case OSystem::Feature::kFeatureAspectRatioCorrection:
		//debug("getFeatureState(kFeatureAspectRatioCorrection): %d", _aspectRatioCorrection);
		return _aspectRatioCorrection;
	case OSystem::Feature::kFeatureCursorPalette:
		//debug("getFeatureState(kFeatureCursorPalette): %d", isOverlayVisible());
		return isOverlayVisible();
	case OSystem::Feature::kFeatureVSync:
		//debug("getFeatureState(kFeatureVSync): %d", _vsync);
		return _vsync;
	default:
		return false;
	}
}

bool AtariGraphicsManager::setGraphicsMode(int mode, uint flags) {
	debug("setGraphicsMode: %d, %d", mode, flags);

	if (mode >= 0 && mode <= 3) {
		_pendingState.mode = (GraphicsMode)mode;
		return true;
	}

	return false;
}

void AtariGraphicsManager::initSize(uint width, uint height, const Graphics::PixelFormat *format) {
	debug("initSize: %d, %d, %d (vsync: %d, mode: %d)", width, height, format ? format->bytesPerPixel : 1, _vsync, (int)_pendingState.mode);

	_pendingState.width = width;
	_pendingState.height = height;
	_pendingState.format = format ? *format : Graphics::PixelFormat::createFormatCLUT8();
}

void AtariGraphicsManager::beginGFXTransaction() {
	debug("beginGFXTransaction");
}

OSystem::TransactionError AtariGraphicsManager::endGFXTransaction() {
	debug("endGFXTransaction");

	int error = OSystem::TransactionError::kTransactionSuccess;

	// always initialize (clear screen, mouse init, ...)
	//if (_pendingState == _currentState)
	//	return static_cast<OSystem::TransactionError>(error);

	if (_pendingState.mode == GraphicsMode::DirectRendering && !_superVidel)
		error |= OSystem::TransactionError::kTransactionModeSwitchFailed;

	if (_pendingState.format != Graphics::PixelFormat::createFormatCLUT8())
		error |= OSystem::TransactionError::kTransactionFormatNotSupported;

	if ((_pendingState.width != 320 || (_pendingState.height != 200 && _pendingState.height != 240))
		&& (_pendingState.width != 640 || (_pendingState.height != 400 && _pendingState.height != 480)))
		error |= OSystem::TransactionError::kTransactionSizeChangeFailed;

	if (error != OSystem::TransactionError::kTransactionSuccess) {
		// all our errors are fatal but engine.cpp takes only this one seriously
		error |= OSystem::TransactionError::kTransactionSizeChangeFailed;
		return static_cast<OSystem::TransactionError>(error);
	}

	_chunkySurface.init(_pendingState.width, _pendingState.height, _pendingState.width,
		_chunkySurface.getPixels(), _pendingState.format);
	_screenSurface8.init(_pendingState.width, _pendingState.height, _pendingState.width,
		_screenAligned[(int)_pendingState.mode <= 1 ? FRONT_BUFFER : BACK_BUFFER1], _pendingState.format);

	// some games do not initialize their viewport entirely
	if (_pendingState.mode != GraphicsMode::DirectRendering) {
		memset(_chunkySurface.getPixels(), 0, _chunkySurface.pitch * _chunkySurface.h);

		if (_pendingState.mode == GraphicsMode::SingleBuffering)
			handleModifiedRect(Common::Rect(_chunkySurface.w, _chunkySurface.h), _modifiedChunkyRects, _chunkySurface);
		else
			_screenModified = true;
	} else {
		memset(_screenSurface8.getPixels(), 0, _screenSurface8.pitch * _screenSurface8.h);
	}

#ifdef SCREEN_ACTIVE
	memset(_palette, 0, sizeof(_palette));
	asm_screen_set_falcon_palette(_palette);
	_pendingResolutionChange = PendingResolutionChange::Screen;
#endif
	static bool firstRun = true;
	if (firstRun) {
		_cursor.setPosition(getOverlayWidth() / 2, getOverlayHeight() / 2, true);
		_cursor.swap();
		firstRun = false;
	}

	warpMouse(_pendingState.width / 2, _pendingState.height / 2);

	_currentState = _pendingState;

	return OSystem::kTransactionSuccess;
}

void AtariGraphicsManager::setPalette(const byte *colors, uint start, uint num) {
	//debug("setPalette: %d, %d", start, num);

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
	//debug("grabPalette: %d, %d", start, num);

	const uint *pal = &_palette[start];

	for (uint i = 0; i < num; ++i) {
		// RRRRRRrr GGGGGGgg 00000000 BBBBBBbb -> RRRRRRRR GGGGGGGG BBBBBBBB
		colors[i * 3 + 0] = pal[i] >> 24;
		colors[i * 3 + 1] = pal[i] >> 16;
		colors[i * 3 + 2] = pal[i];
	}
}

void AtariGraphicsManager::copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) {
	//debug("copyRectToScreen: %d, %d, %d, %d, %d", pitch, x, y, w, h);

	if (_currentState.mode != GraphicsMode::DirectRendering) {
		_chunkySurface.copyRectToSurface(buf, pitch, x, y, w, h);

		if (_currentState.mode == GraphicsMode::SingleBuffering)
			handleModifiedRect(Common::Rect(x, y, x + w, y + h), _modifiedChunkyRects, _chunkySurface);
		else
			_screenModified = true;
	} else {
		// TODO: c2p with 16pix align
		_screenSurface8.copyRectToSurface(buf, pitch, x, y, w, h);

		_modifiedScreenRect = Common::Rect(x, y, x + w, y + h);

		_ignoreVsync = true;
		updateScreen();
		_ignoreVsync = false;
	}
}

Graphics::Surface *AtariGraphicsManager::lockScreen() {
	//debug("lockScreen");

	return &_chunkySurface;
}

void AtariGraphicsManager::fillScreen(uint32 col) {
	debug("fillScreen: %d", col);

	if (_currentState.mode != GraphicsMode::DirectRendering) {
		const Common::Rect rect = Common::Rect(_chunkySurface.w, _chunkySurface.h);
		_chunkySurface.fillRect(rect, col);

		if (_currentState.mode == GraphicsMode::SingleBuffering)
			handleModifiedRect(rect, _modifiedChunkyRects, _chunkySurface);
		else
			_screenModified = true;
	} else {
		const Common::Rect rect = Common::Rect(_screenSurface8.w, _screenSurface8.h);
		_screenSurface8.fillRect(rect, col);
	}
}

void AtariGraphicsManager::updateScreen() {
	//debug("updateScreen");

	// updates outOfScreen OR srcRect/dstRect (only if visible/needed)
	_cursor.update(isOverlayVisible() ? _screenSurface16 : _screenSurface8);

	// TODO: use of the SuperVidel's Blitter?
	if (isOverlayVisible()) {
		updateOverlay();
	} else {
		switch(_currentState.mode) {
		case GraphicsMode::DirectRendering:
			updateDirectBuffer();
			break;
		case GraphicsMode::SingleBuffering:
			updateSingleBuffer();
			break;
		case GraphicsMode::DoubleBuffering:
		case GraphicsMode::TripleBuffering:
			updateDoubleAndTripleBuffer();
			break;
		}
	}

	//if (_cursor.outOfScreen)
	//	warning("mouse out of screen");

	bool vsync = _vsync;

	if (_screenModified) {
		if (_currentState.mode == GraphicsMode::DoubleBuffering) {
			byte *tmp = _screenAligned[FRONT_BUFFER];
			_screenAligned[FRONT_BUFFER] = _screenAligned[BACK_BUFFER1];
			_screenAligned[BACK_BUFFER1] = tmp;

			// always wait for vbl
			vsync = true;
		} else if (_currentState.mode == GraphicsMode::TripleBuffering) {
			if (vsync) {
				// render into BACK_BUFFER1 and/or BACK_BUFFER2 and set the most recent one
				_screenAligned[FRONT_BUFFER] = _screenAligned[BACK_BUFFER1];

				byte *tmp = _screenAligned[BACK_BUFFER1];
				_screenAligned[BACK_BUFFER1] = _screenAligned[BACK_BUFFER2];
				_screenAligned[BACK_BUFFER2] = tmp;
			} else {
				// render into BACK_BUFFER1 and/or BACK_BUFFER2 and/or FRONT_BUFFER
				byte *tmp = _screenAligned[FRONT_BUFFER];
				_screenAligned[FRONT_BUFFER] = _screenAligned[BACK_BUFFER1];
				_screenAligned[BACK_BUFFER1] = _screenAligned[BACK_BUFFER2];
				_screenAligned[BACK_BUFFER2] = tmp;
			}

			// never wait for vbl (use it only as a flag for the above modes)
			vsync = false;
		}

		asm_screen_set_vram(_screenAligned[FRONT_BUFFER]);
		_screenSurface8.setPixels(_screenAligned[BACK_BUFFER1]);
		_screenModified = false;
	}

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
		asm_screen_set_vram(_screenAligned[FRONT_BUFFER]);

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

	if (vsync && !_ignoreVsync)
		waitForVbl();
#endif
	//debug("end of updateScreen");
}

void AtariGraphicsManager::setShakePos(int shakeXOffset, int shakeYOffset) {
	debug("setShakePos: %d, %d", shakeXOffset, shakeYOffset);
}

void AtariGraphicsManager::showOverlay() {
	debug("showOverlay");

	if (_overlayVisible)
		return;

	_pendingResolutionChange = PendingResolutionChange::Overlay;

	_cursor.swap();
	if (_currentState.mode == GraphicsMode::DirectRendering) {
		// make sure that _oldCursorRect is used to restore the original game graphics
		bool wasVisible = showMouse(false);
		updateDirectBuffer();
		showMouse(wasVisible);
	}

	_overlayVisible = true;
}

void AtariGraphicsManager::hideOverlay() {
	debug("hideOverlay");

	if (!_overlayVisible)
		return;

	_pendingResolutionChange = PendingResolutionChange::Screen;

	_cursor.swap();
	// don't fool game cursor logic (especially direct rendering)
	// (the overlay doesn't need any restoration upon re-entering)
	_oldCursorRect = Common::Rect();

	_overlayVisible = false;
}

void AtariGraphicsManager::clearOverlay() {
	debug("clearOverlay");

	if (!_overlayVisible)
		return;

	const Graphics::Surface &sourceSurface = _superVidel ? _screenSurface8 : _chunkySurface;

	int vOffset = 0;
	int height = sourceSurface.h;

	if (height < getOverlayHeight()) {
		vOffset = (getOverlayHeight() - height) / 2;
	} else if (height > getOverlayHeight()) {
		height /= 2;
		vOffset = (getOverlayHeight() - height) / 2;
	}

	static byte palette[256 * 3];
	grabPalette(palette, 0, 256);

	memset(_overlaySurface.getBasePtr(0, 0), 0, _overlaySurface.pitch * vOffset);
	copySurface8ToSurface16(
		sourceSurface,
		palette,
		_overlaySurface,
		0, vOffset,
		Common::Rect(sourceSurface.w, sourceSurface.h));
	memset(_overlaySurface.getBasePtr(0, vOffset + height), 0, _overlaySurface.pitch * vOffset);

	handleModifiedRect(Common::Rect(_overlaySurface.w, _overlaySurface.h), _modifiedOverlayRects, _overlaySurface);
}

void AtariGraphicsManager::grabOverlay(Graphics::Surface &surface) const {
	debug("grabOverlay: %d, %d, %d", surface.pitch, surface.w, surface.h);

	memcpy(surface.getPixels(), _overlaySurface.getPixels(), surface.pitch * surface.h);
}

void AtariGraphicsManager::copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) {
	//debug("copyRectToOverlay: %d, %d, %d, %d, %d", pitch, x, y, w, h);

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
	//debug("showMouse: %d", visible);

	if (_cursor.visible == visible) {
		return visible;
	}

	bool last = _cursor.visible;
	_cursor.visible = visible;
	return last;
}

void AtariGraphicsManager::warpMouse(int x, int y) {
	//debug("warpMouse: %d, %d", x, y);

	_cursor.setPosition(x, y, true);
}

void AtariGraphicsManager::setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor, bool dontScale, const Graphics::PixelFormat *format) {
	//debug("setMouseCursor: %d, %d, %d, %d, %d, %d", w, h, hotspotX, hotspotY, keycolor, format ? format->bytesPerPixel : 1);

	_cursor.setSurface(buf, (int)w, (int)h, hotspotX, hotspotY, keycolor, format);
}

void AtariGraphicsManager::setCursorPalette(const byte *colors, uint start, uint num) {
	debug("setCursorPalette: %d, %d", start, num);

	byte *pal = &_overlayCursorPalette[start];

	for (uint i = 0; i < num; ++i) {
		pal[i * 3 + 0] = colors[i * 3 + 0];
		pal[i * 3 + 1] = colors[i * 3 + 1];
		pal[i * 3 + 2] = colors[i * 3 + 2];
	}
}

void AtariGraphicsManager::updateMousePosition(int deltaX, int deltaY) {
	_cursor.updatePosition(deltaX, deltaY, isOverlayVisible() ? _screenSurface16 : _screenSurface8);
}

bool AtariGraphicsManager::notifyEvent(const Common::Event &event) {
	if (event.type != Common::EVENT_CUSTOM_BACKEND_ACTION_START) {
		return false;
	}

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
		const Graphics::PixelFormat &format, int mode) {
	buf = (byte*)Mxalloc(width * height * format.bytesPerPixel + 15, mode);
	if (!buf)
		return false;

	byte *bufAligned = (byte*)(((uint32)buf + 15) & 0xfffffff0);
	if (_superVidel && mode == MX_STRAM)
		bufAligned = (byte*)((uint32)bufAligned | 0xA0000000);
	memset(bufAligned, 0, width * height * format.bytesPerPixel);

	surface.init(width, height, width * format.bytesPerPixel, bufAligned, format);
	return true;
}

void AtariGraphicsManager::setVidelResolution() const
{
	if (_vgaMonitor) {
		// TODO: aspect ratio correction
		if (_screenSurface8.w == 320) {
			if (_screenSurface8.h == 200)
				asm_screen_set_scp_res(scp_320x200x8_vga);
			else
				asm_screen_set_scp_res(scp_320x240x8_vga);
		} else {
			if (_screenSurface8.h == 400)
				asm_screen_set_scp_res(scp_640x400x8_vga);
			else
				asm_screen_set_scp_res(scp_640x480x8_vga);
		}
	} else {
		if (_screenSurface8.w == 320) {
			if (_screenSurface8.h == 240)
				asm_screen_set_scp_res(scp_320x240x8_rgb);
			else if (_screenSurface8.h == 200 && _aspectRatioCorrection)
				asm_screen_set_scp_res(scp_320x200x8_rgb60);
			else
				asm_screen_set_scp_res(scp_320x200x8_rgb);
		} else {
			if (_screenSurface8.h == 480)
				asm_screen_set_scp_res(scp_640x480x8_rgb);
			else if (_screenSurface8.h == 400 && _aspectRatioCorrection)
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

void AtariGraphicsManager::updateOverlay() {
	// TODO: avoid _overlaySurface, offer higher resolution on SuperVidel?
	bool drawCursor = _cursor.isModified();

	while (!_modifiedOverlayRects.empty()) {
		const Common::Rect &rect = _modifiedOverlayRects.back();

		if (!drawCursor && !_cursor.outOfScreen && _cursor.visible)
			drawCursor = rect.intersects(_cursor.dstRect);

		_screenSurface16.copyRectToSurface(_overlaySurface, rect.left, rect.top, rect);

		_modifiedOverlayRects.pop_back();
	}

	if (_cursor.outOfScreen)
		return;

	if ((_cursor.positionChanged || !_cursor.visible) && !_oldCursorRect.isEmpty()) {
		_screenSurface16.copyRectToSurface(_overlaySurface, _oldCursorRect.left, _oldCursorRect.top, _oldCursorRect);
		_oldCursorRect = Common::Rect();
	}

	if (drawCursor && _cursor.visible) {
		//debug("Redraw cursor (overlay): %d %d %d %d", _cursor.dstRect.left, _cursor.dstRect.top, _cursor.dstRect.width(), _cursor.dstRect.height());

		copySurface8ToSurface16WithKey(
			_cursor.surface,
			_overlayCursorPalette,
			_screenSurface16,
			_cursor.dstRect.left, _cursor.dstRect.top,
			_cursor.srcRect,
			_cursor.keycolor);

		_cursor.positionChanged = _cursor.surfaceChanged = false;
		_oldCursorRect = _cursor.dstRect;
	}
}

void AtariGraphicsManager::updateDirectBuffer()
{
	if (_cursor.outOfScreen)
		return;

	bool drawCursor = _cursor.isModified();

	if (!drawCursor && _cursor.visible && !_modifiedScreenRect.isEmpty())
		drawCursor = _modifiedScreenRect.intersects(_cursor.dstRect);

	if (!_oldCursorRect.isEmpty() && !_modifiedScreenRect.isEmpty()) {
		const Common::Rect intersectingRect = _modifiedScreenRect.findIntersectingRect(_oldCursorRect);
		if (!intersectingRect.isEmpty()) {
			// update cached surface
			const Graphics::Surface intersectingScreenSurface = _screenSurface8.getSubArea(intersectingRect);
			_cursorSurface8.copyRectToSurface(
				intersectingScreenSurface,
				intersectingRect.left - _oldCursorRect.left,
				intersectingRect.top - _oldCursorRect.top,
				Common::Rect(intersectingScreenSurface.w, intersectingScreenSurface.h));
		}
	}

	_modifiedScreenRect = Common::Rect();

	if ((_cursor.positionChanged || !_cursor.visible) && !_oldCursorRect.isEmpty()) {
		_screenSurface8.copyRectToSurface(
			_cursorSurface8,
			_oldCursorRect.left, _oldCursorRect.top,
			Common::Rect(_oldCursorRect.width(), _oldCursorRect.height()));

		_oldCursorRect = Common::Rect();
	}

	if (drawCursor && _cursor.visible) {
		//debug("Redraw cursor (direct): %d %d %d %d", _cursor.dstRect.left, _cursor.dstRect.top, _cursor.dstRect.width(), _cursor.dstRect.height());

		if (_cursorSurface8.w != _cursor.dstRect.width() || _cursorSurface8.h != _cursor.dstRect.height()) {
			_cursorSurface8.create(_cursor.dstRect.width(), _cursor.dstRect.height(), _cursor.surface.format);
		}

		// background has been restored, so it's safe to read _screenSurface8
		if (_oldCursorRect.isEmpty())
			_cursorSurface8.copyRectToSurface(_screenSurface8, 0, 0, _cursor.dstRect);

		_screenSurface8.copyRectToSurfaceWithKey(
			_cursor.surface,
			_cursor.dstRect.left, _cursor.dstRect.top,
			_cursor.srcRect,
			_cursor.keycolor);

		_cursor.positionChanged = _cursor.surfaceChanged = false;
		_oldCursorRect = _cursor.dstRect;
	}
}

void AtariGraphicsManager::updateSingleBuffer() {
	bool drawCursor = _cursor.isModified();

	while (!_modifiedChunkyRects.empty()) {
		const Common::Rect &rect = _modifiedChunkyRects.back();

		if (!drawCursor && !_cursor.outOfScreen && _cursor.visible)
			drawCursor = rect.intersects(_cursor.dstRect);

		if (_superVidel) {
			_screenSurface8.copyRectToSurface(_chunkySurface, rect.left, rect.top, rect);
		} else {
			asm_c2p1x1_8_rect(
				(char*)_chunkySurface.getBasePtr(rect.left, rect.top),
				(char*)_chunkySurface.getBasePtr(rect.right, rect.bottom),
				rect.width(),
				_chunkySurface.pitch,
				(char*)_screenSurface8.getBasePtr(rect.left, rect.top),
				_screenSurface8.pitch);
		}

		_modifiedChunkyRects.pop_back();
	}

	if (_cursor.outOfScreen)
		return;

	if ((_cursor.positionChanged || !_cursor.visible) && !_oldCursorRect.isEmpty()) {
		if (_superVidel) {
			_screenSurface8.copyRectToSurface(_chunkySurface, _oldCursorRect.left, _oldCursorRect.top, _oldCursorRect);
		} else {
			Common::Rect rect = _oldCursorRect;
			// align on 16px
			rect.left &= 0xfff0;
			rect.right = (rect.right + 15) & 0xfff0;
			if (rect.right > _chunkySurface.w)
				rect.right = _chunkySurface.w;

			asm_c2p1x1_8_rect(
				(char*)_chunkySurface.getBasePtr(rect.left, rect.top),
				(char*)_chunkySurface.getBasePtr(rect.right, rect.bottom),
				rect.width(),
				_chunkySurface.pitch,
				(char*)_screenSurface8.getBasePtr(rect.left, rect.top),
				_screenSurface8.pitch);
		}

		_oldCursorRect = Common::Rect();
	}

	if (drawCursor && _cursor.visible) {
		//debug("Redraw cursor (single): %d %d %d %d", _cursor.dstRect.left, _cursor.dstRect.top, _cursor.dstRect.width(), _cursor.dstRect.height());

		if (_superVidel) {
			_screenSurface8.copyRectToSurfaceWithKey(
				_cursor.surface,
				_cursor.dstRect.left, _cursor.dstRect.top,
				_cursor.srcRect,
				_cursor.keycolor);
		} else {
			copyCursorSurface8(_chunkySurface, _screenSurface8);
		}

		_cursor.positionChanged = _cursor.surfaceChanged = false;
		_oldCursorRect = _cursor.dstRect;
	}
}

void AtariGraphicsManager::updateDoubleAndTripleBuffer()
{
	bool drawCursor = _cursor.isModified();

	if (_screenModified) {
		drawCursor = true;

		if (_superVidel) {
			memcpy(_screenSurface8.getPixels(), _chunkySurface.getPixels(), _chunkySurface.h * _chunkySurface.pitch);
		} else {
			asm_c2p1x1_8(
				(char*)_chunkySurface.getPixels(),
				(char*)_chunkySurface.getBasePtr(_chunkySurface.w, _chunkySurface.h),
				(char*)_screenSurface8.getPixels());
		}

		// updated in screen swapping
		//_screenModified = false;
	}

	if (_cursor.outOfScreen)
		return;

	Graphics::Surface frontBufferScreenSurface;
	frontBufferScreenSurface.init(_screenSurface8.w, _screenSurface8.h, _screenSurface8.pitch,
		_screenAligned[_screenModified ? BACK_BUFFER1 : FRONT_BUFFER], _screenSurface8.format);

	if ((_cursor.positionChanged || !_cursor.visible) && !_oldCursorRect.isEmpty() && !_screenModified) {
		if (_superVidel) {
			frontBufferScreenSurface.copyRectToSurface(_chunkySurface, _oldCursorRect.left, _oldCursorRect.top, _oldCursorRect);
		} else {
			Common::Rect rect = _oldCursorRect;
			// align on 16px
			rect.left &= 0xfff0;
			rect.right = (rect.right + 15) & 0xfff0;
			if (rect.right > _chunkySurface.w)
				rect.right = _chunkySurface.w;

			asm_c2p1x1_8_rect(
				(char*)_chunkySurface.getBasePtr(rect.left, rect.top),
				(char*)_chunkySurface.getBasePtr(rect.right, rect.bottom),
				rect.width(),
				_chunkySurface.pitch,
				(char*)frontBufferScreenSurface.getBasePtr(rect.left, rect.top),
				frontBufferScreenSurface.pitch);
		}

		_oldCursorRect = Common::Rect();
	}

	if (drawCursor && _cursor.visible) {
		//debug("Redraw cursor (double/triple): %d %d %d %d", _cursor.dstRect.left, _cursor.dstRect.top, _cursor.dstRect.width(), _cursor.dstRect.height());

		// render directly to the screen to be swapped (so we don't have to refresh full screen when only cursor moves)
		if (_superVidel) {
			frontBufferScreenSurface.copyRectToSurfaceWithKey(
				_cursor.surface,
				_cursor.dstRect.left, _cursor.dstRect.top,
				_cursor.srcRect,
				_cursor.keycolor);
		} else {
			copyCursorSurface8(_chunkySurface, frontBufferScreenSurface);
		}

		_cursor.positionChanged = _cursor.surfaceChanged = false;
		_oldCursorRect = _cursor.dstRect;
	}
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
	if (surface.format.bytesPerPixel == 1 && _currentState.mode == GraphicsMode::SingleBuffering) {
		// align on 16px
		rect.left &= 0xfff0;
		rect.right = (rect.right + 15) & 0xfff0;
		if (rect.right > surface.w)
			rect.right = surface.w;
	}

	if (rect.width() == surface.w && rect.height() == surface.h) {
		//debug("handleModifiedRect: purge");

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

void AtariGraphicsManager::copyCursorSurface8(const Graphics::Surface &backgroundSufrace, Graphics::Surface &screenSurface) {
	Common::Rect backgroundCursorRect = _cursor.dstRect;

	// ensure that background's left and right lie on a 16px boundary and double the width if needed
	backgroundCursorRect.moveTo(backgroundCursorRect.left & 0xfff0, backgroundCursorRect.top);

	const int cursorDeltaX = _cursor.dstRect.left - backgroundCursorRect.left;

	backgroundCursorRect.right = (backgroundCursorRect.right + cursorDeltaX + 15) & 0xfff0;
	if (backgroundCursorRect.right > backgroundSufrace.w)
		backgroundCursorRect.right = backgroundSufrace.w;

	if (_cursorSurface8.w != backgroundCursorRect.width() || _cursorSurface8.h != backgroundCursorRect.height()) {
		_cursorSurface8.create(
			backgroundCursorRect.width(),
			backgroundCursorRect.height(),
			backgroundSufrace.format);
	}

	// copy background
	_cursorSurface8.copyRectToSurface(backgroundSufrace, 0, 0, backgroundCursorRect);
	// copy cursor
	_cursorSurface8.copyRectToSurfaceWithKey(_cursor.surface, cursorDeltaX, 0, _cursor.srcRect, _cursor.keycolor);

	asm_c2p1x1_8_rect(
		(char*)_cursorSurface8.getPixels(),
		(char*)_cursorSurface8.getBasePtr(0, _cursorSurface8.h),
		_cursorSurface8.w,
		_cursorSurface8.pitch,
		(char*)screenSurface.getBasePtr(backgroundCursorRect.left, backgroundCursorRect.top),
		screenSurface.pitch);
}

void AtariGraphicsManager::Cursor::update(const Graphics::Surface &screen) {
	if (!surface.getPixels()) {
		outOfScreen = true;
		return;
	}

	if (!visible || (!surfaceChanged && !positionChanged))
		return;

	srcRect = Common::Rect(surface.w, surface.h);

	dstRect = Common::Rect(
		x - hotspotX,	// left
		y - hotspotY,	// top
		x - hotspotX + surface.w,	// right
		y - hotspotY + surface.h);	// bottom

	outOfScreen = !screen.clip(srcRect, dstRect);
}

void AtariGraphicsManager::Cursor::updatePosition(int deltaX, int deltaY, const Graphics::Surface &screen)
{
	if (!visible)
		return;

	x += deltaX;
	y += deltaY;

	if (x < 0)
		x = 0;
	else if (x >= screen.w)
		x = screen.w - 1;

	if (y < 0)
		y = 0;
	else if (y >= screen.h)
		y = screen.h - 1;

	positionChanged = true;
}

void AtariGraphicsManager::Cursor::setSurface(const void *buf, int w, int h, int _hotspotX, int _hotspotY, uint32 _keycolor, const Graphics::PixelFormat *format) {
	if (w == 0 || h == 0 || buf == nullptr) {
		if (surface.getPixels())
			surface.free();
		return;
	}

	const Graphics::PixelFormat cursorFormat = format ? *format : Graphics::PixelFormat::createFormatCLUT8();

	if (surface.w != w || surface.h != h || surface.format != cursorFormat)
		surface.create(w, h, cursorFormat);

	surface.copyRectToSurface(buf, surface.pitch, 0, 0, w, h);

	hotspotX = _hotspotX;
	hotspotY = _hotspotY;
	keycolor = _keycolor;

	surfaceChanged = true;
}
