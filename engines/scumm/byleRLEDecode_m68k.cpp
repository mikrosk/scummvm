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


#include "scumm/base-costume.h"
#include "scumm/util.h"
#include "backends/platform/atari/atari-debug.h"

#undef warning
#define warning atari_warning

enum class ShadowMode : int {
	Mode0,
	Mode1,
	Mode3,
	Classic
};

#ifndef USE_M68K_COSTUME_ASM
void ByleRLEDecode_m68k_Mode0(
	Scumm::BaseCostumeRenderer::ByleRLEData *pcompData,
	const byte _scaleX, /* unused */
	const byte _scaleY, /* unused */
	const int _height,
	const int pitch,
	const int _numStrips,
	const byte *_srcPtr,
	const byte *_shadowTable, /* unused */
	const uint16 *_palette) {

	Scumm::BaseCostumeRenderer::ByleRLEData &compData = *pcompData;
	warning("%s", __FUNCTION__);

	const byte *src = _srcPtr;

	uint16 len = compData.repLen;
	uint16 color = compData.repColor;

	// reset every column
	byte *dst = compData.destPtr;
	uint16 height = _height;
	byte maskbit = revBitMask(compData.x & 7);
	const byte *mask = compData.maskPtr + compData.x / 8;

	uint16 batch;
	if (len) {
		--len;
		goto StartPos;
	}

	do {
		len = *src++;
		color = len >> compData.shr;
		len &= compData.mask;
		if (!len)
			len = *src++;

		do {
			batch = height < len ? height : len;
			len -= batch;
			height -= batch;

			assert(compData.x >= compData.boundsRect.left && compData.x < compData.boundsRect.right);

			if (color) {
				do {
					if (!(*mask & maskbit))
						*dst = _palette[color];
					dst  += pitch;
					mask += _numStrips;
				} while (--batch);
			} else {
				dst  += batch * pitch;
				mask += batch * _numStrips;
			}

			if (height == 0) {
				if (--compData.skipWidth == 0)
					return;
				height = _height;

				compData.x += compData.scaleXStep;
				maskbit = revBitMask(compData.x & 7);
				compData.destPtr += compData.scaleXStep;

				dst = compData.destPtr;
				mask = compData.maskPtr + compData.x / 8;
			}
		StartPos:;
		} while (len > 0);
	} while (true);
}
#else
extern "C" void ByleRLEDecode_m68k_Mode0(
	Scumm::BaseCostumeRenderer::ByleRLEData *pcompData,
	const byte _scaleX, /* unused */
	const byte _scaleY, /* unused */
	const int _height,
	const int pitch,
	const int _numStrips,
	const byte *_srcPtr,
	const byte *_shadowTable, /* unused */
	const uint16 *_palette);
#endif

void ByleRLEDecode_m68k_Mode1(
	Scumm::BaseCostumeRenderer::ByleRLEData *pcompData,
	const byte _scaleX, /* unused */
	const byte _scaleY, /* unused */
	const int _height,
	const int pitch,
	const int _numStrips,
	const byte *_srcPtr,
	const byte *_shadowTable,
	const uint16 *_palette) {

	Scumm::BaseCostumeRenderer::ByleRLEData &compData = *pcompData;
	warning("%s", __FUNCTION__);

	const byte *src = _srcPtr;

	uint16 len = compData.repLen;
	uint16 color = compData.repColor;

	// reset every column
	byte *dst = compData.destPtr;
	uint16 height = _height;
	byte maskbit = revBitMask(compData.x & 7);
	const byte *mask = compData.maskPtr + compData.x / 8;

	uint16 batch;
	if (len) {
		--len;
		goto StartPos;
	}

	do {
		len = *src++;
		color = len >> compData.shr;
		len &= compData.mask;
		if (!len)
			len = *src++;

		do {
			batch = height < len ? height : len;
			len -= batch;
			height -= batch;

			assert(compData.x >= compData.boundsRect.left && compData.x < compData.boundsRect.right);

			if (color) {
				do {
					if (!(*mask & maskbit)) {
						uint16 pcolor = _palette[color];
						if (pcolor == 13 && _shadowTable) {
							*dst = _shadowTable[*dst];
						} else {
							*dst = pcolor;
						}
					}
					dst  += pitch;
					mask += _numStrips;
				} while (--batch);
			} else {
				dst  += batch * pitch;
				mask += batch * _numStrips;
			}

			if (height == 0) {
				if (--compData.skipWidth == 0)
					return;
				height = _height;

				compData.x += compData.scaleXStep;
				maskbit = revBitMask(compData.x & 7);
				compData.destPtr += compData.scaleXStep;

				dst = compData.destPtr;
				mask = compData.maskPtr + compData.x / 8;
			}
		StartPos:;
		} while (len > 0);
	} while (true);
}

