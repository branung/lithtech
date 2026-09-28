#include "clientheaders.h"
#include "DisplayOptionsMenu.h"
#include "TextHelper.h"
#include "ClientRes.h"
#include "RiotMenu.h"
#include "ClientUtilities.h"
#include <stdio.h>

extern CommandID g_CommandArray[];

#define THIS_MENU_SPACING			3
#define CURRENT_SETTING_SPACING		2

// Row ids, in screen order
#define ID_CHANGE					0
#define ID_RENDERERINFO				1	// Spacer: the renderer/resolution readout
#define ID_GORE						2
#define ID_SCREENFLASH				3
#define ID_DETAIL					4
#define ID_ADVANCED					5
#define ID_FRAMERATE				6
#define ID_HUDSCALE					7
#define ID_BACK						8
#define NUM_DISPLAY_ITEMS			9

#define max(a,b)	((a) > (b) ? (a) : (b))

/*
	Frame rate caps offered in the menu

	The engine clamps MaxFPS to [1, 200], so the list stops there.
*/
static const int kFrameRateCaps[] = { 60, 100, 144, 200 };
static const int kNumFrameRateCaps = sizeof(kFrameRateCaps) / sizeof(kFrameRateCaps[0]);

/*
	HUD scaling as a percentage

	The slider stops are 100, 125, 150... up to 300.
	The bounds match HUDSCALE_MIN and HUDSCALE_MAX in ClientUtilities.h.
*/
#define HUDSCALE_PCT_MIN			100
#define HUDSCALE_PCT_STEP			25
#define HUDSCALE_STOPS				9

static int HUDScalePctFromStop(int nStop)
{
	if (nStop < 0) nStop = 0;
	if (nStop > HUDSCALE_STOPS - 1) nStop = HUDSCALE_STOPS - 1;
	return HUDSCALE_PCT_MIN + (nStop * HUDSCALE_PCT_STEP);
}

static int HUDScaleStopFromPct(int nPct)
{
	int nStop = ((nPct - HUDSCALE_PCT_MIN) + (HUDSCALE_PCT_STEP / 2)) / HUDSCALE_PCT_STEP;
	if (nStop < 0) nStop = 0;
	if (nStop > HUDSCALE_STOPS - 1) nStop = HUDSCALE_STOPS - 1;
	return nStop;
}

static int FrameRateStringID(int nIndex)
{
	switch (nIndex)
	{
		case 1:  return IDS_DISPLAY_FRAMERATE_100;
		case 2:  return IDS_DISPLAY_FRAMERATE_144;
		case 3:  return IDS_DISPLAY_FRAMERATE_200;
		default: return IDS_DISPLAY_FRAMERATE_60;
	}
}


CDisplayOptionsMenu::CDisplayOptionsMenu() : CBaseMenu()
{
	m_nSecondColumn = 0;
	m_fOriginalDetailLevel = 0.0f;
}

LTBOOL CDisplayOptionsMenu::Init (ILTClient* pClientDE, CRiotMenu* pRiotMenu, CBaseMenu* pParent, int nScreenWidth, int nScreenHeight)
{
	if (!pClientDE || !pRiotMenu) return LTFALSE;

	CRiotSettings* pSettings = pRiotMenu->GetSettings();
	if (!pSettings) return LTFALSE;
	
	// init the detail settings menu

	if (!m_DisplayModeMenu.Init (pClientDE, pRiotMenu, this, nScreenWidth, nScreenHeight)) return LTFALSE;
	if (!m_DetailSettingsMenu.Init (pClientDE, pRiotMenu, this, nScreenWidth, nScreenHeight)) return LTFALSE;

	// Init the HUD scale slider (LoadSurfaces sets its position)
	m_sliderHUDScale.Init (pClientDE, 60, HUDSCALE_STOPS);
	m_sliderHUDScale.SetEnabled();

	// call the base class Init() function

	LTBOOL bSuccess = CBaseMenu::Init (pClientDE, pRiotMenu, pParent, nScreenWidth, nScreenHeight);

	if (nScreenWidth < 512)
	{
		m_nSecondColumn = 135;
	}
	else if (nScreenWidth < 640)
	{
		m_nSecondColumn = 135;
	}
	else
	{
		m_nSecondColumn = 200;
	}

	return bSuccess;
}

void CDisplayOptionsMenu::ScreenDimsChanged (int nScreenWidth, int nScreenHeight)
{
	m_DisplayModeMenu.ScreenDimsChanged (nScreenWidth, nScreenHeight);
	m_DetailSettingsMenu.ScreenDimsChanged (nScreenWidth, nScreenHeight);

	CBaseMenu::ScreenDimsChanged (nScreenWidth, nScreenHeight);

	if (nScreenWidth < 512)
	{
		m_nSecondColumn = 135;
	}
	else if (nScreenWidth < 640)
	{
		m_nSecondColumn = 135;
	}
	else
	{
		m_nSecondColumn = 200;
	}
}

void CDisplayOptionsMenu::Reset()
{
	if (!m_pRiotMenu) return;

	CRiotSettings* pSettings = m_pRiotMenu->GetSettings();
	if (!pSettings) return;
	
	m_fOriginalDetailLevel = pSettings->Detail[RS_DET_OVERALL].nValue;
	CBaseMenu::Reset();
}

