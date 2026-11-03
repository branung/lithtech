#ifndef __BLOOD2LT1VARS_H__
#define __BLOOD2LT1VARS_H__

// Blood 2's LT1 engine vars, both shells' in one table.
// Each shell walks it for its own side right after the shared LT1 set, so a row here overrides that set,
// and a var both sides need is one LT1_BOTH row. Names are bare, as the client walker adds the '-'

#include "de_lt1defaults.h"

static const LT1Var g_aBlood2LT1Vars[] =
{
    // Blood 2's near clip for player view models is 0.01. The far clip only has to clear the deepest one
    { "reallyclose_near",      "0.01", LT1_CLIENT },
    { "reallyclose_far",       "20.0", LT1_CLIENT },

    // Horizontal+ widescreen, since Blood 2 always asks for a 4:3 pair (fovY = fovX * 0.80).
    // A 4:3 window is pixel identical
    { "WidescreenFOV",         "1",    LT1_CLIENT },

    // Point sample the 2D. The HUD is pixel art keyed on magenta, and a scaled bilinear blit blends the
    // keyed texels' magenta into every edge. A 1:1 blit looks the same either way
    { "2DBilinear",            "0",    LT1_CLIENT },

    // LT1's box collision: pushback along the normal, slide with a 0.1 gap, the 0.1 recheck against every plane touched,
    // the sweep rebuilt after each resolution, and LT1's polygon test. Without them a crouched player grinds in vents (both sides)
    { "LT1BoxSlide",           "1",    LT1_BOTH },

    // LT1 stair step, and gravity's push into the floor on a standing object (both sides)
    { "LT1StandGravity",       "1",    LT1_BOTH },

    // LT1's motion for a standing object. Without it the player slid 0.039 past the stop where retail's
    // replay of subway3 halted dead (both sides)
    { "LT1CalcMotion",         "1",    LT1_BOTH },

    // The engine's Random, IntRandom and RandomScale draw from LT1's stream with LT1's seeds, as Blood 2's own
    // engine does. Weapon damage is ServerDE Random, so without it no replay repeats itself (both sides)
    { "LT1EngineRand",         "1",    LT1_BOTH },

    // During demo playback the server's step is floored at 0.01 s: every step of retail's subway3 replay is
    // the frame time clamped to [0.01, 0.2] (both sides)
    { "LT1ServerStepFloor",    "1",    LT1_BOTH },

    // The rest of Shogo's LT1 vars, confirmed in Blood 2's own binaries rather than taken from Shogo's.
    // Auto deactivation, Shogo's rule unchanged: the game pings every 0.5 s and the shell runs "serv autodeactivate 1".
    // Retail's AIs sleep on the first unpaused tick and wake when a ping reaches them
    { "LT1AutoDeactivate",     "1",    LT1_SERVER },

    // A client is sent what LT1's vis walk reaches from its view, and the walk takes a node's objects most recently
    // filed first, which orders a ping's wakes
    { "LT1ClientPVS",          "1",    LT1_SERVER },
    { "LT1FileOrder",          "1",    LT1_SERVER },

    // LT1's MoveObject, which Blood 2's engine has statement for statement as Shogo's (both sides)
    { "LT1MoverBoxGate",       "1",    LT1_BOTH },
    { "LT1TunnelPath",         "1",    LT1_BOTH },
    { "LT1TunnelNonsolid",     "1",    LT1_BOTH },
    { "LT1PointBoxTouch",      "1",    LT1_BOTH },
    { "LT1WorldOnce",          "1",    LT1_BOTH },
    { "LT1WorldTouchPos",      "1",    LT1_BOTH },
    { "LT1MoveAxisMin",        "1",    LT1_BOTH },

    // LT1's world tests. LT1StairSwept only works with LT1WorldOnce. LT1BoxHitNormal is on both sides here, where
    // Shogo sets the server's, since Blood 2's client rays reach the same box test (both sides)
    { "LT1BoxBSPProbes",       "1",    LT1_BOTH },
    { "LT1ContainerBSP",       "1",    LT1_BOTH },
    { "LT1StairSwept",         "1",    LT1_BOTH },
    { "LT1PointCollide",       "1",    LT1_BOTH },
    { "LT1IntersectLeaf",      "1",    LT1_BOTH },
    { "LT1BoxHitNormal",       "1",    LT1_BOTH },

    // A resize detaches what the object stood on, grows Y, X, Z through three probes, and collides only for
    // FLAG_SOLID. RemoveObject unlinks at once, the client's SetObjectPos is unforced, the motion step is LT1's,
    // tracked sounds count down by the game step, and positions are sent in LT1's 16 bit truncated encoding
    { "LT1DimsDetach",         "1",    LT1_BOTH },
    { "LT1GrowDim",            "1",    LT1_BOTH },
    { "LT1SolidResize",        "1",    LT1_BOTH },
    { "LT1RemoveUnlinks",      "1",    LT1_SERVER },
    { "LT1ClientMoveMin",      "1",    LT1_CLIENT },
    { "LT1MotionStep",         "1",    LT1_BOTH },
    { "LT1SoundStep",          "1",    LT1_SERVER },
    { "LT1CompPos",            "1",    LT1_BOTH },

    // Playback as retail runs it: the client's clock is the recording's, nothing is held after the world loads,
    // and the end prints retail's two lines and raises the console
    { "LT1DemoClientTime",     "1",    LT1_CLIENT },
    { "LT1DemoNoPrime",        "1",    LT1_CLIENT },
    { "LT1DemoEndConsole",     "1",    LT1_CLIENT },

    // GLOWSPRITE and SPRITEBIAS reach the client, as Blood 2 sends the whole low flag word. Bit 5 is also
    // FLAG_ONLYLIGHTWORLD, which Blood 2's scorch mark lights carry, so they stop lighting models (both sides)
    { "LT1SpriteFlags",        "1",    LT1_BOTH },

    // The renderer, read in Blood 2's own d3d.ren: the world's fullbrite passes add, a dynamic light lights a model
    // as one object, a fullbright skin draws a second unlit pass, and the sky draws through its object's matrix
    { "LT1FullbriteAdd",       "1",    LT1_CLIENT },
    { "LT1ModelDynLight",      "1",    LT1_CLIENT },
    { "LT1ModelFullbrite",     "1",    LT1_CLIENT },
    { "LT1SkyObjectTransform", "1",    LT1_CLIENT },

    // The shell doesn't clear a standing player's fall speed, as retail's doesn't, and the engine's LT1 floor
    // response resets it instead. On a frame the move manager's 0.1 gate skips, gravity builds and carries a slow
    // glide on the next, where the clear stopped the player dead. Read by MoveMgr.cpp
    { "LT1FloorResponse",      "1",    LT1_CLIENT },
};

// The walkers' last three arguments: the set's name, the table and its length
#define BLOOD2_LT1_VARS "Blood2", g_aBlood2LT1Vars, (unsigned)(sizeof(g_aBlood2LT1Vars) / sizeof(g_aBlood2LT1Vars[0]))

#endif
