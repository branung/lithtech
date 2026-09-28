#include "clientheaders.h"
#include "OptionsMenu.h"
#include "TextHelper.h"
#include "ClientRes.h"
#include "Font28.h"
#include "RiotMenu.h"
#include "RiotClientShell.h"

LTBOOL COptionsMenu::Init (ILTClient* pClientDE, CRiotMenu* pRiotMenu, CBaseMenu* pParent, int nScreenWidth, int nScreenHeight)
{
	if (!CBaseMenu::Init (pClientDE, pRiotMenu, pParent, nScreenWidth, nScreenHeight)) return LTFALSE;

	if (!m_DisplayOptionsMenu.Init (pClientDE, pRiotMenu, this, nScreenWidth, nScreenHeight)) return LTFALSE;
	if (!m_SoundOptionsMenu.Init (pClientDE, pRiotMenu, this, nScreenWidth, nScreenHeight)) return LTFALSE;
	if (!m_KeyboardMenu.Init (pClientDE, pRiotMenu, this, nScreenWidth, nScreenHeight)) return LTFALSE;
	if (!m_MouseMenu.Init (pClientDE, pRiotMenu, this, nScreenWidth, nScreenHeight)) return LTFALSE;
	if (!m_JoystickMenu.Init (pClientDE, pRiotMenu, this, nScreenWidth, nScreenHeight)) return LTFALSE;
	if (!m_GeneralOptionsMenu.Init (pClientDE, pRiotMenu, this, nScreenWidth, nScreenHeight)) return LTFALSE;

	return LTTRUE;
}

void COptionsMenu::ScreenDimsChanged (int nScreenWidth, int nScreenHeight)
{
	m_DisplayOptionsMenu.ScreenDimsChanged (nScreenWidth, nScreenHeight);
	m_SoundOptionsMenu.ScreenDimsChanged (nScreenWidth, nScreenHeight);
	m_KeyboardMenu.ScreenDimsChanged (nScreenWidth, nScreenHeight);
	m_MouseMenu.ScreenDimsChanged (nScreenWidth, nScreenHeight);
	m_JoystickMenu.ScreenDimsChanged (nScreenWidth, nScreenHeight);
	m_GeneralOptionsMenu.ScreenDimsChanged (nScreenWidth, nScreenHeight);

	CBaseMenu::ScreenDimsChanged (nScreenWidth, nScreenHeight);
}

#define OPT_GENERAL		0
#define OPT_SCREEN		1
#define OPT_SOUND		2
#define OPT_KEYBOARD	3
#define OPT_MOUSE		4
#define OPT_JOYSTICK	5
#define OPT_BACK		6

#define NUM_OPTIONS		7

void COptionsMenu::Return()
{
	if (!m_pClientDE || !m_pRiotMenu) return;

	if (m_nSelection == OPT_GENERAL)
	{
		m_GeneralOptionsMenu.Reset();
		m_pRiotMenu->SetCurrentMenu (&m_GeneralOptionsMenu);
	}
	else if (m_nSelection == OPT_SCREEN)
	{
		m_DisplayOptionsMenu.Reset();
		m_pRiotMenu->SetCurrentMenu (&m_DisplayOptionsMenu);
	}
	else if (m_nSelection == OPT_SOUND)
	{
		CRiotClientShell* pClientShell = m_pRiotMenu->GetClientShell();
		if (!pClientShell) return;

		if (!pClientShell->SoundInited())
		{
			pClientShell->DoMessageBox (IDS_SOUNDNOTINITED, TH_ALIGN_CENTER);
			return;
		}

		m_SoundOptionsMenu.Reset();
		m_pRiotMenu->SetCurrentMenu (&m_SoundOptionsMenu);
	}
	else if (m_nSelection == OPT_KEYBOARD)
	{
		m_KeyboardMenu.Reset();
		m_pRiotMenu->SetCurrentMenu (&m_KeyboardMenu);
	}
	else if (m_nSelection == OPT_MOUSE)
	{
		m_MouseMenu.Reset();
		m_pRiotMenu->SetCurrentMenu (&m_MouseMenu);
	}
	else if (m_nSelection == OPT_JOYSTICK)
	{
		if (m_JoystickMenu.JoystickEnabled())
		{
			if (!m_JoystickMenu.JoystickMenuDisabled())
			{
				m_JoystickMenu.Reset();
				m_pRiotMenu->SetCurrentMenu (&m_JoystickMenu);
			}
			else
			{
				CRiotClientShell* pClientShell = m_pRiotMenu->GetClientShell();
				if (pClientShell) pClientShell->DoMessageBox (IDS_JOYSTICKMENUDISABLED, TH_ALIGN_CENTER);
			}
		}
		else
		{
			CRiotClientShell* pClientShell = m_pRiotMenu->GetClientShell();
			if (pClientShell) pClientShell->DoMessageBox (IDS_NOJOYSTICKDETECTED, TH_ALIGN_CENTER);
		}
	}
	else if (m_nSelection == OPT_BACK)
	{
		m_pRiotMenu->SetCurrentMenu (m_pParent);
	}

	CBaseMenu::Return();
}

