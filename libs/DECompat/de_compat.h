/*
    DECompat lets LithTech 1.0 era game code compile and run against
    the Jupiter engine without being rewritten first.

    Nothing in this layer changes a data representation.
    Every mapping is a rename onto the Jupiter primitive 
    the converted client shell uses on the other end of the wire.

    This is the server half. The client half (de_client.h, de_clientshell.h) isn't included here,
    as a server module must not see ILTClient.
    Client shells reach it through LT1's client include names in lt1/.
*/

#ifndef __DE_COMPAT_H__
#define __DE_COMPAT_H__

#include "de_types.h"
#include "de_message.h"
#include "de_server.h"
#include "de_engineobjects.h"
#include "de_servershell.h"

#endif // __DE_COMPAT_H__