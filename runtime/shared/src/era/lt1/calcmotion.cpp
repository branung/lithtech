// The LT1 set's CalcMotion: LT1's physics step (lt1_motion_step.h) on Jupiter's objects.
// motion.cpp's CalcMotion reaches it through the era set EngineEra 1 selects.
// Off unless LT1MotionStep is set on both sides, in place of Jupiter's step and the stand vars inside it.
// Shogo's var table sets it

#include "bdefs.h"

#include "de_world.h"
#include "de_objects.h"
#include "motion.h"
#include "era/era.h"
#include "era/lt1/lt1_motion_step.h"
#include "lt1_vars.h"

namespace
{
	// Jupiter's object, as LT1MotionStep reads it
	struct JupiterObject
	{
		typedef LTObject Obj;

		static LTVector &Velocity(Obj &o) { return o.m_Velocity; }
		static LTVector &Acceleration(Obj &o) { return o.m_Acceleration; }
		static bool Standing(const Obj &o) { return o.m_pStandingOn != LTNULL; }
		static const LTVector *NodeNormal(const Obj &o)
		{
			return o.m_pNodeStandingOn ? &o.m_pNodeStandingOn->GetPlane()->m_Normal : LTNULL;
		}
		static float Friction(const Obj &o) { return o.m_FrictionCoefficient; }
		static void StopPhysics(Obj &o) { o.m_InternalFlags &= ~IFLAG_APPLYPHYSICS; }
	};
}

// The era set's entry (era/era.h).  LT1 has no per-object force override
// (Jupiter's GetGlobalForceOverride, added 2002), so the global force is
// MotionInfo's, as LT1's was.
bool era_lt1_CalcMotion(MotionInfo *pInfo, LTObject *pObj, LTVector &dr, LTVector &v, LTVector &a,
	bool bApplyGravity, float dt)
{
	if (!g_bLT1MotionStep)
		return false;

	LT1MotionStep<JupiterObject>::Info in;
	in.m_dt = dt;
	in.m_bGravity = bApplyGravity;
	in.m_pVelocity = &v;
	in.m_pAcceleration = &a;
	in.m_Force = pInfo->m_Force;
	in.m_UnitForce = pInfo->m_UnitForce;
	in.m_ForceMag = pInfo->m_ForceMag;
	in.m_SlideRatio = pInfo->m_SlideRatio;
	LT1MotionStep<JupiterObject>::Run(in, *pObj);
	dr = in.m_dr;
	return true;
}
