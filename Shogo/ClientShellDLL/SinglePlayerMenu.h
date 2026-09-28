#ifndef __SINGLEPLAYERMENU_H
#define __SINGLEPLAYERMENU_H

#include "BaseMenu.h"
#include "NewGameMenu.h"
#include "LoadLevelMenu.h"
#include "LoadSavedLevelMenu.h"
#include "SaveLevelMenu.h"

class CSinglePlayerMenu : public CBaseMenu
{
public:

	virtual LTBOOL		Init (ILTClient* pClientDE, CRiotMenu* pRiotMenu, CBaseMenu* pParent, int nScreenWidth, int nScreenHeight);
	virtual void		ScreenDimsChanged (int nScreenWidth, int nScreenHeight);
	
	virtual LTBOOL		LoadAllSurfaces()		{ LTBOOL bAll = LTTRUE;
											  LoadChildSurfaces (m_NewGameMenu,        "NewGame",        bAll);
											  LoadChildSurfaces (m_LoadLevelMenu,      "LoadLevel",      bAll);
											  LoadChildSurfaces (m_LoadSavedLevelMenu, "LoadSavedLevel", bAll);
											  LoadChildSurfaces (m_SaveLevelMenu,      "SaveLevel",      bAll);
											  LTBOOL bMine = LoadSurfaces();
											  if (!bMine && m_pClientDE) m_pClientDE->CPrint ("[D:MENU] SinglePlayer failed to load its own surfaces");
											  return (bAll && bMine); }
	virtual void		UnloadAllSurfaces()		{ m_NewGameMenu.UnloadAllSurfaces(); m_LoadLevelMenu.UnloadAllSurfaces();
												  m_LoadSavedLevelMenu.UnloadAllSurfaces(); m_SaveLevelMenu.UnloadAllSurfaces(); UnloadSurfaces(); }

	virtual void		OnEnterWorld()		{ 
											m_NewGameMenu.OnEnterWorld();
											m_LoadLevelMenu.OnEnterWorld();
											m_LoadSavedLevelMenu.OnEnterWorld();
											m_SaveLevelMenu.OnEnterWorld(); 
											CBaseMenu::OnEnterWorld();
											}
	
	virtual void		OnExitWorld()		{ 
											m_NewGameMenu.OnExitWorld();
											m_LoadLevelMenu.OnExitWorld();
											m_LoadSavedLevelMenu.OnExitWorld();
											m_SaveLevelMenu.OnExitWorld(); 
											CBaseMenu::OnExitWorld();
											}
	
	virtual void		Return();

	CLoadSavedLevelMenu*	 GetLoadSavedLevelMenu()	{ return &m_LoadSavedLevelMenu; }

protected:

	virtual LTBOOL		LoadSurfaces();
	virtual void		UnloadSurfaces();

protected:

	CNewGameMenu		m_NewGameMenu;
	CLoadLevelMenu		m_LoadLevelMenu;
	CLoadSavedLevelMenu	m_LoadSavedLevelMenu;
	CSaveLevelMenu		m_SaveLevelMenu;
};

#endif
