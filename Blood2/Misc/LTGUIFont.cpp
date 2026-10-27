// LTGUIFont.cpp: implementation of the CLTGUIFont class.

#include "basedefs_de.h"
#include "cpp_clientshell_de.h"

#include "CoolFont.h"
#include "LTGUIFont.h"

#include <mbstring.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////
// Static members
//////////////////////////////////////////////////////////////////////

// On by default. CMainMenus::InitFonts sets it from IDS_FONT_WRAP_USE_SPACES
DBOOL CLTGUIFont::s_bWrapUseSpaces = DTRUE;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLTGUIFont::CLTGUIFont()
{
	m_bBitmap	= DFALSE;
	m_pCoolFont	= DNULL;
	m_hFont		= DNULL;
}

CLTGUIFont::~CLTGUIFont()
{
	Term();
}

//////////////////////////////////////////////////////////////////////
// Initialization
//////////////////////////////////////////////////////////////////////

DBOOL CLTGUIFont::Init(CClientDE *pClientDE, const char *lpszBitmapFile, const char *lpszFontWidths)
{
	m_pClientDE = pClientDE;
	if (!pClientDE)
		return DFALSE;

	m_pCoolFont = new CoolFont;
	if (!m_pCoolFont)
		return DFALSE;

	if (!m_pCoolFont->Init(pClientDE, (char *)lpszBitmapFile))
	{
		delete m_pCoolFont;
		m_pCoolFont = DNULL;
		return DFALSE;
	}

	if (!m_pCoolFont->LoadXWidths((char *)lpszFontWidths))
	{
		m_pCoolFont->Free();
		delete m_pCoolFont;
		m_pCoolFont = DNULL;
		return DFALSE;
	}

	m_bBitmap = DTRUE;
	return DTRUE;
}

DBOOL CLTGUIFont::Init(CClientDE *pClientDE, const char *pFontName, int nWidth, int nHeight,
					   DBOOL bItalic, DBOOL bUnderline, DBOOL bBold)
{
	m_pClientDE = pClientDE;
	if (!pClientDE)
		return DFALSE;

	m_hFont = pClientDE->CreateFont((char *)pFontName, nWidth, nHeight, bItalic, bUnderline, bBold);
	if (!m_hFont)
		return DFALSE;

	m_nFontHeight = nHeight;
	return DTRUE;
}

void CLTGUIFont::Term()
{
	if (m_bBitmap)
	{
		if (m_pCoolFont)
		{
			m_pCoolFont->Free();
			delete m_pCoolFont;
			m_pCoolFont = DNULL;
		}
	}
	else
	{
		if (m_hFont && m_pClientDE)
		{
			m_pClientDE->DeleteFont(m_hFont);
			m_hFont = DNULL;
		}
	}
}

//////////////////////////////////////////////////////////////////////
// Drawing
//////////////////////////////////////////////////////////////////////

void CLTGUIFont::Draw(HSTRING hString, HSURFACE hDest, int x, int y, int justify)
{
	if (!m_pClientDE)	return;
	if (!hString)		return;

	if (m_bBitmap && m_pCoolFont)
	{
		m_pCoolFont->Draw(m_pClientDE->GetStringData(hString), hDest,
						  (short)x, (short)y, (short)justify);
	}

	if (!m_bBitmap && m_hFont)
	{
		DrawSolid(hString, hDest, x, y, justify, SETRGB(255,255,255));
	}
}

void CLTGUIFont::DrawFormat(HSTRING hString, HSURFACE hDest, int x, int y, int nWidth)
{
	if (!m_pClientDE)	return;
	if (!hString)		return;

	if (m_bBitmap && m_pCoolFont)
	{
		m_pCoolFont->DrawFormat(m_pClientDE->GetStringData(hString), hDest,
								(short)x, (short)y, (short)nWidth);
	}

	if (!m_bBitmap && m_hFont)
	{
		DrawSolidFormat(hString, hDest, x, y, nWidth, SETRGB(255,255,255));
	}
}

void CLTGUIFont::DrawSolid(HSTRING hString, HSURFACE hDest, int x, int y, int justify, HDECOLOR color)
{
	if (!m_pClientDE)	return;
	if (!hString)		return;

	if (m_bBitmap && m_pCoolFont)
	{
		m_pCoolFont->DrawSolid(m_pClientDE->GetStringData(hString), hDest,
							   (short)x, (short)y, (short)justify, color);
	}

	if (!m_bBitmap && m_hFont)
	{
		DDWORD	nSurfWidth  = 0;
		DDWORD	nSurfHeight = 0;

		m_pClientDE->GetSurfaceDims(hDest, &nSurfWidth, &nSurfHeight);

		if (justify == CF_JUSTIFY_CENTER)
		{
			x -= GetTextExtents(hString).x / 2;
		}
		else if (justify == CF_JUSTIFY_RIGHT)
		{
			x -= GetTextExtents(hString).x;
		}

		DRect rect;
		rect.left	= x;
		rect.top	= y;
		rect.right	= (int)nSurfWidth  - 1;
		rect.bottom	= (int)nSurfHeight - 1;

		m_pClientDE->DrawStringToSurface(hDest, m_hFont, hString, &rect, color,
										 m_pClientDE->SetupColor1(0.0f, 0.0f, 0.0f, DFALSE));
	}
}

//////////////////////////////////////////////////////////////////////
// Word wrapping
//////////////////////////////////////////////////////////////////////


