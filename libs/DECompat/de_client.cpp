// CClientDE's adapters
// The messaging bodies match de_server.cpp's, and the two must stay in step since between them they define the wire format.

#include "de_client.h"
#include "de_clientshell.h"
#include "iltstream.h"
#include "ltobjectcreate.h"
#include "ltmodule.h"

#include <string.h>


// The holder g_pClientDE expands to, filled at load like g_pLTClient
ILTClient* g_pDELTClient = NULL;
define_holder(ILTClient, g_pDELTClient);

static ILTMath* s_pDEMath = NULL;
define_holder(ILTMath, s_pDEMath);

ILTMath* CClientDE::DEMath()
{
    return s_pDEMath;
}

CClientShellDE* CClientShellDE::s_pInstance = NULL;


// What makes g_pClientDE's cast safe, as in de_server.cpp
static_assert(sizeof(CClientDE) == sizeof(ILTClient),
              "CClientDE must not add data members");


namespace
{
    // Rotating string buffers, as in de_server.cpp.
    // Each module links its own copy of this library so the two halves never share one
    const uint32 kStringSlots = 4;
    const uint32 kStringLen   = 512;

    char* NextStringSlot()
    {
        static char  s_Buffers[kStringSlots][kStringLen];
        static uint32 s_nNext = 0;

        char* pResult = s_Buffers[s_nNext];
        s_nNext = (s_nNext + 1) % kStringSlots;
        return pResult;
    }

    // The caller owns the first reference and EndMessage2/EndHMessageWrite release it
    ILTMessage_Write* NewMessage(ILTClient* pClient)
    {
        ILTMessage_Write* pMsg = NULL;
        if (!pClient || !pClient->Common())
            return NULL;

        if (pClient->Common()->CreateMessage(pMsg) != LT_OK)
            return NULL;

        if (!pMsg)
            return NULL;

        pMsg->IncRef();
        return pMsg;
    }

    // The one video LT1's client could have playing.
    // Here because CClientDE can't hold a member
    HVIDEO s_hVideo = NULL;

    // SoundMgr() is typed as the common ILTSoundMgr, but listener, pause, volume, reverb and init are on the client subclass
    ILTClientSoundMgr* ClientSound(ILTClient* pClient)
    {
        return pClient ? (ILTClientSoundMgr*)pClient->SoundMgr() : NULL;
    }
}


// Messaging

HMESSAGEWRITE CClientDE::StartMessage(DBYTE messageID)
{
    ILTMessage_Write* pMsg = NewMessage(this);
    if (!pMsg)
        return NULL;

    // A uint8, which CServerShellDE::OnMessage reads back
    pMsg->Writeuint8(messageID);
    return pMsg;
}

DRESULT CClientDE::EndMessage(HMESSAGEWRITE hMessage)
{
    return EndMessage2(hMessage, MESSAGE_GUARANTEED);
}

DRESULT CClientDE::EndMessage2(HMESSAGEWRITE hMessage, DDWORD flags)
{
    if (!hMessage)
        return DE_ERROR;

    CLTMsgRef_Read cRead(hMessage->Read());
    LTRESULT dResult = SendToServer(cRead, flags);

    hMessage->DecRef();
    return dResult;
}

HMESSAGEWRITE CClientDE::StartHMessageWrite()
{
    return NewMessage(this);
}

DRESULT CClientDE::EndHMessageWrite(HMESSAGEWRITE hMessage)
{
    if (!hMessage)
        return DE_ERROR;
    hMessage->DecRef();
    return DE_OK;
}

DRESULT CClientDE::EndHMessageRead(HMESSAGEREAD hMessage)
{
    // Returns the reference ReadFromMessageHMessageRead handed out
    if (!hMessage)
        return DE_ERROR;
    hMessage->DecRef();
    return DE_OK;
}


// Writing

DRESULT CClientDE::WriteToMessageByte(HMESSAGEWRITE hMessage, DBYTE val)
{
    if (!hMessage) return DE_ERROR;
    hMessage->Writeuint8(val);
    return DE_OK;
}

DRESULT CClientDE::WriteToMessageWord(HMESSAGEWRITE hMessage, D_WORD val)
{
    if (!hMessage) return DE_ERROR;
    hMessage->Writeuint16(val);
    return DE_OK;
}

DRESULT CClientDE::WriteToMessageDWord(HMESSAGEWRITE hMessage, DDWORD val)
{
    if (!hMessage) return DE_ERROR;
    hMessage->Writeuint32(val);
    return DE_OK;
}