void CDisplayOptionsMenu::Up()
{
	if (!TextHelperCheckStringID(m_pClientDE, IDS_ALLOW_NO_GORE, "TRUE"))
	{
		if (m_nSelection == ID_GORE) m_nSelection = ID_RENDERERINFO;
	}
	else
	{
		if (m_nSelection == ID_SCREENFLASH) m_nSelection = ID_RENDERERINFO;
	}

	CBaseMenu::Up();
}

void CDisplayOptionsMenu::Down()
{
	if (!TextHelperCheckStringID(m_pClientDE, IDS_ALLOW_NO_GORE, "TRUE"))
	{
		if (m_nSelection == ID_CHANGE) m_nSelection = ID_RENDERERINFO;
	}
	else
	{
		if (m_nSelection == ID_RENDERERINFO) m_nSelection = ID_GORE;
	}

	CBaseMenu::Down();
}

void CDisplayOptionsMenu::Left()
{
	if (!m_pRiotMenu || !m_pClientDE) return;

	CRiotSettings* pSettings = m_pRiotMenu->GetSettings();
	if (!pSettings) return;

	if (m_nSelection == ID_FRAMERATE)
	{
		m_nFrameRate--;
		if (m_nFrameRate < 0) m_nFrameRate = kNumFrameRateCaps - 1;
		RebuildFrameRateSurfaces();
		SaveFrameRateSetting();
		return;
	}

	if (m_nSelection == ID_HUDSCALE)
	{
		// Saved on every step so that it's in place if the player goes straight back to the game
		if (m_sliderHUDScale.DecPos()) SaveHUDScaleSetting();
		return;
	}

	if (m_nSelection == ID_GORE)
	{
		CBitmapFont* pFontNormal = LTNULL;
		CBitmapFont* pFontSelected = LTNULL;
		if (m_szScreen.cy < 400)
		{
			pFontNormal = m_pRiotMenu->GetFont08n();
			pFontSelected = m_pRiotMenu->GetFont08s();
		}
		else
		{
			pFontNormal = m_pRiotMenu->GetFont12n();
			pFontSelected = m_pRiotMenu->GetFont12s();
		}

		pSettings->Detail[RS_DET_GORE].nValue = !pSettings->Detail[RS_DET_GORE].nValue;

		if (m_GoreSetting.hMenuItem) m_pClientDE->DeleteSurface (m_GoreSetting.hMenuItem);
		if (m_GoreSetting.hMenuItemSelected) m_pClientDE->DeleteSurface (m_GoreSetting.hMenuItemSelected);

		m_GoreSetting.hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, pSettings->Detail[RS_DET_GORE].nValue ? IDS_ON : IDS_OFF);
		m_GoreSetting.hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, pSettings->Detail[RS_DET_GORE].nValue ? IDS_ON : IDS_OFF);

		pSettings->WriteDetailSettings();
	}
	else if (m_nSelection == ID_SCREENFLASH)
	{
		CBitmapFont* pFontNormal = LTNULL;
		CBitmapFont* pFontSelected = LTNULL;
		if (m_szScreen.cy < 400)
		{
			pFontNormal = m_pRiotMenu->GetFont08n();
			pFontSelected = m_pRiotMenu->GetFont08s();
		}
		else
		{
			pFontNormal = m_pRiotMenu->GetFont12n();
			pFontSelected = m_pRiotMenu->GetFont12s();
		}

		pSettings->Misc[RS_MISC_SCREENFLASH].nValue = !pSettings->Misc[RS_MISC_SCREENFLASH].nValue;

		if (m_ScreenFlash.hMenuItem) m_pClientDE->DeleteSurface (m_ScreenFlash.hMenuItem);
		if (m_ScreenFlash.hMenuItemSelected) m_pClientDE->DeleteSurface (m_ScreenFlash.hMenuItemSelected);

		m_ScreenFlash.hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, pSettings->ScreenFlash() ? IDS_ON : IDS_OFF);
		m_ScreenFlash.hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, pSettings->ScreenFlash() ? IDS_ON : IDS_OFF);

		HLTCOLOR hTrans = m_pClientDE->SetupColor2 (0.0f, 0.0f, 0.0f, LTTRUE);

		pSettings->WriteDetailSettings();
	}
	else if (m_nSelection == ID_DETAIL)
	{
		CBitmapFont* pFontNormal = LTNULL;
		CBitmapFont* pFontSelected = LTNULL;
		if (m_szScreen.cy < 400)
		{
			pFontNormal = m_pRiotMenu->GetFont08n();
			pFontSelected = m_pRiotMenu->GetFont08s();
		}
		else
		{
			pFontNormal = m_pRiotMenu->GetFont12n();
			pFontSelected = m_pRiotMenu->GetFont12s();
		}

		pSettings->Detail[RS_DET_OVERALL].nValue--;
		if (pSettings->Detail[RS_DET_OVERALL].nValue < 0) pSettings->Detail[RS_DET_OVERALL].nValue = 2;

		if (m_DetailSetting.hMenuItem) m_pClientDE->DeleteSurface (m_DetailSetting.hMenuItem);
		if (m_DetailSetting.hMenuItemSelected) m_pClientDE->DeleteSurface (m_DetailSetting.hMenuItemSelected);

		int nDetailStringID = 0;
		switch ((int)pSettings->Detail[RS_DET_OVERALL].nValue)
		{
			default:	nDetailStringID = IDS_LOW;		pSettings->SetLowDetail();	break;
			case 1:		nDetailStringID = IDS_MEDIUM;	pSettings->SetMedDetail();	break;
			case 2:		nDetailStringID = IDS_HIGH;		pSettings->SetHiDetail();	break;
		}
		m_DetailSetting.hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, nDetailStringID);

		m_DetailSetting.hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, nDetailStringID);

		// implement the detail settings that might have changed...

		for (int i = RS_SUBDET_FIRST; i <= RS_SUBDET_LAST; i++)
		{
			pSettings->ImplementDetailSetting (i);
		}
	}
	
	CBaseMenu::Left();
}

