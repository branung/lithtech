#include "precompile.h"
#ifdef _WINDOWS
#include "interface_helpers.h"
#endif
#include "pixelformat.h"
#include "d3d_surface.h"
#include "renderstruct.h"
#include "d3d_device.h"
#include "dirtyrect.h"
#include "common_stuff.h"
#include "d3d_utils.h"
#include "d3d_shell.h"
#include "client_formatmgr.h"
#include "d3d_draw.h"
#include "rendererconsolevars.h"

#include <vector>
#include "zlib.h"


// ---------------------------------------------------------------- //
// Globals.
// ---------------------------------------------------------------- //

RECT g_ScreenLockRect;
bool g_bScreenLocked=false;

static IClientFormatMgr *s_pFormatMgr;
define_holder(IClientFormatMgr, s_pFormatMgr);

// ---------------------------------------------------------------- //
// Externs.
// ---------------------------------------------------------------- //
extern bool g_bInOptimized2D;
extern FormatMgr g_FormatMgr;

extern void d3d_DestroyTiles(RSurface *pSurface);
extern void d3d_BlitToScreen3D(BlitRequest *pRequest);
extern void d3d_BlitToScreen3D_Old(BlitRequest *pRequest);
extern void d3d_BlitToScreen3D_ScaledSub(BlitRequest *pRequest);
extern void d3d_WarpToScreen3D(BlitRequest *pRequest);
extern int  d3d_RenderScene(SceneDesc* pDesc);

// For the optimized 2D stuff
extern bool d3d_GetOptimized2DBlend(LTSurfaceBlend &blend);
extern bool d3d_OptimizeSurface(HLTBUFFER hBuffer, PValue transparentColor);


// ---------------------------------------------------------------- //
// Helpers.
// ---------------------------------------------------------------- //
static inline void LTRectToRect(RECT *pDest, LTRect *pSrc)
{
	pDest->left = pSrc->left;
	pDest->top = pSrc->top;
	pDest->right = pSrc->right;
	pDest->bottom = pSrc->bottom;
}


// ---------------------------------------------------------------- //
// RenderStruct functions.
// ---------------------------------------------------------------- //
HLTBUFFER d3d_CreateSurface(int width, int height)
{
	HRESULT				hResult;
	LPDIRECT3DSURFACE9	pSurface;
	RSurface*			pRSurface;

	if (!PD3DDEVICE)				return NULL;
	if (width == 0 || height == 0)	return NULL;

	// Creates in the format of the screen. (Note: Don't get too used to this. I plan on changing it).
//	uint32 iBitCount = 0, iAlphaMask = 0, iRedMask = 0, iGreenMask = 0, iBlueMask = 0;
//	d3d_GetColorMasks(g_Device.GetModeInfo()->Format,iBitCount,iAlphaMask,iRedMask,iGreenMask,iBlueMask);
//	if (iBitCount == 16) { hResult = PD3DDEVICE->CreateImageSurface(width,height,D3DFMT_A1R5G5B5,&pSurface); }
//	else { hResult = PD3DDEVICE->CreateImageSurface(width,height,D3DFMT_A8R8G8B8,&pSurface); }
	hResult = PD3DDEVICE->CreateOffscreenPlainSurface(width,height,g_Device.GetModeInfo()->Format, D3DPOOL_SCRATCH, &pSurface);
	if (hResult != D3D_OK) return NULL;

	LT_MEM_TRACK_ALLOC(pRSurface = (RSurface*)dalloc_z(sizeof(RSurface)),LT_MEM_TYPE_RENDERER);
	if (!pRSurface) { pSurface->Release(); return NULL; }

	pRSurface->m_pSurface					= pSurface;
	pRSurface->m_LastTransparentColor.dwVal = 0;
	pSurface->GetDesc(&pRSurface->m_Desc);

	return (HLTBUFFER)pRSurface;
}


