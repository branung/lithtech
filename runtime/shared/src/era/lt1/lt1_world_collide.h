// LT1WorldCollide, LT1's box against world collision: retail Shogo 27.1's CollideWithWorld,
// with the stair pass, the 40 pass main walk and everything they call, transcribed from a source tree
// that compiles with VC6 to retail Client.exe's bytes.
//
// A template over the world it walks, so an offline test can run the same code the engine does.
// collidewithworld.cpp instantiates it on Jupiter's nodes and polygons.
//
// The world class W supplies:
//   typedef Node, Poly, Info           Info has m_nHits, m_pStandingOn (const Node *), m_VelOffset and m_FinalPos
//   const Node *Root()
//   static bool IsLeaf(const Node *)
//   static const Node *Side(const Node *, int)   0 back, 1 front
//   static const LTPlane *Plane(const Node *)    also a plane's identity
//   static int PlaneType(const Node *)           0-5 axial, 6 generic
//   static const Poly *PolyOf(const Node *)
//   static bool Solid(const Poly *)
//   static const LTVector &Center(const Poly *); static float Radius(const Poly *)
//   static uint32 NumVerts(const Poly *); static const LTVector &Vert(const Poly *, uint32)
//   void RecordHit(const Node *)          the stair walk's and the no pushback hit record: plane, object, HPOLY
//   void Respond(Info *, const Node *)    DoObjectCollisionResponse
//   const LTVector &Velocity()            the mover's
//   void Recursed()                       the 40 pass cap's console line
//
// Retail's state is globals (g_Pos, g_Offset, g_MovePoints, ...). Here it's this object, one per call,
// which is what retail's request copy and context chaining give it.
//
// What isn't the og's, said where it happens:
//  - the walk starts at the BSP's root, where retail starts at the root of the PBlock the move's start
//    falls in, a pruned copy of the tree for that cell of the world's grid. Jupiter's worlds carry no PBlocks;
//  - the node stacks are bounded (retail's 1000 WORDs aren't checked);
//  - a polygon of more than 40 vertices is taken as overlapping, the conservative answer, where retail
//    would overrun its buffer;
//  - the floating point is SSE single where retail's was x87 at 24 bit precision. Retail's summation order
//    is kept where the source records it (the stair walk's two dots, the move back's). Elsewhere the order
//    is the source's, and a result can differ in its last bit

#ifndef __ERA_LT1_WORLD_COLLIDE_H__
#define __ERA_LT1_WORLD_COLLIDE_H__

#include <math.h>

namespace lt1walk
{
	const float CLASSIFY_EPSILON = -0.001f;
	const float CLIP_EPSILON     = 0.001f;
	const int   MAX_PUSHAWAY     = 20;
	const int   MAX_ITERATIONS   = 40;
	const int   STACK_SIZE       = 1000;

	struct Sphere
	{
		float		m_Radius;
		LTVector	m_Center;
	};

	// retail's summation orders, from the matched functions' bytes
	inline float DotZYX(const LTVector &a, const LTVector &b) { return (a.z * b.z + a.y * b.y) + a.x * b.x; }
	inline float DotXZY(const LTVector &a, const LTVector &b) { return (a.x * b.x + a.z * b.z) + a.y * b.y; }
	inline float DotXYZ(const LTVector &a, const LTVector &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	inline float Comp(const LTVector &v, int i) { return i == 0 ? v.x : (i == 1 ? v.y : v.z); }
}

template <class W>
class LT1WorldCollide
{
public:
	typedef typename W::Node	Node;
	typedef typename W::Poly	Poly;
	typedef typename W::Info	Info;

	explicit LT1WorldCollide(W &w) : m_W(w), m_pInfo(0), m_nPush(0), m_fRadius(0.0f), m_bSlide(false) {}

