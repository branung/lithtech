// Where LT1 files an object in its BSP: the smallest node of the FastNode tree that holds the object's
// bounding sphere (obj_FindMinimumNode, 25.1 SERVER 0x5488c, 27.1 0x41be50). The converter writes that
// tree into WorldBsp::m_pVisNodes node for node, on both sides.
// Shared by the server's visibility walk (s_lt1vis.cpp) and the object pass's gather (moveobject.cpp),
// which both need LT1's filing without keeping LT1's per node lists in step with every move.
// Planes come from each node's polygon, as everywhere else in the port. The file's own plane D differs
// by more than 0.01 on 919 of Shogo's 225,505 nodes, so an object that close to a plane can be filed
// one level differently

#ifndef __ERA_LT1_BSP_FILING_H__
#define __ERA_LT1_BSP_FILING_H__

namespace lt1filing
{
	inline const LTPlane *NodePlane(const WorldBsp *pBsp, const SVisNode &v)
	{
		if (v.m_nBspNode >= pBsp->m_nNodes)
			return LTNULL;
		const WorldPoly *pPoly = pBsp->m_Nodes[v.m_nBspNode].GetPoly();
		return pPoly ? pPoly->GetPlane() : LTNULL;
	}

	// LT1's bounding radius: |half-extents| + 0.5 (ChangeObjectDimensions), and
	// + 0.1 for a world model (SetupWorldModelDims, 25.1).
	inline float Radius(const LTObject *pObj)
	{
		return pObj->GetDims().Mag() + (pObj->m_ObjectType == OT_WORLDMODEL ? 0.1f : 0.5f);
	}

	// obj_FindMinimumNode: descend while the sphere is wholly on one side; -1
	// when it goes wholly into solid space (the object is then not linked at
	// all), and the current node when the front side is empty space.
	inline int32 FindMinimumNode(const WorldBsp *pBsp, const LTVector &vPos, float fRadius)
	{
		int32 nIdx = pBsp->m_nVisRoot;
		for (uint32 nGuard = 0; nGuard < 4096 && nIdx >= 0 && (uint32)nIdx < pBsp->m_nVisNodes; ++nGuard)
		{
			const SVisNode &v = pBsp->m_pVisNodes[nIdx];
			const LTPlane *pPlane = NodePlane(pBsp, v);
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
}

#endif  // __ERA_LT1_BSP_FILING_H__
