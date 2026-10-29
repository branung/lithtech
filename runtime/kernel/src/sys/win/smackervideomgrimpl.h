// Smacker (.smk) video, the LT1 games' movie format, decoded by libsmacker and drawn through the D3D9 device as DShowVideoInst draws.

#ifndef __SMACKERVIDEOMGRIMPL_H__
#define __SMACKERVIDEOMGRIMPL_H__

#ifndef __VIDEOMGR_H__
#include "videomgr.h"
#endif
#include "dshowvideomgrimpl.h"

struct IDirect3DTexture9;
struct smk_t;

class SmackerVideoMgr : public DShowVideoMgr
{
public:
	// A .smk is played here, and anything else goes to DirectShow
	virtual LTRESULT	CreateScreenVideo(const char *pFilename, uint32 flags, VideoInst* &pVideo);
};


class SmackerVideoInst : public VideoInst
{
public:
						SmackerVideoInst(SmackerVideoMgr *pMgr);

	LTRESULT			InitVideo(const char *pFilename, uint32 flags);

	virtual void		Release();
	virtual void		OnRenderInit();
	virtual void		OnRenderTerm();
	virtual LTRESULT	Update();
	virtual LTRESULT	DrawVideo();
	virtual LTRESULT	GetVideoStatus();

protected:
	virtual				~SmackerVideoInst();

private:
	void				Term();
	void				QueueAudio();
	bool				UploadFrame();

	SmackerVideoMgr		*m_pMgr;
	smk_t				*m_pSmk;

	uint32				m_nWidth, m_nHeight;	// The decoded frame
	uint32				m_nShownHeight;			// With the file's line doubling
	uint32				m_nFrames;
	uint32				m_nFrame;				// The frame decoded now
	double				m_fFrameUS;				// Microseconds per frame

	uint64				m_nStartCount;			// SDL performance counter at frame 0
	bool				m_bDirty;				// Decoded but not yet in the texture
	bool				m_bFinished;

	uint32				m_AudioDevice;			// SDL_AudioDeviceID, 0 for none

	IDirect3DTexture9	*m_pTexture;
	uint32				m_nTexWidth, m_nTexHeight;
};

#endif
