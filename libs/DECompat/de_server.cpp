#include "de_server.h"
#include "de_engineobjects.h"

#include "iltcommon.h"
#include "iltmodel.h"
#include "iltphysics.h"
#include "ltobjectcreate.h"

#include <string.h>
#include <stdlib.h>


// Filled by the module system when the game DLL is bound
ILTServer* g_pDELTServer = NULL;
define_holder(ILTServer, g_pDELTServer);


// ILTMath uses a module holder since it has no accessor on ILTCSBase 
static ILTMath* s_pDEMath = NULL;
define_holder(ILTMath, s_pDEMath);

ILTMath* CServerDE::DEMath()
{
    return s_pDEMath;
}

static_assert(sizeof(CServerDE) == sizeof(ILTServer),
              "CServerDE must not add data members");

namespace
{
    // LT1 returned strings in memory the engine owned, so this layer owns some.
    // Rotating slots let a caller read two strings before using either.
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

    // Creates a message and takes its first reference
    ILTMessage_Write* NewMessage(ILTServer* pServer)
    {
        ILTMessage_Write* pMsg = NULL;
        if (!pServer || !pServer->Common())
            return NULL;

        if (pServer->Common()->CreateMessage(pMsg) != LT_OK)
            return NULL;

        // LT_OK does not guarantee pMsg was written
        if (!pMsg)
            return NULL;

        // CreateMessage returns a refcount of zero.
        // The matching DecRef is in EndMessage2.
        pMsg->IncRef();
        return pMsg;
    }
}


// Starting messages

HMESSAGEWRITE CServerDE::StartMessageToObject(LPBASECLASS pSender,
                                              HOBJECT hSendTo, DDWORD messageID)
{
    ILTMessage_Write* pMsg = NewMessage(this);
    if (!pMsg)
        return NULL;

    // uint32, as LT1's DDWORD parameter.
    // It must match the read in de_engineobjects.h.
    pMsg->Writeuint32(messageID);

    CDEPendingSends::SEntry entry;
    entry.m_pMsg   = pMsg;
    entry.m_eDest  = CDEPendingSends::eDest_Object;
    entry.m_hSender = pSender ? pSender->GetHOBJECT() : NULL;
    entry.m_hSendTo = hSendTo;
    entry.m_hClient = NULL;
    entry.m_vPos.Init();
    CDEPendingSends::Add(entry);

    return pMsg;
}

HMESSAGEWRITE CServerDE::StartMessage(HCLIENT hSendTo, DBYTE messageID)
{
    ILTMessage_Write* pMsg = NewMessage(this);
    if (!pMsg)
        return NULL;

    // uint8, as LT1's DBYTE parameter.
    // The client shell reads it with Readuint8()
    pMsg->Writeuint8(messageID);

    CDEPendingSends::SEntry entry;
    entry.m_pMsg   = pMsg;
    entry.m_eDest  = CDEPendingSends::eDest_Client;
    entry.m_hSender = NULL;
    entry.m_hSendTo = NULL;
    entry.m_hClient = hSendTo;
    entry.m_vPos.Init();
    CDEPendingSends::Add(entry);

    return pMsg;
}

HMESSAGEWRITE CServerDE::StartSpecialEffectMessage(LPBASECLASS pObject)
{
    ILTMessage_Write* pMsg = NewMessage(this);
    if (!pMsg)
        return NULL;

    // No ID because the client reads an SFX message whole

    CDEPendingSends::SEntry entry;
    entry.m_pMsg   = pMsg;
    entry.m_eDest  = CDEPendingSends::eDest_SFX;
    entry.m_hSender = NULL;
    entry.m_hSendTo = pObject ? pObject->GetHOBJECT() : NULL;
    entry.m_hClient = NULL;
    entry.m_vPos.Init();
    CDEPendingSends::Add(entry);

    return pMsg;
}

HMESSAGEWRITE CServerDE::StartInstantSpecialEffectMessage(DVector *pPos)
{
    ILTMessage_Write* pMsg = NewMessage(this);
    if (!pMsg)
        return NULL;

    CDEPendingSends::SEntry entry;
    entry.m_pMsg   = pMsg;
    entry.m_eDest  = CDEPendingSends::eDest_InstantSFX;
    entry.m_hSender = NULL;
    entry.m_hSendTo = NULL;
    entry.m_hClient = NULL;
    if (pPos)
        entry.m_vPos = *pPos;
    else
        entry.m_vPos.Init();
    CDEPendingSends::Add(entry);

    return pMsg;
}


