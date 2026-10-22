// Sky shadow pan render shader implementation.

#include "precompile.h"

#include "d3d_rendershader_skypan.h"
#include "d3d_device.h"
#include "d3d_draw.h"
#include "common_draw.h"
#include "d3d_texture.h"
#include "common_stuff.h"
#include "renderstruct.h"

#include "memstats_world.h"
#include "rendererconsolevars.h"

#include <vector>
#include <math.h>

bool CRenderShader_SkyPan::s_bValidateRequired = true;
bool CRenderShader_SkyPan::s_bValidateResult = false;

bool CRenderShader_SkyPan::HasPanTexture()
{
	return g_CV_SkyPan &&
		   (g_pSceneDesc != NULL) &&
		   (g_pSceneDesc->m_pGlobalPanTexture != NULL) &&
		   (g_pSceneDesc->m_pGlobalPanTexture->m_pRenderData != NULL);
}

bool CRenderShader_SkyPan::ValidateShader(const CRBSection &cSection)
{
	if (!s_bValidateRequired)
		return s_bValidateResult;

	if (!PD3DDEVICE)
	{
		ASSERT(!"Device not yet initialized");
		return false;
	}

	StageStateSet tss0(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	StageStateSet tss1(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	StageStateSet tss2(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	StageStateSet tss3(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
	StageStateSet tss4(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	StageStateSet tss5(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	StageStateSet tss6(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	StageStateSet tss7(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	uint32 nNumPasses;
	HRESULT hr = PD3DDEVICE->ValidateDevice((DWORD*)&nNumPasses);

	s_bValidateRequired = false;
	s_bValidateResult = SUCCEEDED(hr);

	return s_bValidateResult;
}

void CRenderShader_SkyPan::TranslateVertices(SVertex_SkyPan *pOut, const SRBVertex *pIn, uint32 nCount)
{
	SVertex_SkyPan *pCurOut = pOut;
	SVertex_SkyPan *pEndOut = &pCurOut[nCount];
	const SRBVertex *pCurIn = pIn;
	while (pCurOut != pEndOut)
	{
		pCurOut->m_vPos = pCurIn->m_vPos;
		pCurOut->m_nColor = pCurIn->m_nColor;

		// World X and Z, which PreFlush's texture transform turns into UVs
		pCurOut->m_fU = pCurIn->m_fU0;
		pCurOut->m_fV = pCurIn->m_fV0;
		pCurOut->m_fW = 1.0f;
		++pCurOut;
		++pCurIn;
	}
}

void CRenderShader_SkyPan::DrawNormal(const DrawState &cState, uint32 nRenderBlock)
{
	// Nothing to draw without a cloud map
	if (!HasPanTexture())
		return;

	// A block with dynamic lights is lit per vertex, as LT1 lit a sky pan polygon
	if (cState.m_nNumLights == 0)
	{
		QueueRenderBlock(nRenderBlock);
		return;
	}

	PD3DDEVICE->SetVertexShader(NULL);
	PD3DDEVICE->SetFVF(k_SVertex_SkyPan_FVF);

	PreFlush();

	// The lights in the block's space, with LT1's color term per channel
	struct SPanLight { LTVector m_vPos; float m_fRadius, m_fRadiusSqr, m_fInvRadius; float m_fR, m_fG, m_fB; };
	static std::vector<SPanLight> s_aLights;
	s_aLights.clear();
	for (uint32 nLight = 0; nLight < cState.m_nNumLights; ++nLight)
	{
		const DynamicLight *pLight = cState.m_pLightList[nLight];
		if (pLight->m_LightRadius <= 0.0f)
			continue;
		SPanLight sLight;
		MatVMul_H(&sLight.m_vPos, &cState.m_pParams->m_mInvWorld, &pLight->GetPos());
		sLight.m_fRadius = pLight->m_LightRadius;
		sLight.m_fRadiusSqr = sLight.m_fRadius * sLight.m_fRadius;
		sLight.m_fInvRadius = 1.0f / sLight.m_fRadius;
		sLight.m_fR = 2.0f * (float)pLight->m_ColorR - 255.0f;
		sLight.m_fG = 2.0f * (float)pLight->m_ColorG - 255.0f;
		sLight.m_fB = 2.0f * (float)pLight->m_ColorB - 255.0f;
		s_aLights.push_back(sLight);
	}

	static std::vector<SVertex_SkyPan> s_aVerts;
	static std::vector<uint16> s_aIndices;

	CRenderBlockData &cRB = m_aRenderBlocks[nRenderBlock];
	CRenderBlockData::TSectionIndexList::iterator iCurSection = cRB.m_aSections.begin();
	for (; iCurSection != cRB.m_aSections.end(); ++iCurSection)
	{
		CInternalSection &cSection = m_aSections[*iCurSection];
		if (!cSection.m_pOriginalVertices || !cSection.m_pOriginalIndices || !cSection.m_nTriCount)
			continue;

		// The section's vertices in the block's source array, which its original indices address
		const int32 nSrcStart = (int32)cSection.m_nStartVertex - cSection.m_nOriginalVertexOffset;
		const SRBVertex *pSrc = &cSection.m_pOriginalVertices[nSrcStart];

		s_aVerts.resize(cSection.m_nVertexCount);
		TranslateVertices(&s_aVerts[0], pSrc, cSection.m_nVertexCount);

		for (uint32 nVert = 0; nVert < cSection.m_nVertexCount; ++nVert)
		{
			SVertex_SkyPan &v = s_aVerts[nVert];
			int32 nR = (v.m_nColor >> 16) & 0xff;
			int32 nG = (v.m_nColor >> 8) & 0xff;
			int32 nB = v.m_nColor & 0xff;
			for (size_t nLight = 0; nLight < s_aLights.size(); ++nLight)
			{
				const SPanLight &sLight = s_aLights[nLight];
				const float fDistSqr = (v.m_vPos - sLight.m_vPos).MagSqr();
				if (fDistSqr >= sLight.m_fRadiusSqr)
					continue;
				const float fScale = 1.0f - sqrtf(fDistSqr) * sLight.m_fInvRadius;
				nR = LTCLAMP(nR + (int32)(fScale * sLight.m_fR), 0, 255);
				nG = LTCLAMP(nG + (int32)(fScale * sLight.m_fG), 0, 255);
				nB = LTCLAMP(nB + (int32)(fScale * sLight.m_fB), 0, 255);
			}
			v.m_nColor = (v.m_nColor & 0xff000000) | ((uint32)nR << 16) | ((uint32)nG << 8) | (uint32)nB;
		}

		const uint32 nIndexCount = cSection.m_nTriCount * 3;
		s_aIndices.resize(nIndexCount);
		for (uint32 nIndex = 0; nIndex < nIndexCount; ++nIndex)
			s_aIndices[nIndex] = (uint16)((int32)cSection.m_pOriginalIndices[nIndex] - nSrcStart);

		IncFrameStat(eFS_ShaderTris, cSection.m_nTriCount);
		IncFrameStat(eFS_ShaderVerts, cSection.m_nVertexCount);
		IncFrameStat(eFS_ShaderDrawPrim, 1);

		PD3DDEVICE->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, cSection.m_nVertexCount,
			cSection.m_nTriCount, &s_aIndices[0], D3DFMT_INDEX16,
			&s_aVerts[0], sizeof(SVertex_SkyPan));

		IncFrameStat(eFS_WorldTriangles, cSection.m_nTriCount);
	}

	PostFlush();
}

void CRenderShader_SkyPan::PreFlush()
{
	PD3DDEVICE->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	PD3DDEVICE->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	PD3DDEVICE->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	PD3DDEVICE->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	PD3DDEVICE->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
	PD3DDEVICE->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	PD3DDEVICE->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	PD3DDEVICE->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	PD3DDEVICE->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

	SharedTexture *pPan = g_pSceneDesc->m_pGlobalPanTexture;
	d3d_SetTexture(pPan, 0, eFS_WorldBaseTexMemory);

	// LT1's UVs from the world position
	RTexture *pRTexture = (RTexture *)pPan->m_pRenderData;

	const float fTexW = (float)pRTexture->GetBaseWidth();
	const float fTexH = (float)pRTexture->GetBaseHeight();

	// Zero only for a texture that failed to load as SetGlobalPanInfo rejects a zero scale
	const float fDivU = fTexW * g_pSceneDesc->m_fGlobalPanScaleX;
	const float fDivV = fTexH * g_pSceneDesc->m_fGlobalPanScaleZ;

	const float fRateU = (fabsf(fDivU) > 0.0001f) ? (1.0f / fDivU) : 0.0f;
	const float fRateV = (fabsf(fDivV) > 0.0001f) ? (1.0f / fDivV) : 0.0f;

	D3DMATRIX mat;
	memset(&mat, 0, sizeof(mat));
	mat._11 = fRateU;
	mat._22 = fRateV;
	mat._31 = g_pSceneDesc->m_fGlobalPanOffsetX * fRateU;
	mat._32 = g_pSceneDesc->m_fGlobalPanOffsetZ * fRateV;
	mat._44 = 1.0f;

	PD3DDEVICE->SetTransform(D3DTS_TEXTURE0, &mat);
	PD3DDEVICE->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
}

void CRenderShader_SkyPan::PostFlush()
{
	// Every other world shader expects stage 0 without a texture transform
	PD3DDEVICE->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

	PD3DDEVICE->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	PD3DDEVICE->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

	d3d_DisableTexture(0);
}

void CRenderShader_SkyPan::DebugTri(
	uint32 nRenderBlock,
	const CRBSection &cSection,
	uint32 nTriIndex,
	const uint16 *aIndices,
	const SRBVertex *aVertices,
	float fX, float fY,
	float fSizeX, float fSizeY)
{
	// Nothing useful to draw
}

void CRenderShader_SkyPan::GetMemStats(CMemStats_World &cMemStats) const
{
	cMemStats.m_nVertexCount += m_nTotalVertexCount;
	cMemStats.m_nVertexData += m_nTotalVertexCount * sizeof(SVertex_SkyPan);
	cMemStats.m_nTriangleCount += m_nTotalTriCount;
	cMemStats.m_nTriangleData += m_nTotalTriCount * 3 * sizeof(uint16);
}
