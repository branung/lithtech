/****************************************************************************
;
;	 MODULE:		CreditsWin (.CPP)
;
;	PURPOSE:		Credits class - Windows specific stuff
;
;	HISTORY:		07/28/98 [blg] This file was created
;
;	COMMENT:		Copyright (c) 1998, Monolith Productions Inc.
;
****************************************************************************/


// Includes...

#ifdef _WIN32
#include "Windows.h"
#endif
#include "CreditsWin.h"
#include "clientheaders.h"
#include <stdio.h>


// Externs...

extern void* g_hLTDLLInstance;


#ifdef _WIN32

// Statics...

HMODULE s_hModule = NULL;
HRSRC   s_hRes    = NULL;
HGLOBAL	s_hGlobal = NULL;
char*	s_sBuf    = NULL;


// Functions...

char* CreditsWin_GetTextBuffer(char* sName)
{
	//if (s_sBuf)
	//{
		//return(s_sBuf);
	//}
	//else
	{
		void* hModule;
		g_pLTClient->GetEngineHook("cres_hinstance",&hModule);
		s_hModule = (HINSTANCE)hModule;
		if (!s_hModule)	return(NULL);

		s_hRes = FindResource(s_hModule, sName, "TEXT");
		if (!s_hRes) return(NULL);

		s_hGlobal = LoadResource(s_hModule, s_hRes);
		if (!s_hGlobal) return(NULL);

		s_sBuf = (char*)LockResource(s_hGlobal);
		if (!s_sBuf) return(NULL);

		return(s_sBuf);
	}
}

#else // !_WIN32

/*
	Reads the credits and intro text off disk

	On Linux the .rc CREDITS entries aren't resources (DynRes only carries string tables).
	So credits.txt, intro.txt, DemoInfo.txt, DemoIntro.txt and DemoMulti.txt are read from the working directory.
*/
static const char* ResourceNameToFilename(const char* sName)
{
	if (stricmp(sName, "CREDITS") == 0)     return "credits.txt";
	if (stricmp(sName, "INTRO") == 0)       return "intro.txt";
	if (stricmp(sName, "DEMOINFO") == 0)    return "DemoInfo.txt";
	if (stricmp(sName, "DEMOINTRO") == 0)   return "DemoIntro.txt";
	if (stricmp(sName, "DEMOMULTI") == 0)   return "DemoMulti.txt";
	return nullptr;
}

char* CreditsWin_GetTextBuffer(char* sName)
{
	const char* strFilename = ResourceNameToFilename(sName);
	if (!strFilename) return NULL;

	FILE* pFile = fopen(strFilename, "rb");
	if (!pFile) return NULL;

	fseek(pFile, 0, SEEK_END);
	long nSize = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);
	if (nSize <= 0)
	{
		fclose(pFile);
		return NULL;
	}

	char* sBuf = new char[nSize + 1];
	size_t nRead = fread(sBuf, 1, nSize, pFile);
	sBuf[nRead] = '\0';

	fclose(pFile);
	return sBuf;
}

#endif // _WIN32
