#ifndef __BLOOD2LT1VARS_H__
#define __BLOOD2LT1VARS_H__

// Blood 2's LT1 engine vars, both shells' in one table.
// Each shell walks it for its own side right after the shared LT1 set, so a row here overrides that set,
// and a var both sides need is one LT1_BOTH row. Names are bare, as the client walker adds the '-'

#include "de_lt1defaults.h"

static const LT1Var g_aBlood2LT1Vars[] =
{
    // Blood 2's near clip for player view models is 0.01
    { "reallyclose_near",      "0.01", LT1_CLIENT },

    // Point sample the 2D. The HUD is pixel art keyed on magenta, and a scaled bilinear blit blends the
    // keyed texels' magenta into every edge. A 1:1 blit looks the same either way
    { "2DBilinear",            "0",    LT1_CLIENT },

    // LT1's box collision: pushback along the normal, slide with a 0.1 gap, the 0.1 recheck against every plane touched,
    // the sweep rebuilt after each resolution, and LT1's polygon test. Without them a crouched player grinds in vents (both sides)
    { "LT1BoxSlide",           "1",    LT1_BOTH },

    // LT1 stair step, and gravity's push into the floor on a standing object (both sides)
    { "LT1StandGravity",       "1",    LT1_BOTH },

    // A ray hitting an object's box reports the x face's normal for every face. On both sides here, where Shogo
    // sets the server's, since Blood 2's client rays reach the same box test (both sides)
    { "LT1BoxHitNormal",       "1",    LT1_BOTH },

    // The shell doesn't clear a standing player's fall speed, as retail's doesn't, and the engine's LT1 floor
    // response resets it instead. On a frame the move manager's 0.1 gate skips, gravity builds and carries a slow
    // glide on the next, where the clear stopped the player dead. Read by MoveMgr.cpp
    { "LT1FloorResponse",      "1",    LT1_CLIENT },
};

// The walkers' last three arguments: the set's name, the table and its length
#define BLOOD2_LT1_VARS "Blood2", g_aBlood2LT1Vars, (unsigned)(sizeof(g_aBlood2LT1Vars) / sizeof(g_aBlood2LT1Vars[0]))

#endif
