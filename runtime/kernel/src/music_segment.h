#ifndef __MUSIC_SEGMENT_H__
#define __MUSIC_SEGMENT_H__

// The sound system's side of LT1 music

#ifndef __GENLTSTREAM_H__
#include "genltstream.h"
#endif

// An LT1 IMA score section (.sec)
bool music_IsScoreFilename(const char *pszFilename);

// Renders pszFilename, a path such as "Music\\Akuma\\h7.sec", into a RIFF WAVE image.
// Caller owns *ppImage; hand it to CMusicSegmentStream::Init, which takes ownership
LTRESULT music_RenderSegmentStream(const char *pszFilename,
                                   uint8 **ppImage, uint32 *pnImageSize);


// A read only stream over the rendered wave. Release() is empty, as LoadData
// releases its stream on several paths and this one lives on the stack. The destructor owns the image
class CMusicSegmentStream : public CGenLTStream
{
public:

    CMusicSegmentStream() : m_pData(LTNULL), m_Len(0), m_Pos(0), m_Status(LT_OK) {}
    ~CMusicSegmentStream() { delete[] m_pData; }

    // Takes ownership of pData
    void Init(uint8 *pData, uint32 len)
    {
        delete[] m_pData;
        m_pData = pData;
        m_Len = len;
        m_Pos = 0;
        m_Status = LT_OK;
    }

    virtual void Release() {}

    virtual LTRESULT Read(void *pOut, uint32 size)
    {
        if (!m_pData || m_Pos + size > m_Len)
        {
            memset(pOut, 0, size);
            m_Status = LT_ERROR;
            return LT_ERROR;
        }
        memcpy(pOut, m_pData + m_Pos, size);
        m_Pos += size;
        return LT_OK;
    }

    virtual LTRESULT Write(const void *, uint32)  { return LT_ERROR; }
    virtual LTRESULT ErrorStatus()                { return m_Status; }
    virtual LTRESULT GetPos(uint32 *pPos)         { *pPos = m_Pos; return LT_OK; }
    virtual LTRESULT GetLen(uint32 *pLen)         { *pLen = m_Len; return LT_OK; }

    virtual LTRESULT SeekTo(uint32 offset)
    {
        if (offset > m_Len)
        {
            m_Status = LT_ERROR;
            return LT_ERROR;
        }
        m_Pos = offset;
        return LT_OK;
    }

private:

    uint8    *m_pData;
    uint32    m_Len;
    uint32    m_Pos;
    LTRESULT  m_Status;
};


// Rendering off the main thread, so a section's render isn't paid on a frame.
//   PREPARE  main thread   reads every file the render needs through the engine and sequences the section
//   RUN      any thread    renders from that memory. It touches nothing but the job
//   COMMIT   main thread   files the wave for the sound system to find

struct MusicRenderJob;

// Main thread. NULL if the file isn't a score or can't be sequenced, and the reason is printed here
MusicRenderJob *music_PrepareRender(const char *pszFilename);

// Any thread. Touches only the job and never throws
void music_RunRender(MusicRenderJob *pJob);

// Main thread. Files the wave so the next music_RenderSegmentStream for this filename answers from memory,
// and frees the job. False if the render failed, and the synchronous path then reports it at play time
bool music_CommitRender(MusicRenderJob *pJob);

// Main thread. Frees a job whose result isn't wanted
void music_DiscardRender(MusicRenderJob *pJob);

// Main thread. Drops every filed wave nobody has asked for yet, at level teardown
void music_ClearPrerendered();

// For MusicTrace: how long the job's prepare and render took, in ms
void music_GetRenderTimes(const MusicRenderJob *pJob, uint32 *pnPrepMS, uint32 *pnRenderMS);

#endif  // __MUSIC_SEGMENT_H__