void d3d_DeleteSurface(HLTBUFFER hSurf)
{
	RSurface *pRSurface;

	if(!hSurf)
		return;

	pRSurface = (RSurface*)hSurf;

	d3d_DestroyTiles(pRSurface);
	pRSurface->m_pSurface->Release();
	dfree(pRSurface);
}

void d3d_GetSurfaceInfo(HLTBUFFER hSurf, uint32 *pWidth, uint32 *pHeight)
{
	if (!hSurf) return;
	RSurface* pRSurface = (RSurface*)hSurf;
	if (pWidth)			*pWidth = pRSurface->m_Desc.Width;
	if (pHeight)		*pHeight = pRSurface->m_Desc.Height;
}

void* d3d_LockSurface(HLTBUFFER hSurf, uint32& Pitch)
{
	if (!hSurf) return nullptr;
	RSurface* pRSurface = (RSurface*)hSurf;

	D3DLOCKED_RECT LockRect;
	HRESULT hResult = pRSurface->m_pSurface->LockRect(&LockRect, nullptr, 0);
	if(hResult == D3D_OK) {
		Pitch = LockRect.Pitch;
		return LockRect.pBits; }
	else {
		return nullptr; }
}

void d3d_UnlockSurface(HLTBUFFER hSurf)
{
	if (!hSurf) return;
	RSurface* pRSurface = (RSurface*)hSurf;
	pRSurface->m_pSurface->UnlockRect();
}

bool d3d_LockScreen(int left, int top, int right, int bottom, void **pData, long *pPitch)
{
	if (g_bScreenLocked) return false;
	g_ScreenLockRect.left	= left;
	g_ScreenLockRect.top	= top;
	g_ScreenLockRect.right	= right;
	g_ScreenLockRect.bottom = bottom;

	LPDIRECT3DSURFACE9		pBackBuffer = nullptr;
	if (FAILED(PD3DDEVICE->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&pBackBuffer)))	{ return false; }

	D3DLOCKED_RECT LockRect;
	if (FAILED(pBackBuffer->LockRect(&LockRect, &g_ScreenLockRect, 0)))			{
		pBackBuffer->Release(); return false; }

	*pData = LockRect.pBits; *pPitch = LockRect.Pitch;

	if (pBackBuffer)																{ pBackBuffer->Release(); pBackBuffer = nullptr; }
	g_bScreenLocked = true;

	return true;
}

void d3d_UnlockScreen()
{
	if (!g_bScreenLocked) return;

	LPDIRECT3DSURFACE9		pBackBuffer = nullptr;
	if (FAILED(PD3DDEVICE->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&pBackBuffer)))	{ return; }
	if (FAILED(pBackBuffer->UnlockRect()))											{ pBackBuffer->Release(); return; }

	if (pBackBuffer)																{ pBackBuffer->Release(); pBackBuffer = nullptr; }
	g_bScreenLocked = false;

	InvalidateRect((LTRect*)&g_ScreenLockRect);
}

void d3d_BlitFromScreen(BlitRequest* pRequest)
{
	RSurface *pRSurface = (RSurface*)pRequest->m_hBuffer;

	// Spit out a warning if we're in 3D..
	if (g_Device.IsIn3D()) {
		AddDebugMessage(20, "Warning: drawing a nonoptimized surface while in 3D mode.");
		if (PD3DDEVICE) PD3DDEVICE->EndScene(); }

	RECT srcRect; LTRectToRect(&srcRect,  pRequest->m_pSrcRect);
	POINT destPt; destPt.x = pRequest->m_pDestRect->left; destPt.y = pRequest->m_pDestRect->top;

	LPDIRECT3DSURFACE9		pBackBuffer = nullptr;
	if (FAILED(PD3DDEVICE->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&pBackBuffer)))	{ return; }

	// Copy away!
	HRESULT hResult = PD3DDEVICE->UpdateSurface(pBackBuffer,&srcRect,pRSurface->m_pSurface,&destPt);

	if (pBackBuffer)																{ pBackBuffer->Release(); pBackBuffer = nullptr; }

	if (g_Device.IsIn3D()) {
		if (PD3DDEVICE) PD3DDEVICE->BeginScene(); }
}

