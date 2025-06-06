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


#include "scumm/scumm.h"
#include "scumm/actor.h"
#include "scumm/costume.h"
#include "scumm/sound.h"
#include "scumm/util.h"

namespace Scumm {

const byte smallCostumeScaleTable[256] = {
	0xFF, 0xFD, 0x7D, 0xBD, 0x3D, 0xDD, 0x5D, 0x9D,
	0x1D, 0xED, 0x6D, 0xAD, 0x2D, 0xCD, 0x4D, 0x8D,
	0x0D, 0xF5, 0x75, 0xB5, 0x35, 0xD5, 0x55, 0x95,
	0x15, 0xE5, 0x65, 0xA5, 0x25, 0xC5, 0x45, 0x85,
	0x05, 0xF9, 0x79, 0xB9, 0x39, 0xD9, 0x59, 0x99,
	0x19, 0xE9, 0x69, 0xA9, 0x29, 0xC9, 0x49, 0x89,
	0x09, 0xF1, 0x71, 0xB1, 0x31, 0xD1, 0x51, 0x91,
	0x11, 0xE1, 0x61, 0xA1, 0x21, 0xC1, 0x41, 0x81,
	0x01, 0xFB, 0x7B, 0xBB, 0x3B, 0xDB, 0x5B, 0x9B,
	0x1B, 0xEB, 0x6B, 0xAB, 0x2B, 0xCB, 0x4B, 0x8B,
	0x0B, 0xF3, 0x73, 0xB3, 0x33, 0xD3, 0x53, 0x93,
	0x13, 0xE3, 0x63, 0xA3, 0x23, 0xC3, 0x43, 0x83,
	0x03, 0xF7, 0x77, 0xB7, 0x37, 0xD7, 0x57, 0x97,
	0x17, 0xE7, 0x67, 0xA7, 0x27, 0xC7, 0x47, 0x87,
	0x07, 0xEF, 0x6F, 0xAF, 0x2F, 0xCF, 0x4F, 0x8F,
	0x0F, 0xDF, 0x5F, 0x9F, 0x1F, 0xBF, 0x3F, 0x7F,
	0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0,
	0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
	0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8,
	0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
	0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4,
	0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
	0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC,
	0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
	0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2,
	0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
	0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA,
	0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
	0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6,
	0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
	0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
	0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE
};

static const int v1MMNESLookup[25] = {
	0x00, 0x03, 0x01, 0x06, 0x08,
	0x02, 0x00, 0x07, 0x0C, 0x04,
	0x09, 0x0A, 0x12, 0x0B, 0x14,
	0x0D, 0x11, 0x0F, 0x0E, 0x10,
	0x17, 0x00, 0x01, 0x05, 0x16
};

byte ClassicCostumeRenderer::paintCelByleRLE(int xMoveCur, int yMoveCur) {
	int ex1, ex2;
	ByleRLEData compData;

	const bool c64Cost = _loaded._format == 0x57;
	const bool newAmiCost = (_vm->_game.version == 5) && (_vm->_game.platform == Common::kPlatformAmiga);
	const bool pcEngCost = (_vm->_game.id == GID_LOOM && _vm->_game.platform == Common::kPlatformPCEngine);

	switch (_loaded._format) {
	case 0x60:
	case 0x61:
		// This format is used e.g. in the Sam&Max intro
		ex1 = _srcPtr[0];
		ex2 = _srcPtr[1];
		_srcPtr += 2;
		if (ex1 != 0xFF || ex2 != 0xFF) {
			ex1 = READ_LE_UINT16(_loaded._frameOffsets + ex1 * 2);
			_srcPtr = _loaded._baseptr + READ_LE_UINT16(_loaded._baseptr + ex1 + ex2 * 2) + 14;
		}
		break;
	default:
		break;
	}

	compData.scaleTable = smallCostumeScaleTable;

	compData.x = _actorX;
	compData.y = _actorY;

	// V0/V1 games are off by 1
	if (_vm->_game.version <= 1)
		compData.y += 1;

	// It's possible that the scale indexes will overflow and wrap
	// around to zero, so it's important that we use the same
	// method of accessing it both when calculating the size of the
	// scaled costume, and when drawing it. See bug #2729.
	compData.scaleIndexMask = 0xff;

	bool decode = true;

	byte drawFlag = paintCelByleRLECommon(
		xMoveCur,
		yMoveCur,
		_loaded._numColors,
		128,
		newAmiCost || pcEngCost,
		c64Cost,
		compData,
		decode);

	if (!decode)
		return drawFlag;

	compData.maskPtr = _vm->getMaskBuffer(0, compData.y, _zbuf);

	// temporarily replace _shadowMode to distinguish from AkosRenderer's _shadowMode
	byte oldShadowMode = _shadowMode;
	if (!(_shadowMode & 0x20)) {
		_shadowMode = 0xff;
	}

	if (c64Cost) {
		// The v1 costume renderer needs the actor number, which is
		// the same thing as the costume renderer's _actorID.
		byleRLEDecode_C64(compData, _actorID);
	} else if (newAmiCost)
		byleRLEDecode_ami(compData);
	else if (pcEngCost)
		byleRLEDecode_PCEngine(compData);
	else
		byleRLEDecode(compData);

	_shadowMode = oldShadowMode;

	return drawFlag;
}

void ClassicCostumeRenderer::markAsDirty(const Common::Rect &rect, ByleRLEData &compData, bool &decode) {
	if (_vm->_game.version == 1) {
		// V1 games uses 8 x 8 pixels for actors
		_vm->markRectAsDirty(kMainVirtScreen, rect.left, rect.right + 8, rect.top, rect.bottom, _actorID);
	} else {
		_vm->markRectAsDirty(kMainVirtScreen, rect.left, rect.right + 1, rect.top, rect.bottom, _actorID);
	}
}

// Skin colors
static const int v1MMActorPalatte1[25] = {
	8, 8, 8, 8, 4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
};
// Suit colors
static const int v1MMActorPalatte2[25] = {
	0, 7, 2, 6, 9, 1, 3, 7, 7, 1, 1, 9, 1, 4, 5, 5, 4, 1, 0, 5, 4, 2, 2, 7, 7
};

#define MASK_AT(xoff) \
	(mask && (mask[((compData.x + xoff) / 8)] & revBitMask((compData.x + xoff) & 7)))
#define LINE(c,p) \
	pcolor = (color >> c) & 3; \
	if (pcolor) { \
		if (!MASK_AT(p)) \
			dst[p] = palette[pcolor]; \
		if (!MASK_AT(p + 1)) \
			dst[p + 1] = palette[pcolor]; \
	}

void ClassicCostumeRenderer::byleRLEDecode_C64(ByleRLEData &compData, int actor) {
	const byte *mask, *src;
	byte *dst;
	byte len;
	int y;
	uint height;
	byte color, pcolor;
	bool rep;

	y = compData.y;
	src = _srcPtr;
	dst = compData.destPtr;
	len = compData.repLen;
	color = compData.repColor;
	height = _height;

	compData.skipWidth /= 8;

	// Set up the palette data
	byte palette[4] = { 0, 0, 0, 0 };
	if (_vm->getCurrentLights() & LIGHTMODE_actor_use_colors) {
		if (_vm->_game.id == GID_MANIAC) {
			palette[1] = _vm->_gdi->remapColorToRenderMode(v1MMActorPalatte1[actor]);
			palette[2] = _vm->_gdi->remapColorToRenderMode(v1MMActorPalatte2[actor]);
		} else {
			// Adjust for C64 version of Zak McKracken
			palette[1] = _vm->_gdi->remapColorToRenderMode(_vm->_game.platform == Common::kPlatformC64 ? 10 : 8);
			palette[2] = _vm->_gdi->remapColorToRenderMode(_palette[actor]);
		}
	} else {
		palette[2] = _vm->_gdi->remapColorToRenderMode(11);
		palette[3] = _vm->_gdi->remapColorToRenderMode(11);
	}
	mask = compData.maskPtr;

	if (len)
		goto StartPos;

	do {
		len = *src++;
		if (len & 0x80)
			color = *src++;
	StartPos:;
		rep = (len & 0x80) != 0;
		len &= 0x7f;
		while (len--) {
			if (!rep)
				color = *src++;

			if (0 <= y && y < _out.h && 0 <= compData.x && compData.x < _out.w) {
				if (!_mirror) {
					LINE(0, 0); LINE(2, 2); LINE(4, 4); LINE(6, 6);
				} else {
					LINE(6, 0); LINE(4, 2); LINE(2, 4); LINE(0, 6);
				}
			}
			dst += _out.pitch;
			y++;
			mask += _numStrips;
			if (!--height) {
				if (!--compData.skipWidth)
					return;
				height = _height;
				y = compData.y;
				compData.x += 8 * compData.scaleXStep;
				if (compData.x < 0 || compData.x >= _out.w)
					return;
				mask = compData.maskPtr;
				compData.destPtr += 8 * compData.scaleXStep;
				dst = compData.destPtr;
			}
		}
	} while (1);
}

#undef LINE
#undef MASK_AT

void ClassicCostumeRenderer::byleRLEDecode_ami(ByleRLEData &compData) {
	const byte *mask, *src;
	byte *dst;
	byte maskbit, len, height, width;
	int color;
	int y;
	bool masked;
	int oldXpos, oldScaleIndexX;

	mask = compData.maskPtr + compData.x / 8;
	dst = compData.destPtr;
	height = _height;
	width = _width;
	src = _srcPtr;
	maskbit = revBitMask(compData.x & 7);
	y = compData.y;
	oldXpos = compData.x;
	oldScaleIndexX = compData.scaleXIndex;

	// Indy4 Amiga always uses the room map to match colors to the currently
	// setup palette in the actor code in the original, thus we need to do this
	// mapping over here too.
	byte *amigaMap = nullptr;
	if (_vm->_game.platform == Common::kPlatformAmiga && _vm->_game.id == GID_INDY4)
		amigaMap = _vm->_roomPalette;

	do {
		len = *src++;
		color = len >> compData.shr;
		len &= compData.mask;
		if (!len)
			len = *src++;
		do {
			if (_scaleY == 255 || compData.scaleTable[compData.scaleYIndex] < _scaleY) {
				masked = (y < 0 || y >= _out.h) || (compData.x < 0 || compData.x >= _out.w) || (compData.maskPtr && (mask[0] & maskbit));

				if (color && !masked) {
					if (amigaMap)
						*dst = amigaMap[_palette[color]];
					else
						*dst = _palette[color];
				}

				if (_scaleX == 255 || compData.scaleTable[compData.scaleXIndex] < _scaleX) {
					compData.x += compData.scaleXStep;
					dst += compData.scaleXStep;
					maskbit = revBitMask(compData.x & 7);
				}
				compData.scaleXIndex = (compData.scaleXIndex + compData.scaleXStep) & compData.scaleIndexMask;
				mask = compData.maskPtr + compData.x / 8;
			}
			if (!--width) {
				if (!--height)
					return;

				if (y >= _out.h)
					return;

				if (compData.x != oldXpos) {
					dst += _out.pitch - (compData.x - oldXpos);
					compData.maskPtr += _numStrips;
					mask = compData.maskPtr + oldXpos / 8;
					maskbit = revBitMask(oldXpos & 7);
					y++;
				}
				width = _width;
				compData.x = oldXpos;
				compData.scaleXIndex = oldScaleIndexX;
				compData.scaleYIndex = (compData.scaleYIndex + 1) & compData.scaleIndexMask;
			}
		} while (--len);
	} while (1);
}

static void PCESetCostumeData(byte block[16][16], int index, byte value) {
	int row = (index % 16);
	int plane = (index / 16) % 4;
	int colOffset = (index < 64) ? 8 : 0;
	for (int i = 0; i < 8; ++i) {
		int bit = (value >> (7-i)) & 0x1;
		block[row][i + colOffset] |= bit << plane;
	}
}

void ClassicCostumeRenderer::byleRLEDecode_PCEngine(ByleRLEData &compData) {
	const byte *mask, *src;
	byte *dst;
	byte maskbit;
	int xPos, yPos;
	uint pcolor, width, height;
	bool masked;
	int vertShift;
	int xStep;
	byte block[16][16];

	src = _srcPtr;
	width = _width / 16;
	height = _height / 16;

	if (_numBlocks == 0)
		return;

	xStep = _mirror ? +1 : -1;

	for (uint x = 0; x < width; ++x) {
		yPos = 0;
		for (uint y = 0; y < height; ++y) {
			vertShift = *src++;
			if (vertShift == 0xFF) {
				yPos += 16;
				continue;
			} else {
				yPos += vertShift;
			}

			memset(block, 0, sizeof(block));

			int index = 0;
			while (index < 128) {
				byte cmd = *src++;
				int cnt = (cmd & 0x3F) + 1;
				if (!(cmd & 0xC0)) {
					for (int i = 0; i < cnt; ++i)
						PCESetCostumeData(block, index++, 0);
				} else if (cmd & 0x80) {
					int value = *src++;
					for (int i = 0; i < cnt; ++i)
						PCESetCostumeData(block, index++, value);
				} else {
					for (int i = 0; i < cnt; ++i)
						PCESetCostumeData(block, index++, *src++);
				}
			}
			if (index != 128) {
				warning("ClassicCostumeRenderer::procPCEngine: index %d != 128\n", index);
			}

			for (int row = 0; row < 16; ++row) {
				xPos = xStep * x * 16;
				for (int col = 0; col < 16; ++col) {
					dst = compData.destPtr + yPos * _out.pitch + xPos * _vm->_bytesPerPixel;
					mask = compData.maskPtr + yPos * _numStrips + (compData.x + xPos) / 8;
					maskbit = revBitMask((compData.x + xPos) % 8);

					pcolor = block[row][col];
					masked = (compData.y + yPos < 0 || compData.y + yPos >= _out.h) ||
							 (compData.x + xPos < 0 || compData.x + xPos >= _out.w) ||
							 (compData.maskPtr && (mask[0] & maskbit));

					if (pcolor && !masked) {
						WRITE_UINT16(dst, ((uint16 *)_palette)[pcolor]);
					}

					xPos += xStep;
				}
				yPos++;
			}
		}
	}
}

static const byte amigaMonkey2Costume55Room53[16] = {
	0xfa, 0xca, 0xc2, 0xc0, 0xc1, 0xc3, 0xc4, 0xc5,
	0xc6, 0xc7, 0xc8, 0xce, 0xcf, 0xcd, 0xc9, 0xcc
};

void ClassicCostumeLoader::loadCostume(int id) {
	_id = id;
	byte *ptr = _vm->getResourceAddress(rtCostume, id);

	if (_vm->_game.version >= 6)
		ptr += 8;
	else if (_vm->_game.features & GF_OLD_BUNDLE)
		ptr += -2;
	else if (_vm->_game.features & GF_SMALL_HEADER)
		ptr += 0;
	else
		ptr += 2;

	_baseptr = ptr;

	_numAnim = ptr[6];
	_format = ptr[7] & 0x7F;
	_mirror = (ptr[7] & 0x80) != 0;
	_palette = ptr + 8;

	if (_vm->_game.id == GID_LOOM && _vm->_game.platform == Common::kPlatformPCEngine) {
		_numColors = 16;

		ptr += 8 + 17;
		_animCmds = READ_LE_UINT16(ptr) + ptr + 2;
		_frameOffsets = ptr + 2;
		_dataOffsets = ptr + 34;
		return;
	}

	switch (_format) {
	case 0x57:				// Only used in V1 games
		_numColors = 0;
		break;
	case 0x58:
		_numColors = 16;
		break;
	case 0x59:
		_numColors = 32;
		break;
	case 0x60:				// New since version 6
		_numColors = 16;
		break;
	case 0x61:				// New since version 6
		_numColors = 32;
		break;
	default:
		error("Costume %d with format 0x%X is invalid", id, _format);
	}

	// In GF_OLD_BUNDLE games, there is no actual palette, just a single color byte.
	// Don't forget, these games were designed around a fixed 16 color HW palette :-)
	// In addition, all offsets are shifted by 2; we accommodate that via a separate
	// _baseptr value (instead of adding tons of if's throughout the code).
	if (_vm->_game.features & GF_OLD_BUNDLE) {
		_numColors = (_format == 0x57) ? 0 : 1;
		_baseptr += 2;
	}
	ptr += 8 + _numColors;
	_frameOffsets = ptr + 2;
	if (_format == 0x57) {
		_dataOffsets = ptr + 18;
		_baseptr += 4;
	} else {
		_dataOffsets = ptr + 34;
	}
	_animCmds = _baseptr + READ_LE_UINT16(ptr);

	// WORKAROUND bug #13433: Guybrush can give the stick to two dogs: the one
	// guarding the jail, and the one in front of the mansion. But the palette
	// for this costume is invalid in the second case on Amiga, causing a glitch.
	if (_vm->_game.id == GID_MONKEY2 && _vm->_game.platform == Common::kPlatformAmiga && _vm->_currentRoom == 53 && id == 55 && _numColors == 16 && _vm->enhancementEnabled(kEnhMinorBugFixes)) {
		// Note: handmade, trying to match the colors between rooms 53 and 29,
		// and based on (similar) costume 1.
		_palette = amigaMonkey2Costume55Room53;
	}
}

byte NESCostumeRenderer::drawLimb(const Actor *a, int limb) {
	const byte darkpalette[16] = { 0x2d,0x1d,0x2d,0x3d, 0x2d,0x1d,0x2d,0x3d, 0x2d,0x1d,0x2d,0x3d, 0x2d,0x1d,0x2d,0x3d };
	const CostumeData &cost = a->_cost;
	const byte *palette, *src, *sprdata;
	int anim, frameNum, frame, offset, numSprites;

	// If the specified limb is stopped or not existing, do nothing.
	if (cost.curpos[limb] == 0xFFFF)
		return 0;

	if (_vm->getCurrentLights() & LIGHTMODE_actor_use_base_palette)
		palette = _vm->_NESPalette[1];
	else
		palette = darkpalette;

	src = _loaded._dataOffsets;
	anim = cost.frame[limb];
	frameNum = cost.curpos[limb];
	frame = src[src[2 * anim] + frameNum];

	offset = READ_LE_UINT16(_vm->_NEScostdesc + v1MMNESLookup[_loaded._id] * 2);
	numSprites = _vm->_NEScostlens[offset + frame] + 1;
	sprdata = _vm->_NEScostdata + READ_LE_UINT16(_vm->_NEScostoffs + 2 * (offset + frame)) + numSprites * 3;

	bool flipped = (newDirToOldDir(a->getFacing()) == 1);
	int left = 239, right = 0, top = 239, bottom = 0;
	byte *maskBuf = _vm->getMaskBuffer(0, 0, 1);

	for (int spr = 0; spr < numSprites; spr++) {
		byte mask, tile, sprpal;
		int8 y, x;

		sprdata -= 3;

		mask = (sprdata[0] & 0x80) ? 0x01 : 0x80;
		y = sprdata[0] << 1;
		y >>= 1;

		tile = sprdata[1];

		sprpal = (sprdata[2] & 0x03) << 2;
		x = sprdata[2];
		x >>= 2;

		if (flipped) {
			mask = (mask == 0x80) ? 0x01 : 0x80;
			x = -x;
		}

		left = MIN(left, _actorX + x);
		right = MAX(right, _actorX + x + 8);
		top = MIN(top, _actorY + y);
		bottom = MAX(bottom, _actorY + y + 8);

		if ((_actorX + x < 0) || (_actorX + x + 8 >= _out.w))
			continue;
		if ((_actorY + y < 0) || (_actorY + y + 8 >= _out.h))
			continue;

		for (int ty = 0; ty < 8; ty++) {
			byte c1 = _vm->_NESPatTable[0][tile * 16 + ty];
			byte c2 = _vm->_NESPatTable[0][tile * 16 + ty + 8];

			for (int tx = 0; tx < 8; tx++) {
				unsigned char c = ((c1 & mask) ? 1 : 0) | ((c2 & mask) ? 2 : 0) | sprpal;
				if (mask == 0x01) {
					c1 >>= 1;
					c2 >>= 1;
				} else {
					c1 <<= 1;
					c2 <<= 1;
				}
				if (!(c & 3))
					continue;
				int my = _actorY + y + ty;
				int mx = _actorX + x + tx;
				if (!(_zbuf && (maskBuf[my * _numStrips + mx / 8] & revBitMask(mx & 7))))
					*((byte *)_out.getBasePtr(mx, my)) = palette[c];
			}
		}
	}

	_drawTop = top;
	_drawBottom = bottom;

	_vm->markRectAsDirty(kMainVirtScreen, left, right, top, bottom, _actorID);

	return 0;
}

#define PCE_SIGNED(a) (((a) & 0x80) ? -((a) & 0x7F) : (a))

byte ClassicCostumeRenderer::drawLimb(const Actor *a, int limb) {
	int i;
	int code;
	const byte *baseptr, *frameptr;
	const CostumeData &cost = a->_cost;

	// If the specified limb is stopped or not existing, do nothing.
	if (cost.curpos[limb] == 0xFFFF || cost.stopped & (1 << limb))
		return 0;

	// Determine the position the limb is at
	i = cost.curpos[limb] & 0x7FFF;

	baseptr = _loaded._baseptr;

	// Get the frame pointer for that limb
	if (_vm->_game.id == GID_LOOM && _vm->_game.platform == Common::kPlatformPCEngine)
		baseptr = _loaded._frameOffsets + limb * 2 + 2;
	frameptr = baseptr + READ_LE_UINT16(_loaded._frameOffsets + limb * 2);

	// Determine the offset to the costume data for the limb at position i
	code = _loaded._animCmds[i] & 0x7F;

	// Code 0x7B indicates a limb for which there is nothing to draw
	if (code != 0x7B) {
		if (_vm->_game.id == GID_LOOM && _vm->_game.platform == Common::kPlatformPCEngine)
			baseptr = frameptr + code * 2 + 2;
		_srcPtr = baseptr + READ_LE_UINT16(frameptr + code * 2);

		if (!(_vm->_game.features & GF_OLD256) || code < 0x79) {
			const CostumeInfo *costumeInfo;
			int xmoveCur, ymoveCur;

			if (_vm->_game.id == GID_LOOM && _vm->_game.platform == Common::kPlatformPCEngine) {
				_numBlocks = _srcPtr[0];
				_width = _srcPtr[1] * 16;
				_height = _srcPtr[2] * 16;
				xmoveCur = _xMove + PCE_SIGNED(_srcPtr[3]);
				ymoveCur = _yMove + PCE_SIGNED(_srcPtr[4]);
				_xMove += PCE_SIGNED(_srcPtr[5]);
				_yMove -= PCE_SIGNED(_srcPtr[6]);
				_srcPtr += 7;
			} else if (_loaded._format == 0x57) {
				_width = _srcPtr[0] * 8;
				_height = _srcPtr[1];
				xmoveCur = _xMove + (int8)_srcPtr[2] * 8;
				ymoveCur = _yMove - (int8)_srcPtr[3];
				_xMove += (int8)_srcPtr[4] * 8;
				_yMove -= (int8)_srcPtr[5];
				_srcPtr += 6;
			} else {
				costumeInfo = (const CostumeInfo *)_srcPtr;
				_width = READ_LE_UINT16(&costumeInfo->width);
				_height = READ_LE_UINT16(&costumeInfo->height);
				xmoveCur = _xMove + (int16)READ_LE_UINT16(&costumeInfo->relX);
				ymoveCur = _yMove + (int16)READ_LE_UINT16(&costumeInfo->relY);
				_xMove += (int16)READ_LE_UINT16(&costumeInfo->moveX);
				_yMove -= (int16)READ_LE_UINT16(&costumeInfo->moveY);
				_srcPtr += 12;
			}

			// WORKAROUND: During the intro, there are a couple of
			// glitches when Bernard looks out of his Chron-O-John.
			// The actor has two limbs: The lid and the face. The
			// lid is slightly misaligned, and when Bernard faces to
			// the left both the lid and the face are mirrored when
			// only his face should be.
			//
			// We adjust the positioning a bit, and make sure the
			// lid is always mirrored the same way.

			bool mirror = _mirror;

			if (_vm->_game.id == GID_TENTACLE && _vm->_currentRoom == 61 && a->_number == 1 && _loaded._id == 324 && _vm->enhancementEnabled(kEnhMinorBugFixes)) {
				if (limb == 0) {
					_mirror = true;
					xmoveCur--;
				} else {
					if (a->getFacing() == 270) {
						xmoveCur += 4;
					} else {
						xmoveCur--;
					}
				}
			}

			byte result = paintCelByleRLE(xmoveCur, ymoveCur);

			_mirror = mirror;
			return result;
		}
	}

	return 0;

}

void NESCostumeRenderer::setPalette(uint16 *palette) {
	// TODO
}

void NESCostumeRenderer::setFacing(const Actor *a) {
	// TODO
	//_mirror = newDirToOldDir(a->getFacing()) != 0 || _loaded._mirror;
}

void NESCostumeRenderer::setCostume(int costume, int shadow) {
	_loaded.loadCostume(costume);
}

#ifdef USE_RGB_COLOR
void PCEngineCostumeRenderer::setPalette(uint16 *palette) {
	const byte* ptr = _loaded._palette;
	byte rgb[45];
	byte *rgbPtr = rgb;
	_vm->readPCEPalette(&ptr, &rgbPtr, 15);

	// Normally, palette is filled with 0xFF and can be ignored. But this
	// can be changed through scripting, and when entering the darkened
	// tent in Loom certain colors are set to 0. These should be black.
	//
	// There are several other cases where other colors are set, but in my
	// playthrough I couldn't see any case where honoring these would make
	// any improvements. Quite the contrary in fact, since it would
	// introduce numerous glitches comparing to what I've seen of the
	// original behavior.

	_palette[0] = 0;
	for (int i = 0; i < 15; ++i) {
		int c = (palette[i + 1] != 0) ? i : 0;
		_palette[i + 1] = _vm->get16BitColor(rgb[c * 3 + 0], rgb[c * 3 + 1], rgb[c * 3 + 2]);
	}
}
#endif

void ClassicCostumeLoader::costumeDecodeData(Actor *a, int frame, uint usemask) {
	const byte *r;
	const byte *baseptr;
	uint mask, j;
	int i;
	byte extra, cmd;
	int anim;

	loadCostume(a->_costume);

	anim = newDirToOldDir(a->getFacing()) + frame * 4;
	if (anim > _numAnim)
		return;

	if (_vm->_game.id == GID_LOOM && _vm->_game.platform == Common::kPlatformPCEngine)
		baseptr = _dataOffsets + anim * 2 + 2;
	else
		baseptr = _baseptr;

	r = baseptr + READ_LE_UINT16(_dataOffsets + anim * 2);
	if (r == baseptr) {
		return;
	}

	if (_vm->_game.version == 1) {
		mask = *r++ << 8;
	} else {
		mask = READ_LE_UINT16(r);
		r += 2;
	}
	i = 0;
	do {
		if (mask & 0x8000) {
			if ((_vm->_game.version <= 3) &&
				!(_vm->_game.id == GID_LOOM && _vm->_game.platform == Common::kPlatformPCEngine))
			{
				j = *r++;

				if (j == 0xFF)
					j = 0xFFFF;
			} else {
				j = READ_LE_UINT16(r);
				r += 2;
			}
			if (usemask & 0x8000) {
				if (j == 0xFFFF) {
					a->_cost.curpos[i] = 0xFFFF;
					a->_cost.start[i] = 0;
					a->_cost.frame[i] = anim;
				} else {
					extra = *r++;
					cmd = _animCmds[j];
					if (cmd == 0x7A) {
						a->_cost.stopped &= ~(1 << i);
					} else if (cmd == 0x79) {
						a->_cost.stopped |= (1 << i);
					} else {
						a->_cost.curpos[i] = a->_cost.start[i] = j;
						a->_cost.end[i] = j + (extra & 0x7F);
						if (extra & 0x80)
							a->_cost.curpos[i] |= 0x8000;
						a->_cost.frame[i] = anim;
					}
				}
			} else {
				if (j != 0xFFFF)
					r++;
			}
		}
		i++;
		usemask <<= 1;
		mask <<= 1;
	} while (mask & 0xFFFF);
}

void ClassicCostumeRenderer::setPalette(uint16 *palette) {
	int i;
	byte color;

	if (_loaded._format == 0x57) {
		for (i = 0; i < 13; i++)
			_palette[i] = palette[i];
	} else if (_vm->_game.features & GF_OLD_BUNDLE) {
		if (_vm->getCurrentLights() & LIGHTMODE_actor_use_colors) {
			for (i = 0; i < 16; i++)
				_palette[i] = palette[i];
			if (_vm->_game.version == 2)
				_palette[12] = (_vm->_renderMode == Common::kRenderCGA || _vm->_renderMode == Common::kRenderHercA || _vm->_renderMode == Common::kRenderHercG) ? 0x0F : 0x0C;
		} else {
			for (i = 0; i < 16; i++)
				_palette[i] = 8;
			_palette[12] = 0;
		}
		_palette[_loaded._palette[0]] = _palette[0];
	} else {
		if (_vm->getCurrentLights() & LIGHTMODE_actor_use_colors) {
			for (i = 0; i < _loaded._numColors; i++) {
				color = palette[i];
				if (color == 255)
					color = _loaded._palette[i];
				_palette[i] = color;
			}
		} else {
			memset(_palette, 8, _loaded._numColors);
			_palette[12] = 0;
		}
	}
}

void ClassicCostumeRenderer::setFacing(const Actor *a) {
	_mirror = newDirToOldDir(a->getFacing()) != 0 || _loaded._mirror;
}

void ClassicCostumeRenderer::setCostume(int costume, int shadow) {
	_loaded.loadCostume(costume);
}

bool ClassicCostumeLoader::increaseAnims(Actor *a) {
	bool r = false;

	for (int i = 0; i != 16; i++) {
		if (a->_cost.curpos[i] != 0xFFFF)
			r |= increaseAnim(a, i);
	}
	return r;
}

bool ClassicCostumeLoader::increaseAnim(Actor *a, int slot) {
	int highflag;
	int i, end;
	byte code, nc;

	if (a->_cost.curpos[slot] == 0xFFFF)
		return false;

	highflag = a->_cost.curpos[slot] & 0x8000;
	i = a->_cost.curpos[slot] & 0x7FFF;
	end = a->_cost.end[slot];
	code = _animCmds[i] & 0x7F;

	if (_vm->_game.version <= 3) {
		if (_animCmds[i] & 0x80)
			a->_cost.soundCounter++;
	}

	do {
		if (!highflag) {
			if (i++ >= end)
				i = a->_cost.start[slot];
		} else {
			if (i != end)
				i++;
		}
		nc = _animCmds[i];

		if (nc == 0x7C) {
			a->_cost.animCounter++;
			if (a->_cost.start[slot] != end)
				continue;
		} else {
			if (_vm->_game.version >= 6) {
				if (nc >= 0x71 && nc <= 0x78) {
					uint sound = (_vm->_game.heversion == 60) ? 0x78 - nc : nc - 0x71;
					_vm->_sound->addSoundToQueue(a->_sound[sound]);
					if (a->_cost.start[slot] != end)
						continue;
				}
			} else {
				if (nc == 0x78) {
					a->_cost.soundCounter++;
					if (a->_cost.start[slot] != end)
						continue;
				}
			}
		}

		a->_cost.curpos[slot] = i | highflag;
		return (_animCmds[i] & 0x7F) != code;
	} while (true);
}

/**
 * costume ID -> v1MMNESLookup[] -> desc -> lens & offs -> data -> Gfx & pal
 */
void NESCostumeLoader::loadCostume(int id) {
	_id = id;
	_baseptr = _vm->getResourceAddress(rtCostume, id);
	_dataOffsets = _baseptr + 2;
	_numAnim = 0x17;
}

void NESCostumeLoader::costumeDecodeData(Actor *a, int frame, uint usemask) {
	int anim;

	loadCostume(a->_costume);

	anim = 4 * frame + newDirToOldDir(a->getFacing());

	if (anim > _numAnim) {
		return;
	}

	a->_cost.curpos[0] = 0;
	a->_cost.start[0] = 0;
	a->_cost.end[0] = _dataOffsets[2 * anim + 1];
	a->_cost.frame[0] = anim;
}

bool NESCostumeLoader::increaseAnims(Actor *a) {
	bool r = false;

	for (int i = 0; i != 16; i++) {
		if (a->_cost.curpos[i] != 0xFFFF)
			r |= increaseAnim(a, i);
	}
	return r;
}

bool NESCostumeLoader::increaseAnim(Actor *a, int slot) {
	int oldframe = a->_cost.curpos[slot]++;
	if (a->_cost.curpos[slot] >= a->_cost.end[slot])
		a->_cost.curpos[slot] = a->_cost.start[slot];
	return (a->_cost.curpos[slot] != oldframe);
}

static const byte actorV0Colors[25] = {
	0, 7, 2, 6, 9, 1, 3, 7, 7, 1, 1, 9, 1, 4, 5, 5, 4, 1, 0, 5, 4, 2, 2, 7, 7
};

#define MASK_AT(xoff) \
	(mask && (mask[((destX + xoff) / 8)] & revBitMask((destX + xoff) & 7)))
#define LINE(c,p) \
	pcolor = (color >> c) & 3; \
	if (pcolor) { \
		if (!MASK_AT(p)) \
			dst[p] = palette[pcolor]; \
		if (!MASK_AT(p + 1)) \
			dst[p + 1] = palette[pcolor]; \
	}

byte V0CostumeRenderer::drawLimb(const Actor *a, int limb) {
	const Actor_v0* a0 = (const Actor_v0 *)a;

	if (limb >= 8)
		return 0;

	if (limb == 0) {
		_drawTop = 200;
		_drawBottom = 0;
	}

	// Invalid current position?
	if (a->_cost.curpos[limb] == 0xFFFF)
		return 0;

	_loaded.loadCostume(a->_costume);
	byte frame = _loaded._frameOffsets[a->_cost.curpos[limb] + a->_cost.animType[limb]];

	// Get the frame ptr
	byte ptrLow = _loaded._baseptr[frame];
	byte ptrHigh = ptrLow + _loaded._dataOffsets[4];
	int frameOffset = (_loaded._baseptr[ptrHigh] << 8) + _loaded._baseptr[ptrLow + 2];			// 0x23EF / 0x2400

	const byte *data = _loaded._baseptr + frameOffset;

	// Set up the palette data
	byte palette[4] = { 0, 0, 0, 0 };
	if (_vm->getCurrentLights() & LIGHTMODE_actor_use_colors) {
		palette[1] = 10;
		palette[2] = actorV0Colors[_actorID];
	} else {
		palette[2] = (_vm->getCurrentLights() & LIGHTMODE_flashlight_on) ?  actorV0Colors[_actorID] : 11;
		palette[3] = 11;
	}

	int width = data[0];
	int height = data[1];
	int offsetX = _xMove + data[2];
	int offsetY = _yMove + data[3];
	_xMove += (int8)data[4];
	_yMove += (int8)data[5];
	data += 6;

	if (!width || !height)
		return 0;

	int xpos = _actorX + (a0->_limb_flipped[limb] ? -1 : +1) * (offsetX * 8 - a->_width / 2);
	// +1 as we appear to be 1 pixel away from the original interpreter
	int ypos = _actorY - offsetY + 1;

	// This code is very similar to procC64()
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			byte color = data[y * width + x];
			byte pcolor;

			int destX = xpos + (a0->_limb_flipped[limb] ? -(x + 1) : x) * 8;
			int destY = ypos + y;

			if (destY >= 0 && destY < _out.h && destX >= 0 && destX < _out.w) {
				byte *dst = (byte *)_out.getBasePtr(destX, destY);
				byte *mask = _vm->getMaskBuffer(0, destY, _zbuf);
				if (a0->_limb_flipped[limb]) {
					LINE(0, 0); LINE(2, 2); LINE(4, 4); LINE(6, 6);
				} else {
					LINE(6, 0); LINE(4, 2); LINE(2, 4); LINE(0, 6);
				}
			}
		}
	}

	_drawTop = MIN(_drawTop, ypos);
	_drawBottom = MAX(_drawBottom, ypos + height);
	if (a0->_limb_flipped[limb])
		_vm->markRectAsDirty(kMainVirtScreen, xpos - (width * 8), xpos, ypos, ypos + height, _actorID);
	else
		_vm->markRectAsDirty(kMainVirtScreen, xpos, xpos + (width * 8), ypos, ypos + height, _actorID);
	return 0;
}

