#ifndef __OPTIONSMENU_H
#define __OPTIONSMENU_H

#include "BaseMenu.h"
#include "DisplayOptionsMenu.h"
#include "SoundOptionsMenu.h"
#include "KeyboardMenu.h"
#include "MouseMenu.h"
#include "JoystickMenu.h"
#include "GeneralOptionsMenu.h"

class COptionsMenu : public CBaseMenu
{
public:

	virtual LTBOOL		Init (ILTClient* pClientDE, CRiotMenu* pRiotMenu, CBaseMenu* pParent, int nScreenWidth, int nScreenHeight);
	virtual void		ScreenDimsChanged (int nScreenWidth, int nScreenHeight);
	
	virtual LTBOOL		LoadAllSurfaces()		{ LTBOOL bAll = LTTRUE;
											  LoadChildSurfaces (m_DisplayOptionsMenu, "DisplayOptions", bAll);
											  LoadChildSurfaces (m_SoundOptionsMenu,   "SoundOptions",   bAll);
											  LoadChildSurfaces (m_KeyboardMenu,       "Keyboard",       bAll);
											  LoadChildSurfaces (m_MouseMenu,          "Mouse",          bAll);
											  LoadChildSurfaces (m_GeneralOptionsMenu,    "GameOptions",    bAll);
											  if (m_JoystickMenu.JoystickEnabled())
												  LoadChildSurfaces (m_JoystickMenu,   "Joystick",       bAll);
											  LTBOOL bMine = LoadSurfaces();
											  if (!bMine && m_pClientDE) m_pClientDE->CPrint ("[D:MENU] Options failed to load its own surfaces");
											  return (bAll && bMine); }
	virtual void		UnloadAllSurfaces()		{ m_DisplayOptionsMenu.UnloadAllSurfaces(); m_SoundOptionsMenu.UnloadAllSurfaces(); 
												  m_KeyboardMenu.UnloadAllSurfaces(); m_MouseMenu.UnloadAllSurfaces();
												  m_GeneralOptionsMenu.UnloadAllSurfaces();
												  m_JoystickMenu.UnloadAllSurfaces(); UnloadSurfaces(); }
	virtual void		Return();

	virtual void		OnEnterWorld()		{ 
											m_DisplayOptionsMenu.OnEnterWorld();
											m_SoundOptionsMenu.OnEnterWorld();
											m_KeyboardMenu.OnEnterWorld();
											m_MouseMenu.OnEnterWorld();
											m_JoystickMenu.OnEnterWorld();
											m_GeneralOptionsMenu.OnEnterWorld();
											CBaseMenu::OnEnterWorld();
											}

	virtual void		OnExitWorld()		{ 
											m_DisplayOptionsMenu.OnExitWorld();
											m_SoundOptionsMenu.OnExitWorld();
											m_KeyboardMenu.OnExitWorld();
											m_MouseMenu.OnExitWorld();
											m_JoystickMenu.OnExitWorld();
											m_GeneralOptionsMenu.OnExitWorld();
											CBaseMenu::OnExitWorld();
											}
protected:

	virtual LTBOOL		LoadSurfaces();
	virtual void		UnloadSurfaces();

protected:

	CDisplayOptionsMenu	m_DisplayOptionsMenu;
	CSoundOptionsMenu	m_SoundOptionsMenu;
	CKeyboardMenu		m_KeyboardMenu;
	CMouseMenu			m_MouseMenu;
	CJoystickMenu		m_JoystickMenu;
	CGeneralOptionsMenu	m_GeneralOptionsMenu;
};

#endif
