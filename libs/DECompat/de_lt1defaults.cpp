//  The LT1 engine variable defaults shared by every game that links this layer

#include <stdio.h>
#include <string.h>

#include "iltclient.h"
#include "iltserver.h"
#include "de_lt1defaults.h"
#include "de_commandedges.h"


// The shared LT1 set. The client walker gives each row a '-' prefix and the server walker gives none.
// Rows are in the client's old order. The server sets LT1NetInterp and LT1AnimTransition before
// LT1ObjectDims now, which changes nothing as each is a plain store.
// The server compiles its own engine_vars.cpp and collision.cpp, so a physics var is a both sides row
static const LT1Var s_aLT1Set[] =
{
    // The era: LithTech 1.0, which selects the LT1 era set (runtime/shared/src/era/) on both sides.
    // The set is empty for now, so this changes nothing until a routine moves into it
    { "EngineEra",               "1", LT1_BOTH },

    { "LT1ObjectColor",          "1", LT1_CLIENT },
    { "LT1ModelShading",         "1", LT1_CLIENT },
    { "LT1PolyGridCull",         "1", LT1_CLIENT },
    { "LT1PolyGridTexCoords",    "1", LT1_CLIENT },

    // LT1 packs a 2D color key and each pixel to 5:6:5 before comparing, so near magentas are transparent
    { "LT1ColorKey565",          "1", LT1_CLIENT },

    // LT1 vertex animated model nodes rewrite their stream every frame, and a static stream stalls on the GPU
    { "VAMeshDynamicVB",         "1", LT1_CLIENT },

    // Without it d3d_BlitToScreen can't scale, and the splash draws 640x480 in the corner
    { "OptimizeSurfaces",        "1", LT1_CLIENT },

    // Server: Jupiter restarts an animation that's already playing, pinning code that sets it every update to frame 0
    { "LT1ModelAnimNoRestart",   "1", LT1_SERVER },

    // LT1 eases each networked object to its latest update over 1.1 server periods, with no velocity or physics.
    // The switch goes first so Prediction 1 never means Jupiter's extrapolation, which put targets under the floor.
    // Prediction 1 is what retail Shogo cfgs ship, and 0 still snaps as it did in LT1
    // Server: under LT1NetInterp, the server sends no prediction cap resend as LT1 sent none.
    // A Server.dll server has its own copy of the variable
    { "LT1NetInterp",            "1", LT1_BOTH },
    { "Prediction",              "1", LT1_CLIENT },

    // LT1's 200 ms animation hold for objects with FLAG_ANIMTRANSITION. No string key fires inside it,
    // so an attack's first key comes 200 ms after the animation was set (both sides)
    // Server: the fire keys the hold delays are the server's
    { "LT1AnimTransition",       "1", LT1_BOTH },

    // LT1 keeps a partial dims change where Jupiter moves the object back (both sides)
    // Server: both sides have to agree on a box's size
    { "LT1ObjectDims",           "1", LT1_BOTH },

    // LT1 had no FLAG_CONTAINER, an object's type made it one.
    // Without this water, conveyors and ladders are inert (both sides)
    // Server: server physics creates the container links
    { "LT1ContainerType",        "1", LT1_BOTH },

    // Server: LT1 never scans a newborn for touches.
    // The placement move runs before OnObjectCreated, so a flagless object scans nothing.
    { "LT1CreateInPlace",        "1", LT1_SERVER },

    // Server: LT1's object name table is case sensitive. Og content typos come back too, which is what LT1 did
    { "LT1NameCase",             "1", LT1_SERVER },

    // LT1 only diverts a move when both objects are solid (both sides)
    // Server: the restore move after a load runs on the server, which matters for doors
    { "LT1NonsolidNoDivert",     "1", LT1_BOTH },

    // LT1 honors SetupBox's 'too short to sweep' answer (both sides)
    // Server: server physics runs the same collision sweep
    { "LT1SetupBoxCheck",        "1", LT1_BOTH },

    // Only solid polys stop rays, so shots and sights pass LT1's invisible doorway polys (both sides)
    // Server: weapon and AI sight rays are cast on the server
    { "LT1RaySolidOnly",         "1", LT1_BOTH },

    // Server: LT1 reserved client id 0 for 'no player' - ids are handed out on the server
    { "LT1ClientIDFromOne",      "1", LT1_SERVER },

    // Optimized 2D draws over the whole target, not clipped to the last camera's rect
    { "Optimized2DFullViewport", "1", LT1_CLIENT },

    // LT1 shells read OnKeyDown and OnKeyUp as Win32 virtual key codes
    { "LT1VirtualKeys",          "1", LT1_CLIENT },

    // Set by every game that links this layer, Shogo and Blood 2, each with its own reason below

    // Shogo: Reads the object light grid with y the right way round.
    // Jupiter's CLightTable::GetLightVal blends its rows backwards, which LT1's coarse grid shows
    // Blood 2: Jupiter's CLightTable::GetLightVal blends its rows backwards, which LT1's coarse grid shows
    { "LT1LightTableY",        "1", LT1_CLIENT },

    // Shogo: Shogo's dynamic lights light the level as it does in the og.
    // Jupiter only lets a dynamic light reach the world under this.
    // Blood 2: LT1 dynamic lights light the world, and Jupiter's only opt in is a flag LT1 games don't have.
    // 01_SUBWAYA's tunnel is lit by lights swept past the train
    { "DynamicLightWorld",     "1", LT1_CLIENT },

    // Shogo: LT1's translucent WorldModel split, part 1: whether a brush is culled.
    // Much of Shogo is a box textured on one face that a cull would hide
    // Blood 2: LT1 splits WorldModels by polygon 0's SURF_TRANSPARENT: translucent ones draw last and aren't culled.
    // Jupiter's split never puts LT1 glass in the translucent pass, so it was painted over
    { "LT1WorldCull",          "1", LT1_CLIENT },

    // Shogo: Part 2: when the brush draws.
    // LT1 draws the translucent list last, where Jupiter would overdraw them in the solid pass
    { "LT1WorldSort",          "1", LT1_CLIENT },

    // Shogo: LT1 stair step and resting behavior.
    // Without it, MoveToFrontside walks the player up hillsides
    // Shogo server: LT1 stair step and resting behavior.
    // Each side compiles its own collision.cpp so the client shell sets this too
    // Blood 2: LT1 stair step, and gravity's push into the floor on a standing object (both sides)
    { "LT1StairStep",          "1", LT1_BOTH },

    // Shogo: Keeps resolved progress when the strike or restart cap cancels (both sides)
    // Shogo server: keep the solver's resolved position when the strike or restart cap fires (client too)
    // Blood 2: A third repeat hit keeps the resolved progress, and the main world is walked after the object loop (both sides)
    { "LT1StrikeKeep",         "1", LT1_BOTH },

    // Shogo: Travels with LT1StrikeKeep (both sides)
    // Shogo server: LT1StrikeKeep's other half.
    // A strike leaves the candidate loop early, and Jupiter put the main world in that loop (client too)
    { "LT1WorldAfterObjects",  "1", LT1_BOTH },

    // Shogo and Blood 2: Platforms carry riders as LT1's do: no nudge into the platform, tiny moves ignored,
    // and a rotating platform doesn't refuse its riders (both sides)
    { "LT1CarryNoNudge",       "1", LT1_BOTH },
    { "LT1MinMove",            "1", LT1_BOTH },
    { "LT1RotateNoMovingFlag", "1", LT1_BOTH },

    // Shogo: The rest of LT1's contact stack (both sides)
    // Shogo server: the rest of LT1's contact stack: 0.1 clearance, exact contact isn't a hit, no stale detections.
    // Without them LT1StrikeKeep parks the player on the wall plane (client sets all four)
    // Blood 2: LT1's box collision: pushback along the normal, slide with a 0.1 gap, the 0.1 recheck against every plane touched,
    // the sweep rebuilt after each resolution, and LT1's polygon test. Without them a crouched player grinds in vents (both sides)
    { "LT1IntersectPushback",  "1", LT1_BOTH },
    { "LT1PlaneRecheck",       "1", LT1_BOTH },
    { "LT1SweepRebuild",       "1", LT1_BOTH },
    { "LT1PolyTest",           "1", LT1_BOTH },
};


