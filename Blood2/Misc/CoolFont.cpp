//*****************************************************************//
//*****		Project:	Cool Font System
//*****		Author:		Andy Mattingly
//*****		Date:		April 8, 1998
//*****************************************************************//
//*****		File:		CoolFont.cpp
//*****************************************************************//

#include "CoolFont.h"
#include "dstream.h"

#include <string.h>


//***** Constructors *************************************************//

CoolFont::CoolFont()
{
	height     = 0;
	client     = 0;
	font       = 0;
	transColor = 0;
	effects    = 0;

	rect.left = rect.top = rect.right = rect.bottom = 0;

	memset(XLocs,   0, sizeof(XLocs));
	memset(XWidths, 0, sizeof(XWidths));
}


//***** Memory allocation and deallocation ***************************//

char CoolFont::Init(CClientDE *pClient, char *szFile)
{
	client     = pClient;
	font       = client->CreateSurfaceFromBitmap(szFile);
	transColor = client->SetupColor1(1.0f, 0.0f, 1.0f, DFALSE);

	return 1;
}

void CoolFont::Free()
{
	if (font)
	{
		client->DeleteSurface(font);
		font = 0;
	}

	client = 0;
}


//***** Data loading *************************************************//

char CoolFont::LoadXWidths(char *szFontCfg)
{
	DStream *pStream = 0;

	client->OpenFile(szFontCfg, &pStream);

	if (pStream)
	{
		pStream->Read(&height,  sizeof(height));
		pStream->Read(XWidths,  sizeof(XWidths));
		pStream->Release();
	}

	CalcXLocs();
	rect.bottom = height;

	return 1;
}


//***** Measurement **************************************************//

DIntPt CoolFont::GetTextExtents(const char *szString)
{
	DIntPt	result;
	int		nWidth = 0;
	int		i      = 0;

	while (szString[i])
	{
		signed char c = (signed char)(szString[i] - 0x21);
		++i;

		if (c < -1 || c > 0x5D)
			continue;

		if (c == -1)
			nWidth += height / 2;
		else
			nWidth += XWidths[c];
	}

	result.x = nWidth;
	result.y = height;

	return result;
}


//***** Drawing ******************************************************//

void CoolFont::Draw(const char *szString, HSURFACE hDest, short x, short y, short justify)
{
	int	nWidth = 0;
	int	i      = 0;

	if (justify != 0 && szString[0])
	{
		while (szString[i])
		{
			signed char c = (signed char)(szString[i] - 0x21);
			++i;

			if (c < -1 || c > 0x5D)
				continue;

			nWidth += (c == -1) ? (height / 2) : XWidths[c];
		}
	}

	int nX = x;
	if (justify == CF_JUSTIFY_CENTER)
		nX = x - nWidth / 2;
	else if (justify == CF_JUSTIFY_RIGHT)
		nX = x - nWidth;

	i = 0;
	while (szString[i])
	{
		signed char c = (signed char)(szString[i] - 0x21);
		++i;

		if (c < -1 || c > 0x5D)
			continue;

		if (c == -1)
		{
			nX += height / 2;
			continue;
		}

		rect.left  = XLocs[c];
		rect.right = XLocs[c + 1];

		client->DrawSurfaceToSurfaceTransparent(hDest, font, &rect, nX, y, transColor);

		nX += XWidths[c];
	}
}

