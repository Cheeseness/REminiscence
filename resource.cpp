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

#include "file.h"
#include "unpack.h"
#include "resource.h"


Resource::Resource(FileSystem *fs, ResourceType ver, Language lang) {
	memset(this, 0, sizeof(Resource));
	_type = ver;
	_lang = lang;
	_fs = fs;
	_memBuf = (uint8 *)malloc(256 * 224);
	if (!_memBuf) {
		error("Unable to allocate temporary memory buffer");
	}
	static const int kBankDataSize = 0x7000;
	_bankData = (uint8 *)malloc(kBankDataSize);
	if (!_bankData) {
		error("Unable to allocate bank data buffer");
	}
	_bankDataTail = _bankData + kBankDataSize;
	clearBankData();
}

Resource::~Resource() {
	clearLevelRes();
	free(_fnt);
	free(_icn); _icn = 0;
	_icnLen = 0;
	free(_tab);
	free(_spc);
	free(_spr1);
	free(_memBuf);
	free(_cmd);
	free(_pol);
	free(_cine_off);
	free(_cine_txt);
	for (int i = 0; i < _numSfx; ++i) {
		free(_sfxList[i].data);
	}
	free(_sfxList);
}

void Resource::clearLevelRes() {
	free(_tbn); _tbn = 0;
	free(_mbk); _mbk = 0;
	free(_pal); _pal = 0;
	free(_map); _map = 0;
	free(_lev); _lev = 0;
	_levNum = -1;
	free(_sgd); _sgd = 0;
	free(_ani); _ani = 0;
	free_OBJ();
}

void Resource::load_FIB(const char *fileName) {
	debug(DBG_RES, "Resource::load_FIB('%s')", fileName);
	static const uint8 fibonacciTable[] = {
		0xDE, 0xEB, 0xF3, 0xF8, 0xFB, 0xFD, 0xFE, 0xFF,
		0x00, 0x01, 0x02, 0x03, 0x05, 0x08, 0x0D, 0x15
	};
	snprintf(_entryName, sizeof(_entryName), "%s.FIB", fileName);
	File f;
	if (f.open(_entryName, "rb", _fs)) {
		_numSfx = f.readUint16LE();
		_sfxList = (SoundFx *)malloc(_numSfx * sizeof(SoundFx));
		if (!_sfxList) {
			error("Unable to allocate SoundFx table");
		}
		int i;
		for (i = 0; i < _numSfx; ++i) {
			SoundFx *sfx = &_sfxList[i];
			sfx->offset = f.readUint32LE();
			sfx->len = f.readUint16LE();
			sfx->data = 0;
		}
		for (i = 0; i < _numSfx; ++i) {
			SoundFx *sfx = &_sfxList[i];
			if (sfx->len == 0) {
				continue;
			}
			f.seek(sfx->offset);
			uint8 *data = (uint8 *)malloc(sfx->len * 2);
			if (!data) {
				error("Unable to allocate SoundFx data buffer");
			}
			sfx->data = data;
			uint8 c = f.readByte();
			*data++ = c;
			*data++ = c;
			uint16 sz = sfx->len - 1;
			while (sz--) {
				uint8 d = f.readByte();
				c += fibonacciTable[d >> 4];
				*data++ = c;
				c += fibonacciTable[d & 15];
				*data++ = c;
			}
			sfx->len *= 2;
		}
		if (f.ioErr()) {
			error("I/O error when reading '%s'", _entryName);
		}
	} else {
		error("Can't open '%s'", _entryName);
	}
}

void Resource::load_MAP_menu(const char *fileName, uint8 *dstPtr) {
	debug(DBG_RES, "Resource::load_MAP_menu('%s')", fileName);
	snprintf(_entryName, sizeof(_entryName), "%s.MAP", fileName);
	File f;
	if (f.open(_entryName, "rb", _fs)) {
		if (f.size() != 0x3800 * 4) {
			error("Wrong file size for '%s', %d", _entryName, f.size());
		}
		f.read(dstPtr, 0x3800 * 4);
		if (f.ioErr()) {
			error("I/O error when reading '%s'", _entryName);
		}
	} else {
		error("Can't open '%s'", _entryName);
	}
}

void Resource::load_PAL_menu(const char *fileName, uint8 *dstPtr) {
	debug(DBG_RES, "Resource::load_PAL_menu('%s')", fileName);
	snprintf(_entryName, sizeof(_entryName), "%s.PAL", fileName);
	File f;
	if (f.open(_entryName, "rb", _fs)) {
		if (f.size() != 768) {
			error("Wrong file size for '%s', %d", _entryName, f.size());
		}
		f.read(dstPtr, 768);
		if (f.ioErr()) {
			error("I/O error when reading '%s'", _entryName);
		}
	} else {
		error("Can't open '%s'", _entryName);
	}
}

