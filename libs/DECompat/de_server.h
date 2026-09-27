/*
    LithTech 1.0's server interface, backed by Jupiter's ILTServer.
    A derived class, not a wrapper.
*/

#ifndef __DE_SERVER_H__
#define __DE_SERVER_H__

#ifndef __ILTSERVER_H__
#include "iltserver.h"
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

#include <utility>  // std::forward for the forwarders below


class DEBaseClass;


class CServerDE : public ILTServer
{
public:

    /*
        Starting messages.

        Each of these creates a message, writes its ID as the first field, 
        and records the destination for EndMessage. 
        The ID widths follow LT1's own declarations and must match what the converted client shell reads.
    */

    HMESSAGEWRITE   StartMessageToObject(LPBASECLASS pSender, HOBJECT hSendTo,
                                         DDWORD messageID);
    HMESSAGEWRITE   StartMessage(HCLIENT hSendTo, DBYTE messageID);
    HMESSAGEWRITE   StartSpecialEffectMessage(LPBASECLASS pObject);
    HMESSAGEWRITE   StartInstantSpecialEffectMessage(DVector *pPos);

    /*
        Ending messages

        EndMessage is EndMessage2 with MESSAGE_GUARANTEED, as in LT1.
        Calling either on a message this layer did not start does nothing and returns DE_OK,
        because the MID_SAVEOBJECT buffers use these calls and are never ended.
     */

    DRESULT         EndMessage(HMESSAGEWRITE hMessage);
    DRESULT         EndMessage2(HMESSAGEWRITE hMessage, DDWORD flags);

    /*
        Writing
        
        Straight renames onto ILTMessage_Write.
        The mapping table in de_message.h is fixed by the client half of the game.
    */

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
    DRESULT         WriteToMessageHMessageRead(HMESSAGEWRITE hMessage,
                                               HMESSAGEREAD hDataMessage);

    // LT1 had a separate entry point for object references written during MID_SAVEOBJECT.
    // Jupiter's WriteObject handles both cases, so this is the same call.
    DRESULT         WriteToLoadSaveMessageObject(HMESSAGEWRITE hMessage,
                                                 HOBJECT hObject);

    /*
        Reading
    */

    DBYTE           ReadFromMessageByte(HMESSAGEREAD hMessage);
    D_WORD          ReadFromMessageWord(HMESSAGEREAD hMessage);
    DDWORD          ReadFromMessageDWord(HMESSAGEREAD hMessage);
    float           ReadFromMessageFloat(HMESSAGEREAD hMessage);
    HSTRING         ReadFromMessageHString(HMESSAGEREAD hMessage);
    HOBJECT         ReadFromMessageObject(HMESSAGEREAD hMessage);
    void            ReadFromMessageVector(HMESSAGEREAD hMessage, DVector *pVal);

    // The read half of WriteToMessageCompVector/CompPosition above.  
    // The pairing has to match the write side or the two halves disagree.
    void            ReadFromMessageCompVector(HMESSAGEREAD hMessage, DVector *pVal);
    void            ReadFromMessageCompPosition(HMESSAGEREAD hMessage, DVector *pVal);
    void            ReadFromMessageRotation(HMESSAGEREAD hMessage, DRotation *pVal);
    DRESULT         ReadFromLoadSaveMessageObject(HMESSAGEREAD hMessage,
                                                  HOBJECT *hObject);
    HMESSAGEREAD    ReadFromMessageHMessageRead(HMESSAGEREAD hMessage);

    char*           ReadFromMessageString(HMESSAGEREAD hMessage);

    void            EndHMessageRead(HMESSAGEREAD hMessage);
    void            ResetRead(HMESSAGEREAD hRead);


    // Functions outside messaging that LT1 had and Jupiter moved or renamed.

    // -> Common()->GetObjectFlags(hObj, OFT_User, *)
    DDWORD          GetObjectUserFlags(HOBJECT hObj);
    // -> Common()->SetObjectFlags(hObj, OFT_User, flags, FLAGMASK_ALL)
    DRESULT         SetObjectUserFlags(HOBJECT hObj, DDWORD flags);

    // -> Physics()->GetMass / SetMass
    float           GetObjectMass(HOBJECT hObj);
    void            SetObjectMass(HOBJECT hObj, float mass);

    // -> Physics()->SetObjectDims(hObj, pNewDims, SETDIMS_PUSHOBJECTS).
    DRESULT         SetObjectDims2(HOBJECT hObj, DVector *pNewDims);

    // -> Common()->SetObjectFilenames
    DRESULT         SetModelFilenames(HOBJECT hObj, const char *pFilename,
                                      const char *pSkinName);

