#include "bdefs.h"

#include "music_ima.h"
#include "music_segment.h"

#include "iltclient.h"
#include "iltsoundmgr.h"
#include "client_filemgr.h"
#include "timemgr.h"
#include "soundmgr.h"
#include "ltbasedefs.h"

#include <string.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

// From engine_vars.cpp. One line per section started when set
extern int32 g_nMusicTrace;

static ILTClientSoundMgr *ilt_sound;
define_holder(ILTClientSoundMgr, ilt_sound);

static ILTClient *ilt_client;
define_holder(ILTClient, ilt_client);

static IClientFileMgr *client_file_mgr;
define_holder(IClientFileMgr, client_file_mgr);

// The music volume goes through a sound volume class, so the slider changes what's already playing.
// The top class, as NOLF2 numbers its classes from 0 and never loads this driver anyway
static const uint8 kMusicVolumeClass = MAX_SOUND_VOLUME_CLASSES;

// LT1's volume range, from musicmgr.h: 100 is full
static const uint16 kMusicFullVolume = 100;

namespace
{

// A song is a filename. Nothing is preloaded, as a level creates about ninety and most are never reached.
// A section is rendered when it's first played, and the sound system's cache makes later plays free
struct Song
{
    std::string m_sName;        // bare, e.g. "h7.sec"
};

struct PlayList
{
    std::string          m_sName;
    std::vector<Song *>  m_Songs;

    // On the list, as LT1 keeps it there. The driver re-enters PlayList at the wrap with the list's flag
    bool                 m_bLoop;

    PlayList() : m_bLoop(false) {}
};

class ImaDriver
{
public:

    ImaDriver()
        : m_bInited(false), m_bPaused(false), m_nVolume(kMusicFullVolume),
          m_hSound(LTNULL), m_pCurList(LTNULL), m_pCurSong(LTNULL),
          m_pTransition(LTNULL),
          m_nNextIndex(0), m_bStopAfterCurrent(false),
          m_nLastStartMS(0), m_nLastDurMS(0), m_bWarmed(false),
          m_pJob(LTNULL), m_bJobDone(false),
          m_pPendingList(LTNULL), m_pPendingTransition(LTNULL),
          m_bPendingLoop(false), m_bHavePending(false)
    {
        m_szDir[0] = '\0';
    }

    ~ImaDriver()
    {
        // Runs at process exit. A worker still rendering would write into a freed job, so wait for it
        JoinWorker();
    }

    // ------------------------------------------------------------- lifetime --

    bool Init(SMusicMgr *pMgr)
    {
        m_bInited = true;
        m_bPaused = false;
        m_nVolume = kMusicFullVolume;
        ApplyVolume();

        pMgr->m_bValid   = 1;
        pMgr->m_bEnabled = 1;

        // Not motifs: neither game plays one, and claiming them would make ci_PlayMotif call a stub instead of refusing
        pMgr->m_ulCaps = MUSIC_VOLUMECONTROL | MUSIC_INSTRUMENTSET;
        return true;
    }

    void Term()
    {
        JoinWorker();
        StopSound();
        DestroyAllSongs();
        m_bInited = false;
    }

    // ---------------------------------------------------------------- setup --

    bool SetDataDirectory(const char *pszDir)
    {
        if (!pszDir) return false;
        LTStrCpy(m_szDir, pszDir, sizeof(m_szDir));

        // Trim a trailing separator, so the join below is the only place one is added
        size_t n = strlen(m_szDir);
        while (n > 0 && (m_szDir[n - 1] == '\\' || m_szDir[n - 1] == '/'))
            m_szDir[--n] = '\0';

        return true;
    }

    bool InitInstruments(const char *pszDLS, const char *pszStyle)
    {
        // The sequencer finds the bank through the section's own style.
        // This is the one file the driver learns about before playing anything, so it's the probe below
        if (!pszDLS || !pszStyle) return false;
        m_sDLS   = pszDLS;
        m_sStyle = pszStyle;

        return EnsureReachable(pszDLS);
    }

