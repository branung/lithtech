/*
    Sets the LT1 engine variables, one call per side

    Each side compiles its own engine_vars.cpp, so a physics knob set on one side only splits the two.
    Set game overrides after the call, from the game's own var table.
    The '-' prefix keeps them out of the saved settings.
    Linking DECompat is what marks a game as LT1. There's no detection from asset formats.
*/

#ifndef __DE_LT1DEFAULTS_H__
#define __DE_LT1DEFAULTS_H__

class ILTClient;
class ILTServer;

// One row of an LT1 var table: a console setting and the sides it's applied on.
// The name is bare, as each walker adds its own side's prefix
enum LT1Side { LT1_CLIENT = 1, LT1_SERVER = 2, LT1_BOTH = LT1_CLIENT | LT1_SERVER };

struct LT1Var
{
    const char *pszName;
    const char *pszValue;
    unsigned    nSides;
};

// Walk a table for one side in its order, never sorted, as later rows win.
// Each walk then prints "DECompat: <set> <side> set applied (<n>) -- name=value ..."
void DECompat_ApplyLT1ClientVars(ILTClient *pClientDE, const char *pszSet, const LT1Var *pa, unsigned n);
void DECompat_ApplyLT1ServerVars(ILTServer *pServerDE, const char *pszSet, const LT1Var *pa, unsigned n);

// Call from OnEngineInitialized, with game overrides after
void DECompat_ApplyLT1ClientDefaults(ILTClient *pClientDE);

// Call from OnServerInitialized.
// A knob the server acts on is a both sides row in the shared set.
void DECompat_ApplyLT1ServerDefaults(ILTServer *pServerDE);


/*
    Called by DECompat's server code as a sign of life.
    Warns if the server never applied its defaults,
    after a few hundred messages so it can't fire early.
*/
void DECompat_NoteServerActivity(ILTServer *pServerDE);

#endif  // __DE_LT1DEFAULTS_H__