void CDisplayOptionsMenu::Right()
{
	if (!m_pRiotMenu || !m_pClientDE) return;

	CRiotSettings* pSettings = m_pRiotMenu->GetSettings();
	if (!pSettings) return;

	if (m_nSelection == ID_FRAMERATE)
	{
		m_nFrameRate++;
		if (m_nFrameRate >= kNumFrameRateCaps) m_nFrameRate = 0;
		RebuildFrameRateSurfaces();
		SaveFrameRateSetting();
		return;
	}

	if (m_nSelection == ID_HUDSCALE)
	{
		if (m_sliderHUDScale.IncPos()) SaveHUDScaleSetting();
		return;
	}

	if (m_nSelection == ID_GORE)
	{
		Left();
		return;
	}
	else if (m_nSelection == ID_SCREENFLASH)
	{
		Left();
		return;
	}
	else if (m_nSelection == ID_DETAIL)
	{
		CBitmapFont* pFontNormal = LTNULL;
		CBitmapFont* pFontSelected = LTNULL;
		if (m_szScreen.cy < 400)
		{
			pFontNormal = m_pRiotMenu->GetFont08n();
			pFontSelected = m_pRiotMenu->GetFont08s();
		}
		else
		{
			pFontNormal = m_pRiotMenu->GetFont12n();
			pFontSelected = m_pRiotMenu->GetFont12s();
		}

		pSettings->Detail[RS_DET_OVERALL].nValue++;
		if (pSettings->Detail[RS_DET_OVERALL].nValue > 2) pSettings->Detail[RS_DET_OVERALL].nValue = 0;

		if (m_DetailSetting.hMenuItem) m_pClientDE->DeleteSurface (m_DetailSetting.hMenuItem);
		if (m_DetailSetting.hMenuItemSelected) m_pClientDE->DeleteSurface (m_DetailSetting.hMenuItemSelected);

		int nDetailStringID = 0;
		switch ((int)pSettings->Detail[RS_DET_OVERALL].nValue)
		{
			default:	nDetailStringID = IDS_LOW;		pSettings->SetLowDetail();	break;
			case 1:		nDetailStringID = IDS_MEDIUM;	pSettings->SetMedDetail();	break;
			case 2:		nDetailStringID = IDS_HIGH;		pSettings->SetHiDetail();	break;
		}
		m_DetailSetting.hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, nDetailStringID);
		m_DetailSetting.hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, nDetailStringID);

		// implement the detail settings that might have changed...

		for (int i = RS_SUBDET_FIRST; i <= RS_SUBDET_LAST; i++)
		{
			pSettings->ImplementDetailSetting (i);
		}
	}
	
	CBaseMenu::Right();
}

void CDisplayOptionsMenu::PageUp()
{
	CBaseMenu::PageUp();
}

void CDisplayOptionsMenu::PageDown()
{
	CBaseMenu::PageUp();
}

void CDisplayOptionsMenu::Home()
{
	CBaseMenu::Home();
}

void CDisplayOptionsMenu::End()
{
	CBaseMenu::End();
}

void CDisplayOptionsMenu::Return()
{
	if (!m_pRiotMenu) return;

	if (m_nSelection == ID_CHANGE)
	{
		m_DisplayModeMenu.Reset();
		m_pRiotMenu->SetCurrentMenu (&m_DisplayModeMenu);
		CBaseMenu::Return();
	}
	else if (m_nSelection == ID_ADVANCED)
	{
		m_DetailSettingsMenu.Reset();
		m_pRiotMenu->SetCurrentMenu (&m_DetailSettingsMenu);
		CBaseMenu::Return();
	}
	else if (m_nSelection == ID_BACK)
	{
		m_pRiotMenu->SetCurrentMenu (m_pParent);
		CBaseMenu::Return();
	}
}

void CDisplayOptionsMenu::Esc()
{
	if (!m_pClientDE || !m_pRiotMenu) return;

	CRiotSettings* pSettings = m_pRiotMenu->GetSettings();
	if (!pSettings) return;
	
	if (m_fOriginalDetailLevel != pSettings->Detail[RS_DET_OVERALL].nValue)
	{
		m_pClientDE->RunConsoleString ("rebindtextures");
	}

	CBaseMenu::Esc();
}

