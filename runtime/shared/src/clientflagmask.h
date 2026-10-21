#ifndef __CLIENTFLAGMASK_H__
#define __CLIENTFLAGMASK_H__

/*
	The object flags a server sends to its clients. The server's write and the client's read must use the same mask.

	Under LT1AnimTransition FLAG_ANIMTRANSITION is sent too, so the client holds a new animation as LT1's did.
	Without it the client's animations end 200 ms early and snap.

	Bit 22 doesn't fit in 16 bits, so the flags are always sent as 32 bits.
	A size that changed with the switch would misread packets sent before the change.

	Under LT1SpriteFlags FLAG_GLOWSPRITE and FLAG_SPRITEBIAS are sent too, as LT1's mask sent a sprite's glow and bias.
	Shogo's level flares carry both
*/

#include "ltbasedefs.h"

extern int32 g_bLT1AnimTransition;
extern int32 g_bLT1SpriteFlags;

inline uint32 ClientFlagMask()
{
	return (uint32)CLIENT_FLAGMASK
		| (g_bLT1AnimTransition ? (uint32)FLAG_ANIMTRANSITION : 0u)
		| (g_bLT1SpriteFlags ? (uint32)(FLAG_GLOWSPRITE | FLAG_SPRITEBIAS) : 0u);
}

#endif
