#include "bdefs.h"

#include "music_imaseq.h"

#include "imaseq.h"

#include <dmusic_tsf.h>
#include <tsf.h>

#include <math.h>
#include <string.h>

#include <algorithm>


namespace
{

// MSynth25's channel gain: volume and expression each squared, then multiplied
float MSynthGain(uint8 nVolume, uint8 nExpression)
{
    const float v = nVolume / 127.0f, e = nExpression / 127.0f;
    return (v * v) * (e * e);
}

// MSynth25's note velocity, squared like CC7. TinySoundFont is linear in velocity
float MSynthVelocity(uint8 nVelocity)
{
    const float v = nVelocity / 127.0f;
    return v * v;
}

struct ChannelState
{
    uint8  m_nVolume;
    uint8  m_nExpression;
    uint8  m_nBankMSB;
    uint8  m_nBankLSB;
    bool   m_bMissing;          // the last program change found no preset
    bool   m_bReported;
};

int16 ToPcm16(float f, uint32 &nLimited)
{
    if (f > 1.0f)  { f = 1.0f;  ++nLimited; }
    if (f < -1.0f) { f = -1.0f; ++nLimited; }
    return (int16)(f * 32767.0f);
}

}  // namespace


LTRESULT music_RenderImaProgram(const imaseq::Program &prog,
                                const std::vector<uint8_t> &dls,
                                float fGain,
                                uint8 **ppImage, uint32 *pnImageSize,
                                MusicRenderInfo *pInfo, ImaRenderStats *pStats,
                                uint32 nSampleRate)
{
    if (!ppImage || !pnImageSize || dls.empty() || !prog.m_bOk) return LT_INVALIDPARAMS;
    if (nSampleRate < 8000 || nSampleRate > 96000) return LT_INVALIDPARAMS;

    *ppImage = LTNULL;
    *pnImageSize = 0;
    if (pInfo) memset(pInfo, 0, sizeof(*pInfo));
    ImaRenderStats stats;
    memset(&stats, 0, sizeof(stats));

    tsf *pSynth = LTNULL;
    if (DmDls_createTsf(&dls[0], dls.size(), &pSynth) != DmResult_SUCCESS || !pSynth)
        return LT_ERROR;
    tsf_set_output(pSynth, TSF_STEREO_INTERLEAVED, (int)nSampleRate, 0);

    // The wave is the section's own length from its own tick 0, so sections play back to back on time.
    // A note dated before tick 0 plays at the start, and a note held past the end is cut
    const uint32 nFrames = (uint32)((uint64)imaseq::TickToMilliseconds(prog, prog.m_nDurationTicks) *
                                    nSampleRate / 1000);
    if (!nFrames)
    {
        tsf_close(pSynth);
        return LT_ERROR;
    }

    float *pPcm = LTNULL;
    LT_MEM_TRACK_ALLOC(pPcm = new float[nFrames * 2], LT_MEM_TYPE_MUSIC);
    if (!pPcm)
    {
        tsf_close(pSynth);
        return LT_OUTOFMEMORY;
    }
    memset(pPcm, 0, sizeof(float) * nFrames * 2);

    ChannelState ch[16];
    for (int i = 0; i < 16; ++i)
    {
        ch[i].m_nVolume = 100;          // MIDI's power-on default
        ch[i].m_nExpression = 127;
        ch[i].m_nBankMSB = 0;
        ch[i].m_nBankLSB = 0;
        ch[i].m_bMissing = false;
        ch[i].m_bReported = false;
        tsf_channel_set_volume(pSynth, i, MSynthGain(100, 127));
    }

    // Dispatch order within a tick is kept; MIDI time is all that is sorted.
    std::vector<imaseq::Event> ev(prog.m_Events);
    std::stable_sort(ev.begin(), ev.end(),
                     [](const imaseq::Event &a, const imaseq::Event &b) { return a.m_nTick < b.m_nTick; });

    uint32 nDone = 0;
    for (size_t i = 0; i < ev.size(); ++i)
    {
        const imaseq::Event &e = ev[i];
        // The sequencer's ticks carry its preroll. Negative times clamp to 0
        uint32 nAt = (uint32)((uint64)imaseq::TickToMilliseconds(prog, e.m_nTick - prog.m_nPrerollTicks) *
                              nSampleRate / 1000);
        if (nAt >= nFrames)
        {
            if ((e.m_Status & 0xF0) == 0x80) ++stats.m_nDroppedOffs;
            continue;
        }
        if (nAt > nDone)
        {
            tsf_render_float(pSynth, pPcm + nDone * 2, (int)(nAt - nDone), 0, 1.0f);
            nDone = nAt;
        }

        const int c = e.m_Status & 0x0F;
        switch (e.m_Status & 0xF0)
        {
        case 0x90:
            // A band names a program for every channel, played or not, so a miss only counts on a note
            if (ch[c].m_bMissing && !ch[c].m_bReported)
            {
                ++stats.m_nPresetMisses;
                ch[c].m_bReported = true;
            }
            tsf_channel_note_on(pSynth, c, e.m_Data1, MSynthVelocity(e.m_Data2));
            ++stats.m_nNoteOns;
            break;
        case 0x80:
            tsf_channel_note_off(pSynth, c, e.m_Data1);
            break;
        case 0xB0:
            if (e.m_Data1 == 7 || e.m_Data1 == 11)
            {
                if (e.m_Data1 == 7) ch[c].m_nVolume = e.m_Data2 & 0x7F;
                else                ch[c].m_nExpression = e.m_Data2 & 0x7F;
                tsf_channel_set_volume(pSynth, c, MSynthGain(ch[c].m_nVolume, ch[c].m_nExpression));
            }
            else if (e.m_Data1 == 0)
                ch[c].m_nBankMSB = e.m_Data2 & 0x7F;
            else if (e.m_Data1 == 32)
                ch[c].m_nBankLSB = e.m_Data2 & 0x7F;
            else
                tsf_channel_midi_control(pSynth, c, e.m_Data1, e.m_Data2 & 0x7F);
            break;
        case 0xC0:
            // The DLS conversion files a melodic instrument at bank (MSB << 8) | LSB
            ch[c].m_bMissing = !tsf_channel_set_bank_preset(pSynth, c,
                                                            (ch[c].m_nBankMSB << 8) | ch[c].m_nBankLSB,
                                                            e.m_Data1);
            ch[c].m_bReported = false;
            break;
        case 0xE0:
            tsf_channel_set_pitchwheel(pSynth, c, (e.m_Data1 & 0x7F) | ((e.m_Data2 & 0x7F) << 7));
            break;
        default:
            break;
        }
    }
    if (nDone < nFrames)
        tsf_render_float(pSynth, pPcm + nDone * 2, (int)(nFrames - nDone), 0, 1.0f);
    tsf_close(pSynth);

    float fPeak = 0.0f;
    for (uint32 i = 0; i < nFrames * 2; ++i)
        fPeak = (std::max)(fPeak, (float)fabs(pPcm[i]));	// parenthesized, as windows.h defines max

    const uint32 nData = nFrames * 2 * sizeof(int16);
    const uint32 nSize = 12 + 8 + 16 + 8 + nData;
    uint8 *pImage = LTNULL;
    LT_MEM_TRACK_ALLOC(pImage = new uint8[nSize], LT_MEM_TYPE_MUSIC);
    if (!pImage)
    {
        delete[] pPcm;
        return LT_OUTOFMEMORY;
    }

    uint32 nPos = 0;
    struct Put
    {
        uint8 *p; uint32 &n;
        void U32(uint32 v) { memcpy(p + n, &v, 4); n += 4; }
        void U16(uint16 v) { memcpy(p + n, &v, 2); n += 2; }
        void Tag(const char *s) { memcpy(p + n, s, 4); n += 4; }
    } w = { pImage, nPos };

    w.Tag("RIFF");  w.U32(nSize - 8);  w.Tag("WAVE");
    w.Tag("fmt ");  w.U32(16);
    w.U16(1);                                   // WAVE_FORMAT_PCM
    w.U16(2);
    w.U32(nSampleRate);
    w.U32(nSampleRate * 2 * sizeof(int16));
    w.U16(2 * sizeof(int16));
    w.U16(16);
    w.Tag("data");  w.U32(nData);

    uint32 nLimited = 0;
    for (uint32 i = 0; i < nFrames * 2; ++i)
        w.U16((uint16)ToPcm16(pPcm[i] * fGain, nLimited));
    delete[] pPcm;

    if (pInfo)
    {
        pInfo->m_nSamples    = nFrames;
        pInfo->m_nSampleRate = nSampleRate;
        pInfo->m_fPeak       = fPeak;
        pInfo->m_nLimited    = nLimited;
    }
    if (pStats) *pStats = stats;

    *ppImage = pImage;
    *pnImageSize = nSize;
    return LT_OK;
}