void d3d_ReallyBlitToScreen(BlitRequest *pRequest)
{
	RSurface* pRSurface = (RSurface*)pRequest->m_hBuffer;

	// Spit out a warning if we're in 3D..
	if (g_Device.IsIn3D())
	{
		AddDebugMessage(20, "Warning: drawing a nonoptimized surface while in 3D mode.");
		if(PD3DDEVICE) PD3DDEVICE->EndScene();
	}

	RECT srcRect;  LTRectToRect(&srcRect,  pRequest->m_pSrcRect);
	RECT destRect; LTRectToRect(&destRect, pRequest->m_pDestRect);
	POINT destPt;  destPt.x = pRequest->m_pDestRect->left; destPt.y = pRequest->m_pDestRect->top;

	LPDIRECT3DSURFACE9		pBackBuffer = nullptr;
	if (FAILED(PD3DDEVICE->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&pBackBuffer)))
	{
		return;
	}

	D3DSURFACE_DESC			BackBuffer_SurfDesc;
	pBackBuffer->GetDesc(&BackBuffer_SurfDesc);
	D3DSURFACE_DESC			Src_SurfDesc;
	pRSurface->m_pSurface->GetDesc(&Src_SurfDesc);

	LTRGB TranspColor;
	bool bUseTrans = false;

	if (pRequest->m_BlitOptions & BLIT_TRANSPARENT)
	{
  		if (pRSurface->m_LastTransparentColor.dwVal != pRequest->m_TransparentColor.dwVal)
		{
			pRSurface->m_LastTransparentColor = pRequest->m_TransparentColor;
		}
		TranspColor.a = RGBA_GETA(pRequest->m_TransparentColor.dwVal);
		TranspColor.r = RGBA_GETR(pRequest->m_TransparentColor.dwVal);
		TranspColor.g = RGBA_GETG(pRequest->m_TransparentColor.dwVal);
		TranspColor.b = RGBA_GETB(pRequest->m_TransparentColor.dwVal);
		bUseTrans = true;
	}

	// Setup for a ConvertRequest to do it...
	FMConvertRequest request;
	PFormat PSrcFormat;
	if(!d3d_D3DFormatToPFormat(Src_SurfDesc.Format,&PSrcFormat))
		return;

	request.m_pSrcFormat = &PSrcFormat;

	PFormat PDstFormat;
	if(!d3d_D3DFormatToPFormat(BackBuffer_SurfDesc.Format,&PDstFormat))
		return;

	request.m_pDestFormat = &PDstFormat;

	request.m_Width			= (uint32)(srcRect.right - srcRect.left);
	request.m_Height		= (uint32)(srcRect.bottom - srcRect.top);

	D3DLOCKED_RECT SrcLockRect,DstLockRect;
	if (FAILED(pRSurface->m_pSurface->LockRect(&SrcLockRect,&srcRect,D3DLOCK_READONLY))) { pBackBuffer->Release(); return; }
	if (FAILED(pBackBuffer->LockRect(&DstLockRect,&destRect,0))) { pRSurface->m_pSurface->UnlockRect(); pBackBuffer->Release(); return; }
	request.m_pSrc			= (uint8*)SrcLockRect.pBits;
	request.m_SrcPitch		= SrcLockRect.Pitch;
	request.m_pDest			= (uint8*)DstLockRect.pBits;
	request.m_DestPitch		= DstLockRect.Pitch;

	s_pFormatMgr->Mgr()->ConvertPixels(&request,bUseTrans ? &TranspColor : nullptr);

	pRSurface->m_pSurface->UnlockRect();
	pBackBuffer->UnlockRect();

	if (pBackBuffer)
	{
		pBackBuffer->Release();
		pBackBuffer = nullptr;
	}

	if (g_Device.IsIn3D())
	{
		if(PD3DDEVICE) PD3DDEVICE->BeginScene();
	}

	InvalidateRect((LTRect*)&destRect);
}

