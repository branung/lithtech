// LTGUIEditCtrl.cpp: implementation of the CLTGUIEditCtrl class.

#include "basedefs_de.h"
#include "cpp_clientshell_de.h"

#include "LTGUICommandHandler.h"
#include "LTGUICtrl.h"
#include "LTGUIFadeColorCtrl.h"
#include "CoolFont.h"
#include "LTGUIFont.h"
#include "LTGUIEditCtrl.h"

#include <string.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLTGUIEditCtrl::CLTGUIEditCtrl()
{
	m_dwCommandID		= 0;
	m_pFont				= DNULL;
	m_lpszString		= DNULL;
	m_hDescription		= DNULL;
	m_nAlignment		= CF_JUSTIFY_LEFT;
	m_nTextOffset		= 0;
	m_nBufferSize		= 0;
	m_pStringValue		= DNULL;
	m_pCommandHandler	= DNULL;
}

CLTGUIEditCtrl::~CLTGUIEditCtrl()
{
	Destroy();
}

//////////////////////////////////////////////////////////////////////
// Creation
//////////////////////////////////////////////////////////////////////

DBOOL CLTGUIEditCtrl::Create(CClientDE *pClientDE, DDWORD dwCommandID, HSTRING hDescription,
							 CLTGUIFont *pFont, int nTextOffset, int nBufferSize,
							 CLTGUICommandHandler *pCommandHandler, char *pStringValue)
{
	if (!pClientDE)			return DFALSE;
	if (!pFont)				return DFALSE;
	if (nBufferSize < 0)	return DFALSE;

	m_pClientDE			= pClientDE;
	m_dwCommandID		= dwCommandID;
	m_pFont				= pFont;
	m_nTextOffset		= nTextOffset;
	m_nBufferSize		= nBufferSize;
	m_pCommandHandler	= pCommandHandler;
	m_pStringValue		= pStringValue;

	if (hDescription)
	{
		m_hDescription = m_pClientDE->CopyString(hDescription);
	}

	m_lpszString = new char[m_nBufferSize];
	memset(m_lpszString, 0, m_nBufferSize);

	return DTRUE;
}

void CLTGUIEditCtrl::Destroy()
{
	if (m_lpszString)
	{
		delete [] m_lpszString;
		m_lpszString = DNULL;
	}

	if (m_hDescription)
	{
		m_pClientDE->FreeString(m_hDescription);
		m_hDescription = DNULL;
	}
}

void CLTGUIEditCtrl::UpdateData(DBOOL bSaveAndValidate)
{
	if (!m_pStringValue)
		return;

	if (bSaveAndValidate)
	{
		if (!m_lpszString)
			return;

		strcpy(m_pStringValue, m_lpszString);
	}
	else
	{
		SetText(m_pStringValue);
	}
}

//////////////////////////////////////////////////////////////////////
// Text
//////////////////////////////////////////////////////////////////////

void CLTGUIEditCtrl::SetFont(CLTGUIFont *pFont)
{
	m_pFont = pFont;
}

// As in the original, an over-long string is truncated without a terminator
void CLTGUIEditCtrl::SetText(char *lpszString)
{
	if (lpszString)
	{
		strncpy(m_lpszString, lpszString, m_nBufferSize);
	}
}

//////////////////////////////////////////////////////////////////////
// Size
//////////////////////////////////////////////////////////////////////

int CLTGUIEditCtrl::GetWidth()
{
	if (!m_pFont)		return 0;
	if (!m_lpszString)	return 0;

	HSTRING hString = m_pClientDE->CreateString(m_lpszString);

	DIntPt extents = m_pFont->GetTextExtents(hString);

	m_pClientDE->FreeString(hString);

	return extents.x;
}

int CLTGUIEditCtrl::GetHeight()
{
	if (!m_pFont)
		return 0;

	return m_pFont->GetHeight();
}

//////////////////////////////////////////////////////////////////////
// Rendering
//////////////////////////////////////////////////////////////////////

