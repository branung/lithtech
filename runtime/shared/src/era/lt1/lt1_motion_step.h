// LT1MotionStep, LT1's physics step: CalcMotion, transcribed statement for statement from
// Shogo 25.1's server.dll 0x6b1c0. Its control flow matches Shogo 21.4's and 27.1's.
//
// A template over the object it moves, so an offline test can run the same code the engine does.
// calcmotion.cpp instantiates it on Jupiter's LTObject.
//
// The object class O supplies:
//   typedef Obj
//   static LTVector &Velocity(Obj &), &Acceleration(Obj &)   the object's own
//   static bool Standing(const Obj &)                        m_pStandingOn
//   static const LTVector *NodeNormal(const Obj &)           the standing node's plane normal, or null
//   static float Friction(const Obj &)                       m_FrictionCoefficient
//   static void StopPhysics(Obj &)                           clears IFLAG_APPLYPHYSICS
// The caller's Info points m_pVelocity and m_pAcceleration at the vectors being integrated:
// the object's own, as the client's UpdateMovement passes them, or a container's copies, as the server's does.
//
// What isn't retail's, said where it happens:
//  - the floating point is SSE single where retail's is x87, at 24 bit precision on Windows
//    (d3d.ren resets it each frame) and 53 bit under Wine. The order of operations is the source's;
//  - the normalizations are LT1's VEC_NORM (a multiply by the reciprocal of the magnitude), not LTVector::Norm

#ifndef __ERA_LT1_MOTION_STEP_H__
#define __ERA_LT1_MOTION_STEP_H__

#include <math.h>

template <class O>
class LT1MotionStep
{
public:
	typedef typename O::Obj Obj;

	// 25.1's MotionInfo: the step, the gravity flag from the object's flags,
	// the vectors integrated, the global force, and the displacement out
	struct Info
	{
		float		m_dt;
		bool		m_bGravity;
		LTVector	*m_pVelocity;
		LTVector	*m_pAcceleration;
		LTVector	m_Force;
		LTVector	m_UnitForce;
		float		m_ForceMag;
		float		m_SlideRatio;
		LTVector	m_dr;
	};

