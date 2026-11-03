// iltlt1server.h: LT1 server behavior Jupiter dropped, reimplemented for LT1 games.

// LT1's auto deactivation: an object stops updating when its deactivation time runs out,
// unless something that can see it (ex the player, a live camera) pings it first.
// Implemented in runtime/server/src/s_lt1deactivate.cpp, and active only under LT1AutoDeactivate

// LT1's GetModelCommandString: the command string in a model file, where Shogo's weapon models carry their AI fire timings

// LT1's user portals: named brushes across doorways that doors open. An open portal lets the server's
// visibility walk see past it, so clients are sent and pings reach what's beyond it

// LT1's server side command states: each client's keys as its frame start packet sent them, for IsCommandOn
// and the server shell's OnCommandOn. Active only under LT1ServerCommands

#ifndef __ILTLT1SERVER_H__
#define __ILTLT1SERVER_H__

#include "ltbasedefs.h"
#include "ltmodule.h"

class ILTLT1Server : public IBase
{
public:
    interface_version(ILTLT1Server, 0);

    // Sets the interval and countdown and wakes a deactivated object.
    // 0 never deactivates
    virtual void SetDeactivationTime(HOBJECT hObj, float fTime) = 0;

    // Refreshes every object the visibility walk reaches from hObj
    virtual void PingObjects(HOBJECT hObj) = 0;

    // SetObjectState(OBJSTATE_AUTODEACTIVATE_NOW).
    // False when LT1AutoDeactivate is off
    virtual bool AutoDeactivateNow(HOBJECT hObj) = 0;

    // Copies the model file's command string into pStr (an empty string when it has none).
    // LT_INVALIDPARAMS when hModel isn't a model
    virtual LTRESULT GetModelCommandString(HOBJECT hModel, char *pStr, uint32 maxLen) = 0;

    // LT1's GetPortalFlags and SetPortalFlags: a user portal's flags, by its name in the editor.
    // Portals start closed. LT_NOTFOUND when the world has no portal of that name
    virtual LTRESULT GetPortalFlags(const char *pName, uint32 *pFlags) = 0;
    virtual LTRESULT SetPortalFlags(const char *pName, uint32 dwFlags) = 0;

    // LT1's IsCommandOn: the client's command state this frame.
    // False, leaving *pbOn alone, when the engine keeps no states (LT1ServerCommands off, or nothing from that client yet)
    virtual bool IsCommandOn(HCLIENT hClient, int nCommand, bool *pbOn) = 0;

    // LT1's server shell OnCommandOn and OnCommandOff, which Jupiter's shell lacks:
    // fn is called for each command the client's frame start packet changed, after the new state is stored
    typedef void (*CommandChangeFn)(HCLIENT hClient, int nCommand, bool bOn);
    virtual void SetCommandCallback(CommandChangeFn fn) = 0;
};

#endif
