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

#ifndef BACKENDS_GRAPHICS_ATARI_H
#define BACKENDS_GRAPHICS_ATARI_H

#include "backends/graphics/graphics.h"
#include "common/events.h"

#include "graphics/surface.h"

#include "atari-cursor.h"
#include "atari-pendingscreenchanges.h"
#include "atari-screen.h"
#include "atari-supervidel.h"

#define MAX_HZ_SHAKE 16 // Falcon only
#define MAX_V_SHAKE  16

class AtariGraphicsManager final : public GraphicsManager, Common::EventObserver {
	friend class PendingScreenChanges;

public:
	AtariGraphicsManager();
	virtual ~AtariGraphicsManager();

	bool hasFeature(OSystem::Feature f) const override;
	void setFeatureState(OSystem::Feature f, bool enable) override;
	bool getFeatureState(OSystem::Feature f) const override;

	const OSystem::GraphicsMode *getSupportedGraphicsModes() const override {
		static const OSystem::GraphicsMode graphicsModes[] = {
			{ "direct", "Direct rendering", kDirectRendering },
			{ "single", "Single buffering", kSingleBuffering },
			{ "triple", "Triple buffering", kTripleBuffering },
			{ nullptr, nullptr, 0 }
		};
		return graphicsModes;
	}
	int getDefaultGraphicsMode() const override { return kTripleBuffering; }
	bool setGraphicsMode(int mode, uint flags = OSystem::kGfxModeNoFlags) override;
	int getGraphicsMode() const override { return _currentState.mode; }

	void initSize(uint width, uint height, const Graphics::PixelFormat *format = NULL) override;

	int getScreenChangeID() const override { return 0; }

	void beginGFXTransaction() override;
	OSystem::TransactionError endGFXTransaction() override;

	int16 getHeight() const override { return _currentState.height; }
	int16 getWidth() const override { return _currentState.width; }
	void setPalette(const byte *colors, uint start, uint num) override;
	void grabPalette(byte *colors, uint start, uint num) const override;
	void copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) override;
	Graphics::Surface *lockScreen() override;
	void unlockScreen() override;
	void fillScreen(uint32 col) override;
	void fillScreen(const Common::Rect &r, uint32 col) override;
	void updateScreen() override;
	void setShakePos(int shakeXOffset, int shakeYOffset) override;
	void setFocusRectangle(const Common::Rect& rect) override {}
	void clearFocusRectangle() override {}

	void showOverlay(bool inGUI) override;
	void hideOverlay() override;
	bool isOverlayVisible() const override { return _overlayState == kOverlayVisible; }
	Graphics::PixelFormat getOverlayFormat() const override;
	void clearOverlay() override;
	void grabOverlay(Graphics::Surface &surface) const override;
	void copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) override;
	int16 getOverlayHeight() const override { return 480; }
	int16 getOverlayWidth() const override { return _vgaMonitor ? 640 : 640*1.2; }

	bool showMouse(bool visible) override;
	void warpMouse(int x, int y) override;
	void setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor,
						bool dontScale = false, const Graphics::PixelFormat *format = NULL, const byte *mask = NULL) override;
	void setCursorPalette(const byte *colors, uint start, uint num) override;

	Common::Point getMousePosition() const {
		if (isOverlayVisible()) {
			return _screen[kOverlayBuffer]->cursor.getPosition();
		} else {
			// kFrontBuffer is always up to date
			return _screen[kFrontBuffer]->cursor.getPosition();
		}
	}
	void updateMousePosition(int deltaX, int deltaY);

	bool notifyEvent(const Common::Event &event) override;
	Common::Keymap *getKeymap() const;

private:
	enum {
		kUnknownMode		= -1,
		kDirectRendering	= 0,
		kSingleBuffering	= 1,
		kTripleBuffering	= 3
	};

	enum CustomEventAction {
		kActionToggleAspectRatioCorrection = 100,
	};

	void allocateSurfaces();
	void freeSurfaces();

#ifndef DISABLE_FANCY_THEMES
	int16 getMaximumScreenHeight() const { return 480; }
	int16 getMaximumScreenWidth() const { return _tt ? 320 : (_vgaMonitor ? 640 : 640*1.2); }
#else
	int16 getMaximumScreenHeight() const { return _tt ? 480 : 240; }
	int16 getMaximumScreenWidth() const { return _tt ? 320 : (_vgaMonitor ? 320 : 320*1.2); }
#endif

	void addDirtyRectToScreens(const Graphics::Surface &dstSurface,
							   int x, int y, int w, int h, bool directRendering);
	bool updateScreenInternal(Screen *dstScreen, const Graphics::Surface *srcSurface);
	void copyRectToAtariSurface(AtariSurface &dstSurface,
								const byte *buf, int pitch, int x, int y, int w, int h);

	bool isOverlayDirectRendering() const {
#ifndef DISABLE_FANCY_THEMES
		// see osystem_atari.cpp
		extern bool g_gameEngineActive;
#endif
		// overlay is direct rendered if in the launcher or if game is directly rendered
		// (on SuperVidel we always want to use _overlaySurface as source for background pixels)
		return !g_hasSuperVidel
#ifndef DISABLE_FANCY_THEMES
			&& (!g_gameEngineActive || _currentState.mode == kDirectRendering)
#endif
			;
	}

	Graphics::Surface *lockOverlay();

	bool _vgaMonitor = true;
	bool _tt = false;

	struct GraphicsState {
		GraphicsState()
			: inTransaction(false)
			, mode(kUnknownMode)
			, width(0)
			, height(0)
			, format(Graphics::PixelFormat()) {
		}

		bool isValid() const {
			return mode != kUnknownMode && width > 0 && height > 0 && format.bytesPerPixel != 0;
		}

		bool inTransaction;
		int mode;
		int width;
		int height;
		Graphics::PixelFormat format;
	};
	GraphicsState _pendingState;
	GraphicsState _currentState;

	// feature flags
	bool _aspectRatioCorrection = false;

	PendingScreenChanges _pendingScreenChanges;

	enum {
		kFrontBuffer	= 0,
		kBackBuffer1	= 1,
		kBackBuffer2	= 2,
		kOverlayBuffer	= 3,
		kBufferCount
	};
	Screen *_screen[kBufferCount] = {};

	Graphics::Surface _chunkySurface;
	Graphics::Surface _chunkySurfaceOffsetted;

	enum {
		kOverlayVisible,
		kOverlayIgnoredHide,
		kOverlayHidden
	};
	int _overlayState = kOverlayHidden;
	bool _ignoreHideOverlay = true;
	Graphics::Surface _overlaySurface;
	bool _ignoreCursorChanges = false;

	Palette _palette;
	Palette _overlayPalette;
};

#endif