void ByleRLEDecode_m68k_Mode3(
	Scumm::BaseCostumeRenderer::ByleRLEData *pcompData,
	const byte _scaleX, /* unused */
	const byte _scaleY, /* unused */
	const int _height,
	const int pitch,
	const int _numStrips,
	const byte *_srcPtr,
	const byte *_shadowTable,
	const uint16 *_palette) {

	Scumm::BaseCostumeRenderer::ByleRLEData &compData = *pcompData;
	warning("%s", __FUNCTION__);

	const byte *src = _srcPtr;

	uint16 len = compData.repLen;
	uint16 color = compData.repColor;

	// reset every column
	byte *dst = compData.destPtr;
	uint16 height = _height;
	byte maskbit = revBitMask(compData.x & 7);
	const byte *mask = compData.maskPtr + compData.x / 8;

	uint16 batch;
	if (len) {
		--len;
		goto StartPos;
	}

	do {
		len = *src++;
		color = len >> compData.shr;
		len &= compData.mask;
		if (!len)
			len = *src++;

		do {
			batch = height < len ? height : len;
			len -= batch;
			height -= batch;

			assert(compData.x >= compData.boundsRect.left && compData.x < compData.boundsRect.right);

			if (color) {
				do {
					if (!(*mask & maskbit)) {
						uint16 pcolor = _palette[color];
						if (pcolor < 8) {
							pcolor = (pcolor << 8) + *dst;
							*dst = _shadowTable[pcolor];
						} else {
							*dst = pcolor;
						}
					}
					dst  += pitch;
					mask += _numStrips;
				} while (--batch);
			} else {
				dst  += batch * pitch;
				mask += batch * _numStrips;
			}

			if (height == 0) {
				if (--compData.skipWidth == 0)
					return;
				height = _height;

				compData.x += compData.scaleXStep;
				maskbit = revBitMask(compData.x & 7);
				compData.destPtr += compData.scaleXStep;

				dst = compData.destPtr;
				mask = compData.maskPtr + compData.x / 8;
			}
		StartPos:;
		} while (len > 0);
	} while (true);
}

void ByleRLEDecode_m68k_Classic(
	Scumm::BaseCostumeRenderer::ByleRLEData *pcompData,
	const byte _scaleX, /* unused */
	const byte _scaleY, /* unused */
	const int _height,
	const int pitch,
	const int _numStrips,
	const byte *_srcPtr,
	const byte *_shadowTable,
	const uint16 *_palette /* unused */) {

	Scumm::BaseCostumeRenderer::ByleRLEData &compData = *pcompData;
	warning("%s", __FUNCTION__);

	const byte *src = _srcPtr;

	uint16 len = compData.repLen;
	uint16 color = compData.repColor;

	// reset every column
	byte *dst = compData.destPtr;
	uint16 height = _height;
	byte maskbit = revBitMask(compData.x & 7);
	const byte *mask = compData.maskPtr + compData.x / 8;

	uint16 batch;
	if (len) {
		--len;
		goto StartPos;
	}

	do {
		len = *src++;
		color = len >> compData.shr;
		len &= compData.mask;
		if (!len)
			len = *src++;

		do {
			batch = height < len ? height : len;
			len -= batch;
			height -= batch;

			assert(compData.x >= compData.boundsRect.left && compData.x < compData.boundsRect.right);

			if (color) {
				do {
					if (!(*mask & maskbit))
						*dst = _shadowTable[*dst];
					dst  += pitch;
					mask += _numStrips;
				} while (--batch);
			} else {
				dst  += batch * pitch;
				mask += batch * _numStrips;
			}

			if (height == 0) {
				if (--compData.skipWidth == 0)
					return;
				height = _height;

				compData.x += compData.scaleXStep;
				maskbit = revBitMask(compData.x & 7);
				compData.destPtr += compData.scaleXStep;

				dst = compData.destPtr;
				mask = compData.maskPtr + compData.x / 8;
			}
		StartPos:;
		} while (len > 0);
	} while (true);
}

