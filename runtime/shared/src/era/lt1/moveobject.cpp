// The LT1 set's object mover: 25.1's MoveObject, ChangeObjectDimensions and RotateWorldModel
// (lt1_move_object.h) on Jupiter's objects. moveobject.cpp's three functions reach it
// through the era set EngineEra 1 selects. Off unless LT1ObjectPass is set on both sides, which no var table does.
//
// What this side supplies that LT1 kept elsewhere:
//  - the candidate gather. LT1 files every object in the smallest BSP node holding its sphere and walks
//    those nodes' lists (FindObjectsInMovement). Jupiter keeps a WorldTree instead, so the gather asks it
//    for the swept box grown by 1, LT1's own box test, files each answer in LT1's tree afresh
//    (lt1_bsp_filing.h), and orders them as LT1's walk meets them: the move's sphere picking sides,
//    the back side first, and in a node the most recently filed first (m_nLT1FileStamp).
//    s_lt1vis.cpp does the same per ping;
//  - the tree itself: the world model whose BSP carries the vis tree, found as s_lt1vis.cpp finds it;
//  - the main world, which LT1 walks in MoveObject's own pass and Jupiter keeps as an object in the tree:
//    the first IsMainWorldModel object a box query of the move meets;
//  - the box in BSP test, which is Jupiter's DoesBoxIntersectBSP, and LT1's only under LT1ContainerBSP
//    and LT1BoxBSPProbes, both Shogo's

#include "bdefs.h"

#include "de_world.h"
#include "de_objects.h"
#include "de_mainworld.h"
#include "collision.h"
#include "moveobject.h"
#include "world_tree.h"
#include "world_client_bsp.h"
#include "world_server_bsp.h"
#include "era/era.h"
#include "era/lt1/lt1_bsp_filing.h"
#include "era/lt1/lt1_move_object.h"

#include <string.h>
#include <vector>
#include <algorithm>

#include "lt1_vars.h"

namespace
{
	// One box query's answers
	struct BoxQuery
	{
		std::vector<LTObject *> m_Objs;
	};

	void BoxQueryCB(WorldTreeObj *pTreeObj, void *pUser)
	{
		if (pTreeObj->GetObjType() != WTObj_DObject)
			return;
		((BoxQuery *)pUser)->m_Objs.push_back((LTObject *)pTreeObj);
	}

	// A candidate, where LT1 would have it filed
	struct Filed
	{
		int32		m_nNode;
		uint32		m_nStamp;
		LTObject	*m_pObj;
	};

	bool FiledBefore(const Filed &a, const Filed &b)
	{
		if (a.m_nNode != b.m_nNode)
			return a.m_nNode < b.m_nNode;
		return a.m_nStamp > b.m_nStamp;   // most recently filed first
	}

	static IWorldClientBSP *world_bsp_client;
	define_holder(IWorldClientBSP, world_bsp_client);
	static IWorldServerBSP *world_bsp_server;
	define_holder(IWorldServerBSP, world_bsp_server);

	// The LT1 tree to file in: the world model whose BSP carries the vis tree
	// the converter wrote, as s_lt1vis.cpp finds it. The main world object's
	// own BSP need not be that one
	template <class W>
	const WorldBsp *TreeIn(W *pWorld)
	{
		if (!pWorld || !pWorld->IsLoaded())
			return LTNULL;
		const uint32 nModels = pWorld->NumWorldModels();
		for (uint32 i = 0; i < nModels; ++i)
		{
			const WorldData *pData = pWorld->GetWorldModel(i);
			const WorldBsp *pBsp = pData ? pData->OriginalBSP() : LTNULL;
			if (pBsp && pBsp->HasPVS() && pBsp->m_pVisNodes && pBsp->m_nVisRoot >= 0)
				return pBsp;
		}
		return LTNULL;
	}

	const WorldBsp *Tree(bool bServer)
	{
		return bServer ? TreeIn(world_bsp_server) : TreeIn(world_bsp_client);
	}

	class JupiterEngine
	{
	public:
		typedef LTObject	Obj;
		typedef MoveState	State;

		struct CollisionFrame
		{
			CollisionInfo	m_Info;
			CollisionInfo	*m_pOld;
		};

		explicit JupiterEngine(MoveAbstract *pAbstract) : m_pAbstract(pAbstract) {}

