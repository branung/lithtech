// This module does line segment intersects in the BSP.

#include "bdefs.h"
#include "de_world.h"
#include "intersect_line.h"


#define INTERSECT_EPSILON	0.01f


struct RayStack
{
	// The node to go into when this gets popped off the stack.
	Node		*m_GoInto, *m_pNode;
	LTVector	m_IPoint, m_Pt2;
};


// Tells if pPt is inside the convex poly.
inline LTBOOL InsideConvex(WorldPoly *pPoly, LTVector *pPt)
{
	LTPlane edgePlane;
	float edgeDot;
	LTVector *pNormal;
	uint32 iCur, iPrev, iEnd;

	// Reject it if it's outside the radius of the poly
	if (pPoly->GetCenter().DistSqr(*pPt) > (pPoly->GetRadius() * pPoly->GetRadius()))
		return LTFALSE;

	pNormal = &pPoly->GetPlane()->m_Normal;
	iEnd  = pPoly->GetNumVertices();
	iPrev = iEnd - 1;

	for(iCur=0; iCur != iEnd; )
	{
		LTVector &currVec = pPoly->GetVertex(iCur);
		LTVector vTemp = pPoly->GetVertex(iPrev) - currVec;
		edgePlane.m_Normal = pNormal->Cross(vTemp);
		edgePlane.m_Normal.Norm();
		edgePlane.m_Dist = edgePlane.m_Normal.Dot(currVec);

		edgeDot = edgePlane.DistTo(*pPt);
		if(edgeDot < -INTERSECT_EPSILON)
			return LTFALSE;

		iPrev = iCur;
		++iCur;
	}

	return LTTRUE;
}

LTBOOL InternalIntersectLineNode(
	const Node *pRoot,
	IntersectRequest *pRequest,
	LTVector point1,
	const LTVector& point2)
{
	LTVector iPoint;
	float intersection_t;
	float dot1, dot2;
	int side1;

	while((pRoot->m_Flags & (NF_IN|NF_OUT)) == 0)
	{
		// Go into the correct side.
		dot1 = pRoot->GetPlane()->DistTo(point1);
		dot2 = pRoot->GetPlane()->DistTo(point2);

		// Handle the segment being entirely on one side of the plane
		if(dot1 > INTERSECT_EPSILON && dot2 > INTERSECT_EPSILON)
		{
			pRoot = pRoot->m_Sides[FrontSide];
		}
		else if(dot1 < -INTERSECT_EPSILON && dot2 < -INTERSECT_EPSILON)
		{
			pRoot = pRoot->m_Sides[BackSide];
		}
		else
		{
			// Ok, it crosses this plane.. go into the side that pFrom is on first.
			if ((dot1 < -INTERSECT_EPSILON) || (dot1 > INTERSECT_EPSILON))
				side1 = (int)(dot1 > 0.0f);
			else
				side1 = (int)(dot2 < 0.0f);

			// Get the difference between the distances
			intersection_t = dot2 - dot1;
			bool bOnPlane = false;
			// Find the point of intersection
			if (intersection_t != 0.0f)
				intersection_t = -dot1 / intersection_t;
			else
				bOnPlane = true;

			if ((dot1 < -INTERSECT_EPSILON) || (dot1 > INTERSECT_EPSILON) || bOnPlane)
			{
				float fBackSideT = intersection_t + INTERSECT_EPSILON;
				fBackSideT = LTMIN(fBackSideT, 1.0f);
				VEC_LERP(iPoint, point1, point2, fBackSideT);
				// Test the side the starting point is on.
				if (InternalIntersectLineNode(
					pRoot->m_Sides[side1], 
					pRequest,
					point1,
					bOnPlane ? point2 : iPoint))
				{
					return LTTRUE;
				}
			}

			// Check for a polygon intersection
			if((side1 == FrontSide) && (pRoot->m_pPoly))
			{
				// LT1 only lets solid polys stop a ray, as its levels put invisible nonsolid hullmaker polys across doorways.
				// Checked here specifically so a skipped poly still clips the ray and continues instead of ending the ray early.
				extern int32 g_bLT1RaySolidOnly;
				if (!g_bLT1RaySolidOnly || (pRoot->m_pPoly->GetSurface()->m_Flags & SURF_SOLID))
				{
				VEC_LERP(iPoint, point1, point2, intersection_t);
				if(InsideConvex(pRoot->m_pPoly, &iPoint))
				{
					LTBOOL bDone = LTTRUE;

					IntersectQuery* pQuery = pRequest->m_pQuery;
					if (pQuery && pQuery->m_PolyFilterFn && pRequest->m_pWorldBsp)
					{
						if (!pQuery->m_PolyFilterFn(pRequest->m_pWorldBsp->MakeHPoly(pRoot), pQuery->m_pUserData))
						{
							bDone = LTFALSE;
						}
					}

					if (bDone)
					{
						// Congratulations, we have a winner!
						pRequest->m_pNodeHit = pRoot;
						*pRequest->m_pIPos = iPoint;
						return LTTRUE;
					}
				}
				}
			}
			// Jump out if the ray doesn't go to the "other" side
			else if (intersection_t > (1.0f - INTERSECT_EPSILON))
				break;

			// Clip the segment to the plane
			float fFontSideT = intersection_t - INTERSECT_EPSILON;
			fFontSideT = LTMAX(fFontSideT, 0.0f);
			VEC_LERP(iPoint, point1, point2, fFontSideT);
			point1 = iPoint;

			// Go into the other side.
			pRoot = pRoot->m_Sides[!side1];
		}
	}

	return LTFALSE;
}


