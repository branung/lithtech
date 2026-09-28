// ----------------------------------------------------------------------- //
//
// MODULE  : ShogoDiag.cpp
//
// PURPOSE : Runtime diagnostics for Shogo porting
//
// ----------------------------------------------------------------------- //

#include "ShogoDiag.h"
#include "CMoveMgr.h"
#include "WeaponModel.h"
#include "iltphysics.h"
#include "iltcommon.h"
#include "iltmodel.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static ILTMath* pDiagMath;
define_holder(ILTMath, pDiagMath);

// The flags named in a trace (not every flag)
static const struct
{
	uint32		nFlag;
	const char*	pszName;
}
s_FlagNames[] =
{
	{ FLAG_VISIBLE,			"VISIBLE"	},
	{ FLAG_SOLID,			"SOLID"		},
	{ FLAG_GRAVITY,			"GRAVITY"	},
	{ FLAG_STAIRSTEP,		"STAIRSTEP"	},
	{ FLAG_GOTHRUWORLD,		"GOTHRUWORLD" },
	{ FLAG_TOUCH_NOTIFY,	"TOUCH"		},
	{ FLAG_RAYHIT,			"RAYHIT"	},
	{ FLAG_REALLYCLOSE,		"REALLYCLOSE" },
	{ FLAG_POINTCOLLIDE,	"POINTCOLLIDE" },
};

static void FlagsToString(uint32 dwFlags, char* pBuf, int nBufLen)
{
	pBuf[0] = '\0';
	int nUsed = 0;

	for (int i = 0; i < (int)(sizeof(s_FlagNames) / sizeof(s_FlagNames[0])); i++)
	{
		if (!(dwFlags & s_FlagNames[i].nFlag))
			continue;

		int nLen = (int)strlen(s_FlagNames[i].pszName);
		if (nUsed + nLen + 2 >= nBufLen)
			break;

		if (nUsed > 0)
			pBuf[nUsed++] = '|';

		memcpy(pBuf + nUsed, s_FlagNames[i].pszName, nLen);
		nUsed += nLen;
		pBuf[nUsed] = '\0';
	}

	if (nUsed == 0 && nBufLen > 1)
		strcpy(pBuf, "-");
}

static const char* ObjectTypeName(uint32 nType)
{
	switch (nType)
	{
		case OT_NORMAL:			return "NORMAL";
		case OT_MODEL:			return "MODEL";
		case OT_WORLDMODEL:		return "WORLDMODEL";
		case OT_SPRITE:			return "SPRITE";
		case OT_CAMERA:			return "CAMERA";
		case OT_CONTAINER:		return "CONTAINER";
		default:				break;
	}
	return "?";
}


CShogoDiag::CShogoDiag()
{
	m_pClientDE			= LTNULL;
	m_nLevel			= 0;
	m_fNextSampleTime	= 0.0f;
	m_fNextWeaponTime	= 0.0f;
	m_nFrame			= 0;
	m_bHaveLast			= LTFALSE;
	m_hLastStandingOn	= LTNULL;
	m_bLastOnGround		= LTFALSE;
	m_bWorldReported	= LTFALSE;
	m_nShotsReported	= 0;

	VEC_INIT(m_vLastPos);
	VEC_INIT(m_vLastDims);
}


void CShogoDiag::Init(ILTClient* pClientDE)
{
	m_pClientDE = pClientDE;
}


float CShogoDiag::CVar(const char* pName, float fDefault)
{
	if (!m_pClientDE)
		return fDefault;

	HCONSOLEVAR hVar = m_pClientDE->GetConsoleVar(const_cast<char*>(pName));
	if (!hVar)
		return fDefault;

	return m_pClientDE->GetVarValueFloat(hVar);
}


void CShogoDiag::Print(const char* pCategory, const char* pMsg, ...)
{
	if (!m_pClientDE)
		return;

	char szBody[400];
	va_list args;
	va_start(args, pMsg);
	vsnprintf(szBody, sizeof(szBody), pMsg, args);
	va_end(args);
	szBody[sizeof(szBody) - 1] = '\0';

	m_pClientDE->CPrint("[D:%s] %s", pCategory, szBody);
}


