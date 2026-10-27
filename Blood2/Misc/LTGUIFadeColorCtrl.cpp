// LTGUIFadeColorCtrl.cpp: implementation of the CLTGUIFadeColorCtrl class.

// The LTGUI headers use DDWORD, DBOOL and CClientDE without including them
#include "basedefs_de.h"
#include "cpp_clientshell_de.h"
#include "LTGUICommandHandler.h"
#include "LTGUICtrl.h"
#include "LTGUIFadeColorCtrl.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLTGUIFadeColorCtrl::CLTGUIFadeColorCtrl()
{
	m_dwFadeTime	= 300;
	m_selColor		= SETRGB(255,255,255);
	m_nonSelColor	= SETRGB(128,128,128);
	m_disabledColor	= SETRGB(96,96,96);
	m_fStartTime	= 0.0f;
}

CLTGUIFadeColorCtrl::~CLTGUIFadeColorCtrl()
{
}

//////////////////////////////////////////////////////////////////////
// The fade
//////////////////////////////////////////////////////////////////////

void CLTGUIFadeColorCtrl::SetColor(HDECOLOR selColor, HDECOLOR nonSelColor, HDECOLOR disabledColor)
{
	m_selColor		= selColor;
	m_nonSelColor	= nonSelColor;
	m_disabledColor	= disabledColor;
}

void CLTGUIFadeColorCtrl::StartFade()
{
	if (m_pClientDE)
	{
		m_fStartTime = m_pClientDE->GetTime();
	}
}

void CLTGUIFadeColorCtrl::StopFade()
{
	m_fStartTime = 0.0f;
}

float CLTGUIFadeColorCtrl::GetCurrentFadePercentage()
{
	if (m_dwFadeTime == 0)
	{
		return m_bSelected ? 0.0f : 1.0f;
	}

	if (m_bSelected)
	{
		return 0.0f;
	}

	float fPercent = (m_pClientDE->GetTime() - m_fStartTime) /
					 ((float)m_dwFadeTime * 0.001f);

	if (fPercent > 1.0f)
	{
		fPercent = 1.0f;
	}

	return fPercent;
}

HDECOLOR CLTGUIFadeColorCtrl::GetCurrentColor()
{
	if (!m_bEnabled)
	{
		return m_disabledColor;
	}

	float fPercent = GetCurrentFadePercentage();

	int nRed	= (int)((float)GETR(m_selColor)    * (1.0f - fPercent) +
						(float)GETR(m_nonSelColor) * fPercent);
	int nGreen	= (int)((float)GETG(m_selColor)    * (1.0f - fPercent) +
						(float)GETG(m_nonSelColor) * fPercent);
	int nBlue	= (int)((float)GETB(m_selColor)    * (1.0f - fPercent) +
						(float)GETB(m_nonSelColor) * fPercent);

	return (HDECOLOR)((nRed << 16) | (nGreen << 8) | nBlue);
}
