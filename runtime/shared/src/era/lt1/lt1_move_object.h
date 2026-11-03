// LT1MoveObject, LT1's object mover: MoveObject, its object pass, the resize and the world model rotation,
// transcribed function for function from Shogo's 25.1 server.dll unit 0x6bf90..0x70f70.
//
// A template over the engine it runs in, so an offline test can run the same code the engine does.
// moveobject.cpp instantiates it on Jupiter's objects and abstraction.
//
// The engine class E supplies:
//   typedef Obj, State                 the object, and Jupiter's MoveState, which the abstraction's calls take
//   static LTVector &Pos/Dims/MinBox/MaxBox/Velocity(Obj *)
//   static uint32 Flags(Obj *); static uint32 &InternalFlags(Obj *)
//   static int Type(Obj *); static float &Radius(Obj *)
//   static bool HasWorldModel(Obj *); static bool HasSData(Obj *)
//   static Obj *StandingOn(Obj *); static bool OnNode(Obj *)
//   void Detach(Obj *)                 off what it stands on, and the node
//   void Stand(Obj *, Obj *on, const void *pNode)
//   int  Riders(Obj *, Obj **, int)    m_ObjectsStandingOn in list order
//   bool IsServer(State *)
//   void Enter(State *)                MoveState::SetupCall, for every move
//   the abstraction: ShouldPush, Crush, Touch (the world walk's, to the collision info's object),
//     TouchObject, PutInContainer, BreakContainerLinks, CanOptimize, CheckMaxPos, SetChangeFlags,
//     MoveAttachments, ClearCollisionPlane; ForceLimitSqr(Obj *)
//   typedef CollisionFrame; PushCollisionInfo(CollisionFrame &), PopCollisionInfo(CollisionFrame &)
//                                      a fresh collision info for the move, stored on the caller's stack
//   int  CollideWithWorld(State *, Obj *pWorld, Obj *pMover, const LTVector &vStart,
//                         LTVector &vEnd, bool bSlide, WorldHit &)
//                                      one world walk, pWorld null for the main world
//   bool BoxInModel(Obj *pModel, const LTVector &vMin, const LTVector &vMax)
//   void Respond(State *, Obj *, Obj *, const LTVector &n, float fDist)
//                                      DoInterObjectCollisionResponse
//   int  Gather(State *, const Move &, Obj **, int)   FindObjectsInMovement
//   int  GatherModel(State *, Obj *pModel, Obj **, int)
//                                      RecurseAndCollideWorldModel's walk, flattened
//   int  CustomList(State *, Obj ***)  the caller's candidates, or 0
//   void Refile(State *, Obj *)        obj_AddNodeObjToBsp
//   void Inherit(State &, State *, Obj *); void SetupBP(State &, State *, Obj *)
//   void RetransformWorldModel(Obj *)
//   and for the rotation: Rotation, SetRotation, SetupWorldModelTransform, Transform, ModelBox
//
// What isn't LT 25.1's, said where it happens:
//  - the floating point is SSE single where 25.1's is x87 at GCC -O0, so this is a logic transcription,
//    not an arithmetic one;
//  - the object tree is Jupiter's WorldTree plus a per move filing in LT1's BSP (the engine's Gather),
//    not LT1's per node lists;
//  - the rider arrays are 32 long and bounds checked. 25.1 writes a 33rd rider one past the end,
//    which isn't reproduced;
//  - m_RadiusSquared, which 25.1's resize sets to |dims|^2 and nothing in this unit reads, has no Jupiter field

#ifndef __ERA_LT1_MOVE_OBJECT_H__
#define __ERA_LT1_MOVE_OBJECT_H__

#include <math.h>

namespace lt1move
{
	const int MAX_CANDIDATES = 100;		// FindObjectsInMovement's
	const int MAX_MODEL_OBJECTS = 512;	// RecurseAndCollideWorldModel's 32 per node, flattened
	const int MAX_RIDERS = 32;

	// MAX_CREAL as GetSmallestPushaway starts from it (9.99999993e36 as a float)
	const float MAX_PUSH = 1e37f;

	// The change flags MoveObject sets, LT1's values and Jupiter's (CF_POSITION,
	// CF_TELEPORT)
	const uint32 CHANGE_POSITION = (1 << 1);
	const uint32 CHANGE_TELEPORT = (1 << 9);

	inline float &Comp(LTVector &v, int i) { return i == 0 ? v.x : (i == 1 ? v.y : v.z); }
	inline float Comp(const LTVector &v, int i) { return i == 0 ? v.x : (i == 1 ? v.y : v.z); }

	// VEC_LERP
	inline void Lerp(LTVector &d, const LTVector &a, const LTVector &b, float t)
	{
		d.x = a.x + (b.x - a.x) * t;
		d.y = a.y + (b.y - a.y) * t;
		d.z = a.z + (b.z - a.z) * t;
	}

	// DVector::Norm: unchanged when zero
	inline void Norm(LTVector &v)
	{
		const float fMag = (float)sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		if (fMag == 0.0f)
			return;
		const float fInv = 1.0f / fMag;
		v.x *= fInv; v.y *= fInv; v.z *= fInv;
	}

	// VEC_NORM: no zero test
	inline void VecNorm(LTVector &v)
	{
		const float fInv = 1.0f / (float)sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		v.x *= fInv; v.y *= fInv; v.z *= fInv;
	}

	inline float Mag(const LTVector &v) { return (float)sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
	inline float MagSqr(const LTVector &v) { return v.x * v.x + v.y * v.y + v.z * v.z; }

	inline float Clamp01(float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }

	// The normals of a box's six planes, in GetSmallestPushaway's plane order
	inline const LTVector &PushPlane(int i)
	{
		static const LTVector s_Planes[6] =
		{
			LTVector(1.0f, 0.0f, 0.0f), LTVector(-1.0f, 0.0f, 0.0f),
			LTVector(0.0f, 1.0f, 0.0f), LTVector(0.0f, -1.0f, 0.0f),
			LTVector(0.0f, 0.0f, 1.0f), LTVector(0.0f, 0.0f, -1.0f)
		};
		return s_Planes[i];
	}

	// What one world walk reports back (CollideInfo's fields this unit reads)
	struct WorldHit
	{
		uint32			m_nHits;
		const void		*m_pStandingOn;	// the node, or null
		LTVector		m_Force;
		LTVector		m_VelOffset;
		void			*m_hObject;		// the collision info's object, for the touch
		void			*m_pWorldObj;	// the object whose BSP was walked, to stand on
	};
}

template <class E>
class LT1MoveObject
{
public:
	typedef typename E::Obj		Obj;
	typedef typename E::State	State;