void d3d_BlitToScreen(BlitRequest *pRequest)
{
	if (!pRequest || !pRequest->m_hBuffer) return;

	RSurface* pRSurface = (RSurface*)pRequest->m_hBuffer;

	// Optimize the surface if we need to use a blend mode
	LTSurfaceBlend blend;
	d3d_GetOptimized2DBlend(blend);
	if (g_bInOptimized2D && g_Device.IsIn3D() && (pRSurface->m_pTiles == nullptr))
	{
		d3d_OptimizeSurface(pRequest->m_hBuffer, (pRequest->m_BlitOptions == BLIT_TRANSPARENT) ? pRequest->m_TransparentColor.dwVal : 0xFFFFFFFF );
	}

	// Can this be drawn as an optimized 2D surface?
	if (pRSurface->m_pTiles && g_bInOptimized2D && g_Device.IsIn3D())
	{
		if (pRequest->m_bUseOld)
		{
			// The old scaled path draws whole tiles, so it only fits when the source rectangle covers the surface
			const LTRect *pSrc = pRequest->m_pSrcRect;
			const bool bWhole = !pSrc ||
				(pSrc->left <= 0 && pSrc->top <= 0 &&
				 pSrc->right >= (int)pRSurface->m_Desc.Width &&
				 pSrc->bottom >= (int)pRSurface->m_Desc.Height);
			if (bWhole)
				d3d_BlitToScreen3D_Old(pRequest);
			else
				d3d_BlitToScreen3D_ScaledSub(pRequest);
		}
		else
			d3d_BlitToScreen3D(pRequest);
	}
	else
	{
		d3d_ReallyBlitToScreen(pRequest);
	}
}

bool d3d_WarpToScreen(BlitRequest *pRequest)
{
	if (!pRequest || !pRequest->m_hBuffer) return false;

	RSurface* pRSurface = (RSurface*)pRequest->m_hBuffer;

	// Can this be drawn as an optimized 2D surface?
	if (pRSurface->m_pTiles && g_bInOptimized2D && g_Device.IsIn3D())
	{
		d3d_WarpToScreen3D(pRequest);
		return true;
	}
	return false;
}

bool d3d_GetScreenFormat(PFormat* pFormat)
{
	D3DModeInfo* pMode = g_Device.GetModeInfo();
	return d3d_D3DFormatToPFormat(pMode->Format,pFormat);
}

// The back buffer as 0xAARRGGBB pixels, top row first. The buffer is released on every path
static bool d3d_ReadBackBuffer(CMoArray<uint32> &outputBuf, uint32 nWidth, uint32 nHeight, const char *pszWho)
{
	LPDIRECT3DSURFACE9 pBackBuffer = nullptr;
	if (FAILED(PD3DDEVICE->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&pBackBuffer)))	{ return false; }
	if (!outputBuf.SetSize(nWidth * nHeight))
	{
		pBackBuffer->Release();
		return false;
	}

	// Lock the back buffer.
	D3DLOCKED_RECT LockRect; HRESULT hResult;
	if (FAILED(hResult = pBackBuffer->LockRect(&LockRect, nullptr, 0)))
	{
		dsi_ConsolePrint("%s: g_pOffscreen->Lock returned %d.", pszWho, hResult);
		pBackBuffer->Release();
		return false;
	}

	// Do the color conversion.
	FMConvertRequest request;
	d3d_GetScreenFormat(request.m_pSrcFormat);
	request.m_pSrc		= (uint8*)LockRect.pBits;
	request.m_SrcPitch	= LockRect.Pitch;
	request.m_pDestFormat->Init(BPP_32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
	request.m_pDest		= (uint8*)outputBuf.GetArray();
	request.m_DestPitch = nWidth * sizeof(uint32);
	request.m_Width		= nWidth;
	request.m_Height	= nHeight;
	LTRESULT dResult	= g_FormatMgr.ConvertPixels(&request);

	const bool bUnlocked = SUCCEEDED(pBackBuffer->UnlockRect());
	pBackBuffer->Release();
	if (!bUnlocked)
	{
		dsi_ConsolePrint("%s: FormatMgr::ConvertPixels returned %d.", pszWho, dResult);
		return false;
	}
	return true;
}