	// CollideWithWorld,
	// after the point-collision exit, which the caller has taken.
	void Run(const LTVector &vStart, const LTVector &vEnd, const LTVector &vDims,
	         bool bStairStep, bool bSlide, Info *pInfo)
	{
		m_pInfo = pInfo;
		m_bSlide = bSlide;
		pInfo->m_pStandingOn = 0;
		pInfo->m_VelOffset.Init();
		pInfo->m_nHits = 0;

		m_nPush = 0;
		m_Pos[0] = vStart;
		m_Pos[1] = vEnd;
		m_Offset.Init();
		m_Dims = vDims;

		if (bStairStep)
		{
			// the bottom quarter of the box, P0 put back afterwards
			m_Offset.y = -(vDims.y * 0.75f);
			m_Dims = vDims;
			m_Dims.y *= 0.25f;

			const LTVector vSaveStart = m_Pos[0];
			SetupAxisAlignedBox();
			StairStepCollide2(m_W.Root());
			m_Pos[0] = vSaveStart;

			// the top three quarters for the main walk
			m_Offset.y = vDims.y * 0.25f;
			m_Dims = vDims;
			m_Dims.y *= 0.75f;
		}

		m_fRadius = m_Dims.Mag() + 0.01f;

		int i;
		for (i = 0; i < lt1walk::MAX_ITERATIONS; i++)
		{
			const uint32 nHits = pInfo->m_nHits;
			if (SetupBox())
				ClipBoxIntoTree2(m_W.Root(), i > 0);
			if (nHits == pInfo->m_nHits)
				break;
		}

		if (i == lt1walk::MAX_ITERATIONS)
		{
			m_W.Recursed();
			m_Pos[1] = m_Pos[0];
		}

		pInfo->m_FinalPos = m_Pos[1];
	}

	// the end of the move as the walk left it, for a caller that wants it
	const LTVector &GetPos(int i) const { return m_Pos[i]; }
	int GetNumPushaway() const { return m_nPush; }

private:
	struct Classify
	{
		const LTPlane			*m_pPlane;
		const LTVector			*m_pPoints;
		const lt1walk::Sphere	*m_pSphere;
		float					m_fMinDist;
		float					m_fMaxDist;
		int						*m_pbDistsValid;
		LTVector				m_Min;
		LTVector				m_Max;
	};

	struct Pushaway
	{
		LTPlane			m_Plane;
		const LTPlane	*m_pSource;
	};

	W					&m_W;
	Info				*m_pInfo;
	LTVector			m_Pos[2];
	LTVector			m_Offset;
	LTVector			m_Dims;
	float				m_fRadius;
	bool				m_bSlide;
	lt1walk::Sphere		m_Whole, m_Start, m_End;
	LTVector			m_MovePoints[16];
	float				m_MovePlaneDist[6];
	Pushaway			m_Push[lt1walk::MAX_PUSHAWAY];
	int					m_nPush;

	// 0x412dc0: the swept box around both ends, pushed out by the offset and
	// the dims, its sphere (half-diagonal + 1), then the corners
	void SetupAxisAlignedBox()
	{
		LTVector vMin, vMax;
		vMin.x = LTMIN(m_Pos[0].x, m_Pos[1].x);
		vMin.y = LTMIN(m_Pos[0].y, m_Pos[1].y);
		vMin.z = LTMIN(m_Pos[0].z, m_Pos[1].z);
		vMax.x = LTMAX(m_Pos[0].x, m_Pos[1].x);
		vMax.y = LTMAX(m_Pos[0].y, m_Pos[1].y);
		vMax.z = LTMAX(m_Pos[0].z, m_Pos[1].z);
		vMax += m_Offset + m_Dims;
		vMin += m_Offset - m_Dims;

		const LTVector vHalf = (vMax - vMin) * 0.5f;
		m_Whole.m_Center = vMin + vHalf;
		m_Whole.m_Radius = vHalf.Mag() + 1.0f;

		SetupBoxMinMax(vMin, vMax);
	}

