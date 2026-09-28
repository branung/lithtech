#include "bdefs.h"

#include "clientmgr.h"
#include "iltclient.h"
#include "console.h"
#include "bindmgr.h"
#include "linuxclientde_impl.h"
#include "renderstruct.h"
#include "render.h"
#include "text_mgr.h"

//the ILTClient game interface
static ILTClient *ilt_client;
define_holder(ILTClient, ilt_client);

static LTRESULT
cis_GetEngineHook(const char *name, void **pData)
{
    if(stricmp("hwnd", name) == 0)
    {
		*pData = g_ClientGlob.m_window;
        return LT_OK;
    } else if(stricmp("sdl_window", name) == 0)
    {
		*pData = g_ClientGlob.m_window;
        return LT_OK;
    } else if(stricmp(name, "cshell_hinstance") == 0)
	{
		return bm_GetInstanceHandle(g_pClientMgr->m_hShellModule, pData);
	} else if(stricmp("cres_hinstance", name) == 0)
    {
		return bm_GetInstanceHandle(g_pClientMgr->m_hClientResourceModule, pData);
    }

    return LT_ERROR;
}

void cis_Term()
{
    tmgr_Term();
}

bool cis_RendererIsHere(RenderStruct *pStruct)
{return false;}
bool cis_RendererGoingAway()
{return false;}

static HSURFACE cis_CreateSurface(uint32 w, uint32 h)
{
    return (HSURFACE)SDL_CreateRGBSurface(0, w, h,32,0,0,0,0);
}

LTRESULT cis_DeleteSurface(HSURFACE hSurface)
{
    SDL_FreeSurface((SDL_Surface*)hSurface);
    return LT_OK; // rkj STUB
}

HSURFACE
cis_CreateSurfaceFromPcx(LoadedBitmap *pBitmap)
{
    return cis_CreateSurface(32, 32);
}

static HSURFACE
cis_CreateSurfaceFromBitmap(const char* pBitmap)
{
    return cis_CreateSurfaceFromPcx(nullptr);
}

static HSURFACE cis_GetScreenSurface()
{
    return nullptr;
}

static void cis_GetSurfaceDimentions(HSURFACE surface, uint32 *x, uint32 *y)
{
    if(!surface)
        return;
    SDL_Surface *s = (SDL_Surface*)surface;
    *x = s->w;
    *y = s->h;
}
static LTRESULT cis_SetSurfaceAlpha(HSURFACE surface, float alpha)
{
    SDL_SetSurfaceAlphaMod((SDL_Surface*)surface, (0xff && (255.0f * alpha)));
    return LT_OK;
}

static LTRESULT cis_GetSurfaceAlpha(HSURFACE surface, float &alpha)
{
    uint8 nAlpha=0;
    SDL_GetSurfaceAlphaMod((SDL_Surface*)surface, &nAlpha);
    alpha = (float)nAlpha / 255.0f;
    return LT_OK;
}

static HLTCOLOR cis_CreateColor(float r, float g, float b, bool bTransparent)
{
	if(bTransparent)
	{
		return SETRGB_FT(r,g,b);
	}
	else
	{
		return SETRGB_F(r,g,b);
	}
}

static LTRESULT cis_FillRect(HSURFACE surface, LTRect *rc, HLTCOLOR color)
{
    // A NULL rect means the whole surface, and Shogo's menus pass one
    if(!surface)
        return LT_INVALIDPARAMS;

    SDL_Surface *s = (SDL_Surface *)surface;

    SDL_Rect fill;
    if(rc)
    {
        fill.x = rc->left;
        fill.y = rc->top;
        fill.w = rc->right - rc->left;
        fill.h = rc->bottom - rc->top;
        if(fill.w <= 0 || fill.h <= 0)
            return LT_OK;
    }
    else
    {
        fill.x = 0;
        fill.y = 0;
        fill.w = s->w;
        fill.h = s->h;
    }

    // An out of bounds rect is safe because SDL_FillRect clips to the surface 
    if(SDL_FillRect(s, &fill, SDL_MapRGB(s->format, GETR(color), GETG(color), GETB(color))) != 0)
        return LT_ERROR;

    return LT_OK;
}

