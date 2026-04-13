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

#ifndef WAYNESWORLD_WWINTRO_H
#define WAYNESWORLD_WWINTRO_H

#include "common/scummsys.h"

#define MAX_SOUNDS 20

namespace WaynesWorld {

class WaynesWorldEngine;
class GxlArchive;
class WWSurface;

struct Frame {
	const char *filename;
	int x;
	int y;
	int delay;
};

class WWIntro {
protected:
	WaynesWorldEngine *_vm;

	WWSurface *_outlineSurface = nullptr;
	WWSurface *_logoSurface = nullptr;
	WWSurface *_backg2Surface = nullptr;

public:
	WWIntro(WaynesWorldEngine *vm);
	virtual ~WWIntro() {}

	virtual void runIntro() = 0;
	WWSurface *_demoPt2Surface = nullptr;

protected:
	GxlArchive *_oanGxl = nullptr;

	bool initOanGxl();
	void wwEffect(int arg0, int arg1, bool flag);
};

class WWIntro_full : public WWIntro {
public:
	WWIntro_full(WaynesWorldEngine *vm);
	~WWIntro_full() override;

	void runIntro() override;

private:
	int _startOawPos = 0;
	int _startOagPos = 0;
	int _startOaoPos = 0;

	int _old_arg_refreshBackgFl = -1;
	int _old_arg_wBodyIndex = -1;
	int _old_arg_gBodyIndex = -1;
	int _old_argWHead1Index = -1;
	int _old_argGHead1Index = -1;

	bool introPt1();
	bool introPt3(bool flag);
	bool introPt4();
	void introPt5();
	void introPt6();
	void introPt7();

	void sub3009A(int textId);
	void sub2FEFB(int arg_refreshBackgFl, int arg_wBodyIndex, int arg_gBodyIndex, int arg_wHead1Index, int arg_gHead1Index, int arg_TextId);

	void cleanPt3();
	void introPt4_init();
	bool introPt4_intro();
	bool introPt4_displayCallInTime();
	bool introPt4_caller1();
	bool introPt4_caller2();
	bool introPt4_caller3();
	bool introPt4_caller4();
	void introPt4_cleanup();
	bool introPt4_playGuitar();

	WWSurface *_introBackg1Image = nullptr;
	WWSurface *_introWbodyImage[5] = {nullptr};
	WWSurface *_introGbodyImage = nullptr;
	WWSurface *_introWhead1[8] = {nullptr};
	WWSurface *_introGhead1[11] = {nullptr};
};

class WWIntro_demo1 : public WWIntro {
public:
	WWIntro_demo1(WaynesWorldEngine *vm);
	~WWIntro_demo1() override;

	void runIntro() override;

private:
	bool introPt1();
};
} // End of namespace WaynesWorld

#endif /* WAYNESWORLD_WWINTRO_H*/
