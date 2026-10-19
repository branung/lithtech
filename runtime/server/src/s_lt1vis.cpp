// s_lt1vis.cpp: LT1's server side visibility traversal, giving the objects PingObjects reaches and LT1ClientPVS sends

#include "bdefs.h"
#include "de_objects.h"
#include "de_world.h"
#include "servermgr.h"
#include "serverobj.h"
#include "world_server_bsp.h"
#include <vector>
#include <algorithm>

extern int32 g_bLT1FileOrder;

// LT1's order within a node: newest filed first
static bool LT1FiledLater(const LTObject *a, const LTObject *b)
{
	return a->m_nLT1FileStamp > b->m_nLT1FileStamp;
}

static IWorldServerBSP *world_bsp_server;
define_holder(IWorldServerBSP, world_bsp_server);

static const WorldBsp *LT1VisBsp()
{
	if (!world_bsp_server || !world_bsp_server->IsLoaded())
		return LTNULL;
	uint32 nModels = world_bsp_server->NumWorldModels();
	for (uint32 i = 0; i < nModels; ++i)
	{
		const WorldData *pData = world_bsp_server->GetWorldModel(i);
		const WorldBsp *pBsp = pData ? pData->OriginalBSP() : LTNULL;
		if (pBsp && pBsp->HasPVS())
			return pBsp;
	}
	return LTNULL;
}

// LT1 runs neither the countdown nor pings without vis data
bool sm_LT1WorldHasVis()
{
	return LT1VisBsp() != LTNULL;
}

static const LTPlane *LT1NodePlane(const WorldBsp *pBsp, const SVisNode &v)
{
	if (v.m_nBspNode >= pBsp->m_nNodes)
		return LTNULL;
	const WorldPoly *pPoly = pBsp->m_Nodes[v.m_nBspNode].GetPoly();
	return pPoly ? pPoly->GetPlane() : LTNULL;
}

// The objects LT1 links into the BSP: those with a special effect message or any flag below (unless FLAG_FORCEOPTIMIZEOBJECT), 
// and anything with a deactivation time
static bool LT1IsLinked(const LTObject *pObj)
{
	if (!(pObj->m_InternalFlags & IFLAG_INWORLD) || !pObj->sd)
		return false;
	if (pObj->sd->m_fLT1DeactivateTime > 0.0f)
		return true;
	if (!pObj->sd->m_cSpecialEffectMsg.Empty())
		return true;
	const uint32 nMask = FLAG_VISIBLE | FLAG_SOLID | FLAG_TOUCH_NOTIFY | FLAG_GOTHRUWORLD |
		FLAG_RAYHIT | FLAG_FORCECLIENTUPDATE;
	return (pObj->m_Flags & nMask) && !(pObj->m_Flags & FLAG_FORCEOPTIMIZEOBJECT);
}

// LT1's bounding radius: the half extents' length + 0.5, or + 0.1 for a world model
static float LT1Radius(const LTObject *pObj)
{
	return pObj->GetDims().Mag() + (pObj->m_ObjectType == OT_WORLDMODEL ? 0.1f : 0.5f);
}

// Descends while the sphere is wholly on one side. 
// -1 if it goes wholly into solid space, so the object isn't linked
static int32 LT1FindMinimumNode(const WorldBsp *pBsp, const LTVector &vPos, float fRadius)
{
	int32 nIdx = pBsp->m_nVisRoot;
	for (uint32 nGuard = 0; nGuard < 4096 && nIdx >= 0 && (uint32)nIdx < pBsp->m_nVisNodes; ++nGuard)
	{
		const SVisNode &v = pBsp->m_pVisNodes[nIdx];
		const LTPlane *pPlane = LT1NodePlane(pBsp, v);
		if (!pPlane)
			return nIdx;
		const float d = pPlane->DistTo(vPos);
		if (d > fRadius)
		{
			if (v.m_Slot1 < 0)
				return nIdx;
			nIdx = v.m_Slot1;
		}
		else if (d < -fRadius)
		{
			if (v.m_Slot0 < 0)
				return -1;
			nIdx = v.m_Slot0;
		}
		else
			return nIdx;
	}
	return nIdx;
}

namespace
{
	struct SWalk
	{
		const WorldBsp *m_pBsp;
		std::vector<std::vector<LTObject *> > m_NodeObjs;	// Filed objects per vis node
		std::vector<LTObject *> m_Reached;					// Objects this walk has reached
		const uint8 *m_pData;
		uint32 m_nBits, m_nBit;
		void (*m_fn)(LTObject *, void *);
		void *m_pUser;

		bool Bit()
		{
			if (m_nBit >= m_nBits)
				return false;
			uint32 i = m_nBit++;
			return ((m_pData[i >> 3] >> (i & 7)) & 1) != 0;
		}
	};

	static bool IsReached(const SWalk &w, LTObject *pObj)
	{
		for (size_t i = 0; i < w.m_Reached.size(); ++i)
			if (w.m_Reached[i] == pObj)
				return true;
		return false;
	}

