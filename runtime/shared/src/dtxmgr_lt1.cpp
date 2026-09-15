#include "bdefs.h"
#include "dtxmgr.h"
#include "dtxmgr_lt1.h"
#include "genltstream.h"
#include "iltstream.h"

// This expands a v-2 file into the v-5 image dtx_Create already consumes and hands it back through a memory stream, 
// so everything downstream runs unchanged.

class CDtxMemStream : public CGenLTStream
{
public:

	CDtxMemStream(const uint8 *pData, uint32 len)
	{
		m_pData  = pData;
		m_Len    = len;
		m_Pos    = 0;
		m_Status = LT_OK;
	}

	virtual void Release()
	{
		// The buffer belongs to dtx_CreateFromLT1, which frees 
		// it once dtx_Create has finished with it
	}

	virtual LTRESULT Read(void *pData, uint32 size)
	{
		if (m_Pos + size > m_Len)
		{
			// ILTStream's contract: zero-fill on overrun so callers can read
			// straight through and check ErrorStatus() once at the end.
			memset(pData, 0, size);
			m_Status = LT_ERROR;
			return LT_ERROR;
		}

		memcpy(pData, m_pData + m_Pos, size);
		m_Pos += size;
		return LT_OK;
	}

	virtual LTRESULT Write(const void *pData, uint32 size)
	{
		return LT_ERROR;
	}

	virtual LTRESULT ErrorStatus()      { return m_Status; }
	virtual LTRESULT GetPos(uint32 *pos) { *pos = m_Pos; return LT_OK; }
	virtual LTRESULT GetLen(uint32 *len) { *len = m_Len; return LT_OK; }

	virtual LTRESULT SeekTo(uint32 offset)
	{
		if (offset > m_Len)
		{
			m_Status = LT_ERROR;
			return LT_ERROR;
		}

		m_Pos = offset;
		return LT_OK;
	}

private:

	const uint8 *m_pData;
	uint32       m_Len;
	uint32       m_Pos;
	LTRESULT     m_Status;
};

/*
 LT1 fullbrite translation.
 Jupiter drives its fullbright pass per pixel from the texture's alpha
 channel, and a palettized LT1 DTX has no alpha channel. 
 LT1 marks fullbrite texels by palette index instead, in a hard-coded range at the top of the
 palette, 246-255, applied to every palettized texture. 
 Bit 0 only chooses an alpha capable format and sets the flag the draw code tests.
*/
extern int32 g_bLT1Fullbrite;

// First palette index of LT1's fullbrite range, 246
extern int32 g_nLT1FullbriteIndex;

// Give every palettised texture LT1's alpha, 1 on palette indices 246-255, 0 elsewhere
extern int32 g_bLT1PaletteAlpha;



// Helpers

// Total 8-bit pixels across a mip pyramid. Does not clamp the shifted
// dimensions to 1, since dtx_Alloc and CalcImageSize don't either.
static uint32 lt1_PyramidPixels(uint32 width, uint32 height, uint32 nMipmaps)
{
	uint32 total = 0;

	for (uint32 i = 0; i < nMipmaps; i++)
	{
		total += (width >> i) * (height >> i);
	}

	return total;
}


