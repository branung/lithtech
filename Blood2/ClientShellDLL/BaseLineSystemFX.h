// ----------------------------------------------------------------------- //
//
// MODULE  : BaseLineSystemFX.h
//
// PURPOSE : BaseParticleSystem special fx class - Definition
//
// CREATED : 1/17/97
//
// ----------------------------------------------------------------------- //

#ifndef __BASE_LINE_SYSTEM_FX_H__
#define __BASE_LINE_SYSTEM_FX_H__

#include "SpecialFX.h"

class CBaseLineSystemFX : public CSpecialFX
{
	public :

		CBaseLineSystemFX() : CSpecialFX() 
		{
			VEC_INIT(m_vPos);
			ROT_INIT(m_rRot);
			m_rRot.m_Quat[3] = 0.0f;   // LT1's m_Spin: a w of 0 is this class's "no rotation set" sentinel, tested in Init()
		}

		virtual DBOOL Init(SFXCREATESTRUCT* psfxCreateStruct);
		virtual DBOOL Update();
		virtual DBOOL CreateObject(CClientDE* pClientDE);

	protected :

		DVector   m_vPos;
		DRotation m_rRot;
};

#endif // __BASE_LINE_SYSTEM_FX_H__