void Resource::load_SPR_OFF(const char *fileName, uint8 *sprData) {
	debug(DBG_RES, "Resource::load_SPR_OFF('%s')", fileName);
	snprintf(_entryName, sizeof(_entryName), "%s.OFF", fileName);
	File f;
	if (f.open(_entryName, "rb", _fs)) {
		int len = f.size();
		uint8 *offData = (uint8 *)malloc(len);
		if (!offData) {
			error("Unable to allocate sprite offsets");
		}
		f.read(offData, len);
		if (f.ioErr()) {
			error("I/O error when reading '%s'", _entryName);
		}
		const uint8 *p = offData;
		uint16 pos;
		while ((pos = READ_LE_UINT16(p)) != 0xFFFF) {
			uint32 off = READ_LE_UINT32(p + 2);
			if (off == 0xFFFFFFFF) {
				_spr_off[pos] = 0;
			} else {
				_spr_off[pos] = sprData + off;
			}
			p += 6;
		}
		free(offData);
	} else {
		error("Can't open '%s'", _entryName);
	}
}

void Resource::load_CINE() {
	const char *baseName = 0;
	switch (_lang) {
	case LANG_FR:
		baseName = "FR_CINE";
		break;
	case LANG_EN:
		baseName = "ENGCINE";
		break;
	case LANG_DE:
		baseName = "GERCINE";
		break;
	case LANG_SP:
		baseName = "SPACINE";
		break;
	}
	debug(DBG_RES, "Resource::load_CINE('%s')", baseName);
	if (_cine_off == 0) {
		snprintf(_entryName, sizeof(_entryName), "%s.BIN", baseName);
		File f;
		if (f.open(_entryName, "rb", _fs)) {
			int len = f.size();
			_cine_off = (uint8 *)malloc(len);
			if (!_cine_off) {
				error("Unable to allocate cinematics offsets");
			}
			f.read(_cine_off, len);
			if (f.ioErr()) {
				error("I/O error when reading '%s'", _entryName);
			}
		} else {
			error("Can't open '%s'", _entryName);
		}
	}
	if (_cine_txt == 0) {
		snprintf(_entryName, sizeof(_entryName), "%s.TXT", baseName);
		File f;
		if (f.open(_entryName, "rb", _fs)) {
			int len = f.size();
			_cine_txt = (uint8 *)malloc(len);
			if (!_cine_txt) {
				error("Unable to allocate cinematics text data");
			}
			f.read(_cine_txt, len);
			if (f.ioErr()) {
				error("I/O error when reading '%s'", _entryName);
			}
		} else {
			error("Can't open '%s'", _entryName);
		}
	}
}

void Resource::load_TEXT() {
	File f;
	// Load game strings
	_stringsTable = 0;
	if (f.open("STRINGS.TXT", "rb", _fs)) {
		const int sz = f.size();
		_extStringsTable = (uint8 *)malloc(sz);
		if (_extStringsTable) {
			f.read(_extStringsTable, sz);
			_stringsTable = _extStringsTable;
		}
		f.close();
	}
	if (!_stringsTable) {
		switch (_lang) {
		case LANG_FR:
			_stringsTable = LocaleData::_stringsTableFR;
			break;
		case LANG_EN:
			_stringsTable = LocaleData::_stringsTableEN;
			break;
		case LANG_DE:
			_stringsTable = LocaleData::_stringsTableDE;
			break;
		case LANG_SP:
			_stringsTable = LocaleData::_stringsTableSP;
			break;
		}
	}
	// Load menu strings
	_textsTable = 0;
	if (f.open("MENUS.TXT", "rb", _fs)) {
		const int offs = LocaleData::LI_NUM * sizeof(char *);
		const int sz = f.size() + 1;
		_extTextsTable = (char **)malloc(offs + sz);
		if (_extTextsTable) {
			char *textData = (char *)_extTextsTable + offs;
			f.read(textData, sz);
			textData[sz] = 0;
			int textsCount = 0;
			for (char *eol; (eol = strpbrk(textData, "\r\n")) != 0; ) {
				*eol++ = 0;
				if (*eol == '\r' || *eol == '\n') {
					*eol++ = 0;
				}
				if (textsCount < LocaleData::LI_NUM && textData[0] != 0) {
					_extTextsTable[textsCount] = textData;
					++textsCount;
				}
				textData = eol;
			}
			if (textsCount < LocaleData::LI_NUM && textData[0] != 0) {
				_extTextsTable[textsCount] = textData;
				++textsCount;
			}
			if (textsCount < LocaleData::LI_NUM) {
				free(_extTextsTable);
				_extTextsTable = 0;
			} else {
				_textsTable = (const char **)_extTextsTable;
			}
		}
	}
	if (!_textsTable) {
		switch (_lang) {
		case LANG_FR:
			_textsTable = LocaleData::_textsTableFR;
			break;
		case LANG_EN:
			_textsTable = LocaleData::_textsTableEN;
			break;
		case LANG_DE:
			_textsTable = LocaleData::_textsTableDE;
			break;
		case LANG_SP:
			_textsTable = LocaleData::_textsTableSP;
			break;
		}
	}
}