LTBOOL IntersectLineNode(
	const Node *pRoot,
	IntersectRequest *pRequest)
{
	return InternalIntersectLineNode(
		pRoot,
		pRequest,
		*pRequest->m_pPoints[0],
		*pRequest->m_pPoints[1]
		);
}


// LT1's world ray for a query without INTERSECT_HPOLY (LT1IntersectLeaf).
// It never looks at a polygon and instead goes through the BSP's leaves along the segment and stops at the first solid one.
// The hit point is where the segment entered that leaf, and the plane is the last one crossed, turned to face the start.
// A plane only counts as crossed if the ends are at least 0.1 to either side of it, where the polygon search uses 0.01,
// so a short step that starts or ends within 0.1 of a floor isn't a hit. A segment that starts in solid hits nothing
const Node* IntersectLineLT1Leaf(const Node *pRoot, const LTVector *pPoint1, const LTVector *pPoint2,
	LTVector *pIPos, LTPlane *pIPlane)
{
	const float kEpsilon = 0.1f;
	struct LT1RayStack
	{
		const Node	*m_GoInto, *m_pNode;
		LTVector	m_IPoint, m_Pt2;
	};
	LT1RayStack stack[400];
	LT1RayStack *pStackPos = stack;
	const Node *pLastNode = LTNULL;
	LTVector pt1 = *pPoint1, pt2 = *pPoint2;

	while (1)
	{
		while (pRoot->m_Flags & NF_IN)
		{
			if (pStackPos == stack)
				return LTNULL;

			--pStackPos;
			pLastNode = pStackPos->m_pNode;
			pt1 = pStackPos->m_IPoint;
			pt2 = pStackPos->m_Pt2;
			pRoot = pStackPos->m_GoInto;
		}

		if (pRoot->m_Flags & NF_OUT)
		{
			if (pLastNode)
			{
				*pIPos = pt1;

				const LTPlane *pPlane = pLastNode->GetPlane();
				if (pPlane->DistTo(*pPoint1) > 0.0f)
				{
					*pIPlane = *pPlane;
				}
				else
				{
					pIPlane->m_Normal = -pPlane->m_Normal;
					pIPlane->m_Dist = -pPlane->m_Dist;
				}
			}

			return pLastNode;
		}

		const float dot1 = pRoot->GetPlane()->DistTo(pt1);
		const float dot2 = pRoot->GetPlane()->DistTo(pt2);

		if (dot1 > -kEpsilon && dot2 > -kEpsilon)
		{
			pRoot = pRoot->m_Sides[FrontSide];
		}
		else if (dot1 < kEpsilon && dot2 < kEpsilon)
		{
			pRoot = pRoot->m_Sides[BackSide];
		}
		else
		{
			// LT1's stack held 400 deferred sides and ran off its end past that. Stop instead
			if (pStackPos == stack + 400)
				return LTNULL;

			const int side1 = dot1 > 0.0f;
			const float t = -dot1 / (dot2 - dot1);

			// Push the far side, then go into the near one
			VEC_LERP(pStackPos->m_IPoint, pt1, pt2, t);
			pStackPos->m_GoInto = pRoot->m_Sides[!side1];
			pStackPos->m_Pt2 = pt2;
			pStackPos->m_pNode = pRoot;

			pt2 = pStackPos->m_IPoint;
			pRoot = pRoot->m_Sides[side1];
			++pStackPos;
		}
	}
}