#undef LINE
#undef MASK_AT

void V0CostumeRenderer::setCostume(int costume, int shadow) {
	_loaded.loadCostume(costume);
}

void V0CostumeLoader::loadCostume(int id) {
	const byte *ptr = _vm->getResourceAddress(rtCostume, id);

	_id = id;
	_baseptr = ptr + 9;

	_format = 0x57;
	_numColors = 0;
	_numAnim = 0;
	_mirror = 0;
	_palette = &actorV0Colors[id];

	_frameOffsets = _baseptr + READ_LE_UINT16(ptr + 5);
	_dataOffsets = ptr;
	_animCmds = _baseptr + READ_LE_UINT16(ptr + 7);
}

void V0CostumeLoader::costumeDecodeData(Actor *a, int frame, uint usemask) {
	Actor_v0 *a0 = (Actor_v0 *)a;

	if (!a->_costume)
		return;

	loadCostume(a->_costume);

	// Invalid costume command?
	if (a0->_costCommandNew == 0xFF || (a0->_costCommand == a0->_costCommandNew))
		return;

	a0->_costCommand = a0->_costCommandNew;

	int cmd = a0->_costCommand;
	byte limbFrameNumber = 0;

	// Each costume-command has 8 limbs  (0x2622)
	cmd <<= 3;

	for (int limb = 0; limb < 8; ++limb) {

		// get the frame number for the beginning of the costume command
		limbFrameNumber = ((_animCmds + cmd)[limb]);

		// Is this limb flipped?
		if (limbFrameNumber & 0x80) {

			// Invalid frame?
			if (limbFrameNumber == 0xFF)
				continue;

			// Store the limb frame number (clear the flipped status)
			a->_cost.frame[limb] = (limbFrameNumber & 0x7f);

			if (a0->_limb_flipped[limb] != true)
				a->_cost.start[limb] = 0xFFFF;

			a0->_limb_flipped[limb] = true;

		} else {
			//Store the limb frame number
			a->_cost.frame[limb] = limbFrameNumber;

			if (a0->_limb_flipped[limb] != false)
				a->_cost.start[limb] = 0xFFFF;

			a0->_limb_flipped[limb] = false;
		}

		// Set the repeat value
		a0->_limbFrameRepeatNew[limb] = a0->_animFrameRepeat;
	}
}