void CDisplayOptionsMenu::Draw (HSURFACE hScreen, int nScreenWidth, int nScreenHeight, int nTextOffset)
{
	if (!m_pClientDE) return;

	if (!TextHelperCheckStringID(m_pClientDE, IDS_ALLOW_NO_GORE, "TRUE"))
	{
		CBaseMenu::Draw (hScreen, nScreenWidth, nScreenHeight, nTextOffset);
	}
	else
	{
		DrawNoGoreVersion (hScreen, nScreenWidth, nScreenHeight, nTextOffset);
	}

	int x = m_nMenuX;
	int nCurrentSettingX = x + 30;
	int y = m_nMenuY + m_szMenuTitle.cy + m_nMenuTitleSpacing + m_GenericItem[ID_CHANGE].szMenuItem.cy + (int) (m_nMenuSpacing + CURRENT_SETTING_SPACING);
	
	// THIS_MENU_SPACING between the readout's lines, as the spacer row in LoadSurfaces assumes
	m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, m_RendererLine1.hMenuItem, LTNULL, nCurrentSettingX, y, LTNULL);
	if (m_RendererLine1.hMenuItem) y += m_RendererLine1.szMenuItem.cy + THIS_MENU_SPACING;
	m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, m_RendererLine2.hMenuItem, LTNULL, nCurrentSettingX, y, LTNULL);
	if (m_RendererLine2.hMenuItem) y += m_RendererLine2.szMenuItem.cy + THIS_MENU_SPACING;
	m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, m_Resolution.hMenuItem, LTNULL, nCurrentSettingX, y, LTNULL);
	if (m_Resolution.hMenuItem) y += m_Resolution.szMenuItem.cy + THIS_MENU_SPACING;
	m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, m_TextureDepth.hMenuItem, LTNULL, nCurrentSettingX, y, LTNULL);

	y = m_nMenuY + m_szMenuTitle.cy + m_nMenuTitleSpacing + m_GenericItem[ID_CHANGE].szMenuItem.cy + m_GenericItem[ID_RENDERERINFO].szMenuItem.cy + (2 * m_nMenuSpacing);

	if (!TextHelperCheckStringID(m_pClientDE, IDS_ALLOW_NO_GORE, "TRUE"))
	{
		m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, m_nSelection == ID_GORE ? m_GoreSetting.hMenuItemSelected : m_GoreSetting.hMenuItem, LTNULL, m_nMenuX + m_nSecondColumn, y, LTNULL);
	}

	y += m_GenericItem[ID_GORE].szMenuItem.cy + m_nMenuSpacing;
	m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, m_nSelection == ID_SCREENFLASH ? m_ScreenFlash.hMenuItemSelected : m_ScreenFlash.hMenuItem, LTNULL, m_nMenuX + m_nSecondColumn, y, LTNULL);
	
	y += m_GenericItem[ID_SCREENFLASH].szMenuItem.cy + m_nMenuSpacing;
	m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, m_nSelection == ID_DETAIL ? m_DetailSetting.hMenuItemSelected : m_DetailSetting.hMenuItem, LTNULL, m_nMenuX + m_nSecondColumn, y, LTNULL);

	y += m_GenericItem[ID_DETAIL].szMenuItem.cy + m_nMenuSpacing;
	y += m_GenericItem[ID_ADVANCED].szMenuItem.cy + m_nMenuSpacing;
	m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, m_nSelection == ID_FRAMERATE ? m_FrameRateSetting.hMenuItemSelected : m_FrameRateSetting.hMenuItem, LTNULL, m_nMenuX + m_nSecondColumn, y, LTNULL);

	m_sliderHUDScale.SetSelected ((LTBOOL)(m_nSelection == ID_HUDSCALE));

	y += m_GenericItem[ID_FRAMERATE].szMenuItem.cy + m_nMenuSpacing;
	m_sliderHUDScale.Draw (hScreen, m_nMenuX + m_nSecondColumn, y);
}

void CDisplayOptionsMenu::RebuildFrameRateSurfaces()
{
	if (!m_pClientDE || !m_pRiotMenu) return;

	CBitmapFont* pFontNormal   = LTNULL;
	CBitmapFont* pFontSelected = LTNULL;
	if (m_szScreen.cy < 400)
	{
		pFontNormal   = m_pRiotMenu->GetFont08n();
		pFontSelected = m_pRiotMenu->GetFont08s();
	}
	else
	{
		pFontNormal   = m_pRiotMenu->GetFont12n();
		pFontSelected = m_pRiotMenu->GetFont12s();
	}

	if (m_FrameRateSetting.hMenuItem) m_pClientDE->DeleteSurface (m_FrameRateSetting.hMenuItem);
	if (m_FrameRateSetting.hMenuItemSelected) m_pClientDE->DeleteSurface (m_FrameRateSetting.hMenuItemSelected);

	const int nStringID = FrameRateStringID (m_nFrameRate);
	m_FrameRateSetting.hMenuItem         = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, nStringID);
	m_FrameRateSetting.hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, nStringID);
}