void CShogoDiag::ReadLevel()
{
	m_nLevel = (int)CVar("Diag", 0.0f);
}


void CShogoDiag::OnEnterWorld(const char* pWorldName)
{
	m_nShotsReported = 0;

	ReadLevel();
	if (m_nLevel <= 0)
		return;

	if (pWorldName && pWorldName[0])
	{
		Print("ENV", "world '%s'", pWorldName);
	}
	else
	{
		Print("ENV", "WARNING: client shell has no world name. PreLoadWorld did not run");
	}

	// The player view projection settings, read from their console variables
	Print("ENV", "PVModelFOV=%.1f reallyclose_near=%.2f reallyclose_far=%.2f DrawGuns=%.0f",
		CVar("PVModelFOV", -1.0f),
		CVar("reallyclose_near", -1.0f),
		CVar("reallyclose_far", -1.0f),
		CVar("DrawGuns", -1.0f));

	if (m_pClientDE->Physics())
	{
		LTVector vForce;
		VEC_INIT(vForce);
		m_pClientDE->Physics()->GetGlobalForce(vForce);
		Print("ENV", "global force (gravity) = (%.1f, %.1f, %.1f)",
			vForce.x, vForce.y, vForce.z);
	}

	m_bHaveLast			= LTFALSE;
	m_bWorldReported	= LTTRUE;
}


void CShogoDiag::OnTransmissionDraw(float fX, float fY, float fTimeLeft, bool bExternalCam)
{
	ReadLevel();
	if (m_nLevel <= 0)
		return;

	static int s_nCall = 0;
	if ((s_nCall++ % 10) != 0) return;

	Print("TRANSDRAW", "at (%.1f,%.1f) timeLeft %.2f externalCamera %d",
		(double)fX, (double)fY, (double)fTimeLeft, (int)(bExternalCam ? 1 : 0));
}


void CShogoDiag::OnTransmission(uint32 nStringID, const char* pImageName,
                                HSURFACE hImage, HSURFACE hText)
{
	ReadLevel();
	if (m_nLevel <= 0)
		return;

	uint32 nImageW = 0, nImageH = 0, nTextW = 0, nTextH = 0, nScreenW = 0, nScreenH = 0;
	if (hImage) m_pClientDE->GetSurfaceDims(hImage, &nImageW, &nImageH);
	if (hText)  m_pClientDE->GetSurfaceDims(hText, &nTextW, &nTextH);
	HSURFACE hScreen = m_pClientDE->GetScreenSurface();
	if (hScreen) m_pClientDE->GetSurfaceDims(hScreen, &nScreenW, &nScreenH);

	Print("TRANS", "string=%u image='%s' %ux%u text=%ux%u screen=%ux%u",
		nStringID, pImageName ? pImageName : "(null)",
		nImageW, nImageH, nTextW, nTextH, nScreenW, nScreenH);
}


void CShogoDiag::OnWeaponCreated(CWeaponModel* pWeaponModel)
{
	ReadLevel();
	if (m_nLevel <= 0 || !pWeaponModel)
		return;

	HLOCALOBJ hWeapon = pWeaponModel->GetHandle();
	if (!hWeapon)
	{
		Print("WPN", "weapon id %d has NO object! CreateModel failed",
			(int)pWeaponModel->GetId());
		return;
	}

	uint32 dwFlags = 0;
	m_pClientDE->Common()->GetObjectFlags(hWeapon, OFT_Flags, dwFlags);

	char szFlags[128];
	FlagsToString(dwFlags, szFlags, sizeof(szFlags));

	LTVector vPos;
	VEC_INIT(vPos);
	m_pClientDE->GetObjectPos(hWeapon, &vPos);

	char* pszModel = GetPVModelName(pWeaponModel->GetId());

	Print("WPN", "id=%d model='%s'", (int)pWeaponModel->GetId(),
		pszModel ? pszModel : "?");

	// Camera relative because the model is REALLYCLOSE
	Print("WPN", "flags=%s  camera-space pos=(%.2f, %.2f, %.2f)  world pos=(%.1f, %.1f, %.1f)",
		szFlags, vPos.x, vPos.y, vPos.z,
		pWeaponModel->GetModelPos().x,
		pWeaponModel->GetModelPos().y,
		pWeaponModel->GetModelPos().z);

	if (!(dwFlags & FLAG_REALLYCLOSE))
		Print("WPN", "WARNING: weapon model is not FLAG_REALLYCLOSE!");

	Print("WPN", "PVModelFOV=%.1f reallyclose_far=%.2f (world FOV is %d)",
		CVar("PVModelFOV", -1.0f), CVar("reallyclose_far", -1.0f), 90);

	// Where each node points
	DumpModelNodes(hWeapon);
}


