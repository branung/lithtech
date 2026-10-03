
// This module defines all the engine's console variables.
// NOTE: the reason some of the variables are extern'd is because certain modules don't
// compile with engine_vars.cpp so they 'own' the variable.

#include "bdefs.h"
#include "ltbasetypes.h"
#include "concommand.h"
#include "packetdefs.h"




extern float g_CV_DefaultDrawIndexedDist;

int32	g_CV_RenderEnable = LTTRUE;

int32	g_CV_PlayDemoReps = 0;	// How many times to repeat the PlayDemo.

int32	g_CV_ForceNoSound = LTFALSE;
int32	g_CV_BorderlessWindow = LTFALSE; // Create the game window without a frame (fullscreen at desktop resolution)
int32	g_CV_NoSaveConfig = LTFALSE; // Skip rewriting autoexec.cfg on exit

int32	g_CV_CursorCenter = LTTRUE;	// Automatically resets the cursor to the center 
									// of the window/screen each frame

int32	g_CV_VideoDebug = LTFALSE;

int32	g_CV_DebugLoaders = LTFALSE; // Debug output for loader threads?


int32	g_CV_STracePackets = LTFALSE;
int32	g_CV_DelimitPackets = LTTRUE;

int32	g_CV_MeasurePackets = LTFALSE; // Used to have the server build compression tables.


const char	*g_CV_BindIP = LTNULL;
const char	*g_CV_IP = LTNULL;

const char*	g_CV_Console_FontTexFile = NULL;
const char*	g_CV_Console_FontFace = NULL; // System font face the console falls back to without a console font texture
int32	g_CV_NoDefaultEngineRez = 0;

int32	g_CV_ForceConsole = 0;
int32	g_CV_IPDevice = 0;
int32	g_CV_IPDebug = LTFALSE;
int32	g_CV_UDPDebug = LTFALSE;

float	g_CV_IPQueryTimeout = 30.0f;
float	g_CV_MaxFPS = 0.0f;

int32	g_CV_InputDebug = LTFALSE;
int32	g_CV_JoystickDisable = LTFALSE;

int32	g_CV_UpdateRate = DEFAULT_CLIENT_UPDATE_RATE;

int32	g_CV_ConnTroubleCount = 50;
int32	g_CV_ConnTroubleCount2 = 70;
int32	g_CV_ConnTroubleCount3 = 50;

int32	g_CV_TraceConsole = LTFALSE;

int32	g_CV_ShowFileAccess = LTFALSE;

int32	g_CV_ShowConnStats = LTFALSE;

int32	g_CV_FullLightScale = LTFALSE;

int32	g_CV_ForceClear = LTFALSE;	// Force the engine to clear the screen.

int32	g_CV_Show2DScale = LTFALSE;

// The interface upscale
float	g_CV_UI2DScaleX = 0.0f;
float	g_CV_UI2DScaleY = 0.0f;
float	g_CV_UI2DOffsetX = 0.0f;
float	g_CV_UI2DOffsetY = 0.0f;
int32	g_CV_UI2DMarginColor = -1;

// LT1 texture decoding. Doesn't touch any Jupiter textures as it's only read by dtxmgr_lt1.cpp
int32	g_bLT1Fullbrite      = 2;
int32	g_nLT1FullbriteIndex = 246;
int32	g_bLT1PaletteAlpha   = 1;