static LTRESULT cis_OptimizeSurface(HSURFACE surface, HLTCOLOR trans)
{
    return LT_OK;
}

static LTRESULT
cis_DrawSurfaceOnSurface(HSURFACE dest, HSURFACE src, LTRect *rc, int px, int py, HLTCOLOR transparent)
{
    return LT_OK;
}

static LTRESULT
cis_RenderObjects(HLOCALOBJ cam, HLOCALOBJ *objs, int count, float fTime)
{
    if(g_pClientMgr->Render((CameraInstance*)cam, DRAWMODE_OBJECTLIST, objs, count, fTime))
        return LT_OK;
    RETURN_ERROR(1, RenderObjects, LT_ERROR);
}

static LTRESULT
cis_ClearScreen(LTRect*,uint32,LTRGB*)
{
    return LT_OK;
}

static LTRESULT
cis_FlipScreen(uint32)
{
    return LT_OK;
}

/* Start3D
StartOptimized2D
EndOptimized2D  should be 3 functions */
static LTRESULT
cis_GraphicStub()
{
    return LT_OK; // because I cheat.... and It's rarely checked.
}
static LTRESULT
cis_End3D(uint32)
{
    r_GetRenderStruct()->DeleteContext(nullptr);
    return LT_OK; // because I cheat.... and It's rarely checked.
}

static LTRESULT
cis_Start3D()
{
    r_GetRenderStruct()->CreateContext();
    return LT_OK;
}

// ILTClient slots with no implementation on Linux

static void cis_StubOnce(const char* pName)
{
    static const uint32 knMaxReported = 64;
    static const char*  s_pReported[knMaxReported] = {};
    static uint32       s_nReported = 0;

    for (uint32 i = 0; i < s_nReported; ++i)
    {
        if (s_pReported[i] == pName) return;
    }
    if (s_nReported < knMaxReported) s_pReported[s_nReported++] = pName;
    printf("ILTClient::%s is not implemented on this platform (stubbed)\n", pName);
}

#define CIS_STUB(name) cis_StubOnce(#name)

// Colors

// HLTCOLOR is a packed value, so cis_CreateColor is the whole implementation and freeing does nothing
static void cis_DeleteColor(HLTCOLOR) {}

// Surface queries
static LTRESULT cis_GetBorderSize(HSURFACE, HLTCOLOR, LTRect* pRect)
{
    CIS_STUB(GetBorderSize);
    if (pRect) { pRect->left = pRect->top = pRect->right = pRect->bottom = 0; }
    return LT_UNSUPPORTED;
}

static LTRESULT cis_GetPixel(HSURFACE, uint32, uint32, HLTCOLOR* pColor)
{
    CIS_STUB(GetPixel);
    if (pColor) *pColor = 0;
    return LT_UNSUPPORTED;
}

static LTRESULT cis_SetPixel(HSURFACE, uint32, uint32, HLTCOLOR)
{
    CIS_STUB(SetPixel);
    return LT_UNSUPPORTED;
}

static LTRESULT cis_QueryGraphicDevice(LTGraphicsCaps* pCaps)
{
    // pCaps is left alone as LTGraphicsCaps is only declared here
    CIS_STUB(QueryGraphicDevice);
    return LT_UNSUPPORTED;
}

// Surface user data

static void* cis_GetSurfaceUserData(HSURFACE hSurface)
{
    if (!hSurface) return nullptr;
    return ((SDL_Surface*)hSurface)->userdata;
}

static void cis_SetSurfaceUserData(HSURFACE hSurface, void* pUserData)
{
    if (!hSurface) return;
    ((SDL_Surface*)hSurface)->userdata = pUserData;
}

static LTRESULT cis_UnoptimizeSurface(HSURFACE)
{
    // cis_OptimizeSurface does nothing, so undoing it does nothing too
    return LT_OK;
}