void CShogoDiag::SamplePlayer(CMoveMgr* pMoveMgr)
{
	if (!pMoveMgr)
		return;

	HOBJECT hObj = pMoveMgr->GetObject();
	if (!hObj)
	{
		Print("POS", "no movement object yet");
		return;
	}

	ILTPhysics* pPhysics = m_pClientDE->Physics();
	if (!pPhysics)
		return;

	LTVector vPos, vDims, vVel;
	VEC_INIT(vPos);
	VEC_INIT(vDims);
	VEC_INIT(vVel);

	m_pClientDE->GetObjectPos(hObj, &vPos);
	pPhysics->GetObjectDims(hObj, &vDims);
	pPhysics->GetVelocity(hObj, &vVel);

	uint32 dwFlags = 0;
	m_pClientDE->Common()->GetObjectFlags(hObj, OFT_Flags, dwFlags);

	CollisionInfo standingInfo;
	memset(&standingInfo, 0, sizeof(standingInfo));
	pPhysics->GetStandingOn(hObj, &standingInfo);

	const LTBOOL bOnGround = pMoveMgr->IsOnGround();

	// Transitions

	if (m_bHaveLast)
	{
		if (bOnGround != m_bLastOnGround)
		{
			Print("GROUND", "%s at y=%.1f (vel.y=%.1f)",
				bOnGround ? "LANDED" : "LEFT GROUND", vPos.y, vVel.y);
		}

		if (standingInfo.m_hObject != m_hLastStandingOn)
		{
			if (standingInfo.m_hObject)
			{
				uint32 nType = OT_NORMAL;
				m_pClientDE->Common()->GetObjectType(standingInfo.m_hObject, &nType);
				Print("STAND", "now standing on a %s, plane normal (%.2f, %.2f, %.2f)",
					ObjectTypeName(nType),
					standingInfo.m_Plane.m_Normal.x,
					standingInfo.m_Plane.m_Normal.y,
					standingInfo.m_Plane.m_Normal.z);
			}
			else
			{
				Print("STAND", "standing on nothing (was %s)",
					m_hLastStandingOn ? "on something" : "already nothing");
			}
		}

		if (fabs(vDims.x - m_vLastDims.x) > 0.01f ||
		    fabs(vDims.y - m_vLastDims.y) > 0.01f ||
		    fabs(vDims.z - m_vLastDims.z) > 0.01f)
		{
			Print("DIMS", "(%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)   wanted (%.2f, %.2f, %.2f)  scale %.2f",
				m_vLastDims.x, m_vLastDims.y, m_vLastDims.z,
				vDims.x, vDims.y, vDims.z,
				pMoveMgr->GetWantedDims().x,
				pMoveMgr->GetWantedDims().y,
				pMoveMgr->GetWantedDims().z,
				pMoveMgr->GetDimsScale(MS_NORMAL));
		}
	}

	// The state that makes collision impossible

	// FLAG_GOTHRUWORLD skips world collision, and an object without FLAG_SOLID isn't stood on

	if (dwFlags & FLAG_GOTHRUWORLD)
		Print("POS", "WARNING: player object has FLAG_GOTHRUWORLD! It cannot collide with the world");

	if (!(dwFlags & FLAG_SOLID))
		Print("POS", "WARNING: player object is not FLAG_SOLID! Nothing will be stood on");

	// The sample

	if (m_nLevel >= 2 || m_pClientDE->GetTime() >= m_fNextSampleTime)
	{
		char szFlags[128];
		FlagsToString(dwFlags, szFlags, sizeof(szFlags));

		float fDeltaY = m_bHaveLast ? (vPos.y - m_vLastPos.y) : 0.0f;

		Print("POS", "f%-6u pos=(%.1f, %.1f, %.1f) dy=%+.2f vel=(%.1f, %.1f, %.1f) dims=(%.1f, %.1f, %.1f) ground=%d stand=%s flags=%s",
			m_nFrame,
			vPos.x, vPos.y, vPos.z,
			fDeltaY,
			vVel.x, vVel.y, vVel.z,
			vDims.x, vDims.y, vDims.z,
			bOnGround ? 1 : 0,
			standingInfo.m_hObject ? "yes" : "NO",
			szFlags);

		if (m_nLevel < 2)
			m_fNextSampleTime = m_pClientDE->GetTime() + 0.25f;
	}

	m_vLastPos			= vPos;
	m_vLastDims			= vDims;
	m_hLastStandingOn	= standingInfo.m_hObject;
	m_bLastOnGround		= bOnGround;
	m_bHaveLast			= LTTRUE;
}


