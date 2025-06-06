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

#ifndef PINK_PINK_H
#define PINK_PINK_H

#include "common/random.h"
#include "common/savefile.h"

#include "engines/engine.h"
#include "engines/savestate.h"

#include "gui/debugger.h"

#include "pink/constants.h"
#include "pink/file.h"
#include "pink/utils.h"
#include "pink/pda_mgr.h"

/*
 *  This is the namespace of the Pink engine.
 *
 *  Status of this engine: In Development
 *
 *  Internal name of original engine: OxCart Runtime
 *
 *  Games using this engine:
 *  - The Pink Panther: Passport to Peril
 *  - The Pink Panther: Hokus Pokus Pink
 *
 *  Peril game status:
 *      Fully playable*
 *
 *  Pokus game status:
 *      Fully Playable*
 *
 *  Known bugs:
 *      PDA is partially usable(ActionText is not implemented)
 */

struct ADGameDescription;

namespace Common {
	class PEResources;
}

namespace Graphics {
class MacMenu;
struct WinCursorGroup;
class PaletteLookup;
}

namespace Pink {

class Console;
class Screen;
class Archive;
class NamedObject;
class Module;
class Page;
class LeadActor;

enum {
	kPinkDebugGeneral = 1,
	kPinkDebugLoadingResources,
	kPinkDebugLoadingObjects,
	kPinkDebugScripts,
	kPinkDebugActions,
};

enum {
	GF_COMPRESSED = 1 << 0,
};

extern Graphics::PaletteLookup *g_paletteLookup;

class PinkEngine : public Engine {
public:
	PinkEngine(OSystem *system, const ADGameDescription *desc);
	~PinkEngine() override;

	Common::Error run() override;

	bool hasFeature(EngineFeature f) const override;

	Common::Error loadGameState(int slot) override;
	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override;

	Common::Error saveGameState(int slot, const Common::String &desc, bool isAutosave = false) override;
	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override;
	Common::String getSaveStateName(int slot) const override {
		return Common::String::format("%s.s%02d", _targetName.c_str(), slot);
	}
	SaveStateList listSaves() const;

	friend class Console;

protected:
	void pauseEngineIntern(bool pause) override;

public:
	void load(Archive &archive);

	void changeScene();

	bool isPeril() const;
	bool isPerilDemo() const;

	void setVariable(Common::String &variable, Common::String &value);
	bool checkValueOfVariable(const Common::String &variable, const Common::String &value) const;

	void executeMenuCommand(uint id);
	bool executePageChangeCommand(uint id);

	Common::Language getLanguage() const;
	OrbFile *getOrb()  { return &_orb; }
	BroFile *getBro()  { return _bro; }
	Common::RandomSource &getRnd() { return _rnd; };
	Screen *getScreen() { return _screen; }
	PDAMgr &getPdaMgr() { return _pdaMgr; }

	void setNextExecutors(const Common::String &nextModule, const Common::String &nextPage) { _nextModule = nextModule; _nextPage = nextPage; }
	void setLeadActor(LeadActor *actor) { _actor = actor; };
	void setCursor(uint cursorIndex);

private:
	Common::Error init();

	void initMenu();

	bool loadCursors();

	void initModule(const Common::String &moduleName, const Common::String &pageName, Archive *saveFile);
	void addModule(const Common::String &moduleName);
	void removeModule();

	void openLocalWebPage(const Common::String &pageName) const;

private:
	Common::RandomSource _rnd;
	Common::Array<Graphics::WinCursorGroup *> _cursors;

	Common::String _nextModule;
	Common::String _nextPage;

	Common::PEResources *_exeResources;

	OrbFile  _orb;
	BroFile *_bro;

	Graphics::MacMenu *_menu;
	Screen *_screen;
	LeadActor *_actor;

	Module *_module;
	Array<NamedObject *> _modules;

	StringMap _variables;
	PDAMgr _pdaMgr;

	const ADGameDescription *_desc;
	bool _isPeril;
	bool _isPerilDemo;
};

WARN_UNUSED_RESULT bool readSaveHeader(Common::InSaveFile &in, SaveStateDescriptor &desc, bool skipThumbnail = true);
Common::String generateSaveName(int slot, const char *gameId);

} // End of namespace Pink

#endif
