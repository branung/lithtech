#ifndef __MUSIC_IMASEQ_H__
#define __MUSIC_IMASEQ_H__

// Renders a section libs/IMASeq has sequenced into a RIFF WAVE image, through
// TinySoundFont built from the folder's own DLS bank (dmusic's DmDls_createTsf).

#ifndef __LTBASETYPES_H__
#include "ltbasetypes.h"
#endif

#include <cstdint>
#include <vector>

namespace imaseq { struct Program; }

// -6 dB, applied to every section alike so the score's own dynamics are kept.
// The limiter reports a score this is too loud for
const float kMusicDefaultGain = 0.5f;

struct MusicRenderInfo
{
    uint32  m_nSamples;        // per channel
    uint32  m_nSampleRate;
    float   m_fPeak;           // peak of the raw render, before gain
    uint32  m_nLimited;        // samples the limiter caught after gain
};

// What the render saw, beyond MusicRenderInfo
struct ImaRenderStats
{
    uint32  m_nNoteOns;
    uint32  m_nPresetMisses;     // channels asked to play a program the bank lacks
    uint32  m_nDroppedOffs;      // note-offs past the end of the wave
};

LTRESULT music_RenderImaProgram(const imaseq::Program &prog,
                                const std::vector<uint8_t> &dls,
                                float fGain,
                                uint8 **ppImage, uint32 *pnImageSize,
                                MusicRenderInfo *pInfo, ImaRenderStats *pStats,
                                uint32 nSampleRate = 44100);

#endif  // __MUSIC_IMASEQ_H__
