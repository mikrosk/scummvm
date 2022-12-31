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

#include "common/str.h"
#include "graphics/surface.h"

bool AtariGraphicsManager::setGraphicsMode(int mode, uint flags)
{
	Common::String str = Common::String::format("setGraphicsMode: %d, %d\n", mode, flags);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	return (mode == 0);
}

void AtariGraphicsManager::initSize(uint width, uint height, const Graphics::PixelFormat *format)
{
	Common::String str = Common::String::format("initSize: %d, %d\n", width, height);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	_width = width;
	_height = height;
	_format = format ? *format : Graphics::PixelFormat::createFormatCLUT8();
}

void AtariGraphicsManager::beginGFXTransaction()
{
	Common::String str = Common::String::format("beginGFXTransaction\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());
}

OSystem::TransactionError AtariGraphicsManager::endGFXTransaction()
{
	Common::String str = Common::String::format("endGFXTransaction\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	return OSystem::kTransactionSuccess;
}

void AtariGraphicsManager::setPalette(const byte *colors, uint start, uint num)
{
	Common::String str = Common::String::format("setPalette: %d, %d\n", start, num);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());
}

void AtariGraphicsManager::grabPalette(byte *colors, uint start, uint num) const
{
	Common::String str = Common::String::format("grabPalette: %d, %d\n", start, num);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());
}

void AtariGraphicsManager::copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h)
{
	Common::String str = Common::String::format("copyRectToScreen: %d, %d, %d, %d, %d\n", pitch, x, y, w, h);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());
}

Graphics::Surface *AtariGraphicsManager::lockScreen()
{
	Common::String str = Common::String::format("lockScreen\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	return NULL;
}

void AtariGraphicsManager::showOverlay()
{
	Common::String str = Common::String::format("showOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	_overlayVisible = true;
}

void AtariGraphicsManager::hideOverlay()
{
	Common::String str = Common::String::format("hideOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());

	_overlayVisible = false;
}

void AtariGraphicsManager::clearOverlay()
{
	Common::String str = Common::String::format("clearOverlay\n");
	g_system->logMessage(LogMessageType::kDebug, str.c_str());
}

void AtariGraphicsManager::grabOverlay(Graphics::Surface &surface) const
{
	Common::String str = Common::String::format("grabOverlay: %d, %d, %d\n", surface.pitch, surface.w, surface.h);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());
}

void AtariGraphicsManager::copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h)
{
	Common::String str = Common::String::format("copyRectToOverlay: %d, %d, %d, %d, %d\n", pitch, x, y, w, h);
	g_system->logMessage(LogMessageType::kDebug, str.c_str());
}