	// 25.1's MoveState fields past the setup ones: the move as the object
	// pass sees it.  Jupiter's MoveState keeps the world, abstraction, object,
	// priority and candidate list; these are the rest.
	struct Move
	{
		State			*m_pState;
		Obj				*m_pObj;
		const LTVector	*m_pStart;		// +0x1c
		LTVector		m_NewPos;		// +0x20
		LTVector		m_Delta;		// +0x2c
		LTVector		m_Center;		// +0x38
		float			m_Radius;		// +0x44
		LTVector		m_MoveMin;		// +0x48
		LTVector		m_MoveMax;		// +0x54
		const LTMatrix	*m_pTransform;	// +0x60
	};

	explicit LT1MoveObject(E &e) : m_E(e) {}

	// MoveObject (SERVER 0x6dbd8)
	void MoveObject(State *pState, Obj *pMover, LTVector vNewPos, uint32 flags)
	{
		m_E.Enter(pState);
		Move m;
		Init(m, pState, pMover);

		Obj *pStandingOn, *pObj;
		LTVector vStart, vChildPos, vDelta;
		Obj *apRiders[lt1move::MAX_RIDERS];
		LTVector aOffsets[lt1move::MAX_RIDERS];
		int nStanding;

		if (E::InternalFlags(m.m_pObj) & IFLAG_MOVING)
			return;

		if (m_E.CanOptimize(m.m_pObj))
		{
			E::Pos(m.m_pObj) = vNewPos;
			return;
		}

		vDelta = vNewPos - E::Pos(m.m_pObj);
		if (lt1move::MagSqr(vDelta) < 0.0001f)
			return;

		m_E.CheckMaxPos(pState, &vNewPos);
		typename E::CollisionFrame frame;
		m_E.PushCollisionInfo(frame);

		m_E.BreakContainerLinks(m.m_pObj);
		E::InternalFlags(m.m_pObj) |= IFLAG_MOVING | IFLAG_APPLYPHYSICS;

		if (flags & MO_TELEPORT)
			vStart = vNewPos;
		else
			vStart = E::Pos(m.m_pObj);

		if (E::StandingOn(m.m_pObj) && !E::OnNode(m.m_pObj) && !(flags & MO_TELEPORT))
			pStandingOn = E::StandingOn(m.m_pObj);
		else
			pStandingOn = 0;

		nStanding = -1;
		if (flags & MO_MOVESTANDINGONS)
		{
			Obj *apList[256];
			const int nList = m_E.Riders(m.m_pObj, apList, 256);
			for (int i = 0; i < nList; i++)
			{
				pObj = apList[i];
				if (E::Flags(pObj) & FLAG_DONTFOLLOWSTANDING)
				{
					m_E.Detach(pObj);
				}
				else
				{
					// 25.1 tests the bound before the increment and so writes
					// a 33rd rider past its array; this keeps 32
					if (nStanding < lt1move::MAX_RIDERS - 1)
					{
						nStanding++;
						apRiders[nStanding] = pObj;
						aOffsets[nStanding] = E::Pos(pObj) - E::Pos(m.m_pObj);
					}
					else
					{
						m_E.Detach(pObj);
					}
				}
			}
		}
		else
		{
			DetachObjectsStandingOn(m.m_pObj);
		}

		if (IsPhysical(E::Flags(m.m_pObj)) || E::Type(m.m_pObj) == OT_CONTAINER)
		{
			if (!(flags & MO_TELEPORT))
			{
				if (fabs(vDelta.x) < 0.001f)
					vDelta.x = 0.0f;
				if (fabs(vDelta.y) < 0.001f)
					vDelta.y = 0.0f;
				if (fabs(vDelta.z) < 0.001f)
					vDelta.z = 0.0f;
				vNewPos = vStart + vDelta;
			}

			if (flags & MO_DETACHSTANDING)
				m_E.Detach(m.m_pObj);

			E::Pos(m.m_pObj) = vNewPos;
			SetObjectBoundingBox(m.m_pObj, true);
			CollideAgainstObjects(m, vStart, E::Pos(m.m_pObj));
		}
		else
		{
			E::Pos(m.m_pObj) = vNewPos;
		}

		if (!(flags & MO_TELEPORT) && !(flags & MO_GOTHRUWORLD))
		{
			CollideAgainstWorld(m, 0, m.m_pObj, vStart, E::Pos(m.m_pObj),
				!(E::Flags(m.m_pObj) & FLAG_NOSLIDING), true);
		}

		SetObjectBoundingBox(m.m_pObj, true);
		m_E.Refile(pState, m.m_pObj);

		if (flags & MO_MOVESTANDINGONS)
		{
			while (nStanding >= 0)
			{
				pObj = apRiders[nStanding];
				vChildPos = E::Pos(m.m_pObj) + aOffsets[nStanding];

				State child;
				m_E.Inherit(child, pState, pObj);
				MoveObject(&child, pObj, vChildPos,
					MO_DETACHSTANDING | MO_SETCHANGEFLAG | MO_MOVESTANDINGONS | (flags & MO_TELEPORT));

				if (DoBoxesIntersect(E::MinBox(pObj), E::MaxBox(pObj), E::MinBox(m.m_pObj), E::MaxBox(m.m_pObj), -0.01f))
					m_E.Stand(pObj, m.m_pObj, 0);

				nStanding--;
			}
		}

		if (pStandingOn && !E::HasWorldModel(pStandingOn))
		{
			bool bWorldModel;
			if (DoObjectsIntersect(pStandingOn, m.m_pObj, E::MinBox(pStandingOn), E::MaxBox(pStandingOn),
				E::MinBox(m.m_pObj), E::MaxBox(m.m_pObj), -0.01f, bWorldModel))
			{
				m_E.Stand(m.m_pObj, pStandingOn, 0);
			}
		}

		E::InternalFlags(m.m_pObj) &= ~IFLAG_MOVING;

		if (flags & MO_SETCHANGEFLAG)
		{
			m_E.SetChangeFlags(m.m_pObj, lt1move::CHANGE_POSITION);
			if (flags & MO_TELEPORT)
				m_E.SetChangeFlags(m.m_pObj, lt1move::CHANGE_TELEPORT);
		}

		m_E.MoveAttachments(pState);
		m_E.PopCollisionInfo(frame);
	}

