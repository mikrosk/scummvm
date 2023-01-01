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

#include "common/rect.h"
#include "graphics/surface.h"

class AtariGraphicsManager : public GraphicsManager {
public:
	virtual ~AtariGraphicsManager() {}

	bool hasFeature(OSystem::Feature f) const override { return false; }
	void setFeatureState(OSystem::Feature f, bool enable) override {}
	bool getFeatureState(OSystem::Feature f) const override { return false; }

	bool setGraphicsMode(int mode, uint flags = OSystem::kGfxModeNoFlags) override;

	void initSize(uint width, uint height, const Graphics::PixelFormat *format = NULL) override;

	int getScreenChangeID() const override { return 0; }

	void beginGFXTransaction() override;
	OSystem::TransactionError endGFXTransaction() override;

	int16 getHeight() const override { return _height; }
	int16 getWidth() const override { return _width; }
	void setPalette(const byte *colors, uint start, uint num) override;
	void grabPalette(byte *colors, uint start, uint num) const override;
	void copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) override;
	Graphics::Surface *lockScreen() override;
	void unlockScreen() override {}
	void fillScreen(uint32 col) override;
	void updateScreen() override;
	void setShakePos(int shakeXOffset, int shakeYOffset) override {}
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
	void setCursorPalette(const byte *colors, uint start, uint num) override {}

private:
	void updateCursorRect();
	void updateOverlayCursor();

	uint _width = 0, _height = 0;
	Graphics::PixelFormat _format = Graphics::PixelFormat(0, 0, 0, 0, 0, 0, 0, 0, 0);

	uint _oldWidth = 0, _oldHeight = 0;
	Graphics::PixelFormat _oldFormat = Graphics::PixelFormat(0, 0, 0, 0, 0, 0, 0, 0, 0);

	bool _screenModified = false;
	byte *_screen = nullptr;
	Graphics::Surface _screenSurface8;
	Graphics::Surface _screenSurface16;

	byte *_chunkyBuffer = nullptr;
	Graphics::Surface _chunkySurface;

	bool _overlayVisible;
	Graphics::Surface _overlaySurface;

	bool _mouseVisible = false;
	int _mouseX = -1, _mouseY = -1;

	uint _cursorWidth = 0, _cursorHeight = 0;
	int _cursorHotspotX = 0, _cursorHotspotY = 0;
	uint32 _cursorKeycolor = 0;
	Graphics::Surface _cursorSurface;
	Graphics::Surface _clippedCursorSurface;

	Common::Rect _cursorRect;
	Common::Rect _oldCursorRect;

};

#endif
