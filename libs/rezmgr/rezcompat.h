/****************************************************************************
Implementations of the MSVC CRT calls RezMgr is written against
(_splitpath, _mkdir _findfirst)
****************************************************************************/

#ifndef __REZCOMPAT_H__
#define __REZCOMPAT_H__

#ifndef _WIN32

#include <ctype.h>
#include <dirent.h>
#include <fnmatch.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

// The MSVC path-component limits. Callers size their buffers off these.
const unsigned int _MAX_DRIVE = 3;
const unsigned int _MAX_DIR   = 256;
const unsigned int _MAX_FNAME = 256;
const unsigned int _MAX_EXT   = 256;

// A path separator for the host filesystem
#define REZ_PATH_SEPARATOR '/'
#define REZ_PATH_SEPARATOR_STR "/"

// True for either separator, since rez tooling passes DOS paths around and
// a .rez directory name may carry backslashes
inline bool RezIsPathSep(char c) { return c == '/' || c == '\\'; }

// MSVC's _splitpath
inline void _splitpath(const char* path, char* drive, char* dir, char* fname, char* ext)
{
	if (drive != NULL) drive[0] = '\0';
	if (dir   != NULL) dir[0]   = '\0';
	if (fname != NULL) fname[0] = '\0';
	if (ext   != NULL) ext[0]   = '\0';
	if (path  == NULL) return;

	const char* pCursor = path;

	// A drive letter, if there is one.
	if (path[0] != '\0' && path[1] == ':')
	{
		if (drive != NULL)
		{
			drive[0] = path[0];
			drive[1] = ':';
			drive[2] = '\0';
		}
		pCursor = &path[2];
	}

	// Everything up to and including the last separator is the directory.
	const char* pLastSep = NULL;
	for (const char* p = pCursor; *p != '\0'; ++p)
	{
		if (RezIsPathSep(*p)) pLastSep = p;
	}

	const char* pName = pCursor;
	if (pLastSep != NULL)
	{
		pName = pLastSep + 1;
		if (dir != NULL)
		{
			size_t nDirLen = (size_t)(pName - pCursor);
			if (nDirLen > _MAX_DIR) nDirLen = _MAX_DIR;
			memcpy(dir, pCursor, nDirLen);
			dir[nDirLen] = '\0';
		}
	}

	// The last dot in the name splits stem from extension
	const char* pDot = NULL;
	for (const char* p = pName; *p != '\0'; ++p)
	{
		if (*p == '.' && p != pName) pDot = p;
	}

	const char* pNameEnd = (pDot != NULL) ? pDot : (pName + strlen(pName));

	if (fname != NULL)
	{
		size_t nNameLen = (size_t)(pNameEnd - pName);
		if (nNameLen > _MAX_FNAME) nNameLen = _MAX_FNAME;
		memcpy(fname, pName, nNameLen);
		fname[nNameLen] = '\0';
	}

	if (ext != NULL && pDot != NULL)
	{
		size_t nExtLen = strlen(pDot);
		if (nExtLen > _MAX_EXT) nExtLen = _MAX_EXT;
		memcpy(ext, pDot, nExtLen);
		ext[nExtLen] = '\0';
	}
}

// MSVC's _mkdir
inline int _mkdir(const char* path)
{
	if (path == NULL) return -1;
	return mkdir(path, 0777);
}


struct _finddata_t {
	unsigned int  attrib;
	time_t        time_create;
	time_t        time_access;
	time_t        time_write;
	unsigned long size;
	char          name[260];
	
};

// _A_SUBDIR keeps the value the old stub block gave it
enum { _S_IFDIR, _A_SUBDIR };

// Searches nest, since CreateDir recurses while its own search is still open,
// so handles are indices into a table.
struct RezFindState {
	DIR* pDir;
	char sDirPath[1024];    // includes the trailing separator, or empty
	char sPattern[256];
	bool bMatchAll;         // "*.*"
};

const int kRezMaxFinds = 32;

inline RezFindState* RezFindSlots()
{
	static RezFindState s_Slots[kRezMaxFinds] = {};
	return s_Slots;
}

