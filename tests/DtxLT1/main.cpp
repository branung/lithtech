// Runs the engine's own dtx_Create over a LithTech 1.0 (version -2) 
// texture and dumps the decoded pixels for a companion script to diff

#include "bdefs.h"
#include "dtxmgr.h"
#include "genltstream.h"
#include "iltstream.h"

// bdefs.h redirects main, so this is needed fix that
#ifdef main
#undef main
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int32 g_CV_TextureMipMapOffset = 0;
int32 g_CV_TextureGroupOffset[4] = { 0, 0, 0, 0 };

// The LT1 fullbrite knobs, held at the engine's own defaults so this test
// exercises the path the game gets
int32 g_bLT1Fullbrite      = 2;
int32 g_nLT1FullbriteIndex = 246;
int32 g_bLT1PaletteAlpha   = 1;

// dalloc/dfree without linking runtime/kernel/mem, which is malloc here
void* dalloc(size_t size)   { return malloc(size); }
void* dalloc_z(size_t size) { void *p = malloc(size); if (p) memset(p, 0, size); return p; }
void  dfree(void *ptr)      { free(ptr); }

void dsi_OnReturnError(int err)
{
}

void dsi_PrintToConsole(const char *pMsg, ...)
{
	va_list args;
	va_start(args, pMsg);
	vprintf(pMsg, args);
	va_end(args);
	printf("\n");
}


class CFileStream : public CGenLTStream
{
public:

	CFileStream(FILE *fp, uint32 len)
	{
		m_fp     = fp;
		m_Len    = len;
		m_Status = LT_OK;
	}

	virtual void Release() {}

	virtual LTRESULT Read(void *pData, uint32 size)
	{
		if (fread(pData, 1, size, m_fp) != size)
		{
			memset(pData, 0, size);
			m_Status = LT_ERROR;
			return LT_ERROR;
		}
		return LT_OK;
	}

	virtual LTRESULT Write(const void *pData, uint32 size) { return LT_ERROR; }
	virtual LTRESULT ErrorStatus() { return m_Status; }

	virtual LTRESULT SeekTo(uint32 offset)
	{
		return fseek(m_fp, (long)offset, SEEK_SET) == 0 ? LT_OK : LT_ERROR;
	}

	virtual LTRESULT GetPos(uint32 *offset)
	{
		*offset = (uint32)ftell(m_fp);
		return LT_OK;
	}

	virtual LTRESULT GetLen(uint32 *len)
	{
		*len = m_Len;
		return LT_OK;
	}

private:

	FILE     *m_fp;
	uint32    m_Len;
	LTRESULT  m_Status;
};


int main(int argc, char **argv)
{
	if (argc < 3)
	{
		return 2;
	}

	FILE *fp = fopen(argv[1], "rb");
	if (!fp)
	{
		printf("FAIL: cannot open %s\n", argv[1]);
		return 1;
	}

	fseek(fp, 0, SEEK_END);
	uint32 fileLen = (uint32)ftell(fp);
	fseek(fp, 0, SEEK_SET);

	CFileStream stream(fp, fileLen);

	TextureData *pTexture = NULL;
	uint32 baseWidth = 0, baseHeight = 0;

	LTRESULT dResult = dtx_Create(&stream, &pTexture, baseWidth, baseHeight);
	fclose(fp);

	if (dResult != LT_OK || !pTexture)
	{
		printf("FAIL: dtx_Create returned %d\n", (int)dResult);
		return 1;
	}

	printf("version   = %d\n", (int)pTexture->m_Header.m_Version);
	printf("base      = %ux%u\n", baseWidth, baseHeight);
	printf("mipmaps   = %u\n", (uint32)pTexture->m_Header.m_nMipmaps);
	printf("bpp ident = %d\n", (int)pTexture->m_Header.GetBPPIdent());
	printf("iflags    = 0x%x\n", (unsigned)pTexture->m_Header.m_IFlags);
	printf("userflags = 0x%x\n", (unsigned)pTexture->m_Header.m_UserFlags);

	// The command string is where a v-5 texture carries "AlphaRef <n>", the
	// only channel that turns on alpha testing. An LT1 texture has it empty.
	printf("cmdstring = '%s'\n", pTexture->m_Header.m_CommandString);

	TextureMipData *pMip = &pTexture->m_Mips[0];
	printf("mip0      = %ux%u pitch=%d size=%u\n",
	       pMip->m_Width, pMip->m_Height, (int)pMip->m_Pitch, pMip->m_dataSize);

	FILE *out = fopen(argv[2], "wb");
	if (!out)
	{
		printf("FAIL: cannot write %s\n", argv[2]);
		dtx_Destroy(pTexture);
		return 1;
	}

	for (uint32 y = 0; y < pMip->m_Height; y++)
	{
		fwrite(pMip->m_Data + (y * pMip->m_Pitch), 1, pMip->m_Width * sizeof(uint32), out);
	}

	fclose(out);
	dtx_Destroy(pTexture);

	printf("OK\n");
	return 0;
}
