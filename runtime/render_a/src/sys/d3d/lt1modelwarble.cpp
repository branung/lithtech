// LT1's model warble, from retail Shogo 27.1's d3d.ren and Shogo Linux's glx.ren.
//
// While ModelWarble is set (the games' shells set it when the camera is in liquid), LT1 scales every
// model vertex's node space position by
//     k = T[idx] + D[idx] * Between,   idx = ((ulong)Offset + n) & 31
// where n counts the model's vertices in LT1's order and the 32 entry table swings between WarbleScale
// and 1. The scale comes before the node's matrix, so a vertex moves toward or away from its node's
// origin, or for a deformation node the animation's translate. Only the position changes: normals, UVs,
// lighting and shadows are untouched.
//
// The converted meshes hold bind positions, and k scales an affine map, so the same displacement is
// O + k * (P - O) with O the node's bind origin. The converter can't keep LT1's vertex order, so it
// writes each vertex's LT1 phase and each bone set's origin. A mesh without that block is never touched,
// and with ModelWarble at 0 nothing here runs

#include "precompile.h"
#include "lt1modelwarble.h"
#include "d3dmeshrendobj_skel.h"
#include "rendererconsolevars.h"
#include <math.h>

// "LT1W", written by model_abc.cpp
#define LT1WARBLE_MAGIC		0x5731544Cu

namespace
{
	// LT1's two tables, and the scale they were built for
	float	s_T[32];
	float	s_D[32];
	float	s_fTableScale = 0.0f;
	bool	s_bTable = false;

	// LT1's clock. Nothing in this renderer reinits model drawing, so it starts at zero once
	float	s_fOffset = 0.0f;
	float	s_fBetween = 0.0f;

	// Which scene the clock is in, so a mesh's copy is rebuilt once per scene.
	uint32	s_iScene = 1;

	void BuildTable(float fScale)
	{
		float fClamped = fScale < 0.0f ? 0.0f : (fScale > 1.0f ? 1.0f : fScale);
		float a = (1.0f - fClamped) * 0.5f;
		for (uint32 i = 0; i < 32; ++i)
		{
			float s = (float)sin((float)i * 0.03125f * 6.2831855f);
			s_T[i] = fScale + a + s * a;
		}
		for (uint32 i = 0; i < 32; ++i)
			s_D[i] = s_T[(i + 1) & 31] - s_T[i];

		s_fTableScale = fScale;
		s_bTable = true;
	}
}

void lt1warble_Tick(float fFrameTime)
{
	// Runs whether or not ModelWarble is set: the clock keeps time while the warble is off,
	// so it resumes at a phase that depends on how long it was off, as LT1's does
	if (!s_bTable || g_CV_WarbleScale.m_Val != s_fTableScale)
		BuildTable(g_CV_WarbleScale.m_Val);

	s_fOffset += g_CV_WarbleSpeed.m_Val * fFrameTime;
	s_fBetween = s_fOffset - (float)floor(s_fOffset);
	++s_iScene;
}

bool lt1warble_On()
{
	return g_CV_ModelWarble.m_Val != 0;
}

float lt1warble_K(uint32 nPhase)
{
	if (!s_bTable)
		BuildTable(g_CV_WarbleScale.m_Val);

	// LT1 truncates the offset
	uint32 idx = ((uint32)(int32)s_fOffset + nPhase) & 31;
	return s_T[idx] + s_D[idx] * s_fBetween;
}

CLT1WarbleData::CLT1WarbleData() :
	m_pPhase(NULL), m_nPhase(0), m_pOrigin(NULL), m_nOrigin(0), m_pSetOf(NULL),
	m_bVB(false), m_iScene(0)
{
	m_VB.Reset();
}

void CLT1WarbleData::Free()
{
	FreeDeviceObjects();
	delete[] m_pPhase;	m_pPhase = NULL;	m_nPhase = 0;
	delete[] m_pOrigin;	m_pOrigin = NULL;	m_nOrigin = 0;
	delete[] m_pSetOf;	m_pSetOf = NULL;
}

bool CLT1WarbleData::LoadOptional(ILTStream& File, uint32 iObjEnd)
{
	uint32 iPos = 0;
	File.GetPos(&iPos);
	if (iPos + 8 > iObjEnd)
		return false;

	uint32 nMagic = 0;
	File.Read(&nMagic, sizeof(nMagic));
	if (nMagic != LT1WARBLE_MAGIC)
	{
		File.SeekTo(iPos);
		return false;
	}

	Free();

	uint32 nPhase = 0;
	File.Read(&nPhase, sizeof(nPhase));
	if (nPhase == 0 || nPhase > 65535)
		return false;
	LT_MEM_TRACK_ALLOC(m_pPhase = new uint8[nPhase], LT_MEM_TYPE_RENDERER);
	if (!m_pPhase)
		return false;
	File.Read(m_pPhase, nPhase);
	m_nPhase = nPhase;

	uint32 nOrigin = 0;
	File.Read(&nOrigin, sizeof(nOrigin));
	if (nOrigin == 0 || nOrigin > 65535)
	{
		Free();
		return false;
	}
	LT_MEM_TRACK_ALLOC(m_pOrigin = new LTVector[nOrigin], LT_MEM_TYPE_RENDERER);
	if (!m_pOrigin)
	{
		Free();
		return false;
	}
	for (uint32 i = 0; i < nOrigin; ++i)
		File.Read(&m_pOrigin[i], sizeof(float) * 3);
	m_nOrigin = nOrigin;
	return true;
}

