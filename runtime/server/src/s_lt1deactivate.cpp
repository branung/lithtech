// s_lt1deactivate.cpp: LT1's object auto deactivation for ported LT1 games (sdk/inc/iltlt1server.h)
//
// The rule, as Shogo does it:
// - Each pass, an active object's countdown (SetDeactivationTime, 0 = never) loses the frame time. 
//   - At zero the object gets no update, animation, or physics until awoken
// - Only while the AutoDeactivate console variable is on and the world has vis data
// - PingObjects resets the countdown of every object it reaches and wakes it
// This is all gated behind LT1AutoDeactivate

#include "bdefs.h"
#include "s_client.h"
#include "packetdefs.h"
#include "ltmodule.h"
#include "iltlt1server.h"
#include "de_objects.h"
#include "animtracker.h"
#include "servermgr.h"
#include "s_object.h"
#include "serverobj.h"
#include "concommand.h"
#include "server_consolestate.h"
#include "dhashtable.h"
#include "lt1_vars.h"
#include "de_world.h"


// For LT1DeactivateTrace
static const char *LT1ObjName(LTObject *pObj)
{
	const char *pName = (pObj && pObj->sd && pObj->sd->m_hName) ? (const char *)hs_GetElementKey(pObj->sd->m_hName, LTNULL) : LTNULL;
	return pName ? pName : "?";
}

static IServerConsoleState *console_state;
define_holder(IServerConsoleState, console_state);

// The visibility walk
bool sm_LT1WorldHasVis();
void sm_LT1VisitVisibleObjects(const LTVector &vPos, void (*fn)(LTObject *pObj, void *pUser), void *pUser);
SUserPortal *sm_LT1FindPortal(const char *pName);

// LT1ServerCommands: the game's stand in for its server shell's OnCommandOn, called by OnLT1CommandsPacket (s_net.cpp)
static ILTLT1Server::CommandChangeFn s_pfnLT1CommandChange = LTNULL;

void sm_LT1CommandChanged(Client *pClient, int nCommand, bool bOn)
{
	if (g_bLT1ServerCommands && s_pfnLT1CommandChange)
		s_pfnLT1CommandChange((HCLIENT)pClient, nCommand, bOn);
}

static bool LT1AutoDeactivateOn()
{
	if (!g_bLT1AutoDeactivate || !console_state)
		return false;
	LTCommandVar *pVar = cc_FindConsoleVar(console_state->State(), "AutoDeactivate");
	if (!pVar || pVar->floatVal == 0.0f)
		return false;
	return sm_LT1WorldHasVis();
}

// Moves an object between the active head and inactive tail of the object list with no notification
static void LT1SetStateBits(LTObject *pObj, uint32 nBits)
{
	if ((pObj->m_InternalFlags & IFLAG_INACTIVE_MASK) == nBits)
		return;
	pObj->m_InternalFlags = (pObj->m_InternalFlags & ~IFLAG_INACTIVE_MASK) | nBits;
	dl_RemoveAt(&g_pServerMgr->m_Objects, &pObj->sd->m_ListNode);
	if (nBits)
		dl_AddTail(&g_pServerMgr->m_Objects, &pObj->sd->m_ListNode, pObj);
	else
		dl_AddHead(&g_pServerMgr->m_Objects, &pObj->sd->m_ListNode, pObj);
}

// The countdown, run before each active object's update. True when the object has just deactivated
bool sm_LT1DeactivateTick(LTObject *pObj)
{
	SObjData *sd = pObj->sd;
	if (sd->m_fLT1DeactivateLeft <= 0.0f || !LT1AutoDeactivateOn())
		return false;
	sd->m_fLT1DeactivateLeft -= g_pServerMgr->m_FrameTime;
	if (sd->m_fLT1DeactivateLeft > 0.0f)
		return false;
	LT1SetStateBits(pObj, (pObj->m_InternalFlags & (IFLAG_INACTIVE | IFLAG_INACTIVE_TOUCH)) | IFLAG_AUTODEACTIVATED);
	if (g_LT1DeactivateTrace)
		dsi_ConsolePrint("LT1DEACT off %s t=%.4f", LT1ObjName(pObj), g_pServerMgr->m_GameTime);
	return true;
}

