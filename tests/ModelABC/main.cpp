// Runs the engine's ABC -> LTB conversion over a LithTech 1.0 model and writes
// the image out for replay through Model::Load and a vertex-by-vertex diff

#include "bdefs.h"
#include "model_abc.h"
#include "iltstream.h"
#include "genltstream.h"

#ifdef main
#undef main
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void* dalloc(size_t size)   { return malloc(size); }
void* dalloc_z(size_t size) { void *p = malloc(size); if (p) memset(p, 0, size); return p; }
void  dfree(void *ptr)      { free(ptr); }

void dsi_OnReturnError(int err) {}

int32 g_bPaletteReport = 0;

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

	CFileStream(FILE *fp, uint32 len) : m_fp(fp), m_Len(len), m_Status(LT_OK) {}

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

	virtual LTRESULT Write(const void *, uint32) { return LT_ERROR; }
	virtual LTRESULT ErrorStatus() { return m_Status; }

	virtual LTRESULT SeekTo(uint32 offset)
	{
		return fseek(m_fp, (long)offset, SEEK_SET) == 0 ? LT_OK : LT_ERROR;
	}

	virtual LTRESULT GetPos(uint32 *pOff) { *pOff = (uint32)ftell(m_fp); return LT_OK; }
	virtual LTRESULT GetLen(uint32 *pLen) { *pLen = m_Len; return LT_OK; }

private:

	FILE     *m_fp;
	uint32    m_Len;
	LTRESULT  m_Status;
};


int main(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("Usage: testModelABC <input.abc> <output.ltb>\n");
		return 2;
	}

	FILE *fp = fopen(argv[1], "rb");
	if (!fp)
	{
		printf("FAIL: cannot open %s\n", argv[1]);
		return 1;
	}

	fseek(fp, 0, SEEK_END);
	uint32 len = (uint32)ftell(fp);
	fseek(fp, 0, SEEK_SET);

	CFileStream stream(fp, len);

	uint8 *pImage = NULL;
	uint32 nImageSize = 0;

	LTRESULT dResult = abc_BuildLTBImage(&stream, &pImage, &nImageSize);
	fclose(fp);

	if (dResult != LT_OK || !pImage)
	{
		printf("FAIL: abc_BuildLTBImage returned %d\n", (int)dResult);
		return 1;
	}

	FILE *out = fopen(argv[2], "wb");
	if (!out)
	{
		printf("FAIL: cannot write %s\n", argv[2]);
		delete[] pImage;
		return 1;
	}

	fwrite(pImage, 1, nImageSize, out);
	fclose(out);
	delete[] pImage;

	printf("OK %u bytes\n", nImageSize);
	return 0;
}
