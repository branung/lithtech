// LithTech 1.0 model shadows

#include "precompile.h"
#include "d3d_viewparams.h"
#include "de_objects.h"
#include "d3d_draw.h"
#include "d3d_texture.h"
#include "d3d_renderstatemgr.h"
#include "d3dmeshrendobj_rigid.h"
#include "d3dmeshrendobj_skel.h"
#include "rendererconsolevars.h"
#include "tagnodes.h"
#include "fullintersectline.h"
#include "common_draw.h"
#include "lt1modelshadow.h"

#include "world_client_bsp.h"
static IWorldClientBSP *world_bsp_client;
define_holder(IWorldClientBSP, world_bsp_client);

extern int32 g_bLT1ModelShading;

// LT1's three shadow directions.
// MaxModelShadows says how many are used
static const LTVector g_LT1ShadowDirs[3] =
{
	LTVector( 0.0f, -1.0f, -1.0f),
	LTVector(-2.0f, -2.0f, -2.0f),
	LTVector( 2.0f, -2.0f, -1.0f)
};

static const float kLT1ShadowMaxDist   = 1500.0f; // No shadow past this
static const float kLT1ShadowFadeDist  = 1000.0f; // Alpha reaches 0 here
static const float kLT1ShadowMaxAlpha  = 110.0f;
static const float kLT1ShadowRayLen    = 3000.0f;
static const float kLT1ShadowMinNormY  = 0.7f;
static const float kLT1ShadowWindThresh = 0.7f; // View space signed area
static const float kLT1ShadowZRange    = 17.0f; // LT1's ShadowZRange default

#define LT1SHADOWVERTEX_FORMAT (D3DFVF_XYZ | D3DFVF_DIFFUSE)

struct CLT1ShadowVertex
{
	LTVector	m_Vec;
	uint32		m_Color;
};

// One model's world space triangle vertices, 3 per triangle
static LTVector g_LT1ShadowVerts[ABC_SHADOW_MAX_VERTS];
static CLT1ShadowVertex g_LT1ShadowOut[ABC_SHADOW_MAX_VERTS];

static bool LT1_FindGroundPlane(const LTVector& vPos, LTPlane& plane)
{
	IntersectQuery iQuery;
	IntersectInfo iInfo;

	iQuery.m_From = vPos;
	iQuery.m_To = vPos - LTVector(0.0f, kLT1ShadowRayLen, 0.0f);
	iQuery.m_Flags = 0;

	if(!i_IntersectSegment(&iQuery, &iInfo, world_bsp_client->ClientTree()))
		return false;

	plane = iInfo.m_Plane;
	// Flipped to face the ray's start, so the accepted plane faces up
	if(plane.m_Normal.Dot(vPos) - plane.m_Dist < 0.0f)
	{
		plane.m_Normal = -plane.m_Normal;
		plane.m_Dist = -plane.m_Dist;
	}
	return plane.m_Normal.y > kLT1ShadowMinNormY;
}

// Projects p along L onto the plane
static inline LTVector LT1_Project(const LTVector& p, const LTPlane& plane, const LTVector& L, float fInvNdotL)
{
	float t = (plane.m_Dist - plane.m_Normal.Dot(p)) * fInvNdotL;
	return p + L * t;
}

