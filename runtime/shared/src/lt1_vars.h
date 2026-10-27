#ifndef __LT1_VARS_H__
#define __LT1_VARS_H__

// The engine variables that switch on LT1 behavior, for the LT1 games (ie Shogo and Blood 2).
// Each is defined and described in engine_vars.cpp, and is off unless a game sets it. So Jupiter games keep their original behavior.
// Shared LT1 defaults are set in libs/DECompat/de_lt1defaults.cpp, and per game ones in the game's client and server shells.

#include "ltinteger.h"

// Demo playback
extern int32 g_bLT1ServerStepFloor;
extern int32 g_bLT1DemoNoPrime;
extern int32 g_bLT1DemoClientTime;
extern int32 g_bLT1DemoEndConsole;

// Textures
extern int32 g_bLT1Fullbrite;
extern int32 g_nLT1FullbriteIndex;
extern int32 g_bLT1PaletteAlpha;

// Objects, dims and animation
extern int32 g_bLT1CreateInPlace;
extern int32 g_bLT1ModelAnimNoRestart;
extern int32 g_bLT1AnimTransition;
extern int32 g_bLT1ObjectDims;
extern int32 g_bLT1DimsDetach;
extern int32 g_bLT1SolidResize;
extern int32 g_bLT1GrowDim;
extern int32 g_bLT1ContainerType;
extern int32 g_bLT1RemoveUnlinks;
extern int32 g_bLT1ClientIDFromOne;

// Motion and stairs
extern int32 g_bLT1CalcMotion;
extern int32 g_bLT1StandVelStore;
extern int32 g_bLT1StandGravity;
extern int32 g_bLT1StairStep;
extern int32 g_bLT1StairSwept;
extern int32 g_bLT1StairDamp;

// Collision
extern int32 g_bLT1WorldOnce;
extern int32 g_bLT1WorldAfterObjects;
extern int32 g_bLT1StrikeKeep;
extern int32 g_bLT1TunnelPath;
extern int32 g_bLT1TunnelNonsolid;
extern int32 g_bLT1NonsolidNoDivert;
extern int32 g_bLT1MoverBoxGate;
extern int32 g_bLT1SetupBoxCheck;
extern int32 g_bLT1IntersectPushback;	// The pushback loop in ClipBoxIntoTree2
extern int32 g_bLT1PlaneRecheck;		// LT1RecheckAccumulatedPlanes
extern int32 g_bLT1SweepRebuild;		// The SetupBox call after MoveToFrontside
extern int32 g_bLT1PolyTest;			// WorldPolyIntersectsAABB
extern int32 g_bLT1PointCollide;
extern int32 g_bLT1WorldTouchPos;
extern int32 g_bLT1CarryNoNudge;
extern int32 g_bLT1MinMove;
extern int32 g_bLT1RotateNoMovingFlag;
extern int32 g_bLT1MoveAxisMin;
extern int32 g_bLT1PointBoxTouch;
extern int32 g_bLT1ContainerBSP;
extern int32 g_bLT1BoxBSPProbes;
extern int32 g_bLT1BoxSlide;

// Rays
extern int32 g_bLT1IntersectLeaf;
extern int32 g_bLT1BoxHitNormal;
extern int32 g_bLT1RaySolidOnly;

// Server
extern int32 g_bLT1SoundStep;
extern int32 g_bLT1AutoDeactivate;
extern int32 g_bLT1ClientPVS;
extern int32 g_bLT1FileOrder;
extern int32 g_bLT1EngineRand;

// Network
extern int32 g_bLT1NetInterp;
extern int32 g_bLT1CompPos;
extern int32 g_bLT1SpriteFlags;

// Client
extern int32 g_bLT1ClientMoveMin;
extern int32 g_bLT1VirtualKeys;

// Rendering
extern int32 g_bLT1ObjectColor;
extern int32 g_bLT1ModelShading;
extern int32 g_bLT1ModelFullbrite;
extern int32 g_bLT1ModelDynLight;
extern int32 g_bLT1SkyObjectTransform;
extern int32 g_bLT1LightTableY;
extern int32 g_bLT1FullbriteAdd;
extern int32 g_bLT1WorldSort;
extern int32 g_bLT1WorldCull;
extern int32 g_bLT1PolyGridCull;
extern int32 g_bLT1PolyGridTexCoords;

// Diagnostics
extern int32 g_LT1DeactivateTrace;

#endif