DRESULT CClientDE::WriteToMessageFloat(HMESSAGEWRITE hMessage, float val)
{
    if (!hMessage) return DE_ERROR;
    hMessage->Writefloat(val);
    return DE_OK;
}

DRESULT CClientDE::WriteToMessageString(HMESSAGEWRITE hMessage, const char *pStr)
{
    if (!hMessage) return DE_ERROR;
    hMessage->WriteString(pStr ? pStr : "");
    return DE_OK;
}

DRESULT CClientDE::WriteToMessageHString(HMESSAGEWRITE hMessage, HSTRING hString)
{
    if (!hMessage) return DE_ERROR;
    hMessage->WriteHString(hString);
    return DE_OK;
}

DRESULT CClientDE::WriteToMessageVector(HMESSAGEWRITE hMessage, DVector *pVal)
{
    if (!hMessage || !pVal) return DE_ERROR;
    hMessage->WriteLTVector(*pVal);
    return DE_OK;
}

DRESULT CClientDE::WriteToMessageCompVector(HMESSAGEWRITE hMessage, DVector *pVal)
{
    if (!hMessage || !pVal) return DE_ERROR;
    hMessage->WriteCompLTVector(*pVal);
    return DE_OK;
}

DRESULT CClientDE::WriteToMessageCompPosition(HMESSAGEWRITE hMessage, DVector *pVal)
{
    if (!hMessage || !pVal) return DE_ERROR;
    hMessage->WriteCompPos(*pVal);
    return DE_OK;
}

DRESULT CClientDE::WriteToMessageRotation(HMESSAGEWRITE hMessage, DRotation *pVal)
{
    if (!hMessage || !pVal) return DE_ERROR;
    hMessage->WriteLTRotation(*pVal);
    return DE_OK;
}

DRESULT CClientDE::WriteToMessageObject(HMESSAGEWRITE hMessage, HOBJECT hObj)
{
    if (!hMessage) return DE_ERROR;
    hMessage->WriteObject(hObj);
    return DE_OK;
}

DRESULT CClientDE::WriteToMessageHMessageWrite(HMESSAGEWRITE hMessage, HMESSAGEWRITE hDataMessage)
{
    if (!hMessage || !hDataMessage) return DE_ERROR;
    CLTMsgRef_Read cRead(hDataMessage->Read());
    hMessage->WriteMessage(cRead);
    return DE_OK;
}


// Reading

DBYTE CClientDE::ReadFromMessageByte(HMESSAGEREAD hMessage)
{
    return hMessage ? hMessage->Readuint8() : 0;
}

D_WORD CClientDE::ReadFromMessageWord(HMESSAGEREAD hMessage)
{
    return hMessage ? hMessage->Readuint16() : 0;
}

DDWORD CClientDE::ReadFromMessageDWord(HMESSAGEREAD hMessage)
{
    return hMessage ? hMessage->Readuint32() : 0;
}

float CClientDE::ReadFromMessageFloat(HMESSAGEREAD hMessage)
{
    return hMessage ? hMessage->Readfloat() : 0.0f;
}

HSTRING CClientDE::ReadFromMessageHString(HMESSAGEREAD hMessage)
{
    return hMessage ? hMessage->ReadHString() : NULL;
}

HOBJECT CClientDE::ReadFromMessageObject(HMESSAGEREAD hMessage)
{
    return hMessage ? hMessage->ReadObject() : NULL;
}

void CClientDE::ReadFromMessageVector(HMESSAGEREAD hMessage, DVector *pVal)
{
    if (!pVal) return;
    if (!hMessage) { pVal->Init(); return; }
    *pVal = hMessage->ReadLTVector();
}

void CClientDE::ReadFromMessageCompVector(HMESSAGEREAD hMessage, DVector *pVal)
{
    if (!pVal) return;
    if (!hMessage) { pVal->Init(); return; }
    *pVal = hMessage->ReadCompLTVector();
}

void CClientDE::ReadFromMessageCompPosition(HMESSAGEREAD hMessage, DVector *pVal)
{
    if (!pVal) return;
    if (!hMessage) { pVal->Init(); return; }
    *pVal = hMessage->ReadCompPos();
}

void CClientDE::ReadFromMessageRotation(HMESSAGEREAD hMessage, DRotation *pVal)
{
    if (!pVal) return;
    if (!hMessage) { pVal->Init(); return; }
    *pVal = hMessage->ReadLTRotation();
}

