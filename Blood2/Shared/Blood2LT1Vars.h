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
};

// The walkers' last three arguments: the set's name, the table and its length
#define BLOOD2_LT1_VARS "Blood2", g_aBlood2LT1Vars, (unsigned)(sizeof(g_aBlood2LT1Vars) / sizeof(g_aBlood2LT1Vars[0]))

#endif
