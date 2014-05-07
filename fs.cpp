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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif
#include "fs.h"


struct FileSystem_impl {
	FileSystem_impl() :
		_fileList(0), _fileCount(0), _filePathLen(0) {
	}

	~FileSystem_impl() {
		for (int i = 0; i < _fileCount; ++i) {
			free(_fileList[i]);
		}
		free(_fileList);
	}

	void setRootDirectory(const char *dir) {
		_filePathLen = strlen(dir) + 1;
		buildFileListFromDirectory(dir);
	}

	const char *findFilePath(const char *file) {
		const int len = strlen(file);
		for (int i = 0; i < _fileCount; ++i) {
			const char *filePath = _fileList[i];
			const int filePathLen = strlen(filePath);
			if (filePathLen > len && strcasecmp(filePath + filePathLen - len, file) == 0) {
				return filePath;
			}
		}
		return 0;
	}

	void addFileToList(const char *filePath) {
		_fileList = (char **)realloc(_fileList, (_fileCount + 1) * sizeof(char *));
		if (_fileList) {
			_fileList[_fileCount] = strdup(filePath);
			++_fileCount;
		}
	}

	void buildFileListFromDirectory(const char *dir);

	char **_fileList;
	int _fileCount;
	int _filePathLen;
};

#ifdef _WIN32
void FileSystem_impl::buildFileListFromDirectory(const char *dir) {
	WIN32_FIND_DATA findData;
	char searchPath[MAX_PATH];
	snprintf(searchPath, sizeof(searchPath), "%s/*", dir);
	HANDLE h = FindFirstFile(searchPath, &findData);
	if (h) {
		do {
			if (findData.cFileName[0] == '.') {
				continue;
			}
			char filePath[MAX_PATH];
			snprintf(filePath, sizeof(filePath), "%s/%s", dir, findData.cFileName);
			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				buildFileListFromDirectory(filePath);
			} else {
				addFileToList(filePath);
			}
		} while (FindNextFile(h, &findData));
		FindClose(h);
	}
}
#else
void FileSystem_impl::buildFileListFromDirectory(const char *dir) {
	DIR *d = opendir(dir);
	if (d) {
		dirent *de;
		while ((de = readdir(d)) != NULL) {
			if (de->d_name[0] == '.') {
				continue;
			}
			char filePath[512];
			snprintf(filePath, sizeof(filePath), "%s/%s", dir, de->d_name);
			struct stat st;
			if (stat(filePath, &st) == 0) {
				if (S_ISDIR(st.st_mode)) {
					buildFileListFromDirectory(filePath);
				} else {
					addFileToList(filePath);
				}
			}
		}
		closedir(d);
	}
}
#endif

FileSystem::FileSystem(const char *dataPath) {
	_impl = new FileSystem_impl;
	_impl->setRootDirectory(dataPath);
}

FileSystem::~FileSystem() {
	delete _impl;
}

const char *FileSystem::findPath(const char *filename) {
	return _impl->findFilePath(filename);
}