	// 0x413040: the top ring twice, the bottom ring twice, the six distances
	void SetupBoxMinMax(const LTVector &vMin, const LTVector &vMax)
	{
		m_MovePoints[0].Init(vMin.x, vMax.y, vMin.z);
		m_MovePoints[1].Init(vMin.x, vMax.y, vMax.z);
		m_MovePoints[2].Init(vMax.x, vMax.y, vMax.z);
		m_MovePoints[3].Init(vMax.x, vMax.y, vMin.z);
		for (int i = 0; i < 4; i++)
			m_MovePoints[4 + i] = m_MovePoints[i];
		m_MovePoints[8].Init(vMin.x, vMin.y, vMin.z);
		m_MovePoints[9].Init(vMin.x, vMin.y, vMax.z);
		m_MovePoints[10].Init(vMax.x, vMin.y, vMax.z);
		m_MovePoints[11].Init(vMax.x, vMin.y, vMin.z);
		for (int i = 0; i < 4; i++)
			m_MovePoints[12 + i] = m_MovePoints[8 + i];
		SetPlaneDists(vMin, vMax);
	}

	void SetPlaneDists(const LTVector &vMin, const LTVector &vMax)
	{
		m_MovePlaneDist[0] = vMin.x;
		m_MovePlaneDist[1] = -vMax.x;
		m_MovePlaneDist[2] = vMin.y;
		m_MovePlaneDist[3] = -vMax.y;
		m_MovePlaneDist[4] = vMin.z;
		m_MovePlaneDist[5] = -vMax.z;
	}

	// 0x411fb0: the main walk's box at each end, declining a move under 1e-4
	// on every axis and writing nothing
	bool SetupBox()
	{
		const LTVector vDiff = m_Pos[1] - m_Pos[0];
		if (fabsf(vDiff.x) < 0.0001f && fabsf(vDiff.y) < 0.0001f && fabsf(vDiff.z) < 0.0001f)
			return false;

		LTVector vCenters[2];
		vCenters[0] = m_Pos[0] + m_Offset;
		vCenters[1] = m_Pos[1] + m_Offset;

		for (int i = 0; i < 2; i++)
		{
			LTVector *p = &m_MovePoints[i * 8];
			const LTVector &c = vCenters[i];
			const LTVector &d = m_Dims;
			p[0].Init(c.x - d.x, c.y + d.y, c.z - d.z);
			p[1].Init(c.x - d.x, c.y + d.y, c.z + d.z);
			p[2].Init(c.x + d.x, c.y + d.y, c.z + d.z);
			p[3].Init(c.x + d.x, c.y + d.y, c.z - d.z);
			p[4].Init(c.x - d.x, c.y - d.y, c.z - d.z);
			p[5].Init(c.x - d.x, c.y - d.y, c.z + d.z);
			p[6].Init(c.x + d.x, c.y - d.y, c.z + d.z);
			p[7].Init(c.x + d.x, c.y - d.y, c.z - d.z);
		}

		m_Start.m_Center = vCenters[0];
		m_End.m_Center = vCenters[1];
		m_Start.m_Radius = m_End.m_Radius = m_fRadius;

		m_Whole.m_Center = m_Start.m_Center + (vDiff * 0.5f);
		m_Whole.m_Radius = vDiff.Mag() + m_fRadius;

		LTVector vMin, vMax;
		vMin.x = LTMIN(vCenters[0].x, vCenters[1].x);
		vMin.y = LTMIN(vCenters[0].y, vCenters[1].y);
		vMin.z = LTMIN(vCenters[0].z, vCenters[1].z);
		vMax.x = LTMAX(vCenters[0].x, vCenters[1].x);
		vMax.y = LTMAX(vCenters[0].y, vCenters[1].y);
		vMax.z = LTMAX(vCenters[0].z, vCenters[1].z);
		vMin -= m_Dims;
		vMax += m_Dims;
		SetPlaneDists(vMin, vMax);
		return true;
	}