		static LTVector &Pos(Obj *p)		{ return p->m_Pos; }
		static LTVector &Dims(Obj *p)		{ return p->m_Dims; }
		static LTVector &MinBox(Obj *p)		{ return p->m_MinBox; }
		static LTVector &MaxBox(Obj *p)		{ return p->m_MaxBox; }
		static LTVector &Velocity(Obj *p)	{ return p->m_Velocity; }
		static uint32 Flags(Obj *p)			{ return p->m_Flags; }
		static uint32 &InternalFlags(Obj *p)	{ return p->m_InternalFlags; }
		static int Type(Obj *p)				{ return p->m_ObjectType; }
		static float &Radius(Obj *p)		{ return p->m_Radius; }
		static bool HasWorldModel(Obj *p)	{ return p->HasWorldModel(); }
		static bool HasSData(Obj *p)		{ return p->sd != LTNULL; }
		static Obj *StandingOn(Obj *p)		{ return p->m_pStandingOn; }
		static bool OnNode(Obj *p)			{ return p->m_pNodeStandingOn != LTNULL; }
		static LTRotation &Rotation(Obj *p)	{ return p->m_Rotation; }

		void Enter(State *pState)
		{
			pState->SetupCall();
		}

		void Detach(Obj *p)
		{
			DetachObjectStanding(p);
		}

		void Stand(Obj *p, Obj *pOn, const void *pNode)
		{
			SetObjectStanding(p, pOn, (const Node *)pNode);
		}

		int Riders(Obj *p, Obj **ppOut, int nMax)
		{
			int n = 0;
			for (LTLink *pCur = p->m_ObjectsStandingOn.m_pNext; pCur != &p->m_ObjectsStandingOn && n < nMax; pCur = pCur->m_pNext)
				ppOut[n++] = (Obj *)pCur->m_pData;
			return n;
		}

		bool CanOptimize(Obj *p)					{ return !!m_pAbstract->CanOptimizeObject(p); }
		void CheckMaxPos(State *pState, LTVector *pPos)	{ m_pAbstract->CheckMaxPos(pState, pPos); }
		void BreakContainerLinks(Obj *p)			{ m_pAbstract->BreakContainerLinks(p); }
		void SetChangeFlags(Obj *p, uint32 flags)	{ m_pAbstract->SetObjectChangeFlags(p, flags); }
		void MoveAttachments(State *pState)			{ m_pAbstract->MoveAttachments(pState); }
		bool ShouldPush(State *pState, Obj *a, Obj *b)	{ return !!m_pAbstract->ShouldPushObject(pState, a, b); }
		void Crush(Obj *p, Obj *pCrusher)			{ m_pAbstract->DoCrush(p, pCrusher); }
		void PutInContainer(Obj *p, Obj *pCont)		{ m_pAbstract->PutObjectInContainer(p, pCont); }
		float ForceLimitSqr(Obj *p)					{ return p->m_ForceIgnoreLimitSqr; }

		// 25.1's CollideAgainstWorld touches the mover whatever its flags
		// (SMoveAbstract::DoTouchNotify sends MID_TOUCHNOTIFY unasked)
		void Touch(Obj *p, void *hObject, LTVector &vStopVel, float fForce)
		{
			m_pAbstract->DoTouchNotify(p, (Obj *)hObject, vStopVel, fForce);
		}

		void TouchObject(Obj *p, Obj *pOther, LTVector &vStopVel, float fForce)
		{
			m_pAbstract->DoTouchNotify(p, pOther, vStopVel, fForce);
		}

		void PushCollisionInfo(CollisionFrame &f)
		{
			f.m_pOld = m_pAbstract->GetCollisionInfo();
			m_pAbstract->GetCollisionInfo() = &f.m_Info;
			memset((void *)&f.m_Info, 0, sizeof(f.m_Info));
		}

		void PopCollisionInfo(CollisionFrame &f)
		{
			m_pAbstract->GetCollisionInfo() = f.m_pOld;
		}

		void ClearCollisionPlane()
		{
			CollisionInfo *pInfo = m_pAbstract->GetCollisionInfo();
			if (!pInfo)
				return;
			pInfo->m_Plane.m_Normal.Init(0.0f, 0.0f, 0.0f);
			pInfo->m_Plane.m_Dist = 0.0f;
			pInfo->m_hPoly = INVALID_HPOLY;
		}