    // Can the engine's file system see this level's music, and if not, mount it.
    // LT1's music is loose on disk and in no archive, since ima.dll read it with fopen,
    // and Jupiter's file manager only searches mounted trees.
    // The path is tried as authored first, so music packed into a .rez plays from there
    bool EnsureReachable(const char *pszProbe)
    {
        if (CanOpen(m_szDir, pszProbe)) return true;

        // Mount the first component ("music" for "music\Inform") rather than the game directory,
        // which would put every loose file ahead of every archive
        char szRoot[256], szRest[256];
        SplitRoot(m_szDir, szRoot, sizeof(szRoot), szRest, sizeof(szRest));

        if (szRoot[0] && MountOnce(szRoot) && CanOpen(szRest, pszProbe))
        {
            LTStrCpy(m_szDir, szRest, sizeof(m_szDir));
            return true;
        }

        dsi_PrintToConsole("music: cannot reach %s in %s - no music this level. "
                           "LT1 keeps its music LOOSE and in no archive, so the "
                           "game directory needs a Music folder copied from the "
                           "retail install beside the .rez files.",
                           pszProbe, m_szDir);
        return false;
    }

    // ----------------------------------------------------------- song store --

    void *CreateSong(const char *pszName)
    {
        if (!pszName || !pszName[0]) return NULL;

        Song *pSong = new Song;
        pSong->m_sName = pszName;
        m_Songs.push_back(pSong);
        return pSong;
    }

    void *GetSong(const char *pszName)
    {
        if (!pszName) return NULL;
        for (size_t i = 0; i < m_Songs.size(); ++i)
            if (SameNameCI(pszName, m_Songs[i]->m_sName))
                return m_Songs[i];
        return NULL;
    }

    void DestroySong(void *pSongV)
    {
        Song *pSong = (Song *)pSongV;
        if (!pSong) return;

        // The playlists hold raw pointers, so a destroyed song leaves none behind
        ForgetSong(pSong);

        for (size_t i = 0; i < m_Songs.size(); ++i)
            if (m_Songs[i] == pSong)
            {
                m_Songs.erase(m_Songs.begin() + i);
                break;
            }
        delete pSong;
    }

    void DestroyAllSongs()
    {
        // The only teardown either game does, so the playlists go too.
        // Otherwise the next level finds its lists by name and plays the last level's sections.
        // A render in flight is for this level: wait for it, then drop it and any wave never asked for
        JoinWorker();
        music_ClearPrerendered();

        StopSound();
        m_pCurList = NULL;
        m_pTransition = NULL;
        m_pCurSong = NULL;
        ClearPending();

        for (size_t i = 0; i < m_Lists.size(); ++i) delete m_Lists[i];
        m_Lists.clear();

        for (size_t i = 0; i < m_Songs.size(); ++i) delete m_Songs[i];
        m_Songs.clear();
    }

    // ------------------------------------------------------------ playlists --

    void *CreatePlayList(const char *pszName)
    {
        if (!pszName || !pszName[0]) return NULL;

        PlayList *pList = new PlayList;
        pList->m_sName = pszName;
        m_Lists.push_back(pList);
        return pList;
    }

    void *GetPlayList(const char *pszName)
    {
        if (!pszName) return NULL;
        for (size_t i = 0; i < m_Lists.size(); ++i)
            if (SameNameCI(pszName, m_Lists[i]->m_sName))
                return m_Lists[i];
        return NULL;
    }

    bool AddSongToList(void *pListV, void *pSongV)
    {
        PlayList *pList = (PlayList *)pListV;
        Song     *pSong = (Song *)pSongV;
        if (!pList || !pSong) return false;

        pList->m_Songs.push_back(pSong);
        return true;
    }

    void *GetSongFromList(void *pListV, unsigned long dwIndex)
    {
        PlayList *pList = (PlayList *)pListV;
        if (!pList || dwIndex >= pList->m_Songs.size()) return NULL;
        return pList->m_Songs[dwIndex];
    }

