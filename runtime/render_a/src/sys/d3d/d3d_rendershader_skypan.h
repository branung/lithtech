#ifndef __D3D_RENDERSHADER_SKYPAN_H__
#define __D3D_RENDERSHADER_SKYPAN_H__

#include "d3d_rendershader_base.h"
#include "d3d_texture.h"

class TextureFormat;
class RTexture;

struct SVertex_SkyPan
{
	LTVector m_vPos;

	uint32 m_nColor;
	float m_fU, m_fV, m_fW;
};

struct CSection_SkyPan
{
	bool operator<(const CSection_SkyPan &cOther) const { return false; }
	bool operator!=(const CSection_SkyPan &cOther) const { return false; }
	bool operator==(const CSection_SkyPan &cOther) const { return true; }

	void SetTexture(SharedTexture *pTexture) { }
	SharedTexture *GetTexture() { return 0; }

	bool ShouldAlphaTest() const { return false; }
	bool ShouldGlow() const { return false; }
};

enum { k_SVertex_SkyPan_FVF =
	D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE3(0) };

class CRenderShader_SkyPan :
	public CRenderShader_Base<SVertex_SkyPan, CSection_SkyPan, k_SVertex_SkyPan_FVF>
{
public:

	~CRenderShader_SkyPan() { s_bValidateRequired = true; }

	virtual bool ValidateShader(const CRBSection &cSection);

	virtual void DebugTri(
		uint32 nRenderBlock,
		const CRBSection &cSection,
		uint32 nTriIndex,
		const uint16 *aIndices,
		const SRBVertex *aVertices,
		float fX, float fY,
		float fSizeX, float fSizeY);

	virtual void GetMemStats(CMemStats_World &cMemStats) const;

	virtual ERenderShader GetShaderID() const { return eShader_SkyPan; }

protected:

	virtual void DrawNormal(const DrawState &cState, uint32 nRenderBlock);

	virtual void DrawGlow(uint32 nRenderBlock) { /* Don't glow */ }

	virtual void PreFlush();
	virtual void FlushChangeSection(CInternalSection &cSection) { /* one global texture */ }
	virtual void PreFlushBlock(CInternalSection &cSection) { }
	virtual void PostFlushBlock(CInternalSection &cSection,
		uint32 nStartIndex, uint32 nEndIndex,
		uint32 nStartVertex, uint32 nEndVertex) { }
	virtual void PostFlush();

	virtual void TranslateVertices(SVertex_SkyPan *pOut, const SRBVertex *pIn, uint32 nCount);

	virtual void FillSection(CSection_SkyPan &cInternalSection, const CRBSection &cSection) { }

private:

	static bool HasPanTexture();

	static bool s_bValidateRequired, s_bValidateResult;
};

#endif  // __D3D_RENDERSHADER_SKYPAN_H__
