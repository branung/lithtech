// LTGUICtrl.cpp: implementation of the CLTGUICtrl class.

// The LTGUI headers use DDWORD, DBOOL and CClientDE without including them
#include "basedefs_de.h"
#include "cpp_clientshell_de.h"
#include "LTGUICommandHandler.h"
#include "LTGUICtrl.h"

//////////////////////////////////////////////////////////////////////
// Static members
//////////////////////////////////////////////////////////////////////

DBOOL CLTGUICtrl::s_bShiftState = DFALSE;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLTGUICtrl::CLTGUICtrl()
{
	m_pClientDE		= DNULL;
	m_bCreated		= DFALSE;
	m_pos.x			= 0;
	m_pos.y			= 0;
	m_bSelected		= DFALSE;
	m_bEnabled		= DTRUE;
}

CLTGUICtrl::~CLTGUICtrl()
{
}

//////////////////////////////////////////////////////////////////////
// The defaults
//////////////////////////////////////////////////////////////////////

void CLTGUICtrl::Destroy()
{
}

void CLTGUICtrl::ResetAnimation()
{
}

DBOOL CLTGUICtrl::HandleKeyDown(int key, int rep)
{
	return DFALSE;
}

void CLTGUICtrl::OnLeft()
{
}

void CLTGUICtrl::OnRight()
{
}

void CLTGUICtrl::OnUp()
{
}

void CLTGUICtrl::OnDown()
{
}

void CLTGUICtrl::OnEnter()
{
}

void CLTGUICtrl::OnMouseClick(int x, int y)
{
	OnEnter();
}

void CLTGUICtrl::OnCommand(DDWORD dwCommandID)
{
}

void CLTGUICtrl::UpdateData(DBOOL bSaveAndValidate)
{
}

//////////////////////////////////////////////////////////////////////
// Selection
//////////////////////////////////////////////////////////////////////

void CLTGUICtrl::Select(DBOOL bSelected)
{
	if (m_bSelected != bSelected)
	{
		m_bSelected = bSelected;
		OnSelChange();
	}
}