    void RemoveList(void *pListV)
    {
        PlayList *pList = (PlayList *)pListV;
        if (!pList) return;

        if (m_pCurList == pList)     { StopSound(); m_pCurList = NULL; }
        if (m_pPendingList == pList) ClearPending();

        for (size_t i = 0; i < m_Lists.size(); ++i)
            if (m_Lists[i] == pList)
            {
                m_Lists.erase(m_Lists.begin() + i);
                break;
            }
        delete pList;
    }

    // -------------------------------------------------------------- playing --

    bool PlayBreak(void *pSongV, uint32 dwFlags)
    {
        // Neither game calls it
        Song *pSong = (Song *)pSongV;
        if (!pSong) return false;

        if (dwFlags == MUSIC_NEXTSONG && IsPlaying())
        {
            m_pTransition = pSong;
            return true;
        }

        StopSound();
        return PlaySong(pSong);
    }

    bool DoPlayList(void *pListV, void *pTransitionV, bool bLoop, uint32 dwFlags)
    {
        PlayList *pList = (PlayList *)pListV;
        if (!pList) return false;

        // MUSIC_NEXTSONG is at the next song boundary, so the change is remembered.
        // Only IMMEDIATE and NEXTSONG ever arrive from these games
        if (dwFlags == MUSIC_NEXTSONG && IsPlaying())
        {
            m_pPendingList       = pList;
            m_pPendingTransition = (Song *)pTransitionV;
            m_bPendingLoop       = bLoop;
            m_bHavePending       = true;
            m_bStopAfterCurrent  = false;

            // The next section has changed, so what was warmed is the wrong one
            m_bWarmed = false;
            return true;
        }

        StopSound();
        Begin(pList, (Song *)pTransitionV, bLoop);

        if (m_bPaused) return false;
        return StartNext();
    }

    void Stop(uint32 dwFlags)
    {
        if (dwFlags == MUSIC_NEXTSONG && IsPlaying())
        {
            m_bStopAfterCurrent = true;
            ClearPending();
            return;
        }

        StopSound();
        m_pCurList = NULL;
        m_pTransition = NULL;
        m_pCurSong = NULL;
        m_bPaused = false;      // LT1's Stop clears its paused flag too
        ClearPending();
    }

    // LT1's Pause and Resume stop and restart the same section, as ima.dll keeps no playback position.
    // Pause leaves the current song and list alone, so Resume brings back what was playing
    bool Pause(uint32)
    {
        if (m_bPaused) return true;

        m_bPaused = true;
        StopSound();
        return true;
    }

    bool Resume()
    {
        if (!m_bPaused) return true;

        m_bPaused = false;
        if (m_pCurSong)  return PlaySong(m_pCurSong);
        if (m_pCurList)  return StartNext();
        return true;
    }

    // --------------------------------------------------------------- volume --

    uint16 GetVolume() const { return m_nVolume; }

    void SetVolume(uint16 nVolume)
    {
        m_nVolume = (nVolume > kMusicFullVolume) ? kMusicFullVolume : nVolume;
        ApplyVolume();
    }

    // ----------------------------------------------------------------- tick --

