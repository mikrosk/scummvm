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

WWIntro_demo1::WWIntro_demo1(WaynesWorldEngine *vm) : WWIntro(vm) {
}

WWIntro_demo1::~WWIntro_demo1() {
}

void WWIntro_demo1::runIntro() {
	// continueFl is used like in the full version, but for the moment it's not possible to skip (demo is not interactive)
	bool continueFl = initOanGxl();

	if (continueFl)
		continueFl = introPt1();

	if (continueFl)
		continueFl = introPt3();

	if (continueFl)
		continueFl = introPt4();

}

bool WWIntro_demo1::introPt1() {
	GxlArchive *capspinGxl = new GxlArchive("capspin");
	_vm->loadPalette(capspinGxl, "paramnt.pcx");
	_vm->paletteFadeOut(0, 256, 64);
	_vm->drawImageToScreen(capspinGxl, "paramnt.pcx", 0, 0);
	_vm->paletteFadeIn(0, 256, 3);
	_vm->waitSeconds(1);

	WWSurface *paramaSprite = new WWSurface(190, 112);
	// "And" animation
	_vm->drawImageToSurface(capspinGxl, "parama.pcx", paramaSprite, 0, 0);
	_vm->drawRandomEffect(paramaSprite, 66, 30, 2, 2);
	delete paramaSprite;
	_vm->waitSeconds(2);

	_vm->paletteFadeOut(0, 256, 6);
	_vm->_screen->clear(0);
	_vm->loadPalette(capspinGxl, "backg.pcx");
	_vm->paletteFadeOut(0, 256, 64);
	_vm->drawImageToScreen(capspinGxl, "backg.pcx", 0, 0);
	_vm->paletteFadeIn(0, 256, 6);

	// The original has all the frames hardcoded one after the other, I used a loop instead.
	Frame animation[] = {
		{"cap01.pcx", 179, 97, 30},
		{"cap02.pcx", 179, 97, 30},
		{"cap03.pcx", 181, 98, 30},
		{"cap04.pcx", 183, 98, 30},
		{"cap05.pcx", 186, 97, 30},
		{"cap06.pcx", 189, 96, 30},
		{"cap07.pcx", 192, 94, 30},
		{"cap08.pcx", 198, 91, 30},
		{"cap09.pcx", 208, 88, 30},
		{"cap10.pcx", 212, 85, 30},
		{"cap11.pcx", 217, 84, 30},
		{"cap12.pcx", 223, 82, 30},
		{"cap13.pcx", 234, 81, 30},
		{"cap14.pcx", 233, 78, 30},
		{"cap15.pcx", 236, 76, 30},
		{"cap16.pcx", 240, 76, 30},
		{"cap17.pcx", 241, 75, 30},
		{"cap18.pcx", 238, 74, 30},
		{"cap19.pcx", 238, 72, 30},
		{"cap20.pcx", 238, 72, 30},
		{"cap21.pcx", 225, 71, 30},
		{"cap22.pcx", 218, 69, 30},
		{"cap23.pcx", 218, 68, 30},
		{"cap24.pcx", 192, 68, 30},
		{"cap25.pcx", 164, 67, 30},
		{"cap26.pcx", 133, 67, 30},
		{"cap27.pcx", 103, 66, 30},
		{"cap28.pcx", 87, 68, 1030},
		{"txt01.pcx", 0, 138, 30},
		{"txt02.pcx", 0, 138, 30},
		{"txt03.pcx", 0, 138, 30},
		{"txt04.pcx", 0, 138, 30},
		{"txt05.pcx", 0, 138, 30},
		{"txt06.pcx", 0, 138, 30},
		{"txt07.pcx", 0, 138, 30},
		{"txt08.pcx", 0, 138, 30},
		{"txt09.pcx", 0, 138, 1030},
		{"captxt01.pcx", 113, 89, 30},
		{"captxt02.pcx", 113, 89, 30},
		{"captxt03.pcx", 113, 89, 30},
		{"captxt04.pcx", 113, 89, 30},
		{"captxt05.pcx", 113, 89, 30},
		{"captxt06.pcx", 113, 89, 30},
		{"captxt07.pcx", 113, 89, 30},
		{"captxt08.pcx", 113, 89, 30},
		{"captxt09.pcx", 113, 89, 30},
		{"captxt10.pcx", 113, 89, 30},
		{"captxt11.pcx", 113, 89, 1030},
		{"txt09.pcx", 0, 138, 30},
		{"txt08.pcx", 0, 138, 30},
		{"txt07.pcx", 0, 138, 30},
		{"txt06.pcx", 0, 138, 30},
		{"txt05.pcx", 0, 138, 30},
		{"txt04.pcx", 0, 138, 30},
		{"txt03.pcx", 0, 138, 30},
		{"txt02.pcx", 0, 138, 30},
		{"txt01.pcx", 0, 138, 30},
		{"txtbkg.pcx", 0, 138, 1030},
		{"capawy01.pcx", 80, 67, 30},
		{"capawy02.pcx", 80, 67, 30},
		{"capawy03.pcx", 69, 64, 30},
		{"capawy04.pcx", 37, 54, 30},
		{"capawy05.pcx", 0, 41, 30},
		{"capawy06.pcx", 0, 27, 30},
		{"capawy07.pcx", 0, 27, 30},
		{"capawy08.pcx", 0, 27, 1030}
	};

	for (const auto &frame : animation) {
		_vm->drawImageToScreen(capspinGxl, frame.filename, frame.x, frame.y);
		_vm->waitMillis(frame.delay);
	}

	delete capspinGxl;
	return true;
}