// Heightmaps
static LTRESULT cis_CreateHeightmapFromBitmap(const char*, uint32* pWidth, uint32* pHeight, uint8** ppData)
{
    CIS_STUB(CreateHeightmapFromBitmap);
    if (pWidth)  *pWidth  = 0;
    if (pHeight) *pHeight = 0;
    if (ppData)  *ppData  = nullptr;
    return LT_UNSUPPORTED;
}

static LTRESULT cis_FreeHeightmap(uint8*)
{
    return LT_OK;
}

// Rendering
static LTRESULT cis_RenderCamera(HLOCALOBJ, float)
{
    CIS_STUB(RenderCamera);
    return LT_OK;
}

static LTRESULT cis_MakeCubicEnvMap(HLOCALOBJ, uint32, const char*)
{
    CIS_STUB(MakeCubicEnvMap);
    return LT_UNSUPPORTED;
}

static LTRESULT cis_SetOptimized2DBlend(LTSurfaceBlend)
{
    CIS_STUB(SetOptimized2DBlend);
    return LT_OK;
}

static LTRESULT cis_SetOptimized2DColor(HLTCOLOR)
{
    CIS_STUB(SetOptimized2DColor);
    return LT_OK;
}

// Blits
static bool cis_DrawBitmapToSurface(HSURFACE, const char*, const LTRect*, int, int)
{
    CIS_STUB(DrawBitmapToSurface);
    return false;
}

static LTRESULT cis_DrawSurfaceToSurface(HSURFACE, HSURFACE, LTRect*, int, int)
{
    CIS_STUB(DrawSurfaceToSurface);
    return LT_OK;
}

static LTRESULT cis_DrawSurfaceSolidColor(HSURFACE, HSURFACE, LTRect*, int, int, HLTCOLOR, HLTCOLOR)
{
    CIS_STUB(DrawSurfaceSolidColor);
    return LT_OK;
}

static LTRESULT cis_DrawSurfaceMasked(HSURFACE, HSURFACE, HSURFACE, LTRect*, int, int, HLTCOLOR)
{
    CIS_STUB(DrawSurfaceMasked);
    return LT_OK;
}

static LTRESULT cis_ScaleSurfaceToSurface(HSURFACE, HSURFACE, LTRect*, LTRect*)
{
    CIS_STUB(ScaleSurfaceToSurface);
    return LT_OK;
}

static LTRESULT cis_ScaleSurfaceToSurfaceTransparent(HSURFACE, HSURFACE, LTRect*, LTRect*, HLTCOLOR)
{
    CIS_STUB(ScaleSurfaceToSurfaceTransparent);
    return LT_OK;
}

static LTRESULT cis_ScaleSurfaceToSurfaceSolidColor(HSURFACE, HSURFACE, LTRect*, LTRect*, HLTCOLOR, HLTCOLOR)
{
    CIS_STUB(ScaleSurfaceToSurfaceSolidColor);
    return LT_OK;
}

static LTRESULT cis_TransformSurfaceToSurface(HSURFACE, HSURFACE, LTFloatPt*, int, int, float, float, float)
{
    CIS_STUB(TransformSurfaceToSurface);
    return LT_OK;
}

static LTRESULT cis_TransformSurfaceToSurfaceTransparent(HSURFACE, HSURFACE, LTFloatPt*, int, int, float, float, float, HLTCOLOR)
{
    CIS_STUB(TransformSurfaceToSurfaceTransparent);
    return LT_OK;
}

static LTRESULT cis_WarpSurfaceToSurface(HSURFACE, HSURFACE, LTWarpPt*, int)
{
    CIS_STUB(WarpSurfaceToSurface);
    return LT_OK;
}

static LTRESULT cis_WarpSurfaceToSurfaceTransparent(HSURFACE, HSURFACE, LTWarpPt*, int, HLTCOLOR)
{
    CIS_STUB(WarpSurfaceToSurfaceTransparent);
    return LT_OK;
}

static LTRESULT cis_WarpSurfaceToSurfaceSolidColor(HSURFACE, HSURFACE, LTWarpPt*, int, HLTCOLOR, HLTCOLOR)
{
    CIS_STUB(WarpSurfaceToSurfaceSolidColor);
    return LT_OK;
}

