/*
    LithTech 1.0's client interface, backed by Jupiter's ILTClient.
    A derived class - not a wrapper - like CServerDE.
*/

#ifndef __DE_CLIENT_H__
#define __DE_CLIENT_H__

#ifndef __ILTCLIENT_H__
#include "iltclient.h"
#endif

#ifndef __DE_TYPES_H__
#include "de_types.h"
#endif

#ifndef __DE_MESSAGE_H__
#include "de_message.h"
#endif

#ifndef __ILTMATH_H__
#include "iltmath.h"
#endif

#ifndef __ILTCOMMON_H__
#include "iltcommon.h"
#endif

#ifndef __ILTPHYSICS_H__
#include "iltphysics.h"
#endif

#ifndef __ILTMODEL_H__
#include "iltmodel.h"
#endif

#ifndef __ILTSOUNDMGR_H__
#include "iltsoundmgr.h"
#endif

#ifndef __ILTVIDEOMGR_H__
#include "iltvideomgr.h"
#endif

#include <utility>


/*
    LT1 client type names. All are exact aliases.
*/

typedef LTLinePt    DELinePt;
typedef LTLine      DELine;
typedef LTParticle  DEParticle;
typedef HLTCOLOR    HDECOLOR;
typedef HLTFONT     HDEFONT;
typedef HLTLINE     HDELINE;
typedef LTGUID      DGUID;

// LT1's C style spelling of the interface ('ClientDE *pClientDE')
class CClientDE;
typedef CClientDE   ClientDE;

// LT1's client physics interface
// UpdateMovement, MovePushObjects and RotatePushObjects only exist on ILTClientPhysics, not the ILTPhysics base
typedef ILTClientPhysics CPhysicsLT;

// The same struct. LT1's wrote the light add through a pointer and named the flags m_Flags,
// so those two field changes are fixed at the hook
typedef ModelHookData ModelHookData_t;

// Defined in de_clientshell.h
class CClientShellDE;


class CClientDE : public ILTClient
{
public:

    /*
        Messaging, client to server.
        StartMessage writes the id as a uint8, the width CServerShellDE::OnMessage reads it back as.
        EndMessage sends guaranteed and EndMessage2 takes the flags. Both release the message.
    */

    HMESSAGEWRITE   StartMessage(DBYTE messageID);
    DRESULT         EndMessage(HMESSAGEWRITE hMessage);
    DRESULT         EndMessage2(HMESSAGEWRITE hMessage, DDWORD flags);

    // A message with no id, to embed in another. End drops the reference and the parent's EndMessage sends it
    HMESSAGEWRITE   StartHMessageWrite();
    DRESULT         EndHMessageWrite(HMESSAGEWRITE hMessage);
    DRESULT         EndHMessageRead(HMESSAGEREAD hMessage);

    // Writing. The same bodies as CServerDE's, which must stay in step or the two halves of the game disagree
    DRESULT         WriteToMessageByte(HMESSAGEWRITE hMessage, DBYTE val);
    DRESULT         WriteToMessageWord(HMESSAGEWRITE hMessage, D_WORD val);
    DRESULT         WriteToMessageDWord(HMESSAGEWRITE hMessage, DDWORD val);
    DRESULT         WriteToMessageFloat(HMESSAGEWRITE hMessage, float val);
    DRESULT         WriteToMessageString(HMESSAGEWRITE hMessage, const char *pStr);
    DRESULT         WriteToMessageHString(HMESSAGEWRITE hMessage, HSTRING hString);
    DRESULT         WriteToMessageVector(HMESSAGEWRITE hMessage, DVector *pVal);
    DRESULT         WriteToMessageCompVector(HMESSAGEWRITE hMessage, DVector *pVal);
    DRESULT         WriteToMessageCompPosition(HMESSAGEWRITE hMessage, DVector *pVal);
    DRESULT         WriteToMessageRotation(HMESSAGEWRITE hMessage, DRotation *pVal);
    DRESULT         WriteToMessageObject(HMESSAGEWRITE hMessage, HOBJECT hObj);
    DRESULT         WriteToMessageHMessageWrite(HMESSAGEWRITE hMessage, HMESSAGEWRITE hDataMessage);