    void Update()
    {
        // Before any early return, as a job finishes on its own clock
        PollWorker();

        if (!m_bInited || m_bPaused) return;
        if (!m_pCurList && !m_bHavePending) return;

        if (IsPlaying())
        {
            // Two things happen before the current section ends, or the boundary stumbles
            uint32 nElapsed = time_GetMSTime() - m_nLastStartMS;

            // (1) Render the next section early, so its cost isn't paid at the boundary.
            // A second in, or halfway through a short section
            if (!m_bWarmed && m_nLastDurMS)
            {
                uint32 nWarmAt = m_nLastDurMS / 2;
                if (nWarmAt > 1000) nWarmAt = 1000;

                // A pending change re-arms this mid-section, so warm at once
                if (m_bHavePending || nElapsed >= nWarmAt)
                {
                    // False only while an earlier job is running, so ask again next frame
                    if (WarmNext())
                        m_bWarmed = true;
                }
            }

            // (2) Start the next section when the current one is due to end, by its own duration.
            // IsSoundDone lagged 9-65 ms behind it
            if (m_nLastDurMS && nElapsed >= m_nLastDurMS)
            {
                StopSound();          // it is at its end, and this reaps the voice
            }
            else
            {
                return;
            }
        }

        // The section has ended. The only place a deferred change is applied, as MUSIC_NEXTSONG means
        if (m_bStopAfterCurrent)
        {
            m_bStopAfterCurrent = false;
            m_pCurList = NULL;
            m_pTransition = NULL;
            return;
        }

        if (m_bHavePending)
        {
            PlayList *pList       = m_pPendingList;
            Song     *pTransition = m_pPendingTransition;
            bool      bLoop       = m_bPendingLoop;
            ClearPending();
            Begin(pList, pTransition, bLoop);
        }

        StartNext();
    }

private:

    // ------------------------------------------------------------- internals --

    static bool SameNameCI(const char *a, const std::string &b)
    {
        if (!a) return false;
        size_t n = strlen(a);
        if (n != b.size()) return false;
        for (size_t i = 0; i < n; ++i)
        {
            char x = a[i], y = b[i];
            if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
            if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
            if (x != y) return false;
        }
        return true;
    }

    // "music\Inform" -> root "music", rest "Inform". A directory with no separator is its own root
    static void SplitRoot(const char *pszDir, char *pszRoot, size_t nRoot,
                          char *pszRest, size_t nRest)
    {
        pszRoot[0] = pszRest[0] = '\0';
        if (!pszDir || !pszDir[0]) return;

        const char *pSep = NULL;
        for (const char *p = pszDir; *p; ++p)
            if (*p == '\\' || *p == '/') { pSep = p; break; }

        if (!pSep)
        {
            LTStrCpy(pszRoot, pszDir, nRoot);
            return;
        }

        size_t n = (size_t)(pSep - pszDir);
        if (n >= nRoot) n = nRoot - 1;
        memcpy(pszRoot, pszDir, n);
        pszRoot[n] = '\0';

        LTStrCpy(pszRest, pSep + 1, nRest);
    }

    // A tree can't be unmounted, and one mounted twice is searched twice for the session
    bool MountOnce(const char *pszTree)
    {
        if (!client_file_mgr || !pszTree || !pszTree[0]) return false;

        for (size_t i = 0; i < m_Mounted.size(); ++i)
            if (SameNameCI(pszTree, m_Mounted[i]))
                return true;

        int nLoaded = 0;
        const char *pName = pszTree;
        client_file_mgr->AddResourceTrees(&pName, 1, NULL, &nLoaded);
        if (nLoaded < 1) return false;

        m_Mounted.push_back(pszTree);
        dsi_PrintToConsole("music: mounted %s (LT1 keeps its music loose on disk)",
                           pszTree);
        return true;
    }

    static bool CanOpen(const char *pszDir, const char *pszName)
    {
        if (!ilt_client || !pszName) return false;

        char szPath[512];
        if (pszDir && pszDir[0])
            LTSNPrintF(szPath, sizeof(szPath), "%s\\%s", pszDir, pszName);
        else
            LTStrCpy(szPath, pszName, sizeof(szPath));

        ILTStream *pStream = LTNULL;
        if (ilt_client->OpenFile(szPath, &pStream) != LT_OK || !pStream)
            return false;

        pStream->Release();
        return true;
    }

    void ForgetSong(Song *pSong)
    {
        for (size_t i = 0; i < m_Lists.size(); ++i)
        {
            std::vector<Song *> &v = m_Lists[i]->m_Songs;
            for (size_t j = v.size(); j > 0; --j)
                if (v[j - 1] == pSong) v.erase(v.begin() + (j - 1));
        }
        if (m_pTransition == pSong)        m_pTransition = NULL;
        if (m_pPendingTransition == pSong) m_pPendingTransition = NULL;
        if (m_pCurSong == pSong)           m_pCurSong = NULL;
    }