void cis_Init()
{
    ilt_client->GetEngineHook = cis_GetEngineHook;
    ilt_client->CreateSurfaceFromBitmap = cis_CreateSurfaceFromBitmap;
    ilt_client->DeleteSurface = cis_DeleteSurface;
    ilt_client->GetScreenSurface = cis_GetScreenSurface;
    ilt_client->CreateSurface = cis_CreateSurface;
    ilt_client->GetSurfaceDims = cis_GetSurfaceDimentions;
    ilt_client->SetSurfaceAlpha = cis_SetSurfaceAlpha;
    ilt_client->GetSurfaceAlpha = cis_GetSurfaceAlpha;
    ilt_client->OptimizeSurface = cis_OptimizeSurface;
    ilt_client->FillRect = cis_FillRect;
    ilt_client->SetupColor1 = cis_CreateColor;
    ilt_client->SetupColor2 = cis_CreateColor;
    ilt_client->ClearScreen = cis_ClearScreen;
    ilt_client->FlipScreen = cis_FlipScreen;
    ilt_client->Start3D = cis_Start3D;// ();
    ilt_client->StartOptimized2D = cis_GraphicStub;//();
    ilt_client->DrawSurfaceToSurfaceTransparent = cis_DrawSurfaceOnSurface;//(ilt_client->GetScreenSurface(), m_hSurfCursor, LTNULL,
    ilt_client->EndOptimized2D = cis_GraphicStub;//();
    ilt_client->End3D = cis_End3D;//(END3D_CANDRAWCONSOLE);

    ilt_client->RenderObjects = cis_RenderObjects;

    // The slots the Win32 client fills
    ilt_client->CreateColor = cis_CreateColor;
    ilt_client->DeleteColor = cis_DeleteColor;
    ilt_client->GetBorderSize = cis_GetBorderSize;
    ilt_client->GetPixel = cis_GetPixel;
    ilt_client->SetPixel = cis_SetPixel;
    ilt_client->QueryGraphicDevice = cis_QueryGraphicDevice;
    ilt_client->GetSurfaceUserData = cis_GetSurfaceUserData;
    ilt_client->SetSurfaceUserData = cis_SetSurfaceUserData;
    ilt_client->UnoptimizeSurface = cis_UnoptimizeSurface;
    ilt_client->CreateHeightmapFromBitmap = cis_CreateHeightmapFromBitmap;
    ilt_client->FreeHeightmap = cis_FreeHeightmap;
    ilt_client->RenderCamera = cis_RenderCamera;
    ilt_client->MakeCubicEnvMap = cis_MakeCubicEnvMap;
    ilt_client->SetOptimized2DBlend = cis_SetOptimized2DBlend;
    ilt_client->SetOptimized2DColor = cis_SetOptimized2DColor;
    ilt_client->DrawBitmapToSurface = cis_DrawBitmapToSurface;
    ilt_client->DrawSurfaceToSurface = cis_DrawSurfaceToSurface;
    ilt_client->DrawSurfaceSolidColor = cis_DrawSurfaceSolidColor;
    ilt_client->DrawSurfaceMasked = cis_DrawSurfaceMasked;
    ilt_client->ScaleSurfaceToSurface = cis_ScaleSurfaceToSurface;
    ilt_client->ScaleSurfaceToSurfaceTransparent = cis_ScaleSurfaceToSurfaceTransparent;
    ilt_client->ScaleSurfaceToSurfaceSolidColor = cis_ScaleSurfaceToSurfaceSolidColor;
    ilt_client->TransformSurfaceToSurface = cis_TransformSurfaceToSurface;
    ilt_client->TransformSurfaceToSurfaceTransparent = cis_TransformSurfaceToSurfaceTransparent;
    ilt_client->WarpSurfaceToSurface = cis_WarpSurfaceToSurface;
    ilt_client->WarpSurfaceToSurfaceTransparent = cis_WarpSurfaceToSurfaceTransparent;
    ilt_client->WarpSurfaceToSurfaceSolidColor = cis_WarpSurfaceToSurfaceSolidColor;

    tmgr_Init();
}
