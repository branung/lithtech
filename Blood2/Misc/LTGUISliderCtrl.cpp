// LTGUISliderCtrl.cpp: implementation of the CLTGUISliderCtrl class.

#include "basedefs_de.h"
#include "cpp_clientshell_de.h"

#include "LTGUICommandHandler.h"
#include "LTGUICtrl.h"
#include "LTGUIFadeColorCtrl.h"
#include "CoolFont.h"
#include "LTGUIFont.h"
#include "LTGUISliderCtrl.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLTGUISliderCtrl::CLTGUISliderCtrl()
{
	m_pFont				= DNULL;
	m_dwWidth			= 0;
	m_dwHeight			= 0;
	m_hText				= DNULL;
	m_nMinSlider		= 0;
	m_nMaxSlider		= 100;
	m_nSliderPos		= 0;
	m_nSliderIncrement	= 10;
	m_nAlignment		= CF_JUSTIFY_LEFT;
	m_nSliderOffset		= 0;
	m_hBarSurf			= DNULL;
	m_hTabSurf			= DNULL;
}

CLTGUISliderCtrl::~CLTGUISliderCtrl()
{
	Destroy();
}

//////////////////////////////////////////////////////////////////////
// Creation
//////////////////////////////////////////////////////////////////////

DBOOL CLTGUISliderCtrl::Create(CClientDE *pClientDE, HSTRING hText, CLTGUIFont *pFont,
							   int nSliderOffset, HSURFACE hBarSurf, HSURFACE hTabSurf,
							   int *pnValue)
{
	m_pClientDE		= pClientDE;
	m_hBarSurf		= hBarSurf;
	m_hTabSurf		= hTabSurf;
	m_nSliderOffset	= nSliderOffset;
	m_pnValue		= pnValue;

	if (!pClientDE)	return DFALSE;
	if (!hBarSurf)	return DFALSE;
	if (!hTabSurf)	return DFALSE;

	DDWORD nBarWidth  = 0;
	DDWORD nBarHeight = 0;

	pClientDE->GetSurfaceDims(hBarSurf, &nBarWidth, &nBarHeight);

	m_dwWidth  = nBarWidth + nSliderOffset;
	m_dwHeight = nBarHeight;

	SetFont(pFont);
	SetText(hText);

	return DTRUE;
}

void CLTGUISliderCtrl::Destroy()
{
	if (m_hText && m_pClientDE)
	{
		m_pClientDE->FreeString(m_hText);
		m_hText = DNULL;
	}
}

void CLTGUISliderCtrl::UpdateData(DBOOL bSaveAndValidate)
{
	if (!m_pnValue)
		return;

	if (bSaveAndValidate)
	{
		*m_pnValue = m_nSliderPos;
	}
	else
	{
		SetSliderPos(*m_pnValue);
	}
}

//////////////////////////////////////////////////////////////////////
// Range and position
//////////////////////////////////////////////////////////////////////

void CLTGUISliderCtrl::SetSliderRange(int nMin, int nMax)
{
	m_nMinSlider = nMin;
	m_nMaxSlider = nMax;

	SetSliderPos(m_nSliderPos);
}

void CLTGUISliderCtrl::SetSliderPos(int nPos)
{
	m_nSliderPos = nPos;

	if (m_nSliderPos > m_nMaxSlider)
		m_nSliderPos = m_nMaxSlider;

	if (m_nSliderPos < m_nMinSlider)
		m_nSliderPos = m_nMinSlider;
}

//////////////////////////////////////////////////////////////////////
// Text
//////////////////////////////////////////////////////////////////////

void CLTGUISliderCtrl::SetText(HSTRING hString)
{
	if (m_hText && m_pClientDE)
	{
		m_pClientDE->FreeString(m_hText);
		m_hText = DNULL;
	}

	if (hString && m_pClientDE)
	{
		m_hText = m_pClientDE->CopyString(hString);
	}
}

void CLTGUISliderCtrl::SetText(int messageCode)
{
	HSTRING hString = m_pClientDE->FormatString(messageCode);

	SetText(hString);

	m_pClientDE->FreeString(hString);
}

void CLTGUISliderCtrl::SetFont(CLTGUIFont *pFont)
{
	m_pFont = pFont;
}