    // Reading. ReadFromMessageString returns one of a few rotating buffers,
    // which keeps LT1's "read two strings, then use both" working
    DBYTE           ReadFromMessageByte(HMESSAGEREAD hMessage);
    D_WORD          ReadFromMessageWord(HMESSAGEREAD hMessage);
    DDWORD          ReadFromMessageDWord(HMESSAGEREAD hMessage);
    float           ReadFromMessageFloat(HMESSAGEREAD hMessage);
    HSTRING         ReadFromMessageHString(HMESSAGEREAD hMessage);
    HOBJECT         ReadFromMessageObject(HMESSAGEREAD hMessage);
    void            ReadFromMessageVector(HMESSAGEREAD hMessage, DVector *pVal);
    void            ReadFromMessageCompVector(HMESSAGEREAD hMessage, DVector *pVal);
    void            ReadFromMessageCompPosition(HMESSAGEREAD hMessage, DVector *pVal);
    void            ReadFromMessageRotation(HMESSAGEREAD hMessage, DRotation *pVal);
    HMESSAGEREAD    ReadFromMessageHMessageRead(HMESSAGEREAD hMessage);
    char*           ReadFromMessageString(HMESSAGEREAD hMessage);

    /*
        Object flags.
        LT1 had separate Flags, ClientFlags and UserFlags calls. Jupiter has one pair keyed by OFT_Flags/OFT_Client/OFT_User.
        SetObjectFlags replaces the whole word, as LT1's did.
    */

    DDWORD          GetObjectFlags(HLOCALOBJ hObj);
    void            SetObjectFlags(HLOCALOBJ hObj, DDWORD flags);
    DDWORD          GetObjectClientFlags(HLOCALOBJ hObj);
    void            SetObjectClientFlags(HLOCALOBJ hObj, DDWORD flags);
    DRESULT         GetObjectUserFlags(HLOCALOBJ hObj, DDWORD *pFlags);

    // The client physics object as what it is, so the three client only calls resolve.
    CPhysicsLT*     Physics() { return (CPhysicsLT*)static_cast<ILTClient*>(this)->Physics(); }

    /*
        Calls that gained an argument. The new argument is the value Shogo's converted shell passes.
            SetRenderMode(pMode)            + the window title, set once through SetWindowName
            ClearScreen(rect, flags)        + no clear color (black)
            RenderCamera(hCam)              + GetFrameTime()
            End3D()                         + END3D_CANDRAWCONSOLE. LT1 drew the console at FlipScreen instead
            SetupPolyGrid(.., bHalf)        the half-triangle flag has no Jupiter equivalent
            Set/GetPolyGridTextureInfo      + no reflection, an IOR of 1, which is the LT1 look
            GetCameraRect(h, DBOOL*, ..)    a bool out-param
    */

    DRESULT         SetRenderMode(RMode *pMode);
    static void     SetWindowName(const char *pName);
    DRESULT         ClearScreen(DRect *pClearRect, DDWORD flags);
    DRESULT         RenderCamera(HLOCALOBJ hCamera);
    DRESULT         End3D();
    DBOOL           SetupPolyGrid(HLOCALOBJ hObj, DDWORD width, DDWORD height, DBOOL bHalfTriangles);
    DRESULT         SetPolyGridTextureInfo(HLOCALOBJ hObj, float xPan, float yPan,
                                           float xScale, float yScale);
    DRESULT         GetPolyGridTextureInfo(HLOCALOBJ hObj, float *xPan, float *yPan,
                                           float *xScale, float *yScale);
    void            GetCameraRect(HLOCALOBJ hObj, DBOOL *pFullscreen,
                                  int *pLeft, int *pTop, int *pRight, int *pBottom);

    // Objects and the world

    // LT1's name for RemoveObject
    DRESULT         DeleteObject(HLOCALOBJ hObj);

    DDWORD          GetObjectType(HLOCALOBJ hObj);

    // Jupiter's takes an ObjectCreateStruct rather than two strings
    DRESULT         SetObjectFilenames(HLOCALOBJ hObj, const char *pFilename,
                                       const char *pSkinName);

    DRESULT         GetPointStatus(DVector *pPoint);

    // LT1 wrote the particle's position out. Jupiter's particle carries m_Pos
    void            GetParticlePos(HLOCALOBJ hSystem, DEParticle *pParticle, DVector *pPos);

    // LT1 returned the value, 0 or NULL when the variable didn't exist
    float           GetServerConVarValueFloat(const char *pName);
    char*           GetServerConVarValueString(const char *pName);

    // Sound - all of it moved to SoundMgr(), and some changed shape

