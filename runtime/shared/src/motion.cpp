
#include "bdefs.h"
#include "de_objects.h"
#include "de_world.h"
#include "motion.h"
#include "lt1_vars.h"

// Reimplementation of LT1's CalcMotion for a standing object
// It differs from Jupiter's in three ways:
// 1) An object moving away from its plane still gets only the force's normal part on a shallow plane, not the whole force
// 2) The velocity clip and zeroing change only a copy used for this step's movement. The object's own velocity keeps them only when friction applies,
// unless LT1StandVelStore is on, which keeps them either way as LT1 does
// 3) LT1's thresholds decide when no force is applied: almost no speed, too little speed along the plane, or little speed left after the clip
static void LT1StandingMotion(MotionInfo *pInfo, LTObject *pObj, const LTVector &vForce,
	LTVector &dr, LTVector &v, LTVector &a, float dt, float velocityMagSqr, float accelMagSqr)
{
	const LTVector *pPlaneNormal;
	LTVector objectNormal;
	if (pObj->m_pNodeStandingOn)
		pPlaneNormal = &pObj->m_pNodeStandingOn->GetPlane()->m_Normal;
	else
	{
		// Standing on another object, so the plane is taken as opposite to gravity
		objectNormal = -pInfo->m_UnitForce;
		pPlaneNormal = &objectNormal;
	}

	// The acceleration turned into the plane, keeping its size
	if (accelMagSqr > 0.001f)
	{
		LTVector vSide = a.Cross(*pPlaneNormal);
		const float fSideMagSqr = vSide.MagSqr();
		if (fSideMagSqr > 0.1f)
		{
			vSide *= 1.0f / sqrtf(fSideMagSqr);
			a = pPlaneNormal->Cross(vSide) * sqrtf(accelMagSqr);
		}
	}

	LTVector vStepVel = v; // The velocity this step integrates
	LTVector vAppliedForce(0.0f, 0.0f, 0.0f);
	LTBOOL bFriction = LTFALSE;
	if (velocityMagSqr > 0.001f)
	{
		LTVector vAlongPlane(0.0f, 0.0f, 0.0f); // The velocity's direction along the plane
		LTVector vSide = vStepVel.Cross(*pPlaneNormal);
		const float fSideMagSqr = vSide.MagSqr();
		if (fSideMagSqr > 0.001f)
		{
			vSide *= 1.0f / sqrtf(fSideMagSqr);
			vAlongPlane = pPlaneNormal->Cross(vSide);
		}
		float fNormalSpeed = pPlaneNormal->Dot(vStepVel); // Negative when moving into the plane
		LTBOOL bConsiderForce = LTTRUE;
		if (fNormalSpeed < 0.001f)
		{
			bConsiderForce = LTFALSE;
			if (vAlongPlane.MagSqr() >= 0.001f)
			{
				// Speed into the plane beyond 10 is clipped
				if (fNormalSpeed <= -10.0f)
				{
					vStepVel -= *pPlaneNormal * (fNormalSpeed + 10.0f);
					velocityMagSqr = vStepVel.MagSqr();
				}
				const float fPlaneSpeed = vStepVel.Dot(vAlongPlane);
				if ((vAlongPlane * fPlaneSpeed).MagSqr() > 1.0f)
				{
					bFriction = LTTRUE;
					bConsiderForce = LTTRUE;
				}
			}
			if (!bConsiderForce)
				vStepVel.Init();
		}
		// A steep enough plane takes the whole force, a shallow one only its normal part
		if (bConsiderForce && velocityMagSqr >= 0.1f && pInfo->m_ForceMag > 0.001f)
		{
			const float fForceAlongNormal = pPlaneNormal->Dot(vForce);
			if (pInfo->m_SlideRatio * pInfo->m_ForceMag <= fForceAlongNormal)
				vAppliedForce = vForce;
			else
				vAppliedForce = *pPlaneNormal * fForceAlongNormal;
		}
	}
	a += vAppliedForce;

	if (bFriction && pObj->m_FrictionCoefficient > 0.0f)
	{
		const float fFriction = pObj->m_FrictionCoefficient;
		const float fDecay = (float)exp(-fFriction * dt);
		const LTVector vTerminalVel = a / fFriction; // The velocity friction settles at
		dr = vTerminalVel * dt + (vStepVel * fFriction - a) * ((1.0f - fDecay) / fFriction / fFriction);
		pObj->m_Velocity = (vStepVel - vTerminalVel) * fDecay + vTerminalVel;
		v = pObj->m_Velocity;
	}
	else
	{
		dr = vStepVel * dt + a * (dt * dt * 0.5f * 0.5f);
		// LT1 clips and zeroes the velocity in place, so it keeps the clipped one (LT1StandVelStore)
		if (g_bLT1StandVelStore)
			pObj->m_Velocity = vStepVel + a * dt;
		else
			pObj->m_Velocity += a * dt;
		v = pObj->m_Velocity;
	}
}