void d3d_MakeScreenShotBMP(const char *pFilename, uint32 nWidth, uint32 nHeight)
{
#ifdef _WINDOWS
	CMoArray<uint32> outputBuf;
	if (!d3d_ReadBackBuffer(outputBuf, nWidth, nHeight, "MakeScreenShotBMP")) return;
	uint32 imageSize = nWidth * nHeight;

	// Save the bitmap.
	FILE* fp = fopen(pFilename, "wb");
	if (!fp)
	{
		dsi_ConsolePrint("MakeScreenShotBMP: fopen(%s) failed.", pFilename);
		return;
	}

	BITMAPFILEHEADER fileHeader; BITMAPINFOHEADER infoHeader;
	memset(&fileHeader, 0, sizeof(fileHeader));
	memset(&infoHeader, 0, sizeof(infoHeader));

	fileHeader.bfType = ('M' << 8) | 'B';
	fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	fileHeader.bfSize = imageSize*3 + fileHeader.bfOffBits;

	infoHeader.biSize = sizeof(infoHeader);
	infoHeader.biWidth = nWidth;
	infoHeader.biHeight = nHeight;
	infoHeader.biPlanes = 1;
	infoHeader.biBitCount = 24;
	infoHeader.biCompression = BI_RGB;

	fwrite(&fileHeader, sizeof(fileHeader), 1, fp);
	fwrite(&infoHeader, sizeof(infoHeader), 1, fp);

	for (uint32 y=0; y < nHeight; ++y)
	{
		uint32* pOutLine = &outputBuf[(nHeight-y-1) * nWidth];
		for(uint32 x=0; x < nWidth; ++x)
		{
			fwrite(pOutLine, 3, 1, fp);
			++pOutLine;
		}
	}

	fclose(fp);

	dsi_ConsolePrint("ScreenShot: Created %s successfully.", pFilename);
#endif
}

static void d3d_PutBE32(uint8 *p, uint32 n)
{
	p[0] = (uint8)(n >> 24); p[1] = (uint8)(n >> 16); p[2] = (uint8)(n >> 8); p[3] = (uint8)n;
}

// One PNG chunk: its length, type, data, and the CRC of the type and data
static void d3d_WritePNGChunk(FILE *fp, const char *pszType, const uint8 *pData, uint32 nLen)
{
	uint8 b[4];
	d3d_PutBE32(b, nLen);
	fwrite(b, 4, 1, fp);
	fwrite(pszType, 4, 1, fp);
	if (nLen) fwrite(pData, nLen, 1, fp);
	uLong crc = crc32(0L, (const Bytef *)pszType, 4);
	if (nLen) crc = crc32(crc, pData, nLen);
	d3d_PutBE32(b, (uint32)crc);
	fwrite(b, 4, 1, fp);
}