// LT1's sm_ActivateObject, for each object a ping reaches
static void LT1ActivateObject(LTObject *pObj, void *)
{
	if (!pObj || !pObj->sd)
		return;
	SObjData *sd = pObj->sd;
	sd->m_fLT1DeactivateLeft = (sd->m_fLT1DeactivateTime > 0.0f)
		? sd->m_fLT1DeactivateTime + g_pServerMgr->m_FrameTime
		: sd->m_fLT1DeactivateTime;
	if (pObj->m_InternalFlags & IFLAG_AUTODEACTIVATED)
	{
		LT1SetStateBits(pObj, pObj->m_InternalFlags & (IFLAG_INACTIVE | IFLAG_INACTIVE_TOUCH));
		if (g_LT1DeactivateTrace)
			dsi_ConsolePrint("LT1DEACT on  %s t=%.4f (ping)", LT1ObjName(pObj), g_pServerMgr->m_GameTime);
	}
}

class CLTLT1Server : public ILTLT1Server
{
public:
	declare_interface(CLTLT1Server);

	virtual void SetDeactivationTime(HOBJECT hObj, float fTime)
	{
		LTObject *pObj = HandleToServerObj(hObj);
		if (!pObj || !pObj->sd)
			return;
		pObj->sd->m_fLT1DeactivateTime = fTime;
		pObj->sd->m_fLT1DeactivateLeft = fTime;
		if (pObj->m_InternalFlags & IFLAG_AUTODEACTIVATED)
			LT1SetStateBits(pObj, pObj->m_InternalFlags & (IFLAG_INACTIVE | IFLAG_INACTIVE_TOUCH));
	}

	virtual void PingObjects(HOBJECT hObj)
	{
		LTObject *pObj = HandleToServerObj(hObj);
		if (!pObj || !LT1AutoDeactivateOn())
			return;
		sm_LT1VisitVisibleObjects(pObj->GetPos(), &LT1ActivateObject, LTNULL);
	}

	// LT1 only deactivates here while AutoDeactivate is on
	virtual bool AutoDeactivateNow(HOBJECT hObj)
	{
		if (!g_bLT1AutoDeactivate)
			return false;
		LTObject *pObj = HandleToServerObj(hObj);
		if (pObj && LT1AutoDeactivateOn())
			LT1SetStateBits(pObj, IFLAG_AUTODEACTIVATED);
		return true;
	}

	// The model file's command string, which the model loader keeps in Model::m_CommandString
	virtual LTRESULT GetModelCommandString(HOBJECT hModel, char *pStr, uint32 maxLen)
	{
		if (!pStr || !maxLen)
			return LT_INVALIDPARAMS;
		pStr[0] = '\0';
		LTObject *pObj = HandleToServerObj(hModel);
		if (!pObj || pObj->m_ObjectType != OT_MODEL)
			return LT_INVALIDPARAMS;
		Model *pModel = pObj->ToModel()->GetModelDB();
		if (pModel && pModel->m_CommandString)
		{
			strncpy(pStr, pModel->m_CommandString, maxLen - 1);
			pStr[maxLen - 1] = '\0';
		}
		return LT_OK;
	}

	virtual LTRESULT GetPortalFlags(const char *pName, uint32 *pFlags)
	{
		SUserPortal *pPortal = sm_LT1FindPortal(pName);
		if (!pPortal)
			return LT_NOTFOUND;
		if (pFlags)
			*pFlags = pPortal->m_Flags;
		return LT_OK;
	}

	// LT1 keeps only PORTAL_OPEN. It also tells every client, which reads nothing from it, so no message is sent.
	// Not gated on a var, as only an LT1 world has portals and only LT1 game code calls this
	virtual LTRESULT SetPortalFlags(const char *pName, uint32 dwFlags)
	{
		SUserPortal *pPortal = sm_LT1FindPortal(pName);
		if (!pPortal)
			return LT_NOTFOUND;
		pPortal->m_Flags = (uint16)(dwFlags & PORTAL_OPEN);
		return LT_OK;
	}

	// LT1's IsCommandOn: the live set
	virtual bool IsCommandOn(HCLIENT hClient, int nCommand, bool *pbOn)
	{
		Client *pClient = (Client *)hClient;
		if (!g_bLT1ServerCommands || !pClient || !pClient->m_bLT1Commands)
			return false;
		if (pbOn)
		{
			*pbOn = nCommand >= 0 && nCommand < 32 * LT1_COMMAND_WORDS &&
				(pClient->m_LT1Commands[pClient->m_iLT1CommandSet][nCommand >> 5] & (1u << (nCommand & 31))) != 0;
		}
		return true;
	}

	virtual void SetCommandCallback(CommandChangeFn fn)
	{
		s_pfnLT1CommandChange = fn;
	}
};

define_interface(CLTLT1Server, ILTLT1Server);