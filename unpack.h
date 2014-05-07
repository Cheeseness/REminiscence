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

#ifndef UNPACK_H__
#define UNPACK_H__

#include "intern.h"


struct UnpackCtx {
	int size, datasize;
	uint32 crc;
	uint32 chk;
	uint8 *dst;
	const uint8 *src;
};

extern bool delphine_unpack(uint8 *dst, const uint8 *src, int len);


#endif // UNPACK_H__
