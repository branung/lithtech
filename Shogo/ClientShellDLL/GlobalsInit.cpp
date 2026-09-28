// ----------------------------------------------------------------------- //
//
// MODULE  : GlobalsInit.cpp
//
// PURPOSE : Globals the Shogo client declares extern and never defines
//
// ----------------------------------------------------------------------- //

#include "clientheaders.h"

// Shogo's name for the client interface.
// It lives here since define_holder would collide with SETUP_CLIENTSHELL()'s
ILTClient*	g_pClientDE = NULL;
define_holder(ILTClient, g_pClientDE);