void CDisplayOptionsMenu::SaveFrameRateSetting()
{
	if (!m_pClientDE) return;
	if (m_nFrameRate < 0 || m_nFrameRate >= kNumFrameRateCaps) return;

	// '+' writes the choice back to autoexec.cfg on exit
	char szConsole[64];
	sprintf (szConsole, "+MaxFPS %d", kFrameRateCaps[m_nFrameRate]);
	m_pClientDE->RunConsoleString (szConsole);
}

void CDisplayOptionsMenu::SaveHUDScaleSetting()
{
	if (!m_pClientDE) return;

	char szConsole[64];
	sprintf (szConsole, "+HUDScale %.2f", (float)HUDScalePctFromStop (m_sliderHUDScale.GetPos()) / 100.0f);
	m_pClientDE->RunConsoleString (szConsole);
}

void CDisplayOptionsMenu::SetGlobalDetail (int nSetting)
{
	if (!m_pClientDE || !m_pRiotMenu) return;

	CBitmapFont* pFontNormal = LTNULL;
	CBitmapFont* pFontSelected = LTNULL;
	if (m_szScreen.cy < 400)
	{
		pFontNormal = m_pRiotMenu->GetFont08n();
		pFontSelected = m_pRiotMenu->GetFont08s();
	}
	else
	{
		pFontNormal = m_pRiotMenu->GetFont12n();
		pFontSelected = m_pRiotMenu->GetFont12s();
	}
	
	CRiotSettings* pSettings = m_pRiotMenu->GetSettings();
	if (!pSettings) return;
	
	pSettings->Detail[RS_DET_OVERALL].nValue = (LTFLOAT)nSetting;
	pSettings->WriteDetailSettings();

	if (m_DetailSetting.hMenuItem) m_pClientDE->DeleteSurface (m_DetailSetting.hMenuItem);
	if (m_DetailSetting.hMenuItemSelected) m_pClientDE->DeleteSurface (m_DetailSetting.hMenuItemSelected);

	int nDetailStringID = 0;
	switch ((int)pSettings->GlobalDetail())
	{
		default:	nDetailStringID = IDS_LOW;		break;
		case 1:		nDetailStringID = IDS_MEDIUM;	break;
		case 2:		nDetailStringID = IDS_HIGH;		break;
		case 3:		nDetailStringID = IDS_ADVANCED;	break;
	}

	m_DetailSetting.hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, nDetailStringID);
	m_DetailSetting.hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, nDetailStringID);
}

void CDisplayOptionsMenu::SetupCurrentRendererSurfaces()
{
	if (!m_pClientDE || !m_pRiotMenu) return;

	CRiotSettings* pSettings = m_pRiotMenu->GetSettings();
	if (!pSettings) return;
	
	CBitmapFont* pFontNormal = m_pRiotMenu->GetFont08n();
	
	RMode* pRMode = pSettings->GetRenderMode();
	if (!pRMode) return;

	if (m_RendererLine1.hMenuItem) m_pClientDE->DeleteSurface (m_RendererLine1.hMenuItem);
	if (m_RendererLine2.hMenuItem) m_pClientDE->DeleteSurface (m_RendererLine2.hMenuItem);
	if (m_Resolution.hMenuItem) m_pClientDE->DeleteSurface (m_Resolution.hMenuItem);
	if (m_TextureDepth.hMenuItem) m_pClientDE->DeleteSurface (m_TextureDepth.hMenuItem);

	char str[256];

	//sprintf (str, "(%s)", pRMode->m_RenderDLL);
	//m_RendererLine1.hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, str);
	
	sprintf (str, "%s", pRMode->m_Description);
	m_RendererLine2.hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, str);

	sprintf (str, "%d x %d", pRMode->m_Width, pRMode->m_Height);
	m_Resolution.hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, str);

	int nStringID = pSettings->Textures8Bit() ? IDS_DISPLAY_8BIT : IDS_DISPLAY_16BIT;
	m_TextureDepth.hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, nStringID);

	m_pClientDE->GetSurfaceDims (m_RendererLine1.hMenuItem, &m_RendererLine1.szMenuItem.cx, &m_RendererLine1.szMenuItem.cy);
	m_pClientDE->GetSurfaceDims (m_RendererLine2.hMenuItem, &m_RendererLine2.szMenuItem.cx, &m_RendererLine2.szMenuItem.cy);
	m_pClientDE->GetSurfaceDims (m_Resolution.hMenuItem, &m_Resolution.szMenuItem.cx, &m_Resolution.szMenuItem.cy);
	m_pClientDE->GetSurfaceDims (m_TextureDepth.hMenuItem, &m_TextureDepth.szMenuItem.cx, &m_TextureDepth.szMenuItem.cy);
}