inline bool RezFindFill(RezFindState* pState, _finddata_t* pInfo)
{
	struct dirent* pEntry;
	while ((pEntry = readdir(pState->pDir)) != NULL)
	{
		if (!pState->bMatchAll &&
			fnmatch(pState->sPattern, pEntry->d_name, FNM_CASEFOLD) != 0)
		{
			continue;
		}

		// Sized to hold sDirPath and a name of any length dirent can report.
		char sFullPath[sizeof(pState->sDirPath) + sizeof(pInfo->name)];
		snprintf(sFullPath, sizeof(sFullPath), "%s%s", pState->sDirPath, pEntry->d_name);

		struct stat statBuf;
		if (stat(sFullPath, &statBuf) != 0) continue;

		memset(pInfo, 0, sizeof(*pInfo));
		pInfo->attrib      = S_ISDIR(statBuf.st_mode) ? (unsigned int)_A_SUBDIR : 0u;
		pInfo->time_create = statBuf.st_ctime;
		pInfo->time_access = statBuf.st_atime;
		pInfo->time_write  = statBuf.st_mtime;
		pInfo->size        = (unsigned long)statBuf.st_size;
		strncpy(pInfo->name, pEntry->d_name, sizeof(pInfo->name) - 1);
		pInfo->name[sizeof(pInfo->name) - 1] = '\0';
		return true;
	}
	return false;
}

inline intptr_t _findfirst(const char* filespec, _finddata_t* fileinfo)
{
	if (filespec == NULL || fileinfo == NULL) return -1;

	int nSlot = 0;
	RezFindState* pSlots = RezFindSlots();
	while (nSlot < kRezMaxFinds && pSlots[nSlot].pDir != NULL) ++nSlot;
	if (nSlot == kRezMaxFinds) return -1;

	RezFindState* pState = &pSlots[nSlot];
	memset(pState, 0, sizeof(*pState));

	const char* pLastSep = NULL;
	for (const char* p = filespec; *p != '\0'; ++p)
	{
		if (RezIsPathSep(*p)) pLastSep = p;
	}

	const char* pPattern = filespec;
	const char* pOpenDir = ".";
	char sDirOnly[1024];
	if (pLastSep != NULL)
	{
		pPattern = pLastSep + 1;
		size_t nDirLen = (size_t)(pLastSep - filespec);
		if (nDirLen >= sizeof(sDirOnly)) return -1;
		memcpy(sDirOnly, filespec, nDirLen);
		sDirOnly[nDirLen] = '\0';

		pOpenDir = (nDirLen > 0) ? sDirOnly : "/";

		size_t nKeep = (size_t)(pPattern - filespec);
		if (nKeep >= sizeof(pState->sDirPath)) return -1;
		memcpy(pState->sDirPath, filespec, nKeep);
		pState->sDirPath[nKeep] = '\0';
	}

	strncpy(pState->sPattern, pPattern, sizeof(pState->sPattern) - 1);
	pState->sPattern[sizeof(pState->sPattern) - 1] = '\0';
	pState->bMatchAll = (strcmp(pState->sPattern, "*.*") == 0) || (strcmp(pState->sPattern, "*") == 0) || (pState->sPattern[0] == '\0');

	pState->pDir = opendir(pOpenDir);
	if (pState->pDir == NULL) return -1;

	if (!RezFindFill(pState, fileinfo))
	{
		closedir(pState->pDir);
		pState->pDir = NULL;
		return -1;
	}

	return (intptr_t)nSlot;
}

inline int _findnext(intptr_t handle, _finddata_t* fileinfo)
{
	if (handle < 0 || handle >= kRezMaxFinds || fileinfo == NULL) return -1;
	RezFindState* pState = &RezFindSlots()[handle];
	if (pState->pDir == NULL) return -1;
	return RezFindFill(pState, fileinfo) ? 0 : -1;
}

inline int _findclose(intptr_t handle)
{
	if (handle < 0 || handle >= kRezMaxFinds) return -1;
	RezFindState* pState = &RezFindSlots()[handle];
	if (pState->pDir == NULL) return -1;
	closedir(pState->pDir);
	pState->pDir = NULL;
	return 0;
}

#else // _WIN32

#define REZ_PATH_SEPARATOR '\\'
#define REZ_PATH_SEPARATOR_STR "\\"
inline bool RezIsPathSep(char c) { return c == '/' || c == '\\'; }

#endif // !_WIN32

#endif // __REZCOMPAT_H__
