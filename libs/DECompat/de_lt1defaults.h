/*
    Sets the LT1 engine variables, one call per side

    Each side compiles its own engine_vars.cpp, so a physics knob set on one side only splits the two.
    Set game overrides after the call (DynamicLightWorld, LT1WorldCull with LT1WorldSort, LT1StairStep).
    The '-' prefix keeps them out of the saved settings.
    Linking DECompat is what marks a game as LT1. There's no detection from asset formats.
*/

#ifndef __DE_LT1DEFAULTS_H__
#define __DE_LT1DEFAULTS_H__

class ILTClient;
class ILTServer;

// Call from OnEngineInitialized, with game overrides after
void DECompat_ApplyLT1ClientDefaults(ILTClient *pClientDE);

// Call from OnServerInitialized.
// A knob the server acts on goes here as well as in the client half.
void DECompat_ApplyLT1ServerDefaults(ILTServer *pServerDE);


/*
    Called by DECompat's server code as a sign of life.
    Warns if the server never applied its defaults,
    after a few hundred messages so it can't fire early.
*/
void DECompat_NoteServerActivity(ILTServer *pServerDE);

#endif  // __DE_LT1DEFAULTS_H__