void ByleRLEDecode_m68k_Scaled_Mode0(
	Scumm::BaseCostumeRenderer::ByleRLEData *pcompData,
	const byte _scaleX,
	const byte _scaleY,
	const int _height,
	const int pitch,
	const int _numStrips,
	const byte *_srcPtr,
	const byte *_shadowTable, /* unused */
	const uint16 *_palette) {

	Scumm::BaseCostumeRenderer::ByleRLEData &compData = *pcompData;
	warning("%s", __FUNCTION__);

	const byte *src = _srcPtr;

	byte len = compData.repLen;
	uint16 color = compData.repColor;

	// reset every column
	byte *dst = compData.destPtr;
	uint16 height = _height;
	int scaleIndexY = compData.scaleYIndex;
	byte maskbit = revBitMask(compData.x & 7);
	const byte *mask = compData.maskPtr + compData.x / 8;

	byte batch;
	if (len) {
		--len;
		goto StartPos;
	}

	do {
		len = *src++;
		color = len >> compData.shr;
		len &= compData.mask;
		if (!len)
			len = *src++;

		do {
			batch = height < len ? (byte)height : len;
			len -= batch;
			height -= batch;

			assert(compData.x >= compData.boundsRect.left && compData.x < compData.boundsRect.right);

			do {
				if (compData.scaleTable[scaleIndexY++] < _scaleY) {
					if (color) {
						if (!(*mask & maskbit))
							*dst = _palette[color];
					}
					dst += pitch;
					mask += _numStrips;
				}
			} while (--batch);

			if (height == 0) {
				if (--compData.skipWidth == 0)
					return;
				height = _height;

				scaleIndexY = compData.scaleYIndex;

				if (compData.scaleTable[compData.scaleXIndex] < _scaleX) {
					compData.x += compData.scaleXStep;
					maskbit = revBitMask(compData.x & 7);
					compData.destPtr += compData.scaleXStep;
				}

				compData.scaleXIndex += compData.scaleXStep;

				dst = compData.destPtr;
				mask = compData.maskPtr + compData.x / 8;
			}
		StartPos:;
		} while (len > 0);
	} while (true);
}

void ByleRLEDecode_m68k_Scaled_Mode0_SMask(
	Scumm::BaseCostumeRenderer::ByleRLEData *pcompData,
	const byte _scaleX,
	const byte _scaleY,
	const int _height,
	const int pitch,
	const int _numStrips,
	const byte *_srcPtr,
	const byte *_shadowTable, /* unused */
	const uint16 *_palette) {

	Scumm::BaseCostumeRenderer::ByleRLEData &compData = *pcompData;
	warning("%s", __FUNCTION__);

	const byte *src = _srcPtr;

	byte len = compData.repLen;
	uint16 color = compData.repColor;

	// reset every column
	byte *dst = compData.destPtr;
	uint16 height = _height;
	int scaleIndexY = compData.scaleYIndex;
	byte maskbit = revBitMask(compData.x & 7);
	const byte *mask = compData.maskPtr + compData.x / 8;

	byte batch;
	if (len) {
		--len;
		goto StartPos;
	}

	do {
		len = *src++;
		color = len >> compData.shr;
		len &= compData.mask;
		if (!len)
			len = *src++;

		do {
			batch = height < len ? (byte)height : len;
			len -= batch;
			height -= batch;

			assert(compData.x >= compData.boundsRect.left && compData.x < compData.boundsRect.right);

			do {
				if (compData.scaleTable[scaleIndexY++ & compData.scaleIndexMask] < _scaleY) {
					if (color) {
						if (!(*mask & maskbit))
							*dst = _palette[color];
					}
					dst += pitch;
					mask += _numStrips;
				}
			} while (--batch);

			if (height == 0) {
				if (--compData.skipWidth == 0)
					return;
				height = _height;

				scaleIndexY = compData.scaleYIndex;

				if (compData.scaleTable[compData.scaleXIndex] < _scaleX) {
					compData.x += compData.scaleXStep;
					maskbit = revBitMask(compData.x & 7);
					compData.destPtr += compData.scaleXStep;
				}

				compData.scaleXIndex = (compData.scaleXIndex + compData.scaleXStep) & compData.scaleIndexMask;

				dst = compData.destPtr;
				mask = compData.maskPtr + compData.x / 8;
			}
		StartPos:;
		} while (len > 0);
	} while (true);
}