		int CollideWithWorld(State *pState, Obj *pWorld, Obj *pMover, const LTVector &vStart,
			LTVector &vEnd, bool bSlide, lt1move::WorldHit &hit)
		{
			hit.m_nHits = 0;
			hit.m_pStandingOn = LTNULL;
			hit.m_Force.Init(0.0f, 0.0f, 0.0f);
			hit.m_VelOffset.Init(0.0f, 0.0f, 0.0f);
			hit.m_hObject = LTNULL;
			hit.m_pWorldObj = LTNULL;

			if (!pWorld)
				pWorld = MainWorld(pState, vStart, vEnd, pMover->GetDims());
			if (!pWorld || !HasWorldModel(pWorld) || !pWorld->ToWorldModel()->m_pValidBsp)
				return 0;

			CollideRequest request;
			CollideInfo info;
			request.m_pAbstract = m_pAbstract;
			request.m_pCollisionInfo = m_pAbstract->GetCollisionInfo();
			request.m_pWorld = pWorld->ToWorldModel()->m_pValidBsp;
			request.m_pWorldObj = pWorld;
			request.m_OriginalPos = vStart;
			request.m_NewPos = vEnd;
			request.m_Dims = pMover->GetDims();
			request.m_pObject = pMover;
			request.m_bSlide = bSlide;
			request.m_nRestart = 0;

			::CollideWithWorld(request, &info);
			vEnd = info.m_FinalPos;

			hit.m_nHits = info.m_nHits;
			hit.m_pStandingOn = info.m_pStandingOn;
			hit.m_Force = info.m_vForce;
			hit.m_VelOffset = info.m_VelOffset;
			hit.m_hObject = m_pAbstract->GetCollisionInfo() ? (void *)m_pAbstract->GetCollisionInfo()->m_hObject : LTNULL;
			hit.m_pWorldObj = pWorld;
			return (int)info.m_nHits;
		}

		bool BoxInModel(Obj *pModel, const LTVector &vMin, const LTVector &vMax)
		{
			const WorldBsp *pBsp = pModel->ToWorldModel()->m_pValidBsp;
			if (!pBsp)
				return false;
			return DoesBoxIntersectBSP(pBsp->GetRootNode(), vMin, vMax, pModel->m_ObjectType == OT_CONTAINER);
		}

		void Respond(State *pState, Obj *a, Obj *b, const LTVector &vNormal, float fDist)
		{
			LTVector vPlane = vNormal;
			DoInterObjectCollisionResponse(m_pAbstract, a, b, &vPlane, fDist, INVALID_HPOLY);
		}

		int CustomList(State *pState, Obj ***pppList)
		{
			*pppList = pState->m_CustomTestObjects;
			return (int)pState->m_nCustomTestObjects;
		}

		void Refile(State *pState, Obj *p)
		{
			pState->m_pWorldTree->InsertObject(p);
		}

		void Inherit(State &child, State *pParent, Obj *p)
		{
			child.Inherit(pParent, p);
		}

		void SetupBP(State &child, State *pParent, Obj *p)
		{
			child.Setup(pParent->m_pWorldTree, pParent->m_pAbstract, p, p->m_BPriority);
		}

		void RetransformWorldModel(Obj *p)
		{
			::RetransformWorldModel(p->ToWorldModel());
		}

		void SetupWorldModelTransform(Obj *p)	{ obj_SetupWorldModelTransform(p->ToWorldModel()); }
		const LTMatrix &Transform(Obj *p)		{ return p->ToWorldModel()->m_Transform; }

		void ModelBox(Obj *p, LTVector &vMin, LTVector &vMax)
		{
			const WorldBsp *pBsp = p->ToWorldModel()->m_pOriginalBsp;
			vMin = pBsp->m_MinBox;
			vMax = pBsp->m_MaxBox;
		}