void CoolFont::DrawSolid(const char *szString, HSURFACE hDest, short x, short y,
						 short justify, HDECOLOR color)
{
	int	nWidth = 0;
	int	i      = 0;

	if (justify != 0 && szString[0])
	{
		while (szString[i])
		{
			signed char c = (signed char)(szString[i] - 0x21);
			++i;

			if (c < -1 || c > 0x5D)
				continue;

			nWidth += (c == -1) ? (height / 2) : XWidths[c];
		}
	}

	int nX = x;
	if (justify == CF_JUSTIFY_CENTER)
		nX = x - nWidth / 2;
	else if (justify == CF_JUSTIFY_RIGHT)
		nX = x - nWidth;

	i = 0;
	while (szString[i])
	{
		signed char c = (signed char)(szString[i] - 0x21);
		++i;

		if (c < -1 || c > 0x5D)
			continue;

		if (c == -1)
		{
			nX += height / 2;
			continue;
		}

		rect.left  = XLocs[c];
		rect.right = XLocs[c + 1];

		client->DrawSurfaceSolidColor(hDest, font, &rect, nX, y, transColor, color);

		nX += XWidths[c];
	}
}

// The nBreaks < 25 bound isn't in the original, which overran its 25 entry stack buffer on long text
void CoolFont::DrawFormat(const char *szString, HSURFACE hDest, short x, short y, short width)
{
	short	breaks[25];
	int		nBreaks   = 0;
	int		nLastSpace = 0;
	int		nWidth    = 0;
	int		i         = 0;

	memset(breaks, 0, sizeof(breaks));

	if (width != 0 && szString[0])
	{
		while (szString[i])
		{
			signed char c = (signed char)(szString[i] - 0x21);
			++i;

			if (c < -1 || c > 0x5D)
				continue;

			if (c == -1)					// space
			{
				nLastSpace = i - 1;
				nWidth += height / 2;
			}
			else if (c == 0x5B)				// '|', a line break that resets the width
			{
				nWidth = 0;
			}
			else if (c == 0x5D)				// '~', a two line gap
			{
				nWidth += height * 2;
			}
			else
			{
				nWidth += XWidths[c];
			}

			if (nWidth > width)
			{
				if (nBreaks < (int)(sizeof(breaks) / sizeof(breaks[0])))
					breaks[nBreaks] = (short)nLastSpace;
				++nBreaks;
				i      = nLastSpace + 1;
				nWidth = 0;
			}
		}
	}

	int nX       = x;
	int nY       = y;
	int nBreakAt = 0;

	i = 0;
	while (szString[i])
	{
		signed char c = (signed char)(szString[i] - 0x21);
		++i;

		if (c < -1 || c > 0x5D)
			continue;

		if (width != 0 && breaks[nBreakAt] != 0 && breaks[nBreakAt] == (short)(i - 1))
		{
			nX = x;
			nY += height;
			++nBreakAt;
			continue;
		}

		if (c == -1)					// space
		{
			nX += height / 2;
			continue;
		}

		if (c == 0x5B)					// '|', a line break
		{
			nY += height;
			nX  = x;
			continue;
		}

		if (c == 0x5D)					// '~', a two line gap
		{
			nX += height * 2;
			continue;
		}

		rect.left  = XLocs[c];
		rect.right = XLocs[c + 1];

		client->DrawSurfaceToSurfaceTransparent(hDest, font, &rect, nX, nY, transColor);

		nX += XWidths[c];
	}
}