int32	g_bLocalPreLoadWorld = 1; // PreLoadWorld for a local client too, not just a remote one
int32	g_bLT1CreateInPlace = 0; // Placement move before InitialUpdate, so a new object isn't sent MID_TOUCHNOTIFY
int32	g_bLT1ModelAnimNoRestart = 0; // Ignore a request for the animation already playing.
int32	g_bLT1StandGravity = 0; // Keep gravity on a standing object, as LT1 did
int32	g_bLT1SetupBoxCheck = 0; // Stop a sweep SetupBox says is too short, and rebuild the box during pushback like LT1
int32	g_bLT1RaySolidOnly = 0; // Only solid polys stop a ray, so LT1 doorway polys pass shots and sight
int32	g_bLT1ClientIDFromOne = 0; // Hand out client ids from 1 (LT1 reserved 0 for 'no player')
int32	g_bLT1StairStep = 0; // LT1's stair step: floor choice, push along the normal, no SURF_NOTASTEP gate
int32	g_bLT1StairDamp = 1; // Keep the stair step's velocity damping as LT1 did
int32	g_bLT1IntersectPushback = 0; // Push an embedded box along the plane normal as well as back, leaving P0 in place.
int32	g_bLT1PlaneRecheck = 0; // Push the box back out to 0.1 from every plane touched in one collide
int32	g_bLT1SweepRebuild = 0; // Rebuild the swept box after each resolution
int32	g_bLT1PolyTest = 0; // LT1's polygon against box clip test: a polygon exactly on a face is a miss
int32	g_bLT1ContainerType = 0; // An object's type (OT_CONTAINER) makes it a container (LT1 had no FLAG_CONTAINER)
int32	g_bLT1TunnelPath = 0; // Anti-tunnelling path for any move bigger than the mover's dims
int32	g_bLT1NonsolidNoDivert = 0; // Only the main world pushes a nonsolid mover
int32	g_bLT1StrikeKeep = 0; // Stop at the first repeat hit and keep the resolved position
int32	g_bLT1WorldAfterObjects = 0; // Test the main world after the object loop if the loop skipped it
int32	g_bLT1ObjectDims = 0; // Keep a partial resize and place it with a swept move
int32	g_bLT1ObjectColor = 0; // A model's object color brightens it from a (0,0,0) default rather than scaling its light
int32	g_bLT1SkyObjectTransform = 0; // Draw a sky WorldModel through its own transform so that its rotation applies
int32	g_bLT1LightTableY = 0; // Read the object light grid's y rows the right way round
int32	g_bLT1FullbriteAdd = 0; // Add the fullbright texel to the lit world rather than blend it over
int32	g_bLT1WorldSort = 0; // A WorldModel whose polygon 0 is SURF_TRANSPARENT sorts as translucent
int32	g_bLT1WorldCull = 0; // Those translucent WorldModels draw without backface culling
int32	g_bLT1PolyGridCull = 0; // Draw polygrids without backface culling
int32	g_bLT1PolyGridTexCoords = 0; // Polygrid scale is world units per texel and its pan is in texels
int32	g_bLT1ModelShading = 0; // Shade models with LT1's fixed direction ramp rather than flat ambient
int32	g_bWidescreenFOV = 0; // Horizontal widescreen - widens the horizontal FOV by aspect/4:3 at render time

int32	g_CV_MasterPaletteMode = 2;

int32	g_CV_ModelTransitionMS = 200;

int32	g_CV_ModelOnlyUpdateDirtyTrackers = 1;

float	g_CV_LatencySim = 0.0f;	// Simulate latency.
float	g_CV_DropRate = 0.0f;   // Simulate packet drops - affects both client and server at the same time

int32	g_CV_InputRate=0;		// The amount of time an input sample is spread over in milliseconds


int32	g_bForceRemote = LTFALSE;	// Force remote connection even if it's local.. less efficient.
int32	g_bShowRunningTime = LTFALSE;	// Shows how long the engine has been running.
LTBOOL	g_bNullRender = LTFALSE;	// Set this when using NullRender.. the engine
									// will do things to make it work better. 

LTBOOL	g_CV_CollideParticles = LTTRUE;	// Should marked particle systems handle collisions with other objects?

LTBOOL	g_CV_HighPriority = LTFALSE;	// Should the process be set into high priority?

int32	g_bLocalDebug = LTFALSE;
int32	g_bPrediction = LTTRUE;
int32	g_nPredictionLines = 0; // Draw lines showing the results of client-side prediction
  								// 0 = no lines   1 = track prediction   >1 = stop 
int32	g_TransportDebug = 0;
int32	g_CV_ParseNet_Incoming = 0; // Parse incoming network packets
int32	g_CV_ParseNet_Outgoing = 0; // Parse outgoing network packets
int32   g_CV_ParseNet = 0; // Parse network packets going either direction
int32	g_ClientSleepMS = 0;
int32	g_ShowTickCounts = 0;
int32	g_bShowMemStats = LTFALSE;
int32	g_bDebugStrings = LTFALSE;
int32	g_bDebugPackets = LTFALSE;
int32	g_CV_NetMaxQueue = 32;

float	g_CV_MaxExtrapolateTime = 0.5f;  // Maximum amount of time to extrapolate a position

float	g_CV_NearRoundoff = 0.001f; // Max distance to consider two points in the same place