		// FindObjectsInMovement (SERVER 0x6c370), on Jupiter's tree: see the
		// file header
		int Gather(State *pState, const LT1MoveObject<JupiterEngine>::Move &m, Obj **ppOut, int nMax)
		{
			LTVector vMin = m.m_MoveMin, vMax = m.m_MoveMax;
			vMin -= LTVector(1.0f, 1.0f, 1.0f);
			vMax += LTVector(1.0f, 1.0f, 1.0f);

			BoxQuery q;
			pState->m_pWorldTree->FindObjectsInBox(&vMin, &vMax, BoxQueryCB, &q);

			// the main world is not an object in LT1's tree
			std::vector<LTObject *> cands;
			for (size_t i = 0; i < q.m_Objs.size(); ++i)
			{
				LTObject *pObj = q.m_Objs[i];
				if (pObj->IsMainWorldModel())
					continue;
				if (pObj == m.m_pObj)
					continue;
				if (!(LT1MoveObject<JupiterEngine>::IsPhysical(pObj->m_Flags) || pObj->m_ObjectType == OT_CONTAINER))
					continue;
				if (!pState->m_bServer)
				{
					if ((pObj->m_Flags & FLAG_CLIENTNONSOLID) || !(pObj->m_Flags & FLAG_SOLID))
						continue;
				}
				if (!LT1MoveObject<JupiterEngine>::DoBoxesIntersect(pObj->m_MinBox, pObj->m_MaxBox,
					m.m_MoveMin, m.m_MoveMax, -1.0f))
					continue;
				cands.push_back(pObj);
			}

			const WorldBsp *pBsp = Tree(!!pState->m_bServer);
			if (!pBsp)
			{
				NoTree();
				int n = 0;
				for (size_t i = 0; i < cands.size() && n < nMax; ++i)
					ppOut[n++] = cands[i];
				return n;
			}

			std::vector<Filed> filed;
			File(pBsp, cands, filed);

			// the walk, with LT1's explicit stack: a straddled node's back side
			// is taken now and its front pushed
			int n = 0;
			int32 stack[1000];
			int nStack = 0;
			int32 nIdx = pBsp->m_nVisRoot;
			for (;;)
			{
				while (nIdx < 0 || (uint32)nIdx >= pBsp->m_nVisNodes)
				{
					if (nStack == 0)
						return n;
					nIdx = stack[--nStack];
				}

				if (!Take(filed, nIdx, ppOut, n, nMax))
					return n;

				const SVisNode &v = pBsp->m_pVisNodes[nIdx];
				const LTPlane *pPlane = lt1filing::NodePlane(pBsp, v);
				if (!pPlane)
				{
					nIdx = -1;
					continue;
				}
				const float fDist = pPlane->DistTo(m.m_Center);
				if (fDist < -m.m_Radius)
					nIdx = v.m_Slot0;
				else if (fDist > m.m_Radius)
					nIdx = v.m_Slot1;
				else
				{
					if (nStack < 1000)
						stack[nStack++] = v.m_Slot1;
					nIdx = v.m_Slot0;
				}
			}
		}

		// RecurseAndCollideWorldModel's walk (SERVER 0x6d3cc), flattened: the
		// objects filed in each node the model's sphere reaches, the node's own
		// first, then the front side, then the back
		int GatherModel(State *pState, Obj *pModel, Obj **ppOut, int nMax)
		{
			BoxQuery q;
			pState->m_pWorldTree->FindObjectsInBox(&pModel->m_MinBox, &pModel->m_MaxBox, BoxQueryCB, &q);

			// the main world is not an object in LT1's tree
			std::vector<LTObject *> cands;
			for (size_t i = 0; i < q.m_Objs.size(); ++i)
			{
				LTObject *pObj = q.m_Objs[i];
				if (pObj->IsMainWorldModel())
					continue;
				if (pObj != pModel)
					cands.push_back(pObj);
			}

			const WorldBsp *pBsp = Tree(!!pState->m_bServer);
			if (!pBsp)
			{
				NoTree();
				int n = 0;
				for (size_t i = 0; i < cands.size() && n < nMax; ++i)
					ppOut[n++] = cands[i];
				return n;
			}

			std::vector<Filed> filed;
			File(pBsp, cands, filed);
			int n = 0;
			Recurse(pBsp, filed, pBsp->m_nVisRoot, pModel->GetPos(), pModel->m_Radius, ppOut, n, nMax, 0);
			return n;
		}

	private:
		MoveAbstract	*m_pAbstract;

		// The main world: the first IsMainWorldModel object in the move's box
		LTObject *MainWorld(State *pState, const LTVector &vStart, const LTVector &vEnd, const LTVector &vDims)
		{
			LTVector vMin, vMax;
			VEC_MIN(vMin, vStart, vEnd);
			VEC_MAX(vMax, vStart, vEnd);
			vMin -= vDims;
			vMax += vDims;
			BoxQuery q;
			pState->m_pWorldTree->FindObjectsInBox(&vMin, &vMax, BoxQueryCB, &q);
			for (size_t i = 0; i < q.m_Objs.size(); ++i)
				if (q.m_Objs[i]->IsMainWorldModel())
					return q.m_Objs[i];
			return LTNULL;
		}

