// LTGUIColumnTextCtrl.cpp: implementation of the CLTGUIColumnTextCtrl class.

#include "basedefs_de.h"
#include "cpp_clientshell_de.h"

// For CMoArray, which these LTGUI headers use without including it
#include "stdlith.h"

#include "LTGUICommandHandler.h"
#include "LTGUICtrl.h"
#include "LTGUIFadeColorCtrl.h"
#include "CoolFont.h"
#include "LTGUIFont.h"
#include "LTGUIColumnTextCtrl.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLTGUIColumnTextCtrl::CLTGUIColumnTextCtrl()
{
	m_dwCommandID		= 0;
	m_pFont				= DNULL;
	m_dwWidth			= 0;
	m_dwHeight			= 0;
	m_pCommandHandler	= DNULL;
}

CLTGUIColumnTextCtrl::~CLTGUIColumnTextCtrl()
{
	Destroy();
}

//////////////////////////////////////////////////////////////////////
// Creation
//////////////////////////////////////////////////////////////////////

DBOOL CLTGUIColumnTextCtrl::Create(CClientDE *pClientDE, DDWORD dwCommandID, CLTGUIFont *pFont,
								   CLTGUICommandHandler *pCommandHandler)
{
	m_pClientDE			= pClientDE;
	m_dwCommandID		= dwCommandID;
	m_pCommandHandler	= pCommandHandler;

	SetFont(pFont);

	return DTRUE;
}

void CLTGUIColumnTextCtrl::Destroy()
{
	RemoveAllColumns();
}

//////////////////////////////////////////////////////////////////////
// Columns
//////////////////////////////////////////////////////////////////////

DBOOL CLTGUIColumnTextCtrl::AddColumn(HSTRING hString, int nWidth, short nJustify)
{
	if (!hString)
		return DFALSE;

	LTGUIColumn *pColumn = new LTGUIColumn;

	pColumn->m_nWidth	= nWidth;
	pColumn->m_hString	= m_pClientDE->CopyString(hString);
	pColumn->m_nJustify	= nJustify;

	m_columnArray.Add(pColumn);

	return DTRUE;
}

DBOOL CLTGUIColumnTextCtrl::AddColumn(int messageCode, int nWidth, short nJustify)
{
	if (!messageCode)
		return DFALSE;

	HSTRING hString = m_pClientDE->FormatString(messageCode);

	DBOOL bResult = AddColumn(hString, nWidth, nJustify);

	m_pClientDE->FreeString(hString);

	return bResult;
}

HSTRING CLTGUIColumnTextCtrl::GetString(int nColumnIndex)
{
	return m_pClientDE->CopyString(m_columnArray[nColumnIndex]->m_hString);
}

void CLTGUIColumnTextCtrl::SetString(int nColumnIndex, HSTRING hString)
{
	m_pClientDE->FreeString(m_columnArray[nColumnIndex]->m_hString);

	m_columnArray[nColumnIndex]->m_hString = m_pClientDE->CopyString(hString);
}

void CLTGUIColumnTextCtrl::SetString(int nColumnIndex, int messageCode)
{
	if (!messageCode)
		return;

	HSTRING hString = m_pClientDE->FormatString(messageCode);

	SetString(nColumnIndex, hString);

	m_pClientDE->FreeString(hString);
}

void CLTGUIColumnTextCtrl::RemoveColumn(int nIndex)
{
	LTGUIColumn *pColumn = m_columnArray[nIndex];

	if (pColumn)
	{
		if (pColumn->m_hString)
		{
			m_pClientDE->FreeString(pColumn->m_hString);
			pColumn->m_hString = DNULL;
		}

		delete pColumn;
	}

	m_columnArray.Remove(nIndex);
}

void CLTGUIColumnTextCtrl::RemoveAllColumns()
{
	while (m_columnArray.GetSize() > 0)
	{
		RemoveColumn(0);
	}
}

//////////////////////////////////////////////////////////////////////
// Font and size
//////////////////////////////////////////////////////////////////////

void CLTGUIColumnTextCtrl::SetFont(CLTGUIFont *pFont)
{
	m_pFont = pFont;

	if (pFont)
		m_dwHeight = pFont->GetHeight();
	else
		m_dwHeight = 0;

	m_dwWidth = 0;

	for (DDWORD i = 0; i < m_columnArray.GetSize(); ++i)
	{
		m_dwWidth += m_columnArray[i]->m_nWidth;
	}
}

//////////////////////////////////////////////////////////////////////
// Rendering
//////////////////////////////////////////////////////////////////////

void CLTGUIColumnTextCtrl::Render(HSURFACE hDestSurf)
{
	if (!m_pFont)
		return;

	HDECOLOR color = GetCurrentColor();

	int nCurX = m_pos.x;

	for (DDWORD i = 0; i < m_columnArray.GetSize(); ++i)
	{
		LTGUIColumn *pColumn = m_columnArray[i];

		int nX = nCurX;

		if (pColumn->m_nJustify == CF_JUSTIFY_RIGHT)
			nX += pColumn->m_nWidth;

		m_pFont->DrawSolid(pColumn->m_hString, hDestSurf, nX, m_pos.y,
						   pColumn->m_nJustify, color);

		nCurX += pColumn->m_nWidth;
	}
}

//////////////////////////////////////////////////////////////////////
// Input
//////////////////////////////////////////////////////////////////////

void CLTGUIColumnTextCtrl::OnEnter()
{
	if (m_pCommandHandler)
	{
		m_pCommandHandler->SendCommand(m_dwCommandID, 0, 0);
	}
}

void CLTGUIColumnTextCtrl::OnSelChange()
{
	if (!m_bSelected)
	{
		StartFade();
	}
}