//*
// ----------------------------------------------------------------------- //
// Returns the vector that the physics would have the object move by.
// ----------------------------------------------------------------------- //
void CalcMotion
(
    MotionInfo* pInfo,
    LTObject*   pObj,           //the object
    LTVector&   dr,             //displacement
    LTVector&   v,              //velocity
    LTVector&   a,              //acceleration
    const bool  bApplyGravity,
    const float dt              //time step
)
{
	LTVector velocityDelta, accelDelta;
	LTVector q, slopeVel, slopeAccel, vel;
	const LTVector *n;
	LTVector objectNormal, vTemp, vTemp2;

	float fExp;
	float timeIntegral;
	float velocityMagSqr, accelMagSqr;
	LTBOOL bFriction;


	bFriction = LTFALSE;
	timeIntegral = dt * dt * 0.5f;	
	velocityMagSqr = v.MagSqr();
	accelMagSqr = a.MagSqr();

	// [KLS - 3/12/02] - Added support for per-object force override...

	LTVector vForce = pObj->GetGlobalForceOverride();
	
	// If the global force override is zero, use the MotionInfo force...

	if (LTVector(0.0, 0.0, 0.0) == vForce)
	{
		vForce = pInfo->m_Force;
	}

	/* 
	// Debug variables
	LTVector vOldAccel, vOldVel, vOldPos;
	vOldAccel = a;
	vOldVel = v;
	vOldPos = pObj->GetPos();
	//*/

	// Stop objects that are moving very slowly...
	if(velocityMagSqr < 0.1f )
	{
		v.Init();
		velocityMagSqr = 0;
	}

	if( accelMagSqr < 0.1f )
	{
		a.Init();
		accelMagSqr = 0;
	}

	// Zero out the displacement to start with.
	dr.Init();

	// Update objects affected by gravity...
	if(bApplyGravity)
	{
		// Add friction to objects standing on something...
		if(pObj->m_pStandingOn)
		{
			// Try to disable their physics.
			if( velocityMagSqr < 0.1f && accelMagSqr < 0.1f )
			{
				pObj->m_Velocity.Init();
				pObj->m_Acceleration.Init();
				pObj->m_InternalFlags &= ~IFLAG_APPLYPHYSICS;
				return;
			}
			else if (g_bLT1CalcMotion)
			{
				LT1StandingMotion(pInfo, pObj, vForce, dr, v, a, dt, velocityMagSqr, accelMagSqr);
				return;
			}
			else
			{
				// Check if object on world geometry...
				if( pObj->m_pNodeStandingOn )
				{
					// Calculate vector parallel to plane...
					n = &pObj->m_pNodeStandingOn->GetPlane()->m_Normal;
				}
				else
				{
					// Object standing on another object, so assume opposite to gravity...
					n = &objectNormal;
					objectNormal = -pInfo->m_UnitForce;
				}

				// Calculate the acceleration including the force
				LTVector vAccelWithForce = a + vForce;

				// If we're on a slope that's at enough of an angle, allow it to slide
				LTVector vForceDir = vForce;
				vForceDir.Norm();
				if (vForceDir.Dot(*n) > pInfo->m_SlideRatio)
				{
					float fAccelMag = vAccelWithForce.Mag();
					a = vAccelWithForce - *n * n->Dot(vAccelWithForce);

					// Don't allow it to accelerate up the slope..
					float fAccelDotForce = a.Dot(vForceDir);
					if (fAccelDotForce < 0.0f)
					{
						a -= vForceDir * fAccelDotForce;
					}
					
					a.Norm(fAccelMag);

					// dsi_ConsolePrint("Steep");
				}				
				else
				{
					// Figure out what our new velocity would be if only the force was used (i.e. are they jumping?)
					LTVector vNewVel = v + vForce * dt;
					// If we're going to be moving away from the plane, use the full force
					if (vNewVel.Dot(*n) > 0.01f)
					{
						a = vAccelWithForce;
						// dsi_ConsolePrint("Jump");
					}
					// If the acceleration without the force isn't moving into the surface, project it there
					else if (a.Dot(*n) > 0.01f)
					{
						float fAccelMag = a.Mag();
						a -= *n * (n->Dot(a) + 1.0f);
						a.Norm(fAccelMag);

						bFriction = LTTRUE;

						// LT1 adds the force here too, see below
						if (g_bLT1StandGravity)
							a += *n * n->Dot(vForce);

						// dsi_ConsolePrint("Downhill");
					}
					else
					{
						bFriction = LTTRUE;

						// LT1 keeps gravity's into-plane part on a standing object, and Jupiter drops it.
						// Without it the move never reaches the floor and the object flickers on and off the ground
						if (g_bLT1StandGravity)
							a += *n * n->Dot(vForce);

						// dsi_ConsolePrint("Walk");
					}
				}
			}
		}
		// Otherwise just apply gravity
		else
		{
			a += vForce;
			// dsi_ConsolePrint("Fall");
		}
	}
	// If there's no gravity and they aren't moving, then disable their physics
	else if( velocityMagSqr < 0.1f && accelMagSqr < 0.1f )
	{
		pObj->m_Velocity.Init();
		pObj->m_Acceleration.Init();
		pObj->m_InternalFlags &= ~IFLAG_APPLYPHYSICS;
		return;
	}

	// If friction
	// new velocity is given by:		v = ( a / k ) + ( v_0 - a / k ) * exp( -k * t )
	// new position is given by:		x = x_0 + ( a / k ) * t + ( k * v_0 - a ) * ( 1 - exp( -k * t )) / k^2
	if( bFriction && pObj->m_FrictionCoefficient > 0.0f )
	{
		// Velocity...
		fExp = ( float )exp( -pObj->m_FrictionCoefficient * dt );
		vTemp = a / pObj->m_FrictionCoefficient;
		vTemp2 = v - vTemp;
		vTemp2 *= fExp;
		vel = vTemp2 + vTemp;

		// Position delta...
		dr = vTemp * dt;
		vTemp = v * pObj->m_FrictionCoefficient;
		vTemp -= a;
		vTemp *= (( 1.0f - fExp ) / pObj->m_FrictionCoefficient / pObj->m_FrictionCoefficient);
		dr += vTemp;
		pObj->m_Velocity = vel;
		v = pObj->m_Velocity;
	}
	// If no friction
	// new velocity is given by:	v = v_0 + a * t
	// new position is given by:	x = x_0 + v_0 * t + .5 * a * t^2
	else
	{
		// Find the change in velocity...
		velocityDelta = a * dt;

		// Position delta...
		dr = v * dt;

		vTemp = a * (timeIntegral * 0.5f);
		dr += vTemp;

		// Add the final velocity to the new velocity.
		pObj->m_Velocity += velocityDelta;
		v = pObj->m_Velocity;
	}

	/*
	// Show debug information, filtering out the server-side player object
	if(bApplyGravity)
	{
		dsi_ConsolePrint("Old - A:<%7.1f,%7.1f,%7.1f> V:<%7.1f,%7.1f,%7.1f> P:<%7.1f,%7.1f,%7.1f>",
			VEC_EXPAND(vOldAccel), VEC_EXPAND(vOldVel), VEC_EXPAND(vOldPos));

		LTVector vNewAccel, vNewVel, vNewPos;
		vNewAccel = a;
		vNewVel = v;
		vNewPos = vOldPos + dr;

		dsi_ConsolePrint("New   - A:<%7.1f,%7.1f,%7.1f> V:<%7.1f,%7.1f,%7.1f> P:<%7.1f,%7.1f,%7.1f>",
			VEC_EXPAND(vNewAccel), VEC_EXPAND(vNewVel), VEC_EXPAND(vNewPos));
	}
	//*/

	return;
}