LTBOOL COptionsMenu::LoadSurfaces()
{
	if (!m_pClientDE || !m_pRiotMenu) return LTFALSE;

	CBitmapFont* pFontNormal = LTNULL;
	CBitmapFont* pFontSelected = LTNULL;
	if (m_szScreen.cx < 512)
	{
		pFontNormal = m_pRiotMenu->GetFont12n();
		pFontSelected = m_pRiotMenu->GetFont12s();
	}
	else if (m_szScreen.cx < 640)
	{
		pFontNormal = m_pRiotMenu->GetFont18n();
		pFontSelected = m_pRiotMenu->GetFont18s();
	}
	else
	{
		pFontNormal = m_pRiotMenu->GetFont28n();
		pFontSelected = m_pRiotMenu->GetFont28s();
	}

	m_GenericItem[OPT_GENERAL].hMenuItem  = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_GENERAL, IDS_MENUREPLACEMENTFONT);
	m_GenericItem[OPT_SCREEN].hMenuItem   = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_SCREEN, IDS_MENUREPLACEMENTFONT);
	m_GenericItem[OPT_SOUND].hMenuItem    = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_SOUND, IDS_MENUREPLACEMENTFONT);
	m_GenericItem[OPT_KEYBOARD].hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_KEYBOARD, IDS_MENUREPLACEMENTFONT);
	m_GenericItem[OPT_MOUSE].hMenuItem    = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_MOUSE, IDS_MENUREPLACEMENTFONT);
	m_GenericItem[OPT_JOYSTICK].hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_JOYSTICK, IDS_MENUREPLACEMENTFONT);
	m_GenericItem[OPT_BACK].hMenuItem     = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_BACK, IDS_MENUREPLACEMENTFONT);
	
	m_GenericItem[OPT_GENERAL].hMenuItemSelected  = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_GENERAL, IDS_MENUREPLACEMENTFONT);
	m_GenericItem[OPT_SCREEN].hMenuItemSelected   = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_SCREEN, IDS_MENUREPLACEMENTFONT);
	m_GenericItem[OPT_SOUND].hMenuItemSelected    = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_SOUND, IDS_MENUREPLACEMENTFONT);
	m_GenericItem[OPT_KEYBOARD].hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_KEYBOARD, IDS_MENUREPLACEMENTFONT);
	m_GenericItem[OPT_MOUSE].hMenuItemSelected    = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_MOUSE, IDS_MENUREPLACEMENTFONT);
	m_GenericItem[OPT_JOYSTICK].hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_JOYSTICK, IDS_MENUREPLACEMENTFONT);
	m_GenericItem[OPT_BACK].hMenuItemSelected     = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_BACK, IDS_MENUREPLACEMENTFONT);
	
	m_hMenuTitle = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_TITLE_OPTIONS, IDS_MENUREPLACEMENTFONT);
	m_pClientDE->GetSurfaceDims (m_hMenuTitle, &m_szMenuTitle.cx, &m_szMenuTitle.cy);
	
	for (int i = 0; i < NUM_OPTIONS; i++)
	{
		if (!m_GenericItem[i].hMenuItem || !m_GenericItem[i].hMenuItemSelected)
		{
			UnloadSurfaces();
			return LTFALSE;
		}
	}

	for (int i = 0; i < NUM_OPTIONS; i++)
	{
		m_pClientDE->GetSurfaceDims (m_GenericItem[i].hMenuItem, &m_GenericItem[i].szMenuItem.cx, &m_GenericItem[i].szMenuItem.cy);
	}
	
	return CBaseMenu::LoadSurfaces();
}

void COptionsMenu::UnloadSurfaces()
{
	if (!m_pClientDE) return;

	for (int i = 0; i < NUM_OPTIONS; i++)
	{
		if (m_GenericItem[i].hMenuItem) m_pClientDE->DeleteSurface (m_GenericItem[i].hMenuItem);
		if (m_GenericItem[i].hMenuItemSelected) m_pClientDE->DeleteSurface (m_GenericItem[i].hMenuItemSelected);
		m_GenericItem[i].hMenuItem = LTNULL;
		m_GenericItem[i].hMenuItemSelected = LTNULL;
		m_GenericItem[i].szMenuItem.cx = m_GenericItem[i].szMenuItem.cy = 0;
	}

	if (m_hMenuTitle) m_pClientDE->DeleteSurface (m_hMenuTitle);
	m_hMenuTitle = LTNULL;
	
	CBaseMenu::UnloadSurfaces();
}