    void ClearPending()
    {
        m_pPendingList       = NULL;
        m_pPendingTransition = NULL;
        m_bPendingLoop       = false;
        m_bHavePending       = false;
    }

    // The commit half of LT1's PlayList. LT1 writes these fields before checking m_bEnabled and paused,
    // so a PlayList while music is off still changes the current list
    void Begin(PlayList *pList, Song *pTransition, bool bLoop)
    {
        m_pCurList          = pList;
        pList->m_bLoop      = bLoop;
        m_nNextIndex        = 0;
        m_bStopAfterCurrent = false;

        m_pTransition = pTransition;
        if (pTransition)
        {
            // As LT1: the index runs off the end, so after the transition the loop check decides.
            // Without loop, a transition plays and then stops
            m_nNextIndex = pList->m_Songs.size();
        }
    }

    // The next section in list order. ima.dll only ever increments its index and has no rand,
    // so its randomness is IMUSIC25's within a section, never which section plays
    size_t PickNext()
    {
        return m_nNextIndex;
    }

    // What StartNext would play now, without playing it. The two have to agree, or the wrong section is warmed
    Song *PeekNext()
    {
        if (m_bHavePending)
        {
            if (m_pPendingTransition) return m_pPendingTransition;
            if (m_pPendingList && !m_pPendingList->m_Songs.empty())
                return m_pPendingList->m_Songs[0];
            return NULL;
        }

        if (m_bStopAfterCurrent) return NULL;
        if (m_pTransition)       return m_pTransition;
        if (!m_pCurList || m_pCurList->m_Songs.empty()) return NULL;

        size_t i = m_nNextIndex;
        if (i >= m_pCurList->m_Songs.size())
        {
            if (!m_pCurList->m_bLoop) return NULL;
            i = 0;
        }
        return m_pCurList->m_Songs[i];
    }

    // Render the next section on the worker thread. Prepare reads through the engine here,
    // the worker renders from that memory only, and commit hands the wave over here again.
    // False only while an earlier job is still running
    bool WarmNext()
    {
        Song *pSong = PeekNext();
        if (!pSong || !client_file_mgr) return true;

        char szPath[1024];
        BuildPath(szPath, sizeof(szPath), pSong);

        FileRef ref;
        ref.m_FileType  = FILE_CLIENTFILE;
        ref.m_pFilename = szPath;

        FileIdentifier *pIdent = client_file_mgr->GetFileIdentifier(&ref, TYPECODE_SOUND);
        if (!pIdent) return true;               // play time will report it
        if (pIdent->m_pData) return true;       // the sound system has it already

        // One job at a time
        if (m_pJob) return false;

        MusicRenderJob *pJob = music_PrepareRender(szPath);
        if (!pJob)
        {
            // Not a score, or it won't sequence. Warm it here, so the cost still lands mid-section
            GetClientILTSoundMgrImpl()->CreateBuffer(*pIdent);
            return true;
        }

        try
        {
            m_bJobDone.store(false);
            m_Worker = std::thread(&ImaDriver::WorkerMain, this, pJob);
        }
        catch (...)
        {
            // No thread to be had, so render here
            music_DiscardRender(pJob);
            GetClientILTSoundMgrImpl()->CreateBuffer(*pIdent);
            return true;
        }

        m_pJob     = pJob;
        m_sJobPath = szPath;
        return true;
    }

    // The worker. The job is its argument, so the two threads share only the atomic flag
    static void WorkerMain(ImaDriver *pSelf, MusicRenderJob *pJob)
    {
        music_RunRender(pJob);
        pSelf->m_bJobDone.store(true);
    }

