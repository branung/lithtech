#include "bdefs.h"

#include "music_segment.h"
#include "music_imaseq.h"

#include "iltclient.h"
#include "imaseq.h"

#include <string.h>

#include <chrono>
#include <ctime>
#include <map>
#include <mutex>
#include <string>
#include <vector>


static ILTClient *ilt_client;
define_holder(ILTClient, ilt_client);

extern int32 g_nMusicTrace;


static bool HasExtension(const char *pszFilename, const char *pszExt3)
{
    if (!pszFilename) return false;

    size_t n = strlen(pszFilename);
    if (n < 4) return false;

    const char *p = pszFilename + n - 4;
    if (p[0] != '.') return false;
    for (int i = 0; i < 3; ++i)
    {
        char a = p[1 + i], b = pszExt3[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}


bool music_IsScoreFilename(const char *pszFilename)
{
    return HasExtension(pszFilename, "sec");
}


namespace
{

// Reads a section's folder through the engine's file system, so a score plays from a .rez as from loose files.
// The archives' lookup is case-insensitive, and loose files on a case-sensitive file system aren't covered
class EngineFolderProvider : public imaseq::FileProvider
{
public:
    explicit EngineFolderProvider(const char *pszDirWithSeparator)
    {
        LTStrCpy(m_szDir, pszDirWithSeparator, sizeof(m_szDir));
    }

    virtual bool Read(const char *pszName, std::vector<uint8_t> &out)
    {
        if (!ilt_client || !pszName) return false;

        char szPath[1024];
        LTSNPrintF(szPath, sizeof(szPath), "%s%s", m_szDir, pszName);

        ILTStream *pStream = LTNULL;
        if (ilt_client->OpenFile(szPath, &pStream) != LT_OK || !pStream)
            return false;

        uint32 nLen = 0;
        pStream->GetLen(&nLen);
        out.resize(nLen);
        bool bOk = (nLen == 0) || (pStream->Read(&out[0], nLen) == LT_OK);
        pStream->Release();
        return bOk;
    }

    virtual bool List(std::vector<std::string> &out)
    {
        if (!ilt_client) return false;

        // GetFileList wants the directory without its trailing separator
        char szDir[512];
        LTStrCpy(szDir, m_szDir, sizeof(szDir));
        size_t n = strlen(szDir);
        while (n > 0 && (szDir[n - 1] == '\\' || szDir[n - 1] == '/'))
            szDir[--n] = '\0';

        FileEntry *pHead = ilt_client->GetFileList(szDir);
        if (!pHead) return false;

        for (FileEntry *p = pHead; p; p = p->m_pNext)
            if (p->m_pBaseFilename)
                out.push_back(p->m_pBaseFilename);

        ilt_client->FreeFileList(pHead);
        return true;
    }

private:
    char m_szDir[512];
};

// "Music\Akuma\h7.sec" -> the directory with its separator, and the bare section name.
// Either separator, as a Linux host may hand back forward slashes
const char *SplitPath(const char *pszFilename, char *pszDir, size_t nDir)
{
    LTStrCpy(pszDir, pszFilename, nDir);

    char *pLast = LTNULL;
    for (char *p = pszDir; *p; ++p)
        if (*p == '\\' || *p == '/') pLast = p;

    if (!pLast)
    {
        pszDir[0] = '\0';
        return pszFilename;
    }

    const char *pszSegment = pszFilename + (pLast - pszDir) + 1;
    pLast[1] = '\0';
    return pszSegment;
}

// One random stream for the session, as IMUSIC25 draws every section from one rand().
// Seeded once from the clock, as the DLL's DllMain does. Main thread only
imaseq::Random &SessionRandom()
{
    static imaseq::Random s_Rng;
    static bool s_bSeeded = false;
    if (!s_bSeeded)
    {
        s_Rng.m_nState = (uint32)time(LTNULL);
        s_bSeeded = true;
    }
    return s_Rng;
}

// Sequence a section with IMUSIC25's rules. Its random draws continue the session's stream
bool SequenceSection(EngineFolderProvider &provider, const char *pszSegment,
                     imaseq::Program &prog)
{
    imaseq::Options opts;
    prog = imaseq::SequenceWith(provider, pszSegment, opts, SessionRandom());
    if (!prog.m_bOk)
    {
        dsi_PrintToConsole("music: cannot sequence %s: %s", pszSegment, prog.m_sError.c_str());
        return false;
    }
    if (g_nMusicTrace)
        dsi_PrintToConsole("music: sequenced %s (%s): %u grids, %u notes, %u curve messages, "
                           "%u draws, pattern %u",
                           pszSegment, prog.m_sStyleName.c_str(), prog.m_nGrids, prog.m_nNotes,
                           prog.m_nCurveMessages, prog.m_nDraws, prog.m_nFirstPattern);
    return true;
}

void ReportRender(const char *pszSegment, const MusicRenderInfo &info, const ImaRenderStats &stats)
{
    // Distortion from the limiter would be heard as a bad synth, so it's reported
    if (info.m_nLimited)
        dsi_PrintToConsole("music: %s clipped %u samples -- kMusicDefaultGain "
                           "is too high for this score (raw peak %.3f)",
                           pszSegment, (unsigned)info.m_nLimited, info.m_fPeak);
    if (stats.m_nPresetMisses)
        dsi_PrintToConsole("music: %s asked for %u instruments its bank does not have",
                           pszSegment, (unsigned)stats.m_nPresetMisses);
}

// Uppercase with backslashes, for both sides of every lookup
std::string NormalizeKey(const char *psz)
{
    std::string s(psz ? psz : "");
    for (size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (c == '/') c = '\\';
        s[i] = c;
    }
    return s;
}

// Waves rendered ahead of time, waiting for the sound system to ask
struct Prerendered
{
    uint8  *m_pImage;
    uint32  m_nSize;
};

std::mutex                          g_PrerenderedLock;
std::map<std::string, Prerendered>  g_Prerendered;

// The bank is 2-3 MB and the same file for every section in a folder, so it's read once per folder.
// Main thread only
std::string           g_sDlsKey;
std::vector<uint8_t>  g_Dls;

uint32 NowMS()
{
    using namespace std::chrono;
    return (uint32)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace


struct MusicRenderJob
{
    // Filled by prepare, on the main thread
    std::string           m_sPath;          // as the caller named it
    std::string           m_sSegment;       // bare, e.g. "c3.sec"
    imaseq::Program       m_Prog;           // the sequenced events
    std::vector<uint8_t>  m_Dls;            // the bank the band names
    uint32                m_nPrepMS;

    // Filled by run, on whichever thread ran it
    LTRESULT              m_Result;
    uint8                *m_pImage;
    uint32                m_nImageSize;
    MusicRenderInfo       m_Info;
    ImaRenderStats        m_ImaStats;
    uint32                m_nRenderMS;

    MusicRenderJob() : m_nPrepMS(0), m_Result(LT_ERROR), m_pImage(LTNULL), m_nImageSize(0), m_nRenderMS(0)
    {
        memset(&m_Info, 0, sizeof(m_Info));
        memset(&m_ImaStats, 0, sizeof(m_ImaStats));
    }

    ~MusicRenderJob() { delete[] m_pImage; }
};


MusicRenderJob *music_PrepareRender(const char *pszFilename)
{
    if (!pszFilename || !ilt_client) return LTNULL;

    const uint32 nT0 = NowMS();

    char szDir[512];
    const char *pszSegment = SplitPath(pszFilename, szDir, sizeof(szDir));
    if (!music_IsScoreFilename(pszSegment))
        return LTNULL;

    MusicRenderJob *pJob = new MusicRenderJob;
    pJob->m_sPath    = pszFilename;
    pJob->m_sSegment = pszSegment;

    // Sequencing reads through the engine and draws from the session's stream, so it's done here
    EngineFolderProvider provider(szDir);
    if (!SequenceSection(provider, pszSegment, pJob->m_Prog))
    {
        delete pJob;
        return LTNULL;
    }

    const std::string &sDlsName = pJob->m_Prog.m_sDlsName;
    std::string sKey = NormalizeKey(szDir) + NormalizeKey(sDlsName.c_str());
    if (sKey != g_sDlsKey)
    {
        g_Dls.clear();
        g_sDlsKey.clear();
        if (!provider.Read(sDlsName.c_str(), g_Dls) || g_Dls.empty())
        {
            dsi_PrintToConsole("music: cannot read %s%s for %s",
                               szDir, sDlsName.c_str(), pszSegment);
            g_Dls.clear();
            delete pJob;
            return LTNULL;
        }
        g_sDlsKey = sKey;
    }
    pJob->m_Dls = g_Dls;

    pJob->m_nPrepMS = NowMS() - nT0;
    return pJob;
}


void music_RunRender(MusicRenderJob *pJob)
{
    if (!pJob) return;

    const uint32 nT0 = NowMS();
    pJob->m_Result = LT_ERROR;

    // Nothing may throw past the thread boundary, as that would end the process
    try
    {
        pJob->m_Result = music_RenderImaProgram(pJob->m_Prog, pJob->m_Dls, kMusicDefaultGain,
                                                &pJob->m_pImage, &pJob->m_nImageSize,
                                                &pJob->m_Info, &pJob->m_ImaStats);
    }
    catch (...)
    {
        delete[] pJob->m_pImage;
        pJob->m_pImage = LTNULL;
        pJob->m_nImageSize = 0;
        pJob->m_Result = LT_OUTOFMEMORY;
    }

    pJob->m_nRenderMS = NowMS() - nT0;
}


bool music_CommitRender(MusicRenderJob *pJob)
{
    if (!pJob) return false;

    if (pJob->m_Result != LT_OK || !pJob->m_pImage)
    {
        // Not reported here, as the synchronous render at play time fails the same way and says so
        delete pJob;
        return false;
    }

    ReportRender(pJob->m_sSegment.c_str(), pJob->m_Info, pJob->m_ImaStats);

    Prerendered pre;
    pre.m_pImage = pJob->m_pImage;
    pre.m_nSize  = pJob->m_nImageSize;
    pJob->m_pImage = LTNULL;                 // ownership moves to the map

    {
        std::lock_guard<std::mutex> lock(g_PrerenderedLock);
        std::string sKey = NormalizeKey(pJob->m_sPath.c_str());
        std::map<std::string, Prerendered>::iterator it = g_Prerendered.find(sKey);
        if (it != g_Prerendered.end())
        {
            delete[] it->second.m_pImage;    // rendered twice, keep the newer
            it->second = pre;
        }
        else
        {
            g_Prerendered[sKey] = pre;
        }
    }

    delete pJob;
    return true;
}


void music_DiscardRender(MusicRenderJob *pJob)
{
    delete pJob;
}


void music_ClearPrerendered()
{
    std::lock_guard<std::mutex> lock(g_PrerenderedLock);
    for (std::map<std::string, Prerendered>::iterator it = g_Prerendered.begin();
         it != g_Prerendered.end(); ++it)
        delete[] it->second.m_pImage;
    g_Prerendered.clear();
}


void music_GetRenderTimes(const MusicRenderJob *pJob, uint32 *pnPrepMS, uint32 *pnRenderMS)
{
    if (pnPrepMS)   *pnPrepMS   = pJob ? pJob->m_nPrepMS   : 0;
    if (pnRenderMS) *pnRenderMS = pJob ? pJob->m_nRenderMS : 0;
}


LTRESULT music_RenderSegmentStream(const char *pszFilename,
                                   uint8 **ppImage, uint32 *pnImageSize)
{
    if (!pszFilename || !ppImage || !pnImageSize) return LT_INVALIDPARAMS;

    // A wave rendered ahead of time on the worker is handed over whole
    {
        std::lock_guard<std::mutex> lock(g_PrerenderedLock);
        std::map<std::string, Prerendered>::iterator it =
            g_Prerendered.find(NormalizeKey(pszFilename));
        if (it != g_Prerendered.end())
        {
            *ppImage     = it->second.m_pImage;
            *pnImageSize = it->second.m_nSize;
            g_Prerendered.erase(it);
            return LT_OK;
        }
    }

    char szDir[512];
    const char *pszSegment = SplitPath(pszFilename, szDir, sizeof(szDir));
    if (!music_IsScoreFilename(pszSegment))
        return LT_ERROR;

    EngineFolderProvider provider(szDir);
    imaseq::Program prog;
    if (!SequenceSection(provider, pszSegment, prog))
        return LT_ERROR;

    std::vector<uint8_t> dls;
    if (!provider.Read(prog.m_sDlsName.c_str(), dls) || dls.empty())
    {
        dsi_PrintToConsole("music: cannot read %s%s for %s",
                           szDir, prog.m_sDlsName.c_str(), pszSegment);
        return LT_ERROR;
    }

    MusicRenderInfo info;
    ImaRenderStats stats;
    LTRESULT r = music_RenderImaProgram(prog, dls, kMusicDefaultGain, ppImage, pnImageSize,
                                        &info, &stats);
    if (r != LT_OK)
        return r;

    ReportRender(pszSegment, info, stats);
    return LT_OK;
}