// An 8-bit RGB PNG, each row with the Sub filter, which shrinks a rendered frame for one subtraction a byte
void d3d_MakeScreenShotPNG(const char *pFilename, uint32 nWidth, uint32 nHeight)
{
	CMoArray<uint32> outputBuf;
	if (!d3d_ReadBackBuffer(outputBuf, nWidth, nHeight, "MakeScreenShotPNG")) return;

	const uint32 nRow = nWidth * 3 + 1;
	std::vector<uint8> raw((size_t)nRow * nHeight);
	for (uint32 y = 0; y < nHeight; ++y)
	{
		uint8 *pRow = &raw[(size_t)y * nRow];
		pRow[0] = 1; // Sub
		uint8 prev[3] = { 0, 0, 0 };
		const uint32 *pIn = &outputBuf[y * nWidth];
		for (uint32 x = 0; x < nWidth; ++x)
		{
			const uint8 rgb[3] = { (uint8)(pIn[x] >> 16), (uint8)(pIn[x] >> 8), (uint8)pIn[x] };
			for (int c = 0; c < 3; ++c)
			{
				pRow[1 + x * 3 + c] = (uint8)(rgb[c] - prev[c]);
				prev[c] = rgb[c];
			}
		}
	}

	uLongf nPacked = compressBound((uLong)raw.size());
	std::vector<uint8> packed(nPacked);
	if (compress2(&packed[0], &nPacked, &raw[0], (uLong)raw.size(), 6) != Z_OK)
	{
		dsi_ConsolePrint("MakeScreenShotPNG: compressing %s failed.", pFilename);
		return;
	}

	FILE* fp = fopen(pFilename, "wb");
	if (!fp)
	{
		dsi_ConsolePrint("MakeScreenShotPNG: fopen(%s) failed.", pFilename);
		return;
	}

	static const uint8 kSignature[8] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
	fwrite(kSignature, sizeof(kSignature), 1, fp);

	uint8 ihdr[13];
	d3d_PutBE32(ihdr, nWidth);
	d3d_PutBE32(ihdr + 4, nHeight);
	ihdr[8] = 8;  // bits per channel
	ihdr[9] = 2;  // RGB
	ihdr[10] = ihdr[11] = ihdr[12] = 0; // deflate, adaptive filtering, not interlaced
	d3d_WritePNGChunk(fp, "IHDR", ihdr, sizeof(ihdr));
	d3d_WritePNGChunk(fp, "IDAT", &packed[0], (uint32)nPacked);
	d3d_WritePNGChunk(fp, "IEND", nullptr, 0);

	if (fclose(fp) != 0)
	{
		dsi_ConsolePrint("MakeScreenShotPNG: writing %s failed.", pFilename);
		return;
	}
	dsi_ConsolePrint("ScreenShot: Created %s successfully.", pFilename);
}