LTBOOL CDisplayOptionsMenu::LoadSurfaces()
{
	if (!m_pClientDE || !m_pRiotMenu) return LTFALSE;

	// determine the correct setting for the music source string

	CRiotSettings* pSettings = m_pRiotMenu->GetSettings();
	if (!pSettings) return LTFALSE;
	
	// get detail string id

	int nDetailStringID = 0;
	switch ((int)pSettings->GlobalDetail())
	{
		default:	nDetailStringID = IDS_LOW;		break;
		case 1:		nDetailStringID = IDS_MEDIUM;	break;
		case 2:		nDetailStringID = IDS_HIGH;		break;
		case 3:		nDetailStringID = IDS_ADVANCED;	break;
	}

	// create the menu surfaces

	CBitmapFont* pFontNormal = LTNULL;
	CBitmapFont* pFontSelected = LTNULL;
	if (m_szScreen.cy < 400)
	{
		pFontNormal = m_pRiotMenu->GetFont08n();
		pFontSelected = m_pRiotMenu->GetFont08s();
	}
	else
	{
		pFontNormal = m_pRiotMenu->GetFont12n();
		pFontSelected = m_pRiotMenu->GetFont12s();
	}
	CBitmapFont* pFontTitle = m_pRiotMenu->GetFont12n();

	m_GenericItem[ID_CHANGE].hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_DISPLAY_CHANGE);
	m_GenericItem[ID_GORE].hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_DISPLAY_GORE);
	m_GenericItem[ID_SCREENFLASH].hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_DISPLAY_SCREENFLASH);
	m_GenericItem[ID_DETAIL].hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_DISPLAY_DETAIL);
	m_GenericItem[ID_ADVANCED].hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_DISPLAY_ADVANCED);
	m_GenericItem[ID_FRAMERATE].hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_DISPLAY_FRAMERATE);
	m_GenericItem[ID_HUDSCALE].hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_DISPLAY_HUDSCALE);
	m_GenericItem[ID_BACK].hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, IDS_BACK);

	m_GoreSetting.hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, pSettings->Gore() ? IDS_ON : IDS_OFF);
	m_ScreenFlash.hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, pSettings->ScreenFlash() ? IDS_ON : IDS_OFF);
	m_DetailSetting.hMenuItem = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, nDetailStringID);

	m_GenericItem[ID_CHANGE].hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_DISPLAY_CHANGE);
	m_GenericItem[ID_GORE].hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_DISPLAY_GORE);
	m_GenericItem[ID_SCREENFLASH].hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_DISPLAY_SCREENFLASH);
	m_GenericItem[ID_DETAIL].hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_DISPLAY_DETAIL);
	m_GenericItem[ID_ADVANCED].hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_DISPLAY_ADVANCED);
	m_GenericItem[ID_FRAMERATE].hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_DISPLAY_FRAMERATE);
	m_GenericItem[ID_HUDSCALE].hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_DISPLAY_HUDSCALE);
	m_GenericItem[ID_BACK].hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, IDS_BACK);

	m_GoreSetting.hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, pSettings->Gore() ? IDS_ON : IDS_OFF);
	m_ScreenFlash.hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, pSettings->ScreenFlash() ? IDS_ON : IDS_OFF);
	m_DetailSetting.hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, nDetailStringID);

	// Frame Rate Cap: read MaxFPS and snap to the nearest cap in the list
	m_nFrameRate = 0;
	HCONSOLEVAR hFPSVar = m_pClientDE->GetConsoleVar ("MaxFPS");
	if (hFPSVar)
	{
		int nCur = (int)m_pClientDE->GetVarValueFloat (hFPSVar);
		if (nCur > 0)
		{
			int nBest = 0;
			for (int n = 1; n < kNumFrameRateCaps; n++)
			{
				if (abs (kFrameRateCaps[n] - nCur) < abs (kFrameRateCaps[nBest] - nCur)) nBest = n;
			}
			m_nFrameRate = nBest;
		}
	}
	m_FrameRateSetting.hMenuItem         = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontNormal, FrameRateStringID (m_nFrameRate));
	m_FrameRateSetting.hMenuItemSelected = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontSelected, FrameRateStringID (m_nFrameRate));

	// HUD Scale: read HUDScale and put the slider on the nearest stop
	int nHUDPct = HUDSCALE_PCT_MIN;
	HCONSOLEVAR hHUDVar = m_pClientDE->GetConsoleVar ("HUDScale");
	if (hHUDVar)
	{
		nHUDPct = (int)((m_pClientDE->GetVarValueFloat (hHUDVar) * 100.0f) + 0.5f);
	}
	m_sliderHUDScale.SetPos (HUDScaleStopFromPct (nHUDPct));

	m_hMenuTitle = CTextHelper::CreateSurfaceFromString (m_pClientDE, pFontTitle, IDS_TITLE_DISPLAYOPTIONS);
	m_pClientDE->GetSurfaceDims (m_hMenuTitle, &m_szMenuTitle.cx, &m_szMenuTitle.cy);
	
	SetupCurrentRendererSurfaces();
	
	const GENERIC_ITEM* pReadout[4] = { &m_RendererLine1, &m_RendererLine2, &m_Resolution, &m_TextureDepth };
	uint32 nSpacerHeight = CURRENT_SETTING_SPACING;
	int nReadoutLines = 0;
	for (int r = 0; r < 4; r++)
	{
		if (!pReadout[r]->hMenuItem) continue;
		nSpacerHeight += pReadout[r]->szMenuItem.cy;
		nReadoutLines++;
	}
	if (nReadoutLines > 1) nSpacerHeight += (nReadoutLines - 1) * THIS_MENU_SPACING;
	if (nSpacerHeight < 1) nSpacerHeight = 1;
	m_GenericItem[ID_RENDERERINFO].hMenuItem = m_pClientDE->CreateSurface (1, nSpacerHeight);
	m_GenericItem[ID_RENDERERINFO].hMenuItemSelected = m_pClientDE->CreateSurface (1, nSpacerHeight);
	m_pClientDE->FillRect (m_GenericItem[ID_RENDERERINFO].hMenuItem, LTNULL, LTNULL);
	m_pClientDE->FillRect (m_GenericItem[ID_RENDERERINFO].hMenuItemSelected, LTNULL, LTNULL);

	for (int i = 0; i < NUM_DISPLAY_ITEMS; i++)
	{
		if (!m_GenericItem[i].hMenuItem || !m_GenericItem[i].hMenuItemSelected)
		{
			UnloadSurfaces();
			return LTFALSE;
		}
	}

	if (!m_GoreSetting.hMenuItem || !m_DetailSetting.hMenuItem || !m_ScreenFlash.hMenuItem ||
		!m_GoreSetting.hMenuItemSelected || !m_DetailSetting.hMenuItemSelected || !m_ScreenFlash.hMenuItem ||
		!m_FrameRateSetting.hMenuItem || !m_FrameRateSetting.hMenuItemSelected)
	{
		UnloadSurfaces();
		return LTFALSE;
	}

	for (int i = 0; i < NUM_DISPLAY_ITEMS; i++)
	{
		m_pClientDE->GetSurfaceDims (m_GenericItem[i].hMenuItem, &m_GenericItem[i].szMenuItem.cx, &m_GenericItem[i].szMenuItem.cy);
	}

	return CBaseMenu::LoadSurfaces();
}