bool CLT1WarbleData::BindSets(const BoneSetListItem* pSets, uint32 nSets, uint32 nVerts)
{
	if (!Has())
		return false;

	// A block that doesn't line up wasn't written for this mesh, so drop it rather than scale about the wrong points
	if (nVerts != m_nPhase || (pSets ? nSets != m_nOrigin : m_nOrigin != 1))
	{
		Free();
		return false;
	}

	LT_MEM_TRACK_ALLOC(m_pSetOf = new uint8[nVerts], LT_MEM_TYPE_RENDERER);
	if (!m_pSetOf)
	{
		Free();
		return false;
	}
	memset(m_pSetOf, 0, nVerts);
	for (uint32 s = 0; pSets && s < nSets; ++s)
	{
		uint32 iEnd = (uint32)pSets[s].iFirstVertIndex + pSets[s].iVertCount;
		for (uint32 v = pSets[s].iFirstVertIndex; v < iEnd && v < nVerts; ++v)
			m_pSetOf[v] = (uint8)s;
	}
	return true;
}

VertexBufferController* CLT1WarbleData::GetCopy(const uint8* pSrcVerts, uint32 nVerts, uint32 iStreamFlags,
												VERTEX_BLEND_TYPE eVertType, bool bSWVertProc,
												const uint8* pIndexData, uint32 nIndices)
{
	if (!m_pSetOf || !pSrcVerts || !pIndexData || nVerts != m_nPhase)
		return NULL;

	if (!m_bVB)
	{
		// Dynamic, so the per scene rewrite can discard rather than wait on the GPU.
		// The indices never change, so they're written once
		if (!m_VB.CreateStream(0, nVerts, iStreamFlags, eVertType, true, true, bSWVertProc) ||
			!m_VB.CreateIndexBuffer(nIndices, false, true, bSWVertProc))
		{
			FreeDeviceObjects();
			return NULL;
		}
		m_VB.Lock(VertexBufferController::eINDEX, false);
		memcpy(m_VB.getIndexData(), pIndexData, sizeof(uint16) * nIndices);
		m_VB.UnLock(VertexBufferController::eINDEX);
		m_bVB = true;
		m_iScene = 0;
	}

	if (m_iScene != s_iScene)
	{
		if (!m_VB.Lock(VertexBufferController::eVERTSTREAM0, true))
			return NULL;

		// The whole vertex first, for the fields this doesn't change, then each position from the source,
		// since a write only buffer can't be read back
		const uint32 iVertSize = m_VB.getVertexSize(0);
		uint8* pDst = (uint8*)m_VB.getVertexData(0);
		memcpy(pDst, pSrcVerts, iVertSize * nVerts);
		for (uint32 v = 0; v < nVerts; ++v)
		{
			const float* pIn = (const float*)(pSrcVerts + v * iVertSize);
			float* pOut = (float*)(pDst + v * iVertSize);
			const LTVector& o = m_pOrigin[m_pSetOf[v]];
			const float k = lt1warble_K(m_pPhase[v]);
			pOut[0] = o.x + k * (pIn[0] - o.x);
			pOut[1] = o.y + k * (pIn[1] - o.y);
			pOut[2] = o.z + k * (pIn[2] - o.z);
		}

		m_VB.UnLock(VertexBufferController::eVERTSTREAM0);
		m_iScene = s_iScene;
	}

	return &m_VB;
}

bool CLT1WarbleData::GetVAOrigin(uint32 iPrevAnim, uint32 iCurAnim, float fPercent, LTVector& vOut) const
{
	if (!Has() || iPrevAnim >= m_nOrigin || iCurAnim >= m_nOrigin)
		return false;

	const LTVector& a = m_pOrigin[iPrevAnim];
	const LTVector& b = m_pOrigin[iCurAnim];
	vOut = a + (b - a) * fPercent;
	return true;
}

void CLT1WarbleData::ApplyVA(uint8* pVerts, uint32 iVertSize, uint32 nVerts, const LTVector& o) const
{
	for (uint32 v = 0; v < nVerts && v < m_nPhase; ++v)
	{
		float* p = (float*)(pVerts + v * iVertSize);
		const float k = lt1warble_K(m_pPhase[v]);
		p[0] = o.x + k * (p[0] - o.x);
		p[1] = o.y + k * (p[1] - o.y);
		p[2] = o.z + k * (p[2] - o.z);
	}
}