	// ChangeObjectDimensions (SERVER 0x6e5f0)
	bool ChangeObjectDimensions(State *pState, Obj *pObj, LTVector &newDims, bool bCollide, bool bTrivialReject)
	{
		LTVector vWanted, vDiff;
		bool bRet;

		if (bTrivialReject)
		{
			if (lt1move::MagSqr(E::Dims(pObj) - newDims) < 0.0001f)
				return true;
		}

		if (bCollide && (E::Flags(pObj) & FLAG_SOLID))
		{
			vWanted = newDims;

			if (E::Dims(pObj).y >= newDims.y)
				E::Dims(pObj).y = newDims.y;
			else
				GrowDim(pState, pObj, 1, newDims.y);

			if (E::Dims(pObj).x >= newDims.x)
				E::Dims(pObj).x = newDims.x;
			else
				GrowDim(pState, pObj, 0, newDims.x);

			if (E::Dims(pObj).z >= newDims.z)
				E::Dims(pObj).z = newDims.z;
			else
				GrowDim(pState, pObj, 2, newDims.z);

			vDiff = vWanted - E::Dims(pObj);
			bRet = lt1move::MagSqr(vDiff) < 0.001f;
			newDims = E::Dims(pObj);
		}
		else
		{
			E::Dims(pObj) = newDims;
			bRet = true;
		}

		E::Radius(pObj) = lt1move::Mag(E::Dims(pObj)) + 0.5f;
		m_E.Detach(pObj);
		SetObjectBoundingBox(pObj, true);
		m_E.Refile(pState, pObj);
		return bRet;
	}

	// RotateWorldModel (SERVER 0x6e8fc).  The rotation's matrix is the
	// engine's to build and store (the caller's E::Rotate); this is what
	// follows it.
	void RotateWorldModel(State *pState, Obj *pModel, const LTRotation &rRotation)
	{
		LTMatrix fullTransform, temp, backTrans, backRot, forwardRot, forwardTrans;
		LTVector newPos;
		const LTVector &vPos = E::Pos(pModel);

		backTrans.Init(
			1.0f, 0.0f, 0.0f, -vPos.x,
			0.0f, 1.0f, 0.0f, -vPos.y,
			0.0f, 0.0f, 1.0f, -vPos.z,
			0.0f, 0.0f, 0.0f, 1.0f);

		forwardTrans.Init(
			1.0f, 0.0f, 0.0f, vPos.x,
			0.0f, 1.0f, 0.0f, vPos.y,
			0.0f, 0.0f, 1.0f, vPos.z,
			0.0f, 0.0f, 0.0f, 1.0f);

		E::Rotation(pModel).ConvertToMatrix(backRot);
		MatTranspose3x3(&backRot);
		rRotation.ConvertToMatrix(forwardRot);

		MatMul(&fullTransform, &forwardTrans, &forwardRot);
		MatMul(&temp, &fullTransform, &backRot);
		MatMul(&fullTransform, &temp, &backTrans);

		E::Rotation(pModel) = rRotation;
		m_E.SetupWorldModelTransform(pModel);
		SetupWorldModelDims(pModel, m_E.Transform(pModel));
		SetObjectBoundingBox(pModel, true);
		m_E.Refile(pState, pModel);

		if (E::Flags(pModel) & FLAG_BOXPHYSICS)
			return;

		typename E::CollisionFrame frame;
		m_E.PushCollisionInfo(frame);

		Obj *apRiders[lt1move::MAX_RIDERS];
		LTVector aStart[lt1move::MAX_RIDERS];
		int nStanding = -1;
		{
			Obj *apList[256];
			const int nList = m_E.Riders(pModel, apList, 256);
			for (int i = 0; i < nList; i++)
			{
				Obj *pOn = apList[i];
				if (E::Flags(pOn) & FLAG_DONTFOLLOWSTANDING)
				{
					m_E.Detach(pOn);
				}
				else if (nStanding < lt1move::MAX_RIDERS - 1)
				{
					nStanding++;
					apRiders[nStanding] = pOn;
					aStart[nStanding] = E::Pos(pOn);
				}
				else
				{
					m_E.Detach(pOn);
				}
			}
		}

		Move m;
		Init(m, pState, pModel);
		m.m_pTransform = &fullTransform;

		Obj **ppCustom = 0;
		const int nCustom = m_E.CustomList(pState, &ppCustom);
		if (ppCustom)
		{
			for (int i = 0; i < nCustom; i++)
				MaybeCollideWorldModel(m, ppCustom[i]);
		}
		else
		{
			// RecurseAndCollideWorldModel (SERVER 0x6d3cc): the tree's
			// objects near the model, node by node, each node's gathered
			// before any is pushed; the engine flattens the walk
			Obj *apObjs[lt1move::MAX_MODEL_OBJECTS];
			const int nObjs = m_E.GatherModel(pState, pModel, apObjs, lt1move::MAX_MODEL_OBJECTS);
			for (int i = 0; i < nObjs; i++)
				MaybeCollideWorldModel(m, apObjs[i]);
		}

		while (nStanding >= 0)
		{
			Obj *pOn = apRiders[nStanding];
			MatVMul_H(&newPos, &fullTransform, &aStart[nStanding]);
			State child;
			m_E.SetupBP(child, pState, pOn);
			newPos.y -= 0.5f;
			MoveObject(&child, pOn, newPos, MO_DETACHSTANDING | MO_SETCHANGEFLAG | MO_MOVESTANDINGONS);
			nStanding--;
		}

		m_E.PopCollisionInfo(frame);
	}