    // Main thread, every frame. Collects a finished job and hands it to the sound system now,
    // so LoadData finds the wave filed rather than rendering at the boundary
    void PollWorker()
    {
        if (!m_pJob || !m_bJobDone.load()) return;

        m_Worker.join();
        MusicRenderJob *pJob = m_pJob;
        m_pJob = LTNULL;

        uint32 nPrep = 0, nRender = 0;
        music_GetRenderTimes(pJob, &nPrep, &nRender);

        const bool bOk = music_CommitRender(pJob);      // frees pJob

        if (g_nMusicTrace)
            dsi_PrintToConsole("music: %s %s off-thread (prepare %ums on the main thread, render %ums)",
                               bOk ? "rendered" : "could not render",
                               m_sJobPath.c_str(), (unsigned)nPrep, (unsigned)nRender);

        if (bOk && client_file_mgr)
        {
            FileRef ref;
            ref.m_FileType  = FILE_CLIENTFILE;
            ref.m_pFilename = m_sJobPath.c_str();

            FileIdentifier *pIdent = client_file_mgr->GetFileIdentifier(&ref, TYPECODE_SOUND);
            if (pIdent) GetClientILTSoundMgrImpl()->CreateBuffer(*pIdent);
        }

        m_sJobPath.clear();
    }

    // Wait for the worker and drop its job. Never detach, as the worker would write into a freed job
    void JoinWorker()
    {
        if (!m_pJob) return;

        if (m_Worker.joinable()) m_Worker.join();
        music_DiscardRender(m_pJob);
        m_pJob = LTNULL;
        m_sJobPath.clear();
    }

    void BuildPath(char *pszOut, size_t nOut, Song *pSong)
    {
        if (m_szDir[0])
            LTSNPrintF(pszOut, nOut, "%s\\%s", m_szDir, pSong->m_sName.c_str());
        else
            LTStrCpy(pszOut, pSong->m_sName.c_str(), nOut);
    }

    bool StartNext()
    {
        if (m_bPaused) return false;

        // A transition plays once, ahead of the list it leads into
        if (m_pTransition)
        {
            Song *pSong = m_pTransition;
            m_pTransition = NULL;
            if (PlaySong(pSong)) return true;
            // A missing transition shouldn't cost the level its music
        }

        if (!m_pCurList || m_pCurList->m_Songs.empty()) return false;

        // Try each section once and then give up on the list, rather than retrying every frame
        PlayList *pList = m_pCurList;
        size_t nTries = pList->m_Songs.size();

        for (size_t nTry = 0; nTry < nTries; ++nTry)
        {
            if (m_nNextIndex >= pList->m_Songs.size())
            {
                if (!pList->m_bLoop && nTry == 0) { m_pCurList = NULL; return false; }
                m_nNextIndex = 0;
            }

            size_t i = PickNext();
            if (i >= pList->m_Songs.size()) i = 0;
            m_nNextIndex = i + 1;

            if (PlaySong(pList->m_Songs[i])) return true;
        }

        dsi_PrintToConsole("music: nothing in '%s' would play (%u section(s)); "
                           "music stopped for this level",
                           pList->m_sName.c_str(), (unsigned)nTries);
        m_pCurList = NULL;
        return false;
    }