// Ending messages

DRESULT CServerDE::EndMessage(HMESSAGEWRITE hMessage)
{
    return EndMessage2(hMessage, MESSAGE_GUARANTEED);
}

DRESULT CServerDE::EndMessage2(HMESSAGEWRITE hMessage, DDWORD flags)
{
    if (!hMessage)
        return DE_ERROR;

    CDEPendingSends::SEntry entry;
    if (!CDEPendingSends::Take(hMessage, entry))
    {
        // Not ours: an MID_SAVEOBJECT buffer, which is never ended
        return DE_OK;
    }

    CLTMsgRef_Read cRead(hMessage->Read());
    LTRESULT dResult = DE_ERROR;

    switch (entry.m_eDest)
    {
        case CDEPendingSends::eDest_Object:
            dResult = SendToObject(cRead, entry.m_hSender, entry.m_hSendTo, flags);
            break;
        case CDEPendingSends::eDest_Client:
            dResult = SendToClient(cRead, entry.m_hClient, flags);
            break;
        case CDEPendingSends::eDest_Server:
            dResult = SendToServer(cRead, entry.m_hSender, flags);
            break;
        case CDEPendingSends::eDest_SFX:
            dResult = SetObjectSFXMessage(entry.m_hSendTo, cRead);
            break;
        case CDEPendingSends::eDest_InstantSFX:
            dResult = SendSFXMessage(cRead, entry.m_vPos, flags);
            break;
    }

    hMessage->DecRef();
    return dResult;
}

// Writing

DRESULT CServerDE::WriteToMessageByte(HMESSAGEWRITE hMessage, DBYTE val)
{
    if (!hMessage) return DE_ERROR;
    hMessage->Writeuint8(val);
    return DE_OK;
}

DRESULT CServerDE::WriteToMessageWord(HMESSAGEWRITE hMessage, D_WORD val)
{
    if (!hMessage) return DE_ERROR;
    hMessage->Writeuint16(val);
    return DE_OK;
}

DRESULT CServerDE::WriteToMessageDWord(HMESSAGEWRITE hMessage, DDWORD val)
{
    if (!hMessage) return DE_ERROR;
    hMessage->Writeuint32(val);
    return DE_OK;
}

DRESULT CServerDE::WriteToMessageFloat(HMESSAGEWRITE hMessage, float val)
{
    if (!hMessage) return DE_ERROR;
    hMessage->Writefloat(val);
    return DE_OK;
}

DRESULT CServerDE::WriteToMessageString(HMESSAGEWRITE hMessage, const char *pStr)
{
    if (!hMessage) return DE_ERROR;
    hMessage->WriteString(pStr ? pStr : "");
    return DE_OK;
}

DRESULT CServerDE::WriteToMessageHString(HMESSAGEWRITE hMessage, HSTRING hString)
{
    if (!hMessage) return DE_ERROR;
    hMessage->WriteHString(hString);
    return DE_OK;
}

DRESULT CServerDE::WriteToMessageVector(HMESSAGEWRITE hMessage, DVector *pVal)
{
    if (!hMessage || !pVal) return DE_ERROR;
    hMessage->WriteLTVector(*pVal);
    return DE_OK;
}

DRESULT CServerDE::WriteToMessageCompVector(HMESSAGEWRITE hMessage, DVector *pVal)
{
    if (!hMessage || !pVal) return DE_ERROR;
    hMessage->WriteCompLTVector(*pVal);
    return DE_OK;
}

DRESULT CServerDE::WriteToMessageCompPosition(HMESSAGEWRITE hMessage, DVector *pVal)
{
    if (!hMessage || !pVal) return DE_ERROR;
    hMessage->WriteCompPos(*pVal);
    return DE_OK;
}

DRESULT CServerDE::WriteToMessageRotation(HMESSAGEWRITE hMessage, DRotation *pVal)
{
    if (!hMessage || !pVal) return DE_ERROR;
    hMessage->WriteLTRotation(*pVal);
    return DE_OK;
}

DRESULT CServerDE::WriteToMessageObject(HMESSAGEWRITE hMessage, HOBJECT hObj)
{
    if (!hMessage) return DE_ERROR;
    hMessage->WriteObject(hObj);
    return DE_OK;
}

DRESULT CServerDE::WriteToMessageHMessageRead(HMESSAGEWRITE hMessage,
                                              HMESSAGEREAD hDataMessage)
{
    if (!hMessage || !hDataMessage) return DE_ERROR;
    hMessage->WriteMessage(hDataMessage);
    return DE_OK;
}