void Resource::free_TEXT() {
	if (_extTextsTable) {
		free(_extTextsTable);
		_extTextsTable = 0;
	}
	_stringsTable = 0;
	if (_extStringsTable) {
		free(_extStringsTable);
		_extStringsTable = 0;
	}
	_textsTable = 0;
}

void Resource::load(const char *objName, int objType, const char *ext) {
	debug(DBG_RES, "Resource::load('%s', %d)", objName, objType);
	LoadStub loadStub = 0;
	switch (objType) {
	case OT_MBK:
		snprintf(_entryName, sizeof(_entryName), "%s.MBK", objName);
		loadStub = &Resource::load_MBK;
		break;
	case OT_PGE:
		snprintf(_entryName, sizeof(_entryName), "%s.PGE", objName);
		loadStub = &Resource::load_PGE;
		break;
	case OT_PAL:
		snprintf(_entryName, sizeof(_entryName), "%s.PAL", objName);
		loadStub = &Resource::load_PAL;
		break;
	case OT_CT:
		snprintf(_entryName, sizeof(_entryName), "%s.CT", objName);
		loadStub = &Resource::load_CT;
		break;
	case OT_MAP:
		snprintf(_entryName, sizeof(_entryName), "%s.MAP", objName);
		loadStub = &Resource::load_MAP;
		break;
	case OT_SPC:
		snprintf(_entryName, sizeof(_entryName), "%s.SPC", objName);
		loadStub = &Resource::load_SPC;
		break;
	case OT_RP:
		snprintf(_entryName, sizeof(_entryName), "%s.RP", objName);
		loadStub = &Resource::load_RP;
		break;
	case OT_RPC:
		snprintf(_entryName, sizeof(_entryName), "%s.RPC", objName);
		loadStub = &Resource::load_RP;
		break;
	case OT_SPR:
		snprintf(_entryName, sizeof(_entryName), "%s.SPR", objName);
		loadStub = &Resource::load_SPR;
		break;
	case OT_SPRM:
		snprintf(_entryName, sizeof(_entryName), "%s.SPR", objName);
		loadStub = &Resource::load_SPRM;
		break;
	case OT_ICN:
		snprintf(_entryName, sizeof(_entryName), "%s.ICN", objName);
		loadStub = &Resource::load_ICN;
		break;
	case OT_FNT:
		snprintf(_entryName, sizeof(_entryName), "%s.FNT", objName);
		loadStub = &Resource::load_FNT;
		break;
	case OT_OBJ:
		snprintf(_entryName, sizeof(_entryName), "%s.OBJ", objName);
		loadStub = &Resource::load_OBJ;
		break;
	case OT_ANI:
		snprintf(_entryName, sizeof(_entryName), "%s.ANI", objName);
		loadStub = &Resource::load_ANI;
		break;
	case OT_TBN:
		snprintf(_entryName, sizeof(_entryName), "%s.TBN", objName);
		loadStub = &Resource::load_TBN;
		break;
	case OT_CMD:
		snprintf(_entryName, sizeof(_entryName), "%s.CMD", objName);
		loadStub = &Resource::load_CMD;
		break;
	case OT_POL:
		snprintf(_entryName, sizeof(_entryName), "%s.POL", objName);
		loadStub = &Resource::load_POL;
		break;
	case OT_CMP:
		snprintf(_entryName, sizeof(_entryName), "%s.CMP", objName);
		loadStub = &Resource::load_CMP;
		break;
	case OT_OBC:
		snprintf(_entryName, sizeof(_entryName), "%s.OBC", objName);
		loadStub = &Resource::load_OBC;
		break;
	case OT_SPL:
		snprintf(_entryName, sizeof(_entryName), "%s.SPL", objName);
		loadStub = &Resource::load_SPL;
		break;
	case OT_LEV:
		snprintf(_entryName, sizeof(_entryName), "%s.LEV", objName);
		loadStub = &Resource::load_LEV;
		break;
	case OT_SGD:
		snprintf(_entryName, sizeof(_entryName), "%s.SGD", objName);
		loadStub = &Resource::load_SGD;
		break;
	case OT_SPM:
		snprintf(_entryName, sizeof(_entryName), "%s.SPM", objName);
		loadStub = &Resource::load_SPM;
		break;
	default:
		error("Unimplemented Resource::load() type %d", objType);
		break;
	}
	if (ext) {
		snprintf(_entryName, sizeof(_entryName), "%s.%s", objName, ext);
	}
	File f;
	if (f.open(_entryName, "rb", _fs)) {
		assert(loadStub);
		(this->*loadStub)(&f);
		if (f.ioErr()) {
			error("I/O error when reading '%s'", _entryName);
		}
	} else {
		error("Can't open '%s'", _entryName);
	}
}