		// Files each candidate where LT1 would have it: an object LT1 does not
		// link (the abstraction's CanOptimizeObject) or whose sphere is
		// wholly in solid space is in no node, and no walk meets it
		void File(const WorldBsp *pBsp, const std::vector<LTObject *> &cands, std::vector<Filed> &filed)
		{
			for (size_t i = 0; i < cands.size(); ++i)
			{
				LTObject *pObj = cands[i];
				if (m_pAbstract->CanOptimizeObject(pObj))
					continue;
				const int32 nNode = lt1filing::FindMinimumNode(pBsp, pObj->GetPos(), lt1filing::Radius(pObj));
				if (nNode < 0)
					continue;
				Filed f;
				f.m_nNode = nNode;
				f.m_nStamp = pObj->m_nLT1FileStamp;
				f.m_pObj = pObj;
				filed.push_back(f);
			}
			std::sort(filed.begin(), filed.end(), FiledBefore);
		}

		// The objects filed at one node, in LT1's order; false when the list is full
		static bool Take(const std::vector<Filed> &filed, int32 nIdx, Obj **ppOut, int &n, int nMax)
		{
			Filed key;
			key.m_nNode = nIdx;
			key.m_nStamp = 0xFFFFFFFF;
			key.m_pObj = LTNULL;
			std::vector<Filed>::const_iterator it = std::lower_bound(filed.begin(), filed.end(), key, FiledBefore);
			for (; it != filed.end() && it->m_nNode == nIdx; ++it)
			{
				ppOut[n++] = it->m_pObj;
				if (n >= nMax)
					return false;
			}
			return true;
		}

		static void Recurse(const WorldBsp *pBsp, const std::vector<Filed> &filed, int32 nIdx,
			const LTVector &vPos, float fRadius, Obj **ppOut, int &n, int nMax, int nDepth)
		{
			while (nIdx >= 0 && (uint32)nIdx < pBsp->m_nVisNodes && nDepth < 4096)
			{
				if (!Take(filed, nIdx, ppOut, n, nMax))
					return;

				const SVisNode &v = pBsp->m_pVisNodes[nIdx];
				const LTPlane *pPlane = lt1filing::NodePlane(pBsp, v);
				if (!pPlane)
					return;
				const float fDist = pPlane->DistTo(vPos);
				if (fDist < -fRadius)
					nIdx = v.m_Slot0;
				else if (fDist > fRadius)
					nIdx = v.m_Slot1;
				else
				{
					Recurse(pBsp, filed, v.m_Slot1, vPos, fRadius, ppOut, n, nMax, nDepth + 1);
					nIdx = v.m_Slot0;
				}
				++nDepth;
			}
		}

		static void NoTree()
		{
			static bool s_bSaid = false;
			if (!s_bSaid)
			{
				s_bSaid = true;
				dsi_ConsolePrint("LT1ObjectPass: this world carries no LT1 tree; candidates in Jupiter's order");
			}
		}
	};

}

bool era_lt1_MoveObject(MoveState *pState, const LTVector &vNewPos, uint32 flags)
{
	if (!g_bLT1ObjectPass)
		return false;
	JupiterEngine e(pState->m_pAbstract);
	LT1MoveObject<JupiterEngine> mover(e);
	mover.MoveObject(pState, pState->m_pObj, vNewPos, flags);
	return true;
}

bool era_lt1_ChangeObjectDimensions(MoveState *pState, LTVector &newDims, LTBOOL bCollide,
	LTBOOL bTrivialReject, LTBOOL *pbRet)
{
	if (!g_bLT1ObjectPass)
		return false;
	pState->SetupCall();
	JupiterEngine e(pState->m_pAbstract);
	LT1MoveObject<JupiterEngine> mover(e);
	*pbRet = mover.ChangeObjectDimensions(pState, pState->m_pObj, newDims, !!bCollide, !!bTrivialReject) ? LTTRUE : LTFALSE;
	return true;
}

// Jupiter's client rotates without collisions (bDoCollisions false); the
// rotation is still stored and the model re-filed, which is what Jupiter does.
// 25.1's server has no such switch, and its client copy is unread.
bool era_lt1_RotateWorldModel(MoveState *pState, const LTRotation &cRotation, LTBOOL bDoCollisions)
{
	if (!g_bLT1ObjectPass)
		return false;
	if (!bDoCollisions)
		return false;
	pState->SetupCall();
	JupiterEngine e(pState->m_pAbstract);
	LT1MoveObject<JupiterEngine> mover(e);
	mover.RotateWorldModel(pState, pState->m_pObj, cRotation);
	return true;
}