static void LT1_DrawModelShadow(const ViewParams& Params, ModelInstance* pInstance, uint32 nDirs, float fAlpha)
{
	Model* pModel = pInstance->GetModelDB();
	if(!pModel)
		return;

	LTPlane plane;
	if(!LT1_FindGroundPlane(pInstance->GetPos(), plane))
		return;

	DDMatrix* pTransforms = pInstance->GetRenderingTransforms();
	if(!pTransforms)
		return;

	// Gather every visible piece's triangles once, then project them per direction
	uint32 nVerts = 0;
	for(uint32 nPiece = 0; nPiece < pModel->NumPieces(); ++nPiece)
	{
		if(pInstance->IsPieceHidden(nPiece))
			continue;

		ModelPiece* pPiece = pModel->GetPiece(nPiece);
		CDIModelDrawable* pLOD = pPiece->GetLOD((uint32)0);
		if(!pLOD)
			continue;

		switch(pLOD->GetType())
		{
		case CRenderObject::eSkelMesh:
			nVerts += ((CD3DSkelMesh*)pLOD)->GetLT1ShadowVerts(pTransforms, g_LT1ShadowVerts + nVerts, ABC_SHADOW_MAX_VERTS - nVerts);
			break;
		case CRenderObject::eRigidMesh:
			nVerts += ((CD3DRigidMesh*)pLOD)->GetLT1ShadowVerts(pTransforms, g_LT1ShadowVerts + nVerts, ABC_SHADOW_MAX_VERTS - nVerts);
			break;
		default:
			break;
		}
	}
	if(nVerts < 3)
		return;
	uint32 nTris = nVerts / 3;

	uint32 nColor = ((uint32)fAlpha) << 24; // Black, alpha only

	// Depth writes are forced on for the ramp, as LT1's renderer did
	d3d_DisableTexture(0);
	StateSet ssFog(D3DRS_FOGENABLE, FALSE);
	StateSet ssShade(D3DRS_SHADEMODE, D3DSHADE_FLAT);
	StateSet ssBlend(D3DRS_ALPHABLENDENABLE, TRUE);
	StateSet ssSrc(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	StateSet ssDst(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	StateSet ssZWrite(D3DRS_ZWRITEENABLE, TRUE);
	StateSet ssLight(D3DRS_LIGHTING, FALSE);
	StateSet ssCull(D3DRS_CULLMODE, D3DCULL_NONE);
	StateSet ssAlphaTest(D3DRS_ALPHATESTENABLE, FALSE);

	g_RenderStateMgr.SetTransform(D3DTS_WORLDMATRIX(0), (const D3DMATRIX*)&Params.m_mIdentity);
	D3D_CALL(PD3DDEVICE->SetVertexShader(NULL));
	D3D_CALL(PD3DDEVICE->SetFVF(LT1SHADOWVERTEX_FORMAT));

	for(uint32 nDir = 0; nDir < nDirs; ++nDir)
	{
		LTVector L = g_LT1ShadowDirs[nDir];
		L.Normalize();
		float fNdotL = plane.m_Normal.Dot(L);
		if(fNdotL > -0.001f) // The direction must point into the plane
			continue;
		float fInvNdotL = 1.0f / fNdotL;

		LTVector vCenter = LT1_Project(pInstance->GetPos(), plane, L, fInvNdotL);
		float fRadius = pInstance->GetDims().Mag();
		bool bOutside = false;
		for(uint32 nPlane = 0; nPlane < NUM_CLIPPLANES && !bOutside; ++nPlane)
		{
			if(Params.m_ClipPlanes[nPlane].DistTo(vCenter) < -fRadius)
				bOutside = true;
		}
		if(bOutside)
			continue;

		// The first triangle sits ShadowZRange toward the camera, the last nearly on the plane
		float fZStep = kLT1ShadowZRange / (float)nTris;
		float fZBias = kLT1ShadowZRange;

		uint32 nOut = 0;
		for(uint32 t = 0; t < nTris; ++t)
		{
			LTVector p[3];
			LTVector v[3];
			for(uint32 c = 0; c < 3; ++c)
			{
				p[c] = LT1_Project(g_LT1ShadowVerts[t * 3 + c], plane, L, fInvNdotL);
				Params.m_mView.Apply(p[c], v[c]);
			}

			// Keeps one flattened side of the model and drops degenerate triangles
			float fArea = (v[1].x - v[0].x) * (v[2].y - v[0].y) - (v[2].x - v[0].x) * (v[1].y - v[0].y);
			if(fArea <= kLT1ShadowWindThresh)
			{
				fZBias -= fZStep;
				continue;
			}

			for(uint32 c = 0; c < 3; ++c)
			{
				LTVector vToCam = Params.m_Pos - p[c];
				float fLen = vToCam.Mag();
				if(fLen > 0.001f)
					p[c] += vToCam * (fZBias / fLen);
				g_LT1ShadowOut[nOut].m_Vec = p[c];
				g_LT1ShadowOut[nOut].m_Color = nColor;
				++nOut;
			}
			fZBias -= fZStep;
		}

		if(nOut >= 3)
		{
			D3D_CALL(PD3DDEVICE->DrawPrimitiveUP(D3DPT_TRIANGLELIST, nOut / 3, g_LT1ShadowOut, sizeof(CLT1ShadowVertex)));
		}
	}
}

void d3d_DrawLT1ModelShadows(const ViewParams& Params, BaseObjectSet* pSet)
{
	if(!g_bLT1ModelShading || !pSet || pSet->IsEmpty())
		return;
	if(Params.m_eRenderMode != ViewParams::eRenderMode_Normal)
		return;
	if(!g_have_world)
		return;

	int32 nDirs = g_CV_MaxModelShadows.m_Val;
	if(nDirs <= 0)
		return;
	if(nDirs > 3)
		nDirs = 3;

	for(uint32 i = 0; i < pSet->m_nObjects; ++i)
	{
		LTObject* pObject = pSet->m_pObjects[i];
		if(!pObject || pObject->m_ObjectType != OT_MODEL)
			continue;
		if(!(pObject->m_Flags & FLAG_SHADOW) || (pObject->m_Flags & FLAG_REALLYCLOSE))
			continue;

		ModelInstance* pInstance = pObject->ToModel();

		float fDist = (pInstance->GetPos() - Params.m_Pos).Mag();
		if(fDist > kLT1ShadowMaxDist)
			continue;

		float d = fDist < kLT1ShadowFadeDist ? fDist : kLT1ShadowFadeDist;
		float fAlpha = (kLT1ShadowFadeDist - d) * 0.001f * 255.0f;
		if(fAlpha > kLT1ShadowMaxAlpha)
			fAlpha = kLT1ShadowMaxAlpha;
		if(fAlpha < 1.0f)
			continue;

		LT1_DrawModelShadow(Params, pInstance, (uint32)nDirs, fAlpha);
	}

	// Leave the shader state as other immediate draws expect
	D3D_CALL(PD3DDEVICE->SetVertexShader(NULL));
}
