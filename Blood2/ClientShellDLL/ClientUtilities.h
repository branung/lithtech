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

#include "client_de.h"
#include "SharedDefs.h"

struct CSize
{
	CSize()		{ cx = 0; cy = 0; }
	
	unsigned long	cx;
	unsigned long	cy;
};


DBOOL IsRandomChance(int nPercent);
int GetRandom();
int GetRandom(int range);
int GetRandom(int lo, int hi);
float GetRandom(float min, float max);
void PrintError(char *pMsg, ...);

// HUD scaling
// The HUD is fixed pixel art anchored to the screen, so HUDScale enlarges it at high resolutions.
// The clamp has to match the Display menu's slider range

#define HUDSCALE_MIN	1.0f
#define HUDSCALE_MAX	3.0f

DFLOAT GetHUDScale();

// DrawHUDScaled blits at the HUD scale, and HUDScaled scales a layout offset to match.
// CoolFont text (the CommLink, the frag table) has no size to ask for, so it's magnified.
// MessageMgr's font is rebuilt larger instead
void DrawHUDScaled (HSURFACE hDest, HSURFACE hSrc, int x, int y, DFLOAT fScale, HDECOLOR hTransColor);
int  HUDScaled (int n, DFLOAT fScale);


#define INTERPOLATE(a,b,c)  (a*(1.0f-c) + b*c)


HSOUNDDE PlaySoundFromObject( HOBJECT hObject, const char *pSoundName, DFLOAT fRadius, 
						  DBYTE nSoundPriority, DBOOL bLoop = DFALSE, DBOOL bHandle = DFALSE, DBOOL bTime = DFALSE, DBYTE nVolume = 100 );

HSOUNDDE PlaySoundFromPos( DVector *vPos, const char *pSoundName, DFLOAT fRadius, 
						  DBYTE nSoundPriority, DBOOL bLoop = DFALSE, DBOOL bHandle = DFALSE, DBOOL bTime = DFALSE, DBYTE nVolume = 100 );

HSOUNDDE PlaySoundLocal( const char *pSoundName, DBYTE nSoundPriority, 
						DBOOL bLoop = DFALSE, DBOOL bHandle = DFALSE, DBOOL bTime = DFALSE, DBOOL bStream = DFALSE, DBYTE nVolume = 100 );

SurfaceType GetSurfaceType(HOBJECT hObject, HPOLY hPoly);


inline DBOOL IsRandomChance(int percent)
{
	return((rand() % 100) < percent);
}

void TiltVectorToPlane(DVector *pVec, DVector *pNormal);

bool ObjListFilterFn(HLOCALOBJ hTest, void *pUserData);

#endif // __CLIENT_UTILITIES_H__