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

#ifndef BACKENDS_GRAPHICS_ATARI_ASM_H
#define BACKENDS_GRAPHICS_ATARI_ASM_H

#include <stdint.h>

extern "C" {

void asm_screen_tt_save(void);
void asm_screen_falcon_save(void);

void asm_screen_tt_restore(void);
void asm_screen_falcon_restore(void);

void asm_screen_set_tt_palette(const uint16_t* pPalette, uint16_t palette_size);
void asm_screen_set_falcon_palette(const uint32_t* pPalette, uint16_t palette_size);

void asm_screen_set_vram(const void* pScreen);

}

#endif