	// LT1's RecurseAndCallback
	static void Walk(SWalk &w, int32 nIdx, std::vector<LTObject *> list, uint32 nDepth)
	{
		const WorldBsp *pBsp = w.m_pBsp;
		while (nIdx >= 0 && (uint32)nIdx < pBsp->m_nVisNodes && nDepth < 4096)
		{
			const SVisNode &v = pBsp->m_pVisNodes[nIdx];

			// This node's objects not reached yet
			const std::vector<LTObject *> &objs = w.m_NodeObjs[nIdx];
			for (size_t i = 0; i < objs.size(); ++i)
				if (!IsReached(w, objs[i]))
					list.push_back(objs[i]);

			if (v.m_nLeaf != 0xFFFF && w.Bit())
			{
				for (size_t i = 0; i < list.size(); ++i)
				{
					if (!IsReached(w, list[i]))
					{
						w.m_Reached.push_back(list[i]);
						w.m_fn(list[i], w.m_pUser);
					}
				}
				list.clear();
			}

			const bool bBack = v.m_Slot0 >= 0, bFront = v.m_Slot1 >= 0;
			const LTPlane *pPlane = LT1NodePlane(pBsp, v);

			if (bBack && w.Bit())
			{
				std::vector<LTObject *> back;
				for (size_t i = 0; i < list.size(); ++i)
				{
					LTObject *o = list[i];
					if (!IsReached(w, o) && (!pPlane || pPlane->DistTo(o->GetPos()) < LT1Radius(o)))
						back.push_back(o);
				}
				if (bFront)
					Walk(w, v.m_Slot0, back, nDepth + 1); // Keeps the parent's list for the front
				else
				{
					nIdx = v.m_Slot0;
					list.swap(back);
					++nDepth;
					continue;
				}
			}
			if (bFront && w.Bit())
			{
				std::vector<LTObject *> front;
				for (size_t i = 0; i < list.size(); ++i)
				{
					LTObject *o = list[i];
					if (!IsReached(w, o) && (!pPlane || pPlane->DistTo(o->GetPos()) > -LT1Radius(o)))
						front.push_back(o);
				}
				nIdx = v.m_Slot1;
				list.swap(front);
				++nDepth;
				continue;
			}
			return;
		}
	}
}

// Calls fn for every object the walk reaches from vPos
void sm_LT1VisitVisibleObjects(const LTVector &vPos, void (*fn)(LTObject *pObj, void *pUser), void *pUser)
{
	const WorldBsp *pBsp = LT1VisBsp();
	if (!pBsp || !fn)
		return;

	const uint32 nLeaf = pBsp->PVSFindLeaf(vPos);
	if (nLeaf == kPVSNoLeaf || nLeaf >= pBsp->m_nVisLeaves)
		return;

	SWalk w;
	w.m_pBsp = pBsp;
	w.m_fn = fn;
	w.m_pUser = pUser;
	w.m_NodeObjs.resize(pBsp->m_nVisNodes);

	// File every linked server object
	LTLink *pHead = &g_pServerMgr->m_Objects.m_Head;
	for (LTLink *pCur = pHead->m_pNext; pCur != pHead; pCur = pCur->m_pNext)
	{
		LTObject *pObj = (LTObject *)pCur->m_pData;
		if (!pObj || !LT1IsLinked(pObj))
			continue;
		const int32 nNode = LT1FindMinimumNode(pBsp, pObj->GetPos(), LT1Radius(pObj));
		if (nNode >= 0 && (uint32)nNode < pBsp->m_nVisNodes)
			w.m_NodeObjs[nNode].push_back(pObj);
	}

	// The reach order sets the order pings wake objects and LT1ClientPVS sends them
	if (g_bLT1FileOrder)
		for (size_t n = 0; n < w.m_NodeObjs.size(); ++n)
			std::stable_sort(w.m_NodeObjs[n].begin(), w.m_NodeObjs[n].end(), LT1FiledLater);

	const SVisLeaf &lf = pBsp->m_pVisLeaves[nLeaf];
	if (lf.m_nNumLists == 0)
	{
		// No vis data for this leaf, so every filed object
		for (size_t n = 0; n < w.m_NodeObjs.size(); ++n)
			for (size_t i = 0; i < w.m_NodeObjs[n].size(); ++i)
				fn(w.m_NodeObjs[n][i], pUser);
		return;
	}

	// The base list only as later lists belong to portals
	const uint32 nList = lf.m_nFirstList;
	if (nList >= pBsp->m_nVisLists)
		return;
	const SVisList &entry = pBsp->m_pVisLists[nList];
	if ((uint32)entry.m_nOffset + entry.m_nSize > pBsp->m_nVisDataSize)
		return;
	w.m_pData = &pBsp->m_pVisData[entry.m_nOffset];
	w.m_nBits = (uint32)entry.m_nSize * 8;
	w.m_nBit = 0;
	Walk(w, pBsp->m_nVisRoot, std::vector<LTObject *>(), 0);
}