    // -> GetModelLT()->GetNodeTransform after resolving the node by name.
    DBOOL           GetModelNodeTransform(HOBJECT hObj, const char *pNodeName,
                                          DVector *pPos, DRotation *pRot);

    // The same for an attached model, resolving the attachment's child first
    DBOOL           GetAttachedModelNodeTransform(HATTACHMENT hAttachment,
                                          const char *pNodeName,
                                          DVector *pPos, DRotation *pRot);

    /*
        Node visibility
        
        Casting away const is safe since Jupiter only reads the name .
        Its bool* cannot take a DBOOL's address, as a DBOOL is four bytes.
        A missing node answers LT_NODENOTFOUND like in LT1.
    */

    DRESULT         GetModelNodeHideStatus(HOBJECT hObj, const char *pNodeName,
                                           DBOOL *pHidden)
    {
        bool bHidden = false;
        DRESULT dr = ILTServer::GetModelNodeHideStatus(
            hObj, const_cast<char*>(pNodeName), &bHidden);
        if (pHidden)
            *pHidden = bHidden ? DTRUE : DFALSE;
        return dr;
    }

    // Overload for callers already holding a bool
    DRESULT         GetModelNodeHideStatus(HOBJECT hObj, const char *pNodeName,
                                           bool *pHidden)
    {
        return ILTServer::GetModelNodeHideStatus(
            hObj, const_cast<char*>(pNodeName), pHidden);
    }

    DRESULT         SetModelNodeHideStatus(HOBJECT hObj, const char *pNodeName,
                                           DBOOL bHidden)
    {
        return ILTServer::SetModelNodeHideStatus(
            hObj, const_cast<char*>(pNodeName), bHidden ? true : false);
    }

    /*
        Functions Jupiter moved to Common(), Physics(), GetModelLT() or SoundMgr()
        
        Each call site is checked against Jupiter's own declaration.
    */
   
#define DE_FORWARD(iface, name)                                             \
    template <typename... Args>                                             \
    auto name(Args&&... args) -> decltype(iface->name(std::forward<Args>(args)...)) \
    { return iface->name(std::forward<Args>(args)...); }

    // -> Common()
    DE_FORWARD(Common(), GetModelAnimUserDims)
    DE_FORWARD(Common(), GetPolyTextureFlags)
    DE_FORWARD(Common(), GetAttachedModelNodeTransform)

    // -> Physics()
    DE_FORWARD(Physics(), GetObjectDims)
    // Returns DEBaseClass* like LT1 did. 
    // Every server object the games create derives from DEBaseClass
    DEBaseClass*    HandleToObject(HOBJECT hObject);

    // The same downcast as HandleToObject
    DEBaseClass*    CreateObject(HCLASS hClass, ObjectCreateStruct *pStruct);

    // Replaces LT1's GetWorldObject comparison.
    // It answers LT_YES or LT_NO, never LT_OK.
    DE_FORWARD(Physics(), IsWorldObject)
    DE_FORWARD(Physics(), GetVelocity)
    DE_FORWARD(Physics(), SetVelocity)
    DE_FORWARD(Physics(), GetAcceleration)
    DE_FORWARD(Physics(), SetAcceleration)
    DE_FORWARD(Physics(), SetFrictionCoefficient)
    DE_FORWARD(Physics(), SetForceIgnoreLimit)

    // -> SoundMgr()
    DE_FORWARD(SoundMgr(), KillSound)
    DE_FORWARD(SoundMgr(), GetSoundDuration)

#undef DE_FORWARD

    /*
        Functions that also changed shape
        
        Output pointers became references or output parameters.
        Flag calls gained an ObjFlagType, and LT1's all mean OFT_Flags.
    */

    DDWORD  GetObjectFlags(HOBJECT hObj);
    DRESULT SetObjectFlags(HOBJECT hObj, DDWORD flags);
    DDWORD  GetObjectType(HOBJECT hObj);

    // Jupiter takes an ObjectCreateStruct in place of a filename and skin
    DRESULT SetObjectFilenames(HOBJECT hObj, const char *pFilename,
                               const char *pSkinName);

    // Passes flag 0
    DRESULT SetObjectDims(HOBJECT hObj, DVector *pNewDims);