    bool PlaySong(Song *pSong)
    {
        if (!pSong || !ilt_sound) return false;

        PlaySoundInfo info;
        PLAYSOUNDINFO_INIT(info);

        info.m_dwFlags = PLAYSOUND_LOCAL | PLAYSOUND_GETHANDLE | PLAYSOUND_CTRL_VOL;
        info.m_nVolume = kMusicFullVolume;     // the class multiplier carries the setting
        info.m_nPriority = 255;                // music must not lose a voice to a footstep
        info.m_nSoundVolumeClass = kMusicVolumeClass;

        BuildPath(info.m_szSoundName, sizeof(info.m_szSoundName), pSong);

        HLTSOUND hSound = LTNULL;
        uint32 nT0 = time_GetMSTime();
        LTRESULT rPlay = ilt_sound->PlaySound(&info, hSound);
        uint32 nT1 = time_GetMSTime();

        if (rPlay != LT_OK || !hSound)
        {
            // A missing .sec, a section that won't sequence (music_segment.cpp says why), or the sound system off
            dsi_PrintToConsole("music: cannot play %s", info.m_szSoundName);
            m_hSound = LTNULL;
            return false;
        }

        m_hSound   = hSound;
        m_pCurSong = pSong;

        // For MusicTrace: load is what PlaySound cost here, and dead is the silence since the last section ended
        LTFLOAT fDur = 0.0f;
        ilt_sound->GetSoundDuration(hSound, fDur);

        if (g_nMusicTrace)
        {
            if (m_nLastStartMS)
            {
                int32 nDead = (int32)(nT0 - m_nLastStartMS) - (int32)m_nLastDurMS;
                dsi_PrintToConsole("music: playing %s (load %ums, dead %dms, "
                                   "section %.2fs)",
                                   info.m_szSoundName, (unsigned)(nT1 - nT0),
                                   (int)nDead, (float)fDur);
            }
            else
            {
                dsi_PrintToConsole("music: playing %s (load %ums, section %.2fs)",
                                   info.m_szSoundName, (unsigned)(nT1 - nT0),
                                   (float)fDur);
            }
        }

        m_nLastStartMS = nT0;
        m_nLastDurMS   = (uint32)(fDur * 1000.0f + 0.5f);
        m_bWarmed      = false;

        return true;
    }

    bool IsPlaying()
    {
        if (!m_hSound || !ilt_sound) return false;

        bool bDone = true;
        ilt_sound->IsSoundDone(m_hSound, bDone);
        if (bDone)
        {
            // PLAYSOUND_GETHANDLE keeps the instance until it's killed, so a finished section is reaped
            ilt_sound->KillSound(m_hSound);
            m_hSound = LTNULL;
            return false;
        }
        return true;
    }

    void StopSound()
    {
        if (m_hSound && ilt_sound) ilt_sound->KillSound(m_hSound);
        m_hSound = LTNULL;
        m_bStopAfterCurrent = false;

    }

    void ApplyVolume()
    {
        if (!ilt_sound) return;
        ilt_sound->SetSoundClassMultiplier(kMusicVolumeClass,
                                           (float)m_nVolume / (float)kMusicFullVolume);
        // Applies the new multiplier to what's already playing
        ilt_sound->UpdateVolumeSettings();
    }

    bool                    m_bInited;
    bool                    m_bPaused;
    uint16                  m_nVolume;
    char                    m_szDir[256];
    std::string             m_sDLS;
    std::string             m_sStyle;

    std::vector<Song *>      m_Songs;
    std::vector<PlayList *>  m_Lists;
    std::vector<std::string> m_Mounted;

    HLTSOUND                m_hSound;
    PlayList               *m_pCurList;
    Song                   *m_pCurSong;      // what Resume brings back
    Song                   *m_pTransition;
    size_t                  m_nNextIndex;
    bool                    m_bStopAfterCurrent;

    uint32                  m_nLastStartMS;   // when the last section started
    uint32                  m_nLastDurMS;     // and how long it runs for
    bool                    m_bWarmed;        // next section already rendered?

    // The render in flight. m_pJob is the worker's while it runs, and m_bJobDone is the one word both threads share
    std::thread             m_Worker;
    MusicRenderJob         *m_pJob;
    std::string             m_sJobPath;
    std::atomic<bool>       m_bJobDone;

