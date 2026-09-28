// ----------------------------------------------------------------------- //
//
// MODULE  : GeneralOptionsMenu.cpp
//
// PURPOSE : The GENERAL options screen, a newly added menu to add QOL
//			 gameplay updates as optional toggles. 
//
// ----------------------------------------------------------------------- //

#include "clientheaders.h"
#include "GeneralOptionsMenu.h"
#include "TextHelper.h"
#include "ClientRes.h"
#include "RiotMenu.h"

#define ID_SKIPCUTSCENES	0
#define ID_BACK				1

#define NUM_GENERAL_ITEMS		2

CGeneralOptionsMenu::CGeneralOptionsMenu() : CBaseMenu()
{
	m_nSecondColumn = 0;
}

LTBOOL CGeneralOptionsMenu::Init (ILTClient* pClientDE, CRiotMenu* pRiotMenu, CBaseMenu* pParent, int nScreenWidth, int nScreenHeight)
{
	if (!pClientDE || !pRiotMenu) return LTFALSE;

	return CBaseMenu::Init (pClientDE, pRiotMenu, pParent, nScreenWidth, nScreenHeight);
}

void CGeneralOptionsMenu::Reset()
{
	CBaseMenu::Reset();
}

/*
	The setting is the console variable itself, read every frame.
	The leading + keeps it in autoexec.cfg.
*/
LTBOOL CGeneralOptionsMenu::GetSkipCutscenes()
{
	if (!m_pClientDE) return LTFALSE;

	HCONSOLEVAR hVar = m_pClientDE->GetConsoleVar ("SkipCutscenes");
	if (!hVar) return LTFALSE;

	return (m_pClientDE->GetVarValueFloat (hVar) > 0.0f) ? LTTRUE : LTFALSE;
}

void CGeneralOptionsMenu::SetSkipCutscenes (LTBOOL bOn)
{
	if (!m_pClientDE) return;

	m_pClientDE->RunConsoleString (bOn ? "+SkipCutscenes 1" : "+SkipCutscenes 0");
}

void CGeneralOptionsMenu::RebuildSkipCutscenesSurfaces()
{
	if (!m_pClientDE || !m_pRiotMenu) return;

	CBitmapFont* pFontNormal = m_pRiotMenu->GetFont12n();
	CBitmapFont* pFontSelected = m_pRiotMenu->GetFont12s();
	if (!pFontNormal || !pFontSelected) return;

	if (m_SkipCutscenes.hMenuItem) m_pClientDE->DeleteSurface (m_SkipCutscenes.hMenuItem);
	if (m_SkipCutscenes.hMenuItemSelected) m_pClientDE->DeleteSurface (m_SkipCutscenes.hMenuItemSelected);

	int nStringID = GetSkipCutscenes() ? IDS_ON : IDS_OFF;
	m_SkipCutscenes.hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, nStringID);
	m_SkipCutscenes.hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, nStringID);
}

void CGeneralOptionsMenu::Left()
{
	if (!m_pClientDE) return;

	if (m_nSelection == ID_SKIPCUTSCENES)
	{
		SetSkipCutscenes (!GetSkipCutscenes());
		RebuildSkipCutscenesSurfaces();
	}

	CBaseMenu::Left();
}

void CGeneralOptionsMenu::Right()
{
	if (m_nSelection == ID_SKIPCUTSCENES)
	{
		Left();
		return;
	}

	CBaseMenu::Right();
}

void CGeneralOptionsMenu::Return()
{
	if (!m_pRiotMenu) return;

	if (m_nSelection == ID_BACK)
	{
		m_pRiotMenu->SetCurrentMenu (m_pParent);
		CBaseMenu::Return();
	}
}

void CGeneralOptionsMenu::Activate (int nItem)
{
	if (nItem == ID_SKIPCUTSCENES)
	{
		Left();
		return;
	}

	CBaseMenu::Activate (nItem);
}

int CGeneralOptionsMenu::GetItemWidth (int nItem)
{
	int nLabel = CBaseMenu::GetItemWidth (nItem);

	if (nItem == ID_SKIPCUTSCENES && m_pClientDE && m_SkipCutscenes.hMenuItem)
	{
		uint32 nWidth, nHeight;
		m_pClientDE->GetSurfaceDims (m_SkipCutscenes.hMenuItem, &nWidth, &nHeight);
		int nThroughValue = m_nSecondColumn + (int)nWidth;
		if (nThroughValue > nLabel) return nThroughValue;
	}

	return nLabel;
}

