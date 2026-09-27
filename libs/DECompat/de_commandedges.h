/*
    Command edge detection for LT1 game code

    In LT1 the engine told the server shell whenever a player pressed a command key (jump, use, fire, etc).
    Jupiter doesn't however, so the client has to send its command state in the player update.
    An LT1 game can then pass the old and new state to DECompat_FireCommandEdges, 
    which calls back for each command that just went down so the game can fire OnCommandOn as before.
*/

#ifndef __DE_COMMANDEDGES_H__
#define __DE_COMMANDEDGES_H__

#ifndef __LTBASETYPES_H__
#include "ltbasetypes.h"
#endif

class ILTServer;


// Called once per command that went from clear to set.
// nCommand is the bit index in the mask
typedef void (*DECompat_CommandEdgeFn)(void *pContext, int nCommand);


// Fires pfnOn for every rising edge between pOld and pNew
void DECompat_FireCommandEdges(const uint32 *pOld, const uint32 *pNew,
                               uint32 nWords,
                               DECompat_CommandEdgeFn pfnOn, void *pContext);

void DECompat_FireCommandEdges(const uint64 *pOld, const uint64 *pNew,
                               uint32 nWords,
                               DECompat_CommandEdgeFn pfnOn, void *pContext);


// Whether any command has fired this way in this process
bool DECompat_CommandEdgesSeen();

// Where the one time confirmation prints
void DECompat_SetCommandEdgeReportSink(ILTServer *pServerDE);

#endif  // __DE_COMMANDEDGES_H__
