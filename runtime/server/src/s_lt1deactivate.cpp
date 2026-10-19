// s_lt1deactivate.cpp: LT1's object auto deactivation for ported LT1 games (sdk/inc/iltlt1server.h)
//
// The rule, as Shogo does it:
// - Each pass, an active object's countdown (SetDeactivationTime, 0 = never) loses the frame time. 
//   - At zero the object gets no update, animation, or physics until awoken
// - Only while the AutoDeactivate console variable is on and the world has vis data
// - PingObjects resets the countdown of every object it reaches and wakes it
// This is all gated behind LT1AutoDeactivate

#include "bdefs.h"
#include "ltmodule.h"
#include "iltlt1server.h"
#include "servermgr.h"
#include "s_object.h"
#include "serverobj.h"
#include "concommand.h"
#include "server_consolestate.h"

extern int32 g_bLT1AutoDeactivate;

static IServerConsoleState *console_state;
define_holder(IServerConsoleState, console_state);

// The visibility walk
bool sm_LT1WorldHasVis();
void sm_LT1VisitVisibleObjects(const LTVector &vPos, void (*fn)(LTObject *pObj, void *pUser), void *pUser);

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
		LT1SetStateBits(pObj, pObj->m_InternalFlags & (IFLAG_INACTIVE | IFLAG_INACTIVE_TOUCH));
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
};

define_interface(CLTLT1Server, ILTLT1Server);