void ByleRLEDecode_m68k_Scaled_Mode1(
	Scumm::BaseCostumeRenderer::ByleRLEData *pcompData,
	const byte _scaleX,
	const byte _scaleY,
	const int _height,
	const int pitch,
	const int _numStrips,
	const byte *_srcPtr,
	const byte *_shadowTable,
	const uint16 *_palette) {

	Scumm::BaseCostumeRenderer::ByleRLEData &compData = *pcompData;
	warning("%s", __FUNCTION__);

	const byte *src = _srcPtr;

	byte len = compData.repLen;
	uint16 color = compData.repColor;

	// reset every column
	byte *dst = compData.destPtr;
	int lastColumnX = -1;
	uint16 height = _height;
	int scaleIndexY = compData.scaleYIndex;
	byte maskbit = revBitMask(compData.x & 7);
	const byte *mask = compData.maskPtr + compData.x / 8;

	byte batch;
	if (len) {
		--len;
		goto StartPos;
	}

	do {
		len = *src++;
		color = len >> compData.shr;
		len &= compData.mask;
		if (!len)
			len = *src++;

		do {
			batch = height < len ? (byte)height : len;
			len -= batch;
			height -= batch;

			assert(compData.x >= compData.boundsRect.left && compData.x < compData.boundsRect.right);

			do {
				if (compData.scaleTable[scaleIndexY++] < _scaleY) {
					if (color) {
						if (!(*mask & maskbit)) {
							uint16 pcolor;

							pcolor = _palette[color];
							if (pcolor == 13 && _shadowTable) {
								if (lastColumnX != compData.x)
									*dst = _shadowTable[*dst];
							} else {
								*dst = pcolor;
							}
						}
					}
					dst += pitch;
					mask += _numStrips;
				}
			} while (--batch);

			if (height == 0) {
				if (--compData.skipWidth == 0)
					return;
				height = _height;

				scaleIndexY = compData.scaleYIndex;
				lastColumnX = compData.x;

				if (compData.scaleTable[compData.scaleXIndex] < _scaleX) {
					compData.x += compData.scaleXStep;
					maskbit = revBitMask(compData.x & 7);
					compData.destPtr += compData.scaleXStep;
				}

				compData.scaleXIndex += compData.scaleXStep;

				dst = compData.destPtr;
				mask = compData.maskPtr + compData.x / 8;
			}
		StartPos:;
		} while (len > 0);
	} while (true);
}

void ByleRLEDecode_m68k_Scaled_Mode1_SMask(
	Scumm::BaseCostumeRenderer::ByleRLEData *pcompData,
	const byte _scaleX,
	const byte _scaleY,
	const int _height,
	const int pitch,
	const int _numStrips,
	const byte *_srcPtr,
	const byte *_shadowTable,
	const uint16 *_palette) {

	Scumm::BaseCostumeRenderer::ByleRLEData &compData = *pcompData;
	warning("%s", __FUNCTION__);

	const byte *src = _srcPtr;

	byte len = compData.repLen;
	uint16 color = compData.repColor;

	// reset every column
	byte *dst = compData.destPtr;
	int lastColumnX = -1;
	uint16 height = _height;
	int scaleIndexY = compData.scaleYIndex;
	byte maskbit = revBitMask(compData.x & 7);
	const byte *mask = compData.maskPtr + compData.x / 8;

	byte batch;
	if (len) {
		--len;
		goto StartPos;
	}

	do {
		len = *src++;
		color = len >> compData.shr;
		len &= compData.mask;
		if (!len)
			len = *src++;

		do {
			batch = height < len ? (byte)height : len;
			len -= batch;
			height -= batch;

			assert(compData.x >= compData.boundsRect.left && compData.x < compData.boundsRect.right);

			do {
				if (compData.scaleTable[scaleIndexY++ & compData.scaleIndexMask] < _scaleY) {
					if (color) {
						if (!(*mask & maskbit)) {
							uint16 pcolor;

							pcolor = _palette[color];
							if (pcolor == 13 && _shadowTable) {
								if (lastColumnX != compData.x)
									*dst = _shadowTable[*dst];
							} else {
								*dst = pcolor;
							}
						}
					}
					dst += pitch;
					mask += _numStrips;
				}
			} while (--batch);

			if (height == 0) {
				if (--compData.skipWidth == 0)
					return;
				height = _height;

				scaleIndexY = compData.scaleYIndex;
				lastColumnX = compData.x;

				if (compData.scaleTable[compData.scaleXIndex] < _scaleX) {
					compData.x += compData.scaleXStep;
					maskbit = revBitMask(compData.x & 7);
					compData.destPtr += compData.scaleXStep;
				}

				compData.scaleXIndex = (compData.scaleXIndex + compData.scaleXStep) & compData.scaleIndexMask;

				dst = compData.destPtr;
				mask = compData.maskPtr + compData.x / 8;
			}
		StartPos:;
		} while (len > 0);
	} while (true);
}

