// LTGUITextItemCtrl.cpp: implementation of the CLTGUITextItemCtrl class.

#include "basedefs_de.h"
#include "cpp_clientshell_de.h"

// For CMoArray, which these LTGUI headers use without including it
#include "stdlith.h"

#include "LTGUICommandHandler.h"
#include "LTGUICtrl.h"
#include "LTGUIFadeColorCtrl.h"
#include "CoolFont.h"
#include "LTGUIFont.h"
#include "LTGUITextItemCtrl.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLTGUITextItemCtrl::CLTGUITextItemCtrl()
{
	m_dwCommandID		= 0;
	m_dwWidth			= 0;
	m_dwHeight			= 0;
	m_nSelIndex			= 0;
	m_pCommandHandler	= DNULL;
}

CLTGUITextItemCtrl::~CLTGUITextItemCtrl()
{
	Destroy();
}

//////////////////////////////////////////////////////////////////////
// Creation
//////////////////////////////////////////////////////////////////////

DBOOL CLTGUITextItemCtrl::Create(CClientDE *pClientDE, DDWORD dwCommandID, HSTRING hText,
								 CLTGUIFont *pFontArray, int nNumFonts, DBOOL bDrawSolid,
								 CLTGUICommandHandler *pCommandHandler, int *pnValue)
{
	m_pClientDE			= pClientDE;
	m_dwCommandID		= dwCommandID;
	m_pCommandHandler	= pCommandHandler;

	if (hText)
	{
		AddString(hText);
	}

	if (!SetFont(pFontArray, nNumFonts, bDrawSolid))
	{
		Destroy();
		return DFALSE;
	}

	m_pnValue = pnValue;

	return DTRUE;
}

void CLTGUITextItemCtrl::Destroy()
{
	RemoveAll();
}

void CLTGUITextItemCtrl::UpdateData(DBOOL bSaveAndValidate)
{
	if (!m_pnValue)
		return;

	if (bSaveAndValidate)
	{
		*m_pnValue = m_nSelIndex;
	}
	else
	{
		SetSelIndex(*m_pnValue);
	}
}

//////////////////////////////////////////////////////////////////////
// Strings
//////////////////////////////////////////////////////////////////////

void CLTGUITextItemCtrl::AddString(HSTRING hString)
{
	if (!hString)		return;
	if (!m_pClientDE)	return;

	m_stringArray.Add(m_pClientDE->CopyString(hString));
}

void CLTGUITextItemCtrl::AddString(int messageCode)
{
	if (!m_pClientDE)
		return;

	HSTRING hString = m_pClientDE->FormatString(messageCode);

	AddString(hString);

	m_pClientDE->FreeString(hString);
}

HSTRING CLTGUITextItemCtrl::GetString(int nIndex)
{
	return m_stringArray[nIndex];
}

void CLTGUITextItemCtrl::RemoveString(int nIndex)
{
	if (m_pClientDE)
	{
		m_pClientDE->FreeString(m_stringArray[nIndex]);
	}

	m_stringArray.Remove(nIndex);
}

void CLTGUITextItemCtrl::RemoveAll()
{
	while (m_stringArray.GetSize() > 0)
	{
		RemoveString(0);
	}
}

// No clamp, as in the original
void CLTGUITextItemCtrl::SetSelIndex(int nIndex)
{
	m_nSelIndex = nIndex;
}

//////////////////////////////////////////////////////////////////////
// Font and size
//////////////////////////////////////////////////////////////////////

DBOOL CLTGUITextItemCtrl::SetFont(CLTGUIFont *pFontArray, int nNumFonts, DBOOL bDrawSolid)
{
	if (!pFontArray)		return DFALSE;
	if (nNumFonts <= 0)		return DFALSE;

	m_pFontArray = pFontArray;

	if (m_stringArray.GetSize() > 0)
	{
		DIntPt extents = pFontArray->GetTextExtents(m_stringArray[m_nSelIndex]);

		m_dwWidth  = extents.x;
		m_dwHeight = extents.y;
	}
	else
	{
		m_dwWidth  = 0;
		m_dwHeight = pFontArray->GetHeight();
	}

	m_nNumFonts  = nNumFonts;
	m_bDrawSolid = bDrawSolid;

	return DTRUE;
}

//////////////////////////////////////////////////////////////////////
// Rendering
//////////////////////////////////////////////////////////////////////

void CLTGUITextItemCtrl::Render(HSURFACE hDestSurf)
{
	if (!m_pFontArray)					return;
	if (m_stringArray.GetSize() == 0)	return;

	if (m_bDrawSolid)
	{
		m_pFontArray[0].DrawSolid(m_stringArray[m_nSelIndex], hDestSurf,
								  m_pos.x, m_pos.y, CF_JUSTIFY_LEFT,
								  GetCurrentColor());
		return;
	}

	int nFontIndex;

	if (!m_bEnabled)
	{
		nFontIndex = m_nNumFonts - 1;
	}
	else
	{
		int nRange = m_nNumFonts - 2;
		if (nRange < 0)
			nRange = 0;

		nFontIndex = (int)(GetCurrentFadePercentage() * (float)nRange);
	}

	m_pFontArray[nFontIndex].Draw(m_stringArray[m_nSelIndex], hDestSurf,
								  m_pos.x, m_pos.y, CF_JUSTIFY_LEFT);
}

//////////////////////////////////////////////////////////////////////
// Input
//////////////////////////////////////////////////////////////////////

void CLTGUITextItemCtrl::OnEnter()
{
	if (m_pCommandHandler)
	{
		m_pCommandHandler->SendCommand(m_dwCommandID, 0, 0);
	}
}

void CLTGUITextItemCtrl::OnLeft()
{
	if (m_stringArray.GetSize() == 0)
		return;

	--m_nSelIndex;

	if (m_nSelIndex < 0)
		m_nSelIndex = (int)m_stringArray.GetSize() - 1;
}

void CLTGUITextItemCtrl::OnRight()
{
	if (m_stringArray.GetSize() == 0)
		return;

	++m_nSelIndex;

	if (m_nSelIndex >= (int)m_stringArray.GetSize())
		m_nSelIndex = 0;
}

void CLTGUITextItemCtrl::OnSelChange()
{
	if (!m_bSelected)
	{
		StartFade();
	}
}
