// ----------------------------------------------------------------------- //
//
// MODULE  : Camera.h
//
// PURPOSE : Camera class definition
//
// CREATED : 5/20/98
//
// ----------------------------------------------------------------------- //

#ifndef __CAMERA_H__
#define __CAMERA_H__

#include "cpp_engineobjects_de.h"

class Camera : public DEBaseClass
{
	public :

		Camera();

		// Cutscene skipping
		DBOOL	IsLive() const;
		// True when the camera turns itself off after m_fActiveTime.
		// Otherwise a trigger message switches it off
		DBOOL	IsSelfTerminating() const	{ return m_fActiveTime > 0.0f; }

		// How long this camera stays on, or 0 if it never stops by itself.
		// A fast forward uses it to tell a cutscene beat from a level timer
		DFLOAT	GetActiveTime() const		{ return m_fActiveTime; }
		void	Expire();

	protected :

		DDWORD	EngineMessageFn(DDWORD messageID, void *pData, DFLOAT fData);
		DDWORD	ObjectMessageFn(HOBJECT hSender, DDWORD messageID, HMESSAGEREAD hRead);
		void	TriggerMsg(HOBJECT hSender, HSTRING hMsg);

	private :

		DBOOL ReadProp(ObjectCreateStruct *pData);
		void  Update();
		void  InitialUpdate(int nInfo);

		void Save(HMESSAGEWRITE hWrite, DDWORD dwSaveFlags);
		void Load(HMESSAGEREAD hRead, DDWORD dwLoadFlags);

		DFLOAT	m_fActiveTime;			// How long camera stays on
		DFLOAT	m_fTurnOffTime;			// Time to turn the camera off
		DBYTE	m_nCameraType;			// Camera type (CT_XXX in ClientServerShared.h)
		DBOOL	m_bAllowPlayerMovement;	// Can the player move when the camera is live
		DBOOL	m_bOneTime;				// Do we activate only one time
		DBOOL	m_bStartActive;			// The camera starts active
		DBOOL	m_bIsListener;			// Listen for sounds at camera position
};

#endif // __CAMERA_H__