DRESULT CServerDE::WriteToLoadSaveMessageObject(HMESSAGEWRITE hMessage,
                                                HOBJECT hObject)
{
    if (!hMessage) return DE_ERROR;
    hMessage->WriteObject(hObject);
    return DE_OK;
}


// Reading

DBYTE CServerDE::ReadFromMessageByte(HMESSAGEREAD hMessage)
{
    if (!hMessage)
        return 0;

    const auto ret = hMessage->Readuint8();
    DECompatMsg::AfterRead(hMessage);
    return ret;
}

D_WORD CServerDE::ReadFromMessageWord(HMESSAGEREAD hMessage)
{
    if (!hMessage)
        return 0;

    const auto ret = hMessage->Readuint16();
    DECompatMsg::AfterRead(hMessage);
    return ret;
}

DDWORD CServerDE::ReadFromMessageDWord(HMESSAGEREAD hMessage)
{
    if (!hMessage)
        return 0;

    const auto ret = hMessage->Readuint32();
    DECompatMsg::AfterRead(hMessage);
    return ret;
}

float CServerDE::ReadFromMessageFloat(HMESSAGEREAD hMessage)
{
    if (!hMessage)
        return 0.0f;

    const auto ret = hMessage->Readfloat();
    DECompatMsg::AfterRead(hMessage);
    return ret;
}

HSTRING CServerDE::ReadFromMessageHString(HMESSAGEREAD hMessage)
{
    if (!hMessage)
        return NULL;

    const auto ret = hMessage->ReadHString();
    DECompatMsg::AfterRead(hMessage);
    return ret;
}

HOBJECT CServerDE::ReadFromMessageObject(HMESSAGEREAD hMessage)
{
    if (!hMessage)
        return NULL;

    const auto ret = hMessage->ReadObject();
    DECompatMsg::AfterRead(hMessage);
    return ret;
}

void CServerDE::ReadFromMessageVector(HMESSAGEREAD hMessage, DVector *pVal)
{
    if (!pVal)
        return;

    if (!hMessage)
    {
        pVal->Init();
        return;
    }

    hMessage->ReadType(pVal);
    DECompatMsg::AfterRead(hMessage);
}

void CServerDE::ReadFromMessageCompVector(HMESSAGEREAD hMessage, DVector *pVal)
{
    if (!pVal)
        return;

    if (!hMessage)
    {
        pVal->Init();
        return;
    }

    *pVal = hMessage->ReadCompLTVector();
    DECompatMsg::AfterRead(hMessage);
}

void CServerDE::ReadFromMessageCompPosition(HMESSAGEREAD hMessage, DVector *pVal)
{
    if (!pVal)
        return;

    if (!hMessage)
    {
        pVal->Init();
        return;
    }

    *pVal = hMessage->ReadCompPos();
    DECompatMsg::AfterRead(hMessage);
}

void CServerDE::ReadFromMessageRotation(HMESSAGEREAD hMessage, DRotation *pVal)
{
    if (!pVal)
        return;

    if (!hMessage)
    {
        pVal->Init();
        return;
    }

    hMessage->ReadType(pVal);
    DECompatMsg::AfterRead(hMessage);
}

DRESULT CServerDE::ReadFromLoadSaveMessageObject(HMESSAGEREAD hMessage,
                                                 HOBJECT *hObject)
{
    if (!hMessage || !hObject)
        return DE_ERROR;

    *hObject = hMessage->ReadObject();
    DECompatMsg::AfterRead(hMessage);
    return DE_OK;
}

HMESSAGEREAD CServerDE::ReadFromMessageHMessageRead(HMESSAGEREAD hMessage)
{
    if (!hMessage)
        return NULL;

    ILTMessage_Read* pSub = hMessage->ReadMessage();
    DECompatMsg::AfterRead(hMessage);
    if (pSub)
    {
        // Balanced by the caller's EndHMessageRead
        pSub->IncRef();
    }
    return pSub;
}

char* CServerDE::ReadFromMessageString(HMESSAGEREAD hMessage)
{
    char* pBuffer = NextStringSlot();
    pBuffer[0] = '\0';

    if (hMessage)
    {
        hMessage->ReadString(pBuffer, kStringLen);
        DECompatMsg::AfterRead(hMessage);
    }

    return pBuffer;
}

void CServerDE::EndHMessageRead(HMESSAGEREAD hMessage)
{
    if (hMessage)
        hMessage->DecRef();
}

