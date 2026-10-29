// Smacker (.smk) video

#include "bdefs.h"
#include "clientmgr.h"
#include "interface_helpers.h"
#include "render.h"
#include <d3d9.h>
#include <SDL.h>
#include "smackervideomgrimpl.h"
#include "smacker.h"


// A quad in screen space, as DShowVideoInst draws
struct SSmackerVertex
{
	float	m_fX, m_fY, m_fZ, m_fRHW;
	float	m_fU, m_fV;

	void	Init(float fX, float fY, float fU, float fV)
	{
		m_fX = fX;
		m_fY = fY;
		m_fZ = 0.0f;
		m_fRHW = 1.0f;
		m_fU = fU;
		m_fV = fV;
	}
};

#define SMACKERVERTEX_FVF	(D3DFVF_XYZRHW | D3DFVF_TEX1)


// -------------------------------------------------------------------------------- //
// SmackerVideoMgr
// -------------------------------------------------------------------------------- //

static bool IsSmackerFile(const char *pFilename)
{
	size_t n = strlen(pFilename);
	return n >= 4 && stricmp(pFilename + n - 4, ".smk") == 0;
}

extern int32 g_CV_VideoDebug;

LTRESULT SmackerVideoMgr::CreateScreenVideo(const char *pFilename, uint32 flags, VideoInst* &pVideo)
{
	pVideo = NULL;
	if (g_CV_VideoDebug)
		dsi_ConsolePrint("Smacker: asked for %s (flags %u), %s", pFilename, flags,
						 IsSmackerFile(pFilename) ? "ours" : "to DirectShow");

	if (!IsSmackerFile(pFilename))
		return DShowVideoMgr::CreateScreenVideo(pFilename, flags, pVideo);

	SmackerVideoInst *pRet;
	LT_MEM_TRACK_ALLOC(pRet = new SmackerVideoInst(this), LT_MEM_TYPE_MISC);
	if (!pRet)
	{
		RETURN_ERROR(1, SmackerVideoMgr::CreateScreenVideo, LT_OUTOFMEMORY);
	}

	LTRESULT dResult = pRet->InitVideo(pFilename, flags);
	if (dResult != LT_OK)
	{
		pRet->Release();
		return dResult;
	}

	m_Videos.AddTail(pRet, &pRet->m_Link);
	pVideo = pRet;
	return LT_OK;
}


// -------------------------------------------------------------------------------- //
// SmackerVideoInst
// -------------------------------------------------------------------------------- //

SmackerVideoInst::SmackerVideoInst(SmackerVideoMgr *pMgr)
	: m_pMgr(pMgr), m_pSmk(NULL), m_nWidth(0), m_nHeight(0), m_nShownHeight(0),
	  m_nFrames(0), m_nFrame(0), m_fFrameUS(0.0), m_nStartCount(0),
	  m_bDirty(false), m_bFinished(true), m_AudioDevice(0),
	  m_pTexture(NULL), m_nTexWidth(0), m_nTexHeight(0)
{
}

SmackerVideoInst::~SmackerVideoInst()
{
	Term();
}

void SmackerVideoInst::Term()
{
	if (m_AudioDevice)
	{
		SDL_CloseAudioDevice(m_AudioDevice);
		m_AudioDevice = 0;
	}
	if (m_pSmk)
	{
		smk_close(m_pSmk);
		m_pSmk = NULL;
	}
	OnRenderTerm();
	m_bFinished = true;
}