void Resource::load_CT(File *pf) {
	debug(DBG_RES, "Resource::load_CT()");
	int len = pf->size();
	uint8 *tmp = (uint8 *)malloc(len);
	if (!tmp) {
		error("Unable to allocate CT buffer");
	} else {
		pf->read(tmp, len);
		if (!delphine_unpack((uint8 *)_ctData, tmp, len)) {
			error("Bad CRC for collision data");
		}
		free(tmp);
	}
}

void Resource::load_FNT(File *f) {
	debug(DBG_RES, "Resource::load_FNT()");
	int len = f->size();
	_fnt = (uint8 *)malloc(len);
	if (!_fnt) {
		error("Unable to allocate FNT buffer");
	} else {
		f->read(_fnt, len);
	}
}

void Resource::load_MBK(File *f) {
	debug(DBG_RES, "Resource::load_MBK()");
	int len = f->size();
	_mbk = (uint8 *)malloc(len);
	if (!_mbk) {
		error("Unable to allocate MBK buffer");
	} else {
		f->read(_mbk, len);
	}
}

void Resource::load_ICN(File *f) {
	debug(DBG_RES, "Resource::load_ICN()");
	int len = f->size();
	if (_icnLen == 0) {
		_icn = (uint8 *)malloc(len);
	} else {
		_icn = (uint8 *)realloc(_icn, _icnLen + len);
	}
	if (!_icn) {
		error("Unable to allocate ICN buffer");
	} else {
		f->read(_icn + _icnLen, len);
	}
	_icnLen += len;
}

void Resource::load_SPR(File *f) {
	debug(DBG_RES, "Resource::load_SPR()");
	int len = f->size() - 12;
	_spr1 = (uint8 *)malloc(len);
	if (!_spr1) {
		error("Unable to allocate SPR buffer");
	} else {
		f->seek(12);
		f->read(_spr1, len);
	}
}

void Resource::load_SPRM(File *f) {
	debug(DBG_RES, "Resource::load_SPRM()");
	int len = f->size() - 12;
	f->seek(12);
	f->read(_sprm, len);
}

void Resource::load_RP(File *f) {
	debug(DBG_RES, "Resource::load_RP()");
	f->read(_rp, 0x4A);
}

void Resource::load_SPC(File *f) {
	debug(DBG_RES, "Resource::load_SPC()");
	int len = f->size();
	_spc = (uint8 *)malloc(len);
	if (!_spc) {
		error("Unable to allocate SPC buffer");
	} else {
		f->read(_spc, len);
		_numSpc = READ_BE_UINT16(_spc) / 2;
	}
}

void Resource::load_PAL(File *f) {
	debug(DBG_RES, "Resource::load_PAL()");
	int len = f->size();
	_pal = (uint8 *)malloc(len);
	if (!_pal) {
		error("Unable to allocate PAL buffer");
	} else {
		f->read(_pal, len);
	}
}

void Resource::load_MAP(File *f) {
	debug(DBG_RES, "Resource::load_MAP()");
	int len = f->size();
	_map = (uint8 *)malloc(len);
	if (!_map) {
		error("Unable to allocate MAP buffer");
	} else {
		f->read(_map, len);
	}
}

