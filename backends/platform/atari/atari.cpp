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

#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>

#include <mint/osbind.h>

// We use some stdio.h functionality here thus we need to allow some
// symbols. Alternatively, we could simply allow everything by defining
// FORBIDDEN_SYMBOL_ALLOW_ALL
#define FORBIDDEN_SYMBOL_EXCEPTION_FILE
#define FORBIDDEN_SYMBOL_EXCEPTION_stdout
#define FORBIDDEN_SYMBOL_EXCEPTION_stderr
#define FORBIDDEN_SYMBOL_EXCEPTION_fputs
#define FORBIDDEN_SYMBOL_EXCEPTION_exit
#define FORBIDDEN_SYMBOL_EXCEPTION_time_h

#include "common/scummsys.h"

#if defined(ATARI)
#include "backends/keymapper/hardware-input.h"
#include "backends/modular-backend.h"
#include "backends/mutex/null/null-mutex.h"
#include "base/main.h"

#include "backends/saves/default/default-saves.h"
#include "backends/timer/default/default-timer.h"
#include "backends/events/default/default-events.h"
#include "backends/mixer/null/null-mixer.h"
#include "backends/graphics/atari/atari-graphics.h"
#include "gui/debugger.h"

/*
 * Include header files needed for the getFilesystemFactory() method.
 */
#include "backends/fs/posix/posix-fs-factory.h"

class OSystem_Atari : public ModularMixerBackend, public ModularGraphicsBackend, Common::EventSource {
public:
	OSystem_Atari();
	virtual ~OSystem_Atari();

	void initBackend() override;

	bool pollEvent(Common::Event &event) override;

	Common::MutexInternal *createMutex() override;
	uint32 getMillis(bool skipRecord = false) override;
	void delayMillis(uint msecs) override;
	void getTimeAndDate(TimeDate &td, bool skipRecord = false) const override;

	Common::HardwareInputSet *getHardwareInputSet() override;

	void quit() override;

	void logMessage(LogMessageType::Type type, const char *message) override;

	void addSysArchivesToSearchSet(Common::SearchSet &s, int priority) override;

private:
	clock_t _startTime;
	bool _ikbd_initialized = false;
	int _mouseX = -1;
	int _mouseY = -1;
	bool _oldLmbDown = false;
	bool _oldRmbDown = false;
};

OSystem_Atari::OSystem_Atari() {
	_fsFactory = new POSIXFilesystemFactory();
}

OSystem_Atari::~OSystem_Atari() {
}

static volatile bool intReceived = false;

static sighandler_t last_handler;

void intHandler(int dummy) {
	signal(SIGINT, last_handler);
	intReceived = true;
}

extern "C" void atari_ikbd_init();
extern "C" void atari_ikbd_shutdown();

extern void nf_init(void);
extern void nf_print(const char* msg);

void OSystem_Atari::initBackend() {
	_startTime = clock();

	last_handler = signal(SIGINT, intHandler);

	_timerManager = new DefaultTimerManager();
	_eventManager = new DefaultEventManager(this);
	_savefileManager = new DefaultSaveFileManager();
	_graphicsManager = new AtariGraphicsManager();
	_mixerManager = new NullMixerManager();
	// Setup and start mixer
	_mixerManager->init();

	nf_init();

	// TODO: store video settings
	Supexec(atari_ikbd_init);
	_ikbd_initialized = true;

	BaseBackend::initBackend();
}

//! bit 0: rmb
//! bit 1: lmb
volatile uint8	g_atari_ikbd_mouse_buttons_state = 0;
volatile int16	g_atari_ikbd_mouse_delta_x = 0;
volatile int16	g_atari_ikbd_mouse_delta_y = 0;

