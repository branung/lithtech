// Engine important LithTech 1.0 world loader elements

#include "bdefs.h"

#include "world_v56.h"
#include "genltstream.h"
#include "iltstream.h"

#include <string.h>
#include <stdio.h>

#include <string>
#include <vector>


// A read only stream over the converted image, which it also owns.
// Release() frees the object as well as the buffer.
class CWorldV56Stream : public CGenLTStream
{
public:

	CWorldV56Stream(uint8 *pData, uint32 nLen) : m_pData(pData), m_nLen(nLen), m_nPos(0), m_Status(LT_OK) {}

	virtual void Release()
	{
		delete this;
	}

	virtual LTRESULT Read(void *pOut, uint32 size)
	{
		if (!m_pData || m_nPos + size > m_nLen || m_nPos + size < m_nPos)
		{
			memset(pOut, 0, size);
			m_Status = LT_ERROR;
			return LT_ERROR;
		}
		memcpy(pOut, m_pData + m_nPos, size);
		m_nPos += size;
		return LT_OK;
	}

	virtual LTRESULT Write(const void *, uint32) { return LT_ERROR; }
	virtual LTRESULT ErrorStatus() { return m_Status; }
	virtual LTRESULT GetPos(uint32 *pPos) { *pPos = m_nPos; return LT_OK; }
	virtual LTRESULT GetLen(uint32 *pLen) { *pLen = m_nLen; return LT_OK; }

	virtual LTRESULT SeekTo(uint32 offset)
	{
		if (offset > m_nLen)
		{
			m_Status = LT_ERROR;
			return LT_ERROR;
		}
		m_nPos = offset;
		return LT_OK;
	}

private:

	virtual ~CWorldV56Stream() { delete[] m_pData; }

	uint8 *m_pData;
	uint32 m_nLen;
	uint32 m_nPos;
	LTRESULT m_Status;
};


// A converted level that needs render-block splitting says so on the game console
static void V56Warn(const char *pszMessage, void * /*pUser*/)
{
	dsi_ConsolePrint("%s", pszMessage);
}


// Names already warned about during the current conversion.
// GetTexDims is called once per polygon, so each warning would otherwise print hundreds of times.
static std::vector<std::string> g_V56WarnedTextures;

static void V56WarnOnce(const char *pszName, const char *pszWhat)
{
	for (size_t i = 0; i < g_V56WarnedTextures.size(); ++i)
	{
		if (stricmp(g_V56WarnedTextures[i].c_str(), pszName) == 0) return;
	}
	g_V56WarnedTextures.push_back(pszName);

	char szMsg[512];
	snprintf(szMsg, sizeof(szMsg), "world_v56: %s for '%s' - surface UVs fall back to 128x128, so it "
			 "will be drawn at the wrong scale", pszWhat, pszName);
	V56Warn(szMsg, LTNULL);
}


// An LT1 world surface may name an animated sprite instead of a texture, returns frame 0's texture name.
// Jupiter normalises UVs once, so one frame has to be chosen and frame 0 is what LT1 binds at world load.
static bool V56SpriteFrame0(const char *pszSprite, char *pszOut, size_t nOut, WorldV56OpenFn pOpen)
{
	ILTStream *pStream = pOpen(pszSprite);
	if (!pStream) return false;

	uint32 nHead[5];
	uint16 nNameLen = 0;
	bool bOk = (pStream->Read(nHead, sizeof(nHead)) == LT_OK) && nHead[0] != 0;
	if (bOk) bOk = (pStream->Read(&nNameLen, sizeof(nNameLen)) == LT_OK);
	if (bOk) bOk = (nNameLen > 0 && (size_t)nNameLen < nOut);
	if (bOk) bOk = (pStream->Read(pszOut, nNameLen) == LT_OK);
	pStream->Release();

	if (!bOk) return false;
	pszOut[nNameLen] = '\0';
	return true;
}


// v56 stores surface texture axes in texel units, so the UV normalisation needs each texture's real size.
// A miss falls back to 128x128.
static bool V56TexDims(const char *pszName, uint32 *pWidth, uint32 *pHeight, void *pUser)
{
	WorldV56OpenFn pOpen = (WorldV56OpenFn)pUser;
	if (!pOpen || !pszName || !pszName[0]) return false;

	// A sprite already carries .spr, and appending .dtx to it finds nothing
	char szPath[256];
	LTStrCpy(szPath, pszName, sizeof(szPath));

	size_t nLen = strlen(szPath);
	if (nLen >= 4 && stricmp(szPath + nLen - 4, ".spr") == 0)
	{
		char szFrame[256];
		if (!V56SpriteFrame0(szPath, szFrame, sizeof(szFrame), pOpen))
		{
			V56WarnOnce(szPath, "cannot read sprite");
			return false;
		}
		LTStrCpy(szPath, szFrame, sizeof(szPath));
		nLen = strlen(szPath);
	}

	if (nLen < 4 || stricmp(szPath + nLen - 4, ".dtx") != 0)
		LTStrCat(szPath, ".dtx", sizeof(szPath));

	ILTStream *pStream = pOpen(szPath);
	if (!pStream)
	{
		V56WarnOnce(szPath, "no such texture");
		return false;
	}

	// DTX header: int32 resource type, int32 version, uint16 width, uint16 height
	struct { int32 m_nType; int32 m_nVersion; uint16 m_nWidth; uint16 m_nHeight; } head;
	bool bOk = (pStream->Read(&head, sizeof(head)) == LT_OK);
	pStream->Release();

	if (!bOk || !head.m_nWidth || !head.m_nHeight)
	{
		V56WarnOnce(szPath, "unreadable texture header");
		return false;
	}

	*pWidth = head.m_nWidth;
	*pHeight = head.m_nHeight;
	return true;
}


ILTStream *world_v56_WrapStream(ILTStream *pSrc, WorldV56OpenFn pOpen)
{
	if (!pSrc) return LTNULL;

	g_V56WarnedTextures.clear();

	uint32 nLen = 0;
	if (pSrc->GetLen(&nLen) != LT_OK || nLen < 4) return LTNULL;

	// Peek the version before reading the whole file.
	// Anything but 56 returns LTNULL and the engine's loader reads it directly.
	uint32 nVersion = 0;
	if (pSrc->SeekTo(0) != LT_OK) return LTNULL;
	if (pSrc->Read(&nVersion, 4) != LT_OK) return LTNULL;

	if (nVersion != 56)
	{
		pSrc->SeekTo(0);
		return LTNULL;
	}

	uint8 *pRaw = new uint8[nLen];
	if (!pRaw) return LTNULL;

	bool bRead = (pSrc->SeekTo(0) == LT_OK) && (pSrc->Read(pRaw, nLen) == LT_OK);
	pSrc->SeekTo(0);
	if (!bRead)
	{
		delete[] pRaw;
		return LTNULL;
	}

	uint8 *pImage = LTNULL;
	uint32 nImageSize = 0;
	LTRESULT nResult = world_v56_BuildV85Image(pRaw, nLen, &pImage, &nImageSize, pOpen ? V56TexDims : LTNULL, (void *)pOpen, V56Warn, LTNULL);
	delete[] pRaw;

	if (nResult != LT_OK || !pImage)
	{
		dsi_ConsolePrint("world_v56: conversion of a v%u world failed (%d)", (unsigned)nVersion, (int)nResult);
		return LTNULL;
	}

	dsi_ConsolePrint("world_v56: converted a v%u world (%u -> %u bytes)", (unsigned)nVersion, (unsigned)nLen, (unsigned)nImageSize);

	return new CWorldV56Stream(pImage, nImageSize);
}