void Resource::load_OBJ(File *f) {
	debug(DBG_RES, "Resource::load_OBJ()");
	_numObjectNodes = f->readUint16LE();
	assert(_numObjectNodes < 255);
	uint32 offsets[256];
	for (int i = 0; i < _numObjectNodes; ++i) {
		offsets[i] = f->readUint32LE();
	}
	offsets[_numObjectNodes] = f->size() - 2;
	int numObjectsCount = 0;
	uint16 objectsCount[256];
	for (int i = 0; i < _numObjectNodes; ++i) {
		int diff = offsets[i + 1] - offsets[i];
		if (diff != 0) {
			objectsCount[numObjectsCount] = (diff - 2) / 0x12;
			debug(DBG_RES, "i=%d objectsCount[numObjectsCount]=%d", i, objectsCount[numObjectsCount]);
			++numObjectsCount;
		}
	}
	uint32 prevOffset = 0;
	ObjectNode *prevNode = 0;
	int iObj = 0;
	for (int i = 0; i < _numObjectNodes; ++i) {
		if (prevOffset != offsets[i]) {
			ObjectNode *on = (ObjectNode *)malloc(sizeof(ObjectNode));
			if (!on) {
				error("Unable to allocate ObjectNode num=%d", i);
			}
			f->seek(offsets[i] + 2);
			on->last_obj_number = f->readUint16LE();
			on->num_objects = objectsCount[iObj];
			debug(DBG_RES, "last=%d num=%d", on->last_obj_number, on->num_objects);
			on->objects = (Object *)malloc(sizeof(Object) * on->num_objects);
			for (int j = 0; j < on->num_objects; ++j) {
				Object *obj = &on->objects[j];
				obj->type = f->readUint16LE();
				obj->dx = f->readByte();
				obj->dy = f->readByte();
				obj->init_obj_type = f->readUint16LE();
				obj->opcode2 = f->readByte();
				obj->opcode1 = f->readByte();
				obj->flags = f->readByte();
				obj->opcode3 = f->readByte();
				obj->init_obj_number = f->readUint16LE();
				obj->opcode_arg1 = f->readUint16LE();
				obj->opcode_arg2 = f->readUint16LE();
				obj->opcode_arg3 = f->readUint16LE();
				debug(DBG_RES, "obj_node=%d obj=%d op1=0x%X op2=0x%X op3=0x%X", i, j, obj->opcode2, obj->opcode1, obj->opcode3);
			}
			++iObj;
			prevOffset = offsets[i];
			prevNode = on;
		}
		_objectNodesMap[i] = prevNode;
	}
}

void Resource::free_OBJ() {
	debug(DBG_RES, "Resource::free_OBJ()");
	ObjectNode *prevNode = 0;
	for (int i = 0; i < _numObjectNodes; ++i) {
		if (_objectNodesMap[i] != prevNode) {
			ObjectNode *curNode = _objectNodesMap[i];
			free(curNode->objects);
			free(curNode);
			prevNode = curNode;
		}
		_objectNodesMap[i] = 0;
	}
}

void Resource::load_OBC(File *f) {
	const int packedSize = f->readUint32BE();
	uint8 *packedData = (uint8 *)malloc(packedSize);
	if (!packedData) {
		error("Unable to allocate OBC temporary buffer 1");
	}
	f->seek(packedSize);
	const int unpackedSize = f->readUint32BE();
	uint8 *tmp = (uint8 *)malloc(unpackedSize);
	if (!tmp) {
		error("Unable to allocate OBC temporary buffer 2");
	}
	f->seek(4);
	f->read(packedData, packedSize);
	if (!delphine_unpack(tmp, packedData, packedSize)) {
		error("Bad CRC for compressed object data");
	}
	free(packedData);
	uint32 offsets[256];
	int tmpOffset = 0;
	_numObjectNodes = 230;
	for (int i = 0; i < _numObjectNodes; ++i) {
		offsets[i] = READ_BE_UINT32(tmp + tmpOffset); tmpOffset += 4;
	}
	offsets[_numObjectNodes] = unpackedSize;
	int numObjectsCount = 0;
	uint16 objectsCount[256];
	for (int i = 0; i < _numObjectNodes; ++i) {
		int diff = offsets[i + 1] - offsets[i];
		if (diff != 0) {
			objectsCount[numObjectsCount] = (diff - 2) / 0x12;
			++numObjectsCount;
		}
	}
	uint32 prevOffset = 0;
	ObjectNode *prevNode = 0;
	int iObj = 0;
	for (int i = 0; i < _numObjectNodes; ++i) {
		if (prevOffset != offsets[i]) {
			ObjectNode *on = (ObjectNode *)malloc(sizeof(ObjectNode));
			if (!on) {
				error("Unable to allocate ObjectNode num=%d", i);
			}
			const uint8 *objData = tmp + offsets[i];
			on->last_obj_number = READ_BE_UINT16(objData); objData += 2;
			on->num_objects = objectsCount[iObj];
			on->objects = (Object *)malloc(sizeof(Object) * on->num_objects);
			for (int j = 0; j < on->num_objects; ++j) {
				Object *obj = &on->objects[j];
				obj->type = READ_BE_UINT16(objData); objData += 2;
				obj->dx = *objData++;
				obj->dy = *objData++;
				obj->init_obj_type = READ_BE_UINT16(objData); objData += 2;
				obj->opcode2 = *objData++;
				obj->opcode1 = *objData++;
				obj->flags = *objData++;
				obj->opcode3 = *objData++;
				obj->init_obj_number = READ_BE_UINT16(objData); objData += 2;
				obj->opcode_arg1 = READ_BE_UINT16(objData); objData += 2;
				obj->opcode_arg2 = READ_BE_UINT16(objData); objData += 2;
				obj->opcode_arg3 = READ_BE_UINT16(objData); objData += 2;
				debug(DBG_RES, "obj_node=%d obj=%d op1=0x%X op2=0x%X op3=0x%X", i, j, obj->opcode2, obj->opcode1, obj->opcode3);
			}
			++iObj;
			prevOffset = offsets[i];
			prevNode = on;
		}
		_objectNodesMap[i] = prevNode;
	}
	free(tmp);
}

