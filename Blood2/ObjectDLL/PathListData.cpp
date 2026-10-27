// ----------------------------------------------------------------------- //
//
// MODULE  : PathListData.cpp
//
// PURPOSE : PathListData definition for PathList Dynamic Array class
//
// CREATED : 2/9/98
//
// ----------------------------------------------------------------------- //

#include "PathPoint.h"
#include "PathListData.h"
#include "cpp_engineobjects_de.h"

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	PathListData::PathListData()
//
//	PURPOSE:	Initialize object
//
// ----------------------------------------------------------------------- //

PathListData::PathListData()
{
	VEC_INIT(m_vPos);
	m_hstrName			= DNULL;
	m_hstrActionTarget	= DNULL;
	m_hstrActionMessage	= DNULL;
}

PathListData::~PathListData()
{
	// PathMgr lives in the static server shell, so this can run at DLL unload after g_pServerDE is cleared.
	// LT1 destroyed the shell while the engine was still running
	if (!g_pServerDE)
		return;

	g_pServerDE->FreeString( m_hstrName );
	g_pServerDE->FreeString( m_hstrActionTarget	);
	g_pServerDE->FreeString( m_hstrActionMessage );
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	PathListData::Copy()
//
//	PURPOSE:	Copy the PathPoint object's data
//
// ----------------------------------------------------------------------- //

DBOOL PathListData::Copy(HOBJECT hPathPoint)
{
	CServerDE* pServerDE = DEBaseClass::GetServerDE();
	if (!hPathPoint || !pServerDE) return DFALSE;

    // Get the Object Position
	pServerDE->GetObjectPos(hPathPoint, &m_vPos);
	char* pName = pServerDE->GetObjectName(hPathPoint);
	if (pName)
	{
		m_hstrName = pServerDE->CreateString(pName);
	}
    
    // Get the Objects action
    PathPoint *pPathPoint = (PathPoint*)pServerDE->HandleToObject(hPathPoint);

    HSTRING hstrAction = pPathPoint->GetActionTarget();
	if (hstrAction)
	{
		m_hstrActionTarget = pServerDE->CopyString(hstrAction);
	}

    hstrAction = pPathPoint->GetActionMessage();
	if (hstrAction)
	{
		m_hstrActionMessage = pServerDE->CopyString(hstrAction);
	}

	return DTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	PathListData::Save()
//
//	PURPOSE:	Save the PathPoint's data
//
// ----------------------------------------------------------------------- //

void PathListData::Save(HMESSAGEWRITE hWrite)
{
	CServerDE* pServerDE = DEBaseClass::GetServerDE();
	if (!pServerDE || !hWrite) return;

	pServerDE->WriteToMessageVector(hWrite, &m_vPos);
	pServerDE->WriteToMessageHString(hWrite, m_hstrName);
	pServerDE->WriteToMessageHString(hWrite, m_hstrActionTarget);
	pServerDE->WriteToMessageHString(hWrite, m_hstrActionMessage);
}


// ----------------------------------------------------------------------- //
//
//	ROUTINE:	PathListData::Load()
//
//	PURPOSE:	Load the PathPoint's data
//
// ----------------------------------------------------------------------- //

void PathListData::Load(HMESSAGEREAD hRead)
{
	CServerDE* pServerDE = DEBaseClass::GetServerDE();
	if (!pServerDE || !hRead) return;

	pServerDE->ReadFromMessageVector(hRead, &m_vPos);
	m_hstrName			= pServerDE->ReadFromMessageHString(hRead);
	m_hstrActionTarget	= pServerDE->ReadFromMessageHString(hRead);
	m_hstrActionMessage = pServerDE->ReadFromMessageHString(hRead);
}
