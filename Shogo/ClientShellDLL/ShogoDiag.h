// ----------------------------------------------------------------------- //
//
// MODULE  : ShogoDiag.h
//
// PURPOSE : Runtime diagnostics for Shogo porting
//
// Off by default and turned on with the Diag console variable:
//
//     Diag 1     state changes plus a position sample 4x a second
//     Diag 2     the above plus a position sample every frame
//
// Output goes through CPrint to error.log, prefixed by category:
//
//     [D:POS]     position, velocity and dims sample
//     [D:GROUND]  on ground state changed
//     [D:STAND]   the object being stood on changed
//     [D:DIMS]    the physics box was resized
//     [D:ENV]     one shot dump on entering a world
//     [D:WPN]     one shot dump when the player view weapon is created
//     [D:FIRE]    a shot that disagrees with where the camera looks
//     [D:NODE]    the player view weapon's node axes in camera space
//     [D:SLIDER]  an options slider was rebuilt
//     [D:MENU]    a left/right press and its row
//
// A menu that fails to build its surfaces always prints under [D:MENU].
// An arrow with no [D:SLIDER] after it means the handler is at fault, and a moving position with a static bar means the drawing is.
//
// // ----------------------------------------------------------------------- //

#ifndef __SHOGO_DIAG_H__
#define __SHOGO_DIAG_H__

#include "clientheaders.h"

class CMoveMgr;
class CWeaponModel;

class CShogoDiag
{
	public:

		CShogoDiag();

		void	Init(ILTClient* pClientDE);

		// Called once a frame after movement in order to sample what the frame ended with
		void	Update(CMoveMgr* pMoveMgr, CWeaponModel* pWeaponModel);

		// One shot dumps, which check the console variable themselves
		void	OnEnterWorld(const char* pWorldName);
		void	OnWeaponCreated(CWeaponModel* pWeaponModel);

		/*
			Called from CWeaponModel::SendFireMsg with the direction sent and the camera basis

			Reports the first few shots after entering a world, then only ones that disagree with the camera.
		*/
		void	OnWeaponFired(const LTVector& vFirePos,
		                      const LTVector& vFireDir,
		                      const LTVector& vCameraForward);


		// Where the transmission window was drawn, and whether a cutscene camera was up
		void	OnTransmissionDraw(float fX, float fY, float fTimeLeft, bool bExternalCam);

		void	OnTransmission(uint32 nStringID, const char* pImageName,
		                       HSURFACE hImage, HSURFACE hText);

		LTBOOL	IsOn() const { return m_nLevel > 0; }

	private:

		void	ReadLevel();
		void	SamplePlayer(CMoveMgr* pMoveMgr);
		void	SampleWeapon(CWeaponModel* pWeaponModel);
		void	DumpModelNodes(HLOCALOBJ hObj);
		void	Print(const char* pCategory, const char* pMsg, ...);
		float	CVar(const char* pName, float fDefault);

		ILTClient*	m_pClientDE;

		int			m_nLevel;
		LTFLOAT		m_fNextSampleTime;
		LTFLOAT		m_fNextWeaponTime;
		uint32		m_nFrame;

		LTBOOL		m_bHaveLast;
		LTVector	m_vLastPos;
		LTVector	m_vLastDims;
		HOBJECT		m_hLastStandingOn;
		LTBOOL		m_bLastOnGround;

		LTBOOL		m_bWorldReported;

		uint32		m_nShotsReported;
};

#endif
