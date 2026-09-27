
// LithTech 1.0's server shell base on top of Jupiter's IServerShellStub

#ifndef __DE_SERVERSHELL_H__
#define __DE_SERVERSHELL_H__

#ifndef __ISERVERSHELL_H__
#include "iservershell.h"
#endif

#ifndef __DE_SERVER_H__
#include "de_server.h"
#endif

#ifndef __DE_ENGINEOBJECTS_H__
#include "de_engineobjects.h"
#endif


class CServerShellDE : public IServerShellStub
{
public:

    virtual ~CServerShellDE() {}

    // LT1's accessor
    CServerDE* GetServerDE() const { return g_pServerDE; }


    /*
        Jupiter's entry points
        All final, as overriding one directly would skip the ID read and misalign the payload.
    */

    virtual void OnMessage(HCLIENT hSender, ILTMessage_Read *pMessage) override final
    {
        DBYTE messageID = pMessage ? pMessage->Readuint8() : 0;
        OnMessage(hSender, messageID, pMessage);
    }

    virtual void OnObjectMessage(LPBASECLASS pSender, ILTMessage_Read *pMessage) override final
    {
        DDWORD messageID = pMessage ? pMessage->Readuint32() : 0;
        OnObjectMessage(pSender, messageID, pMessage);
    }

    virtual LPBASECLASS OnClientEnterWorld(HCLIENT hClient) override final
    {
        return OnClientEnterWorld(hClient, NULL, 0);
    }

    virtual LTRESULT ServerAppMessageFn(ILTMessage_Read& msg) override final
    {
        char szMsg[512];
        szMsg[0] = '\0';
        msg.ReadString(szMsg, sizeof(szMsg));
        return ServerAppMessageFn(szMsg);
    }

    virtual void PreStartWorld(bool bSwitchingWorlds) override final
    {
        PreStartWorld((DBOOL)(bSwitchingWorlds ? DTRUE : DFALSE));
    }


    /*
        LT1's entry points
        Shells override these.
    */

protected:

    virtual DRESULT ServerAppMessageFn(char * /*pMsg*/) { return DE_OK; }

    virtual LPBASECLASS OnClientEnterWorld(HCLIENT hClient, void *pClientData,
                                           DDWORD clientDataLen) = 0;

    virtual void OnMessage(HCLIENT /*hSender*/, DBYTE /*messageID*/,
                           HMESSAGEREAD /*hMessage*/) {}
    virtual void OnObjectMessage(LPBASECLASS /*pSender*/, DDWORD /*messageID*/,
                                 HMESSAGEREAD /*hMessage*/) {}

    virtual void PreStartWorld(DBOOL /*bSwitchingWorlds*/) {}

    // Nothing in Jupiter calls these
    virtual void OnCommandOn(HCLIENT /*hClient*/, int /*command*/) {}
    virtual void OnCommandOff(HCLIENT /*hClient*/, int /*command*/) {}

    // LT1 took no argument and seeded with a constant.
    // Jupiter's seed is ignored.
    virtual void SRand() { srand(123); }
    virtual void SRand(unsigned int uiRand) override { SRand(); (void)uiRand; }
};

#endif // __DE_SERVERSHELL_H__
