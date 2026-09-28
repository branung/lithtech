// ----------------------------------------------------------------------- //
//
// MODULE  : ClientUtilities.h
//
// PURPOSE : Utility functions
//
// CREATED : 9/25/97
//
// ----------------------------------------------------------------------- //

#ifndef __CLIENT_UTILITIES_H__
#define __CLIENT_UTILITIES_H__

#include "RiotSoundTypes.h"
#include "clientheaders.h"
#include "RiotCommonUtilities.h"

#define NUM_COMMANDS 28

struct CommandID
{
	int		nStringID;
	int		nCommandID;
};

struct CSize
{
	CSize()		{ cx = 0; cy = 0; }
	
	uint32	cx;
	uint32	cy;
};

int CommandToArrayPos(int nCommand);
char* CommandName(int nCommand);

HLTSOUND PlaySoundFromObject( HOBJECT hObject, char *pSoundName, LTFLOAT fRadius, uint8 nSoundPriority, 
							 LTBOOL bLoop = LTFALSE, LTBOOL bHandle = LTFALSE, LTBOOL bTime = LTFALSE, uint8 nVolume = 100 );
HLTSOUND PlaySoundFromPos( LTVector *vPos, char *pSoundName, LTFLOAT fRadius, uint8 nSoundPriority, 
						  LTBOOL bLoop = LTFALSE, LTBOOL bHandle = LTFALSE, LTBOOL bTime = LTFALSE, uint8 nVolume = 100 );

HLTSOUND PlaySoundLocal( char *pSoundName, uint8 nSoundPriority, LTBOOL bLoop = LTFALSE, LTBOOL bHandle = LTFALSE, uint8 nVolume = 100, LTBOOL bReverb = LTFALSE );

HSURFACE CropSurface ( HSURFACE hSurf, HLTCOLOR hBorderColor );

#define HUDSCALE_MIN	1.0f
#define HUDSCALE_MAX	3.0f

LTFLOAT GetHUDScale();

struct FONT;

void DrawHUDScaled (HSURFACE hDest, HSURFACE hSrc, LTRect* pSrcRect, int x, int y,
					LTFLOAT fScale, HLTCOLOR hTransColor);

void DrawHUDScaledSolidColor (HSURFACE hDest, HSURFACE hSrc, LTRect* pSrcRect, int x, int y,
							  LTFLOAT fScale, HLTCOLOR hTransColor, HLTCOLOR hFillColor);

void DrawHUDScaledOpaque (HSURFACE hDest, HSURFACE hSrc, LTRect* pSrcRect, int x, int y,
						  LTFLOAT fScale);

int  HUDScaled (int n, LTFLOAT fScale);

void PlaceScaledPanel (uint32 nScreenW, uint32 nScreenH,
					   uint32 nPanelW, uint32 nPanelH,
					   int nDesignX, int nDesignY,
					   LTFLOAT fScale, int* pX, int* pY);
void ScaleFontDef (FONT* pFontDef, LTFLOAT fScale);

LTFLOAT GetServerConVarFloat( const char *pName, LTFLOAT fDefault = 0.0f );
char*   GetServerConVarString( const char *pName );

HLTSOUND PlaySoundInfoLocal( PlaySoundInfo *pPlaySoundInfo );
bool     IsSoundFinished( HLTSOUND hSound );

#endif // __CLIENT_UTILITIES_H__