bool WWIntro_demo1::introPt3() {
	// sub1 - Parameter is always 'true' so it has been removed and the code simplified
	_vm->paletteFadeOut(0, 256, 64);
	_vm->_screen->clear(0);
	_vm->loadPalette(_oanGxl, "backg1.pcx");
	_vm->paletteFadeOut(0, 256, 64);
	
	introPt3_init();
	// End of sub1

	_demoPt2Surface->clear(0);
	_demoPt2Surface->drawSurface(_logoSurface, 47, 25);
	wwEffect(1, 0);
	_vm->paletteFadeIn(0, 256, 6);

	_vm->playSound("theme1.snd", 0);

	wwEffect(1, 1);
	wwEffect(1, 2);
	wwEffect(1, 3);

	for (int i = 0; i < 24; ++i) {
		setColor236(i % 19);
		wwEffect((i % 8) + 1, 4);
	}

	wwEffect(1, 3);
	wwEffect(1, 2);
	wwEffect(1, 1);
	wwEffect(1, 0);

	while (_vm->_sound->isSFXPlaying())
		_vm->waitMillis(30);

	_vm->_sound->playSound("exclnt.snd", false);

	introPt3_clean();

	return true;
}

bool WWIntro_demo1::introPt4() {
	bool retVal = true;

	introPt4_init();
	if (!introPt4_intro()) {
		retVal = false;
	}

	introPt4_cleanup();
	return retVal;
}

void WWIntro_demo1::introPt4_init() {
	_vm->_fontWW = new GFTFont();
	_vm->_fontWW->loadFromFile("ww.gft");

	while (_vm->_sound->isSFXPlaying())
		_vm->waitMillis(30);

	_introBackg1Image = new WWSurface(320, 170);
	_vm->drawImageToSurface(_oanGxl, "backg1.pcx", _introBackg1Image, 0, 0);
	_introWbodyImage[0] = new WWSurface(145, 118);
	_vm->drawImageToSurface(_oanGxl, "wbody0.pcx", _introWbodyImage[0], 0, 0);
	_introGbodyImage = new WWSurface(160, 149);
	_vm->drawImageToSurface(_oanGxl, "gbody0.pcx", _introGbodyImage, 0, 0);

	// The original is overwriting the song name (default metal1.xmi) instead of setting the musicIndex.
	_vm->_musicIndex = 2; // metal2.xmi
	_vm->changeMusic();

	for (int i = 0; i < 8; ++i) {
		_introWhead1[i] = new WWSurface(98, 71);
		Common::String filename = Common::String::format("whead1%d.pcx", i);
		_vm->drawImageToSurface(_oanGxl, filename.c_str(), _introWhead1[i], 0, 0);
	}

	for (int i = 0; i < 11; ++i) {
		_introGhead1[i] = new WWSurface(138, 80);
		Common::String filename = Common::String::format("ghead1%d.pcx", i);
		_vm->drawImageToSurface(_oanGxl, filename.c_str(), _introGhead1[i], 0, 0);
	}
	
	_vm->drawImageToScreen(_oanGxl, "backg1.pcx", 0, 15);
	_vm->paletteFadeIn(0, 256, 2);
}

bool WWIntro_demo1::introPt4_intro() {
	_vm->_sound->playSound("ok.abt", false);
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 20; ++j) {
			sub2FEFB(1, 0, 1, _vm->getRandom(3), 9, 0);
		}

		++_startOawPos;
	}

	return true;
}

void WWIntro_demo1::introPt4_cleanup() {
	delete _vm->_fontWW;
	_vm->_fontWW = nullptr;
	delete _introBackg1Image;
	_introBackg1Image = nullptr;
	for (int i = 0; i < 5; ++i) {
		delete _introWbodyImage[i];
		_introWbodyImage[i] = nullptr;
	}
	delete _introGbodyImage;
	_introGbodyImage = nullptr;
	for (int i = 0; i < 7; ++i) {
		delete _introWhead1[i];
		_introWhead1[i] = nullptr;
	}
	for (int i = 0; i < 11; ++i) {
		delete _introGhead1[i];
		_introGhead1[i] = nullptr;
	}
}

} // End of namespace WaynesWorld