void ByleRLEDecode_m68k_Scaled_Mode3(
	Scumm::BaseCostumeRenderer::ByleRLEData *pcompData,
	const byte _scaleX,
	const byte _scaleY,
	const int _height,
	const int pitch,
	const int _numStrips,
	const byte *_srcPtr,
	const byte *_shadowTable,
	const uint16 *_palette) {

	Scumm::BaseCostumeRenderer::ByleRLEData &compData = *pcompData;
	warning("%s", __FUNCTION__);

	const byte *src = _srcPtr;

	byte len = compData.repLen;
	uint16 color = compData.repColor;

	// reset every column
	byte *dst = compData.destPtr;
	int lastColumnX = -1;
	uint16 height = _height;
	int scaleIndexY = compData.scaleYIndex;
	byte maskbit = revBitMask(compData.x & 7);
	const byte *mask = compData.maskPtr + compData.x / 8;

	byte batch;
	if (len) {
		--len;
		goto StartPos;
	}

	do {
		len = *src++;
		color = len >> compData.shr;
		len &= compData.mask;
		if (!len)
			len = *src++;

		do {
			batch = height < len ? (byte)height : len;
			len -= batch;
			height -= batch;

			assert(compData.x >= compData.boundsRect.left && compData.x < compData.boundsRect.right);

			do {
				if (compData.scaleTable[scaleIndexY++] < _scaleY) {
					if (color) {
						if (!(*mask & maskbit)) {
							uint16 pcolor;

							pcolor = _palette[color];
							if (pcolor < 8) {
								if (lastColumnX != compData.x) {
									pcolor = (pcolor << 8) + *dst;
									*dst = _shadowTable[pcolor];
								}
							} else {
								*dst = pcolor;
							}
						}
					}
					dst += pitch;
					mask += _numStrips;
				}
			} while (--batch);

			if (height == 0) {
				if (--compData.skipWidth == 0)
					return;
				height = _height;

				scaleIndexY = compData.scaleYIndex;
				lastColumnX = compData.x;

				if (compData.scaleTable[compData.scaleXIndex] < _scaleX) {
					compData.x += compData.scaleXStep;
					maskbit = revBitMask(compData.x & 7);
					compData.destPtr += compData.scaleXStep;
				}

				compData.scaleXIndex += compData.scaleXStep;

				dst = compData.destPtr;
				mask = compData.maskPtr + compData.x / 8;
			}
		StartPos:;
		} while (len > 0);
	} while (true);
}