void CLTGUIEditCtrl::Render(HSURFACE hDestSurf)
{
	if (!m_pFont)		return;
	if (!m_lpszString)	return;

	HDECOLOR color = GetCurrentColor();

	if (m_hDescription)
	{
		int nX = m_pos.x;

		if (m_nAlignment == CF_JUSTIFY_RIGHT)
			nX = m_nTextOffset + m_pos.x - 25;

		m_pFont->DrawSolid(m_hDescription, hDestSurf, nX, m_pos.y, CF_JUSTIFY_LEFT, color);
	}

	if (!m_lpszString)
		return;

	HSTRING hString = m_pClientDE->CreateString(m_lpszString);

	m_pFont->DrawSolid(hString, hDestSurf, m_nTextOffset + m_pos.x, m_pos.y,
					   CF_JUSTIFY_LEFT, color);

	m_pClientDE->FreeString(hString);
}

//////////////////////////////////////////////////////////////////////
// Editing
//////////////////////////////////////////////////////////////////////

void CLTGUIEditCtrl::AddCharacter(char c)
{
	if (!m_lpszString)
		return;

	if ((int)strlen(m_lpszString) + 1 >= m_nBufferSize - 1)
		return;

	int nLen = (int)strlen(m_lpszString);

	m_lpszString[nLen]     = c;
	m_lpszString[nLen + 1] = '\0';
}

void CLTGUIEditCtrl::RemoveCharacter()
{
	if (!m_lpszString)
		return;

	int nLen = (int)strlen(m_lpszString) - 1;

	if (nLen < 0)
		return;

	m_lpszString[nLen] = '\0';
}

//////////////////////////////////////////////////////////////////////
// Input
//////////////////////////////////////////////////////////////////////

DBOOL CLTGUIEditCtrl::HandleKeyDown(int key, int rep)
{
	if (CLTGUICtrl::HandleKeyDown(key, rep))
		return DTRUE;

	if (key == 0x08)									// VK_BACK
	{
		RemoveCharacter();
		return DTRUE;
	}

	if (AsciiXlate(key))
	{
		AddCharacter(AsciiXlate(key));
	}

	return DTRUE;
}

char CLTGUIEditCtrl::AsciiXlate(int key)
{
	if (key >= 'A' && key <= 'Z')
	{
		return IsShiftKeyDown() ? (char)key : (char)(key + 0x20);
	}

	if (key >= '0' && key <= '9')
	{
		if (IsShiftKeyDown())
			return ")!@#$%^&*("[key - '0'];

		return (char)key;
	}

	switch (key)
	{
		case 0x20:		return ' ';							// VK_SPACE

		case 0xBA:		return IsShiftKeyDown() ? ':'  : ';';	// VK_OEM_1
		case 0xBB:		return IsShiftKeyDown() ? '+'  : '=';	// VK_OEM_PLUS
		case 0xBC:		return IsShiftKeyDown() ? '<'  : ',';	// VK_OEM_COMMA
		case 0xBD:		return IsShiftKeyDown() ? '_'  : '-';	// VK_OEM_MINUS
		case 0xBE:		return IsShiftKeyDown() ? '>'  : '.';	// VK_OEM_PERIOD
		case 0xBF:		return IsShiftKeyDown() ? '?'  : '/';	// VK_OEM_2

		case 0xDB:		return IsShiftKeyDown() ? '{'  : '[';	// VK_OEM_4
		case 0xDC:		return IsShiftKeyDown() ? '|'  : '\\';	// VK_OEM_5
		case 0xDD:		return IsShiftKeyDown() ? '}'  : ']';	// VK_OEM_6
		case 0xDE:		return IsShiftKeyDown() ? '"'  : '\'';	// VK_OEM_7
	}

	return 0;
}

void CLTGUIEditCtrl::OnEnter()
{
	if (m_pCommandHandler)
	{
		m_pCommandHandler->SendCommand(m_dwCommandID, 0, 0);
	}
}

void CLTGUIEditCtrl::OnSelChange()
{
	if (!m_bSelected)
	{
		StartFade();
	}
}