// Each walk prints one line naming its set and every row it applied, which the suites check for.
// CPrint formats into 512 bytes, so a long set continues on further lines
static const int kVarLineMax = 400;

template <class T>
static void PrintVarLines(T *p, const char *pszSet, const char *pszSide, const LT1Var *pa, unsigned n, unsigned nSide)
{
    unsigned nApplied = 0;
    for (unsigned i = 0; i < n; ++i)
        if (pa[i].nSides & nSide)
            ++nApplied;
    char szLine[kVarLineMax + 64];
    int nLen = snprintf(szLine, sizeof(szLine), "DECompat: %s %s set applied (%u) --", pszSet, pszSide, nApplied);
    for (unsigned i = 0; i < n; ++i)
    {
        if (!(pa[i].nSides & nSide))
            continue;
        char szItem[160];
        int nItem = snprintf(szItem, sizeof(szItem), " %s=%s", pa[i].pszName, pa[i].pszValue);
        if (nItem >= (int)sizeof(szItem))
            nItem = (int)sizeof(szItem) - 1;
        if (nLen + nItem > kVarLineMax)
        {
            p->CPrint("%s", szLine);
            nLen = snprintf(szLine, sizeof(szLine), "DECompat: %s %s set, continued --", pszSet, pszSide);
        }
        memcpy(szLine + nLen, szItem, nItem + 1);
        nLen += nItem;
    }
    p->CPrint("%s", szLine);
}