void ByleRLEDecode_m68k_Scaled_Mode3_SMask(
	Scumm::BaseCostumeRenderer::ByleRLEData *pcompData,
	const byte _scaleX,
	const byte _scaleY,
	const int _height,
	const int pitch,
	const int _numStrips,
	const byte *_srcPtr,
	const byte *_shadowTable,
	const uint16 *_palette) {

	Scumm::BaseCostumeRenderer::ByleRLEData &compData = *pcompData;
	warning("%s", __FUNCTION__);

	const byte *src = _srcPtr;

	byte len = compData.repLen;
	uint16 color = compData.repColor;

	// reset every column
	byte *dst = compData.destPtr;
	int lastColumnX = -1;
	uint16 height = _height;
	int scaleIndexY = compData.scaleYIndex;
	byte maskbit = revBitMask(compData.x & 7);
	const byte *mask = compData.maskPtr + compData.x / 8;

	byte batch;
	if (len) {
		--len;
		goto StartPos;
	}

	do {
		len = *src++;
		color = len >> compData.shr;
		len &= compData.mask;
		if (!len)
			len = *src++;

		do {
			batch = height < len ? (byte)height : len;
			len -= batch;
			height -= batch;

			assert(compData.x >= compData.boundsRect.left && compData.x < compData.boundsRect.right);

			do {
				if (compData.scaleTable[scaleIndexY++ & compData.scaleIndexMask] < _scaleY) {
					if (color) {
						if (!(*mask & maskbit)) {
							uint16 pcolor;

							pcolor = _palette[color];
							if (pcolor < 8) {
								if (lastColumnX != compData.x) {
									pcolor = (pcolor << 8) + *dst;
									*dst = _shadowTable[pcolor];
								}
							} else {
								*dst = pcolor;
							}
						}
					}
					dst += pitch;
					mask += _numStrips;
				}
			} while (--batch);

			if (height == 0) {
				if (--compData.skipWidth == 0)
					return;
				height = _height;

				scaleIndexY = compData.scaleYIndex;
				lastColumnX = compData.x;

				if (compData.scaleTable[compData.scaleXIndex] < _scaleX) {
					compData.x += compData.scaleXStep;
					maskbit = revBitMask(compData.x & 7);
					compData.destPtr += compData.scaleXStep;
				}

				compData.scaleXIndex = (compData.scaleXIndex + compData.scaleXStep) & compData.scaleIndexMask;

				dst = compData.destPtr;
				mask = compData.maskPtr + compData.x / 8;
			}
		StartPos:;
		} while (len > 0);
	} while (true);
}

void ByleRLEDecode_m68k_Scaled_Classic(
	Scumm::BaseCostumeRenderer::ByleRLEData *pcompData,
	const byte _scaleX,
	const byte _scaleY,
	const int _height,
	const int pitch,
	const int _numStrips,
	const byte *_srcPtr,
	const byte *_shadowTable,
	const uint16 *_palette /* unused */) {

	Scumm::BaseCostumeRenderer::ByleRLEData &compData = *pcompData;
	warning("%s", __FUNCTION__);

	const byte *src = _srcPtr;

	byte len = compData.repLen;
	uint16 color = compData.repColor;

	// reset every column
	byte *dst = compData.destPtr;
	int lastColumnX = -1;
	uint16 height = _height;
	int scaleIndexY = compData.scaleYIndex;
	byte maskbit = revBitMask(compData.x & 7);
	const byte *mask = compData.maskPtr + compData.x / 8;

	byte batch;
	if (len) {
		--len;
		goto StartPos;
	}

	do {
		len = *src++;
		color = len >> compData.shr;
		len &= compData.mask;
		if (!len)
			len = *src++;

		do {
			batch = height < len ? (byte)height : len;
			len -= batch;
			height -= batch;

			assert(compData.x >= compData.boundsRect.left && compData.x < compData.boundsRect.right);

			do {
				if (compData.scaleTable[scaleIndexY++] < _scaleY) {
					if (color) {
						if (!(*mask & maskbit)) {
							if (lastColumnX != compData.x)
								*dst = _shadowTable[*dst];
						}
					}
					dst += pitch;
					mask += _numStrips;
				}
			} while (--batch);

			if (height == 0) {
				if (--compData.skipWidth == 0)
					return;
				height = _height;

				scaleIndexY = compData.scaleYIndex;
				lastColumnX = compData.x;

				if (compData.scaleTable[compData.scaleXIndex] < _scaleX) {
					compData.x += compData.scaleXStep;
					maskbit = revBitMask(compData.x & 7);
					compData.destPtr += compData.scaleXStep;
				}

				compData.scaleXIndex += compData.scaleXStep;

				dst = compData.destPtr;
				mask = compData.maskPtr + compData.x / 8;
			}
		StartPos:;
		} while (len > 0);
	} while (true);
}

