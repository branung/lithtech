// LTGUIListCtrl.cpp: implementation of the CLTGUIListCtrl class.

#include "basedefs_de.h"
#include "cpp_clientshell_de.h"

#include "LTGUICommandHandler.h"
#include "LTGUICtrl.h"
#include "LTGUIListCtrl.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLTGUIListCtrl::CLTGUIListCtrl()
{
	m_nItemSpacing			= 10;
	m_nCurrentIndex			= 0;
	m_nFirstDisplayedItem	= 0;
	m_nHeight				= 100;
	m_bScrollWrap			= DTRUE;
	m_bEnableBoxFormat		= DTRUE;
}

// A list doesn't own its controls
CLTGUIListCtrl::~CLTGUIListCtrl()
{
	Destroy();
}

//////////////////////////////////////////////////////////////////////
// Creation
//////////////////////////////////////////////////////////////////////

DBOOL CLTGUIListCtrl::Create(int nHeight)
{
	m_nHeight	= nHeight;
	m_bCreated	= DTRUE;

	return DTRUE;
}

void CLTGUIListCtrl::Destroy()
{
	RemoveAllControls();

	m_bCreated = DFALSE;
}

//////////////////////////////////////////////////////////////////////
// Rendering
//////////////////////////////////////////////////////////////////////

void CLTGUIListCtrl::Render(HSURFACE hDestSurf)
{
	if (m_bEnableBoxFormat)
	{
		int nCurY = m_pos.y;

		for (DDWORD i = m_nFirstDisplayedItem; i < m_controlArray.GetSize(); ++i)
		{
			m_controlArray[i]->GetWidth();
			int nHeight = m_controlArray[i]->GetHeight();

			if (nCurY + nHeight >= m_pos.y + m_nHeight)
				break;

			DIntPt oldPos = m_controlArray[i]->GetPos();

			m_controlArray[i]->SetPos(m_pos.x, nCurY);
			m_controlArray[i]->Render(hDestSurf);
			m_controlArray[i]->SetPos(oldPos);

			nCurY += m_nItemSpacing + nHeight;
		}
	}
	else
	{
		for (DDWORD i = 0; i < m_controlArray.GetSize(); ++i)
		{
			m_controlArray[i]->Render(hDestSurf);
		}
	}
}

//////////////////////////////////////////////////////////////////////
// Measurement
//////////////////////////////////////////////////////////////////////

int CLTGUIListCtrl::GetWidth()
{
	int nWidth = 0;
	int nCurY  = 0;

	for (DDWORD i = 0; i < m_controlArray.GetSize(); ++i)
	{
		int nCtrlWidth  = m_controlArray[i]->GetWidth();
		int nCtrlHeight = m_controlArray[i]->GetHeight();

		if (nCurY + nCtrlHeight >= m_nHeight)
			break;

		if (nCtrlWidth > nWidth)
			nWidth = nCtrlWidth;

		nCurY += m_nItemSpacing + nCtrlHeight;
	}

	return nWidth;
}

int CLTGUIListCtrl::GetHeight()
{
	return m_nHeight;
}

int CLTGUIListCtrl::GetLastDisplayedIndex(int nStartIndex)
{
	int nCurY = m_pos.y;

	if ((DDWORD)nStartIndex >= m_controlArray.GetSize())
		return (int)m_controlArray.GetSize();

	for (DDWORD i = nStartIndex; i < m_controlArray.GetSize(); ++i)
	{
		m_controlArray[i]->GetWidth();
		int nHeight = m_controlArray[i]->GetHeight();

		if (nCurY + nHeight >= m_pos.y + m_nHeight)
			return (int)i - 1;

		nCurY += m_nItemSpacing + nHeight;
	}

	return (int)m_controlArray.GetSize();
}

//////////////////////////////////////////////////////////////////////
// Input
//////////////////////////////////////////////////////////////////////

DBOOL CLTGUIListCtrl::HandleKeyDown(int key, int rep)
{
	if (CLTGUICtrl::HandleKeyDown(key, rep))
		return DTRUE;

	if (m_nCurrentIndex < (int)m_controlArray.GetSize())
		return m_controlArray[m_nCurrentIndex]->HandleKeyDown(key, rep);

	return DFALSE;
}

// As in the original, wrapping needs more than two items,
// and stopping at the start without wrapping leaves the selection alone
void CLTGUIListCtrl::OnUp()
{
	int nStart = m_nCurrentIndex;
	int nIndex = m_nCurrentIndex - 1;

	while (nIndex != nStart)
	{
		if (nIndex < 0)
		{
			if (!m_bScrollWrap)
				return;

			if (m_controlArray.GetSize() <= 2)
				return;

			nIndex = (int)m_controlArray.GetSize() - 1;
		}

		if (m_controlArray[nIndex]->IsEnabled())
		{
			nStart = nIndex;
			break;
		}

		--nIndex;
	}

	SelectItem(nStart);
}