void DECompat_ApplyLT1ClientVars(ILTClient *pClientDE, const char *pszSet, const LT1Var *pa, unsigned n)
{
    if (!pClientDE || !pa)
        return;

    // The '-' prefix keeps these out of autoexec.cfg
    char sz[160];
    for (unsigned i = 0; i < n; ++i)
    {
        if (!(pa[i].nSides & LT1_CLIENT))
            continue;
        snprintf(sz, sizeof(sz), "-%s %s", pa[i].pszName, pa[i].pszValue);
        pClientDE->RunConsoleString(sz);
    }

    PrintVarLines(pClientDE, pszSet, "client", pa, n, LT1_CLIENT);
}


void DECompat_ApplyLT1ServerVars(ILTServer *pServerDE, const char *pszSet, const LT1Var *pa, unsigned n)
{
    if (!pServerDE || !pa)
        return;

    // RunGameConString takes the bare name
    char sz[160];
    for (unsigned i = 0; i < n; ++i)
    {
        if (!(pa[i].nSides & LT1_SERVER))
            continue;
        snprintf(sz, sizeof(sz), "%s %s", pa[i].pszName, pa[i].pszValue);
        pServerDE->RunGameConString(sz);
    }

    PrintVarLines(pServerDE, pszSet, "server", pa, n, LT1_SERVER);
}


void DECompat_ApplyLT1ClientDefaults(ILTClient *pClientDE)
{
    DECompat_ApplyLT1ClientVars(pClientDE, "LT1", s_aLT1Set, sizeof(s_aLT1Set) / sizeof(s_aLT1Set[0]));
}


// Worlds loaded this process
static uint32 s_nServerInits = 0;

// Set by DECompat_ApplyLT1ServerDefaults and read by DECompat_NoteServerActivity
static bool s_bServerDefaultsApplied = false;


void DECompat_ApplyLT1ServerDefaults(ILTServer *pServerDE)
{
    if (!pServerDE)
        return;

    // Firing OnCommandOn from the player update is the game's job.
    // The check below catches it missing from the second world load on.

    DECompat_ApplyLT1ServerVars(pServerDE, "LT1", s_aLT1Set, sizeof(s_aLT1Set) / sizeof(s_aLT1Set[0]));

    s_bServerDefaultsApplied = true;

    DECompat_SetCommandEdgeReportSink(pServerDE);

    if (++s_nServerInits >= 2 && !DECompat_CommandEdgesSeen())
    {
        pServerDE->CPrint(
            "DECompat: %d worlds loaded and OnCommandOn never fired. Call DECompat_FireCommandEdges from the player update.",
            (int)s_nServerInits);
    }
}


void DECompat_NoteServerActivity(ILTServer *pServerDE)
{
    static bool  s_bDone = false;
    static uint32 s_nSeen = 0;

    if (s_bDone)
        return;

    // Give the server a few hundred messages before judging
    if (++s_nSeen < 300)
        return;

    s_bDone = true;

    if (s_bServerDefaultsApplied || !pServerDE)
        return;

    pServerDE->CPrint(
        "DECompat: DECompat_ApplyLT1ServerDefaults was never called. Call it from OnServerInitialized.");
}