    PlayList               *m_pPendingList;
    Song                   *m_pPendingTransition;
    bool                    m_bPendingLoop;
    bool                    m_bHavePending;
};

// A function-local static, so nothing runs during static initialization
ImaDriver &Driver()
{
    static ImaDriver s_Driver;
    return s_Driver;
}

// ------------------------------------------------------ the C function table --
//
// What MusicDLLSetup would have written into SMusicMgr

bool  ima_Init(SMusicMgr *pMgr)                        { return Driver().Init(pMgr); }
void  ima_Term()                                       { Driver().Term(); }
bool  ima_SetDataDirectory(const char *d)              { return Driver().SetDataDirectory(d); }
bool  ima_InitInstruments(const char *a, const char *b){ return Driver().InitInstruments(a, b); }
void *ima_CreateSong(const char *n)                    { return Driver().CreateSong(n); }
void *ima_GetSong(const char *n)                       { return Driver().GetSong(n); }
void  ima_DestroySong(void *p)                         { Driver().DestroySong(p); }
bool  ima_PlayBreak(void *p, uint32 f)                 { return Driver().PlayBreak(p, f); }
void *ima_CreatePlayList(const char *n)                { return Driver().CreatePlayList(n); }
void *ima_GetPlayList(const char *n)                   { return Driver().GetPlayList(n); }
bool  ima_AddSongToList(void *l, void *s)              { return Driver().AddSongToList(l, s); }
void *ima_GetSongFromList(void *l, unsigned long i)    { return Driver().GetSongFromList(l, i); }
void  ima_RemoveList(void *l)                          { Driver().RemoveList(l); }
bool  ima_PlayList(void *l, void *t, bool b, uint32 f) { return Driver().DoPlayList(l, t, b, f); }
void  ima_DestroyAllSongs()                            { Driver().DestroyAllSongs(); }
void  ima_Stop(uint32 f)                               { Driver().Stop(f); }
bool  ima_Pause(uint32 f)                              { return Driver().Pause(f); }
bool  ima_Resume()                                     { return Driver().Resume(); }
uint16 ima_GetVolume()                                 { return Driver().GetVolume(); }
void  ima_SetVolume(unsigned short v)                  { Driver().SetVolume(v); }

// Motifs. The caps say there are none, so ci_PlayMotif refuses first. These keep every slot filled
bool  ima_PlayMotif(const char *, bool)                { return false; }
bool  ima_StopMotif(const char *)                      { return false; }

}  // namespace

bool music_ima_IsBuiltinName(const char *pszMusicDLLName)
{
    if (!pszMusicDLLName) return false;

    // Exactly the name both games pass, case-insensitive as it's a filename
    static const char *pszWanted = "ima.dll";
    size_t n = strlen(pszWanted);
    if (strlen(pszMusicDLLName) != n) return false;

    for (size_t i = 0; i < n; ++i)
    {
        char c = pszMusicDLLName[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c != pszWanted[i]) return false;
    }
    return true;
}

void music_ima_Install(SMusicMgr *pMusicMgr)
{
    if (!pMusicMgr) return;

    pMusicMgr->Init             = ima_Init;
    pMusicMgr->Term             = ima_Term;
    pMusicMgr->SetDataDirectory = ima_SetDataDirectory;
    pMusicMgr->InitInstruments  = ima_InitInstruments;
    pMusicMgr->CreateSong       = ima_CreateSong;
    pMusicMgr->GetSong          = ima_GetSong;
    pMusicMgr->DestroySong      = ima_DestroySong;
    pMusicMgr->PlayBreak        = ima_PlayBreak;
    pMusicMgr->CreatePlayList   = ima_CreatePlayList;
    pMusicMgr->GetPlayList      = ima_GetPlayList;
    pMusicMgr->AddSongToList    = ima_AddSongToList;
    pMusicMgr->GetSongFromList  = ima_GetSongFromList;
    pMusicMgr->RemoveList       = ima_RemoveList;
    pMusicMgr->PlayList         = ima_PlayList;
    pMusicMgr->PlayMotif        = ima_PlayMotif;
    pMusicMgr->StopMotif        = ima_StopMotif;
    pMusicMgr->DestroyAllSongs  = ima_DestroyAllSongs;
    pMusicMgr->Stop             = ima_Stop;
    pMusicMgr->Pause            = ima_Pause;
    pMusicMgr->Resume           = ima_Resume;
    pMusicMgr->GetVolume        = ima_GetVolume;
    pMusicMgr->SetVolume        = ima_SetVolume;

    // ConsolePrint is left alone, as music_InitDriver sets the engine's own after this, as for a real DLL
}

void music_ima_Update()
{
    Driver().Update();
}