void d3d_MakeScreenShotTGA(const char *pFilename, uint32 nWidth, uint32 nHeight)
{
	LPDIRECT3DSURFACE9 pBackBuffer = nullptr; D3DSURFACE_DESC SurfDesc;
	if (FAILED(PD3DDEVICE->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&pBackBuffer)))	{ return; }
	pBackBuffer->GetDesc(&SurfDesc);

	CMoArray<uint32> outputBuf;
	uint32 imageSize = nWidth * nHeight;
	if (!outputBuf.SetSize(imageSize)) return;

	// Lock the back buffer.
	D3DLOCKED_RECT LockRect; HRESULT hResult;
	if (FAILED(hResult = pBackBuffer->LockRect(&LockRect, nullptr, 0)))
	{
		dsi_ConsolePrint("MakeScreenShotTGA: g_pOffscreen->Lock returned %d.", hResult);
		return;
	}

	// Do the color conversion.
	FMConvertRequest request;
	d3d_GetScreenFormat(request.m_pSrcFormat);
	request.m_pSrc		= (uint8*)LockRect.pBits;
	request.m_SrcPitch	= LockRect.Pitch;
	request.m_pDestFormat->Init(BPP_32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
	request.m_pDest		= (uint8*)outputBuf.GetArray();
	request.m_DestPitch = nWidth * sizeof(uint32);
	request.m_Width		= nWidth;
	request.m_Height	= nHeight;
	LTRESULT dResult	= g_FormatMgr.ConvertPixels(&request);

	if (FAILED(pBackBuffer->UnlockRect()))
	{
		dsi_ConsolePrint("MakeScreenShotTGA: FormatMgr::ConvertPixels returned %d.", dResult);
		return;
	}

	// Save the bitmap.
	FILE* fp = fopen(pFilename, "wb");
	if (!fp)
	{
		dsi_ConsolePrint("MakeScreenShotTGA: fopen(%s) failed.", pFilename);
		return;
	}

	uint8 nByte;

	//write out the ID length
	nByte = 0;
	fwrite(&nByte, sizeof(nByte), 1, fp);

	//write out the color map information (none)
	nByte = 0;
	fwrite(&nByte, sizeof(nByte), 1, fp);

	//write out the image type (uncompressed 32bit)
	nByte = 2;
	fwrite(&nByte, sizeof(nByte), 1, fp);

	//we have no color data, so just write out 5 empty bytes
	nByte = 0;
	for(uint32 nCurr = 0; nCurr < 5; nCurr++)
		fwrite(&nByte, sizeof(nByte), 1, fp);

	//write out the texture origin
	uint16 nWord = 0;
	fwrite(&nWord, sizeof(nWord), 1, fp);
	fwrite(&nWord, sizeof(nWord), 1, fp);

	//now the image dimensions
	nWord = nWidth;
	fwrite(&nWord, sizeof(nWord), 1, fp);
	nWord = nHeight;
	fwrite(&nWord, sizeof(nWord), 1, fp);

	//now the pixel depth
	nByte = 24;
	fwrite(&nByte, sizeof(nByte), 1, fp);

	//now the image descriptor
	nByte = 0;
	fwrite(&nByte, sizeof(nByte), 1, fp);

	//and now the image data!
	for (uint32 y=0; y < nHeight; ++y)
	{
		uint32* pOutLine = &outputBuf[(nHeight-y-1) * nWidth];
		for(uint32 x=0; x < nWidth; ++x)
		{
			fwrite(pOutLine, 3, 1, fp);
			++pOutLine;
		}
	}

	//now the footer

	//extension offset
	uint32 nDWord = 0;
	fwrite(&nDWord, sizeof(nDWord), 1, fp);

	//developer offset
	nDWord = 0;
	fwrite(&nDWord, sizeof(nDWord), 1, fp);

	//write out the string tag
	char pszTag[] = "TRUEVISION-XFILE.";
	fwrite(pszTag, sizeof(pszTag) - 1, 1, fp);

	//binary terminator
	nByte = 0;
	fwrite(&nByte, sizeof(nByte), 1, fp);

	//close the file
	fclose(fp);

	if (pBackBuffer)
	{
		pBackBuffer->Release();
		pBackBuffer = nullptr;
	}

	dsi_ConsolePrint("ScreenShot: Created %s successfully.", pFilename);
}


void d3d_MakeScreenShot(const char *pFilename)
{
	LPDIRECT3DSURFACE9 pBackBuffer = nullptr;
	if (FAILED(PD3DDEVICE->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&pBackBuffer)))
	{
		return;
	}

	D3DSURFACE_DESC SurfDesc;
	pBackBuffer->GetDesc(&SurfDesc);

	// PNG for a .png name, the BMP the engine always wrote for anything else
	const size_t nLen = strlen(pFilename);
	if (nLen >= 4 && stricmp(pFilename + nLen - 4, ".png") == 0)
		d3d_MakeScreenShotPNG(pFilename, SurfDesc.Width, SurfDesc.Height);
	else
		d3d_MakeScreenShotBMP(pFilename, SurfDesc.Width, SurfDesc.Height);

	pBackBuffer->Release();
}