HMESSAGEREAD CClientDE::ReadFromMessageHMessageRead(HMESSAGEREAD hMessage)
{
    if (!hMessage)
        return NULL;

    // ReadMessage, not SubMsg. It undoes WriteMessage's framing: it reads the bit-length prefix,
    // bounds the sub message, and moves the parent past it as CServerDE's does.
    // Blood II's save block is written by the client and read by the server, and loaded back through here
    ILTMessage_Read* pSub = hMessage->ReadMessage();
    if (pSub)
    {
        // Released by EndHMessageRead, which LT1 callers make
        pSub->IncRef();
    }
    return pSub;
}

char* CClientDE::ReadFromMessageString(HMESSAGEREAD hMessage)
{
    char* pBuffer = NextStringSlot();
    pBuffer[0] = '\0';

    if (hMessage)
        hMessage->ReadString(pBuffer, kStringLen);

    return pBuffer;
}


// Calls that gained an argument (see de_client.h)

namespace
{
    const char* s_pWindowName = "LithTech";
}

void CClientDE::SetWindowName(const char *pName)
{
    if (pName && pName[0])
        s_pWindowName = pName;
}

DRESULT CClientDE::SetRenderMode(RMode *pMode)
{
    return ILTClient::SetRenderMode(pMode, s_pWindowName);
}

DRESULT CClientDE::ClearScreen(DRect *pClearRect, DDWORD flags)
{
    return ILTClient::ClearScreen(pClearRect, flags, NULL);
}

DRESULT CClientDE::RenderCamera(HLOCALOBJ hCamera)
{
    return ILTClient::RenderCamera(hCamera, GetFrameTime());
}

DRESULT CClientDE::End3D()
{
    return ILTClient::End3D(END3D_CANDRAWCONSOLE);
}

// Sky shadow panning passes straight through. Both engines return 0 for success
DRESULT CClientDE::SetGlobalPanTexture(DDWORD index, const char *pFilename)
{
    return ILTClient::SetGlobalPanTexture((uint32)index, pFilename);
}

DRESULT CClientDE::SetGlobalPanInfo(DDWORD index, float xOffset, float zOffset,
                                    float xScale, float zScale)
{
    return ILTClient::SetGlobalPanInfo((uint32)index, xOffset, zOffset, xScale, zScale);
}

DBOOL CClientDE::SetMusicDirectory(const char * /*pDirectory*/)
{
    return DFALSE;
}

DBOOL CClientDE::InitInstruments(const char * /*pDLSFile*/, const char * /*pStyleFile*/)
{
    return DFALSE;
}

DBOOL CClientDE::SetupPolyGrid(HLOCALOBJ hObj, DDWORD width, DDWORD height, DBOOL /*bHalfTriangles*/)
{
    return ILTClient::SetupPolyGrid(hObj, width, height, 0, NULL) ? DTRUE : DFALSE;
}

DRESULT CClientDE::SetPolyGridTextureInfo(HLOCALOBJ hObj, float xPan, float yPan,
                                          float xScale, float yScale)
{
    return ILTClient::SetPolyGridTextureInfo(hObj, xPan, yPan, xScale, yScale, 0.0f, 1.0f);
}

DRESULT CClientDE::GetPolyGridTextureInfo(HLOCALOBJ hObj, float *xPan, float *yPan,
                                          float *xScale, float *yScale)
{
    float fBaseReflection = 0.0f, fVolumeIOR = 1.0f;
    return ILTClient::GetPolyGridTextureInfo(hObj, xPan, yPan, xScale, yScale,
                                             &fBaseReflection, &fVolumeIOR);
}

void CClientDE::GetCameraRect(HLOCALOBJ hObj, DBOOL *pFullscreen,
                              int *pLeft, int *pTop, int *pRight, int *pBottom)
{
    bool bFullscreen = false;
    ILTClient::GetCameraRect(hObj, &bFullscreen, pLeft, pTop, pRight, pBottom);
    if (pFullscreen)
        *pFullscreen = bFullscreen ? DTRUE : DFALSE;
}


// Object flags

DDWORD CClientDE::GetObjectFlags(HLOCALOBJ hObj)
{
    uint32 nFlags = 0;
    if (Common() && hObj)
        Common()->GetObjectFlags(hObj, OFT_Flags, nFlags);
    return nFlags;
}

void CClientDE::SetObjectFlags(HLOCALOBJ hObj, DDWORD flags)
{
    if (Common() && hObj)
        Common()->SetObjectFlags(hObj, OFT_Flags, flags, FLAGMASK_ALL);
}