LTRESULT SmackerVideoInst::InitVideo(const char *pFilename, uint32 flags)
{
	(void)flags;	// PLAYBACK_FULLSCREEN is the only one the games pass, see DrawVideo

	m_pSmk = smk_open_file(pFilename, SMK_MODE_DISK);
	if (!m_pSmk)
	{
		dsi_ConsolePrint("Smacker: cannot open %s", pFilename);
		RETURN_ERROR(1, SmackerVideoInst::InitVideo, LT_ERROR);
	}

	unsigned long nW = 0, nH = 0, nFrame = 0, nFrames = 0;
	unsigned char yScale = SMK_FLAG_Y_NONE;
	double fUS = 0.0;
	smk_info_all(m_pSmk, &nFrame, &nFrames, &fUS);
	smk_info_video(m_pSmk, &nW, &nH, &yScale);
	if (!nW || !nH || !nFrames)
	{
		dsi_ConsolePrint("Smacker: %s has no video (%lux%lu, %lu frames)", pFilename, nW, nH, nFrames);
		Term();
		RETURN_ERROR(1, SmackerVideoInst::InitVideo, LT_ERROR);
	}
	m_nWidth = (uint32)nW;
	m_nHeight = (uint32)nH;
	m_nFrames = (uint32)nFrames;
	m_fFrameUS = fUS > 0.0 ? fUS : 100000.0;
	m_nShownHeight = yScale == SMK_FLAG_Y_NONE ? m_nHeight : m_nHeight * 2;

	// The video and the first audio track
	unsigned char trackMask = 0, channels[7] = {}, bitDepth[7] = {};
	unsigned long rate[7] = {};
	smk_info_audio(m_pSmk, &trackMask, channels, bitDepth, rate);
	bool bAudio = (trackMask & SMK_AUDIO_TRACK_0) != 0 && rate[0] && channels[0];
	smk_enable_all(m_pSmk, SMK_VIDEO_TRACK | (bAudio ? SMK_AUDIO_TRACK_0 : 0));

	// The sound plays on its own SDL device, queued a frame at a time.
	if (bAudio)
	{
		if (!SDL_WasInit(SDL_INIT_AUDIO))
			SDL_InitSubSystem(SDL_INIT_AUDIO);

		SDL_AudioSpec want, have;
		SDL_zero(want);
		want.freq = (int)rate[0];
		want.format = bitDepth[0] == 16 ? AUDIO_S16LSB : AUDIO_U8;
		want.channels = channels[0];
		want.samples = 2048;
		m_AudioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
		if (!m_AudioDevice)
			dsi_ConsolePrint("Smacker: no audio for %s (%s)", pFilename, SDL_GetError());
	}

	if (smk_first(m_pSmk) == SMK_ERROR)
	{
		dsi_ConsolePrint("Smacker: cannot decode %s", pFilename);
		Term();
		RETURN_ERROR(1, SmackerVideoInst::InitVideo, LT_ERROR);
	}

	m_nFrame = 0;
	m_bDirty = true;
	m_bFinished = false;
	QueueAudio();
	if (m_AudioDevice)
		SDL_PauseAudioDevice(m_AudioDevice, 0);
	m_nStartCount = SDL_GetPerformanceCounter();
	return LT_OK;
}

void SmackerVideoInst::QueueAudio()
{
	if (!m_AudioDevice)
		return;
	unsigned long n = smk_get_audio_size(m_pSmk, 0);
	const unsigned char *p = smk_get_audio(m_pSmk, 0);
	if (p && n)
		SDL_QueueAudio(m_AudioDevice, p, (Uint32)n);
}

// Frames follow the clock from the first frame, and the sound is queued as each frame decodes, so the two stay together.
// After a hitch the picture catches up to the sound
LTRESULT SmackerVideoInst::Update()
{
	if (m_bFinished || !m_pSmk)
		return GetVideoStatus();

	double fElapsedUS = (double)(SDL_GetPerformanceCounter() - m_nStartCount) * 1000000.0
		/ (double)SDL_GetPerformanceFrequency();
	uint32 nTarget = (uint32)(fElapsedUS / m_fFrameUS);

	while (m_nFrame < nTarget && m_nFrame + 1 < m_nFrames)
	{
		char r = smk_next(m_pSmk);
		if (r == SMK_ERROR)
		{
			m_bFinished = true;
			break;
		}
		++m_nFrame;
		m_bDirty = true;
		QueueAudio();
		if (r == SMK_DONE)
			break;
	}

	// Done once the last frame has had its time and the sound has drained, waiting at most two seconds more
	if (fElapsedUS >= m_fFrameUS * m_nFrames)
	{
		bool bDrained = !m_AudioDevice || SDL_GetQueuedAudioSize(m_AudioDevice) == 0;
		if (bDrained || fElapsedUS >= m_fFrameUS * m_nFrames + 2000000.0)
			m_bFinished = true;
	}

	// A finished video gives back its file, sound device, and texture at once.
	// It stays in the list answering VIDEO_NOTPLAYING until the game stops it, as Shogo never stops its logo
	if (m_bFinished)
		Term();

	return GetVideoStatus();
}

// LT1's VIDEO_PLAYING and VIDEO_NOTPLAYING, as Shogo and Blood 2 test != VIDEO_PLAYING.
// DShowVideoInst answers LT_OK and LT_FINISHED for NOLF2, which has no .smk movies
LTRESULT SmackerVideoInst::GetVideoStatus()
{
	return m_bFinished ? VIDEO_NOTPLAYING : VIDEO_PLAYING;
}

void SmackerVideoInst::OnRenderInit()
{
	// The texture is made on the next draw
	m_bDirty = true;
}

void SmackerVideoInst::OnRenderTerm()
{
	if (m_pTexture)
	{
		m_pTexture->Release();
		m_pTexture = NULL;
	}
}

void SmackerVideoInst::Release()
{
	if (m_pMgr->m_Videos.FindElement(this) != BAD_INDEX)
		m_pMgr->m_Videos.RemoveAt(&m_Link);

	LT_MEM_TRACK_FREE(delete this);
}

