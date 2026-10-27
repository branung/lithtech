// LTGUIMessageBox.cpp: implementation of the CLTGUIMessageBox class.

#include "basedefs_de.h"
#include "cpp_clientshell_de.h"

// For CMoArray, which these LTGUI headers use without including it
#include "stdlith.h"

#include "LTGUICommandHandler.h"
#include "CoolFont.h"
#include "LTGUIFont.h"
#include "LTGUIMessageBox.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLTGUIMessageBox::CLTGUIMessageBox()
{
	m_pClientDE			= DNULL;
	m_hBackgroundSurf	= DNULL;
	m_pFont				= DNULL;
	m_hText				= DNULL;
	m_textColor			= SETRGB(255,255,255);
	m_pCommandHandler	= DNULL;
}

CLTGUIMessageBox::~CLTGUIMessageBox()
{
	Destroy();
}

//////////////////////////////////////////////////////////////////////
// Creation
//////////////////////////////////////////////////////////////////////

DBOOL CLTGUIMessageBox::Create(CClientDE *pClientDE, char *lpszBackground, CLTGUIFont *pFont,
							   HSTRING hText, CLTGUICommandHandler *pCommandHandler)
{
	if (!pClientDE)			return DFALSE;
	if (!lpszBackground)	return DFALSE;

	m_pClientDE = pClientDE;

	m_hBackgroundSurf = pClientDE->CreateSurfaceFromBitmap(lpszBackground);
	if (!m_hBackgroundSurf)
		return DFALSE;

	m_pFont = pFont;

	SetText(hText);

	m_pCommandHandler = pCommandHandler;

	return DTRUE;
}

void CLTGUIMessageBox::Destroy()
{
	if (m_pClientDE && m_hBackgroundSurf)
	{
		m_pClientDE->DeleteSurface(m_hBackgroundSurf);
		m_hBackgroundSurf = DNULL;
	}

	if (m_hText && m_pClientDE)
	{
		m_pClientDE->FreeString(m_hText);
		m_hText = DNULL;
	}

	m_pCommandHandler = DNULL;
	m_pFont           = DNULL;

	RemoveKeys();

	m_pClientDE = DNULL;
}

//////////////////////////////////////////////////////////////////////
// Text and keys
//////////////////////////////////////////////////////////////////////

void CLTGUIMessageBox::SetText(HSTRING hString)
{
	if (!m_pClientDE)
		return;

	if (m_hText)
	{
		m_pClientDE->FreeString(m_hText);
		m_hText = DNULL;
	}

	if (hString)
	{
		m_hText = m_pClientDE->CopyString(hString);
	}
}

void CLTGUIMessageBox::AddKey(int nVKeyCode, DDWORD dwMessageID)
{
	LTGUIKeyMessage keyMessage;

	keyMessage.m_nVKeyCode   = nVKeyCode;
	keyMessage.m_dwMessageID = dwMessageID;

	m_keyBindings.Add(keyMessage);
}

//////////////////////////////////////////////////////////////////////
// Size
//////////////////////////////////////////////////////////////////////

int CLTGUIMessageBox::GetWidth()
{
	if (!m_hBackgroundSurf)
		return 0;

	DDWORD nWidth  = 0;
	DDWORD nHeight = 0;

	m_pClientDE->GetSurfaceDims(m_hBackgroundSurf, &nWidth, &nHeight);

	return (int)nWidth;
}

int CLTGUIMessageBox::GetHeight()
{
	if (!m_hBackgroundSurf)
		return 0;

	DDWORD nWidth  = 0;
	DDWORD nHeight = 0;

	m_pClientDE->GetSurfaceDims(m_hBackgroundSurf, &nWidth, &nHeight);

	return (int)nHeight;
}

//////////////////////////////////////////////////////////////////////
// Rendering
//////////////////////////////////////////////////////////////////////

void CLTGUIMessageBox::Render(HSURFACE hDestSurf, int xPos, int yPos)
{
	if (!m_pClientDE)		return;
	if (!m_hBackgroundSurf)	return;

	DDWORD nSurfWidth  = 0;
	DDWORD nSurfHeight = 0;

	m_pClientDE->GetSurfaceDims(hDestSurf, &nSurfWidth, &nSurfHeight);

	if (xPos == LTGUI_MB_CENTER)
		xPos = (int)(nSurfWidth / 2) - GetWidth() / 2;

	if (yPos == LTGUI_MB_CENTER)
		yPos = (int)(nSurfHeight / 2) - GetHeight() / 2;

	HDECOLOR hTransColor = m_pClientDE->SetupColor1(1.0f, 0.0f, 1.0f, DFALSE);

	m_pClientDE->DrawSurfaceToSurfaceTransparent(hDestSurf, m_hBackgroundSurf, DNULL,
												 xPos, yPos, hTransColor);

	if (!m_pFont)	return;
	if (!m_hText)	return;

	int nTextWidth = GetWidth() - 40;

	DIntPt extents = m_pFont->GetTextExtentsFormat(m_hText, nTextWidth);

	int nX = xPos + GetWidth()  / 2 - extents.x / 2;
	int nY = yPos + GetHeight() / 2 - extents.y / 2 - 10;

	m_pFont->DrawSolidFormat(m_hText, hDestSurf, nX, nY, nTextWidth, m_textColor);
}

//////////////////////////////////////////////////////////////////////
// Input
//////////////////////////////////////////////////////////////////////

DBOOL CLTGUIMessageBox::HandleKeyDown(int key, int rep)
{
	for (DDWORD i = 0; i < m_keyBindings.GetSize(); ++i)
	{
		if (m_keyBindings[i].m_nVKeyCode == key && m_pCommandHandler)
		{
			m_pCommandHandler->SendCommand(m_keyBindings[i].m_dwMessageID, 0, 0);
			return DTRUE;
		}
	}

	return DFALSE;
}