DDWORD CClientDE::GetObjectClientFlags(HLOCALOBJ hObj)
{
    uint32 nFlags = 0;
    if (Common() && hObj)
        Common()->GetObjectFlags(hObj, OFT_Client, nFlags);
    return nFlags;
}

void CClientDE::SetObjectClientFlags(HLOCALOBJ hObj, DDWORD flags)
{
    if (Common() && hObj)
        Common()->SetObjectFlags(hObj, OFT_Client, flags, FLAGMASK_ALL);
}

DRESULT CClientDE::GetObjectUserFlags(HLOCALOBJ hObj, DDWORD *pFlags)
{
    if (!pFlags)
        return DE_ERROR;
    *pFlags = 0;
    if (!Common() || !hObj)
        return DE_ERROR;
    uint32 nFlags = 0;
    LTRESULT dr = Common()->GetObjectFlags(hObj, OFT_User, nFlags);
    *pFlags = nFlags;
    return dr;
}


// Objects and the world

DRESULT CClientDE::DeleteObject(HLOCALOBJ hObj)
{
    return RemoveObject(hObj);
}

DDWORD CClientDE::GetObjectType(HLOCALOBJ hObj)
{
    uint32 nType = 0;
    if (Common() && hObj)
        Common()->GetObjectType(hObj, &nType);
    return nType;
}

DRESULT CClientDE::SetObjectFilenames(HLOCALOBJ hObj, const char *pFilename,
                                      const char *pSkinName)
{
    if (!Common() || !hObj)
        return DE_ERROR;

    ObjectCreateStruct ocs;
    ocs.Clear();
    if (pFilename)
        LTStrCpy(ocs.m_Filename, pFilename, sizeof(ocs.m_Filename) / sizeof(ocs.m_Filename[0]));
    if (pSkinName)
        LTStrCpy(ocs.m_SkinName, pSkinName, sizeof(ocs.m_SkinName) / sizeof(ocs.m_SkinName[0]));
    return Common()->SetObjectFilenames(hObj, &ocs);
}

DRESULT CClientDE::GetPointStatus(DVector *pPoint)
{
    if (!Common() || !pPoint)
        return DE_ERROR;
    return Common()->GetPointStatus(pPoint);
}

void CClientDE::GetParticlePos(HLOCALOBJ /*hSystem*/, DEParticle *pParticle, DVector *pPos)
{
    if (!pPos)
        return;
    if (!pParticle) { pPos->Init(); return; }
    *pPos = pParticle->m_Pos;
}

float CClientDE::GetServerConVarValueFloat(const char *pName)
{
    float fVal = 0.0f;
    if (pName && GetSConValueFloat(pName, fVal) == LT_OK)
        return fVal;
    return 0.0f;
}

char* CClientDE::GetServerConVarValueString(const char *pName)
{
    char* pBuffer = NextStringSlot();
    pBuffer[0] = '\0';
    if (pName && GetSConValueString(pName, pBuffer, kStringLen) == LT_OK)
        return pBuffer;
    return NULL;
}


// Sound

DRESULT CClientDE::PlaySound(PlaySoundInfo *pInfo)
{
    if (!SoundMgr() || !pInfo)
        return DE_ERROR;

    HLTSOUND hSound = NULL;
    LTRESULT dr = SoundMgr()->PlaySound(pInfo, hSound);

    // LT1 only filled it in for PLAYSOUND_GETHANDLE. Writing it always is harmless
    pInfo->m_hSound = hSound;
    return dr;
}

DRESULT CClientDE::InitSound(InitSoundInfo *pInfo)
{
    if (!ClientSound(this) || !pInfo)
        return DE_ERROR;
    return ClientSound(this)->InitSound(pInfo);
}

void CClientDE::KillSound(HSOUNDDE hSound)
{
    if (SoundMgr() && hSound)
        SoundMgr()->KillSound(hSound);
}

DRESULT CClientDE::GetSoundDuration(HSOUNDDE hSound, DFLOAT *pDuration)
{
    if (!pDuration)
        return DE_ERROR;
    *pDuration = 0.0f;
    if (!SoundMgr() || !hSound)
        return DE_ERROR;
    return SoundMgr()->GetSoundDuration(hSound, *pDuration);
}

DBOOL CClientDE::IsDone(HSOUNDDE hSound)
{
    // LT1 answered true for a handle it no longer knew, so a caller polling a finished sound stops
    if (!SoundMgr() || !hSound)
        return DTRUE;
    bool bDone = true;
    SoundMgr()->IsSoundDone(hSound, bDone);
    return bDone ? DTRUE : DFALSE;
}