void ByleRLEDecode_m68k_Scaled_Classic_SMask(
	Scumm::BaseCostumeRenderer::ByleRLEData *pcompData,
	const byte _scaleX,
	const byte _scaleY,
	const int _height,
	const int pitch,
	const int _numStrips,
	const byte *_srcPtr,
	const byte *_shadowTable,
	const uint16 *_palette /* unused */) {

	Scumm::BaseCostumeRenderer::ByleRLEData &compData = *pcompData;
	warning("%s", __FUNCTION__);

	const byte *src = _srcPtr;

	byte len = compData.repLen;
	uint16 color = compData.repColor;

	// reset every column
	byte *dst = compData.destPtr;
	int lastColumnX = -1;
	uint16 height = _height;
	int scaleIndexY = compData.scaleYIndex;
	byte maskbit = revBitMask(compData.x & 7);
	const byte *mask = compData.maskPtr + compData.x / 8;

	byte batch;
	if (len) {
		--len;
		goto StartPos;
	}

	do {
		len = *src++;
		color = len >> compData.shr;
		len &= compData.mask;
		if (!len)
			len = *src++;

		do {
			batch = height < len ? (byte)height : len;
			len -= batch;
			height -= batch;

			assert(compData.x >= compData.boundsRect.left && compData.x < compData.boundsRect.right);

			do {
				if (compData.scaleTable[scaleIndexY++ & compData.scaleIndexMask] < _scaleY) {
					if (color) {
						if (!(*mask & maskbit)) {
							if (lastColumnX != compData.x)
								*dst = _shadowTable[*dst];
						}
					}
					dst += pitch;
					mask += _numStrips;
				}
			} while (--batch);

			if (height == 0) {
				if (--compData.skipWidth == 0)
					return;
				height = _height;

				scaleIndexY = compData.scaleYIndex;
				lastColumnX = compData.x;

				if (compData.scaleTable[compData.scaleXIndex] < _scaleX) {
					compData.x += compData.scaleXStep;
					maskbit = revBitMask(compData.x & 7);
					compData.destPtr += compData.scaleXStep;
				}

				compData.scaleXIndex = (compData.scaleXIndex + compData.scaleXStep) & compData.scaleIndexMask;

				dst = compData.destPtr;
				mask = compData.maskPtr + compData.x / 8;
			}
		StartPos:;
		} while (len > 0);
	} while (true);
}

typedef void (*ByleRLEDecodeFunc)(
	Scumm::BaseCostumeRenderer::ByleRLEData *pcompData,
	const byte _scaleX,
	const byte _scaleY,
	const int _height,
	const int pitch,
	const int _numStrips,
	const byte *_srcPtr,
	const byte *_shadowTable,
	const uint16 *_palette);

static const ByleRLEDecodeFunc byleRLEDecodeNoScaleTable[4] = {
	ByleRLEDecode_m68k_Mode0,    // 0: Mode0
	ByleRLEDecode_m68k_Mode1,    // 1: Mode1
	ByleRLEDecode_m68k_Mode3,    // 2: Mode3
	ByleRLEDecode_m68k_Classic,  // 3: Classic
};

static const ByleRLEDecodeFunc byleRLEDecodeScaledTable[8] = {
	ByleRLEDecode_m68k_Scaled_Mode0,         // 0: Mode0,   no scaleIndexMask
	ByleRLEDecode_m68k_Scaled_Mode0_SMask,   // 1: Mode0,   scaleIndexMask
	ByleRLEDecode_m68k_Scaled_Mode1,         // 2: Mode1,   no scaleIndexMask
	ByleRLEDecode_m68k_Scaled_Mode1_SMask,   // 3: Mode1,   scaleIndexMask
	ByleRLEDecode_m68k_Scaled_Mode3,         // 4: Mode3,   no scaleIndexMask
	ByleRLEDecode_m68k_Scaled_Mode3_SMask,   // 5: Mode3,   scaleIndexMask
	ByleRLEDecode_m68k_Scaled_Classic,       // 6: Classic, no scaleIndexMask
	ByleRLEDecode_m68k_Scaled_Classic_SMask, // 7: Classic, scaleIndexMask
};

extern "C" void ByleRLEDecodeM68K(
	Scumm::BaseCostumeRenderer::ByleRLEData *pcompData,
	const byte _scaleX,
	const byte _scaleY,
	const int _height,
	const int pitch,
	const int _numStrips,
	bool _akosRendering,
	int _shadowMode,
	const byte *_srcPtr,
	const byte *_shadowTable,
	const uint16 *_palette);