    /*!
        Property getters
        
        LT1 read into a DBOOL and a DDWORD, Jupiter into a bool and an int32.
        Made into Templates because the games pass several destination types.
        These hide the base versions, which are function pointer members.
    */
    template <typename T>
    DRESULT GetPropBool(const char *pPropName, T *pRet)
    {
        bool bValue = false;
        DRESULT dResult = ILTServer::GetPropBool(pPropName, &bValue);
        if (pRet)
            *pRet = (T)(bValue ? 1 : 0);
        return dResult;
    }

    template <typename T>
    DRESULT GetPropLongInt(const char *pPropName, T *pRet)
    {
        int32 nValue = 0;
        DRESULT dResult = ILTServer::GetPropLongInt(pPropName, &nValue);
        if (pRet)
            *pRet = (T)nValue;
        return dResult;
    }

    // Pointers became references
    DRESULT GetRotationVectors(DRotation *pRot, DVector *pUp, DVector *pRight,
                               DVector *pForward);

    // The world's gravity vector
    DRESULT GetGlobalForce(DVector *pVec);
    DRESULT SetGlobalForce(DVector *pVec);

    DRESULT GetGlobalForce(DVector &vec)       { return Physics() ? Physics()->GetGlobalForce(vec) : DE_ERROR; }
    DRESULT SetGlobalForce(const DVector &vec) { return Physics() ? Physics()->SetGlobalForce(vec) : DE_ERROR; }
    DRESULT SetupEuler(DRotation *pRot, float fPitch, float fYaw, float fRoll);
    DRESULT AlignRotation(DRotation *pRot, DVector *pForward, DVector *pUp);
    DRESULT EulerRotateX(DRotation *pRot, float fAngle);
    DRESULT EulerRotateY(DRotation *pRot, float fAngle);
    DRESULT EulerRotateZ(DRotation *pRot, float fAngle);
    DRESULT InterpolateRotation(DRotation *pDest, DRotation *pRot1,
                                DRotation *pRot2, float fT);
    DRESULT RotateAroundAxis(DRotation *pRot, DVector *pAxis, float fAngle);
    DRESULT SetupRotationMatrix(DMatrix *pMat, DRotation *pRot);
    DRESULT SetupRotationFromMatrix(DRotation *pRot, DMatrix *pMat);

    // The ILTMath instance the rotation functions use
    static ILTMath* DEMath();

    // FindNamedObjects allocates the list LT1 returned, and callers free it with RelinquishList.
    ObjectList*     FindNamedObjects(const char *pName);

    // Wraps the string in a message
    DRESULT         SendToServerApp(const char *pMsg);

    // PlaySound drops Jupiter's output handle, because LT1 callers read it from the PlaySoundInfo.
    DRESULT         PlaySound(PlaySoundInfo *pPlaySoundInfo);

    DRESULT         PlaySoundLocal(const char *pSoundName, unsigned char nType,
                                   unsigned char nPriority);

    // Jupiter takes a bool&. Made a Template for the same reason as GetPropBool.
    template <typename T>
    DRESULT IsSoundDone(HLTSOUND hSound, T *pDone)
    {
        if (!SoundMgr())
            return DE_ERROR;

        bool bDone = false;
        DRESULT dResult = SoundMgr()->IsSoundDone(hSound, bDone);
        if (pDone)
            *pDone = (T)(bDone ? 1 : 0);
        return dResult;
    }

    // Jupiter takes an LTFLOAT& where LT1 took a DFLOAT*
    DRESULT GetSoundDuration(HLTSOUND hSound, DFLOAT *pDuration)
    {
        if (!SoundMgr() || !pDuration)
            return DE_ERROR;

        return SoundMgr()->GetSoundDuration(hSound, *pDuration);
    }

    // Most objects FindNamedObjects returns. 
    // Overflow goes to the console.
    enum { kMaxNamedObjects = 256 };

    
    //Like LT1, GetObjectName returns the name from the same rotating slots as ReadFromMessageString.
    // GetStringData and GetVarValueString only lost const and are virtual, so they cannot be redeclared here.
    char*           GetObjectName(HOBJECT hObj);

    //Both empty because Jupiter manages deactivation itself
    void            SetDeactivationTime(HOBJECT hObj, DFLOAT fDeactivationTime);
    void            PingObjects(HOBJECT hObj);
};


// LT1 spelled this class both ways
typedef CServerDE   ServerDE;

// LT1's names for the two interfaces it exposed separately
typedef ILTCommon   CommonLT;
typedef ILTPhysics  PhysicsLT;

// The engine as a CServerDE read through the module's ILTServer holder.
// It cannot be assigned because there is no variable.
extern ILTServer*   g_pDELTServer;

#define g_pServerDE (static_cast<CServerDE*>(g_pDELTServer))

#endif // __DE_SERVER_H__
