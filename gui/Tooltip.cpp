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

#include "common/util.h"
#include "gui/widget.h"
#include "gui/dialog.h"
#include "gui/gui-manager.h"
#include "graphics/VectorRenderer.h"

#include "gui/Tooltip.h"
#include "gui/ThemeEval.h"

namespace GUI {


Tooltip::Tooltip() :
	Dialog(-1, -1, -1, -1), _maxWidth(-1), _parent(nullptr), _xdelta(0), _ydelta(0), _xpadding(0), _ypadding(0), _firstDraw(true) {

	_backgroundType = GUI::ThemeEngine::kDialogBackgroundTooltip;
}

void Tooltip::setup(Dialog *parent, Widget *widget, int x, int y) {
	assert(widget->hasTooltip());

	_parent = parent;

	setMouseUpdatedOnFocus(false);

	_maxWidth = g_gui.xmlEval()->getVar("Globals.Tooltip.MaxWidth", 100);
	_xdelta = g_gui.xmlEval()->getVar("Globals.Tooltip.XDelta", 0);
	_ydelta = g_gui.xmlEval()->getVar("Globals.Tooltip.YDelta", 0);
	_xpadding = g_gui.xmlEval()->getVar("Globals.Tooltip.XPadding", 2);
	_ypadding = g_gui.xmlEval()->getVar("Globals.Tooltip.YPadding", 2);

	const Graphics::Font *tooltipFont = g_gui.theme()->getFont(ThemeEngine::kFontStyleTooltip);

	_wrappedLines.clear();
	_w = tooltipFont->wordWrapText(widget->getTooltip(), _maxWidth - _xpadding * 2, _wrappedLines) + _xpadding * 2;
	_h = (tooltipFont->getFontHeight() + 2) * _wrappedLines.size() + _ypadding * 2;

	_x = MIN<int16>(parent->_x + x + _xdelta + _xpadding, g_system->getOverlayWidth() - _w - _xpadding * 2);
	_y = MIN<int16>(parent->_y + y + _ydelta + _ypadding, g_system->getOverlayHeight() - _h - _ypadding * 2);

	if (ConfMan.hasKey("tts_enabled", "scummvm") &&
			ConfMan.getBool("tts_enabled", "scummvm")) {
		Common::TextToSpeechManager *ttsMan = g_system->getTextToSpeechManager();
		if (ttsMan == nullptr)
			return;
		ttsMan->say(widget->getTooltip(), Common::TextToSpeechManager::QUEUE_NO_REPEAT);
	}
}

void Tooltip::drawDialog(DrawLayer layerToDraw) {
	int num = 0;
	int h = g_gui.theme()->getFontHeight(ThemeEngine::kFontStyleTooltip) + 2;

	// 	Dialog::drawDialog(layerToDraw)
	if (!isVisible())
		return;

	g_gui.theme()->disableClipRect();
	g_gui.theme()->_layerToDraw = layerToDraw;

	if (_firstDraw) {
		ThemeEngine *theme = g_gui.theme();

		// store backgrounds from Backbuffer and Screen
		theme->drawDialogBackground(Common::Rect(_x, _y, _x + _w, _y + _h), _backgroundType, &_bgRect);

		theme->drawToBackbuffer();
		_bgBackbufferSurf.create(_bgRect.width(), _bgRect.height(), theme->renderer()->getActiveSurface()->format);
		_bgBackbufferSurf.copyRectToSurface(*theme->renderer()->getActiveSurface(), 0, 0, _bgRect);

		theme->drawToScreen();
		_bgScreenSurf.create(_bgRect.width(), _bgRect.height(), theme->renderer()->getActiveSurface()->format);
		_bgScreenSurf.copyRectToSurface(*theme->renderer()->getActiveSurface(), 0, 0, _bgRect);

		theme->drawToBackbuffer();
		_firstDraw = false;
	}

	g_gui.theme()->drawDialogBackground(Common::Rect(_x, _y, _x + _w, _y + _h), _backgroundType);

	markWidgetsAsDirty();

#ifdef LAYOUT_DEBUG_DIALOG
	return;
#endif
	drawWidgets();
	// end of Dialog::drawDialog(layerToDraw)

	int16 textX = _x + 1 + _xpadding;
	if (g_gui.useRTL()) {
		textX = g_system->getOverlayWidth() - _w - textX;
	}

	int16 textY = _y + 1 + _ypadding;

	Graphics::TextAlign textAlignment = g_gui.useRTL() ? Graphics::kTextAlignRight : Graphics::kTextAlignLeft;

	for (Common::U32StringArray::const_iterator i = _wrappedLines.begin(); i != _wrappedLines.end(); ++i, ++num) {
		g_gui.theme()->drawText(
			Common::Rect(textX, textY + num * h, textX + _w, textY + (num + 1) * h),
			*i,
			ThemeEngine::kStateEnabled,
			textAlignment,
			ThemeEngine::kTextInversionNone,
			0,
			false,
			ThemeEngine::kFontStyleTooltip,
			ThemeEngine::kFontColorNormal,
			false
		);
	}
}

void Tooltip::open() {
	Dialog::open();
	g_gui._redrawStatus = GuiManager::kRedrawOpenTooltip;
}

void Tooltip::close() {
	Dialog::close();
	g_gui._redrawStatus = GuiManager::kRedrawDisabled;

	if (!_bgRect.isEmpty()) {
		ThemeEngine *theme = g_gui.theme();

		theme->drawToBackbuffer();
		theme->renderer()->getActiveSurface()->copyRectToSurface(
			_bgBackbufferSurf, _bgRect.left, _bgRect.top, Common::Rect(_bgRect.width(), _bgRect.height()));

		theme->drawToScreen();
		theme->renderer()->getActiveSurface()->copyRectToSurface(
			_bgScreenSurf, _bgRect.left, _bgRect.top, Common::Rect(_bgRect.width(), _bgRect.height()));

		theme->addDirtyRect(_bgRect);

		_bgRect = Common::Rect();
		_bgBackbufferSurf.free();
		_bgScreenSurf.free();
	}
}

}