void CClientDE::PauseSounds()
{
    if (ClientSound(this))
        ClientSound(this)->PauseSounds();
}

void CClientDE::ResumeSounds()
{
    if (ClientSound(this))
        ClientSound(this)->ResumeSounds();
}

DRESULT CClientDE::SetReverbProperties(ReverbProperties *pProps)
{
    if (!ClientSound(this) || !pProps)
        return DE_ERROR;
    return ClientSound(this)->SetReverbProperties(pProps);
}

unsigned short CClientDE::GetSoundVolume()
{
    uint16 nVolume = 0;
    if (ClientSound(this))
        ClientSound(this)->GetVolume(nVolume);
    return nVolume;
}

void CClientDE::SetSoundVolume(unsigned short nVolume)
{
    if (ClientSound(this))
        ClientSound(this)->SetVolume(nVolume);
}

DRESULT CClientDE::GetSound3DProviderLists(Sound3DProvider *&pList, DBOOL bVerify)
{
    pList = NULL;
    if (!ClientSound(this))
        return DE_ERROR;
    return ClientSound(this)->GetSound3DProviderLists(pList, bVerify ? true : false, 16);
}

void CClientDE::ReleaseSound3DProviderList(Sound3DProvider *pList)
{
    if (ClientSound(this) && pList)
        ClientSound(this)->ReleaseSound3DProviderList(pList);
}

void CClientDE::SetListener(DBOOL bListenerInClient, DVector *pPos, DRotation *pRot)
{
    if (!ClientSound(this))
        return;
    ClientSound(this)->SetListener(bListenerInClient ? true : false, pPos, pRot, true);
}


// Video

DRESULT CClientDE::StartVideo(const char *pFilename, DDWORD flags)
{
    if (!VideoMgr() || !pFilename)
        return DE_ERROR;

    if (s_hVideo)
        StopVideo();

    HVIDEO hVideo = NULL;
    LTRESULT dr = VideoMgr()->StartOnScreenVideo(pFilename, flags, hVideo);
    if (dr == LT_OK)
        s_hVideo = hVideo;
    return dr;
}

DRESULT CClientDE::StopVideo()
{
    if (!VideoMgr() || !s_hVideo)
        return DE_ERROR;
    LTRESULT dr = VideoMgr()->StopVideo(s_hVideo);
    s_hVideo = NULL;
    return dr;
}

DRESULT CClientDE::UpdateVideo()
{
    if (!VideoMgr() || !s_hVideo)
        return DE_ERROR;
    return VideoMgr()->UpdateVideo(s_hVideo);
}

DRESULT CClientDE::IsVideoPlaying()
{
    // LT_OK while playing, and LT_FINISHED or LT_ERROR otherwise, in both engines
    if (!VideoMgr() || !s_hVideo)
        return DE_ERROR;
    return VideoMgr()->GetVideoStatus(s_hVideo);
}


// Rotation helpers, as in de_server.cpp

DRESULT CClientDE::GetRotationVectors(DRotation *pRot, DVector *pUp, DVector *pRight,
                                      DVector *pForward)
{
    if (!Common() || !pRot || !pUp || !pRight || !pForward)
        return DE_ERROR;
    return Common()->GetRotationVectors(*pRot, *pUp, *pRight, *pForward);
}

DRESULT CClientDE::SetupEuler(DRotation *pRot, float fPitch, float fYaw, float fRoll)
{
    if (!DEMath() || !pRot)
        return DE_ERROR;
    return DEMath()->SetupEuler(*pRot, fPitch, fYaw, fRoll);
}

DRESULT CClientDE::AlignRotation(DRotation *pRot, DVector *pForward, DVector *pUp)
{
    if (!DEMath() || !pRot || !pForward)
        return DE_ERROR;

    // A NULL up meant world up in LT1
    DVector vUp;
    if (pUp)
        vUp = *pUp;
    else
        vUp.Init(0.0f, 1.0f, 0.0f);

    return DEMath()->AlignRotation(*pRot, *pForward, vUp);
}

DRESULT CClientDE::EulerRotateX(DRotation *pRot, float fAngle)
{
    if (!DEMath() || !pRot) return DE_ERROR;
    return DEMath()->EulerRotateX(*pRot, fAngle);
}