void CLTGUIListCtrl::OnDown()
{
	int nStart = m_nCurrentIndex;
	int nIndex = m_nCurrentIndex + 1;

	while (nIndex != nStart)
	{
		if (nIndex >= (int)m_controlArray.GetSize())
		{
			if (!m_bScrollWrap)
				return;

			if (m_controlArray.GetSize() <= 2)
				return;

			nIndex = 0;
		}

		if (m_controlArray[nIndex]->IsEnabled())
		{
			nStart = nIndex;
			break;
		}

		++nIndex;
	}

	SelectItem(nStart);
}

void CLTGUIListCtrl::OnLeft()
{
	if (m_nCurrentIndex < (int)m_controlArray.GetSize())
		m_controlArray[m_nCurrentIndex]->OnLeft();
}

void CLTGUIListCtrl::OnRight()
{
	if (m_nCurrentIndex < (int)m_controlArray.GetSize())
		m_controlArray[m_nCurrentIndex]->OnRight();
}

void CLTGUIListCtrl::OnEnter()
{
	if (m_nCurrentIndex < (int)m_controlArray.GetSize())
		m_controlArray[m_nCurrentIndex]->OnEnter();
}

//////////////////////////////////////////////////////////////////////
// Mouse hit testing, added to the Jupiter port
//////////////////////////////////////////////////////////////////////

int CLTGUIListCtrl::GetControlUnderPoint(int x, int y)
{
	if (m_bEnableBoxFormat)
	{
		int nCurY = m_pos.y;

		for (DDWORD i = m_nFirstDisplayedItem; i < m_controlArray.GetSize(); ++i)
		{
			int nHeight = m_controlArray[i]->GetHeight();

			if (nCurY + nHeight >= m_pos.y + m_nHeight)
				break;

			if (x >= m_pos.x && y >= nCurY && y < nCurY + nHeight)
			{
				m_controlArray[i]->SetPos(m_pos.x, nCurY);
				return (int)i;
			}

			nCurY += m_nItemSpacing + nHeight;
		}
	}
	else
	{
		for (DDWORD i = 0; i < m_controlArray.GetSize(); ++i)
		{
			DIntPt pos = m_controlArray[i]->GetPos();
			int nWidth  = m_controlArray[i]->GetWidth();
			int nHeight = m_controlArray[i]->GetHeight();

			if (x >= pos.x && x < pos.x + nWidth && y >= pos.y && y < pos.y + nHeight)
				return (int)i;
		}
	}

	return -1;
}

//////////////////////////////////////////////////////////////////////
// Contents
//////////////////////////////////////////////////////////////////////

int CLTGUIListCtrl::AddControl(CLTGUICtrl *pControl)
{
	m_controlArray.Add(pControl);

	int nIndex = (int)m_controlArray.GetSize() - 1;

	if (nIndex == m_nCurrentIndex)
		m_controlArray[nIndex]->Select(DTRUE);

	return nIndex;
}

void CLTGUIListCtrl::RemoveControl(CLTGUICtrl *pControl)
{
	for (DDWORD i = 0; i < m_controlArray.GetSize(); ++i)
	{
		if (m_controlArray[i] == pControl)
		{
			m_controlArray.Remove(i);
			return;
		}
	}
}

void CLTGUIListCtrl::RemoveControl(int nIndex)
{
	m_controlArray.Remove(nIndex);
}

void CLTGUIListCtrl::RemoveAllControls()
{
	while ((int)m_controlArray.GetSize() > 0)
	{
		RemoveControl(0);
	}
}

CLTGUICtrl *CLTGUIListCtrl::GetControl(int nIndex)
{
	return m_controlArray[nIndex];
}

//////////////////////////////////////////////////////////////////////
// Selection and scrolling
//////////////////////////////////////////////////////////////////////

void CLTGUIListCtrl::SelectItem(int nIndex)
{
	if (m_nCurrentIndex < (int)m_controlArray.GetSize())
	{
		m_controlArray[m_nCurrentIndex]->Select(DFALSE);
	}

	if (m_controlArray.GetSize() == 0)
	{
		m_nFirstDisplayedItem = 0;
		m_nCurrentIndex       = 0;
		return;
	}

	m_nCurrentIndex = nIndex;
	m_controlArray[nIndex]->Select(DTRUE);

	if (m_nCurrentIndex < m_nFirstDisplayedItem)
	{
		while (m_nCurrentIndex < m_nFirstDisplayedItem)
			--m_nFirstDisplayedItem;

		return;
	}

	while (GetLastDisplayedIndex(m_nFirstDisplayedItem) < m_nCurrentIndex)
	{
		if (m_nFirstDisplayedItem >= (int)m_controlArray.GetSize())
			break;

		++m_nFirstDisplayedItem;
	}
}