	// 0x4111f0: solid, near the move's sphere, and surviving a clip against
	// the box's six faces with a 0.001 inset
	bool ClipPolyIntoBox(const Poly *pPoly)
	{
		if (!W::Solid(pPoly))
			return false;

		const float fRadius = m_Whole.m_Radius + W::Radius(pPoly);
		const LTVector vToPoly = m_Whole.m_Center - W::Center(pPoly);
		if (lt1walk::DotXYZ(vToPoly, vToPoly) > fRadius * fRadius)
			return false;

		const uint32 nVerts = W::NumVerts(pPoly);
		if (nVerts > 40)
			return true;                    // see the file header

		const LTVector *pPoints[40], *pOutPoints[200];
		LTVector newVerts[200];
		const LTVector **pIn = pPoints, **pOut = pOutPoints, **pOutStart = pOutPoints;
		int nIn = (int)nVerts, nNewVerts = 0;
		for (int i = 0; i < nIn; i++)
			pPoints[i] = &W::Vert(pPoly, (uint32)i);

		static const float s_MulTable[6] = { 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f };
		for (int i = 0; i < 6; i++)
		{
			const float fMul = s_MulTable[i];
			const int axis = i >> 1;
			int iPrev = nIn - 1;
			float fPrevDist = fMul * lt1walk::Comp(*pIn[iPrev], axis) - m_MovePlaneDist[i];
			bool bPrevInside = fPrevDist > lt1walk::CLIP_EPSILON;

			for (int j = 0; j < nIn; j++)
			{
				const float fCurDist = fMul * lt1walk::Comp(*pIn[j], axis) - m_MovePlaneDist[i];
				const bool bCurInside = fCurDist > lt1walk::CLIP_EPSILON;

				if (bPrevInside && pOut < pOutPoints + 200)
					*pOut++ = pIn[iPrev];

				if (bPrevInside != bCurInside && pOut < pOutPoints + 200 && nNewVerts < 200)
				{
					const float t = fPrevDist / (fPrevDist - fCurDist);
					LTVector &v = newVerts[nNewVerts];
					v.x = (pIn[j]->x - pIn[iPrev]->x) * t + pIn[iPrev]->x;
					v.y = (pIn[j]->y - pIn[iPrev]->y) * t + pIn[iPrev]->y;
					v.z = (pIn[j]->z - pIn[iPrev]->z) * t + pIn[iPrev]->z;
					*pOut++ = &newVerts[nNewVerts];
					nNewVerts++;
				}

				fPrevDist = fCurDist;
				bPrevInside = bCurInside;
				iPrev = j;
			}

			nIn = (int)(pOut - pOutStart);
			pIn = pOutStart;
			pOutStart = pOut;
			if (nIn == 0)
				return false;
		}
		return true;
	}