DRESULT CClientDE::EulerRotateY(DRotation *pRot, float fAngle)
{
    if (!DEMath() || !pRot) return DE_ERROR;
    return DEMath()->EulerRotateY(*pRot, fAngle);
}

DRESULT CClientDE::EulerRotateZ(DRotation *pRot, float fAngle)
{
    if (!DEMath() || !pRot) return DE_ERROR;
    return DEMath()->EulerRotateZ(*pRot, fAngle);
}

DRESULT CClientDE::RotateAroundAxis(DRotation *pRot, DVector *pAxis, float fAngle)
{
    if (!DEMath() || !pRot || !pAxis) return DE_ERROR;
    return DEMath()->RotateAroundAxis(*pRot, *pAxis, fAngle);
}


// Model nodes

DRESULT CClientDE::GetModelNodeHideStatus(HLOCALOBJ hObj, const char *pNodeName, DBOOL *pHidden)
{
    // Jupiter's client has no node hide query. "Not hidden" is the answer the server side gives too
    if (!pHidden)
        return DE_ERROR;
    *pHidden = DFALSE;
    if (!hObj || !pNodeName)
        return DE_ERROR;
    return DE_OK;
}

DRESULT CClientDE::SetModelNodeHideStatus(HLOCALOBJ hObj, const char *pNodeName, DBOOL bHidden)
{
    // For a model the client created, such as Blood II's player-view weapon
    if (!hObj || !pNodeName || !GetModelLT())
        return DE_ERROR;
    return GetModelLT()->SetNodeHideStatus(hObj, pNodeName, bHidden != DFALSE);
}

DBOOL CClientDE::GetModelNodeTransform(HLOCALOBJ hObj, const char *pNodeName,
                                       DVector *pPos, DRotation *pRot)
{
    if (!GetModelLT() || !hObj || !pNodeName)
        return DFALSE;

    HMODELNODE hNode = INVALID_MODEL_NODE;
    if (GetModelLT()->GetNode(hObj, pNodeName, hNode) != LT_OK)
        return DFALSE;

    LTransform tf;
    if (GetModelLT()->GetNodeTransform(hObj, hNode, tf, true) != LT_OK)
        return DFALSE;

    if (pPos) *pPos = tf.m_Pos;
    if (pRot) *pRot = tf.m_Rot;
    return DTRUE;
}


// The shell

CClientShellDE* CClientDE::GetClientShell()
{
    return CClientShellDE::GetInstance();
}

DRESULT CClientDE::SetSoftwarePSColor(HLOCALOBJ /*hObj*/, float /*r*/, float /*g*/, float /*b*/)
{
    return DE_OK;
}

DRESULT CClientDE::GetWorldInfoString(const char * /*pFilename*/, char *pInfoString,
                                      DDWORD maxLen, DDWORD *pActualLen)
{
    if (pInfoString && maxLen)
        pInfoString[0] = '\0';
    if (pActualLen)
        *pActualLen = 0;
    return DE_ERROR;
}

/*
    LT1 filled pPos and pScale with a recommended position and scale for the box.
    Blood II's CPolyGridFX::CreateObject passes an uninitialized vScale, ignores the return and then calls
    SetObjectScale with it, so this must always write a scale.
    Jupiter sizes a polygrid from its dims (drawpolygrid.cpp), as NOLF2's own PolyGridFX does,
    so the dims do the fitting and the scale is 1.
*/
DRESULT CClientDE::FitPolyGrid(HLOCALOBJ hObj, DVector *pMin, DVector *pMax,
                               DVector *pPos, DVector *pScale)
{
    if (!hObj || !pMin || !pMax)
        return DE_ERROR;

    // Half extents, which is what Jupiter's dims are
    LTVector vDims;
    vDims.x = (pMax->x - pMin->x) * 0.5f;
    vDims.y = (pMax->y - pMin->y) * 0.5f;
    vDims.z = (pMax->z - pMin->z) * 0.5f;

    // A box with no area is left alone and fails, as LT1's did
    if (vDims.x <= 0.0f || vDims.z <= 0.0f)
        return DE_ERROR;

    if (Physics())
        Physics()->SetObjectDims(hObj, &vDims, 0);

    if (pPos)
    {
        pPos->x = (pMin->x + pMax->x) * 0.5f;
        pPos->y = (pMin->y + pMax->y) * 0.5f;
        pPos->z = (pMin->z + pMax->z) * 0.5f;
    }

    if (pScale)
    {
        pScale->x = 1.0f;
        pScale->y = 1.0f;
        pScale->z = 1.0f;
    }

    return DE_OK;
}