// As in the original, both halves draw character N-1, and a ratio over 1 reads past the string
void CoolFont::DrawFormatTimed(const char *szString, HSURFACE hDest, short x, short y,
							   short width, DFLOAT ratio, DBOOL fromStart)
{
	short	breaks[25];
	int		nBreaks    = 0;
	int		nLastSpace = 0;
	int		nWidth     = 0;
	int		i          = 0;

	memset(breaks, 0, sizeof(breaks));

	if (width != 0 && szString[0])
	{
		while (szString[i])
		{
			signed char c = (signed char)(szString[i] - 0x21);
			++i;

			if (c < -1 || c > 0x5D)
				continue;

			if (c == -1)					// space
			{
				nLastSpace = i - 1;
				nWidth += height / 2;
			}
			else if (c == 0x5B)				// '|', a line break that resets the width
			{
				nWidth = 0;
			}
			else if (c == 0x5D)				// '~', a two line gap
			{
				nWidth += height * 2;
			}
			else
			{
				nWidth += XWidths[c];
			}

			if (nWidth > width)
			{
				if (nBreaks < (int)(sizeof(breaks) / sizeof(breaks[0])))
					breaks[nBreaks] = (short)nLastSpace;
				++nBreaks;
				i      = nLastSpace + 1;
				nWidth = 0;
			}
		}
	}

	short	nLen  = (short)strlen(szString);
	short	nSplit = (short)((float)nLen * ratio);

	int		nX       = x;
	int		nY       = y;
	int		nBreakAt = 0;

	if (fromStart)
	{
		if (nSplit <= 0)
			return;

		i = 0;

		for (short k = 0; k < nSplit; ++k)
		{
			signed char c = (signed char)(szString[i] - 0x21);
			++i;

			if (c < -1 || c > 0x5D)
				continue;

			if (width != 0 && breaks[nBreakAt] != 0 && breaks[nBreakAt] == (short)(i - 1))
			{
				nX = x;
				nY += height;
				++nBreakAt;
				continue;
			}

			if (c == -1)					// space
			{
				nX += height / 2;
				continue;
			}

			if (c == 0x5B)					// '|', a line break
			{
				nY += height;
				nX  = x;
				continue;
			}

			if (c == 0x5D)					// '~', a two line gap
			{
				nX += height * 2;
				continue;
			}

			rect.left  = XLocs[c];
			rect.right = XLocs[c + 1];

			client->DrawSurfaceToSurfaceTransparent(hDest, font, &rect, nX, nY, transColor);

			nX += XWidths[c];
		}
	}
	else
	{
		if (nLen <= 0)
			return;

		for (i = 0; i < nLen; ++i)
		{
			signed char c = (signed char)(szString[i] - 0x21);

			if (c < -1 || c > 0x5D)
				continue;

			if (width != 0 && breaks[nBreakAt] != 0 && breaks[nBreakAt] == (short)i)
			{
				nX = x;
				nY += height;
				++nBreakAt;
				continue;
			}

			if (c == -1)					// space
			{
				nX += height / 2;
				continue;
			}

			if (c == 0x5B)					// '|', a line break
			{
				nY += height;
				nX  = x;
				continue;
			}

			if (c == 0x5D)					// '~', a two line gap
			{
				nX += height * 2;
				continue;
			}

			rect.left  = XLocs[c];
			rect.right = XLocs[c + 1];

			if ((short)(i + 1) >= nSplit)
			{
				client->DrawSurfaceToSurfaceTransparent(hDest, font, &rect, nX, nY, transColor);
			}

			nX += XWidths[c];
		}
	}
}


//***** Metric calculation *******************************************//

char CoolFont::CalcXLocs()
{
	short	nSum = 0;

	XLocs[0] = 0;

	for (int k = 0; k < 94; ++k)
	{
		nSum = (short)(nSum + XWidths[k]);
		XLocs[k + 1] = nSum;
	}

	return 1;
}


//***** CoolFontCursor ***********************************************//

// As in the original, font and dest aren't cleared
CoolFontCursor::CoolFontCursor()
{
	x         = 0;
	y         = 0;
	justify   = 0;

	sourceC   = 0;
	tempC     = 0;
	destC     = 0;

	startTime = 0.0f;
	totalTime = 0.0f;
}