void Resource::load_PGE(File *f) {
	debug(DBG_RES, "Resource::load_PGE()");
	int len = f->size() - 2;
	_pgeNum = f->readUint16LE();
	if (_type == kResourceTypeAmiga) {
		SWAP_UINT16(&_pgeNum);
	}
	memset(_pgeInit, 0, sizeof(_pgeInit));
	debug(DBG_RES, "len=%d _pgeNum=%d", len, _pgeNum);
	for (uint16 i = 0; i < _pgeNum; ++i) {
		InitPGE *pge = &_pgeInit[i];
		pge->type = f->readUint16LE();
		pge->pos_x = f->readUint16LE();
		pge->pos_y = f->readUint16LE();
		pge->obj_node_number = f->readUint16LE();
		pge->life = f->readUint16LE();
		for (int lc = 0; lc < 4; ++lc) {
			pge->counter_values[lc] = f->readUint16LE();
		}
		pge->object_type = f->readByte();
		pge->init_room = f->readByte();
		pge->room_location = f->readByte();
		pge->init_flags = f->readByte();
		pge->colliding_icon_num = f->readByte();
		pge->icon_num = f->readByte();
		pge->object_id = f->readByte();
		pge->skill = f->readByte();
		pge->mirror_x = f->readByte();
		pge->flags = f->readByte();
		pge->unk1C = f->readByte();
		f->readByte();
		pge->text_num = f->readUint16LE();
	}
	if (_type == kResourceTypeAmiga) {
		for (uint16 i = 0; i < _pgeNum; ++i) {
			InitPGE *pge = &_pgeInit[i];
			SWAP_UINT16((uint16 *)&pge->type);
			SWAP_UINT16((uint16 *)&pge->pos_x);
			SWAP_UINT16((uint16 *)&pge->pos_y);
			SWAP_UINT16((uint16 *)&pge->obj_node_number);
			SWAP_UINT16((uint16 *)&pge->life);
			for (int lc = 0; lc < 4; ++lc) {
				SWAP_UINT16((uint16 *)&pge->counter_values[lc]);
			}
			SWAP_UINT16((uint16 *)&pge->text_num);
		}
	}
}

void Resource::load_ANI(File *f) {
	debug(DBG_RES, "Resource::load_ANI()");
	int size = f->size() - 2;
	_ani = (uint8 *)malloc(size);
	if (!_ani) {
		error("Unable to allocate ANI buffer");
	} else {
		uint16 count = f->readUint16LE();
		f->read(_ani, size);
		if (_type == kResourceTypeAmiga) {
			const uint8 *end = _ani + size;
			SWAP_UINT16(&count);
			// byte-swap animation data
			for (uint16 i = 0; i < count; ++i) {
				uint8 *p = _ani + READ_BE_UINT16(_ani + 2 * i);
				// byte-swap offset
				SWAP<uint8>(_ani[2 * i], _ani[2 * i + 1]);
				if (p >= end) {
					continue;
				}
				const int frames = READ_BE_UINT16(p);
				if (p[0] != 0) {
					// byte-swap only once
					continue;
				}
				// byte-swap anim count
				SWAP<uint8>(p[0], p[1]);
				debug(DBG_RES, "ani=%d frames=%d", i, frames);
				for (int j = 0; j < frames; ++j) {
					// byte-swap next frame
					SWAP<uint8>(p[6 + j * 4], p[6 + j * 4 + 1]);
				}
			}
		}
	}
}

