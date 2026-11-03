// The LT1 set's CollideWithWorld: retail Shogo's world walk (lt1_world_collide.h) on Jupiter's world.
// collision.cpp's CollideWithWorld reaches it through the era set EngineEra 1 selects.
// Off unless LT1CollideWithWorld is set on both sides, which no var table does yet:
// it replaces the walk Shogo and Blood 2 run today (Jupiter's with the LT1 walk vars), and waits for a replay against retail

#include "bdefs.h"

#include "de_world.h"
#include "collision.h"
#include "de_objects.h"
#include "impl_common.h"
#include "moveobject.h"
#include "era/era.h"
#include "era/lt1/lt1_world_collide.h"
#include "lt1_vars.h"

extern int32 g_DebugLevel;

namespace
{
	// Jupiter's world, as LT1WorldCollide reads it
	class JupiterWorld
	{
	public:
		typedef ::Node			Node;
		typedef WorldPoly		Poly;
		typedef CollideInfo		Info;

		explicit JupiterWorld(CollideRequest &request) : m_Request(request) {}

		const Node *Root() const { return m_Request.m_pWorld->GetRootNode(); }

		static bool IsLeaf(const Node *p) { return !p || p == NODE_IN || p == NODE_OUT; }
		static const Node *Side(const Node *p, int s) { return p->m_Sides[s]; }
		static const LTPlane *Plane(const Node *p) { return p->GetPlane(); }
		// Jupiter's load sets these as LT1's does: axial within 0.99999 of an
		// axis, generic otherwise, and generic for a moving copy
		static int PlaneType(const Node *p) { return p->m_PlaneType; }
		static const Poly *PolyOf(const Node *p) { return p->m_pPoly; }

		static bool Solid(const Poly *p) { return (p->GetSurface()->m_Flags & SURF_SOLID) != 0; }
		static const LTVector &Center(const Poly *p) { return p->GetCenter(); }
		static float Radius(const Poly *p) { return p->GetRadius(); }
		static uint32 NumVerts(const Poly *p) { return p->GetNumVertices(); }
		static const LTVector &Vert(const Poly *p, uint32 i) { return p->GetVertex(i); }

		void RecordHit(const Node *pNode)
		{
			CollisionInfo *pCI = m_Request.m_pCollisionInfo;
			pCI->m_Plane = *pNode->GetPlane();
			pCI->m_hObject = (HOBJECT)m_Request.m_pWorldObj;
			pCI->m_hPoly = m_Request.m_pWorld ? m_Request.m_pWorld->MakeHPoly(pNode) : INVALID_HPOLY;
		}

		void Respond(Info *pInfo, const Node *pNode) { DoObjectCollisionResponse(m_Request, pInfo, pNode); }

		const LTVector &Velocity() const { return m_Request.m_pObject->m_Velocity; }

		void Recursed()
		{
			if (g_DebugLevel > 0)
				dsi_ConsolePrint("Error: physics recursed infinitely on a %s",
					m_Request.m_pAbstract->GetObjectClassName(m_Request.m_pObject));
		}

	private:
		CollideRequest &m_Request;
	};
}

// The era set's entry (era/era.h).  A box mover only, as LT1 had nothing
// else: a cylinder (FLAG2_PLAYERCOLLIDE, Jupiter's) keeps Jupiter's walk, and
// a point mover never gets here.  LT1 stair-steps a request exactly when the
// object has FLAG_STAIRSTEP (25.1's CollideAgainstWorld).
bool era_lt1_CollideWithWorld(CollideRequest &request, CollideInfo *pInfo)
{
	if( !g_bLT1CollideWithWorld || (request.m_pObject->m_Flags2 & FLAG2_PLAYERCOLLIDE) || !request.m_pWorld )
		return false;

	JupiterWorld world(request);
	LT1WorldCollide<JupiterWorld> walk(world);
	walk.Run(request.m_OriginalPos, request.m_NewPos, request.m_Dims,
	         (request.m_pObject->m_Flags & FLAG_STAIRSTEP) != 0, request.m_bSlide != 0, pInfo);
	return true;
}
