// LTGUIOnOffCtrl.cpp: implementation of the CLTGUIOnOffCtrl class.

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
#include "LTGUIOnOffCtrl.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLTGUIOnOffCtrl::CLTGUIOnOffCtrl()
{
	m_bOn = DFALSE;
}

CLTGUIOnOffCtrl::~CLTGUIOnOffCtrl()
{
	Destroy();
}

//////////////////////////////////////////////////////////////////////
// Creation
//////////////////////////////////////////////////////////////////////

DBOOL CLTGUIOnOffCtrl::Create(CClientDE *pClientDE, HSTRING hString, CLTGUIFont *pFont,
							  int nRightColumnOffset, DBOOL *pbValue)
{
	if (!CLTGUIColumnTextCtrl::Create(pClientDE, 0, pFont, DNULL))
		return DFALSE;

	AddColumn(hString, nRightColumnOffset - 25, CF_JUSTIFY_RIGHT);

	HSTRING hSpace = m_pClientDE->CreateString(" ");

	AddColumn(hSpace, 25, CF_JUSTIFY_LEFT);
	AddColumn(hSpace, 25, CF_JUSTIFY_LEFT);

	m_pClientDE->FreeString(hSpace);

	m_pbValue = pbValue;

	SetOn(DFALSE);

	return DTRUE;
}

void CLTGUIOnOffCtrl::UpdateData(DBOOL bSaveAndValidate)
{
	if (!m_pbValue)
		return;

	if (bSaveAndValidate)
	{
		*m_pbValue = m_bOn;
	}
	else
	{
		SetOn(*m_pbValue);
	}
}

// Leaks an HSTRING each call, as the original did
void CLTGUIOnOffCtrl::SetOn(DBOOL bOn)
{
	m_bOn = bOn;

	if (bOn)
	{
		SetString(2, m_pClientDE->CreateString("ON"));
	}
	else
	{
		SetString(2, m_pClientDE->CreateString("OFF"));
	}
}
