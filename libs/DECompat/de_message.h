// LithTech 1.0 message handles on top of Jupiter's ILTMessage_Read and ILTMessage_Write.

#ifndef __DE_MESSAGE_H__
#define __DE_MESSAGE_H__

#ifndef __ILTMESSAGE_H__
#include "iltmessage.h"
#endif

#ifndef __DE_TYPES_H__
#include "de_types.h"
#endif


// LT1's write handle, Jupiter's own type so save buffers cast to it
typedef ILTMessage_Write*   HMESSAGEWRITE;

// LT1's read handle, also Jupiter's own type
typedef ILTMessage_Read*    HMESSAGEREAD;


// LT1's rewind at the end of a message
namespace DECompatMsg
{
    struct SDispatch
    {
        ILTMessage_Read* m_pMsg;    // The message being handed to LT1 code
        uint32           m_nStart;  // It's payload start in bits
    };

    inline SDispatch& Current()
    {
        static SDispatch s_Cur = { NULL, 0 };
        return s_Cur;
    }

    // Marks pMsg as being dispatched from nStartBits.
    // Restores the previous one, because a handler can dispatch another.
    class CScope
    {
    public:
        CScope(ILTMessage_Read* pMsg, uint32 nStartBits)
        {
            m_Prev = Current();
            Current().m_pMsg   = pMsg;
            Current().m_nStart = nStartBits;
        }
        ~CScope() { Current() = m_Prev; }

    private:
        CScope(const CScope&);
        CScope& operator=(const CScope&);
        SDispatch m_Prev;
    };

    // Call at the end of every LT1 read shim
    inline void AfterRead(ILTMessage_Read* pMsg)
    {
        if (!pMsg)
            return;
        const SDispatch& cur = Current();
        if (cur.m_pMsg != pMsg)
            return;
        if (pMsg->EOM())
            pMsg->SeekTo(cur.m_nStart);
    }
}



/*
    Destinations of messages started by StartMessage*, held until EndMessage

    Usually only one message is under construction, so a small array beats a hash map.
*/
class CDEPendingSends
{
public:
    enum EDest
    {
        eDest_Object,       // SendToObject(msg, hSender, hSendTo, flags)
        eDest_Client,       // SendToClient(msg, hClient, flags)
        eDest_Server,       // SendToServer(msg, hSender, flags)
        eDest_SFX,          // SetObjectSFXMessage(hObject, msg)
        eDest_InstantSFX,   // SendSFXMessage(msg, vPos, flags)
    };

    struct SEntry
    {
        ILTMessage_Write*   m_pMsg;
        EDest               m_eDest;
        HOBJECT             m_hSender;
        HOBJECT             m_hSendTo;
        HCLIENT             m_hClient;
        LTVector            m_vPos;
    };

    // Takes no reference as StartMessage* holds the only one until EndMessage
    static void  Add(const SEntry& entry);

    // Looks up and forgets pMsg's destination.
    // Returns false for a buffer this layer didn't start.
    static bool  Take(ILTMessage_Write* pMsg, SEntry& entryOut);

    // Called on shell shutdown so an unsent message doesnt outlive the level
    static void  Clear();

    // Messages awaiting EndMessage, as a leak check
    static uint32 Count();
};

#endif // __DE_MESSAGE_H__