	// SetupWorldModelDims (SERVER 0x6d5b4)
	void SetupWorldModelDims(Obj *pModel, const LTMatrix &mTransform)
	{
		LTVector pts[8], minPt, maxPt, vBoxMin, vBoxMax;
		const LTVector vPos = E::Pos(pModel);
		LTVector &vDims = E::Dims(pModel);

		m_E.ModelBox(pModel, vBoxMin, vBoxMax);
		vDims = (vBoxMax - vBoxMin) * 0.5f;

		pts[0].Init(vPos.x + vDims.x, vPos.y + vDims.y, vPos.z + vDims.z);
		pts[1].Init(vPos.x + vDims.x, vPos.y - vDims.y, vPos.z + vDims.z);
		pts[2].Init(vPos.x + vDims.x, vPos.y + vDims.y, vPos.z - vDims.z);
		pts[3].Init(vPos.x + vDims.x, vPos.y - vDims.y, vPos.z - vDims.z);

		pts[4].Init(vPos.x - vDims.x, vPos.y + vDims.y, vPos.z + vDims.z);
		pts[5].Init(vPos.x - vDims.x, vPos.y - vDims.y, vPos.z + vDims.z);
		pts[6].Init(vPos.x - vDims.x, vPos.y + vDims.y, vPos.z - vDims.z);
		pts[7].Init(vPos.x - vDims.x, vPos.y - vDims.y, vPos.z - vDims.z);

		minPt.Init(lt1move::MAX_PUSH, lt1move::MAX_PUSH, lt1move::MAX_PUSH);
		maxPt.Init(-lt1move::MAX_PUSH, -lt1move::MAX_PUSH, -lt1move::MAX_PUSH);

		for (int i = 0; i < 8; i++)
		{
			MatVMul_InPlace_H(&mTransform, &pts[i]);
			VEC_MIN(minPt, minPt, pts[i]);
			VEC_MAX(maxPt, maxPt, pts[i]);
		}

		vDims = (maxPt - minPt) * 0.5f;
		E::Radius(pModel) = lt1move::Mag(vDims) + 0.1f;
	}

	// The 25.1 IsPhysical: solid or touch-notify, nothing else
	static bool IsPhysical(uint32 flags)
	{
		return (flags & (FLAG_SOLID | FLAG_TOUCH_NOTIFY)) != 0;
	}

	// DoBoxesIntersect (SERVER 0x6c160).  A negative tolerance grows the boxes.
	static bool DoBoxesIntersect(const LTVector &min1, const LTVector &max1,
		const LTVector &min2, const LTVector &max2, float fTolerance)
	{
		if (min1.x - max2.x >= -fTolerance || max1.x - min2.x <= fTolerance ||
			min1.y - max2.y >= -fTolerance || max1.y - min2.y <= fTolerance ||
			min1.z - max2.z >= -fTolerance || max1.z - min2.z <= fTolerance)
		{
			return false;
		}
		return true;
	}

	// GetSmallestPushaway (SERVER 0x6c22c)
	static void GetSmallestPushaway(const LTVector &moverMin, const LTVector &moverMax,
		const LTVector &blockerMin, const LTVector &blockerMax, LTVector &pushAmount, int &pushPlane)
	{
		float minPush = lt1move::MAX_PUSH, testPush;
		int minPushDim = 0, curPushPlane = 0, minPushPlane = 0;

		for (int i = 0; i < 3; i++)
		{
			testPush = lt1move::Comp(blockerMax, i) - lt1move::Comp(moverMin, i);
			if (fabs(testPush) < fabs(minPush))
			{
				minPush = testPush;
				minPushDim = i;
				minPushPlane = curPushPlane;
			}
			++curPushPlane;

			testPush = lt1move::Comp(blockerMin, i) - lt1move::Comp(moverMax, i);
			if (fabs(testPush) < fabs(minPush))
			{
				minPush = testPush;
				minPushDim = i;
				minPushPlane = curPushPlane;
			}
			++curPushPlane;
		}

		pushAmount.Init(0.0f, 0.0f, 0.0f);
		lt1move::Comp(pushAmount, minPushDim) = minPush;
		pushPlane = minPushPlane;
	}

	// GetMovementBox (SERVER 0x6f664): the destination box stretched back
	// along the delta
	static void GetMovementBox(LTVector &vMoveMin, LTVector &vMoveMax, const LTVector &vDelta,
		const LTVector &vMinBox, const LTVector &vMaxBox)
	{
		vMoveMin = vMinBox;
		vMoveMax = vMaxBox;

		if (vDelta.x > 0.0f)
			vMoveMin.x -= vDelta.x;
		else if (vDelta.x < 0.0f)
			vMoveMax.x -= vDelta.x;

		if (vDelta.y > 0.0f)
			vMoveMin.y -= vDelta.y;
		else if (vDelta.y < 0.0f)
			vMoveMax.y -= vDelta.y;

		if (vDelta.z > 0.0f)
			vMoveMin.z -= vDelta.z;
		else if (vDelta.z < 0.0f)
			vMoveMax.z -= vDelta.z;
	}

private:
	E &m_E;

	void Init(Move &m, State *pState, Obj *pObj)
	{
		m.m_pState = pState;
		m.m_pObj = pObj;
		m.m_pStart = 0;
		m.m_NewPos.Init(0.0f, 0.0f, 0.0f);
		m.m_Delta.Init(0.0f, 0.0f, 0.0f);
		m.m_Center.Init(0.0f, 0.0f, 0.0f);
		m.m_Radius = 0.0f;
		m.m_MoveMin.Init(0.0f, 0.0f, 0.0f);
		m.m_MoveMax.Init(0.0f, 0.0f, 0.0f);
		m.m_pTransform = 0;
	}

	// DetachObjectsStandingOn (SERVER 0x6bfd4)
	void DetachObjectsStandingOn(Obj *pObj)
	{
		Obj *apList[256];
		const int nList = m_E.Riders(pObj, apList, 256);
		for (int i = 0; i < nList; i++)
		{
			m_E.Detach(apList[i]);
			E::InternalFlags(apList[i]) |= IFLAG_APPLYPHYSICS;
		}
	}

	// SetObjectBoundingBox (SERVER 0x6f0dc)
	void SetObjectBoundingBox(Obj *pObj, bool bTransformWorldModel)
	{
		E::MinBox(pObj) = E::Pos(pObj) - E::Dims(pObj);
		E::MaxBox(pObj) = E::Pos(pObj) + E::Dims(pObj);

		if (bTransformWorldModel && E::HasWorldModel(pObj))
			m_E.RetransformWorldModel(pObj);
	}