LTVector	g_ConsoleModelAdd(0.0f, 0.0f, 0.0f);
LTVector	g_ConsoleModelDirAdd(0.0f, 0.0f, 0.0f);
LTCommandVar	*g_pNameVar = LTNULL;
LTBOOL		g_bUpdateServer = LTTRUE;

int32	g_nSoundDebugLevel = 0;
LTBOOL	g_bSoundShowCounts = LTFALSE;

int32	g_bErrorLog = LTFALSE;
int32	g_bAlwaysFlushLog = LTFALSE;

int32	g_CV_BitDepth = 32;
LTBOOL  g_bHWTnLDisabled = LTFALSE;
int32	g_bSoftware = LTFALSE;
int32	g_ScreenWidth = 640, g_ScreenHeight = 480;
int32	g_CV_ShowFrameRate = LTFALSE;
int32	g_nConsoleLines = 6;
LTBOOL  g_bConsoleEnable = LTTRUE;
char	g_SSFile[_MAX_PATH + 1] = "Screenshot";

int32	g_bDoExtraObjectStuff = LTFALSE;

LTBOOL	g_bMusicEnable = LTTRUE;
LTBOOL	g_bSoundEnable = LTTRUE;
int32	g_CV_ForceSoundDisable = LTFALSE; // Force it to not initialize music or sound (behind the game's back).

float	g_fLodScale = 1.0f;

// Framerate for server logic.
float	g_ServerFPS = 30.0f;
#ifdef DE_SERVER_COMPILE
int32	g_LockServerFPS = 0;
#endif // DE_SERVER_COMPILE

// Scale the time to make things go slower or faster.
float	g_ServerTimeScale = 1.0f;

// If an object moves to a position with coord larger than this, it'll complain in the console.
float	g_DebugMaxPos = 0.0f;

// If they do a SetObjectDims larger than this, it'll complain in the console.
float	g_DebugMaxDims = 3000.0f;

int32	g_CV_ShowGameTime = LTFALSE;

int32	g_CV_ShowClassTicks = LTFALSE;
const char	*g_CV_ShowClassTicksSpecific = LTNULL;

int32	g_CV_ShowSphereFindTicks = LTFALSE;

// Console attributes
int32	g_CV_ConsoleHistoryLen = 20;
int32	g_CV_ConsoleBufferLen = 500;
float	g_CV_ConsoleAlpha = 1.0f;		// Note : OptimizeSurfaces must be 1 for this to have any effect
int32	g_CV_ConsoleLeft = -1;	// Note : Negative values move the edge away from the screen edge by
int32	g_CV_ConsoleTop = -1;	//	that positive fraction of the screen dimension.  (-1 = no offset)
int32	g_CV_ConsoleRight = -1;
int32	g_CV_ConsoleBottom = -2;

// The LithTech DirectMusic console output level
int32	g_CV_LTDMConsoleOutput = 2;

int32	g_CV_CompressLightGrid		 = 2;

int32	g_CV_DrawDebugGeometry		 = 0;
int32	g_CV_DebugModelRez           = 0;


//Texture groups, used to control offsetting of Mip Maps when they are loaded, thus allowing
//us to not waste any memory loading mips higher than those that we are going to use. Note that
//this define needs to match the one in the texture loading code. This is ugly, but there is no
//shared header.
#define MAX_TEXTURE_GROUPS 4
int32	g_CV_TextureMipMapOffset	= 0;
int32	g_CV_TextureGroupOffset[MAX_TEXTURE_GROUPS];

#ifdef LITHTECH_ESD
int32 g_CV_LTRConsoleOutput = 1;
#endif // LITHTECH_ESD

//int32 g_CV_ShowFrameRate = 0;
int32 g_CV_OrientFrameRate = 0;

// default client port number, range: 0 or (recommended) 1024 - 65535.
// [default] value of zero means "let system choose the port number".
int32 g_CV_IPClientPort = 0;			// base of IPClientPort range.  See additional description info in udpdriver.cpp
int32 g_CV_IPClientPortRange = 1;		// client port range
int32 g_CV_IPClientPortMRU = 0;			// most-recently-used client port

int32 g_CV_BandwidthTargetClient = 256000; // client send bandwidth target in bits-per-second (n/a for local unless "ForceRemote" is set)
int32 g_CV_BandwidthTargetServer = 256000; // server send bandwidth target in bits-per-second (n/a for local unless "ForceRemote" is set)

int32 g_CV_NewPlayerPhysics = 1;	// Use the new player physics

