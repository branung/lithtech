// ----------------------------------------------------------------------- //
//
// MODULE  : GeneralOptionsMenu.h
//
// PURPOSE : The GENERAL options screen, a newly added menu to add QOL
//			 gameplay updates as optional toggles. 	
//
// ----------------------------------------------------------------------- //

#ifndef __GENERALOPTIONSMENU_H
#define __GENERALOPTIONSMENU_H

#include "BaseMenu.h"

class CGeneralOptionsMenu : public CBaseMenu
{
public:

	CGeneralOptionsMenu();

	virtual LTBOOL		Init (ILTClient* pClientDE, CRiotMenu* pRiotMenu, CBaseMenu* pParent, int nScreenWidth, int nScreenHeight);
	virtual void		Reset();

	virtual LTBOOL		LoadAllSurfaces()		{ return LoadSurfaces(); }
	virtual void		UnloadAllSurfaces()		{ UnloadSurfaces(); }

	virtual void		Left();
	virtual void		Right();
	virtual void		Return();

	virtual void		Activate (int nItem);

	virtual int			GetItemWidth (int nItem);

	virtual void		Draw (HSURFACE hScreen, int nScreenWidth, int nScreenHeight, int nTextOffset = 0);

protected:

	virtual LTBOOL		LoadSurfaces();
	virtual void		UnloadSurfaces();

	virtual void		PostCalculateMenuDims();

			LTBOOL		GetSkipCutscenes();
			void		SetSkipCutscenes (LTBOOL bOn);
			void		RebuildSkipCutscenesSurfaces();

protected:

	int					m_nSecondColumn;

	GENERIC_ITEM		m_SkipCutscenes;
};

#endif