void CShogoDiag::SampleWeapon(CWeaponModel* pWeaponModel)
{
	// OnWeaponCreated runs before the first UpdateWeaponModel and always reads (0, 0, 0).
	// So the weapon's placement is sampled

	if (!pWeaponModel)
		return;

	HLOCALOBJ hWeapon = pWeaponModel->GetHandle();
	if (!hWeapon)
		return;

	LTVector vPos, vDims;
	VEC_INIT(vPos);
	VEC_INIT(vDims);

	m_pClientDE->GetObjectPos(hWeapon, &vPos);
	if (m_pClientDE->Physics())
		m_pClientDE->Physics()->GetObjectDims(hWeapon, &vDims);

	uint32 dwFlags = 0;
	m_pClientDE->Common()->GetObjectFlags(hWeapon, OFT_Flags, dwFlags);

	char szFlags[128];
	FlagsToString(dwFlags, szFlags, sizeof(szFlags));

	uint32 nAnim = m_pClientDE->GetModelAnimation(hWeapon);
	uint32 nState = m_pClientDE->GetModelPlaybackState(hWeapon);

	uint32 nAnimTime = 0;
	uint32 nAnimLen = 0;
	if (m_pClientDE->GetModelLT())
	{
		m_pClientDE->GetModelLT()->GetCurAnimTime(hWeapon, MAIN_TRACKER, nAnimTime);
		m_pClientDE->GetModelLT()->GetAnimLength(hWeapon, (HMODELANIM)nAnim, nAnimLen);
	}

	float fFrameTime = m_pClientDE->GetFrameTime();

	Print("WPN", "id=%d pos=(%.2f, %.2f, %.2f) anim=%u t=%u/%ums state=0x%x frame=%.4fs flags=%s",
		(int)pWeaponModel->GetId(),
		vPos.x, vPos.y, vPos.z,
		nAnim, nAnimTime, nAnimLen, nState, fFrameTime,
		szFlags);
}


