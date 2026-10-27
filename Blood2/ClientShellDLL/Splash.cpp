/****************************************************************************
;
;	 MODULE:		Splash (.CPP)
;
;	PURPOSE:		Splash screen state functions
;
;	HISTORY:		10/18/98 [blg] This file was created
;
;	COMMENT:		Copyright (c) 1998, Monolith Productions Inc.
;
****************************************************************************/


// Includes...

#include "Splash.h"
#include "BloodClientShell.h"
#include "VkDefs.h"


// Externs...

extern CBloodClientShell* g_pBloodClientShell;


// Statics...

static	HSURFACE	s_hBitmap   = DNULL;
static	DBOOL		s_bExit     = DFALSE;
static	CClientDE*	s_pClientDE = DNULL;


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	Splash_FitRect
//
//	PURPOSE:	The largest centered rectangle with the bitmap's aspect
//				ratio that fits the screen
//
// ----------------------------------------------------------------------- //

static void Splash_FitRect(DDWORD nScreenW, DDWORD nScreenH,
                           DDWORD nBmpW, DDWORD nBmpH, DRect& rcDst)
{
	DDWORD nFitW = nScreenW;
	DDWORD nFitH = nScreenH;

	if (nBmpW > 0 && nBmpH > 0)
	{
		// The screen is wider than the bitmap when screenW/screenH > bmpW/bmpH
		if (nScreenW * nBmpH > nScreenH * nBmpW)
		{
			nFitH = nScreenH;
			nFitW = (nBmpW * nScreenH) / nBmpH;
		}
		else
		{
			nFitW = nScreenW;
			nFitH = (nBmpH * nScreenW) / nBmpW;
		}
	}

	rcDst.left   = (nScreenW - nFitW) / 2;
	rcDst.top    = (nScreenH - nFitH) / 2;
	rcDst.right  = rcDst.left + nFitW;
	rcDst.bottom = rcDst.top  + nFitH;
}


// Functions...

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	Splash_SetState
//
//	PURPOSE:	Sets the splash screen info and sets the GS_SPLASH state
//
// ----------------------------------------------------------------------- //