void Resource::load_TBN(File *f) {
	debug(DBG_RES, "Resource::load_TBN()");
	int len = f->size();
	_tbn = (uint8 *)malloc(len);
	if (!_tbn) {
		error("Unable to allocate TBN buffer");
	} else {
		f->read(_tbn, len);
	}
	if (_type == kResourceTypeAmiga) {
		const int firstOffset = READ_BE_UINT16(_tbn);
		for (int i = 0; i < firstOffset; i += 2) {
			// byte-swap offset
			SWAP<uint8>(_tbn[i], _tbn[i + 1]);
		}
	}
}

void Resource::load_CMD(File *pf) {
	debug(DBG_RES, "Resource::load_CMD()");
	free(_cmd);
	int len = pf->size();
	_cmd = (uint8 *)malloc(len);
	if (!_cmd) {
		error("Unable to allocate CMD buffer");
	} else {
		pf->read(_cmd, len);
	}
}

void Resource::load_POL(File *pf) {
	debug(DBG_RES, "Resource::load_POL()");
	free(_pol);
	int len = pf->size();
	_pol = (uint8 *)malloc(len);
	if (!_pol) {
		error("Unable to allocate POL buffer");
	} else {
		pf->read(_pol, len);
	}
}

void Resource::load_CMP(File *pf) {
	free(_pol);
	free(_cmd);
	int len = pf->size();
	uint8 *tmp = (uint8 *)malloc(len);
	if (!tmp) {
		error("Unable to allocate CMP buffer");
	}
	pf->read(tmp, len);
	struct {
		int offset, packedSize, size;
	} data[2];
	int offset = 0;
	for (int i = 0; i < 2; ++i) {
		int packedSize = READ_BE_UINT32(tmp + offset); offset += 4;
		assert((packedSize & 1) == 0);
		if (packedSize < 0) {
			data[i].size = packedSize = -packedSize;
		} else {
			data[i].size = READ_BE_UINT32(tmp + offset + packedSize - 4);
		}
		data[i].offset = offset;
		data[i].packedSize = packedSize;
		offset += packedSize;
	}
	_pol = (uint8 *)malloc(data[0].size);
	if (!_pol) {
		error("Unable to allocate POL buffer");
	}
	if (data[0].packedSize == data[0].size) {
		memcpy(_pol, tmp + data[0].offset, data[0].packedSize);
	} else if (!delphine_unpack(_pol, tmp + data[0].offset, data[0].packedSize)) {
		error("Bad CRC for cutscene polygon data");
	}
	_cmd = (uint8 *)malloc(data[1].size);
	if (!_cmd) {
		error("Unable to allocate CMD buffer");
	}
	if (data[1].packedSize == data[1].size) {
		memcpy(_cmd, tmp + data[1].offset, data[1].packedSize);
	} else if (!delphine_unpack(_cmd, tmp + data[1].offset, data[1].packedSize)) {
		error("Bad CRC for cutscene command data");
	}
	free(tmp);
}

void Resource::load_VCE(int num, int segment, uint8 **buf, uint32 *bufSize) {
	*buf = 0;
	int offset = _voicesOffsetsTable[num];
	if (offset != 0xFFFF) {
		const uint16 *p = _voicesOffsetsTable + offset / 2;
		offset = (*p++) * 2048;
		int count = *p++;
		if (segment < count) {
			File f;
			if (f.open("VOICE.VCE", "rb", _fs)) {
				int voiceSize = p[segment] * 2048 / 5;
				uint8 *voiceBuf = (uint8 *)malloc(voiceSize);
				if (voiceBuf) {
					uint8 *dst = voiceBuf;
					offset += 0x2000;
					for (int s = 0; s < count; ++s) {
						int len = p[s] * 2048;
						for (int i = 0; i < len / (0x2000 + 2048); ++i) {
							if (s == segment) {
								f.seek(offset);
								int n = 2048;
								while (n--) {
									int v = f.readByte();
									if (v & 0x80) {
										v = -(v & 0x7F);
									}
									*dst++ = (uint8)(v & 0xFF);
								}
							}
							offset += 0x2000 + 2048;
						}
						if (s == segment) {
							break;
						}
					}
					*buf = voiceBuf;
					*bufSize = voiceSize;
				}
			}
		}
	}
}