void CServerDE::ResetRead(HMESSAGEREAD hRead)
{
    // Position 0 is the first payload field.
    // Object handlers get a message that starts after the ID.
    if (hRead)
        hRead->SeekTo(0);
}


// Outside messaging

DDWORD CServerDE::GetObjectUserFlags(HOBJECT hObj)
{
    uint32 dwFlags = 0;
    if (Common())
        Common()->GetObjectFlags(hObj, OFT_User, dwFlags);
    return dwFlags;
}

DRESULT CServerDE::SetObjectUserFlags(HOBJECT hObj, DDWORD flags)
{
    if (!Common())
        return DE_ERROR;
    return Common()->SetObjectFlags(hObj, OFT_User, flags, FLAGMASK_ALL);
}

float CServerDE::GetObjectMass(HOBJECT hObj)
{
    float fMass = 0.0f;
    if (Physics())
        Physics()->GetMass(hObj, &fMass);
    return fMass;
}

void CServerDE::SetObjectMass(HOBJECT hObj, float mass)
{
    if (Physics())
        Physics()->SetMass(hObj, mass);
}

/*
    SETDIMS_PUSHOBJECTS fits the box against the world.
    Without it a box that grows around a fixed center can embed the object in the floor.
    A missed fit returns LT_ERROR and writes the achieved dims back.
*/
DRESULT CServerDE::SetObjectDims2(HOBJECT hObj, DVector *pNewDims)
{
    if (!Physics() || !pNewDims)
        return DE_ERROR;
    // LT1ObjectDims gates the flag.
    // It's read once since it is fixed for the level.
    static int s_nLT1 = -1;
    if (s_nLT1 < 0)
    {
        HCONVAR hVar = GetGameConVar("LT1ObjectDims");
        const char *pVal = hVar ? GetVarValueString(hVar) : LTNULL;
        s_nLT1 = (pVal && atoi(pVal) != 0) ? 1 : 0;
    }

    return Physics()->SetObjectDims(hObj, pNewDims,
                                    s_nLT1 ? SETDIMS_PUSHOBJECTS : 0);
}

DRESULT CServerDE::SetModelFilenames(HOBJECT hObj, const char *pFilename,
                                     const char *pSkinName)
{
    if (!Common())
        return DE_ERROR;

    ObjectCreateStruct ocs;
    INIT_OBJECTCREATESTRUCT(ocs);

    if (pFilename)
        LTStrCpy(ocs.m_Filename, pFilename, sizeof(ocs.m_Filename));
    if (pSkinName)
        LTStrCpy(ocs.m_SkinNames[0], pSkinName, sizeof(ocs.m_SkinNames[0]));

    return Common()->SetObjectFilenames(hObj, &ocs);
}

DBOOL CServerDE::GetModelNodeTransform(HOBJECT hObj, const char *pNodeName,
                                       DVector *pPos, DRotation *pRot)
{
    if (!GetModelLT() || !pNodeName)
        return DFALSE;

    HMODELNODE hNode = INVALID_MODEL_NODE;
    if (GetModelLT()->GetNode(hObj, pNodeName, hNode) != LT_OK)
        return DFALSE;

    LTransform transform;
    if (GetModelLT()->GetNodeTransform(hObj, hNode, transform, true) != LT_OK)
        return DFALSE;

    if (pPos)
        *pPos = transform.m_Pos;
    if (pRot)
        *pRot = transform.m_Rot;

    return DTRUE;
}

DBOOL CServerDE::GetAttachedModelNodeTransform(HATTACHMENT hAttachment,
                                               const char *pNodeName,
                                               DVector *pPos, DRotation *pRot)
{
    if (!GetModelLT() || !pNodeName)
        return DFALSE;

    HOBJECT hParent = DNULL, hChild = DNULL;
    if (Common()->GetAttachmentObjects(hAttachment, hParent, hChild) != LT_OK || !hChild)
        return DFALSE;

    HMODELNODE hNode = INVALID_MODEL_NODE;
    if (GetModelLT()->GetNode(hChild, pNodeName, hNode) != LT_OK)
        return DFALSE;

    LTransform transform;
    if (Common()->GetAttachedModelNodeTransform(hAttachment, hNode, transform) != LT_OK)
        return DFALSE;

    if (pPos)
        *pPos = transform.m_Pos;
    if (pRot)
        *pRot = transform.m_Rot;

    return DTRUE;
}