void CoolFont::DrawSolidFormat(const char *szString, HSURFACE hDest, short x, short y,
							   short width, HDECOLOR color)
{
	short	breaks[25];
	int		nBreaks    = 0;
	int		nLastSpace = 0;
	int		nWidth     = 0;
	int		i          = 0;

	memset(breaks, 0, sizeof(breaks));

	if (width != 0 && szString[0])
	{
		while (szString[i])
		{
			signed char c = (signed char)(szString[i] - 0x21);
			++i;

			if (c < -1 || c > 0x5D)
				continue;

			if (c == -1)					// space
			{
				nLastSpace = i - 1;
				nWidth += height / 2;
			}
			else if (c == 0x5B)				// '|', a line break that resets the width
			{
				nWidth = 0;
			}
			else if (c == 0x5D)				// '~', a two line gap
			{
				nWidth += height * 2;
			}
			else
			{
				nWidth += XWidths[c];
			}

			if (nWidth > width)
			{
				if (nBreaks < (int)(sizeof(breaks) / sizeof(breaks[0])))
					breaks[nBreaks] = (short)nLastSpace;
				++nBreaks;
				i      = nLastSpace + 1;
				nWidth = 0;
			}
		}
	}

	int nX       = x;
	int nY       = y;
	int nBreakAt = 0;

	i = 0;
	while (szString[i])
	{
		signed char c = (signed char)(szString[i] - 0x21);
		++i;

		if (c < -1 || c > 0x5D)
			continue;

		if (width != 0 && breaks[nBreakAt] != 0 && breaks[nBreakAt] == (short)(i - 1))
		{
			nX = x;
			nY += height;
			++nBreakAt;
			continue;
		}

		if (c == -1)					// space
		{
			nX += height / 2;
			continue;
		}

		if (c == 0x5B)					// '|', a line break
		{
			nY += height;
			nX  = x;
			continue;
		}

		if (c == 0x5D)					// '~', a two line gap
		{
			nX += height * 2;
			continue;
		}

		rect.left  = XLocs[c];
		rect.right = XLocs[c + 1];

		client->DrawSurfaceSolidColor(hDest, font, &rect, nX, nY, transColor, color);

		nX += XWidths[c];
	}
}

DIntPt CoolFont::GetTextExtentsFormat(const char *szString, short width)
{
	short	breaks[25];
	int		nBreaks    = 0;
	int		nLastSpace = 0;
	int		nWidth     = 0;
	int		i          = 0;

	memset(breaks, 0, sizeof(breaks));

	if (width != 0 && szString[0])
	{
		while (szString[i])
		{
			signed char c = (signed char)(szString[i] - 0x21);
			++i;

			if (c < -1 || c > 0x5D)
				continue;

			if (c == -1)
			{
				nLastSpace = i - 1;
				nWidth += height / 2;
			}
			else if (c == 0x5B)
			{
				nWidth = 0;
			}
			else if (c == 0x5D)
			{
				nWidth += height * 2;
			}
			else
			{
				nWidth += XWidths[c];
			}

			if (nWidth > width)
			{
				if (nBreaks < (int)(sizeof(breaks) / sizeof(breaks[0])))
					breaks[nBreaks] = (short)nLastSpace;
				++nBreaks;
				i      = nLastSpace + 1;
				nWidth = 0;
			}
		}
	}

	int nX       = 0;
	int nY       = 0;
	int nMaxX    = 0;
	int nMaxY    = 0;
	int nBreakAt = 0;

	i = 0;
	while (szString[i])
	{
		signed char c = (signed char)(szString[i] - 0x21);
		++i;

		if (c < -1 || c > 0x5D)
			continue;

		if (width != 0 && breaks[nBreakAt] != 0 && breaks[nBreakAt] == (short)(i - 1))
		{
			nX = 0;
			nY += height;
			++nBreakAt;
			continue;
		}

		if (c == -1)
		{
			nX += height / 2;
			continue;
		}

		if (c == 0x5B)
		{
			nY += height;
			nX  = 0;
			continue;
		}

		if (c == 0x5D)
		{
			nX += height * 2;
			continue;
		}

		rect.left  = XLocs[c];
		rect.right = XLocs[c + 1];

		int nRight = (short)nX + (rect.right - rect.left);
		if (nRight > nMaxX)
			nMaxX = nRight;

		int nBottom = (short)nY + (rect.bottom - rect.top);
		if (nBottom > nMaxY)
			nMaxY = nBottom;

		nX += XWidths[c];
	}

	DIntPt	result;
	result.x = nMaxX;
	result.y = nMaxY;

	return result;
}