//////////////////////////////////////////////////////////////////////
// Rendering
//////////////////////////////////////////////////////////////////////

void CLTGUISliderCtrl::Render(HSURFACE hDestSurf)
{
	RenderSliderText(hDestSurf);
	RenderSliderBar(hDestSurf);
}

void CLTGUISliderCtrl::RenderSliderText(HSURFACE hDestSurf)
{
	HDECOLOR color = GetCurrentColor();

	if (!m_pFont)	return;
	if (!m_hText)	return;

	int nX = m_pos.x;

	if (m_nAlignment == CF_JUSTIFY_RIGHT)
		nX = m_nSliderOffset + m_pos.x - 25;

	m_pFont->DrawSolid(m_hText, hDestSurf, nX, m_pos.y, m_nAlignment, color);
}

void CLTGUISliderCtrl::RenderSliderBar(HSURFACE hDestSurf)
{
	if (!m_hBarSurf)	return;
	if (!m_hTabSurf)	return;

	HDECOLOR hTransColor = m_pClientDE->SetupColor1(1.0f, 0.0f, 1.0f, DFALSE);

	m_pClientDE->DrawSurfaceToSurfaceTransparent(hDestSurf, m_hBarSurf, DNULL,
												 m_nSliderOffset + m_pos.x, m_pos.y,
												 hTransColor);

	DDWORD nBarWidth  = 0;
	DDWORD nBarHeight = 0;
	DDWORD nTabWidth  = 0;
	DDWORD nTabHeight = 0;

	m_pClientDE->GetSurfaceDims(m_hBarSurf, &nBarWidth, &nBarHeight);
	m_pClientDE->GetSurfaceDims(m_hTabSurf, &nTabWidth, &nTabHeight);

	float fPercent = (float)(m_nSliderPos - m_nMinSlider) /
					 (float)(m_nMaxSlider - m_nMinSlider);

	int nX = (int)(fPercent * (float)((int)nBarWidth - (int)nTabWidth - 10))
			 + m_nSliderOffset + m_pos.x + 5;

	m_pClientDE->DrawSurfaceToSurfaceTransparent(hDestSurf, m_hTabSurf, DNULL,
												 nX, m_pos.y, hTransColor);
}

//////////////////////////////////////////////////////////////////////
// Input
//////////////////////////////////////////////////////////////////////

void CLTGUISliderCtrl::OnLeft()
{
	int nPos = m_nSliderPos - m_nSliderIncrement;

	if (nPos < m_nMinSlider)
		nPos = m_nMinSlider;

	SetSliderPos(nPos);
}

void CLTGUISliderCtrl::OnRight()
{
	int nPos = m_nSliderPos + m_nSliderIncrement;

	if (nPos > m_nMaxSlider)
		nPos = m_nMaxSlider;

	SetSliderPos(nPos);
}

// The inverse of RenderSliderBar: the click's fraction of the tab's travel (the bar less the tab and
// RenderSliderBar's 10 pixel margin), clamped so a click on the label lands on the minimum
void CLTGUISliderCtrl::OnMouseClick(int x, int y)
{
	if (!m_hBarSurf || !m_hTabSurf || !m_pClientDE)
		return;

	DDWORD nBarWidth  = 0;
	DDWORD nBarHeight = 0;
	DDWORD nTabWidth  = 0;
	DDWORD nTabHeight = 0;

	m_pClientDE->GetSurfaceDims(m_hBarSurf, &nBarWidth, &nBarHeight);
	m_pClientDE->GetSurfaceDims(m_hTabSurf, &nTabWidth, &nTabHeight);

	int nTravel = (int)nBarWidth - (int)nTabWidth - 10;
	if (nTravel <= 0)
		return;

	int nBarLeft = m_nSliderOffset + m_pos.x + 5;
	float fPercent = (float)(x - nBarLeft) / (float)nTravel;

	if (fPercent < 0.0f)	fPercent = 0.0f;
	if (fPercent > 1.0f)	fPercent = 1.0f;

	SetSliderPos(m_nMinSlider + (int)(fPercent * (float)(m_nMaxSlider - m_nMinSlider) + 0.5f));
}

void CLTGUISliderCtrl::OnSelChange()
{
	if (!m_bSelected)
	{
		StartFade();
	}
}