DBOOL Splash_SetState(CClientDE* pClientDE, char* sScreen, DBOOL bExit)
{
	// Sanity checks...

	if (!pClientDE) return(DFALSE);
	if (!sScreen) return(DFALSE);
	if (!g_pBloodClientShell) return(DFALSE);


	// Set the splash info...

	if (!Splash_SetInfo(pClientDE, sScreen, bExit))
	{
		return(DFALSE);
	}


	// Set the game state...

	if (!g_pBloodClientShell->SetGameState(GS_SPLASH))
	{
		return(DFALSE);
	}


	// All done...

	return(DTRUE);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	Splash_SetInfo
//
//	PURPOSE:	Sets info needed by the splash screen state
//
//	COMMENT:	Call this function before setting the GS_SPLASH state
//
// ----------------------------------------------------------------------- //

DBOOL Splash_SetInfo(CClientDE* pClientDE, char* sScreen, DBOOL bExit)
{
	// Sanity checks...

	if (!pClientDE) return(DFALSE);
	if (!sScreen) return(DFALSE);


	// Terminate any previous info...

	Splash_Term();


	// Set simple members...

	s_pClientDE = pClientDE;
	s_bExit     = bExit;


	// Create the bitmap surface for our screen...

	s_hBitmap = s_pClientDE->CreateSurfaceFromBitmap(sScreen);	
	if (!s_hBitmap) return(DFALSE);


	// All done...

	return(DTRUE);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	Splash_Term
//
//	PURPOSE:	Terminates splash state stuff
//
// ----------------------------------------------------------------------- //

void Splash_Term()
{
	if (s_pClientDE && s_hBitmap)
	{
		s_pClientDE->DeleteSurface(s_hBitmap);
		s_hBitmap = DNULL;
	}

	if (s_bExit && s_pClientDE)
	{
		s_pClientDE->Shutdown();
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	Splash_Update
//
//	PURPOSE:	Updates splash state stuff
//
// ----------------------------------------------------------------------- //

void Splash_Update()
{
	// Sanity checks...

	if (!s_pClientDE) return;
	if (!s_hBitmap) return;


	// Get the screen surface...

	HSURFACE hScreen = s_pClientDE->GetScreenSurface();
	if (!hScreen) return;


	// Draw our bitmap to the screen, scaling as necessary...

	DDWORD nScreenW = 0, nScreenH = 0, nBmpW = 0, nBmpH = 0;

	s_pClientDE->GetSurfaceDims(hScreen, &nScreenW, &nScreenH);
	s_pClientDE->GetSurfaceDims(s_hBitmap, &nBmpW, &nBmpH);

	DRect rcSrc;
	rcSrc.left   = 0;
	rcSrc.top    = 0;
	rcSrc.right  = nBmpW;
	rcSrc.bottom = nBmpH;

	// Fit the bitmap without stretching it. 1998 drew it to the whole screen, which only fits at 640x480
	DRect rcDst;
	Splash_FitRect(nScreenW, nScreenH, nBmpW, nBmpH, rcDst);

	// Cleared every frame, as the fitted rectangle doesn't cover the screen
	s_pClientDE->ClearScreen(DNULL, CLEARSCREEN_SCREEN);

	s_pClientDE->Start3D();
	s_pClientDE->StartOptimized2D();
	s_pClientDE->ScaleSurfaceToSurface(hScreen, s_hBitmap, &rcDst, &rcSrc);
	s_pClientDE->EndOptimized2D();
	s_pClientDE->End3D();
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	Splash_OnKeyDown
//
//	PURPOSE:	Handles key down events
//
// ----------------------------------------------------------------------- //

void Splash_OnKeyDown(int nKey)
{
	switch (nKey)
	{
		case VK_ESCAPE:
		case VK_SPACE:
		case VK_RETURN:
		{
			if (s_bExit)
			{
				Splash_Term();
			}
			else
			{
				if (g_pBloodClientShell)
				{
					g_pBloodClientShell->SetGameState(GS_MENU);
				}
			}
		}
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	Splash_Display
//
//	PURPOSE:	Displays the given screen
//
// ----------------------------------------------------------------------- //

HSURFACE Splash_Display(CClientDE* pClientDE, char* sScreen, DBOOL bStretch)
{
	// Sanity checks...

	if (!pClientDE) return(DNULL);
	if (!sScreen) return(DNULL);


	// Get the screen surface...

	HSURFACE hScreen = pClientDE->GetScreenSurface();
	if (!hScreen) return(DNULL);


	// Load the screen

	HSURFACE hBitmap = pClientDE->CreateSurfaceFromBitmap(sScreen);	
	if (!hBitmap) return(DNULL);


	// Draw our bitmap to the screen...

	if (bStretch)
	{
		DDWORD nScreenW = 0, nScreenH = 0, nBmpW = 0, nBmpH = 0;

		pClientDE->GetSurfaceDims(hScreen, &nScreenW, &nScreenH);
		pClientDE->GetSurfaceDims(hBitmap, &nBmpW, &nBmpH);

		DRect rcSrc;
		rcSrc.left   = 0;
		rcSrc.top    = 0;
		rcSrc.right  = nBmpW;
		rcSrc.bottom = nBmpH;

		// Fit the bitmap without stretching it, as above
		DRect rcDst;
		Splash_FitRect(nScreenW, nScreenH, nBmpW, nBmpH, rcDst);

		pClientDE->ClearScreen(DNULL, CLEARSCREEN_SCREEN);
		pClientDE->Start3D();
		pClientDE->StartOptimized2D();
		pClientDE->ScaleSurfaceToSurface(hScreen, hBitmap, &rcDst, &rcSrc);
		pClientDE->EndOptimized2D();
		pClientDE->End3D();
	}
	else
	{
		pClientDE->ClearScreen(DNULL, CLEARSCREEN_SCREEN);
		pClientDE->Start3D();
		pClientDE->DrawSurfaceToSurface(hScreen, hBitmap, NULL, 0, 0);
		pClientDE->End3D();
	}

	pClientDE->FlipScreen(FLIPSCREEN_CANDRAWCONSOLE);


	// All done...

	return(hBitmap);
}
