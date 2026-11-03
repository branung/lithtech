#ifndef __SHOGOLT1VARS_H__
#define __SHOGOLT1VARS_H__

// Shogo's LT1 engine vars, both shells' in one table.
// Each shell walks it for its own side right after the shared LT1 set, so a row here overrides that set,
// and a var both sides need is one LT1_BOTH row. Names are bare, as the client walker adds the '-'

#include "de_lt1defaults.h"

static const LT1Var g_aShogoLT1Vars[] =
{
    // Room for the player view weapon.
    // The engine's default far plane for REALLYCLOSE is 7.0 which clips most of Shogo's guns.
    // '-' keeps it out of autoexec.cfg
    { "reallyclose_far",       "20.0", LT1_CLIENT },

    // Draw the player view weapon with the scene's own projection as LT1 did (it had no separate one)
    { "PVModelSceneFOV",       "1",    LT1_CLIENT },

    // The fullbrite pass adds the masked texel over the lit result, as LT1 does
    { "LT1FullbriteAdd",       "1",    LT1_CLIENT },

    // Sky world models draw through their object transform, so a rotated sky box faces right
    { "LT1SkyObjectTransform", "1",    LT1_CLIENT },

    // Hor+ widescreen, since Shogo's fovY only matches the screen at 4:3.
    // A 4:3 window is pixel identical
    { "WidescreenFOV",         "1",    LT1_CLIENT },

    // LT1's motion for a standing object (both sides)
    // Server: LT1's motion for a standing object, which keeps gravity on it.
    // Without it a standing character flickers between grounded and airborne and loses jumps (client too)
    { "LT1CalcMotion",         "1",    LT1_BOTH },

    // The physics step is LT1's whole CalcMotion, the LT1 era set's module, in place of Jupiter's with LT1CalcMotion
    // inside it. It reproduces all 7,658 of retail's logged calls in a demo, and clips the standing velocity in place
    // as LT1 does, so a resting player is at v 0, not -21.44. LT1CalcMotion above stays set for Jupiter's step
    // if this row is taken out (both sides)
    { "LT1MotionStep",         "1",    LT1_BOTH },

    // A resized box is detached from what it stood on (both sides)
    // Server: a resized box is detached from what it stood on, so its first step from rest falls and lands like LT1 (client too)
    { "LT1DimsDetach",         "1",    LT1_BOTH },

    // The main world is walked once per move, and the stair step's box covers the whole move.
    // Only both together match LT1 on slopes (both sides)
    // Server: walk the main world once per move, and give the stair step a box covering the whole move.
    // Only both together match LT1 on slopes: the swept box alone lifts a climbing player twice, on the world's second walk (client too)
    { "LT1WorldOnce",          "1",    LT1_BOTH },
    { "LT1StairSwept",         "1",    LT1_BOTH },

    // Server: tracked sounds count down by game time, so a character's next pain sound waits for the last one to end in game time
    { "LT1SoundStep",          "1",    LT1_SERVER },

    // Server: an AI nobody has seen for 10 s (AI_DEACTIVATION_TIME) stops updating until the player or a camera pings it.
    // 'autodeactivate 1.0' in the server shell is LT1's own switch, and this turns on the engine's part
    { "LT1AutoDeactivate",     "1",    LT1_SERVER },

    // Server: only send the client objects it could see from its position
    { "LT1ClientPVS",          "1",    LT1_SERVER },

    // Server: pings wake objects in LT1's order, so the server updates them in LT1's order
    { "LT1FileOrder",          "1",    LT1_SERVER },

    // The engine's random stream and its seeds are LT1's, for its particles (both sides)
    // Server: the engine's Random, IntRandom and RandomScale draw from LT1's stream with LT1's seeds,
    // so things like how many pieces a destroyed prop breaks into match og Shogo (client too)
    { "LT1EngineRand",         "1",    LT1_BOTH },

    // A mover's world touch sees where it was stopped, and only a solid object's resize collides (both sides)
    // Server: a mover's world touch sees where it was stopped, which is where a projectile's impact is read from (client too)
    { "LT1WorldTouchPos",      "1",    LT1_BOTH },

    // Server: only a solid object's resize collides. Anything else takes its new dims with no move and no touch (client too)
    { "LT1SolidResize",        "1",    LT1_BOTH },

    // Server: a ray hitting an object's box reports the x face's normal for every face, and impact effects are aligned to it
    { "LT1BoxHitNormal",       "1",    LT1_SERVER },

    // LT1's world rays and point collision, and its collision candidates tested at the mover's destination (both sides)
    // Server: a world ray without INTERSECT_HPOLY walks leaves with a 0.1 tolerance, so debris that sinks a little into a floor rests
    // rather than bouncing (client too)
    { "LT1IntersectLeaf",      "1",    LT1_BOTH },

    // Server: a point collide mover - such as every projectile - uses both LT1 walks' 0.1 tolerance and stops 0.1 short of the plane (client too)
    { "LT1PointCollide",       "1",    LT1_BOTH },

    // Server: collision candidates are tested against the mover's box at its destination,
    // so a player stepping away from an NPC it touches isn't pulled back to it (client too)
    { "LT1MoverBoxGate",       "1",    LT1_BOTH },

    // A fast non solid mover touches at the blocker's face, and a resize grows Y, X, Z with LT1's probes (both sides)
    // Server: a move bigger than the mover's dims takes the swept test even when the pair isn't solid,
    // so a projectile's touch - and its impact point - comes at the target's face (client too)
    { "LT1TunnelNonsolid",     "1",    LT1_BOTH },

    // Server: a resize grows Y, then X, then Z with LT1's three test moves/probes,
    // so a player starting to walk from a wall gets the same box as in LT1 (client too)
    { "LT1GrowDim",            "1",    LT1_BOTH },

    // Tiny move axes are dropped, touches are box overlap alone,
    // and containers use LT1's box against BSP test with its probes (both sides)
    // Server: MoveObject drops any axis of a move under 0.001, so a player against a wall doesn't creep into its clearance (client too)
    { "LT1MoveAxisMin",        "1",    LT1_BOTH },

    // Server: touches are box overlap alone, so stepping onto a resting rocket detonates it (client too)
    { "LT1PointBoxTouch",      "1",    LT1_BOTH },

    // Server: containers use LT1's box against BSP test with its center and corner probes,
    // so Shogo's airshaft fans lift the player when its suppose to (client too)
    { "LT1ContainerBSP",       "1",    LT1_BOTH },
    { "LT1BoxBSPProbes",       "1",    LT1_BOTH },

    // Positions in game messages at LT1's precision, as the server encodes them (both sides)
    // Server: positions in game messages at LT1's precision, so a weapon impact sent to the client
    // is where LT1's explosion debris starts (client too)
    { "LT1CompPos",            "1",    LT1_BOTH },

    // Server: a removed object leaves the world tree at once
    { "LT1RemoveUnlinks",      "1",    LT1_SERVER },

    // SetObjectPos drops moves under 0.0316 units as LT1's does.
    // The weapon fires from the camera object, so this decides where shots start
    { "LT1ClientMoveMin",      "1",    LT1_CLIENT },

    // A sprite's glow and bias reach the client, as LT1's flag mask sent them.
    // The server writes the mask and this client reads it, so both sides set it
    // Server: send a sprite's glow and bias to clients, as LT1's flag mask did (client too)
    { "LT1SpriteFlags",        "1",    LT1_BOTH },

    // LT1's model fullbright pass: seen in the glowing parts of Shogo's Laser Cannon and Pulse Rifle skins
    { "LT1ModelFullbrite",     "1",    LT1_CLIENT },

    // LT1's model dynamic lighting: a dynamic light lights a model as one object, measured at its position.
    // A Pulse Rifle hit in Shogo lights a whole truck or enemy in retail, where Jupiter's point lights leave a faint spot
    { "LT1ModelDynLight",      "1",    LT1_CLIENT },

    // Demo playback as og runs it, playback only: the server's step floored at 0.01 s,
    // no held frame after the world loads, the client's clock is the recording's,
    // and at the end the console comes up over the still running level
    // Server: during demo playback the server's step is floored at 0.01 s, as LT1's is (client too)
    { "LT1ServerStepFloor",    "1",    LT1_BOTH },
    { "LT1DemoNoPrime",        "1",    LT1_CLIENT },
    { "LT1DemoClientTime",     "1",    LT1_CLIENT },
    { "LT1DemoEndConsole",     "1",    LT1_CLIENT },

    // LT1 picks the path that prevents tunneling by geometry, not FLAG2_PLAYERCOLLIDE (both sides)
    // Server: LT1 picks the path that prevents tunneling geometrically, Jupiter by a flag no 1998 game sets (client too)
    { "LT1TunnelPath",         "1",    LT1_BOTH },
};

// The walkers' last three arguments: the set's name, the table and its length
#define SHOGO_LT1_VARS "Shogo", g_aShogoLT1Vars, (unsigned)(sizeof(g_aShogoLT1Vars) / sizeof(g_aShogoLT1Vars[0]))

#endif