void CShogoDiag::Update(CMoveMgr* pMoveMgr, CWeaponModel* pWeaponModel)
{
	ReadLevel();

	if (m_nLevel <= 0)
	{
		// Reset so turning it back on starts from a clean baseline
		m_bHaveLast = LTFALSE;
		return;
	}

	m_nFrame++;

	const LTBOOL bSampleNow = (m_pClientDE->GetTime() >= m_fNextSampleTime);

	SamplePlayer(pMoveMgr);

	if (bSampleNow && m_pClientDE->GetTime() >= m_fNextWeaponTime)
	{
		SampleWeapon(pWeaponModel);
		m_fNextWeaponTime = m_pClientDE->GetTime() + 1.0f;
	}
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CShogoDiag::OnWeaponFired
//
//	PURPOSE:	Report a shot that doesn't go where the camera looks
//
// ----------------------------------------------------------------------- //

void CShogoDiag::OnWeaponFired(const LTVector& vFirePos,
                               const LTVector& vFireDir,
                               const LTVector& vCameraForward)
{
	ReadLevel();
	if (m_nLevel <= 0)
		return;

	// The dot of two unit vectors: 1.0 is where the player looks, -1.0 straight back
	LTFLOAT fAgreement = (vFireDir.x * vCameraForward.x) +
	                     (vFireDir.y * vCameraForward.y) +
	                     (vFireDir.z * vCameraForward.z);

	const uint32 kAlwaysReport = 3;
	LTBOOL bDisagrees = (fAgreement < 0.99f);

	if (m_nShotsReported >= kAlwaysReport && !bDisagrees)
		return;

	if (m_nShotsReported < kAlwaysReport)
		m_nShotsReported++;

	Print("FIRE", "pos=(%.1f, %.1f, %.1f) dir=(%.3f, %.3f, %.3f) camera=(%.3f, %.3f, %.3f) agreement=%.3f%s",
		vFirePos.x, vFirePos.y, vFirePos.z,
		vFireDir.x, vFireDir.y, vFireDir.z,
		vCameraForward.x, vCameraForward.y, vCameraForward.z,
		fAgreement,
		bDisagrees ? "  <-- NOT WHERE THE PLAYER IS AIMING" : "");

	if (bDisagrees && fAgreement < -0.5f)
	{
		Print("FIRE", "  the shot is travelling backwards through the player");
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CShogoDiag::DumpModelNodes
//
//	PURPOSE:	Where each of a model's nodes points
//
// ----------------------------------------------------------------------- //
void CShogoDiag::DumpModelNodes(HLOCALOBJ hObj)
{
	if (!hObj || !m_pClientDE || !m_pClientDE->GetModelLT())
		return;

	ILTModel* pModelLT = m_pClientDE->GetModelLT();

	// The weapon is REALLYCLOSE, so world space here is camera space.
	// A barrel node with a negative Z forward axis is aimed back at the viewer
	uint32 nNumNodes = 0;
	if (pModelLT->GetNumNodes(hObj, nNumNodes) != LT_OK)
	{
		Print("NODE", "GetNumNodes failed - model has no skeleton?");
		return;
	}

	Print("NODE", "%u nodes. Axes are camera-space (+Z is away from the player)",
		nNumNodes);

	const uint32 kMaxNodes = 24;
	uint32 nShown = 0;

	HMODELNODE hNode = INVALID_MODEL_NODE;
	while (pModelLT->GetNextNode(hObj, hNode, hNode) == LT_OK && nShown < kMaxNodes)
	{
		char szName[64];
		szName[0] = '\0';
		pModelLT->GetNodeName(hObj, hNode, szName, sizeof(szName));

		LTransform tf;
		if (pModelLT->GetNodeTransform(hObj, hNode, tf, true) != LT_OK)
			continue;

		LTVector vR, vU, vF;
		VEC_INIT(vR);
		VEC_INIT(vU);
		VEC_INIT(vF);
		if (pDiagMath)
			pDiagMath->GetRotationVectors(tf.m_Rot, vR, vU, vF);

		Print("NODE", "  %-20s pos=(%6.2f,%6.2f,%6.2f) forward=(%.3f, %.3f, %.3f)%s",
			szName[0] ? szName : "?",
			tf.m_Pos.x, tf.m_Pos.y, tf.m_Pos.z,
			vF.x, vF.y, vF.z,
			(vF.z < -0.5f) ? "  <-- pointing back at the player" : "");

		nShown++;
	}

	if (nNumNodes > kMaxNodes)
		Print("NODE", "  ... %u more not shown", nNumNodes - kMaxNodes);
}