void CServerDE::SetDeactivationTime(HOBJECT /*hObj*/, DFLOAT /*fDeactivationTime*/)
{
    // In LT1 this was an idle hint
}

void CServerDE::PingObjects(HOBJECT /*hObj*/)
{
    // The other half of LT1's deactivation model
}


// Functions whose signatures drifted

DDWORD CServerDE::GetObjectFlags(HOBJECT hObj)
{
    uint32 dwFlags = 0;
    if (Common())
        Common()->GetObjectFlags(hObj, OFT_Flags, dwFlags);
    return dwFlags;
}

DRESULT CServerDE::SetObjectFlags(HOBJECT hObj, DDWORD flags)
{
    if (!Common())
        return DE_ERROR;
    return Common()->SetObjectFlags(hObj, OFT_Flags, flags, FLAGMASK_ALL);
}

DDWORD CServerDE::GetObjectType(HOBJECT hObj)
{
    uint32 nType = 0;
    if (Common())
        Common()->GetObjectType(hObj, &nType);
    return nType;
}

DRESULT CServerDE::SetObjectFilenames(HOBJECT hObj, const char *pFilename,
                                      const char *pSkinName)
{
    // LT1 had both names for one operation
    return SetModelFilenames(hObj, pFilename, pSkinName);
}

// Flag 0 resizes around a fixed center with no fit test, as LT1's did
DRESULT CServerDE::SetObjectDims(HOBJECT hObj, DVector *pNewDims)
{
    if (!Physics() || !pNewDims)
        return DE_ERROR;
    return Physics()->SetObjectDims(hObj, pNewDims, 0);
}

DRESULT CServerDE::GetRotationVectors(DRotation *pRot, DVector *pUp,
                                      DVector *pRight, DVector *pForward)
{
    if (!Common() || !pRot || !pUp || !pRight || !pForward)
        return DE_ERROR;
    return Common()->GetRotationVectors(*pRot, *pUp, *pRight, *pForward);
}

DRESULT CServerDE::SetupEuler(DRotation *pRot, float fPitch, float fYaw,
                              float fRoll)
{
    if (!Common() || !pRot)
        return DE_ERROR;
    return Common()->SetupEuler(*pRot, fPitch, fYaw, fRoll);
}

/*
    A NULL up vector means world up - as in LT1 - and must not be rejected.
    Jupiter takes a reference, so NULL becomes (0, 1, 0) - the value LT1 used.
    Rejecting it would leave the rotation unset, and AI would never turn.
*/

DRESULT CServerDE::AlignRotation(DRotation *pRot, DVector *pForward,
                                 DVector *pUp)
{
    if (!DEMath() || !pRot || !pForward)
        return DE_ERROR;

    DVector vUp;
    if (pUp)
        vUp = *pUp;
    else
        vUp.Init(0.0f, 1.0f, 0.0f);

    return DEMath()->AlignRotation(*pRot, *pForward, vUp);
}


/* ========================================================================== *
    GetObjectName.
 * ========================================================================== */

char* CServerDE::GetObjectName(HOBJECT hObj)
{
    char* pBuffer = NextStringSlot();
    pBuffer[0] = '\0';
    // Called through a base pointer, because a qualified call skips virtual
    // dispatch and the base declaration is pure virtual
    static_cast<ILTServer*>(this)->GetObjectName(hObj, pBuffer, kStringLen);
    return pBuffer;
}


DRESULT CServerDE::EulerRotateX(DRotation *pRot, float fAngle)
{
    if (!DEMath() || !pRot)
        return DE_ERROR;
    return DEMath()->EulerRotateX(*pRot, fAngle);
}

DRESULT CServerDE::EulerRotateY(DRotation *pRot, float fAngle)
{
    if (!DEMath() || !pRot)
        return DE_ERROR;
    return DEMath()->EulerRotateY(*pRot, fAngle);
}

DRESULT CServerDE::EulerRotateZ(DRotation *pRot, float fAngle)
{
    if (!DEMath() || !pRot)
        return DE_ERROR;
    return DEMath()->EulerRotateZ(*pRot, fAngle);
}


/*
    Gravity
    
    A failed read returns a zero vector, 
    because uninitialized gravity reads as motion.
*/

DRESULT CServerDE::GetGlobalForce(DVector *pVec)
{
    if (!pVec)
        return DE_ERROR;
    pVec->Init();
    if (!Physics())
        return DE_ERROR;
    return Physics()->GetGlobalForce(*pVec);
}

