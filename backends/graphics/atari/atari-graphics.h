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

#include "common/array.h"
#include "common/events.h"
#include "common/rect.h"
#include "graphics/surface.h"

class AtariGraphicsManager : public GraphicsManager, Common::EventObserver {
public:
	AtariGraphicsManager();
	virtual ~AtariGraphicsManager();

	bool hasFeature(OSystem::Feature f) const override;
	void setFeatureState(OSystem::Feature f, bool enable) override;
	bool getFeatureState(OSystem::Feature f) const override;

	virtual const OSystem::GraphicsMode *getSupportedGraphicsModes() const override {
		static const OSystem::GraphicsMode graphicsModes[] = {
			{"supervidel", "SuperVidel (8-bit)", 0},
			{"single", "Single buffering", 1},
			{"double", "Double buffering", 2},
			{"triple", "Triple buffering", 3},
			{nullptr, nullptr, 0 }
		};
		return graphicsModes;
	}
	int getDefaultGraphicsMode() const override { return (int)GraphicsMode::TripleBuffering; }
	bool setGraphicsMode(int mode, uint flags = OSystem::kGfxModeNoFlags) override;
	int getGraphicsMode() const override { return (int)_currentState.mode; }

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
	void unlockScreen() override {}
	void fillScreen(uint32 col) override;
	void updateScreen() override;
	void setShakePos(int shakeXOffset, int shakeYOffset) override;
	void setFocusRectangle(const Common::Rect& rect) override {}
	void clearFocusRectangle() override {}

	void showOverlay() override;
	void hideOverlay() override;
	bool isOverlayVisible() const override { return _overlayVisible; }
	Graphics::PixelFormat getOverlayFormat() const override { return Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0); }
	void clearOverlay() override;
	void grabOverlay(Graphics::Surface &surface) const override;
	void copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) override;
	int16 getOverlayHeight() const override;
	int16 getOverlayWidth() const override;

	bool showMouse(bool visible) override;
	void warpMouse(int x, int y) override;
	void setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor, bool dontScale = false, const Graphics::PixelFormat *format = NULL) override;
	void setCursorPalette(const byte *colors, uint start, uint num) override;

	Common::Point getMousePosition() const { return Common::Point(_mouseX, _mouseY); }
	void updateMousePosition(int deltaX, int deltaY);

	bool notifyEvent(const Common::Event &event) override;
	Common::Keymap *getKeymap() const;

private:
	enum CustomEventAction {
		kActionToggleAspectRatioCorrection = 100,
	};

	bool allocateAtariSurface(byte *&buf, Graphics::Surface &surface,
							  int width, int height, const Graphics::PixelFormat &format, int mode);
	void setVidelResolution() const;
	void waitForVbl() const;

	void updateOverlay(bool updateCursor);
	void updateDirectBuffer(bool updateCursor);
	void updateSingleBuffer(bool updateCursor);
	void updateDoubleAndTripleBuffer(bool updateCursor);

	static void copySurface8ToSurface16(const Graphics::Surface &srcSurface, const byte *srcPalette,
										Graphics::Surface &dstSurface, int destX, int destY,
										const Common::Rect subRect);
	static void copySurface8ToSurface16WithKey(const Graphics::Surface &srcSurface, const byte* srcPalette,
											   Graphics::Surface &dstSurface, int destX, int destY,
											   const Common::Rect subRect, uint32 key);
	void handleModifiedRect(Common::Rect rect, Common::Array<Common::Rect> &rects, const Graphics::Surface &surface);

	void updateCursorRect();
	void copyCursorSurface8(const Graphics::Surface &backgroundSufrace, Graphics::Surface &screenSurface);

	bool _vgaMonitor = true;
	bool _superVidel = false;
	bool _aspectRatioCorrection = false;
	bool _oldAspectRatioCorrection = false;
	bool _vsync = true;
	bool _ignoreVsync = false;

	enum class GraphicsMode : int {
		DirectRendering,
		SingleBuffering,
		DoubleBuffering,
		TripleBuffering
	};

	struct GraphicsState {
		bool operator==(const GraphicsState &other) const {
			return mode == other.mode
				&& width == other.width
				&& height == other.height
				&& format == other.format;
		}
		bool operator!=(const GraphicsState &other) const {
			return !(*this == other);
		}

		GraphicsMode mode;
		int width;
		int height;
		Graphics::PixelFormat format;
	};
	GraphicsState _currentState = {};
	GraphicsState _pendingState = {};

	enum class PendingResolutionChange {
		None,
		Overlay,
		Screen
	};
	PendingResolutionChange _pendingResolutionChange = PendingResolutionChange::None;

	bool _screenModified = false;	// double/triple buffering only
	static const int SCREENS = 3;
	const int FRONT_BUFFER = 0;
	const int BACK_BUFFER1 = 1;
	const int BACK_BUFFER2 = 2;
	byte *_screen[SCREENS] = {};	// for Mfree() purposes only
	byte *_screenAligned[SCREENS] = {};
	Graphics::Surface _screenSurface8;
	byte *_overlay = nullptr;
	Graphics::Surface _screenSurface16;
	Common::Rect _modifiedScreenRect;	// direct rendering only

	byte *_chunkyBuffer = nullptr;
	Graphics::Surface _chunkySurface;
	Common::Array<Common::Rect> _modifiedChunkyRects;

	bool _overlayVisible = false;
	byte *_overlayBuffer = nullptr;
	Graphics::Surface _overlaySurface;
	Common::Array<Common::Rect> _modifiedOverlayRects;

	bool _mouseVisible = false;
	bool _mouseOutOfScreen = false;
	int _mouseX = -1, _mouseY = -1;
	int _oldMouseX = -1, _oldMouseY = -1;

	bool _cursorModified = false;
	Graphics::Surface _cursorSurface;
	Graphics::Surface _cursorSurface8;
	int _cursorHotspotX;
	int _cursorHotspotY;
	uint32 _cursorKeycolor;
	Common::Rect _cursorSrcRect;
	Common::Rect _cursorDstRect;
	Common::Rect _oldCursorRect;

	uint _palette[256];
	byte _overlayCursorPalette[256*3] = {};
};

#endif