	// CollideAgainstWorld (SERVER 0x6f180): one walk, the end position taken
	// back, and when bPush the standing node, the box, the touch and the
	// velocity offset.  pWorld is the world model walked, or null for the
	// main world.
	bool CollideAgainstWorld(Move &m, Obj *pWorld, Obj *pObj, const LTVector &vStart, LTVector &vEnd,
		bool bSlide, bool bPush)
	{
		if (E::Flags(pObj) & FLAG_GOTHRUWORLD)
			return false;

		lt1move::WorldHit info;
		m_E.CollideWithWorld(m.m_pState, pWorld, pObj, vStart, vEnd, bSlide, info);

		if (bPush)
		{
			if (info.m_pStandingOn)
				m_E.Stand(pObj, (Obj *)info.m_pWorldObj, info.m_pStandingOn);

			if (info.m_nHits)
			{
				SetObjectBoundingBox(pObj, true);
				const float fForce = lt1move::MagSqr(info.m_Force);
				if (fForce >= m_E.ForceLimitSqr(pObj))
					m_E.Touch(pObj, info.m_hObject, info.m_VelOffset, (float)sqrt(fForce));

				E::Velocity(pObj) += info.m_VelOffset;
			}
		}

		return info.m_nHits != 0;
	}

	// DoObjectsIntersect (SERVER 0x6f420): the boxes, then, when either is a
	// world model that does not collide as a box, the other's box against
	// that model's BSP
	bool DoObjectsIntersect(Obj *pObj1, Obj *pObj2, const LTVector &min1, const LTVector &max1,
		const LTVector &min2, const LTVector &max2, float fTolerance, bool &bWorldModel)
	{
		if (!DoBoxesIntersect(min1, max1, min2, max2, fTolerance))
			return false;

		Obj *pModel = 0;
		const LTVector *pOtherMin = 0, *pOtherMax = 0;
		if (E::HasWorldModel(pObj1) && !(E::Flags(pObj1) & FLAG_BOXPHYSICS))
		{
			pModel = pObj1;
			pOtherMin = &min2;
			pOtherMax = &max2;
		}
		else if (E::HasWorldModel(pObj2) && !(E::Flags(pObj2) & FLAG_BOXPHYSICS))
		{
			pModel = pObj2;
			pOtherMin = &min1;
			pOtherMax = &max1;
		}

		if (pModel)
		{
			bWorldModel = true;
			return m_E.BoxInModel(pModel, *pOtherMin, *pOtherMax);
		}

		bWorldModel = false;
		return true;
	}

	// DoNonsolidCollision (SERVER 0x6f54c): the collision info cleared, each
	// told of the other if it asked, and container links by object TYPE
	void DoNonsolidCollision(Obj *pObj1, Obj *pObj2)
	{
		m_E.ClearCollisionPlane();
		LTVector zeroVec(0.0f, 0.0f, 0.0f);

		if (E::Flags(pObj1) & FLAG_TOUCH_NOTIFY)
			m_E.TouchObject(pObj1, pObj2, zeroVec, 0.0f);

		if (E::Flags(pObj2) & FLAG_TOUCH_NOTIFY)
			m_E.TouchObject(pObj2, pObj1, zeroVec, 0.0f);

		if (E::Type(pObj1) == OT_CONTAINER)
			m_E.PutInContainer(pObj2, pObj1);

		if (E::Type(pObj2) == OT_CONTAINER)
			m_E.PutInContainer(pObj1, pObj2);
	}

	static bool IsWorldModel(Obj *pObj1, Obj *pObj2)
	{
		if (E::HasWorldModel(pObj1) && !(E::Flags(pObj1) & FLAG_BOXPHYSICS))
			return true;
		if (E::HasWorldModel(pObj2) && !(E::Flags(pObj2) & FLAG_BOXPHYSICS))
			return true;
		return false;
	}

	// The crush test both solid responses make after pushing a blocker: the
	// mover against the blocker's box shrunk by 0.1, with a 0.1 tolerance
	bool StillInside(Obj *pMover, Obj *pTestObj)
	{
		LTVector minBox = E::MinBox(pTestObj), maxBox = E::MaxBox(pTestObj);
		minBox.x += 0.1f; minBox.y += 0.1f; minBox.z += 0.1f;
		maxBox.x -= 0.1f; maxBox.y -= 0.1f; maxBox.z -= 0.1f;
		bool bWorldModel;
		return DoObjectsIntersect(pMover, pTestObj, E::MinBox(pMover), E::MaxBox(pMover),
			minBox, maxBox, 0.1f, bWorldModel);
	}

	// DoSolidWMCollision (SERVER 0x6f8f0)
	bool DoSolidWMCollision(Move &m, Obj *pTestObj, const LTVector &startPos, LTVector &destPos,
		bool bPush, bool &bCollision)
	{
		LTVector pos1, pos2, vecTo, vDir;

		bCollision = false;

		if (E::HasWorldModel(m.m_pObj))
		{
			pos2 = E::Pos(pTestObj);

			vecTo = destPos - startPos;
			pos1 = pos2 + vecTo;

			vDir = vecTo;
			lt1move::Norm(vDir);
			pos1 += vDir * 0.5f;

			if (CollideAgainstWorld(m, m.m_pObj, pTestObj, pos1, pos2,
				!(E::Flags(m.m_pObj) & FLAG_NOSLIDING), bPush))
			{
				bCollision = true;

				if (m_E.ShouldPush(m.m_pState, m.m_pObj, pTestObj))
				{
					State child;
					m_E.Inherit(child, m.m_pState, pTestObj);
					MoveObject(&child, pTestObj, pos2, MO_DETACHSTANDING | MO_SETCHANGEFLAG | MO_MOVESTANDINGONS);

					if (StillInside(m.m_pObj, pTestObj))
						m_E.Crush(pTestObj, m.m_pObj);
					else
						return false;
				}

				vecTo = E::Pos(pTestObj) - pos2;
				destPos += vecTo;
				return true;
			}
		}
		else
		{
			pos2 = destPos;

			if (CollideAgainstWorld(m, pTestObj, m.m_pObj, startPos, pos2,
				!(E::Flags(m.m_pObj) & FLAG_NOSLIDING), bPush))
			{
				bCollision = true;

				if (m_E.ShouldPush(m.m_pState, m.m_pObj, pTestObj))
				{
					vecTo = destPos - pos2;
					pos1 = E::Pos(pTestObj) + vecTo;

					State child;
					m_E.Inherit(child, m.m_pState, pTestObj);
					MoveObject(&child, pTestObj, pos1, MO_DETACHSTANDING | MO_SETCHANGEFLAG | MO_MOVESTANDINGONS);

					if (StillInside(m.m_pObj, pTestObj))
					{
						m_E.Crush(pTestObj, m.m_pObj);

						vecTo = E::Pos(pTestObj) - pos1;
						destPos += vecTo;
						return true;
					}
				}
				else
				{
					destPos = pos2;
					return true;
				}

				return false;
			}
		}

		return false;
	}

