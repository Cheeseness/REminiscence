/* REminiscence - Flashback interpreter
 * Copyright (C) 2005-2011 Gregory Montoir
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
 */

#ifndef VIDEO_H__
#define VIDEO_H__

#include "intern.h"

struct Resource;
struct SystemStub;

struct Video {
	enum {
		GAMESCREEN_W = 256,
		GAMESCREEN_H = 224,
		SCREENBLOCK_W = 8,
		SCREENBLOCK_H = 8,
		CHAR_W = 8,
		CHAR_H = 8
	};

	static const uint8 _conradPal1[];
	static const uint8 _conradPal2[];
	static const uint8 _textPal[];
	static const uint8 _palSlot0xF[];

	Resource *_res;
	SystemStub *_stub;

	uint8 *_frontLayer;
	uint8 *_backLayer;
	uint8 *_tempLayer;
	uint8 *_tempLayer2;
	uint8 _unkPalSlot1, _unkPalSlot2;
	uint8 _mapPalSlot1, _mapPalSlot2, _mapPalSlot3, _mapPalSlot4;
	uint8 _charFrontColor;
	uint8 _charTransparentColor;
	uint8 _charShadowColor;
	uint8 *_screenBlocks;
	bool _fullRefresh;
	uint8 _shakeOffset;

	Video(Resource *res, SystemStub *stub);
	~Video();

	void markBlockAsDirty(int16 x, int16 y, uint16 w, uint16 h);
	void updateScreen();
	void fullRefresh();
	void fadeOut();
	void fadeOutPalette();
	void setPaletteColorBE(int num, int offset);
	void setPaletteSlotBE(int palSlot, int palNum);
	void setPaletteSlotLE(int palSlot, const uint8 *palData);
	void setTextPalette();
	void setPalette0xF();
	void PC_decodeMap(int level, int room);
	void PC_setLevelPalettes();
	void PC_decodeIcn(const uint8 *src, int num, uint8 *dst);
	void PC_decodeSpc(const uint8 *src, int w, int h, uint8 *dst);
	void AMIGA_decodeLev(int level, int room);
	void AMIGA_decodeSpm(const uint8 *src, uint8 *dst);
	void AMIGA_decodeIcn(const uint8 *src, int num, uint8 *dst);
	void AMIGA_decodeSpc(const uint8 *src, int w, int h, uint8 *dst);
	void drawSpriteSub1(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask);
	void drawSpriteSub2(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask);
	void drawSpriteSub3(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask);
	void drawSpriteSub4(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask);
	void drawSpriteSub5(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask);
	void drawSpriteSub6(const uint8 *src, uint8 *dst, int pitch, int h, int w, uint8 colMask);
	void PC_drawChar(uint8 c, int16 y, int16 x);
	void PC_drawStringChar(uint8 *dst, int pitch, const uint8 *src, uint8 color, uint8 chr);
	void AMIGA_drawStringChar(uint8 *dst, int pitch, const uint8 *src, uint8 color, uint8 chr);
	const char *drawString(const char *str, int16 x, int16 y, uint8 col);
};

#endif // VIDEO_H__
