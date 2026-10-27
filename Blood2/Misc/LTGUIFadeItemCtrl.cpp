// LTGUIFadeItemCtrl.cpp: implementation of the CLTGUIFadeItemCtrl class.

#include "basedefs_de.h"
#include "cpp_clientshell_de.h"

#include "LTGUICommandHandler.h"
#include "LTGUICtrl.h"
#include "LTGUIFadeItemCtrl.h"

#include <string.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLTGUIFadeItemCtrl::CLTGUIFadeItemCtrl()
{
	m_dwFadeTime		= 300;
	m_pSurfArray		= DNULL;
	m_hDisabledSurf		= DNULL;
	m_nSurfaces			= 0;
	m_dwWidth			= 0;
	m_dwHeight			= 0;
	m_fLastSelTime		= 0.0f;
	m_pCommandHandler	= DNULL;

	// Not in the original, see Create
	m_bOwnSurfaces		= DFALSE;
}

CLTGUIFadeItemCtrl::~CLTGUIFadeItemCtrl()
{
	Destroy();
}

//////////////////////////////////////////////////////////////////////
// Creation
//////////////////////////////////////////////////////////////////////

// The original never read bOwnSurfaces, so Destroy depended on uninitialized memory.
// It's set here, so these surfaces are deleted on Destroy where 1998 most likely leaked them
DBOOL CLTGUIFadeItemCtrl::Create(CClientDE *pClientDE, DDWORD dwCommandID, HSURFACE *pSurfArray,
								 int nSurfaces, HSURFACE hDisabledSurf, DBOOL bOwnSurfaces,
								 CLTGUICommandHandler *pCommandHandler)
{
	m_pClientDE		= pClientDE;
	m_dwCommandID	= dwCommandID;
	m_hDisabledSurf	= hDisabledSurf;

	m_bOwnSurfaces	= bOwnSurfaces;			// not in the original; see above

	m_pSurfArray = (HSURFACE *)new char[nSurfaces * sizeof(HSURFACE)];
	memcpy(m_pSurfArray, pSurfArray, nSurfaces * sizeof(HSURFACE));

	m_nSurfaces = nSurfaces;

	DDWORD nWidth  = 0;
	DDWORD nHeight = 0;

	for (int i = 0; i < m_nSurfaces; ++i)
	{
		m_pClientDE->GetSurfaceDims(m_pSurfArray[i], &nWidth, &nHeight);

		if (m_dwWidth <= nWidth)
			m_dwWidth = nWidth;

		if (m_dwHeight <= nHeight)
			m_dwHeight = nHeight;
	}

	if (m_hDisabledSurf)
	{
		m_pClientDE->GetSurfaceDims(m_hDisabledSurf, &nWidth, &nHeight);

		if (m_dwWidth <= nWidth)
			m_dwWidth = nWidth;

		if (m_dwHeight <= nHeight)
			m_dwHeight = nHeight;
	}

	m_pCommandHandler = pCommandHandler;

	m_bCreated = DTRUE;
	return DTRUE;
}

void CLTGUIFadeItemCtrl::Destroy()
{
	if (m_pSurfArray)
	{
		if (m_bOwnSurfaces)
		{
			for (int i = 0; i < m_nSurfaces; ++i)
			{
				m_pClientDE->DeleteSurface(m_pSurfArray[i]);
				m_pSurfArray[i] = DNULL;
			}

			if (m_hDisabledSurf)
			{
				m_pClientDE->DeleteSurface(m_hDisabledSurf);
				m_hDisabledSurf = DNULL;
			}
		}

		delete [] (char *)m_pSurfArray;
		m_pSurfArray = DNULL;
		m_nSurfaces  = 0;
	}

	m_bCreated = DFALSE;
}

//////////////////////////////////////////////////////////////////////
// Rendering
//////////////////////////////////////////////////////////////////////

void CLTGUIFadeItemCtrl::Render(HSURFACE hDestSurf)
{
	HSURFACE hSurf;

	if (!m_bEnabled)
	{
		hSurf = m_hDisabledSurf;

		if (!hSurf)
			hSurf = m_pSurfArray[m_nSurfaces - 1];
	}
	else if (m_bSelected)
	{
		hSurf = m_pSurfArray[0];
	}
	else
	{
		float fPercent = (m_pClientDE->GetTime() - m_fLastSelTime) /
						 ((float)m_dwFadeTime * 0.001f);

		if (fPercent > 1.0f)
		{
			hSurf = m_pSurfArray[m_nSurfaces - 1];
		}
		else
		{
			hSurf = m_pSurfArray[(int)(fPercent * (float)(m_nSurfaces - 1))];
		}
	}

	HDECOLOR hTransColor = m_pClientDE->SetupColor1(1.0f, 0.0f, 1.0f, DFALSE);

	m_pClientDE->DrawSurfaceToSurfaceTransparent(hDestSurf, hSurf, DNULL,
												 m_pos.x, m_pos.y, hTransColor);
}

//////////////////////////////////////////////////////////////////////
// Input
//////////////////////////////////////////////////////////////////////

void CLTGUIFadeItemCtrl::OnEnter()
{
	if (m_pCommandHandler)
	{
		m_pCommandHandler->SendCommand(m_dwCommandID, 0, 0);
	}
}

void CLTGUIFadeItemCtrl::OnSelChange()
{
	if (!m_bSelected)
	{
		m_fLastSelTime = m_pClientDE->GetTime();
	}
}
