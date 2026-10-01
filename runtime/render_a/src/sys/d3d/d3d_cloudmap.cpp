#include "precompile.h"

#include "d3d_cloudmap.h"
#include "common_draw.h"
#include "common_stuff.h"
#include "renderstruct.h"
#include "dtxmgr.h"
#include "d3d_texture.h"
#include "rendererconsolevars.h"

#include "de_world.h"
#include "de_objects.h"
#include "intersect_line.h"
#include "world_client_bsp.h"

static IWorldClientBSP *world_bsp_client_cloud;
define_holder(IWorldClientBSP, world_bsp_client_cloud);

// LT1's constants
static const float kProbeLength   = 200.0f; // Straight down from the model
static const float kHeightFalloff = 0.0025f; // Per unit of clearance
static const float kMinShade      = 0.2f; // Deepest shadow keeps 20%
static const float kMaxShade      = 1.0f;

// The surface flag that opts a brush into the effect. Shared with the world pass
static const uint32 kSurfPanningSky = SURF_PANNINGSKY;

// The cached greyscale grid, rebuilt whenever the pan texture changes
static SharedTexture *s_pGridKey  = NULL;
static uint8 *s_pGrid     = NULL;
static uint32 s_nGridW    = 0;
static uint32 s_nGridH    = 0;
static uint32 s_nMaskU    = 0;
static uint32 s_nMaskV    = 0;

void d3d_ReleaseCloudMap()
{
	delete[] s_pGrid;
	s_pGrid = NULL;
	s_pGridKey = NULL;
	s_nGridW = s_nGridH = s_nMaskU = s_nMaskV = 0;
}

// Build the greyscale grid from the pan texture's top mip.
static bool BuildGrid(SharedTexture *pTexture)
{
	d3d_ReleaseCloudMap();

	if (!pTexture || !g_pStruct->GetTexture)
		return false;

	TextureData *pTD = g_pStruct->GetTexture(pTexture);
	if (!pTD)
		return false;

	TextureMipData &cMip = pTD->m_Mips[0];
	if (!cMip.m_Data || !cMip.m_Width || !cMip.m_Height)
		return false;

	// Power of two only to prevent aliasing of the bit mask wrap
	// Every SkyPan texture inspected in LT1 games are either 256x256 or 128x128, so this is fine
	if ((cMip.m_Width  & (cMip.m_Width - 1)) ||
		(cMip.m_Height & (cMip.m_Height - 1)))
	{
		dsi_ConsolePrint("CloudMapLight: pan texture is %dx%d, not a power of two. "
			"Model cloud shading is off for this level",
			(int)cMip.m_Width, (int)cMip.m_Height);
		return false;
	}

	PFormat cFormat;
	pTD->SetupPFormat(&cFormat);
	const uint32 nBPP = cFormat.GetBytesPerPixel();

	LT_MEM_TRACK_ALLOC(s_pGrid = new uint8[cMip.m_Width * cMip.m_Height],
		LT_MEM_TYPE_RENDER_WORLD);
	if (!s_pGrid)
		return false;

	uint8 *pDst = s_pGrid;
	for (uint32 y = 0; y < cMip.m_Height; ++y)
	{
		const uint8 *pSrc = cMip.m_Data + (int32)y * cMip.m_Pitch;
		for (uint32 x = 0; x < cMip.m_Width; ++x, pSrc += nBPP)
		{
			GenericColor cTexel;
			cTexel.dwVal = 0;
			memcpy((void*)&cTexel, pSrc, nBPP);

			PValue nColor;
			g_FormatMgr.PValueFromFormatColor(&cFormat, cTexel, nColor);

			const uint32 nSum = (uint32)PValue_GetR(nColor)
							  + (uint32)PValue_GetG(nColor)
							  + (uint32)PValue_GetB(nColor);
			*pDst++ = (uint8)(nSum / 3);
		}
	}

	s_pGridKey = pTexture;
	s_nGridW = cMip.m_Width;
	s_nGridH = cMip.m_Height;
	s_nMaskU = cMip.m_Width  - 1;
	s_nMaskV = cMip.m_Height - 1;
	return true;
}

static bool EnsureGrid()
{
	SharedTexture *pPan = g_pSceneDesc ? g_pSceneDesc->m_pGlobalPanTexture : NULL;

	if (!pPan)
	{
		if (s_pGrid)
			d3d_ReleaseCloudMap();
		return false;
	}

	if (pPan != s_pGridKey)
		return BuildGrid(pPan);

	return s_pGrid != NULL;
}

bool d3d_GetCloudMapShade(const LTVector& vPos, float& fShade)
{
	if (!g_CV_CloudMapLight)
		return false;

	if (!EnsureGrid())
		return false;

	if (!world_bsp_client_cloud || !world_bsp_client_cloud->IsLoaded())
		return false;

	// The root world BSP only
	if (world_bsp_client_cloud->NumWorldModels() == 0)
		return false;

	const WorldData *pWorld = world_bsp_client_cloud->GetWorldModel(0);
	if (!pWorld || !pWorld->m_pValidBsp)
		return false;

	LTVector vStart = vPos;
	LTVector vEnd   = vPos;
	vEnd.y -= kProbeLength;

	LTVector vHit;
	LTPlane  cHitPlane;
	const Node *pNode = IntersectLine(pWorld->m_pValidBsp->GetRootNode(),
		&vStart, &vEnd, &vHit, &cHitPlane);
	if (!pNode)
		return false;

	const WorldPoly *pPoly = pNode->GetPoly();
	if (!pPoly)
		return false;

	const Surface *pSurface = pPoly->GetSurface();
	if (!pSurface || !(pSurface->m_Flags & kSurfPanningSky))
		return false;

	// Texel coordinates as the world pass computes them:
	// offset in world units then divided by world units per texel
	const float fU = (vPos.x + g_pSceneDesc->m_fGlobalPanOffsetX) / g_pSceneDesc->m_fGlobalPanScaleX;
	const float fV = (g_pSceneDesc->m_fGlobalPanOffsetZ + vPos.z) / g_pSceneDesc->m_fGlobalPanScaleZ;

	// Truncated toward zero similarly to _ftol in LT1
	const int32 nU = (int32)fU;
	const int32 nV = (int32)fV;

	const uint32 nIndex = ((uint32)nU & s_nMaskU) + (((uint32)nV & s_nMaskV) * s_nGridW);
	float fCloud = (float)s_pGrid[nIndex] * (1.0f / 255.0f);

	// Height above the ground lightens the shadow
	fCloud += (vPos.y - vHit.y) * kHeightFalloff;

	fShade = LTCLAMP(fCloud, kMinShade, kMaxShade);
	return true;
}
