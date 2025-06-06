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

#include "sci/sci.h"
#include "sci/engine/features.h"
#include "sci/engine/state.h"
#include "sci/engine/selector.h"
#include "sci/engine/workarounds.h"
#include "sci/graphics/cache.h"
#include "sci/graphics/coordadjuster.h"
#include "sci/graphics/ports.h"
#include "sci/graphics/paint16.h"
#include "sci/graphics/animate.h"
#include "sci/graphics/scifont.h"
#include "sci/graphics/picture.h"
#include "sci/graphics/view.h"
#include "sci/graphics/screen.h"
#include "sci/graphics/drivers/gfxdriver.h"
#include "sci/graphics/palette16.h"
#include "sci/graphics/portrait.h"
#include "sci/graphics/text16.h"
#include "sci/graphics/transitions.h"

#include "sci/graphics/scifx.h"

namespace Sci {

GfxPaint16::GfxPaint16(ResourceManager *resMan, SegManager *segMan, GfxCache *cache, GfxPorts *ports, GfxCoordAdjuster16 *coordAdjuster, GfxScreen *screen, GfxPalette *palette, GfxTransitions *transitions, AudioPlayer *audio)
	: _resMan(resMan), _segMan(segMan), _cache(cache), _ports(ports),
	  _coordAdjuster(coordAdjuster), _screen(screen), _palette(palette),
	  _transitions(transitions), _audio(audio), _EGAdrawingVisualize(false),
	  _hiresDrawObjs(nullptr), _hiresPortraitWorkaroundFlag(false) {

	// _animate and _text16 will be initialized later on
	_animate = nullptr;
	_text16 = nullptr;
}

// The original KQ6WinCD interpreter saves the hires drawing information in a linked list. This is used to redraw the hires cels after disposing a window.
struct HiresDrawData {
	HiresDrawData(HiresDrawData *chain, reg_t hiresHandle, GuiResourceId id, int16 loop, int16 cel, uint16 left, uint16 top, uint16 pal, byte priority, bool needsWorkaround)
		: handle(hiresHandle), viewId(id), lpNo(loop), celNo(cel), leftPos(left), topPos(top), palNo(pal), prio(priority), waFlag(needsWorkaround), prev(nullptr), next(chain) {
		if (chain)
			chain->prev = this;
	}
	reg_t handle;
	GuiResourceId viewId;
	int16 lpNo, celNo;
	uint16 leftPos, topPos, palNo;
	byte prio;
	bool waFlag;
	HiresDrawData *prev, *next;
};

GfxPaint16::~GfxPaint16() {
	while (_hiresDrawObjs) {
		HiresDrawData *next = _hiresDrawObjs->next;
		delete _hiresDrawObjs;
		_hiresDrawObjs = next;
	}
}

void GfxPaint16::init(GfxAnimate *animate, GfxText16 *text16) {
	_animate = animate;
	_text16 = text16;
}

void GfxPaint16::debugSetEGAdrawingVisualize(bool state) {
	_EGAdrawingVisualize = state;
}

void GfxPaint16::drawPicture(GuiResourceId pictureId, bool mirroredFlag, bool addToFlag, GuiResourceId paletteId) {
	// Set up custom per-picture palette mod
	doCustomPicPalette(_screen, pictureId);

	// do we add to a picture? if not -> clear screen with white
	if (!addToFlag)
		clearScreen(_screen->getColorWhite());

	// Draw the picture
	GfxPicture picture(_resMan, _coordAdjuster, _ports, _screen, _palette, pictureId, _EGAdrawingVisualize);
	picture.draw(mirroredFlag, addToFlag, paletteId);

	// We make a call to SciPalette here, for increasing sys timestamp and also loading targetpalette, if palvary active
	//  (SCI1.1 only)
	if (getSciVersion() == SCI_VERSION_1_1)
		_palette->drewPicture(pictureId);

	// Reset custom per-picture palette mod
	_screen->setCurPaletteMapValue(0);
}

// This one is the only one that updates screen!
void GfxPaint16::drawCelAndShow(GuiResourceId viewId, int16 loopNo, int16 celNo, uint16 leftPos, uint16 topPos, byte priority, uint16 paletteNo, uint16 scaleX, uint16 scaleY, uint16 scaleSignal) {
	GfxView *view = _cache->getView(viewId);
	Common::Rect celRect;

	if (view) {
		celRect.left = leftPos;
		celRect.top = topPos;
		celRect.right = celRect.left + view->getWidth(loopNo, celNo);
		celRect.bottom = celRect.top + view->getHeight(loopNo, celNo);

		drawCel(view, loopNo, celNo, celRect, priority, paletteNo, scaleX, scaleY, scaleSignal);

		if (getSciVersion() >= SCI_VERSION_1_1) {
			if (!_screen->_picNotValidSci11) {
				bitsShow(celRect);
			}
		} else {
			if (!_screen->_picNotValid)
				bitsShow(celRect);
		}
	}
}

// This version of drawCel is not supposed to call bitsShow()!
void GfxPaint16::drawCel(GuiResourceId viewId, int16 loopNo, int16 celNo, const Common::Rect &celRect, byte priority, uint16 paletteNo, uint16 scaleX, uint16 scaleY, uint16 scaleSignal) {
	drawCel(_cache->getView(viewId), loopNo, celNo, celRect, priority, paletteNo, scaleX, scaleY, scaleSignal);
}

// This version of drawCel is not supposed to call bitsShow()!
void GfxPaint16::drawCel(GfxView *view, int16 loopNo, int16 celNo, const Common::Rect &celRect, byte priority, uint16 paletteNo, uint16 scaleX, uint16 scaleY, uint16 scaleSignal) {
	Common::Rect clipRect = celRect;
	clipRect.clip(_ports->_curPort->rect);
	if (clipRect.isEmpty()) // nothing to draw
		return;

	Common::Rect clipRectTranslated = clipRect;
	_ports->offsetRect(clipRectTranslated);
	if (scaleX == 128 && scaleY == 128)
		view->draw(celRect, clipRect, clipRectTranslated, loopNo, celNo, priority, paletteNo, false, scaleSignal);
	else
		view->drawScaled(celRect, clipRect, clipRectTranslated, loopNo, celNo, priority, scaleX, scaleY, scaleSignal);
}

// This is used as replacement for drawCelAndShow() when hires cels are drawn to screen. Hires cels are available only in SCI 1.1.
void GfxPaint16::drawHiresCelAndShow(GuiResourceId viewId, int16 loopNo, int16 celNo, uint16 leftPos, uint16 topPos, byte priority, uint16 paletteNo, reg_t hiresHandle, bool storeDrawingInfo) {
	GfxView *view = _cache->getView(viewId);
	if (!view)
		return;

	byte *memoryPtr = _segMan->getHunkPointer(hiresHandle);
	if (!memoryPtr) {
		// The original KQ6WinCD interpreter does not treat this as an error, it just skips the hires drawing. It happens all the time
		// when attempting to redraw the hires cels from the _hiresDrawObjs chain. We're supposed to just skip the invalidated items.
		return;
	}

	Common::Rect picRect;
	_screen->bitsGetRect(memoryPtr, &picRect);
	Common::Rect clipRect(makeHiresRect(picRect));
	Common::Rect celRect(view->getWidth(loopNo, celNo), view->getHeight(loopNo, celNo));
	celRect.translate(leftPos + clipRect.left, topPos + clipRect.top);
	clipRect.clip(celRect);

	view->draw(celRect, clipRect, clipRect, loopNo, celNo, priority, paletteNo, true);

	// The original KQ6WinCD interpreter saves the hires drawing information in a linked list. There are two use cases, one is redrawing the
	// window background when receiving WM_PAINT messages (which is irrelevant for us, since that happens in the backend) and the other is
	// redrawing the inventory after displaying a text window over it. This only happens in mixed speech+text mode, which does not even exist
	// in the original. We do have that mode as a ScummVM feature, though. That's why we have that code, to be able to refresh the inventory.
	// We also check if the portrait is drawn outside the viewport boundaries (happens in the unofficial mixed speech+text mode) and set
	// a flag to trigger a workaround when restoring the background.
	if (storeDrawingInfo && !hasHiresDrawObjectAt(leftPos, topPos))
		_hiresDrawObjs = new HiresDrawData(_hiresDrawObjs, hiresHandle, viewId, loopNo, celNo, leftPos, topPos, paletteNo, priority, picRect.top < _ports->_curPort->top);
}

void GfxPaint16::redrawHiresCels() {
	for (HiresDrawData *i = _hiresDrawObjs; i; i = i->next)
		drawHiresCelAndShow(i->viewId, i->lpNo, i->celNo, i->leftPos, i->topPos, i->prio, i->palNo, i->handle, false);
}

void GfxPaint16::clearScreen(byte color) {
	fillRect(_ports->_curPort->rect, GFX_SCREEN_MASK_ALL, color, 0, 0);
}

void GfxPaint16::invertRect(const Common::Rect &rect) {
	int16 oldpenmode = _ports->_curPort->penMode;
	_ports->_curPort->penMode = 2;
	fillRect(rect, GFX_SCREEN_MASK_VISUAL, _ports->_curPort->penClr, _ports->_curPort->backClr);
	_ports->_curPort->penMode = oldpenmode;
}

// used in SCI0early exclusively
void GfxPaint16::invertRectViaXOR(const Common::Rect &rect) {
	Common::Rect r = rect;

	r.clip(_ports->_curPort->rect);
	if (r.isEmpty()) // nothing to invert
		return;

	_ports->offsetRect(r);
	for (int16 y = r.top; y < r.bottom; y++) {
		for (int16 x = r.left; x < r.right; x++) {
			byte curVisual = _screen->getVisual(x, y);
			_screen->putPixel(x, y, GFX_SCREEN_MASK_VISUAL, curVisual ^ 0x0f, 0, 0);
		}
	}
}

void GfxPaint16::eraseRect(const Common::Rect &rect) {
	fillRect(rect, GFX_SCREEN_MASK_VISUAL, _ports->_curPort->backClr);
}

void GfxPaint16::paintRect(const Common::Rect &rect) {
	fillRect(rect, GFX_SCREEN_MASK_VISUAL, _ports->_curPort->penClr);
}

void GfxPaint16::fillRect(const Common::Rect &rect, int16 drawFlags, byte color, byte priority, byte control) {
	Common::Rect r = rect;
	r.clip(_ports->_curPort->rect);
	if (r.isEmpty()) // nothing to fill
		return;

	int16 oldPenMode = _ports->_curPort->penMode;
	_ports->offsetRect(r);
	int16 x, y;

	// Doing visual first
	if (drawFlags & GFX_SCREEN_MASK_VISUAL) {
		if (oldPenMode == 2) { // invert mode
			for (y = r.top; y < r.bottom; y++) {
				for (x = r.left; x < r.right; x++) {
					byte curVisual = _screen->getVisual(x, y);
					if (curVisual == color) {
						_screen->putPixel(x, y, GFX_SCREEN_MASK_VISUAL, priority, 0, 0);
					} else if (curVisual == priority) {
						_screen->putPixel(x, y, GFX_SCREEN_MASK_VISUAL, color, 0, 0);
					}
				}
			}
		} else { // just fill rect with color
			for (y = r.top; y < r.bottom; y++) {
				for (x = r.left; x < r.right; x++) {
					_screen->putPixel(x, y, GFX_SCREEN_MASK_VISUAL, color, 0, 0);
				}
			}
		}
	}

	if (drawFlags < 2)
		return;
	drawFlags &= GFX_SCREEN_MASK_PRIORITY|GFX_SCREEN_MASK_CONTROL;

	// we need to isolate the bits, sierra sci saved priority and control inside one byte, we don't
	priority &= 0x0f;
	control &= 0x0f;

	if (oldPenMode != 2) {
		for (y = r.top; y < r.bottom; y++) {
			for (x = r.left; x < r.right; x++) {
				_screen->putPixel(x, y, drawFlags, 0, priority, control);
			}
		}
	} else {
		for (y = r.top; y < r.bottom; y++) {
			for (x = r.left; x < r.right; x++) {
				_screen->putPixel(x, y, drawFlags, 0, !_screen->getPriority(x, y), !_screen->getControl(x, y));
			}
		}
	}
}

void GfxPaint16::frameRect(const Common::Rect &rect) {
	Common::Rect r = rect;
	// left
	r.right = rect.left + 1;
	paintRect(r);
	// right
	r.right = rect.right;
	r.left = rect.right - 1;
	paintRect(r);
	//top
	r.left = rect.left;
	r.bottom = rect.top + 1;
	paintRect(r);
	//bottom
	r.bottom = rect.bottom;
	r.top = rect.bottom - 1;
	paintRect(r);
}

void GfxPaint16::bitsShow(const Common::Rect &rect) {
	Common::Rect workerRect(rect.left, rect.top, rect.right, rect.bottom);

	// WORKAROUND for vertically misplaced hires portraits in mixed speech+text mode in KQ6CD. The original interpreter
	// did not have that mode, so the devs had no need to fix it. These portraits get drawn above the viewport top, where
	// the engine would normally be unable to update the screen. We just have to offset the rect first, before clipping it,
	// to make it work. Another solution would be to improve the script patches that implement the speech/text mode for
	// better vertical placement of the portrait frames (inside the port rect).
	bool triggeredWorkaround = false;;
	if (rect.top < 0 && _hiresPortraitWorkaroundFlag) {
		_ports->offsetRect(workerRect);
		triggeredWorkaround = true;
		_hiresPortraitWorkaroundFlag = false;
	}

	workerRect.clip(_ports->_curPort->rect);
	if (workerRect.isEmpty()) // nothing to show
		return;

	// WORKAROUND, see comment above. Normally, the call to _ports->offsetRect(workerRect) would be unconditional here.
	if (!triggeredWorkaround)
		_ports->offsetRect(workerRect);

	// We adjust the left/right coordinates to even coordinates
	workerRect.left &= 0xFFFE; // round down
	workerRect.right = (workerRect.right + 1) & 0xFFFE; // round up

	_screen->copyRectToScreen(workerRect);
}
reg_t GfxPaint16::bitsSave(const Common::Rect &rect, byte screenMask, bool hiresFlag) {
	Common::Rect workerRect(rect.left, rect.top, rect.right, rect.bottom);
	if (!hiresFlag) { // KQ6CD Win only does this if not called from the special kGraph 15 case (= kGraphSaveUpscaledHiresBox)
		workerRect.clip(_ports->_curPort->rect);
		if (workerRect.isEmpty()) // nothing to save
			return NULL_REG;
		_ports->offsetRect(workerRect);
	}

	// now actually ask _screen how much space it will need for saving
	int size = _screen->bitsGetDataSize(workerRect, screenMask);

	reg_t memoryId = _segMan->allocateHunkEntry("SaveBits()", size);
	byte *memoryPtr = _segMan->getHunkPointer(memoryId);
	if (memoryPtr)
		_screen->bitsSave(workerRect, screenMask, memoryPtr);
	return memoryId;
}

void GfxPaint16::bitsGetRect(reg_t memoryHandle, Common::Rect *destRect) {
	if (!memoryHandle.isNull()) {
		byte *memoryPtr = _segMan->getHunkPointer(memoryHandle);

		if (memoryPtr) {
			_screen->bitsGetRect(memoryPtr, destRect);
		}
	}
}

void GfxPaint16::bitsRestore(reg_t memoryHandle) {
	if (!memoryHandle.isNull()) {
		byte *memoryPtr = _segMan->getHunkPointer(memoryHandle);

		if (memoryPtr) {
			_screen->bitsRestore(memoryPtr);
			bitsFree(memoryHandle);
		}

		// KQ6WinCD specific
		if (_screen->gfxDriver()->supportsHiResGraphics())
			removeHiresDrawObject(memoryHandle);
	}
}

void GfxPaint16::bitsFree(reg_t memoryHandle) {
	if (!memoryHandle.isNull())	// happens in KQ5CD
		_segMan->freeHunkEntry(memoryHandle);
}

void GfxPaint16::kernelDrawPicture(GuiResourceId pictureId, int16 animationNr, bool animationBlackoutFlag, bool mirroredFlag, bool addToFlag, int16 EGApaletteNo) {
	Port *oldPort = _ports->setPort((Port *)_ports->_picWind);

	if (_ports->isFrontWindow(_ports->_picWind)) {
		_screen->_picNotValid = 1;
		drawPicture(pictureId, mirroredFlag, addToFlag, EGApaletteNo);
		_transitions->setup(animationNr, animationBlackoutFlag);
	} else {
		// We need to set it for SCI1EARLY+ (sierra sci also did so), otherwise we get at least the following issues:
		//  LSL5 (english) - last wakeup (taj mahal flute dream)
		//  SQ5 (english v1.03) - during the scene following the scrubbing
		//   in both situations a window is shown when kDrawPic is called, which would result otherwise in
		//   no showpic getting called from kAnimate and we would get graphic corruption
		// XMAS1990 EGA did not set it in this case, VGA did
		if (getSciVersion() >= SCI_VERSION_1_EARLY)
			_screen->_picNotValid = 1;
		_ports->beginUpdate(_ports->_picWind);
		drawPicture(pictureId, mirroredFlag, addToFlag, EGApaletteNo);
		_ports->endUpdate(_ports->_picWind);
	}
	_ports->setPort(oldPort);
}

void GfxPaint16::kernelDrawCel(GuiResourceId viewId, int16 loopNo, int16 celNo, uint16 leftPos, uint16 topPos, int16 priority, uint16 paletteNo, uint16 scaleX, uint16 scaleY, bool hiresMode, reg_t hiresHandle) {
	// some calls are hiresMode even under kq6 DOS, that's why we check for hires caps here
	if (!hiresMode || !_screen->gfxDriver()->supportsHiResGraphics()) {
		drawCelAndShow(viewId, loopNo, celNo, leftPos, topPos, priority, paletteNo, scaleX, scaleY);
	} else {
		drawHiresCelAndShow(viewId, loopNo, celNo, leftPos, topPos, priority, paletteNo, hiresHandle, true);
	}
}

void GfxPaint16::kernelGraphFillBoxForeground(const Common::Rect &rect) {
	paintRect(rect);
}

void GfxPaint16::kernelGraphFillBoxBackground(const Common::Rect &rect) {
	eraseRect(rect);
}

void GfxPaint16::kernelGraphFillBox(const Common::Rect &rect, uint16 colorMask, int16 color, int16 priority, int16 control) {
	fillRect(rect, colorMask, color, priority, control);
}

void GfxPaint16::kernelGraphFrameBox(const Common::Rect &rect, int16 color) {
	int16 oldColor = _ports->getPort()->penClr;
	_ports->penColor(color);
	frameRect(rect);
	_ports->penColor(oldColor);
}

void GfxPaint16::kernelGraphDrawLine(Common::Point startPoint, Common::Point endPoint, int16 color, int16 priority, int16 control) {
	_ports->clipLine(startPoint, endPoint);
	_ports->offsetLine(startPoint, endPoint);
	_screen->drawLine(startPoint.x, startPoint.y, endPoint.x, endPoint.y, color, priority, control);
}

reg_t GfxPaint16::kernelGraphSaveBox(const Common::Rect &rect, uint16 screenMask, bool hiresFlag) {
	return bitsSave(rect, screenMask, hiresFlag);
}

void GfxPaint16::kernelGraphRestoreBox(reg_t handle) {
	bitsRestore(handle);
}

void GfxPaint16::kernelGraphUpdateBox(const Common::Rect &rect) {
	bitsShow(rect);
}

void GfxPaint16::kernelGraphRedrawBox(Common::Rect rect) {
	_coordAdjuster->kernelLocalToGlobal(rect.left, rect.top);
	_coordAdjuster->kernelLocalToGlobal(rect.right, rect.bottom);
	Port *oldPort = _ports->setPort((Port *)_ports->_picWind);
	_coordAdjuster->kernelGlobalToLocal(rect.left, rect.top);
	_coordAdjuster->kernelGlobalToLocal(rect.right, rect.bottom);

	_animate->reAnimate(rect);

	_ports->setPort(oldPort);
}

#define SCI_DISPLAY_MOVEPEN				100
#define SCI_DISPLAY_SETALIGNMENT		101
#define SCI_DISPLAY_SETPENCOLOR			102
#define SCI_DISPLAY_SETBACKGROUNDCOLOR	103
#define SCI_DISPLAY_SETGREYEDOUTPUT		104
#define SCI_DISPLAY_SETFONT				105
#define SCI_DISPLAY_WIDTH				106
#define SCI_DISPLAY_SAVEUNDER			107
#define SCI_DISPLAY_RESTOREUNDER		108
#define SCI_DISPLAY_DONTSHOWBITS		121
#define SCI_DISPLAY_SETSTROKE			122

reg_t GfxPaint16::kernelDisplay(const char *text, uint16 languageSplitter, int argc, reg_t *argv) {
	reg_t displayArg;
	TextAlignment alignment = SCI_TEXT16_ALIGNMENT_LEFT;
	int16 colorPen = -1, colorBack = -1, width = -1, bRedraw = 1;
	bool doSaveUnder = false;
	Common::Rect rect;
	reg_t result = NULL_REG;
	int16 stroke = 0; // Kawa's SCI11+

	// Make a "backup" of the port settings (required for some SCI0LATE and
	// SCI01+ only)
	Port oldPort = *_ports->getPort();

	// setting defaults
	_ports->penMode(0);
	_ports->penColor(0);
	_ports->textGreyedOutput(false);
	// processing codes in argv
	while (argc > 0) {
		displayArg = argv[0];
		if (displayArg.getSegment())
			displayArg.setOffset(0xFFFF);
		argc--; argv++;
		switch (displayArg.getOffset()) {
		case SCI_DISPLAY_MOVEPEN:
			_ports->moveTo(argv[0].toUint16(), argv[1].toUint16());
			argc -= 2; argv += 2;
			break;
		case SCI_DISPLAY_SETALIGNMENT:
			alignment = argv[0].toSint16();
			argc--; argv++;
			break;
		case SCI_DISPLAY_SETPENCOLOR:
			colorPen = argv[0].toUint16();
			_ports->penColor(colorPen);
			argc--; argv++;
			break;
		case SCI_DISPLAY_SETBACKGROUNDCOLOR:
			colorBack = argv[0].toUint16();
			argc--; argv++;
			break;
		case SCI_DISPLAY_SETGREYEDOUTPUT:
			_ports->textGreyedOutput(!argv[0].isNull());
			argc--; argv++;
			break;
		case SCI_DISPLAY_SETFONT:
			_text16->SetFont(argv[0].toUint16());
			argc--; argv++;
			break;
		case SCI_DISPLAY_WIDTH:
			width = argv[0].toUint16();
			argc--; argv++;
			break;
		case SCI_DISPLAY_SAVEUNDER:
			doSaveUnder = true;
			break;
		case SCI_DISPLAY_RESTOREUNDER:
			bitsGetRect(argv[0], &rect);
			rect.translate(-_ports->getPort()->left, -_ports->getPort()->top);
			bitsRestore(argv[0]);
			kernelGraphRedrawBox(rect);
			// finishing loop
			argc = 0;
			break;
		case SCI_DISPLAY_DONTSHOWBITS:
			bRedraw = 0;
			break;
		case SCI_DISPLAY_SETSTROKE: // From Kawa's SCI11+
			stroke = argv[0].toUint16();
			argc--; argv++;
			break;

		default:
			SciCallOrigin originReply;
			SciWorkaroundSolution solution = trackOriginAndFindWorkaround(0, kDisplay_workarounds, &originReply);
			if (solution.type == WORKAROUND_NONE)
				error("Unknown kDisplay argument (%04x:%04x)", PRINT_REG(displayArg));
			assert(solution.type == WORKAROUND_IGNORE);
			break;
		}
	}

	// now drawing the text
	_text16->Size(rect, text, languageSplitter, -1, width);
	rect.moveTo(_ports->getPort()->curLeft, _ports->getPort()->curTop);
	// Note: This code has been found in SCI1 middle and newer games. It was
	// previously only for SCI1 late and newer, but the LSL1 interpreter contains
	// this code.
	if (getSciVersion() >= SCI_VERSION_1_MIDDLE) {
		int16 leftPos = rect.right <= _screen->getWidth() ? 0 : _screen->getWidth() - rect.right;
		int16 topPos = rect.bottom <= _screen->getHeight() ? 0 : _screen->getHeight() - rect.bottom;
		_ports->move(leftPos, topPos);
		rect.moveTo(_ports->getPort()->curLeft, _ports->getPort()->curTop);
	}

	// Kawa's SCI11+
	if (stroke)
		rect.grow(1);

	if (doSaveUnder)
		result = bitsSave(rect, GFX_SCREEN_MASK_VISUAL);
	if (colorBack != -1)
		fillRect(rect, GFX_SCREEN_MASK_VISUAL, colorBack, 0, 0);

	// Kawa's SCI11+
	if (stroke)	{
		_ports->penColor(0);
		rect.translate(1, 0); if (stroke & 1) _text16->Box(text, languageSplitter, false, rect, alignment, -1); // right
		rect.translate(0, 1); if (stroke & 2) _text16->Box(text, languageSplitter, false, rect, alignment, -1); // bottom right
		rect.translate(-1, 0); if (stroke & 4) _text16->Box(text, languageSplitter, false, rect, alignment, -1); // bottom
		rect.translate(-1, 0); if (stroke & 8) _text16->Box(text, languageSplitter, false, rect, alignment, -1); // bottom left
		rect.translate(0, -1); if (stroke & 16) _text16->Box(text, languageSplitter, false, rect, alignment, -1); // left
		rect.translate(0, -1); if (stroke & 32) _text16->Box(text, languageSplitter, false, rect, alignment, -1); // top left
		rect.translate(1, 0); if (stroke & 64) _text16->Box(text, languageSplitter, false, rect, alignment, -1); // top
		rect.translate(1, 0); if (stroke & 128) _text16->Box(text, languageSplitter, false, rect, alignment, -1); // top right
		rect.translate(-1, 1); // and back to center
		_ports->penColor(colorPen);
	}

	// To make sure that the Korean hires font does not get overdrawn we update the display area before printing
	// the text. The PC-98 versions use a lowres font here, so this fix is only for the Korean fan translation.
	if (g_sci->getLanguage() == Common::KO_KOR && !_screen->_picNotValid && bRedraw)
		bitsShow(rect);

	_text16->Box(text, languageSplitter, g_sci->getLanguage() == Common::KO_KOR, rect, alignment, -1);

	// See comment above.
	if (g_sci->getLanguage() != Common::KO_KOR && _screen->_picNotValid == 0 && bRedraw)
		bitsShow(rect);

	// restoring port and cursor pos
	Port *currport = _ports->getPort();
	uint16 tTop = currport->curTop;
	uint16 tLeft = currport->curLeft;
	if (!g_sci->_features->usesOldGfxFunctions()) {
		// Restore port settings for some SCI0LATE and SCI01+ only.
		//
		// The change actually happened inbetween .530 (hoyle1) and .566 (heros
		// quest). We don't have any detection for that currently, so we are
		// using oldGfxFunctions (.502). The only games that could get
		// regressions because of this are hoyle1, kq4 and funseeker. If there
		// are regressions, we should use interpreter version (which would
		// require exe version detection).
		//
		// If we restore the port for whole SCI0LATE, at least sq3old will get
		// an issue - font 0 will get used when scanning for planets instead of
		// font 600 - a setfont parameter is missing in one of the kDisplay
		// calls in script 19. I assume this is a script bug, because it was
		// added in sq3new.
		*currport = oldPort;
	}
	currport->curTop = tTop;
	currport->curLeft = tLeft;
	return result;
}

reg_t GfxPaint16::kernelPortraitLoad(const Common::String &resourceName) {
	//Portrait *myPortrait = new Portrait(_resMan, _screen, _palette, resourceName);
	return NULL_REG;
}

void GfxPaint16::kernelPortraitShow(const Common::String &resourceName, Common::Point position, uint16 resourceId, uint16 noun, uint16 verb, uint16 cond, uint16 seq) {
	Portrait *myPortrait = new Portrait(_resMan, g_sci->getEventManager(), _screen, _palette, _audio, resourceName);
	// TODO: cache portraits
	// adjust given coordinates to curPort (but dont adjust coordinates on upscaledHires_Save_Box and give us hires coordinates
	//  on kDrawCel, yeah this whole stuff makes sense)
	position.x += _ports->getPort()->left; position.y += _ports->getPort()->top;
	myPortrait->doit(position, resourceId, noun, verb, cond, seq);
	delete myPortrait;
}

void GfxPaint16::kernelPortraitUnload(uint16 portraitId) {
}

void GfxPaint16::removeHiresDrawObject(reg_t handle) {
	for (HiresDrawData *i = _hiresDrawObjs; i; ) {
		HiresDrawData *next = i->next;
		if (i->handle != handle) {
			i = next;
			continue;
		}

		// WORKAROUND for vertically misplaced hires portraits in mixed speech+text mode in KQ6CD. If we have
		// an entry which is flagged as needing a workaround, we set the notification for bitsShow() here.
		if (i->waFlag)
			_hiresPortraitWorkaroundFlag = true;

		// Unlink and delete entry
		if (i->next)
			i->next->prev = i->prev;
		if (i->prev)
			i->prev->next = i->next;
		else
			_hiresDrawObjs = i->next;
		delete i;

		i = next;
	}
}

bool GfxPaint16::hasHiresDrawObjectAt(uint16 x, uint16 y) const {
	for (HiresDrawData *i = _hiresDrawObjs; i; i = i->next) {
		if (i->leftPos == x && i->topPos == y)
			return true;
	}
	return false;
}

Common::Rect GfxPaint16::makeHiresRect(Common::Rect &rect) const {
	Common::Point topLeft(rect.left, rect.top);
	Common::Point bottomRight(rect.right, rect.bottom);
	topLeft = _screen->gfxDriver()->getRealCoords(topLeft);
	bottomRight = _screen->gfxDriver()->getRealCoords(bottomRight);
	return Common::Rect(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y);
}

} // End of namespace Sci