void CDisplayOptionsMenu::UnloadSurfaces()
{
	if (!m_pClientDE) return;

	for (int i = 0; i < NUM_DISPLAY_ITEMS; i++)
	{
		if (m_GenericItem[i].hMenuItem) m_pClientDE->DeleteSurface (m_GenericItem[i].hMenuItem);
		if (m_GenericItem[i].hMenuItemSelected) m_pClientDE->DeleteSurface (m_GenericItem[i].hMenuItemSelected);
		m_GenericItem[i].hMenuItem = LTNULL;
		m_GenericItem[i].hMenuItemSelected = LTNULL;
		m_GenericItem[i].szMenuItem.cx = m_GenericItem[i].szMenuItem.cy = 0;
	}
	
	if (m_GoreSetting.hMenuItem) m_pClientDE->DeleteSurface (m_GoreSetting.hMenuItem);
	if (m_GoreSetting.hMenuItemSelected) m_pClientDE->DeleteSurface (m_GoreSetting.hMenuItemSelected);
	m_GoreSetting.hMenuItem = LTNULL;
	m_GoreSetting.hMenuItemSelected = LTNULL;

	if (m_ScreenFlash.hMenuItem) m_pClientDE->DeleteSurface (m_ScreenFlash.hMenuItem);
	if (m_ScreenFlash.hMenuItemSelected) m_pClientDE->DeleteSurface (m_ScreenFlash.hMenuItemSelected);
	m_ScreenFlash.hMenuItem = LTNULL;
	m_ScreenFlash.hMenuItemSelected = LTNULL;

	if (m_DetailSetting.hMenuItem) m_pClientDE->DeleteSurface (m_DetailSetting.hMenuItem);
	if (m_DetailSetting.hMenuItemSelected) m_pClientDE->DeleteSurface (m_DetailSetting.hMenuItemSelected);
	m_DetailSetting.hMenuItem = LTNULL;
	m_DetailSetting.hMenuItemSelected = LTNULL;

	if (m_FrameRateSetting.hMenuItem) m_pClientDE->DeleteSurface (m_FrameRateSetting.hMenuItem);
	if (m_FrameRateSetting.hMenuItemSelected) m_pClientDE->DeleteSurface (m_FrameRateSetting.hMenuItemSelected);
	m_FrameRateSetting.hMenuItem = LTNULL;
	m_FrameRateSetting.hMenuItemSelected = LTNULL;

	if (m_RendererLine1.hMenuItem) m_pClientDE->DeleteSurface (m_RendererLine1.hMenuItem);
	if (m_RendererLine2.hMenuItem) m_pClientDE->DeleteSurface (m_RendererLine2.hMenuItem);
	if (m_Resolution.hMenuItem) m_pClientDE->DeleteSurface (m_Resolution.hMenuItem);
	if (m_TextureDepth.hMenuItem) m_pClientDE->DeleteSurface (m_TextureDepth.hMenuItem);
	m_RendererLine1.hMenuItem = LTNULL;
	m_RendererLine2.hMenuItem = LTNULL;
	m_Resolution.hMenuItem = LTNULL;
	m_TextureDepth.hMenuItem = LTNULL;

	if (m_hMenuTitle) m_pClientDE->DeleteSurface (m_hMenuTitle);
	m_hMenuTitle = LTNULL;
	
	CBaseMenu::UnloadSurfaces();
}