	// 0x413d30: the stair walk.  A hit takes the front
	// child only; anything else stacks the back and takes the front.
	void StairStepCollide2(const Node *pNode)
	{
		const Node *stack[lt1walk::STACK_SIZE];
		int nStack = 0;
		for (;;)
		{
			while (W::IsLeaf(pNode))
			{
				if (nStack == 0)
					return;
				pNode = stack[--nStack];
			}

			const LTPlane *pPlane = W::Plane(pNode);
			// retail sums this dot z + y + x
			const float fDist = lt1walk::DotZYX(pPlane->m_Normal, m_Whole.m_Center) - pPlane->m_Dist;
			if (fDist > m_Whole.m_Radius)
			{
				pNode = W::Side(pNode, 1);
				continue;
			}
			if (fDist < -m_Whole.m_Radius)
			{
				pNode = W::Side(pNode, 0);
				continue;
			}

			const Poly *pPoly = W::PolyOf(pNode);
			if (pPoly && pPlane->m_Normal.y > 0.3f)
			{
				const LTVector vVel = m_W.Velocity() + m_pInfo->m_VelOffset;
				if (lt1walk::DotXYZ(vVel, pPlane->m_Normal) <= 0.0f && ClipPolyIntoBox(pPoly))
				{
					m_pInfo->m_nHits++;
					m_W.RecordHit(pNode);

					// the deepest of the four bottom corners, 500 if none is lower
					float fMinDist = 500.0f;
					for (int i = 0; i < 4; i++)
					{
						// retail sums this dot x + z + y
						const float d = lt1walk::DotXZY(pPlane->m_Normal, m_MovePoints[8 + i]) - pPlane->m_Dist;
						if (d < fMinDist)
							fMinDist = d;
					}

					if (fMinDist < 0.0f)
					{
						if (pPlane->m_Normal.y > 0.9f)
							m_W.Respond(m_pInfo, pNode);

						const LTVector vPush = pPlane->m_Normal * -fMinDist;   // no fudge
						m_Pos[0] += vPush;
						m_Pos[1] += vPush;
						SetupAxisAlignedBox();
						m_pInfo->m_pStandingOn = pNode;
					}

					pNode = W::Side(pNode, 1);
					continue;
				}
			}

			if (nStack < lt1walk::STACK_SIZE)
				stack[nStack++] = W::Side(pNode, 0);
			pNode = W::Side(pNode, 1);
		}
	}

	// 0x411c60 and its axial neighbors; 0 behind, 1 in front, 2 straddling
	int ReallyClassifyGeneric(Classify &cp)
	{
		cp.m_fMinDist = 1e37f;
		cp.m_fMaxDist = -1e37f;
		for (int i = 0; i < 8; i++)
		{
			const float d = lt1walk::DotXYZ(cp.m_pPlane->m_Normal, cp.m_pPoints[i]) - cp.m_pPlane->m_Dist;
			if (d < cp.m_fMinDist) cp.m_fMinDist = d;
			if (d > cp.m_fMaxDist) cp.m_fMaxDist = d;
		}
		if (cp.m_fMinDist < lt1walk::CLASSIFY_EPSILON && cp.m_fMaxDist > lt1walk::CLASSIFY_EPSILON)
			return 2;
		return cp.m_fMinDist > lt1walk::CLASSIFY_EPSILON;
	}

	// 0x411d00 and 0x411d70-0x411f50.  The axial ones read the box captured
	// when ClipBoxIntoTree2 was entered, which a push inside that call does
	// not refresh. That's retail's, and the LT1.5 walk has the same staleness.
	int ClassifyNode(int iType, Classify &cp)
	{
		if (iType >= 6)
		{
			const float d = lt1walk::DotXYZ(cp.m_pPlane->m_Normal, cp.m_pSphere->m_Center) - cp.m_pPlane->m_Dist;
			if (d > cp.m_pSphere->m_Radius) { *cp.m_pbDistsValid = 0; return 1; }
			if (d < -cp.m_pSphere->m_Radius) { *cp.m_pbDistsValid = 0; return 0; }
			return ReallyClassifyGeneric(cp);
		}
		const int axis = iType >> 1;
		const float lo = lt1walk::Comp(cp.m_Min, axis), hi = lt1walk::Comp(cp.m_Max, axis);
		if (!(iType & 1)) { cp.m_fMinDist = lo - cp.m_pPlane->m_Dist;  cp.m_fMaxDist = hi - cp.m_pPlane->m_Dist; }
		else              { cp.m_fMinDist = -hi - cp.m_pPlane->m_Dist; cp.m_fMaxDist = -lo - cp.m_pPlane->m_Dist; }
		if (cp.m_fMinDist <= lt1walk::CLASSIFY_EPSILON && cp.m_fMaxDist > lt1walk::CLASSIFY_EPSILON)
			return 2;
		return cp.m_fMinDist > lt1walk::CLASSIFY_EPSILON;
	}

