#ifndef __SHOGOLT1VARS_H__
#define __SHOGOLT1VARS_H__

// Shogo's LT1 engine vars, both shells' in one table.
// Each shell walks it for its own side right after the shared LT1 set, so a row here overrides that set,
// and a var both sides need is one LT1_BOTH row. Names are bare, as the client walker adds the '-'

#include "de_lt1defaults.h"

static const LT1Var g_aShogoLT1Vars[] =
{
    // Draw the player view weapon with the scene's own projection as LT1 did (it had no separate one)
    { "PVModelSceneFOV",       "1",    LT1_CLIENT },

    // Server: a ray hitting an object's box reports the x face's normal for every face, and impact effects are aligned to it
    { "LT1BoxHitNormal",       "1",    LT1_SERVER },
};

// The walkers' last three arguments: the set's name, the table and its length
#define SHOGO_LT1_VARS "Shogo", g_aShogoLT1Vars, (unsigned)(sizeof(g_aShogoLT1Vars) / sizeof(g_aShogoLT1Vars[0]))

#endif