void ByleRLEDecodeM68K(
	Scumm::BaseCostumeRenderer::ByleRLEData *pcompData,
	const byte _scaleX,
	const byte _scaleY,
	const int _height,
	const int pitch,
	const int _numStrips,
	bool _akosRendering,
	int _shadowMode,
	const byte *_srcPtr,
	const byte *_shadowTable,
	const uint16 *_palette) {

	Scumm::BaseCostumeRenderer::ByleRLEData &compData = *pcompData;

	ShadowMode shadowMode = ShadowMode::Mode0;
	if (!_akosRendering) {
		if (_shadowMode & 0x20)
			shadowMode = ShadowMode::Classic;
		else
			shadowMode = ShadowMode::Mode1;
	} else {
		if (_shadowMode == 1)
			shadowMode = ShadowMode::Mode1;
		else if (_shadowMode == 3)
			shadowMode = ShadowMode::Mode3;
	}

	if (compData.y >= compData.boundsRect.top && compData.y + _height <= compData.boundsRect.bottom) {
		const int scaled = (_scaleX != 255 || _scaleY != 255);
		if (!scaled) {
			byleRLEDecodeNoScaleTable[static_cast<int>(shadowMode)](
				&compData,
				_scaleX,
				_scaleY,
				_height,
				pitch,
				_numStrips,
				_srcPtr,
				_shadowTable,
				_palette);
		} else {
			const int useScaleIndexMask = compData.scaleIndexMask != -1;

			byleRLEDecodeScaledTable[(static_cast<int>(shadowMode) << 1) | useScaleIndexMask](
				&compData,
				_scaleX,
				_scaleY,
				_height,
				pitch,
				_numStrips,
				_srcPtr,
				_shadowTable,
				_palette);
		}
		return;
	}

	warning("%s", __FUNCTION__);

	const byte *src = _srcPtr;

	byte len = compData.repLen;
	uint16 color = compData.repColor;

	// reset every column
	byte *dst = compData.destPtr;
	int lastColumnX = -1;
	int y = compData.y;
	uint16 height = _height;
	int scaleIndexY = compData.scaleYIndex;
	byte maskbit = revBitMask(compData.x & 7);
	const byte *mask = compData.maskPtr + compData.x / 8;

	byte batch;
	if (len) {
		--len;
		goto StartPos;
	}

	do {
		len = *src++;
		color = len >> compData.shr;
		len &= compData.mask;
		if (!len)
			len = *src++;

		do {
			batch = height < len ? (byte)height : len;
			len -= batch;
			height -= batch;

			assert(compData.x >= compData.boundsRect.left && compData.x < compData.boundsRect.right);

			do {
				if (_scaleY == 255 || compData.scaleTable[scaleIndexY++ & compData.scaleIndexMask] < _scaleY) {
					if (color) {
						const bool masked = (y < compData.boundsRect.top || y >= compData.boundsRect.bottom)
							|| (*mask & maskbit);

						if (!masked) {
							uint16 pcolor;

							switch(shadowMode) {
							case ShadowMode::Mode0:
								*dst = _palette[color];
								break;

							case ShadowMode::Classic:
								if (lastColumnX != compData.x)
									*dst = _shadowTable[*dst];
								break;

							case ShadowMode::Mode1:
								pcolor = _palette[color];
								if (pcolor == 13 && _shadowTable) {
									if (lastColumnX != compData.x)
										*dst = _shadowTable[*dst];
								} else {
									*dst = pcolor;
								}
								break;

							case ShadowMode::Mode3:
								pcolor = _palette[color];
								if (pcolor < 8) {
									if (lastColumnX != compData.x) {
										pcolor = (pcolor << 8) + *dst;
										*dst = _shadowTable[pcolor];
									}
								} else {
									*dst = pcolor;
								}
								break;
							}
						}
					}
					dst += pitch;
					mask += _numStrips;
					y++;
				}
			} while (--batch);

			if (height == 0) {
				if (--compData.skipWidth == 0)
					return;
				height = _height;
				y = compData.y;

				scaleIndexY = compData.scaleYIndex;
				lastColumnX = compData.x;

				if (_scaleX == 255 || compData.scaleTable[compData.scaleXIndex] < _scaleX) {
					compData.x += compData.scaleXStep;
					maskbit = revBitMask(compData.x & 7);
					compData.destPtr += compData.scaleXStep;
				}

				compData.scaleXIndex = (compData.scaleXIndex + compData.scaleXStep) & compData.scaleIndexMask;

				dst = compData.destPtr;
				mask = compData.maskPtr + compData.x / 8;
			}
		StartPos:;
		} while (len > 0);
	} while (true);
}