	// The vertical half of both solid responses: the higher of the two stands
	// on the lower when the push was vertical and the move went that way
	void StandAfterPush(Move &m, Obj *pTestObj, const LTVector &startPos, const LTVector &destPos, int pushPlane)
	{
		if (pushPlane == 2 || pushPlane == 3)
		{
			if (E::Pos(m.m_pObj).y > E::Pos(pTestObj).y)
			{
				if (destPos.y < startPos.y)
					m_E.Stand(m.m_pObj, pTestObj, 0);
			}
			else
			{
				if (destPos.y > startPos.y)
					m_E.Stand(pTestObj, m.m_pObj, 0);
			}
		}
	}

	// DoSolidBBoxCollision (SERVER 0x70124)
	bool DoSolidBBoxCollision(Move &m, Obj *pTestObj, const LTVector &startPos, LTVector &destPos)
	{
		LTVector pushAmount, newPos, vDeltaPos, vNormal;
		int pushPlane;
		float fPlaneDist, fDot1, fDot2;

		vDeltaPos.Init(0.0f, 0.0f, 0.0f);
		if ((E::Flags(pTestObj) & FLAG_NOSLIDING) || (E::Flags(m.m_pObj) & FLAG_NOSLIDING))
			vDeltaPos = destPos - startPos;

		if (m_E.ShouldPush(m.m_pState, m.m_pObj, pTestObj))
		{
			GetSmallestPushaway(E::MinBox(pTestObj), E::MaxBox(pTestObj),
				E::MinBox(m.m_pObj), E::MaxBox(m.m_pObj), pushAmount, pushPlane);

			if (pushPlane & 1)
				fPlaneDist = -lt1move::Comp(E::MinBox(m.m_pObj), (pushPlane - 1) >> 1);
			else
				fPlaneDist = lt1move::Comp(E::MaxBox(pTestObj), pushPlane >> 1);

			StandAfterPush(m, pTestObj, startPos, destPos, pushPlane);

			if (!(E::Flags(pTestObj) & FLAG_NOSLIDING))
			{
				newPos = E::Pos(pTestObj) + pushAmount;
			}
			else
			{
				fDot2 = -lt1move::Mag(pushAmount);
				vNormal = pushAmount / -fDot2;
				fDot1 = fDot2 + vNormal.Dot(vDeltaPos);
				newPos = vDeltaPos * (1.0f - (-fDot1 / (fDot2 - fDot1)));
				newPos += E::Pos(pTestObj);
			}

			State child;
			m_E.Inherit(child, m.m_pState, pTestObj);
			MoveObject(&child, pTestObj, newPos, MO_DETACHSTANDING | MO_SETCHANGEFLAG | MO_MOVESTANDINGONS);

			m_E.Respond(m.m_pState, pTestObj, m.m_pObj, lt1move::PushPlane(pushPlane), fPlaneDist);

			if (DoBoxesIntersect(E::MinBox(m.m_pObj), E::MaxBox(m.m_pObj), E::MinBox(pTestObj), E::MaxBox(pTestObj), 0.1f))
				m_E.Crush(pTestObj, m.m_pObj);
			else
				return false;
		}

		GetSmallestPushaway(E::MinBox(m.m_pObj), E::MaxBox(m.m_pObj),
			E::MinBox(pTestObj), E::MaxBox(pTestObj), pushAmount, pushPlane);

		if (pushPlane & 1)
			fPlaneDist = -lt1move::Comp(E::MinBox(pTestObj), (pushPlane - 1) >> 1);
		else
			fPlaneDist = lt1move::Comp(E::MaxBox(pTestObj), pushPlane >> 1);

		StandAfterPush(m, pTestObj, startPos, destPos, pushPlane);

		if (!(E::Flags(m.m_pObj) & FLAG_NOSLIDING))
		{
			destPos += pushAmount;
		}
		else
		{
			fDot2 = -lt1move::Mag(pushAmount);
			vNormal = pushAmount / -fDot2;
			fDot1 = fDot2 - vNormal.Dot(vDeltaPos);
			destPos = vDeltaPos * (-fDot1 / (fDot2 - fDot1));
			destPos += startPos;
		}

		m_E.Respond(m.m_pState, m.m_pObj, pTestObj, lt1move::PushPlane(pushPlane), fPlaneDist);
		return true;
	}