// LT1's inside test for a polygon: no radius pretest, no tolerance on the edges, and a non solid polygon is never inside
static bool InsideConvexLT1(const WorldPoly *pPoly, const LTVector &vPt)
{
	if (!(pPoly->GetSurface()->m_Flags & SURF_SOLID))
		return false;

	const LTVector &vNormal = pPoly->GetPlane()->m_Normal;
	const uint32 nVerts = pPoly->GetNumVertices();
	uint32 iPrev = nVerts - 1;
	for (uint32 iCur = 0; iCur != nVerts; iPrev = iCur, ++iCur)
	{
		const LTVector &vCur = pPoly->GetVertex(iCur);
		LTPlane edge;
		edge.m_Normal = vNormal.Cross(pPoly->GetVertex(iPrev) - vCur);
		edge.m_Normal.Norm();
		edge.m_Dist = edge.m_Normal.Dot(vCur);
		if (edge.DistTo(vPt) < 0.0f)
			return false;
	}
	return true;
}

LTBOOL IntersectLineNodeLT1(const Node *pRoot, IntersectRequest *pRequest)
{
	const float kEpsilon = 0.1f;
	const LTVector &vP0 = *pRequest->m_pPoints[0];
	const LTVector &vP1 = *pRequest->m_pPoints[1];

	while (!(pRoot->m_Flags & (NF_IN | NF_OUT)))
	{
		const LTPlane *pPlane = pRoot->GetPlane();
		const float dot1 = pPlane->DistTo(vP0);
		const float dot2 = pPlane->DistTo(vP1);

		if (dot1 > -kEpsilon && dot2 > -kEpsilon)
		{
			pRoot = pRoot->m_Sides[FrontSide];
		}
		else if (dot1 < kEpsilon && dot2 < kEpsilon)
		{
			pRoot = pRoot->m_Sides[BackSide];
		}
		else
		{
			const int side1 = dot1 > 0.0f;

			// The side the start is on first
			if (IntersectLineNodeLT1(pRoot->m_Sides[side1], pRequest))
				return LTTRUE;

			if (side1 == FrontSide && pRoot->m_pPoly)
			{
				const float t = -dot1 / (dot2 - dot1);
				LTVector vHit;
				VEC_LERP(vHit, vP0, vP1, t);
				if (InsideConvexLT1(pRoot->m_pPoly, vHit))
				{
					pRequest->m_pNodeHit = pRoot;
					*pRequest->m_pIPos = vHit;
					return LTTRUE;
				}
			}

			pRoot = pRoot->m_Sides[!side1];
		}
	}
	return LTFALSE;
}


const Node* IntersectLine(const Node *pRoot, LTVector *pPoint1, LTVector *pPoint2, LTVector *pIPos, LTPlane *pIPlane)
{
	IntersectRequest req;

	req.m_pPoints[0] = pPoint1;
	req.m_pPoints[1] = pPoint2;
	req.m_pIPos = pIPos;

	if( IntersectLineNode( pRoot, &req ) )
	{
		*pIPlane = *req.m_pNodeHit->GetPlane();
		return req.m_pNodeHit;
	}

	return LTNULL;
}