	// 0x413b20: every plane pushed off so far, against the box at the new end
	void FitIntoPushawayPlanes()
	{
		const LTVector c(m_Pos[1].x + m_Offset.x, m_Pos[1].y + m_Offset.y, m_Pos[1].z + m_Offset.z);
		const LTVector &d = m_Dims;
		LTVector p[8];
		p[0].Init(c.x + d.x, c.y + d.y, c.z + d.z);
		p[1].Init(c.x - d.x, c.y + d.y, c.z + d.z);
		p[2].Init(c.x - d.x, c.y + d.y, c.z - d.z);
		p[3].Init(c.x + d.x, c.y + d.y, c.z - d.z);
		p[4].Init(c.x + d.x, c.y - d.y, c.z + d.z);
		p[5].Init(c.x - d.x, c.y - d.y, c.z + d.z);
		p[6].Init(c.x - d.x, c.y - d.y, c.z - d.z);
		p[7].Init(c.x + d.x, c.y - d.y, c.z - d.z);

		for (int i = 0, nIterations = 0; i < m_nPush && nIterations < 10; i++)
		{
			const LTPlane &plane = m_Push[i].m_Plane;
			float fMinDist = 1e37f;
			for (int j = 0; j < 8; j++)
			{
				const float dd = lt1walk::DotXYZ(plane.m_Normal, p[j]) - plane.m_Dist;
				if (dd < fMinDist)
					fMinDist = dd;
			}
			if (fMinDist < 0.09f)
			{
				const LTVector vPush = plane.m_Normal * (0.1f - fMinDist);
				m_Pos[1] += vPush;
				for (int j = 0; j < 8; j++)
					p[j] += vPush;
				i = -1;
				nIterations++;
			}
		}
	}

	void RememberPlane(const LTPlane *pPlane)
	{
		if (m_nPush >= lt1walk::MAX_PUSHAWAY)
			return;
		for (int i = 0; i < m_nPush; i++)
			if (m_Push[i].m_pSource == pPlane)
				return;
		m_Push[m_nPush].m_Plane = *pPlane;
		m_Push[m_nPush].m_pSource = pPlane;
		m_nPush++;
		if (m_nPush > 1)
			FitIntoPushawayPlanes();
	}

	// AddAndPushAway: P1 out along the normal until the box clears by 0.1
	void AddAndPushAway(const LTPlane *pPlane, float fDist)
	{
		m_Pos[1] += pPlane->m_Normal * (0.1f + fDist);
		RememberPlane(pPlane);
	}

	// AddAndMoveBack: P1 back along the move until the box just touches
	void AddAndMoveBack(const LTPlane *pPlane, float fDist)
	{
		const LTVector vMove = m_Pos[1] - m_Pos[0];
		// retail sums this dot z + y + x
		const float t = fDist - lt1walk::DotZYX(pPlane->m_Normal, vMove);
		const float s = -t / (fDist - t);
		m_Pos[1].x = s * vMove.x;
		m_Pos[1].y = s * vMove.y;
		m_Pos[1].z = s * vMove.z;
		m_Pos[1] += m_Pos[0];
		RememberPlane(pPlane);
	}

	// The box went from in front of the plane to straddling it
	bool MoveToFrontside(const Node *pNode, Classify *cps)
	{
		const LTPlane *pPlane = W::Plane(pNode);
		if (pPlane->m_Normal.y > 0.01f &&
			(!m_pInfo->m_pStandingOn || W::Plane(m_pInfo->m_pStandingOn)->m_Normal.y < pPlane->m_Normal.y))
		{
			m_pInfo->m_pStandingOn = pNode;
		}

		m_W.Respond(m_pInfo, pNode);
		m_pInfo->m_nHits++;

		if (!*cps[0].m_pbDistsValid)
			ReallyClassifyGeneric(cps[1]);

		if (m_bSlide)
			AddAndPushAway(pPlane, -cps[1].m_fMinDist);
		else
			AddAndMoveBack(pPlane, cps[1].m_fMinDist);

		return SetupBox();
	}