	// Returns 0 when the object was put to rest (its physics switched off),
	// 1 when it stepped; m_dr is the displacement either way.
	static int Run(Info &in, Obj &obj)
	{
		LTVector velocityDelta, force, q;
		const LTVector *n;
		LTVector tangent, slopeAccel, vel, objectNormal, vTemp, vTemp2;
		float d, fAccelMag, fClip, fTemp, fExp;
		float velocityMagSqr, accelMagSqr, fForceDot;
		int bFriction = 0;

		const float timeIntegral = in.m_dt * in.m_dt * 0.5f;
		force.Init(0.0f, 0.0f, 0.0f);
		velocityMagSqr = in.m_pVelocity->MagSqr();
		accelMagSqr = in.m_pAcceleration->MagSqr();

		if (velocityMagSqr < 0.1f)
		{
			in.m_pVelocity->Init(0.0f, 0.0f, 0.0f);
			velocityMagSqr = 0.0f;
		}

		if (accelMagSqr < 0.1f)
		{
			in.m_pAcceleration->Init(0.0f, 0.0f, 0.0f);
			accelMagSqr = 0.0f;
		}

		in.m_dr.Init(0.0f, 0.0f, 0.0f);

		if (in.m_bGravity)
		{
			if (O::Standing(obj))
			{
				if (velocityMagSqr < 0.1f && accelMagSqr < 0.1f)
				{
					O::Velocity(obj).Init(0.0f, 0.0f, 0.0f);
					O::Acceleration(obj).Init(0.0f, 0.0f, 0.0f);
					O::StopPhysics(obj);
					return 0;
				}

				// the plane stood on, or for an object standing on an
				// object, against gravity
				if ((n = O::NodeNormal(obj)) == 0)
				{
					objectNormal = in.m_UnitForce * -1.0f;
					n = &objectNormal;
				}

				// slide the acceleration along the plane, keeping its size
				if (accelMagSqr > 0.001f)
				{
					q = n->Cross(*in.m_pAcceleration);
					if (q.MagSqr() > 0.1f)
					{
						Normalize(q);
						slopeAccel = q.Cross(*n);
						fAccelMag = (float)sqrt(accelMagSqr);
						*in.m_pAcceleration = slopeAccel * fAccelMag;
					}
				}

				if (velocityMagSqr > 0.001f)
				{
					// the unit in-plane direction along the velocity
					q = n->Cross(*in.m_pVelocity);
					if (q.MagSqr() > 0.001f)
					{
						Normalize(q);
						tangent = q.Cross(*n);
					}
					else
					{
						tangent.Init(0.0f, 0.0f, 0.0f);
					}

					// into the plane: at most 10 units/s of it is kept, and
					// with no in-plane speed left the step stops
					d = in.m_pVelocity->Dot(*n);
					if (d < 0.001f)
					{
						if (tangent.MagSqr() < 0.001)
						{
							in.m_pVelocity->Init(0.0f, 0.0f, 0.0f);
							velocityMagSqr = 0.0f;
						}
						else
						{
							if (d <= -10.0f)
							{
								fClip = d + 10.0f;
								vTemp = *n * fClip;
								*in.m_pVelocity -= vTemp;
								velocityMagSqr = in.m_pVelocity->MagSqr();
							}

							fTemp = tangent.Dot(*in.m_pVelocity);
							vel = tangent * fTemp;
							fTemp = vel.MagSqr();
							if (fTemp > 1.0f)
							{
								bFriction = 1;
							}
							else
							{
								in.m_pVelocity->Init(0.0f, 0.0f, 0.0f);
								velocityMagSqr = 0.0f;
							}
						}
					}

					// the force: the whole of it on a steep plane, its normal
					// component on a shallow one
					if (velocityMagSqr >= 0.1f && in.m_ForceMag > 0.001f)
					{
						fForceDot = in.m_Force.Dot(*n);
						if (fForceDot < in.m_SlideRatio * in.m_ForceMag)
						{
							vTemp = *n * fForceDot;
							force += vTemp;
						}
						else
						{
							force += in.m_Force;
						}
					}
				}
			}
			else
			{
				force += in.m_Force;
			}
		}
		else if (velocityMagSqr < 0.1f && accelMagSqr < 0.1f)
		{
			O::Velocity(obj).Init(0.0f, 0.0f, 0.0f);
			O::Acceleration(obj).Init(0.0f, 0.0f, 0.0f);
			O::StopPhysics(obj);
			return 0;
		}

		*in.m_pAcceleration += force;

		const float k = O::Friction(obj);
		if (bFriction && k > 0.0f)
		{
			// v = a/k + (v0 - a/k) e^(-kt);  x = (a/k) t + (k v0 - a)(1 - e^(-kt)) / k^2
			fExp = (float)exp(-k * in.m_dt);
			vTemp = *in.m_pAcceleration / k;
			vTemp2 = *in.m_pVelocity - vTemp;
			vTemp2 *= fExp;
			vel = vTemp2 + vTemp;
			in.m_dr = vTemp * in.m_dt;
			vTemp = *in.m_pVelocity * k;
			vTemp -= *in.m_pAcceleration;
			vTemp *= (1.0f - fExp) / k / k;
			in.m_dr += vTemp;
			O::Velocity(obj) = vel;
		}
		else
		{
			// LT1's integration: a quarter of a dt^2, not a half
			velocityDelta = *in.m_pAcceleration * in.m_dt;
			in.m_dr = *in.m_pVelocity * in.m_dt;
			vTemp = *in.m_pAcceleration * (timeIntegral * 0.5f);
			in.m_dr += vTemp;
			O::Velocity(obj) += velocityDelta;
			*in.m_pVelocity = O::Velocity(obj);
		}

		return 1;
	}

private:
	// LT1's VEC_NORM: a multiply by the reciprocal of the magnitude
	static void Normalize(LTVector &v)
	{
		const float fInv = 1.0f / (float)sqrt(v.MagSqr());
		v *= fInv;
	}
};

#endif  // __ERA_LT1_MOTION_STEP_H__
