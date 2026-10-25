/*
    LithTech 1.0's client shell base on top of Jupiter's IClientShellStub.
    The client twin of de_servershell.h.
*/

#ifndef __DE_CLIENTSHELL_H__
#define __DE_CLIENTSHELL_H__

#ifndef __ICLIENTSHELL_H__
#include "iclientshell.h"
#endif

#ifndef __DE_CLIENT_H__
#include "de_client.h"
#endif


// LT1's C style spellings of the shell class
class CClientShellDE;
typedef CClientShellDE  ClientShellDE;
typedef CClientShellDE* LPCLIENTSHELLDE;


class CClientShellDE : public IClientShellStub
{
public:

    CClientShellDE()  { s_pInstance = this; }
    virtual ~CClientShellDE() { if (s_pInstance == this) s_pInstance = NULL; }

    // LT1's accessor
    static CClientDE* GetClientDE() { return g_pClientDE; }

    // What CClientDE::GetClientShell returns
    static CClientShellDE* GetInstance() { return s_pInstance; }


    /*
        Jupiter's entry points
        All final so a shell that overrides the Jupiter form by mistake gets a compile error.
    */

    virtual void OnMessage(ILTMessage_Read *pMessage) override final
    {
        DBYTE messageID = pMessage ? pMessage->Readuint8() : 0;
        OnMessage(messageID, pMessage);
    }

    virtual void PreLoadWorld(const char *pWorldName) override final
    {
        // The shell only reads the name
        PreLoadWorld(const_cast<char*>(pWorldName));
    }

    virtual LTRESULT OnObjectMove(HLOCALOBJ hObj, bool bTeleport, LTVector *pNewPos) override final
    {
        return OnObjectMove(hObj, (DBOOL)(bTeleport ? DTRUE : DFALSE), pNewPos);
    }

    virtual LTRESULT OnObjectRotate(HLOCALOBJ hObj, bool bTeleport, LTRotation *pNewRot) override final
    {
        return OnObjectRotate(hObj, (DBOOL)(bTeleport ? DTRUE : DFALSE), pNewRot);
    }


    /*
        LT1's entry points, which shells override.
        The ones Jupiter kept unchanged (OnCommandOn, OnKeyDown, Update, OnEnterWorld, ...) come from IClientShellStub.
    */

protected:

    virtual void    OnMessage(DBYTE /*messageID*/, HMESSAGEREAD /*hMessage*/) {}
    virtual void    PreLoadWorld(char * /*pWorldName*/) {}
    virtual DRESULT OnObjectMove(HLOCALOBJ /*hObj*/, DBOOL /*bTeleport*/, DVector * /*pNewPos*/) { return DE_OK; }
    virtual DRESULT OnObjectRotate(HLOCALOBJ /*hObj*/, DBOOL /*bTeleport*/, DRotation * /*pNewRot*/) { return DE_OK; }

    // Never called
    virtual void    DemoSerialize(ILTStream * /*pStream*/, DBOOL /*bLoad*/) {}

private:

    static CClientShellDE* s_pInstance;
};

#endif // __DE_CLIENTSHELL_H__