LTRESULT dtx_CreateFromLT1(ILTStream *pStream, TextureData **ppOut, uint32 &nBaseWidth, uint32 &nBaseHeight)
{
	*ppOut = LTNULL;

	if (pStream->SeekTo(0) != LT_OK)
		RETURN_ERROR(1, dtx_CreateFromLT1, LT_INVALIDDATA);

	// Read the 44-byte header a field at a time, so the layout does not depend on struct alignment
	uint32	resType;
	int32	version;
	uint16	baseWidth, baseHeight, nMipmaps, nSections;
	int32	iFlags, userFlags;
	uint8	extra[20];

	STREAM_READ(resType);
	STREAM_READ(version);
	STREAM_READ(baseWidth);
	STREAM_READ(baseHeight);
	STREAM_READ(nMipmaps);
	STREAM_READ(nSections);
	STREAM_READ(iFlags);
	STREAM_READ(userFlags);
	pStream->Read(extra, sizeof(extra));

	if (pStream->ErrorStatus() != LT_OK)
		RETURN_ERROR(1, dtx_CreateFromLT1, LT_INVALIDDATA);

	if (resType != LT_RESTYPE_DTX || version != LT1_DTX_VERSION)
		RETURN_ERROR(1, dtx_CreateFromLT1, LT_INVALIDDATA);

	if (baseWidth == 0 || baseHeight == 0 ||
		baseWidth > MAX_DTX_SIZE || baseHeight > MAX_DTX_SIZE)
		RETURN_ERROR(1, dtx_CreateFromLT1, LT_INVALIDDATA);

	if (nMipmaps == 0 || nMipmaps > MAX_DTX_MIPMAPS)
		RETURN_ERROR(1, dtx_CreateFromLT1, LT_INVALIDDATA);

	// Drop any trailing mipmap that would shift a dimension away to nothing
	while (nMipmaps > 1 &&
		((baseWidth >> (nMipmaps - 1)) == 0 || (baseHeight >> (nMipmaps - 1)) == 0))
	{
		nMipmaps--;
	}

	const uint32 nPixels = lt1_PyramidPixels(baseWidth, baseHeight, nMipmaps);
	const bool bHasAlphaMask = (iFlags & LT1_DTX_ALPHAMASK) != 0;

	// Palette: 256 entries stored A,R,G,B, folded into the 0xAARRGGBB form dtx_Create expects
	uint8 palette[256 * 4];
	pStream->Read(palette, sizeof(palette));

	if (pStream->ErrorStatus() != LT_OK)
		RETURN_ERROR(1, dtx_CreateFromLT1, LT_INVALIDDATA);

	uint32 lut[256];
	for (uint32 i = 0; i < 256; i++)
	{
		const uint8 *pEntry = &palette[i * 4];
		lut[i] = 0xFF000000 |
				 ((uint32)pEntry[1] << 16) |
				 ((uint32)pEntry[2] << 8) |
				 ((uint32)pEntry[3]);
	}

	uint8 *pIndices = LTNULL;
	LT_MEM_TRACK_ALLOC(pIndices = new uint8[nPixels], LT_MEM_TYPE_TEXTURE);
	if (!pIndices)
		RETURN_ERROR(1, dtx_CreateFromLT1, LT_OUTOFMEMORY);

	pStream->Read(pIndices, nPixels);

	// The alpha mask, if there is one nibble at it
	uint8 *pAlphaNibbles = LTNULL;
	if (bHasAlphaMask)
	{
		const uint32 nAlphaBytes = (nPixels + 1) / 2;

		LT_MEM_TRACK_ALLOC(pAlphaNibbles = new uint8[nAlphaBytes], LT_MEM_TYPE_TEXTURE);
		if (!pAlphaNibbles)
		{
			delete[] pIndices;
			RETURN_ERROR(1, dtx_CreateFromLT1, LT_OUTOFMEMORY);
		}

		pStream->Read(pAlphaNibbles, nAlphaBytes);
	}

	if (pStream->ErrorStatus() != LT_OK)
	{
		delete[] pIndices;
		delete[] pAlphaNibbles;
		RETURN_ERROR(1, dtx_CreateFromLT1, LT_INVALIDDATA);
	}

	// Build the equivalent v-5 image
	const uint32 imageSize = sizeof(DtxHeader) + nPixels * sizeof(uint32);

	uint8 *pImage = LTNULL;
	LT_MEM_TRACK_ALLOC(pImage = new uint8[imageSize], LT_MEM_TYPE_TEXTURE);
	if (!pImage)
	{
		delete[] pIndices;
		delete[] pAlphaNibbles;
		RETURN_ERROR(1, dtx_CreateFromLT1, LT_OUTOFMEMORY);
	}

	DtxHeader hdr;
	memset(&hdr, 0, sizeof(hdr));

	hdr.m_ResType    = LT_RESTYPE_DTX;
	hdr.m_Version    = CURRENT_DTX_VERSION;
	hdr.m_BaseWidth  = baseWidth;
	hdr.m_BaseHeight = baseHeight;
	hdr.m_nMipmaps   = nMipmaps;
	// Always zero, never the value read off disk
	hdr.m_nSections  = 0;

	// Rebuild the flags by meaning. Only DTX_FULLBRITE survives.
	// LT1's bit 1 reads as DTX_PREFER16BIT in Jupiter and bit 3 is its sections validation flag.
	// DTX_SECTIONSFIXED is set since this image declares its count.
	const bool bFullbrite = (iFlags & LT1_DTX_FULLBRITE) != 0 && g_bLT1Fullbrite != 0;

	hdr.m_IFlags = DTX_SECTIONSFIXED;
	if (bFullbrite)
		hdr.m_IFlags |= DTX_FULLBRITE;

	// Surface flags are game-side meaning, not engine format, so they pass through untouched.
	hdr.m_UserFlags = userFlags;

	hdr.m_Extra[0] = extra[0]; // texture group
	hdr.m_Extra[1] = (uint8)nMipmaps; // mipmaps to use at runtime
	hdr.SetBPPIdent(BPP_32);

	memcpy(pImage, &hdr, sizeof(hdr));

	// Expand indices through the palette, applying the mask as alpha.
	uint32 *pOut = (uint32*)(pImage + sizeof(DtxHeader));

	const bool bIndexAlpha =
		(g_bLT1PaletteAlpha != 0) || (bFullbrite && g_bLT1Fullbrite == 2);
	const uint32 nFirst =
		bIndexAlpha
			? ((g_nLT1FullbriteIndex < 0) ? 0
			 : ((g_nLT1FullbriteIndex > 256) ? 256 : (uint32)g_nLT1FullbriteIndex))
			: 256;

	if (bHasAlphaMask)
	{
		for (uint32 i = 0; i < nPixels; i++)
		{
			const uint8 packed = pAlphaNibbles[i >> 1];
			const uint8 nibble = (i & 1) ? (uint8)(packed >> 4) : (uint8)(packed & 0x0F);

			uint32 alpha = (uint32)nibble * 17;

			if (pIndices[i] >= nFirst)
				alpha = 255;

			pOut[i] = (lut[pIndices[i]] & 0x00FFFFFF) | (alpha << 24);
		}
	}
	else if (nFirst < 256)
	{
		for (uint32 i = 0; i < nPixels; i++)
		{
			const uint32 alpha = (pIndices[i] >= nFirst) ? 0xFF000000 : 0;
			pOut[i] = (lut[pIndices[i]] & 0x00FFFFFF) | alpha;
		}
	}
	else
	{
		for (uint32 i = 0; i < nPixels; i++)
		{
			pOut[i] = lut[pIndices[i]];
		}
	}

	delete[] pIndices;
	delete[] pAlphaNibbles;

	// Hand off to the regular loader.
	CDtxMemStream memStream(pImage, imageSize);
	LTRESULT dResult = dtx_Create(&memStream, ppOut, nBaseWidth, nBaseHeight);

	delete[] pImage;

	return dResult;
}