bool OSystem_Atari::pollEvent(Common::Event &event) {
	((DefaultTimerManager *)getTimerManager())->checkTimers();
	((NullMixerManager *)_mixerManager)->update(1);

	if (intReceived) {
		intReceived = false;

		GUI::Debugger *debugger = g_engine ? g_engine->getOrCreateDebugger() : nullptr;
		if (debugger && !debugger->isActive()) {
			last_handler = signal(SIGINT, intHandler);
			event.type = Common::EVENT_DEBUGGER;
			return true;
		}

		event.type = Common::EVENT_QUIT;
		return true;
	}

	if (_mouseX == -1 || _mouseY == -1) {
		if (isOverlayVisible()) {
			_mouseX = getOverlayWidth() / 2;
			_mouseY = getOverlayHeight() / 2;
		} else {
			_mouseX = getWidth() / 2;
			_mouseY = getHeight() / 2;
		}

		Common::String str = Common::String::format("warping to: %d, %d (%d)\n", _mouseX, _mouseY, isOverlayVisible());
		logMessage(LogMessageType::kDebug, str.c_str());

		warpMouse(_mouseX, _mouseY);
	}

	if ((g_atari_ikbd_mouse_buttons_state & 0x01) && !_oldRmbDown) {
		Common::String str = Common::String::format("EVENT_RBUTTONDOWN\n");
		logMessage(LogMessageType::kDebug, str.c_str());

		event.type = Common::EVENT_RBUTTONDOWN;
		_oldRmbDown = true;
		return true;
	}

	if (!(g_atari_ikbd_mouse_buttons_state & 0x01) && _oldRmbDown) {
		Common::String str = Common::String::format("EVENT_RBUTTONUP\n");
		logMessage(LogMessageType::kDebug, str.c_str());

		event.type = Common::EVENT_RBUTTONUP;
		_oldRmbDown = false;
		return true;
	}

	if ((g_atari_ikbd_mouse_buttons_state & 0x02) && !_oldLmbDown) {
		Common::String str = Common::String::format("EVENT_LBUTTONDOWN\n");
		logMessage(LogMessageType::kDebug, str.c_str());

		event.type = Common::EVENT_LBUTTONDOWN;
		_oldLmbDown = true;
		return true;
	}

	if (!(g_atari_ikbd_mouse_buttons_state & 0x02) && _oldLmbDown) {
		Common::String str = Common::String::format("EVENT_LBUTTONUP\n");
		logMessage(LogMessageType::kDebug, str.c_str());

		event.type = Common::EVENT_LBUTTONUP;
		_oldLmbDown = false;
		return true;
	}

	if (g_atari_ikbd_mouse_delta_x != 0 || g_atari_ikbd_mouse_delta_y != 0) {
		const int deltaX = g_atari_ikbd_mouse_delta_x;
		const int deltaY = g_atari_ikbd_mouse_delta_y;

		g_atari_ikbd_mouse_delta_x = g_atari_ikbd_mouse_delta_y = 0;

		_mouseX += deltaX;
		_mouseY += deltaY;

		const int maxX = isOverlayVisible() ? getOverlayWidth() : getWidth();
		const int maxY = isOverlayVisible() ? getOverlayHeight() : getHeight();

		if (_mouseX < 0)
			_mouseX = 0;
		else if (_mouseX >= maxX)
			_mouseX = maxX - 1;

		if (_mouseY < 0)
			_mouseY = 0;
		else if (_mouseY >= maxY)
			_mouseY = maxY - 1;

		event.type = Common::EVENT_MOUSEMOVE;
		event.mouse	= Common::Point(_mouseX, _mouseY);
		event.relMouse = Common::Point(deltaX, deltaY);

		warpMouse(_mouseX, _mouseY);

		return true;
	}

	return false;
}

Common::MutexInternal *OSystem_Atari::createMutex() {
	return new NullMutexInternal();
}

uint32 OSystem_Atari::getMillis(bool skipRecord) {
	// CLOCKS_PER_SEC is 200, so no need to use floats
	return 1000 * (clock() - _startTime) / CLOCKS_PER_SEC;
}

void OSystem_Atari::delayMillis(uint msecs) {
	usleep(msecs * 1000);
}

void OSystem_Atari::getTimeAndDate(TimeDate &td, bool skipRecord) const {
	time_t curTime = time(0);
	// TODO: if too slow (e.g. when calling RandomSource::RandomSource()), rewrite
	struct tm t = *localtime(&curTime);
	td.tm_sec = t.tm_sec;
	td.tm_min = t.tm_min;
	td.tm_hour = t.tm_hour;
	td.tm_mday = t.tm_mday;
	td.tm_mon = t.tm_mon;
	td.tm_year = t.tm_year;
	td.tm_wday = t.tm_wday;
}

Common::HardwareInputSet *OSystem_Atari::getHardwareInputSet()
{
	Common::CompositeHardwareInputSet *inputSet = new Common::CompositeHardwareInputSet();
	inputSet->addHardwareInputSet(new Common::MouseHardwareInputSet(Common::defaultMouseButtons));
	inputSet->addHardwareInputSet(new Common::KeyboardHardwareInputSet(Common::defaultKeys, Common::defaultModifiers));
	//inputSet->addHardwareInputSet(new Common::JoystickHardwareInputSet(Common::defaultJoystickButtons, Common::defaultJoystickAxes));

	return inputSet;
}

void OSystem_Atari::quit() {
	// TODO: restore video settings
	if (_ikbd_initialized) {
		Supexec(atari_ikbd_shutdown);
		_ikbd_initialized = false;
	}

	exit(0);
}

void OSystem_Atari::logMessage(LogMessageType::Type type, const char *message) {
	FILE *output = 0;

	if (type == LogMessageType::kInfo || type == LogMessageType::kDebug)
		output = stdout;
	else
		output = stderr;

	fputs(message, output);
	fflush(output);

	nf_print(message);
}

void OSystem_Atari::addSysArchivesToSearchSet(Common::SearchSet &s, int priority) {
	s.add("gui/themes", new Common::FSDirectory("gui/themes", 4), priority);
}

OSystem *OSystem_Atari_create() {
	return new OSystem_Atari();
}

int main(int argc, char *argv[]) {
	g_system = OSystem_Atari_create();
	assert(g_system);

	// Invoke the actual ScummVM main entry point:
	int res = scummvm_main(argc, argv);
	g_system->destroy();
	return res;
}

#endif