void CDisplayOptionsMenu::PostCalculateMenuDims()
{
	if (!m_pClientDE) return;

	// get the maximum width of the menu

	int nMenuMaxWidth = 0;
	uint32 nSettingWidth, nSettingHeight;
	m_pClientDE->GetSurfaceDims (m_GoreSetting.hMenuItem, &nSettingWidth, &nSettingHeight);
	if (m_nSecondColumn + (int)nSettingWidth > nMenuMaxWidth) nMenuMaxWidth = m_nSecondColumn + nSettingWidth;
	m_pClientDE->GetSurfaceDims (m_ScreenFlash.hMenuItem, &nSettingWidth, &nSettingHeight);
	if (m_nSecondColumn + (int)nSettingWidth > nMenuMaxWidth) nMenuMaxWidth = m_nSecondColumn + nSettingWidth;
	m_pClientDE->GetSurfaceDims (m_DetailSetting.hMenuItem, &nSettingWidth, &nSettingHeight);
	if (m_nSecondColumn + (int)nSettingWidth > nMenuMaxWidth) nMenuMaxWidth = m_nSecondColumn + nSettingWidth;
	m_pClientDE->GetSurfaceDims (m_FrameRateSetting.hMenuItem, &nSettingWidth, &nSettingHeight);
	if (m_nSecondColumn + (int)nSettingWidth > nMenuMaxWidth) nMenuMaxWidth = m_nSecondColumn + nSettingWidth;
	if (m_nSecondColumn + m_sliderHUDScale.GetWidth() > nMenuMaxWidth) nMenuMaxWidth = m_nSecondColumn + m_sliderHUDScale.GetWidth();

	m_nMenuX = 0;
	//if (m_pRiotMenu->InWorld() || m_szScreen.cx < 512)
	//{
		m_nMenuX = GetMenuAreaLeft() + ((int)m_szMenuArea.cx - nMenuMaxWidth) / 2;
	//}
	//else
	//{
	//	m_nMenuX = GetMenuAreaLeft() + ((int)m_szMenuArea.cx / 2);
	//}

	m_nMenuSpacing = THIS_MENU_SPACING;
}

// Mouse hit testing reaches across both columns here
int CDisplayOptionsMenu::GetItemWidth (int nItem)
{
	int nLabelWidth = CBaseMenu::GetItemWidth (nItem);

	int nValueWidth = 0;
	if (nItem == ID_HUDSCALE)
	{
		nValueWidth = m_sliderHUDScale.GetWidth();
	}
	else
	{
		HSURFACE hValue = LTNULL;
		switch (nItem)
		{
			case ID_GORE:		hValue = m_GoreSetting.hMenuItem;		break;
			case ID_SCREENFLASH:	hValue = m_ScreenFlash.hMenuItem;	break;
			case ID_DETAIL:		hValue = m_DetailSetting.hMenuItem;		break;
			case ID_FRAMERATE:	hValue = m_FrameRateSetting.hMenuItem;	break;
			default: break;
		}

		if (hValue && m_pClientDE)
		{
			uint32 cx = 0, cy = 0;
			m_pClientDE->GetSurfaceDims (hValue, &cx, &cy);
			nValueWidth = (int)cx;
		}
	}

	if (nValueWidth <= 0) return nLabelWidth;

	int nSpan = m_nSecondColumn + nValueWidth;
	return (nSpan > nLabelWidth) ? nSpan : nLabelWidth;
}

void CDisplayOptionsMenu::DrawNoGoreVersion(HSURFACE hScreen, int nScreenWidth, int nScreenHeight, int nTextOffset)
{
	if (!m_pClientDE || !m_pRiotMenu || !hScreen) return;

	// first draw the menu title if there is one

	int nCurrentY = m_nMenuY;
	if (m_hMenuTitle)
	{
		m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, m_hMenuTitle, LTNULL, m_nMenuX, nCurrentY, LTNULL);
		nCurrentY += m_szMenuTitle.cy + m_nMenuTitleSpacing;
	}

	if (m_nTopItem > 0)
	{
		m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, m_pRiotMenu->GetUpArrow(), LTNULL, m_nMenuX + 2, nCurrentY - m_pRiotMenu->GetArrowHeight() - 3, LTNULL);
	}

	LTBOOL bDrawDownArrow = LTFALSE;
	for (int i = m_nTopItem; i < MAX_GENERIC_ITEMS; i++)
	{
		if (m_GenericItem[i].hMenuItem)
		{
			// Don't draw the Gore menu item...
			if (i != ID_GORE)
			{
				m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, i == m_nSelection ? m_GenericItem[i].hMenuItemSelected : m_GenericItem[i].hMenuItem, LTNULL, m_nMenuX, nCurrentY, LTNULL);
			}
			nCurrentY += m_GenericItem[i].szMenuItem.cy + m_nMenuSpacing;
			if (nCurrentY > GetMenuAreaBottom() - (int)m_GenericItem[i].szMenuItem.cy) 
			{
				if (i < m_nGenericItems - 1)
				{
					bDrawDownArrow = LTTRUE;
				}
				break;
			}
		}
	}

	if (bDrawDownArrow)
	{
		m_pClientDE->DrawSurfaceToSurfaceTransparent (hScreen, m_pRiotMenu->GetDownArrow(), LTNULL, m_nMenuX + 2, nCurrentY, LTNULL);
	}
}