	// 0x413270: the main walk
	bool ClipBoxIntoTree2(const Node *pNode, bool bPushback)
	{
		int bDistsValid = 1;
		Classify cps[2];
		cps[0].m_pPoints = &m_MovePoints[0];
		cps[1].m_pPoints = &m_MovePoints[8];
		cps[0].m_pbDistsValid = cps[1].m_pbDistsValid = &bDistsValid;
		cps[0].m_pSphere = &m_Start;
		cps[1].m_pSphere = &m_End;
		const LTVector vS = m_Pos[0] + m_Offset, vE = m_Pos[1] + m_Offset;
		cps[0].m_Min = vS - m_Dims; cps[0].m_Max = vS + m_Dims;
		cps[1].m_Min = vE - m_Dims; cps[1].m_Max = vE + m_Dims;

		const Node *stack[lt1walk::STACK_SIZE];
		int nStack = 0;
		for (;;)
		{
			while (W::IsLeaf(pNode))
			{
				if (nStack == 0)
					return true;
				pNode = stack[--nStack];
			}

			const LTPlane *pPlane = W::Plane(pNode);
			const float fDist = lt1walk::DotXYZ(pPlane->m_Normal, m_Whole.m_Center) - pPlane->m_Dist;
			if (fDist > m_Whole.m_Radius)
			{
				pNode = W::Side(pNode, 1);
				continue;
			}
			if (fDist < -m_Whole.m_Radius)
			{
				pNode = W::Side(pNode, 0);
				continue;
			}

			bDistsValid = 1;
			cps[0].m_pPlane = cps[1].m_pPlane = pPlane;
			const int iType = W::PlaneType(pNode);
			const int iStart = ClassifyNode(iType, cps[0]);
			const int iEnd = ClassifyNode(iType, cps[1]);
			const Poly *pPoly = W::PolyOf(pNode);

			if (iStart == iEnd)
			{
				if (iStart == 2)
				{
					if (pPoly && ClipPolyIntoBox(pPoly))
					{
						if (bPushback)
						{
							// both ends straddle: back against the move and out
							// along the normal until the box clears
							LTVector vBack = -(m_Pos[1] - m_Pos[0]);
							vBack.Norm();
							vBack += pPlane->m_Normal;

							m_W.Respond(m_pInfo, pNode);
							m_pInfo->m_nHits++;

							int i;
							for (i = 0; i < 50; i++)
							{
								if (!ClipPolyIntoBox(pPoly))
									break;
								m_Pos[1] += vBack;
								if (!SetupBox())
									return false;
							}
							if (i == 50)
							{
								m_Pos[1] = m_Pos[0];
								return false;
							}
						}
						else
						{
							m_W.RecordHit(pNode);
							m_pInfo->m_nHits++;
						}
					}

					const int s = (lt1walk::DotXYZ(pPlane->m_Normal, m_Pos[0]) - pPlane->m_Dist) > -0.001f;
					if (nStack < lt1walk::STACK_SIZE)
						stack[nStack++] = W::Side(pNode, !s);
					pNode = W::Side(pNode, s);
				}
				else
				{
					pNode = W::Side(pNode, iStart);
				}
			}
			else
			{
				if (iStart == 1 && pPoly && ClipPolyIntoBox(pPoly))
				{
					if (!MoveToFrontside(pNode, cps))
						return false;
					pNode = W::Side(pNode, 1);
				}
				else
				{
					const int s = (lt1walk::DotXYZ(pPlane->m_Normal, m_Pos[0]) - pPlane->m_Dist) > -0.001f;
					if (nStack < lt1walk::STACK_SIZE)
						stack[nStack++] = W::Side(pNode, !s);
					pNode = W::Side(pNode, s);
				}
			}
		}
	}
};

#endif  // __ERA_LT1_WORLD_COLLIDE_H__
