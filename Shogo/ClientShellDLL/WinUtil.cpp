#ifdef _WIN32
#include "Windows.h"
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include "stdio.h"
#include "WinUtil.h"
#include <time.h>
#include "clientheaders.h"

#ifdef _WIN32

BOOL CWinUtil::GetMoviesPath (char* strPath)
{
	char chDrive   = 'A';
	strPath[0] = '\0';

	char strTemp[256];
	if (_getcwd (strTemp, 255))
	{
		char strFile[270];
		if (strTemp[strlen(strTemp) - 1] != '\\') strcat (strTemp, "\\");
		SAFE_STRCPY(strFile, strTemp);
		strcat (strFile, "intro.smk");

		if (FileExist (strFile))
		{
			SAFE_STRCPY(strPath, strTemp);
			return TRUE;
		}
	}

	while (chDrive <= 'Z')
	{
		sprintf(strPath, "%c:\\", chDrive);
			
		if (GetDriveType(strPath) == DRIVE_CDROM)
		{
			strcat(strPath, "Movies\\");
			if (DirExist (strPath)) return TRUE;
		}

		chDrive++;
	}

	strPath[0] = '\0';

	return FALSE;
}

BOOL CWinUtil::DirExist (char* strPath)
{
	if (!strPath || !*strPath) return FALSE;

	BOOL bDirExists = FALSE;

	BOOL bRemovedBackSlash = FALSE;
	if (strPath[strlen(strPath) - 1] == '\\')
	{
		strPath[strlen(strPath) - 1] = '\0';
		bRemovedBackSlash = TRUE;
	}

	UINT oldErrorMode = SetErrorMode (SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
	struct stat statbuf;
	int error = stat (strPath, &statbuf);
	SetErrorMode (oldErrorMode);
	if (error != -1) bDirExists = TRUE;

	if (bRemovedBackSlash)
	{
		strPath[strlen(strPath)] = '\\';
	}

	return bDirExists;
}

BOOL CWinUtil::CreateDir (char* strPath)
{
	if (DirExist (strPath)) return TRUE;
	if (strPath[strlen(strPath) - 1] == ':') return FALSE;		// special case

	char strPartialPath[MAX_PATH];
	strPartialPath[0] = '\0';

	char* token = strtok (strPath, "\\");
	while (token)
	{
		strcat (strPartialPath, token);
		if (!DirExist (strPartialPath) && strPartialPath[strlen(strPartialPath) - 1] != ':')
		{
			if (!CreateDirectory (strPartialPath, NULL)) return FALSE;
		}
		strcat (strPartialPath, "\\");
		token = strtok (NULL, "\\");
	}

	return TRUE;
}

BOOL CWinUtil::FileExist (char* strPath)
{
	OFSTRUCT ofs;
	HFILE hFile = OpenFile (strPath, &ofs, OF_EXIST);
	if (hFile == HFILE_ERROR) return FALSE;

	return TRUE;
}

DWORD CWinUtil::WinGetPrivateProfileString (char* lpAppName, char* lpKeyName, char* lpDefault, char* lpReturnedString, DWORD nSize, char* lpFileName)
{
	return GetPrivateProfileString (lpAppName, lpKeyName, lpDefault, lpReturnedString, nSize, lpFileName);
}

DWORD CWinUtil::WinWritePrivateProfileString (char* lpAppName, char* lpKeyName, char* lpString, char* lpFileName)
{
	return WritePrivateProfileString (lpAppName, lpKeyName, lpString, lpFileName);
}

void CWinUtil::DebugOut (char* str)
{
	OutputDebugString (str);
}

void CWinUtil::DebugBreak()
{
	::DebugBreak();
}

float CWinUtil::GetTime()
{
	return (float)GetTickCount() / 1000.0f;
}

char* CWinUtil::GetFocusWindow()
{
	static char strText[128];

	HWND hWnd = GetFocus();
	if (!hWnd)
	{
		hWnd = GetForegroundWindow();
		if (!hWnd) return NULL;
	}

	GetWindowText (hWnd, strText, 127);
	return strText;
}

#else // !_WIN32

// No CD autodetection on Linux, and an empty path means no movies
BOOL CWinUtil::GetMoviesPath (char* strPath)
{
	strPath[0] = '\0';
	return FALSE;
}

// CreateDir can pass an "A\B\" partial path, as such separators are normalized before stat()
static void NormalizeSlashesInPlace(char* strPath)
{
	for (char* p = strPath; *p; ++p)
		if (*p == '\\') *p = '/';
}

BOOL CWinUtil::DirExist (char* strPath)
{
	if (!strPath || !*strPath) return FALSE;

	char strNormalized[MAX_PATH];
	SAFE_STRCPY(strNormalized, strPath);
	NormalizeSlashesInPlace(strNormalized);

	size_t nLen = strlen(strNormalized);
	if (nLen > 0 && strNormalized[nLen - 1] == '/')
		strNormalized[nLen - 1] = '\0';

	struct stat statbuf;
	return (stat(strNormalized, &statbuf) != -1) ? TRUE : FALSE;
}

BOOL CWinUtil::CreateDir (char* strPath)
{
	if (DirExist (strPath)) return TRUE;
	if (strPath[strlen(strPath) - 1] == ':') return FALSE; // special case (drive letter, never applies on Linux)

	char strPartialPath[MAX_PATH];
	strPartialPath[0] = '\0';

	char* token = strtok (strPath, "\\");
	while (token)
	{
		strcat (strPartialPath, token);
		if (!DirExist (strPartialPath) && strPartialPath[strlen(strPartialPath) - 1] != ':')
		{
			char strNative[MAX_PATH];
			SAFE_STRCPY(strNative, strPartialPath);
			NormalizeSlashesInPlace(strNative);
			if (mkdir(strNative, 0755) != 0) return FALSE;
		}
		strcat (strPartialPath, "\\");
		token = strtok (NULL, "\\");
	}

	return TRUE;
}

BOOL CWinUtil::FileExist (char* strPath)
{
	char strNormalized[MAX_PATH];
	SAFE_STRCPY(strNormalized, strPath);
	NormalizeSlashesInPlace(strNormalized);

	return (access(strNormalized, F_OK) == 0) ? TRUE : FALSE;
}

static void GetIniPath(const char* lpFileName, char* out, size_t outSize)
{
	LTStrCpy(out, lpFileName, outSize);
	NormalizeSlashesInPlace(out);
}

DWORD CWinUtil::WinGetPrivateProfileString (char* lpAppName, char* lpKeyName, char* lpDefault, char* lpReturnedString, DWORD nSize, char* lpFileName)
{
	lpReturnedString[0] = '\0';

	char strPath[MAX_PATH];
	GetIniPath(lpFileName, strPath, sizeof(strPath));

	FILE* pFile = fopen(strPath, "r");
	if (pFile)
	{
		char strLine[512];
		bool bInSection = false;
		while (fgets(strLine, sizeof(strLine), pFile))
		{
			// Strip trailing newline.
			size_t nLen = strlen(strLine);
			while (nLen > 0 && (strLine[nLen-1] == '\n' || strLine[nLen-1] == '\r'))
				strLine[--nLen] = '\0';

			if (strLine[0] == '[')
			{
				char* pClose = strchr(strLine, ']');
				if (pClose) *pClose = '\0';
				bInSection = (stricmp(strLine + 1, lpAppName) == 0);
				continue;
			}

			if (!bInSection) continue;

			char* pEquals = strchr(strLine, '=');
			if (!pEquals) continue;
			*pEquals = '\0';

			if (stricmp(strLine, lpKeyName) == 0)
			{
				LTStrCpy(lpReturnedString, pEquals + 1, nSize);
				fclose(pFile);
				return (DWORD)strlen(lpReturnedString);
			}
		}
		fclose(pFile);
	}

	if (lpDefault)
		LTStrCpy(lpReturnedString, lpDefault, nSize);
	return (DWORD)strlen(lpReturnedString);
}

DWORD CWinUtil::WinWritePrivateProfileString (char* lpAppName, char* lpKeyName, char* lpString, char* lpFileName)
{
	char strPath[MAX_PATH];
	GetIniPath(lpFileName, strPath, sizeof(strPath));

	// Read the file line by line, replacing or appending the key in its section
	char strLines[256][512];
	int nNumLines = 0;
	bool bReplaced = false;
	int nSectionLine = -1;

	FILE* pFile = fopen(strPath, "r");
	if (pFile)
	{
		bool bInSection = false;
		while (nNumLines < 256 && fgets(strLines[nNumLines], sizeof(strLines[0]), pFile))
		{
			char* pLine = strLines[nNumLines];
			size_t nLen = strlen(pLine);
			while (nLen > 0 && (pLine[nLen-1] == '\n' || pLine[nLen-1] == '\r'))
				pLine[--nLen] = '\0';

			if (pLine[0] == '[')
			{
				char strSection[256];
				LTStrCpy(strSection, pLine + 1, sizeof(strSection));
				char* pClose = strchr(strSection, ']');
				if (pClose) *pClose = '\0';
				bInSection = (stricmp(strSection, lpAppName) == 0);
				if (bInSection) nSectionLine = nNumLines;
			}
			else if (bInSection)
			{
				char strKey[256];
				LTStrCpy(strKey, pLine, sizeof(strKey));
				char* pEquals = strchr(strKey, '=');
				if (pEquals)
				{
					*pEquals = '\0';
					if (stricmp(strKey, lpKeyName) == 0)
					{
						sprintf(pLine, "%s=%s", lpKeyName, lpString ? lpString : "");
						bReplaced = true;
					}
				}
			}

			nNumLines++;
		}
		fclose(pFile);
	}

	if (!bReplaced && nNumLines < 256)
	{
		if (nSectionLine < 0)
		{
			sprintf(strLines[nNumLines++], "[%s]", lpAppName);
			nSectionLine = nNumLines - 1;
		}
		if (nNumLines < 256)
			sprintf(strLines[nNumLines++], "%s=%s", lpKeyName, lpString ? lpString : "");
	}

	pFile = fopen(strPath, "w");
	if (!pFile) return 0;

	for (int i = 0; i < nNumLines; i++)
		fprintf(pFile, "%s\n", strLines[i]);

	fclose(pFile);
	return 1;
}

void CWinUtil::DebugOut (char* str)
{
	fprintf(stderr, "%s", str);
}

void CWinUtil::DebugBreak()
{
	
}

float CWinUtil::GetTime()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (float)ts.tv_sec + (float)ts.tv_nsec / 1e9f;
}

// Window focus means nothing under SDL's single game window
char* CWinUtil::GetFocusWindow()
{
	return NULL;
}

#endif // _WIN32

void CWinUtil::WriteToDebugFile (char *strText)
{
#ifdef _WIN32
	FILE* pFile = fopen ("c:\\shodebug.txt", "a+t");
#else
	FILE* pFile = fopen ("shodebug.txt", "a+t");
#endif
	if (!pFile) return;

	time_t seconds;
	time (&seconds);
	struct tm* timedate = localtime (&seconds);
	if (!timedate) return;
	
	char strTimeDate[128];
	sprintf (strTimeDate, "[%02d/%02d/%02d %02d:%02d:%02d]  ", timedate->tm_mon + 1, timedate->tm_mday, (timedate->tm_year + 1900) % 100, timedate->tm_hour, timedate->tm_min, timedate->tm_sec);
	fwrite (strTimeDate, strlen(strTimeDate), 1, pFile);

	fwrite (strText, strlen(strText), 1, pFile);
	fwrite ("\n", 1, 1, pFile);

	fclose (pFile);
}