//Generates a series of images that form a cubic environment map of the form Prefix[FW|BK|LF|RI|UP|DW].bmp
//aligned along the world's basis space from the given position
void d3d_MakeCubicEnvMap(const char* pszPrefix, uint32 nSize, const SceneDesc& InSceneDesc)
{
	//first off, we need to make sure that the screen is large enough to accomodate our environment
	//map
	if((g_ScreenWidth < nSize) || (g_ScreenHeight < nSize))
	{
		dsi_ConsolePrint("MakeEnvMap: The screen must be at least %d pixels in both directions", nSize);
		return;
	}

	//ok, so the screen can take it, so let us begin setting up all 6 images
	uint32 nOldScreenWidth = g_ScreenWidth;
	uint32 nOldScreenHeight = g_ScreenHeight;

	//first off, override our screen dimensions to fit those of the cubic environment map
	g_ScreenWidth = nSize;
	g_ScreenHeight = nSize;

	//now setup our scene description that we will use to generate the images
	SceneDesc Scene = InSceneDesc;

	Scene.m_Rect.left		= 0;
	Scene.m_Rect.top		= 0;
	Scene.m_Rect.right		= nSize;
	Scene.m_Rect.bottom		= nSize;

	//however, we need to make sure that our FOV is 90 in both directions
	Scene.m_xFov = Scene.m_yFov = MATH_PI / 2.0f;

	//the list of orientations for each view we want to take
	LTVector vForward[6], vUp[6];
	const char* pszPostfix[6] = { "FR", "BK", "RT", "LF", "UP", "DN" };

	vForward[0].Init(0.0f, 0.0f, 1.0f);		vUp[0].Init(0.0f, 1.0f, 0.0f);
	vForward[1].Init(0.0f, 0.0f, -1.0f);	vUp[1].Init(0.0f, 1.0f, 0.0f);
	vForward[2].Init(1.0f, 0.0f, 0.0f);		vUp[2].Init(0.0f, 1.0f, 0.0f);
	vForward[3].Init(-1.0f, 0.0f, 0.0f);	vUp[3].Init(0.0f, 1.0f, 0.0f);
	vForward[4].Init(0.0f, 1.0f, 0.0f);		vUp[4].Init(0.0f, 0.0f, -1.0f);
	vForward[5].Init(0.0f, -1.0f, 0.0f);	vUp[5].Init(0.0f, 0.0f, 1.0f);

	CD3D_Device::Start3D();

	LTRGBColor FillColor;
	FillColor.dwordVal = 0x00000000;

	//run through all the directions and generate the images
	for(uint32 nCurrImage = 0; nCurrImage < 6; nCurrImage++)
	{
		//setup a matrix that has the orientation
		LTVector vRight = vForward[nCurrImage].Cross(vUp[nCurrImage]);
		LTMatrix mOr;
		mOr.SetBasisVectors(&vRight, &vUp[nCurrImage], &vForward[nCurrImage]);

		//convert that to our quaternion
		Scene.m_Rotation.ConvertFromMatrix(mOr);

		//we need to clear out the scene
		d3d_Clear(NULL, CLEARSCREEN_RENDER | CLEARSCREEN_SCREEN, FillColor);

		//now we can render the scene
		d3d_RenderScene(&Scene);

		//now we figure out the name of the image we will be taking
		char pszFilename[MAX_PATH + 1];
		LTStrCpy(pszFilename, pszPrefix, sizeof(pszFilename));
		LTStrCat(pszFilename, pszPostfix[nCurrImage], sizeof(pszFilename));
		LTStrCat(pszFilename, ".tga", sizeof(pszFilename));

		//now we need to take the actual screenshot
		d3d_MakeScreenShotTGA(pszFilename, nSize, nSize);
	}

	//now we restore everything
	g_ScreenWidth  = nOldScreenWidth;
	g_ScreenHeight = nOldScreenHeight;

	//clear one final time with the restored dimensions
	d3d_Clear(NULL, CLEARSCREEN_RENDER | CLEARSCREEN_SCREEN, FillColor);

	CD3D_Device::End3D();

}


void d3d_SwapBuffers(uint flags)
{
	CSAccess cLoadRenderCSLock(&g_Device.GetLoadRenderCS());

	if (!PD3DDEVICE)
		return;

	if ((flags & FLIPSCREEN_DIRTY) != 0)
	{
		DirtyRectSwap();
	}
	else
	{
		HRESULT hResult = PD3DDEVICE->Present(NULL,NULL,NULL,NULL);
	}

	// prevent frame buffering
	g_Device.PreventFrameBuffering();

	ClearDirtyRects();
}