	// CheckIntersectOnMovement (SERVER 0x707c4): the swept test, for a move
	// longer than the mover's dims on some axis
	bool CheckIntersectOnMovement(Move &m, Obj *pTestObj)
	{
		float t1, t2, tClosest, tFarthest;
		int nIterations = 0;
		bool bCollide, bMoved, bStopped, bClosestIsT1;
		LTVector vTempDestPos;

		const bool bPushAway = (E::Flags(pTestObj) & FLAG_SOLID) && (E::Flags(m.m_pObj) & FLAG_SOLID);
		const bool bWorldModel = IsWorldModel(m.m_pObj, pTestObj);

		do
		{
			bMoved = bCollide = bStopped = false;

			for (int i = 0; i < 3 && !bCollide; i++)
			{
				t1 = t2 = 0.0f;
				const float fDelta = lt1move::Comp(m.m_Delta, i);

				if (fDelta < 0.0f || fDelta > 0.0f)
				{
					t1 = (lt1move::Comp(E::MinBox(pTestObj), i) - lt1move::Comp(*m.m_pStart, i)) / fDelta;
					t1 = lt1move::Clamp01(t1);
					t2 = (lt1move::Comp(E::MaxBox(pTestObj), i) - lt1move::Comp(*m.m_pStart, i)) / fDelta;
					t2 = lt1move::Clamp01(t2);
				}

				if (t1 < t2)
				{
					bClosestIsT1 = true;
					tClosest = t1;
					tFarthest = t2;
				}
				else
				{
					bClosestIsT1 = false;
					tClosest = t2;
					tFarthest = t1;
				}

				if (0.0f < tClosest || bWorldModel)
				{
					if (!bPushAway || (bClosestIsT1 && fDelta > 0.0f) || (!bClosestIsT1 && fDelta < 0.0f))
					{
						bMoved = true;
						lt1move::Lerp(E::Pos(m.m_pObj), *m.m_pStart, m.m_NewPos, tClosest);
						SetObjectBoundingBox(m.m_pObj, true);

						if (DoBoxesIntersect(E::MinBox(m.m_pObj), E::MaxBox(m.m_pObj),
							E::MinBox(pTestObj), E::MaxBox(pTestObj), 0.01f))
						{
							bCollide = true;
						}
					}
				}

				if (!bCollide && (0.0f < tFarthest || bWorldModel))
				{
					if (!bPushAway || (bClosestIsT1 && fDelta < 0.0f) || (!bClosestIsT1 && fDelta > 0.0f))
					{
						bMoved = true;
						lt1move::Lerp(E::Pos(m.m_pObj), *m.m_pStart, m.m_NewPos, tFarthest);
						SetObjectBoundingBox(m.m_pObj, true);

						if (DoBoxesIntersect(E::MinBox(m.m_pObj), E::MaxBox(m.m_pObj),
							E::MinBox(pTestObj), E::MaxBox(pTestObj), 0.01f))
						{
							bCollide = true;
						}
					}
				}

				if (bMoved && !bCollide)
				{
					bMoved = false;
					E::Pos(m.m_pObj) = *m.m_pStart + m.m_Delta;
					SetObjectBoundingBox(m.m_pObj, true);
				}
			}

			if (bCollide)
			{
				if (bPushAway)
				{
					if (bWorldModel)
					{
						E::Pos(m.m_pObj) = m.m_NewPos;
						SetObjectBoundingBox(m.m_pObj, true);

						bStopped = DoSolidWMCollision(m, pTestObj, *m.m_pStart, m.m_NewPos, true, bCollide);
						if (bStopped)
						{
							E::Pos(m.m_pObj) = m.m_NewPos;
							SetObjectBoundingBox(m.m_pObj, true);
						}
					}
					else
					{
						bStopped = DoSolidBBoxCollision(m, pTestObj, *m.m_pStart, E::Pos(m.m_pObj));
					}
				}
				else
				{
					bStopped = true;
					if (bWorldModel)
					{
						vTempDestPos = m.m_NewPos;

						if (E::HasWorldModel(pTestObj) && !(E::Flags(pTestObj) & FLAG_BOXPHYSICS))
						{
							bStopped = CollideAgainstWorld(m, pTestObj, m.m_pObj, *m.m_pStart, vTempDestPos, false, false);
						}
						else if (E::HasWorldModel(m.m_pObj) && !(E::Flags(m.m_pObj) & FLAG_BOXPHYSICS))
						{
							bStopped = CollideAgainstWorld(m, m.m_pObj, pTestObj, *m.m_pStart, vTempDestPos, false, false);
						}
					}

					if (bStopped)
					{
						DoNonsolidCollision(pTestObj, m.m_pObj);
						bStopped = false;
					}
				}
			}

			if (bMoved && !bStopped)
			{
				E::Pos(m.m_pObj) = *m.m_pStart + m.m_Delta;
				SetObjectBoundingBox(m.m_pObj, true);
			}
		}
		while (bPushAway && bCollide && !bStopped && nIterations++ < 10);

		return bStopped;
	}

	// MaybeCollide (SERVER 0x6f738)
	bool MaybeCollide(Move &m, Obj *pObj)
	{
		bool bWorldModel, bOut, ret;

		if (E::InternalFlags(pObj) & IFLAG_MOVING)
			return false;

		if (E::Dims(m.m_pObj).x < fabs(m.m_Delta.x) ||
			E::Dims(m.m_pObj).y < fabs(m.m_Delta.y) ||
			E::Dims(m.m_pObj).z < fabs(m.m_Delta.z))
		{
			return CheckIntersectOnMovement(m, pObj);
		}

		if (DoObjectsIntersect(m.m_pObj, pObj, E::MinBox(m.m_pObj), E::MaxBox(m.m_pObj),
			E::MinBox(pObj), E::MaxBox(pObj), 0.0f, bWorldModel))
		{
			const bool bSolid = (E::Flags(pObj) & FLAG_SOLID) && (E::Flags(m.m_pObj) & FLAG_SOLID);
			if (bSolid)
			{
				if (bWorldModel)
					ret = DoSolidWMCollision(m, pObj, *m.m_pStart, m.m_NewPos, true, bOut);
				else
					ret = DoSolidBBoxCollision(m, pObj, *m.m_pStart, m.m_NewPos);

				if (ret)
					E::Pos(m.m_pObj) = m.m_NewPos;
				return ret;
			}

			DoNonsolidCollision(pObj, m.m_pObj);
		}

		return false;
	}