void CGeneralOptionsMenu::Draw (HSURFACE hScreen, int nScreenWidth, int nScreenHeight, int nTextOffset)
{
	if (!m_pClientDE) return;

	CBaseMenu::Draw (hScreen, nScreenWidth, nScreenHeight, nTextOffset);

	// The value column, walked the way CBaseMenu::Draw walks the labels
	int y = m_nMenuY + m_szMenuTitle.cy + m_nMenuTitleSpacing;
	for (int i = m_nTopItem; i < NUM_GENERAL_ITEMS; i++)
	{
		if (i == ID_SKIPCUTSCENES && m_SkipCutscenes.hMenuItem)
		{
			m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen,
				m_nSelection == i ? m_SkipCutscenes.hMenuItemSelected : m_SkipCutscenes.hMenuItem,
				LTNULL, m_nMenuX + m_nSecondColumn, y, LTNULL);
		}

		y += m_GenericItem[i].szMenuItem.cy + m_nMenuSpacing;
		if (y > GetMenuAreaBottom() - (int)m_GenericItem[i].szMenuItem.cy) break;
	}
}

LTBOOL CGeneralOptionsMenu::LoadSurfaces()
{
	if (!m_pClientDE || !m_pRiotMenu) return LTFALSE;

	CBitmapFont* pFontNormal = m_pRiotMenu->GetFont12n();
	CBitmapFont* pFontSelected = m_pRiotMenu->GetFont12s();
	if (!pFontNormal || !pFontSelected) return LTFALSE;

	m_GenericItem[ID_SKIPCUTSCENES].hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_GENERAL_SKIPCUTSCENES);
	m_GenericItem[ID_BACK].hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_BACK);

	m_GenericItem[ID_SKIPCUTSCENES].hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_GENERAL_SKIPCUTSCENES);
	m_GenericItem[ID_BACK].hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_BACK);

	RebuildSkipCutscenesSurfaces();

	m_hMenuTitle = CTextHelper::CreateSurfaceFromString (m_pClientDE, m_pRiotMenu->GetFont18n(), IDS_TITLE_GENERAL);
	if (!m_hMenuTitle) { UnloadSurfaces(); return LTFALSE; }
	m_pClientDE->GetSurfaceDims (m_hMenuTitle, &m_szMenuTitle.cx, &m_szMenuTitle.cy);

	for (int i = 0; i < NUM_GENERAL_ITEMS; i++)
	{
		if (!m_GenericItem[i].hMenuItem || !m_GenericItem[i].hMenuItemSelected)
		{
			UnloadSurfaces();
			return LTFALSE;
		}
	}

	if (!m_SkipCutscenes.hMenuItem || !m_SkipCutscenes.hMenuItemSelected)
	{
		UnloadSurfaces();
		return LTFALSE;
	}

	for (int i = 0; i < NUM_GENERAL_ITEMS; i++)
	{
		m_pClientDE->GetSurfaceDims (m_GenericItem[i].hMenuItem, &m_GenericItem[i].szMenuItem.cx, &m_GenericItem[i].szMenuItem.cy);
	}

	return CBaseMenu::LoadSurfaces();
}

void CGeneralOptionsMenu::UnloadSurfaces()
{
	if (!m_pClientDE) return;

	for (int i = 0; i < NUM_GENERAL_ITEMS; i++)
	{
		if (m_GenericItem[i].hMenuItem) m_pClientDE->DeleteSurface (m_GenericItem[i].hMenuItem);
		if (m_GenericItem[i].hMenuItemSelected) m_pClientDE->DeleteSurface (m_GenericItem[i].hMenuItemSelected);
		m_GenericItem[i].hMenuItem = LTNULL;
		m_GenericItem[i].hMenuItemSelected = LTNULL;
		m_GenericItem[i].szMenuItem.cx = m_GenericItem[i].szMenuItem.cy = 0;
	}

	if (m_SkipCutscenes.hMenuItem) m_pClientDE->DeleteSurface (m_SkipCutscenes.hMenuItem);
	if (m_SkipCutscenes.hMenuItemSelected) m_pClientDE->DeleteSurface (m_SkipCutscenes.hMenuItemSelected);
	m_SkipCutscenes.hMenuItem = LTNULL;
	m_SkipCutscenes.hMenuItemSelected = LTNULL;

	if (m_hMenuTitle) m_pClientDE->DeleteSurface (m_hMenuTitle);
	m_hMenuTitle = LTNULL;

	CBaseMenu::UnloadSurfaces();
}

void CGeneralOptionsMenu::PostCalculateMenuDims()
{
	if (!m_pClientDE) return;

	uint32 nWidth, nHeight;
	int nLabelMax = 0;
	for (int i = 0; i < NUM_GENERAL_ITEMS; i++)
	{
		if (!m_GenericItem[i].hMenuItem) continue;
		m_pClientDE->GetSurfaceDims (m_GenericItem[i].hMenuItem, &nWidth, &nHeight);
		if ((int)nWidth > nLabelMax) nLabelMax = (int)nWidth;
	}

	m_nSecondColumn = nLabelMax + 20;

	int nMenuMaxWidth = m_nSecondColumn;
	if (m_SkipCutscenes.hMenuItem)
	{
		m_pClientDE->GetSurfaceDims (m_SkipCutscenes.hMenuItem, &nWidth, &nHeight);
		nMenuMaxWidth = m_nSecondColumn + (int)nWidth;
	}

	m_nMenuX = GetMenuAreaLeft() + ((int)m_szMenuArea.cx - nMenuMaxWidth) / 2;
}