void Resource::load_SPL(File *f) {
	for (int i = 0; i < _numSfx; ++i) {
		free(_sfxList[i].data);
	}
	free(_sfxList);
	_numSfx = 66;
	_sfxList = (SoundFx *)calloc(_numSfx, sizeof(SoundFx));
	if (!_sfxList) {
		error("Unable to allocate SoundFx table");
	}
	int offset = 0;
	for (int i = 0; i < _numSfx; ++i) {
		const int size = f->readUint16BE(); offset += 2;
		if ((size & 0x8000) != 0) {
			continue;
		}
		debug(DBG_RES, "sfx=%d size=%d", i, size);
		assert(size != 0 && (size & 1) == 0);
		if (i != 64) {
			_sfxList[i].offset = offset;
			_sfxList[i].len = size;
			_sfxList[i].data = (uint8 *)malloc(size);
			assert(_sfxList[i].data);
			f->read(_sfxList[i].data, size);
		} else {
			f->seek(offset + size);
		}
		offset += size;
	}
}

void Resource::load_LEV(File *f) {
	const int len = f->size();
	_lev = (uint8 *)malloc(len);
	if (!_lev) {
		error("Unable to allocate LEV buffer");
	} else {
		f->read(_lev, len);
	}
}

void Resource::load_SGD(File *f) {
	const int len = f->size();
	f->seek(len - 4);
	int size = f->readUint32BE();
	f->seek(0);
	uint8 *tmp = (uint8 *)malloc(len);
	if (!tmp) {
		error("Unable to allocate SGD temporary buffer");
	}
	f->read(tmp, len);
	_sgd = (uint8 *)malloc(size);
	if (!_sgd) {
		error("Unable to allocate SGD buffer");
	}
	if (!delphine_unpack(_sgd, tmp, len)) {
		error("Bad CRC for SGD data");
	}
	free(tmp);
}

void Resource::load_SPM(File *f) {
	static const int kPersoDatSize = 178647;
	const int len = f->size();
	f->seek(len - 4);
	int size = f->readUint32BE();
	f->seek(0);
	uint8 *tmp = (uint8 *)malloc(len);
	if (!tmp) {
		error("Unable to allocate SPM temporary buffer");
	}
	f->read(tmp, len);
	int sprOffset = 0;
	if (size == kPersoDatSize) {
		_spr1 = (uint8 *)malloc(size);
	} else {
		sprOffset = kPersoDatSize;
		_spr1 = (uint8 *)realloc(_spr1, sprOffset + size);
	}
	if (!_spr1) {
		error("Unable to allocate SPM buffer");
	}
	if (!delphine_unpack(_spr1 + sprOffset, tmp, len)) {
		error("Bad CRC for SPM data");
	}
	free(tmp);
	for (int i = 0; i < 1287; ++i) {
		_spr_off[i] = _spr1 + _spmOffsetsTable[i];
	}
}

void Resource::clearBankData() {
	_bankBuffersCount = 0;
	_bankDataHead = _bankData;
}

int Resource::getBankDataSize(uint16 num) {
	int len = READ_BE_UINT16(_mbk + num * 6 + 4);
	int size = 0;
	switch (_type) {
	case kResourceTypeAmiga:
		if (len & 0x8000) {
			len = -(int16)len;
		}
		size = len * 32;
		break;
	case kResourceTypePC:
		size = (len & 0x7FFF) * 32;
		break;
	}
	return size;
}

uint8 *Resource::findBankData(uint16 num) {
	for (int i = 0; i < _bankBuffersCount; ++i) {
		if (_bankBuffers[i].entryNum == num) {
			return _bankBuffers[i].ptr;
		}
	}
	return 0;
}

uint8 *Resource::loadBankData(uint16 num) {
	const uint8 *ptr = _mbk + num * 6;
	int dataOffset = READ_BE_UINT32(ptr);
	if (_type == kResourceTypePC) {
		// first byte of the data buffer corresponds
		// to the total count of entries
		dataOffset &= 0xFFFF;
	}
	const int size = getBankDataSize(num);
	const int avail = _bankDataTail - _bankDataHead;
	if (avail < size) {
		clearBankData();
	}
	assert(_bankDataHead + size <= _bankDataTail);
	assert(_bankBuffersCount < (int)ARRAYSIZE(_bankBuffers));
	_bankBuffers[_bankBuffersCount].entryNum = num;
	_bankBuffers[_bankBuffersCount].ptr = _bankDataHead;
	const uint8 *data = _mbk + dataOffset;
	if (READ_BE_UINT16(ptr + 4) & 0x8000) {
		memcpy(_bankDataHead, data, size);
	} else {
		assert(dataOffset > 4);
		assert(size == (int)READ_BE_UINT32(data - 4));
		if (!delphine_unpack(_bankDataHead, data, 0)) {
			error("Bad CRC for bank data %d", num);
		}
	}
	uint8 *bankData = _bankDataHead;
	_bankDataHead += size;
	return bankData;
}

