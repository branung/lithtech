/*
    LithTech 1.0's server object base on top of Jupiter's BaseClass.
    Game code derives from DEBaseClass in place of BaseClass.
*/

#ifndef __DE_ENGINEOBJECTS_H__
#define __DE_ENGINEOBJECTS_H__

#ifndef __LTENGINEOBJECTS_H__
#include "ltengineobjects.h"
#endif

// BEGIN_CLASS, END_CLASS_DEFAULT, ADD_*PROP and PT_* (LT1 got these from serverobj_de.h)
#ifndef __LTSERVEROBJ_H__
#include "ltserverobj.h"
#endif

#ifndef __LTPROPERTY_H__
#include "ltproperty.h"
#endif

#ifndef __IAGGREGATE_H__
#include "iaggregate.h"
#endif

// ILTMessage_Read and CLTMsgRef_Read for the ObjectMessageFn bridge
#ifndef __ILTMESSAGE_H__
#include "iltmessage.h"
#endif

// ObjectCreateStruct (LT1 got this from engineobjects_de.h)
#ifndef __LTOBJECTCREATE_H__
#include "ltobjectcreate.h"
#endif

// MID_TRIGGER, MID_DAMAGE and the other generic message ids
#include "generic_msg_de.h"


// LT1's PF_GROUP1 through PF_GROUP15 are Jupiter's PF_GROUP(n) with the same bits
#define PF_GROUP1       PF_GROUP(1)
#define PF_GROUP2       PF_GROUP(2)
#define PF_GROUP3       PF_GROUP(3)
#define PF_GROUP4       PF_GROUP(4)
#define PF_GROUP5       PF_GROUP(5)
#define PF_GROUP6       PF_GROUP(6)
#define PF_GROUP7       PF_GROUP(7)
#define PF_GROUP8       PF_GROUP(8)
#define PF_GROUP9       PF_GROUP(9)
#define PF_GROUP10      PF_GROUP(10)
#define PF_GROUP11      PF_GROUP(11)
#define PF_GROUP12      PF_GROUP(12)
#define PF_GROUP13      PF_GROUP(13)
#define PF_GROUP14      PF_GROUP(14)
#define PF_GROUP15      PF_GROUP(15)

#ifndef __DE_SERVER_H__
#include "de_server.h"
#endif

#ifndef __DE_LT1DEFAULTS_H__
#include "de_lt1defaults.h"
#endif


class DEBaseClass : public BaseClass
{
public:

    DEBaseClass(uint8 nType = OT_NORMAL) : BaseClass(nType) {}

    // LT1's static accessor
    static CServerDE* GetServerDE() { return g_pServerDE; }

    /*
        Jupiter's entry point

        Reads the ID StartMessageToObject wrote and hands the payload to LT1's handler below.
        Final, as an override here would skip the ID read and misalign the payload.
    */
    virtual uint32 ObjectMessageFn(HOBJECT hSender, ILTMessage_Read *pMsg) override final
    {
        if (!pMsg)
            return (uint32)ObjectMessageFn(hSender, (DDWORD)0, (HMESSAGEREAD)0);

        DDWORD messageID = pMsg->Readuint32();
        CLTMsgRef_Read cPayload(pMsg->SubMsg(pMsg->Tell()));

        // LT1 rewinds to the payload start when a read lands on the end.
        // The payload starts at 0 here.
        DECompatMsg::CScope cRewind((ILTMessage_Read*)cPayload, 0);

        return (uint32)ObjectMessageFn(hSender, messageID,
                                       (ILTMessage_Read*)cPayload);
    }

    // LT1's entry point.
    // Overrides chain back here, which walks the aggregates (Shogo's damage goes through them)
    virtual DDWORD ObjectMessageFn(HOBJECT hSender, DDWORD messageID,
                                   HMESSAGEREAD hRead);

    /*
        Same signature in both engines

        Overridden only as a sign of life for DECompat_NoteServerActivity.
        The four parameter form is IAggregate's and only runs for aggregates.
    */
    virtual uint32 EngineMessageFn(uint32 messageID, void *pData,
                                   float fData) override
    {
        DECompat_NoteServerActivity(g_pServerDE);

        return BaseClass::EngineMessageFn(messageID, pData, fData);
    }
};


/*
    MID_GETFORCEUPDATEOBJECTS

    Jupiter never sends it, so handlers for it are dead code.
    The type stays because it appears in method signatures.
*/
#define MAX_FORCEUPDATE_OBJECTS 200

typedef struct
{
    HOBJECT*    m_Objects;      // MAX_FORCEUPDATE_OBJECTS large
    DDWORD      m_nObjects;
} ForceUpdate;

// LT1's value, which no Jupiter MID_* uses
#define MID_GETFORCEUPDATEOBJECTS 11



/*
    LT1's aggregate base

    IAggregate dropped the messageID parameter the same way,
    so an unbridged LT1 override compiles and is never called.
*/
class Aggregate : public IAggregate
{
public:

    // Only satisfies IAggregate's pure virtual.
    // Unreachable, since Jupiter reaches aggregates through DEBaseClass's final override.
    virtual uint32 ObjectMessageFn(ILTBaseClass *pObject, HOBJECT hSender,
                                   ILTMessage_Read *pMsg) override final
    {
        DDWORD messageID = pMsg ? pMsg->Readuint32() : 0;
        return (uint32)ObjectMessageFn(static_cast<DEBaseClass*>(pObject),
                                       hSender, messageID, pMsg);
    }

    // LT1's entry point, which aggregates override
    virtual DDWORD ObjectMessageFn(DEBaseClass * /*pObject*/, HOBJECT /*hSender*/,
                                   DDWORD /*messageID*/, HMESSAGEREAD /*hRead*/)
    {
        return 1;
    }

    virtual uint32 EngineMessageFn(ILTBaseClass *pObject, uint32 messageID,
                                   void *pData, LTFLOAT lData) override final
    {
        return EngineMessageFn(static_cast<DEBaseClass*>(pObject), messageID,
                               pData, lData);
    }

    virtual DDWORD EngineMessageFn(DEBaseClass * /*pObject*/, DDWORD /*messageID*/,
                                   void * /*pData*/, DFLOAT /*lData*/)
    {
        return 1;
    }
};


/*
    LT1's BaseClass::ObjectMessageFn

    Loops through the aggregate list, so CDestructable sees MID_DAMAGE.
    Each aggregate reads from the payload start.
    The cast is safe because every aggregate derives from the Aggregate shim.
*/
inline DDWORD DEBaseClass::ObjectMessageFn(HOBJECT hSender, DDWORD messageID,
                                           HMESSAGEREAD hRead)
{
    LPAGGREGATE pAggregate = m_pFirstAggregate;
    while (pAggregate)
    {
        if (hRead)
            hRead->SeekTo(0);
        static_cast<Aggregate*>(pAggregate)->ObjectMessageFn(this, hSender,
                                                             messageID, hRead);
        pAggregate = pAggregate->m_pNextAggregate;
    }

    if (hRead)
        hRead->SeekTo(0);
    return 1;
}


// LT1's StartPoint - registered in de_engineobjects.cpp
class StartPoint : public DEBaseClass
{
public:
    StartPoint(uint8 nType = OT_NORMAL) : DEBaseClass(nType) {}
};


// Free function spelling for unqualified GetServerDE() calls
inline CServerDE* GetServerDE() { return g_pServerDE; }

#endif // __DE_ENGINEOBJECTS_H__