    // LT1 returned the handle in pInfo->m_hSound. This writes it back there
    DRESULT         PlaySound(PlaySoundInfo *pInfo);
    DRESULT         InitSound(InitSoundInfo *pInfo);
    void            KillSound(HSOUNDDE hSound);
    DRESULT         GetSoundDuration(HSOUNDDE hSound, DFLOAT *pDuration);
    DBOOL           IsDone(HSOUNDDE hSound);
    void            PauseSounds();
    void            ResumeSounds();
    DRESULT         SetReverbProperties(ReverbProperties *pProps);
    unsigned short  GetSoundVolume();
    void            SetSoundVolume(unsigned short nVolume);

    // Gained a voice count. 16 is what Shogo's converted shell passes
    DRESULT         GetSound3DProviderLists(Sound3DProvider *&pList, DBOOL bVerify);
    void            ReleaseSound3DProviderList(Sound3DProvider *pList);

    // Gained bTeleport. Every LT1 listener update is a fresh position, so true or the engine would compute a velocity from the jump
    void            SetListener(DBOOL bListenerInClient, DVector *pPos, DRotation *pRot);

    // Video - LT1 played one video at a time with no handle. 
    // The handle lives in de_client.cpp, and StartVideo stops any video already playing as LT1's did
    DRESULT         StartVideo(const char *pFilename, DDWORD flags);
    DRESULT         StopVideo();
    DRESULT         UpdateVideo();
    DRESULT         IsVideoPlaying();

    // Rotation helpers. Pointers became references, the same as CServerDE's
    DRESULT GetRotationVectors(DRotation *pRot, DVector *pUp, DVector *pRight,
                               DVector *pForward);
    DRESULT SetupEuler(DRotation *pRot, float fPitch, float fYaw, float fRoll);
    DRESULT AlignRotation(DRotation *pRot, DVector *pForward, DVector *pUp);
    DRESULT EulerRotateX(DRotation *pRot, float fAngle);
    DRESULT EulerRotateY(DRotation *pRot, float fAngle);
    DRESULT EulerRotateZ(DRotation *pRot, float fAngle);
    DRESULT RotateAroundAxis(DRotation *pRot, DVector *pAxis, float fAngle);

    static ILTMath* DEMath();

    // Model nodes. Jupiter takes char* for a name it only reads, and bool* where LT1 holds a DBOOL
    DRESULT         GetModelNodeHideStatus(HLOCALOBJ hObj, const char *pNodeName, DBOOL *pHidden);
    DRESULT         SetModelNodeHideStatus(HLOCALOBJ hObj, const char *pNodeName, DBOOL bHidden);
    DBOOL           GetModelNodeTransform(HLOCALOBJ hObj, const char *pNodeName,
                                          DVector *pPos, DRotation *pRot);

    /*
        LT1's five argument Parse isn't here.
        Jupiter's client only has Common()->Parse(ConParse*), which keeps its cursor private,
        so the old form can't be rebuilt over it.
    */

    // The shell, which CClientShellDE records when it's created
    CClientShellDE* GetClientShell();

    // Sky shadow panning. xScale and zScale are world units per texel, and the offsets are in world units.
    // Only GLOBALPAN_SKYSHADOW does anything, as in LT1
    DRESULT         SetGlobalPanTexture(DDWORD index, const char *pFilename);
    DRESULT         SetGlobalPanInfo(DDWORD index, float xOffset, float zOffset,
                                     float xScale, float zScale);

    // LT1's IMA music. Both fail until the engine has the two entry points
    // LT1 put InitInstruments on ClientDE, not on Common
    DBOOL           SetMusicDirectory(const char *pDirectory);
    DBOOL           InitInstruments(const char *pDLSFile, const char *pStyleFile);

    // Null - there's no software renderer
    DRESULT         SetSoftwarePSColor(HLOCALOBJ hObj, float r, float g, float b);

    // Null - Jupiter worlds carry no info block
    DRESULT         GetWorldInfoString(const char *pFilename, char *pInfoString,
                                       DDWORD maxLen, DDWORD *pActualLen);

    // Fits a polygrid to a box. Jupiter sizes a polygrid by its dims, not its scale, and has no fit call,
    // so this sets the dims and returns the box's center and a unit scale
    DRESULT         FitPolyGrid(HLOCALOBJ hObj, DVector *pMin, DVector *pMax,
                                DVector *pPos, DVector *pScale);
};


/*
    g_pClientDE.
    LT1 code assigned it in CreateClientShell, which Jupiter doesn't have.
    It's a readonly macro over a holder, so an assignment to it won't compile.
    The cast is safe because CClientDE adds no data or virtual functions (de_client.cpp checks it).
*/

extern ILTClient* g_pDELTClient;
#define g_pClientDE (static_cast<CClientDE*>(g_pDELTClient))

#endif // __DE_CLIENT_H__