void CLTGUIFont::DrawSolidFormat(HSTRING hString, HSURFACE hDest, int x, int y, int nWidth, HDECOLOR color)
{
	if (!m_pClientDE)	return;
	if (!hString)		return;

	if (m_bBitmap && m_pCoolFont)
	{
		m_pCoolFont->DrawSolidFormat(m_pClientDE->GetStringData(hString), hDest,
									 (short)x, (short)y, (short)nWidth, color);
	}

	if (m_bBitmap)		return;
	if (!m_hFont)		return;

	DDWORD	nSurfWidth  = 0;
	DDWORD	nSurfHeight = 0;

	m_pClientDE->GetSurfaceDims(hDest, &nSurfWidth, &nSurfHeight);

	if (x + nWidth >= (int)nSurfWidth)
		nWidth = (int)nSurfWidth - x - 1;

	int nCurX = x;
	int nCurY = y;

	const char *pStart = m_pClientDE->GetStringData(hString);
	const char *pEnd   = pStart;

	while (*pStart)
	{
		if (s_bWrapUseSpaces)
		{
			while (*pEnd && *pEnd != ' ')
				pEnd = (const char *)_mbsinc((const unsigned char *)pEnd);

			if (*pEnd == ' ')
				pEnd = (const char *)_mbsinc((const unsigned char *)pEnd);
		}
		else
		{
			const char *szNoLineStart[10] = { ".", ")", "]", "\"", ",", "?", "!", ";", ":", "/" };

			pEnd = (const char *)_mbsinc((const unsigned char *)pEnd);

			for (;;)
			{
				const char *pNext = (const char *)_mbsinc((const unsigned char *)pEnd);

				int i;
				for (i = 0; i < 10; ++i)
				{
					if (*pNext == *szNoLineStart[i])
						break;
				}

				if (i >= 10)
					break;

				pEnd = (const char *)_mbsinc((const unsigned char *)pNext);

				if (!*pEnd)
					break;
			}
		}

		if (pEnd != pStart)
		{
			int nLen = (int)(pEnd - pStart);

			char *pWord = new char[nLen + 1];
			memcpy(pWord, pStart, nLen);
			pWord[nLen] = '\0';

			HSTRING hWord = m_pClientDE->CreateString(pWord);

			DIntPt extents = GetTextExtents(hWord);

			if (nCurX + extents.x > x + nWidth)
			{
				nCurX = x;
				nCurY += GetHeight();
			}

			DrawSolid(hWord, hDest, nCurX, nCurY, CF_JUSTIFY_LEFT, color);

			nCurX += extents.x;

			m_pClientDE->FreeString(hWord);
			delete [] pWord;
		}

		pStart = pEnd;
	}
}

//////////////////////////////////////////////////////////////////////
// Measurement
//////////////////////////////////////////////////////////////////////

DIntPt CLTGUIFont::GetTextExtents(HSTRING hString)
{
	DIntPt	result;
	result.x = 0;
	result.y = 0;

	if (!m_pClientDE)	return result;
	if (!hString)		return result;

	if (m_bBitmap)
	{
		if (m_pCoolFont)
			return m_pCoolFont->GetTextExtents(m_pClientDE->GetStringData(hString));

		return result;
	}

	if (m_hFont)
	{
		m_pClientDE->GetStringDimensions(m_hFont, hString, &result.x, &result.y);
	}

	return result;
}

DIntPt CLTGUIFont::GetTextExtentsFormat(HSTRING hString, int nWidth)
{
	DIntPt	result;
	result.x = 0;
	result.y = 0;

	if (!m_pClientDE)	return result;
	if (!hString)		return result;

	if (m_bBitmap)
	{
		if (m_pCoolFont)
			return m_pCoolFont->GetTextExtentsFormat(m_pClientDE->GetStringData(hString), (short)nWidth);

		return result;
	}

	if (!m_hFont)
		return result;

	int nLineWidth	 = 0;
	int nTotalHeight = 0;

	const char *pStart = m_pClientDE->GetStringData(hString);
	const char *pEnd   = pStart;

	while (*pStart)
	{
		if (s_bWrapUseSpaces)
		{
			while (*pEnd && *pEnd != ' ')
				pEnd = (const char *)_mbsinc((const unsigned char *)pEnd);

			if (*pEnd == ' ')
				pEnd = (const char *)_mbsinc((const unsigned char *)pEnd);
		}
		else
		{
			const char *szNoLineStart[10] = { ".", ")", "]", "\"", ",", "?", "!", ";", ":", "/" };

			pEnd = (const char *)_mbsinc((const unsigned char *)pEnd);

			for (;;)
			{
				const char *pNext = (const char *)_mbsinc((const unsigned char *)pEnd);

				int i;
				for (i = 0; i < 10; ++i)
				{
					if (*pNext == *szNoLineStart[i])
						break;
				}

				if (i >= 10)
					break;

				pEnd = (const char *)_mbsinc((const unsigned char *)pNext);

				if (!*pEnd)
					break;
			}
		}

		if (pEnd != pStart)
		{
			int nLen = (int)(pEnd - pStart);

			char *pWord = new char[nLen + 1];
			memcpy(pWord, pStart, nLen);
			pWord[nLen] = '\0';

			HSTRING hWord = m_pClientDE->CreateString(pWord);

			DIntPt extents = GetTextExtents(hWord);

			if (nLineWidth + extents.x > nWidth)
			{
				nLineWidth    = 0;
				nTotalHeight += GetHeight();
			}

			nLineWidth += extents.x;

			if (nLineWidth > result.x)
				result.x = nLineWidth;

			m_pClientDE->FreeString(hWord);
			delete [] pWord;
		}

		pStart = pEnd;
	}

	result.y = nTotalHeight + GetHeight();

	return result;
}

int CLTGUIFont::GetHeight()
{
	if (m_bBitmap && m_pCoolFont)
		return m_pCoolFont->GetHeight();

	if (!m_bBitmap && m_hFont)
		return m_nFontHeight;

	return 0;
}