	// CollideAgainstObjects (SERVER 0x6c620): the swept state, the
	// candidates, and MaybeCollide on each, last first.  A hit re-files the
	// box, restarts the list and re-derives the move; at most ten restarts,
	// and a hit on either of the last two objects hit ends the pass.
	void CollideAgainstObjects(Move &m, const LTVector &P0, const LTVector &P1)
	{
		Obj *pLastHits[2];
		Obj *objectList[lt1move::MAX_CANDIDATES];
		Obj **ppList;
		int nObjects;

		m.m_pStart = &P0;
		m.m_NewPos = P1;
		m.m_Delta = P1 - P0;
		m.m_Center = P0 + m.m_Delta * 0.5f;
		m.m_Radius = lt1move::Mag(m.m_Delta) * 0.5f + lt1move::Mag(E::Dims(m.m_pObj));
		GetMovementBox(m.m_MoveMin, m.m_MoveMax, m.m_Delta, E::MinBox(m.m_pObj), E::MaxBox(m.m_pObj));

		Obj **ppCustom = 0;
		const int nCustom = m_E.CustomList(m.m_pState, &ppCustom);
		if (ppCustom)
		{
			ppList = ppCustom;
			nObjects = nCustom;
		}
		else
		{
			ppList = objectList;
			nObjects = m_E.Gather(m.m_pState, m, ppList, lt1move::MAX_CANDIDATES);
		}

		pLastHits[0] = pLastHits[1] = 0;
		int nRestarts = 0;
		for (int i = nObjects - 1; i >= 0 && nRestarts < 10; i--)
		{
			Obj *pObj = ppList[i];
			if (MaybeCollide(m, pObj))
			{
				if (pObj == pLastHits[0] || pObj == pLastHits[1])
					break;

				pLastHits[nRestarts & 1] = pObj;
				SetObjectBoundingBox(m.m_pObj, true);
				i = nObjects;
				nRestarts++;
				m.m_NewPos = E::Pos(m.m_pObj);
				m.m_Delta = m.m_NewPos - P0;
			}
		}
	}

	// GrowDim (SERVER 0x6c9dc): the object probed down by the growth and up
	// by twice it, each a real MoveObject, to find the room; then moved to
	// the middle and grown by half the room found
	void GrowDim(State *pState, Obj *pObj, int nDim, float newDim)
	{
		LTVector vNewPos, vPos;
		float fHigh, fLow, fHalf, fOldPos;
		const float fDiff = newDim - lt1move::Comp(E::Dims(pObj), nDim);

		State probe;
		m_E.SetupBP(probe, pState, pObj);

		vNewPos = E::Pos(pObj);
		fOldPos = lt1move::Comp(vNewPos, nDim);
		lt1move::Comp(vNewPos, nDim) -= fDiff;
		MoveObject(&probe, pObj, vNewPos, MO_DETACHSTANDING);
		vPos = E::Pos(pObj);
		fLow = lt1move::Comp(vPos, nDim);

		vNewPos = E::Pos(pObj);
		lt1move::Comp(vNewPos, nDim) += fDiff * 2.0f;
		MoveObject(&probe, pObj, vNewPos, MO_DETACHSTANDING);
		vPos = E::Pos(pObj);
		fHigh = lt1move::Comp(vPos, nDim);

		if (fHigh < fOldPos + fDiff)
		{
			if (fLow > fOldPos - fDiff)
			{
				vNewPos = E::Pos(pObj);
				fHalf = (fHigh - fLow) / 2.0f;
				lt1move::Comp(vNewPos, nDim) -= fHalf;
			}
			else
			{
				vNewPos = E::Pos(pObj);
				lt1move::Comp(vNewPos, nDim) -= fDiff * 2.0f;
				MoveObject(&probe, pObj, vNewPos, MO_DETACHSTANDING);
				vPos = E::Pos(pObj);
				fLow = lt1move::Comp(vPos, nDim);

				vNewPos = E::Pos(pObj);
				fHalf = (fHigh - fLow) / 2.0f;
				lt1move::Comp(vNewPos, nDim) += fHalf;
			}
		}
		else
		{
			vNewPos = E::Pos(pObj);
			fHalf = (fHigh - fLow) / 2.0f;
			lt1move::Comp(vNewPos, nDim) -= fHalf;
		}

		MoveObject(&probe, pObj, vNewPos, MO_DETACHSTANDING | MO_SETCHANGEFLAG | MO_MOVESTANDINGONS);
		lt1move::Comp(E::Dims(pObj), nDim) += fHalf;
	}

	// MaybeCollideWorldModel (SERVER 0x6cf14): a rotating world model
	// (m.m_pObj) against one object
	void MaybeCollideWorldModel(Move &m, Obj *pObj)
	{
		LTVector vNewPos, vPos, vDir, vMin, vMax, vSaveVel;

		if (!E::HasSData(pObj))
			return;

		if (!DoBoxesIntersect(E::MinBox(m.m_pObj), E::MaxBox(m.m_pObj), E::MinBox(pObj), E::MaxBox(pObj), 0.01f))
			return;

		if (!m_E.BoxInModel(m.m_pObj, E::MinBox(pObj), E::MaxBox(pObj)))
			return;

		if ((E::Flags(m.m_pObj) & FLAG_SOLID) && (E::Flags(pObj) & FLAG_SOLID))
		{
			vPos = E::Pos(pObj);
			MatVMul_H(&vNewPos, m.m_pTransform, &vPos);
			vDir = vNewPos - vPos;
			if (lt1move::MagSqr(vDir) < 0.001f)
				return;

			vDir = E::Pos(pObj) - E::Pos(m.m_pObj);
			lt1move::VecNorm(vDir);
			// 25.1 has no bound here; this one is only against a model whose
			// BSP never lets go of the box
			for (int nGuard = 0; nGuard < 100000; nGuard++)
			{
				vMin = vNewPos - E::Dims(pObj);
				vMax = vNewPos + E::Dims(pObj);
				if (!m_E.BoxInModel(m.m_pObj, vMin, vMax))
					break;
				vNewPos += vDir;
			}

			vSaveVel = E::Velocity(pObj);
			E::Velocity(pObj) = vPos - vNewPos;
			const bool bHit = CollideAgainstWorld(m, m.m_pObj, pObj, vNewPos, vPos,
				!(E::Flags(pObj) & FLAG_NOSLIDING), true);
			E::Velocity(pObj) = vSaveVel;

			if (bHit)
			{
				State child;
				m_E.SetupBP(child, m.m_pState, pObj);
				MoveObject(&child, pObj, vPos, MO_DETACHSTANDING | MO_SETCHANGEFLAG | MO_MOVESTANDINGONS);
				if (m_E.BoxInModel(m.m_pObj, E::MinBox(pObj), E::MaxBox(pObj)))
					m_E.Crush(pObj, m.m_pObj);
			}
		}
		else
		{
			DoNonsolidCollision(m.m_pObj, pObj);
		}
	}
};

#endif  // __ERA_LT1_MOVE_OBJECT_H__