int32 g_CV_UDPSimulatePacketLoss = 0;
int32 g_CV_UDPSimulateCorruption = 0;

//------------------------------------------------------------------
//------------------------------------------------------------------
// The main table of command variables
//------------------------------------------------------------------
//------------------------------------------------------------------

static LTEngineVar g_LTEngineVars[] =
{
	EV_STRING("BindIP", &g_CV_BindIP),
	EV_STRING("IP", &g_CV_IP),

	EV_STRING("Console_FontTexFile", &g_CV_Console_FontTexFile),
	EV_STRING("Console_FontFace", &g_CV_Console_FontFace),
	EV_LONG("NoDefaultEngineRez", &g_CV_NoDefaultEngineRez),

	EV_LONG("RenderEnable", &g_CV_RenderEnable),
	EV_LONG("PlayDemoReps", &g_CV_PlayDemoReps),
	EV_LONG("ForceNoSound", &g_CV_ForceNoSound),
	
	EV_LONG("CursorCenter", &g_CV_CursorCenter),
	EV_LONG("BorderlessWindow", &g_CV_BorderlessWindow),
	EV_LONG("NoSaveConfig", &g_CV_NoSaveConfig),
	

	EV_LONG("VideoDebug", &g_CV_VideoDebug),
	EV_LONG("DebugLoaders", &g_CV_DebugLoaders),
	EV_LONG("MeasurePackets", &g_CV_MeasurePackets),
	
	EV_LONG("STracePackets", &g_CV_STracePackets),
	EV_LONG("DelimitPackets", &g_CV_DelimitPackets),
	EV_LONG("ForceConsole", &g_CV_ForceConsole),
	EV_LONG("IPDevice", &g_CV_IPDevice),
	EV_LONG("IPDebug", &g_CV_IPDebug),
	EV_LONG("UDPDebug", &g_CV_UDPDebug),
	EV_LONG("InputDebug", &g_CV_InputDebug),
	EV_LONG("UpdateRate", &g_CV_UpdateRate),
	EV_LONG("InputRate", &g_CV_InputRate),
	EV_LONG("ForceRemote", &g_bForceRemote),
	EV_LONG("DebugLevel", &g_DebugLevel),
	EV_LONG("ShowRunningTime", &g_bShowRunningTime),
	EV_LONG("NullRender", &g_bNullRender),
	EV_LONG("LocalDebug", &g_bLocalDebug),
	EV_LONG("TransportDebug", &g_TransportDebug),
	EV_LONG("ParseNet_Incoming", &g_CV_ParseNet_Incoming),
	EV_LONG("ParseNet_Outgoing", &g_CV_ParseNet_Outgoing),
	EV_LONG("ParseNet", &g_CV_ParseNet),
	EV_LONG("NetMaxQueue", &g_CV_NetMaxQueue),
	EV_LONG("ClientSleepMS", &g_ClientSleepMS),
	EV_LONG("ShowTickCounts", &g_ShowTickCounts),
	EV_LONG("ShowMemStats", &g_bShowMemStats),
	EV_LONG("Prediction", &g_bPrediction),
	EV_LONG("PredictionLines", &g_nPredictionLines),
	EV_LONG("DebugStrings", &g_bDebugStrings),
	EV_LONG("DebugPackets", &g_bDebugPackets),
	EV_LONG("DoExtraObjectStuff", &g_bDoExtraObjectStuff),
	EV_LONG("CollideParticles", &g_CV_CollideParticles),
	EV_LONG("SoundShowCounts", &g_bSoundShowCounts),
	EV_LONG("SoundDebugLevel", &g_nSoundDebugLevel),
	EV_LONG("ErrorLog", &g_bErrorLog),
	EV_LONG("AlwaysFlushLog", &g_bAlwaysFlushLog),
	EV_LONG("ShowFrameRate", &g_CV_ShowFrameRate),
	EV_LONG("NumConsoleLines", &g_nConsoleLines),
	EV_LONG("ConsoleEnable", &g_bConsoleEnable),
	EV_LONG("ScreenWidth", &g_ScreenWidth),
	EV_LONG("ScreenHeight", &g_ScreenHeight),
	EV_LONG("Software", &g_bSoftware),
	EV_LONG("BitDepth", &g_CV_BitDepth),
	EV_LONG("HWTnLDisabled", &g_bHWTnLDisabled),
	EV_LONG("MusicEnable", &g_bMusicEnable),
	EV_LONG("SoundEnable", &g_bSoundEnable),
	EV_LONG("ForceSoundDisable", &g_CV_ForceSoundDisable),
	EV_LONG("ShowSphereFindTicks", &g_CV_ShowSphereFindTicks),
	EV_LONG("ShowClassTicks", &g_CV_ShowClassTicks),
	EV_STRING("ShowClassTicksSpecific", &g_CV_ShowClassTicksSpecific),
	EV_LONG("ShowGameTime", &g_CV_ShowGameTime),
	EV_LONG("JoystickDisable", &g_CV_JoystickDisable),
	EV_LONG("TraceConsole", &g_CV_TraceConsole),
	EV_LONG("ShowFileAccess", &g_CV_ShowFileAccess),
	EV_LONG("Show2DScale", &g_CV_Show2DScale),
	EV_FLOAT("UI2DScaleX", &g_CV_UI2DScaleX),
	EV_FLOAT("UI2DScaleY", &g_CV_UI2DScaleY),
	EV_FLOAT("UI2DOffsetX", &g_CV_UI2DOffsetX),
	EV_FLOAT("UI2DOffsetY", &g_CV_UI2DOffsetY),
	EV_LONG("UI2DMarginColor", &g_CV_UI2DMarginColor),
	EV_LONG("LT1Fullbrite", &g_bLT1Fullbrite),
	EV_LONG("LT1FullbriteIndex", &g_nLT1FullbriteIndex),
	EV_LONG("LT1PaletteAlpha", &g_bLT1PaletteAlpha),
	EV_LONG("LocalPreLoadWorld", &g_bLocalPreLoadWorld),
	EV_LONG("LT1CreateInPlace", &g_bLT1CreateInPlace),
	EV_LONG("LT1ModelAnimNoRestart", &g_bLT1ModelAnimNoRestart),
	EV_LONG("LT1StandGravity", &g_bLT1StandGravity),
	EV_LONG("LT1SetupBoxCheck", &g_bLT1SetupBoxCheck),
	EV_LONG("LT1RaySolidOnly", &g_bLT1RaySolidOnly),
	EV_LONG("LT1ClientIDFromOne", &g_bLT1ClientIDFromOne),
	EV_LONG("LT1StairStep", &g_bLT1StairStep),
	EV_LONG("LT1StairDamp", &g_bLT1StairDamp),
	EV_LONG("LT1IntersectPushback", &g_bLT1IntersectPushback),
	EV_LONG("LT1PlaneRecheck", &g_bLT1PlaneRecheck),
	EV_LONG("LT1SweepRebuild", &g_bLT1SweepRebuild),
	EV_LONG("LT1PolyTest", &g_bLT1PolyTest),
	EV_LONG("LT1ContainerType", &g_bLT1ContainerType),
	EV_LONG("LT1TunnelPath", &g_bLT1TunnelPath),
	EV_LONG("LT1NonsolidNoDivert", &g_bLT1NonsolidNoDivert),
	EV_LONG("LT1StrikeKeep", &g_bLT1StrikeKeep),
	EV_LONG("LT1WorldAfterObjects", &g_bLT1WorldAfterObjects),
	EV_LONG("LT1ObjectDims", &g_bLT1ObjectDims),
	EV_LONG("LT1ObjectColor", &g_bLT1ObjectColor),
	EV_LONG("LT1SkyObjectTransform", &g_bLT1SkyObjectTransform),
	EV_LONG("LT1LightTableY", &g_bLT1LightTableY),
	EV_LONG("LT1FullbriteAdd", &g_bLT1FullbriteAdd),
	EV_LONG("LT1WorldSort", &g_bLT1WorldSort),
	EV_LONG("LT1WorldCull", &g_bLT1WorldCull),
	EV_LONG("LT1PolyGridCull", &g_bLT1PolyGridCull),
	EV_LONG("LT1PolyGridTexCoords", &g_bLT1PolyGridTexCoords),
	EV_LONG("LT1ModelShading", &g_bLT1ModelShading),
	EV_LONG("WidescreenFOV", &g_bWidescreenFOV),
	EV_LONG("FullLightScale", &g_CV_FullLightScale),
	EV_LONG("ForceClear", &g_CV_ForceClear),
	EV_LONG("ModelTransitionMS", &g_CV_ModelTransitionMS),
	EV_LONG("HighPriority", &g_CV_HighPriority),
	EV_LONG("ShowConnStats", &g_CV_ShowConnStats),
	EV_LONG("MasterPaletteMode", &g_CV_MasterPaletteMode),

	EV_LONG("LTDMConsoleOutput", &g_CV_LTDMConsoleOutput),

	EV_LONG("MipMapOffset", &g_CV_TextureMipMapOffset),
	EV_LONG("GroupOffset0", &g_CV_TextureGroupOffset[0]),
	EV_LONG("GroupOffset1", &g_CV_TextureGroupOffset[1]),
	EV_LONG("GroupOffset2", &g_CV_TextureGroupOffset[2]),
	EV_LONG("GroupOffset3", &g_CV_TextureGroupOffset[3]),
	
	EV_CVAR("Name", &g_pNameVar),

	#ifdef LITHTECH_ESD
	EV_LONG("LTRConsoleOutput", &g_CV_LTRConsoleOutput),
	#endif // LITHTECH_ESD

	EV_FLOAT("DefaultDrawIndexedDist", &g_CV_DefaultDrawIndexedDist),
	EV_FLOAT("MaxFPS", &g_CV_MaxFPS),
	EV_FLOAT("LatencySim", &g_CV_LatencySim),
	EV_FLOAT("DropRate", &g_CV_DropRate),
	EV_FLOAT("LODScale", &g_fLodScale),
	EV_FLOAT("DebugMaxDims", &g_DebugMaxDims),
	EV_FLOAT("DebugMaxPos", &g_DebugMaxPos),
	EV_FLOAT("TimeScale", &g_ServerTimeScale),
	EV_FLOAT("MaxExtrapolateTime", &g_CV_MaxExtrapolateTime),
	EV_FLOAT("NearRoundoff", &g_CV_NearRoundoff),

	EV_LONG("ConsoleHistoryLen", &g_CV_ConsoleHistoryLen),
	EV_LONG("ConsoleBufferLen", &g_CV_ConsoleBufferLen),
	EV_FLOAT("ConsoleAlpha", &g_CV_ConsoleAlpha),
	EV_LONG("ConsoleLeft", &g_CV_ConsoleLeft),
	EV_LONG("ConsoleTop", &g_CV_ConsoleTop),
	EV_LONG("ConsoleRight", &g_CV_ConsoleRight),
	EV_LONG("ConsoleBottom", &g_CV_ConsoleBottom),

	EV_LONG("DrawDebugGeometry", &g_CV_DrawDebugGeometry),
	
	EV_LONG("CompressLightGrid", &g_CV_CompressLightGrid),

	//EV_LONG("ShowFrameRate", &g_CV_ShowFrameRate),
	EV_LONG("OrientFrameRate", &g_CV_OrientFrameRate),	

	// default client IP port number
	EV_LONG("IPClientPort", &g_CV_IPClientPort),				 // IP port number (numeric range: 0 - 65535)
	EV_LONG("IPClientPortRange", &g_CV_IPClientPortRange),		 // port number range (0 - N where N should be pretty small)
	EV_LONG("IPClientPortMRU", &g_CV_IPClientPortMRU),			 // MRU port number
		
	EV_LONG("BandwidthTargetClient", &g_CV_BandwidthTargetClient), // in bytes-per-sec
	EV_LONG("BandwidthTargetServer", &g_CV_BandwidthTargetServer), // in bytes-per-sec

	#ifdef DE_SERVER_COMPILE
	EV_FLOAT("ServerFPS", &g_ServerFPS),						 // server frames-per-second
	EV_LONG("LockServerFPS", &g_LockServerFPS),					 // force delay between frames to achieve lock rate if running too fast
	#endif // DE_SERVER_COMPILE

	EV_LONG("DebugModelRez", &g_CV_DebugModelRez),

	EV_LONG("NewPlayerPhysics", &g_CV_NewPlayerPhysics),

	EV_LONG("UDPSimulatePacketLoss", &g_CV_UDPSimulatePacketLoss),
	EV_LONG("UDPSimulateCorruption", &g_CV_UDPSimulateCorruption),

	EV_LONG("ModelOnlyUpdateDirtyTrackers", &g_CV_ModelOnlyUpdateDirtyTrackers),
};


// Functions that other modules call.
LTEngineVar* GetEngineVars() {return g_LTEngineVars;}
int GetNumEngineVars() {return sizeof(g_LTEngineVars) / sizeof(LTEngineVar);}

