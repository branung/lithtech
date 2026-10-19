// iltlt1server.h: LT1 server behavior Jupiter dropped, reimplemented for LT1 games.

// LT1's auto deactivation: an object stops updating when its deactivation time runs out,
// unless something that can see it (ex the player, a live camera) pings it first.
// Implemented in runtime/server/src/s_lt1deactivate.cpp, and active only under LT1AutoDeactivate

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
};

#endif