// The decoded frame, through its palette, into the texture
bool SmackerVideoInst::UploadFrame()
{
	IDirect3DDevice9 *pDevice = r_GetRenderStruct()->GetD3DDevice();
	if (!pDevice)
		return false;

	if (!m_pTexture)
	{
		D3DCAPS9 caps;
		ZeroMemory(&caps, sizeof(caps));
		pDevice->GetDeviceCaps(&caps);
		m_nTexWidth = m_nWidth;
		m_nTexHeight = m_nHeight;
		if (caps.TextureCaps & D3DPTEXTURECAPS_POW2)
		{
			for (m_nTexWidth = 1; m_nTexWidth < m_nWidth; m_nTexWidth <<= 1) {}
			for (m_nTexHeight = 1; m_nTexHeight < m_nHeight; m_nTexHeight <<= 1) {}
		}
		if (FAILED(pDevice->CreateTexture(m_nTexWidth, m_nTexHeight, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED,
										  &m_pTexture, NULL)))
		{
			m_pTexture = NULL;
			return false;
		}
		m_bDirty = true;
	}

	if (!m_bDirty)
		return true;

	const unsigned char *pPal = smk_get_palette(m_pSmk);
	const unsigned char *pSrc = smk_get_video(m_pSmk);
	if (!pPal || !pSrc)
		return true;

	uint32 aColors[256];
	for (uint32 i = 0; i < 256; ++i)
		aColors[i] = 0xFF000000u | ((uint32)pPal[i * 3] << 16) | ((uint32)pPal[i * 3 + 1] << 8) | pPal[i * 3 + 2];

	D3DLOCKED_RECT lr;
	if (FAILED(m_pTexture->LockRect(0, &lr, NULL, 0)))
		return true;
	for (uint32 y = 0; y < m_nHeight; ++y)
	{
		uint32 *pDst = (uint32 *)((uint8 *)lr.pBits + y * lr.Pitch);
		const unsigned char *pRow = pSrc + y * m_nWidth;
		for (uint32 x = 0; x < m_nWidth; ++x)
			pDst[x] = aColors[pRow[x]];
	}
	m_pTexture->UnlockRect(0);
	m_bDirty = false;
	return true;
}

// Fitted to the screen with its shape kept, centered and the rest black, as DShowVideoInst does.
LTRESULT SmackerVideoInst::DrawVideo()
{
	if (m_bFinished || !m_pSmk)
		return LT_OK;
	if (!r_IsRenderInitted())
	{
		RETURN_ERROR(1, SmackerVideoInst::DrawVideo, LT_NOTINITIALIZED);
	}

	IDirect3DDevice9 *pDevice = r_GetRenderStruct()->GetD3DDevice();
	if (!pDevice || !UploadFrame() || !m_pTexture)
		return LT_OK;

	bool bSet3DState = !r_GetRenderStruct()->IsIn3D();
	if (bSet3DState)
	{
		r_GetRenderStruct()->Start3D();

		D3DVIEWPORT9 vp;
		vp.X = 0;
		vp.Y = 0;
		vp.Width = r_GetRenderStruct()->m_Width;
		vp.Height = r_GetRenderStruct()->m_Height;
		vp.MinZ = 0.0f;
		vp.MaxZ = 1.0f;
		pDevice->SetViewport(&vp);

		LTRGBColor Color;
		Color.dwordVal = 0x00000000;
		r_GetRenderStruct()->Clear(NULL, CLEARSCREEN_SCREEN, Color);
	}

	uint32 nScreenWidth = r_GetRenderStruct()->m_Width;
	uint32 nScreenHeight = r_GetRenderStruct()->m_Height;
	uint32 nWidth = nScreenWidth;
	uint32 nHeight = nWidth * m_nShownHeight / m_nWidth;
	if (nHeight > nScreenHeight)
	{
		nHeight = nScreenHeight;
		nWidth = nHeight * m_nWidth / m_nShownHeight;
	}
	float fLeft = (float)((nScreenWidth - nWidth) / 2);
	float fTop = (float)((nScreenHeight - nHeight) / 2);
	float fRight = fLeft + (float)nWidth;
	float fBottom = fTop + (float)nHeight;

	float fHalfU = 0.5f / (float)m_nTexWidth;
	float fHalfV = 0.5f / (float)m_nTexHeight;
	float fTexRight = (float)m_nWidth / (float)m_nTexWidth - fHalfU;
	float fTexBottom = (float)m_nHeight / (float)m_nTexHeight - fHalfV;

	SSmackerVertex Verts[4];
	Verts[0].Init(fLeft, fTop, fHalfU, fHalfV);
	Verts[1].Init(fRight, fTop, fTexRight, fHalfV);
	Verts[2].Init(fRight, fBottom, fTexRight, fTexBottom);
	Verts[3].Init(fLeft, fBottom, fHalfU, fTexBottom);

	DWORD nMin = 0, nMag = 0;
	pDevice->GetSamplerState(0, D3DSAMP_MINFILTER, &nMin);
	pDevice->GetSamplerState(0, D3DSAMP_MAGFILTER, &nMag);
	pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

	pDevice->SetTexture(0, m_pTexture);
	pDevice->SetVertexShader(NULL);
	pDevice->SetFVF(SMACKERVERTEX_FVF);
	pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
	pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
	pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	pDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, Verts, sizeof(SSmackerVertex));

	pDevice->SetTexture(0, NULL);
	pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, nMin);
	pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, nMag);

	if (bSet3DState)
		r_GetRenderStruct()->End3D();

	return LT_OK;
}
