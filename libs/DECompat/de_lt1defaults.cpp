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

    // Both games' palettes write a table alpha of 255, so a polygrid vertex takes the object's alpha.
    // Shogo: water at its default SurfaceAlpha 0.7 is 70% opaque, where it drew opaque.
    // Blood 2: 03_MUSEUM's basin draws at its 0.55
    { "LT1PolyGridAlpha",        "1", LT1_CLIENT },

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

    // LT1 skips the world walk for a zero length move, so a creation teleport doesn't lift an object out of
    // a slope or eject a trigger placed overlapping the world. Shogo's AIs on slopes are touched by their first
    // fall instead, as in retail, and Blood 2's level triggers stay where they were placed (both sides)
    { "LT1ZeroMoveSkip",         "1", LT1_BOTH },

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

    // Shogo and Blood 2: a world ray with INTERSECT_HPOLY walks the polygons with LT1's 0.1 plane tolerance too.
    // Blood 2: two shell casings whose last step met a face just inside the band bounced in the port and were
    // deleted in retail. Shogo's replays are unchanged by it (both sides)
    { "LT1IntersectPoly",      "1", LT1_BOTH },

    // Shogo and Blood 2: the client sends its command states at the start of each frame, and the server answers
    // IsCommandOn and fires OnCommandOn from them, as LT1's update packet did. From the game's own player update
    // the server saw every held key a tick late, and every key driven animation with it (both sides)
    { "LT1ServerCommands",     "1", LT1_BOTH },

    // Shogo and Blood 2: the rest of the rows both games set the same way.
    // Each was measured in Shogo and confirmed in Blood 2's own binaries
    
    // Shogo: room for the player view weapon, as the engine's default far plane for REALLYCLOSE (7.0) clips most guns.
    // Blood 2: the far clip only has to clear the deepest player view model
    { "reallyclose_far",       "20.0", LT1_CLIENT },

    // Horizontal+ widescreen. A 4:3 window is pixel identical.
    // Shogo: its fovY only matches the screen at 4:3.
    // Blood 2: it always asks for a 4:3 pair (fovY = fovX * 0.80)
    { "WidescreenFOV",         "1", LT1_CLIENT },

    // LT1's motion for a standing object, which keeps gravity on it (both sides).
    // Shogo: without it a standing character flickers between grounded and airborne and loses jumps.
    // Blood 2: without it a stopping player slides 0.039 past where retail stops dead
    { "LT1CalcMotion",         "1", LT1_BOTH },

    // The physics step is LT1's whole CalcMotion, the LT1 era set's module, in place of Jupiter's with LT1CalcMotion
    // inside it. It clips the standing velocity in place as LT1 does, so a resting player is at v 0, not -21.44.
    // LT1CalcMotion above stays set for Jupiter's step if this row is taken out (both sides)
    { "LT1MotionStep",         "1", LT1_BOTH },

    // A resized box is detached from what it stood on, so its first step from rest falls and lands as in LT1 (both sides)
    { "LT1DimsDetach",         "1", LT1_BOTH },

    // The main world is walked once per move, and the stair step's box covers the whole move.
    // Only both together match LT1 on slopes: the swept box alone lifts a climbing player twice,
    // on the world's second walk (both sides)
    { "LT1WorldOnce",          "1", LT1_BOTH },
    { "LT1StairSwept",         "1", LT1_BOTH },

    // Server: tracked sounds count down by game time, so a character's next pain sound waits for the last one
    // to end in game time
    { "LT1SoundStep",          "1", LT1_SERVER },

    // Server: an object nobody has seen for its deactivation time stops updating until the player or a camera pings it.
    // The shell's 'autodeactivate 1' is LT1's own switch, and this turns on the engine's part.
    // Shogo: an AI unseen for 10 s (AI_DEACTIVATION_TIME) sleeps.
    // Blood 2: retail's AIs sleep on the first unpaused tick and wake when a ping reaches them
    { "LT1AutoDeactivate",     "1", LT1_SERVER },

    // Server: a client is only sent what LT1's vis walk reaches from its view, and the walk takes a node's objects
    // most recently filed first, so pings wake them and the server updates them in LT1's order
    { "LT1ClientPVS",          "1", LT1_SERVER },
    { "LT1FileOrder",          "1", LT1_SERVER },

    // The engine's Random, IntRandom and RandomScale, and the particles' draws, come from LT1's stream with LT1's
    // seeds (both sides).
    // Shogo: so things like how many pieces a destroyed prop breaks into match og Shogo.
    // Blood 2: weapon damage is ServerDE Random, which otherwise rolls differently on every run
    { "LT1EngineRand",         "1", LT1_BOTH },

    // A mover's world touch sees where it was stopped, which is where a projectile's impact is read from (both sides)
    { "LT1WorldTouchPos",      "1", LT1_BOTH },

    // Only a solid object's resize collides. Anything else takes its new dims with no move and no touch (both sides)
    { "LT1SolidResize",        "1", LT1_BOTH },

    // A world ray without INTERSECT_HPOLY walks leaves with a 0.1 tolerance, so debris that sinks a little into
    // a floor rests rather than bouncing (both sides)
    { "LT1IntersectLeaf",      "1", LT1_BOTH },

    // A point collide mover, such as every projectile, uses both LT1 walks' 0.1 tolerance and stops 0.1 short
    // of the plane (both sides)
    { "LT1PointCollide",       "1", LT1_BOTH },

    // Collision candidates are tested against the mover's box at its destination,
    // so a player stepping away from an NPC it touches isn't pulled back to it (both sides)
    { "LT1MoverBoxGate",       "1", LT1_BOTH },

    // A move bigger than the mover's dims takes the swept test even when the pair isn't solid,
    // so a projectile's touch, and its impact point, comes at the target's face (both sides)
    { "LT1TunnelNonsolid",     "1", LT1_BOTH },

    // A resize grows Y, then X, then Z with LT1's three probes,
    // so a player starting to walk from a wall gets the same box as in LT1 (both sides)
    { "LT1GrowDim",            "1", LT1_BOTH },

    // MoveObject drops any axis of a move under 0.001, so a player against a wall doesn't creep into its
    // clearance (both sides)
    { "LT1MoveAxisMin",        "1", LT1_BOTH },

    // Touches are box overlap alone, so stepping onto a resting rocket detonates it (both sides)
    { "LT1PointBoxTouch",      "1", LT1_BOTH },

    // Containers use LT1's box against BSP test with its center and corner probes (both sides).
    // Shogo: its airshaft fans lift the player when they're supposed to
    { "LT1ContainerBSP",       "1", LT1_BOTH },
    { "LT1BoxBSPProbes",       "1", LT1_BOTH },

    // Positions in game messages at LT1's precision, as the server encodes them, so a weapon impact sent to the
    // client is where LT1's explosion debris starts (both sides)
    { "LT1CompPos",            "1", LT1_BOTH },

    // Server: a removed object leaves the world tree at once
    { "LT1RemoveUnlinks",      "1", LT1_SERVER },

    // SetObjectPos drops moves under 0.0316 units as LT1's does.
    // Shogo: the weapon fires from the camera object, so this decides where shots start
    { "LT1ClientMoveMin",      "1", LT1_CLIENT },

    // A sprite's glow and bias reach the client, as LT1's flag mask sent them.
    // The server writes the mask and the client reads it, so both sides set it.
    // Blood 2: bit 5 is also FLAG_ONLYLIGHTWORLD, which its scorch mark lights carry, so they stop lighting models
    { "LT1SpriteFlags",        "1", LT1_BOTH },

    // LT1's renderer: the world's fullbrite passes add the masked texel over the lit result, a fullbright skin
    // draws a second unlit pass, a dynamic light lights a model as one object measured at its position,
    // and the sky draws through its object's matrix.
    // Shogo: seen in the glowing parts of the Laser Cannon and Pulse Rifle skins, and a Pulse Rifle hit lights
    // a whole truck or enemy in retail, where Jupiter's point lights leave a faint spot.
    // Blood 2: read in its own d3d.ren
    { "LT1FullbriteAdd",       "1", LT1_CLIENT },
    { "LT1ModelFullbrite",     "1", LT1_CLIENT },
    { "LT1ModelDynLight",      "1", LT1_CLIENT },
    { "LT1SkyObjectTransform", "1", LT1_CLIENT },

    // Demo playback as og runs it, playback only: the server's step floored at 0.01 s,
    // no held frame after the world loads, the client's clock is the recording's,
    // and at the end the console comes up over the still running level
    { "LT1ServerStepFloor",    "1", LT1_BOTH },
    { "LT1DemoNoPrime",        "1", LT1_CLIENT },
    { "LT1DemoClientTime",     "1", LT1_CLIENT },
    { "LT1DemoEndConsole",     "1", LT1_CLIENT },

    // LT1 picks the path that prevents tunneling by geometry, Jupiter by a flag no 1998 game sets (both sides)
    { "LT1TunnelPath",         "1", LT1_BOTH },

    // Shogo and Blood 2: the multiplayer wizard also sends LT1's query, so retail servers show up in its list,
    // and joining one goes through CLT1Driver. While it's on, a join straight to an address always speaks LT1's protocol,
    // so connecting to another copy of this port by address needs LT1Net 0
    { "LT1Net",                "1", LT1_CLIENT },
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
