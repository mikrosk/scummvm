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
#include "graphics/surface.h"

#define SCREEN_WIDTH	320
#define SCREEN_HEIGHT	240

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
		int16 old_mode = VsetMode(VM_INQUIRE);
		VsetMode(VERTFLAG | (old_mode&PAL) | (old_mode&VGA) | COL40 | BPS8);

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

			_screen = (byte*)Mxalloc(SCREEN_WIDTH * SCREEN_HEIGHT + 15, MX_STRAM);
			if (!_screen)
				return OSystem::TransactionError::kTransactionSizeChangeFailed;

			_screenAligned = (byte*)(((unsigned long)_screen + 15) & 0xfffffff0);
			memset(_screenAligned, 0, SCREEN_WIDTH * SCREEN_HEIGHT);

			VsetScreen(SCR_NOCHANGE, _screenAligned, SCR_NOCHANGE, SCR_NOCHANGE);

			_overlayBuffer = (uint16*)Mxalloc(getOverlayWidth() * getOverlayHeight() * getOverlayFormat().bytesPerPixel, MX_STRAM);
			if (!_overlayBuffer)
				return OSystem::TransactionError::kTransactionSizeChangeFailed;

			memset(_overlayBuffer, 0, getOverlayWidth() * getOverlayHeight() * getOverlayFormat().bytesPerPixel);
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
}

Graphics::Surface *AtariGraphicsManager::lockScreen() {
	Common::String str = Common::String::format("lockScreen\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	static Graphics::Surface surface;	// never release via free/delete, we want to Mfree() it
	surface.init(_width, _height, _width, _chunkyBufferAligned, _format);

	return &surface;
}

void AtariGraphicsManager::showOverlay() {
	Common::String str = Common::String::format("showOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (_overlayVisible || !_overlayBuffer)
		return;

	int16 old_mode = VsetMode(VM_INQUIRE);
	VsetMode(VERTFLAG | (old_mode&PAL) | (old_mode&VGA) | COL40 | BPS16);
	VsetScreen(SCR_NOCHANGE, _overlayBuffer, SCR_NOCHANGE, SCR_NOCHANGE);

	_overlayVisible = true;
}

void AtariGraphicsManager::hideOverlay() {
	Common::String str = Common::String::format("hideOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	if (!_overlayVisible || !_screenAligned)
		return;

	int16 old_mode = VsetMode(VM_INQUIRE);
	VsetMode(VERTFLAG | (old_mode&PAL) | (old_mode&VGA) | COL40 | BPS8);
	VsetScreen(SCR_NOCHANGE, _screenAligned, SCR_NOCHANGE, SCR_NOCHANGE);

	_overlayVisible = false;
}

void AtariGraphicsManager::clearOverlay() {
	Common::String str = Common::String::format("clearOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	memset(_overlayBuffer, 0, getOverlayWidth() * getOverlayHeight() * getOverlayFormat().bytesPerPixel);
}

void AtariGraphicsManager::grabOverlay(Graphics::Surface &surface) const {
	Common::String str = Common::String::format("grabOverlay: %d, %d, %d\n", surface.pitch, surface.w, surface.h);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	static Graphics::Surface overlaySurface;	// never release via free/delete, we want to Mfree() it
	overlaySurface.init(getOverlayWidth(),
						getOverlayHeight(),
						getOverlayWidth() * getOverlayFormat().bytesPerPixel,
						_overlayBuffer,
						getOverlayFormat());

	surface.copyFrom(overlaySurface);
}

void AtariGraphicsManager::copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) {
	Common::String str = Common::String::format("copyRectToOverlay: %d, %d, %d, %d, %d\n", pitch, x, y, w, h);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	static Graphics::Surface overlaySurface;	// never release via free/delete, we want to Mfree() it
	overlaySurface.init(getOverlayWidth(),
						getOverlayHeight(),
						getOverlayWidth() * getOverlayFormat().bytesPerPixel,
						_overlayBuffer,
						getOverlayFormat());

	overlaySurface.copyRectToSurface(buf, pitch, x, y, w, h);
}

int16 AtariGraphicsManager::getOverlayHeight() const {
	return SCREEN_HEIGHT;
}

int16 AtariGraphicsManager::getOverlayWidth() const {
	return SCREEN_WIDTH;
}
