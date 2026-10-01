//  The LT1 engine variable defaults shared by every game that links this layer

#include "iltclient.h"
#include "iltserver.h"
#include "de_lt1defaults.h"
#include "de_commandedges.h"


void DECompat_ApplyLT1ClientDefaults(ILTClient *pClientDE)
{
    if (!pClientDE)
        return;

    // The '-' prefix keeps these out of autoexec.cfg
    pClientDE->RunConsoleString("-LT1ObjectColor 1");
    pClientDE->RunConsoleString("-LT1ModelShading 1");
    pClientDE->RunConsoleString("-LT1PolyGridCull 1");
    pClientDE->RunConsoleString("-LT1PolyGridTexCoords 1");

    // LT1 vertex animated model nodes rewrite their stream every frame, and a static stream stalls on the GPU
    pClientDE->RunConsoleString("-VAMeshDynamicVB 1");

    // Without it d3d_BlitToScreen can't scale, and the splash draws 640x480 in the corner
    pClientDE->RunConsoleString("-OptimizeSurfaces 1");

    // Jupiter predicts remote models through client physics.
    // Gravity puts the target under the floor, so they animate in place and snap forward.
    pClientDE->RunConsoleString("-Prediction 0");

    // LT1 keeps a partial dims change where Jupiter moves the object back (both sides)
    pClientDE->RunConsoleString("-LT1ObjectDims 1");

    // LT1 had no FLAG_CONTAINER, an object's type made it one.
    // Without this water, conveyors and ladders are inert (both sides)
    pClientDE->RunConsoleString("-LT1ContainerType 1");

    // LT1 only diverts a move when both objects are solid (both sides)
    pClientDE->RunConsoleString("-LT1NonsolidNoDivert 1");

    // LT1 honors SetupBox's 'too short to sweep' answer (both sides)
    pClientDE->RunConsoleString("-LT1SetupBoxCheck 1");


    // LT1StairStep (both sides) and DynamicLightWorld are set per game after this call
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

    // Jupiter restarts an animation that's already playing, pinning code that sets it every update to frame 0
    pServerDE->RunGameConString("LT1ModelAnimNoRestart 1");

    // Both sides have to agree on a box's size
    pServerDE->RunGameConString("LT1ObjectDims 1");

    // Server physics creates the container links
    pServerDE->RunGameConString("LT1ContainerType 1");

    // LT1 never scans a newborn for touches.
    // The placement move runs before OnObjectCreated, so a flagless object scans nothing.
    pServerDE->RunGameConString("LT1CreateInPlace 1");

    // The restore move after a load runs on the server, which matters for doors
    pServerDE->RunGameConString("LT1NonsolidNoDivert 1");

    // Server physics runs the same collision sweep
    pServerDE->RunGameConString("LT1SetupBoxCheck 1");

    // LT1 reserved client id 0 for 'no player' - ids are handed out on the server
    pServerDE->RunGameConString("LT1ClientIDFromOne 1");

    s_bServerDefaultsApplied = true;

    DECompat_SetCommandEdgeReportSink(pServerDE);

    if (++s_nServerInits >= 2 && !DECompat_CommandEdgesSeen())
    {
        pServerDE->CPrint(
            "DECompat: %d worlds loaded and OnCommandOn never fired. Call DECompat_FireCommandEdges from the player update.",
            (int)s_nServerInits);
    }

    // The server compiles its own engine_vars.cpp and collision.cpp, so physics knobs go here too
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