byte V0CostumeLoader::getFrame(Actor *a, int limb) {
	loadCostume(a->_costume);

	// Get the frame number for the current limb / Command
	return _frameOffsets[_frameOffsets[limb] + a->_cost.start[limb]];
}

bool V0CostumeLoader::increaseAnims(Actor *a) {
	Actor_v0 *a0 = (Actor_v0 *)a;
	int i;
	bool r = false;

	for (i = 0; i != 8; i++) {
		a0->limbFrameCheck(i);
		r |= increaseAnim(a, i);
	}
	return r;
}

bool V0CostumeLoader::increaseAnim(Actor *a, int limb) {
	Actor_v0 *a0 = (Actor_v0 *)a;
	const uint16 limbPrevious = a->_cost.curpos[limb]++;

	loadCostume(a->_costume);

	// 0x2543
	byte frame = _frameOffsets[a->_cost.curpos[limb] + a->_cost.animType[limb]];

	// Is this frame invalid?
	if (frame == 0xFF) {

		// Repeat timer has reached 0?
		if (a0->_limbFrameRepeat[limb] == 0) {

			// Use the previous frame
			--a0->_cost.curpos[limb];

			// Reset the costume command
			a0->_costCommandNew = 0xFF;
			a0->_costCommand = 0xFF;

			// Set the frame/start to invalid
			a0->_cost.frame[limb] = 0xFFFF;
			a0->_cost.start[limb] = 0xFFFF;

		} else {

			// Repeat timer enabled?
			if (a0->_limbFrameRepeat[limb] != -1)
				--a0->_limbFrameRepeat[limb];

			// No, restart at frame 0
			a->_cost.curpos[limb] = 0;
		}
	}

	// Limb frame has changed?
	return limbPrevious != a->_cost.curpos[limb];
}

} // End of namespace Scumm