DRESULT CServerDE::SetGlobalForce(DVector *pVec)
{
    if (!pVec || !Physics())
        return DE_ERROR;
    return Physics()->SetGlobalForce(*pVec);
}

DRESULT CServerDE::InterpolateRotation(DRotation *pDest, DRotation *pRot1,
                                       DRotation *pRot2, float fT)
{
    if (!DEMath() || !pDest || !pRot1 || !pRot2)
        return DE_ERROR;
    return DEMath()->InterpolateRotation(*pDest, *pRot1, *pRot2, fT);
}

DRESULT CServerDE::RotateAroundAxis(DRotation *pRot, DVector *pAxis,
                                    float fAngle)
{
    if (!DEMath() || !pRot || !pAxis)
        return DE_ERROR;
    return DEMath()->RotateAroundAxis(*pRot, *pAxis, fAngle);
}

DRESULT CServerDE::SetupRotationMatrix(DMatrix *pMat, DRotation *pRot)
{
    if (!DEMath() || !pMat || !pRot)
        return DE_ERROR;
    return DEMath()->SetupRotationMatrix(*pMat, *pRot);
}

DRESULT CServerDE::SetupRotationFromMatrix(DRotation *pRot, DMatrix *pMat)
{
    if (!DEMath() || !pRot || !pMat)
        return DE_ERROR;
    return DEMath()->SetupRotationFromMatrix(*pRot, *pMat);
}


DRESULT CServerDE::SendToServerApp(const char *pMsg) {
    if (!Common())
        return DE_ERROR;

    ILTMessage_Write* pWrite = NULL;
    if (Common()->CreateMessage(pWrite) != LT_OK || !pWrite)
        return DE_ERROR;

    pWrite->IncRef();
    pWrite->WriteString(pMsg ? pMsg : "");

    CLTMsgRef_Read cRead(pWrite->Read());
    DRESULT dResult = ILTServer::SendToServerApp(*cRead);

    pWrite->DecRef();
    return dResult;
}

ObjectList* CServerDE::FindNamedObjects(const char *pName)
{
    if (!pName)
        return NULL;

    ObjArray<HOBJECT, kMaxNamedObjects> objArray;
    uint32 nTotalFound = 0;

    // Called through a base pointer, as in GetObjectName above
    LTRESULT dResult =
        static_cast<ILTServer*>(this)->FindNamedObjects(pName, objArray, &nTotalFound);

    if (nTotalFound > objArray.NumObjects())
    {
        CPrint("DECompat: FindNamedObjects(\"%s\") found %u objects, "
               "returning the first %u (see kMaxNamedObjects)",
               pName, nTotalFound, objArray.NumObjects());
    }

    // Always a list (even when empty) because KeyFramer::CreateKeyList walks it without a null check.
    //Callers release it with RelinquishList.
    ObjectList* pList = CreateObjectList();
    if (!pList)
        return NULL;

    if (dResult == LT_OK)
    {
        for (uint32 i = 0; i < objArray.NumObjects(); ++i)
            AddObjectToList(pList, objArray.GetObject(i));
    }

    return pList;
}

DRESULT CServerDE::PlaySound(PlaySoundInfo *pPlaySoundInfo)
{
    if (!SoundMgr())
        return DE_ERROR;

    HLTSOUND hSound = NULL;
    return SoundMgr()->PlaySound(pPlaySoundInfo, hSound);
}


// nType selected a voice channel, which Jupiter's sound manager does not have
DRESULT CServerDE::PlaySoundLocal(const char *pSoundName,
                                  unsigned char /*nType*/,
                                  unsigned char nPriority) {
    if (!SoundMgr() || !pSoundName)
        return DE_ERROR;

    PlaySoundInfo info;
    PLAYSOUNDINFO_INIT(info);

    // PLAYSOUND_AMBIENT means no 3D attenuation
    info.m_dwFlags   = PLAYSOUND_AMBIENT;
    info.m_nPriority = nPriority;
    SAFE_STRCPY(info.m_szSoundName, pSoundName);

    HLTSOUND hSound = NULL;
    return SoundMgr()->PlaySound(&info, hSound);
}


DEBaseClass* CServerDE::HandleToObject(HOBJECT hObject)
{
    return static_cast<DEBaseClass*>(ILTServer::HandleToObject(hObject));
}

DEBaseClass* CServerDE::CreateObject(HCLASS hClass, ObjectCreateStruct *pStruct)
{
    return static_cast<DEBaseClass*>(ILTServer::CreateObject(hClass, pStruct));
}
