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

#include "waynesworld/wwintro.h"
#include "waynesworld/waynesworld.h"
#include "waynesworld/graphics.h"
#include "waynesworld/gxlarchive.h"

#include "audio/audiostream.h"
#include "graphics/paletteman.h"

namespace WaynesWorld {

WWIntro::WWIntro(WaynesWorldEngine *vm) : _vm(vm) {
}

bool WWIntro::initOanGxl() {
	_vm->paletteFadeOut(0, 256, 64);

	_oanGxl = new GxlArchive("oan");
	_vm->loadPalette(_oanGxl, "backg2.pcx");
	_demoPt2Surface = new WWSurface(320, 200);
	_vm->_midi->stopSong();

	return true;
}

void WWIntro::wwEffect(int arg0, int arg1, bool flag) {
	int xmult = 0;
	int ymult = 0;

	switch (arg0) {
	case 1:
		xmult = 10;
		ymult = 7;
		break;
	case 2:
		xmult = 0;
		ymult = 7;
		break;
	case 3:
		xmult = -10;
		ymult = 7;
		break;
	case 4:
		xmult = -10;
		ymult = 0;
		break;
	case 5:
		xmult = -10;
		ymult = -7;
		break;
	case 6:
		xmult = 0;
		ymult = -7;
		break;
	case 7:
		xmult = 10;
		ymult = -7;
		break;
	case 8:
		xmult = 10;
		ymult = 0;
		break;
	default:
		break;
	}

	_demoPt2Surface->clear(0);
	if (flag) {
		_demoPt2Surface->drawSurface(_backg2Surface, 0, 15);
	}

	for (int i = 4; i > 0; --i) {
		if (i <= arg1) {
			_demoPt2Surface->drawSurfaceTransparent(_outlineSurface, (i * xmult) + 47, (i * ymult) + 25);
		}
	}

	_demoPt2Surface->drawSurfaceTransparent(_logoSurface, 47, 25);
	_vm->_screen->drawSurface(_demoPt2Surface, 0, 0);

	// Added for better results
	_vm->waitMillis(200);
}

void WWIntro::setColor236(int index) {
	byte newColor[3] = {0, 0, 0};
	static const byte rArr[] = {9, 9, 9, 9, 43, 43, 53, 63, 63, 63, 63, 63, 63, 63, 45, 28, 9, 9, 9};
	static const byte gArr[] = {33, 33, 40, 47, 47, 47, 47, 47, 35, 23, 0, 0, 0, 0, 0, 0, 0, 33, 33};
	static const byte bArr[] = {29, 20, 20, 20, 20, 0, 0, 0, 0, 0, 0, 23, 37, 50, 50, 50, 50, 50, 40};

	newColor[0] = rArr[index] * 4;
	newColor[1] = gArr[index] * 4;
	newColor[2] = bArr[index] * 4;

	g_system->getPaletteManager()->setPalette((const byte *)&newColor, 236, 1);
}

void WWIntro::sub3009A(int textId) {
	int startPos;
	byte textColor;
	int textType = 0;
	Common::String filename;

	switch (textId) {
	case 0:
		filename = "oaw";
		startPos = _startOawPos;
		textColor = 147;
		break;
	case 1:
		filename = "oag";
		startPos = _startOagPos;
		textColor = 41;
		break;
	default:
		filename = "oao";
		startPos = _startOaoPos;
		++_startOaoPos;
		textColor = 11;
		textType = 1;
		break;
	}

	Common::String displayTxt = _vm->loadString(filename.c_str(), startPos, 0);

	if (textType && !(_vm->_gameDescription->flags & ADGF_DEMO))
		_vm->_fontWW->drawText(_demoPt2Surface, displayTxt.c_str(), 0, 187, textColor);
	else
		_vm->_fontWW->drawText(_demoPt2Surface, displayTxt.c_str(), 0, 2, textColor);
}

void WWIntro::sub2FEFB(int arg_refreshBackgFl, int arg_wBodyIndex, int arg_gBodyIndex, int arg_wHead1Index, int arg_gHead1Index, int arg_TextId) {
	_demoPt2Surface->fillRect(0, 0, 319, 14, 0);
	_demoPt2Surface->fillRect(0, 185, 319, 199, 0);

	if (arg_refreshBackgFl != _oldRefreshBackgFl) {
		_demoPt2Surface->clear(0);
		_demoPt2Surface->drawSurface(_introBackg1Image, 0, 15);
		_oldRefreshBackgFl = arg_refreshBackgFl;
	}

	if (arg_wBodyIndex != _oldWBodyIndex) {
		_demoPt2Surface->drawSurface(_introWbodyImage[arg_wBodyIndex], 0, 21);
		_oldWBodyIndex = arg_wBodyIndex;
	}

	if (arg_gBodyIndex != _oldGBodyIndex) {
		_demoPt2Surface->drawSurface(_introGbodyImage, 160, 25);
		_oldGBodyIndex = arg_gBodyIndex;
	}

	if (arg_wHead1Index != _oldWHead1Index) {
		_demoPt2Surface->drawSurface(_introWhead1[arg_wHead1Index], 12, 22);
		_oldWHead1Index = arg_wHead1Index;
	}

	if (arg_gHead1Index != _oldGHead1Index) {
		_demoPt2Surface->drawSurface(_introGhead1[arg_gHead1Index], 182, 21);
		_oldGHead1Index = arg_gHead1Index;
	}

	if (arg_TextId != -1) {
		sub3009A(arg_TextId);
	}

	_vm->_screen->drawSurface(_demoPt2Surface, 0, 0);
	_vm->waitMillis(170);
}

void WWIntro::introPt3_init() {
	_backg2Surface = new WWSurface(320, 170);
	_logoSurface = new WWSurface(226, 134);
	_outlineSurface = new WWSurface(226, 134);

	_vm->drawImageToSurface(_oanGxl, "backg2.pcx", _backg2Surface, 0, 0);
	_vm->drawImageToSurface(_oanGxl, "logo.pcx", _logoSurface, 0, 0);
	_vm->drawImageToSurface(_oanGxl, "outline.pcx", _outlineSurface, 0, 0);
}

void WWIntro::introPt3_clean() {
	delete _outlineSurface;
	delete _logoSurface;
	delete _backg2Surface;

	_outlineSurface = _logoSurface = _backg2Surface = nullptr;
}

} // End of namespace WaynesWorld
