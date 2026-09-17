// Converts a LithTech 1.0 world with the engine's v56 loader
// Usage: testWorldV56 --check <world.dat> [more worlds...]
//        testWorldV56 <world.dat> <out.dat> [textures.txt] [max-block-verts]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "world_v56.h"

static std::map<std::string, std::pair<uint32, uint32> > g_TexDims;

static std::string UpperOf(const char *p)
{
	std::string s(p ? p : "");
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] >= 'a' && s[i] <= 'z') s[i] = (char)(s[i] - 'a' + 'A');
		if (s[i] == '/') s[i] = '\\';
	}
	return s;
}

static bool TexDims(const char *pszName, uint32 *pW, uint32 *pH, void * /*pUser*/)
{
	std::string key = UpperOf(pszName);

	// A .SPR is looked up under its own name, since the dimension list records sprites by name
	const bool bSprite = (key.size() >= 4 && key.compare(key.size() - 4, 4, ".SPR") == 0);
	if (!bSprite && (key.size() < 4 || key.compare(key.size() - 4, 4, ".DTX") != 0)) key += ".DTX";

	std::map<std::string, std::pair<uint32, uint32> >::const_iterator it = g_TexDims.find(key);
	if (it == g_TexDims.end()) return false;

	*pW = it->second.first;
	*pH = it->second.second;
	return true;
}

static bool LoadTexDims(const char *pszPath)
{
	FILE *f = fopen(pszPath, "r");
	if (!f)
	{
		fprintf(stderr, "cannot open %s\n", pszPath);
		return false;
	}

	// Each line is 'NAME WIDTH HEIGHT'
	// A name can contain spaces, so split from the right
	char szLine[1024];
	while (fgets(szLine, sizeof(szLine), f))
	{
		char *pszEnd = szLine + strlen(szLine);
		while (pszEnd > szLine && (pszEnd[-1] == '\n' || pszEnd[-1] == '\r' || pszEnd[-1] == ' ' || pszEnd[-1] == '\t'))
			--pszEnd;
		*pszEnd = 0;

		// Walk back over the height, the gap, then the width
		char *pszHeight = pszEnd;
		while (pszHeight > szLine && pszHeight[-1] != ' ' && pszHeight[-1] != '\t')
			--pszHeight;
		char *pszGap = pszHeight;
		while (pszGap > szLine && (pszGap[-1] == ' ' || pszGap[-1] == '\t'))
			--pszGap;
		char *pszWidth = pszGap;
		while (pszWidth > szLine && pszWidth[-1] != ' ' && pszWidth[-1] != '\t')
			--pszWidth;
		if (pszWidth <= szLine)
			continue;

		unsigned w = 0, h = 0;
		if (sscanf(pszWidth, "%u", &w) != 1 || sscanf(pszHeight, "%u", &h) != 1 || !w || !h)
			continue;

		// The name is everything before the width, with trailing whitespace removed
		char *pszNameEnd = pszWidth;
		while (pszNameEnd > szLine && (pszNameEnd[-1] == ' ' || pszNameEnd[-1] == '\t'))
			--pszNameEnd;
		*pszNameEnd = 0;
		if (!szLine[0])
			continue;

		g_TexDims[UpperOf(szLine)] = std::make_pair((uint32)w, (uint32)h);
	}

	fclose(f);
	printf("texture dimensions loaded: %u\n", (unsigned)g_TexDims.size());
	return true;
}

static void Warn(const char *pszMessage, void * /*pUser*/)
{
	printf("%s\n", pszMessage);
}


// Rules any correct conversion follows:

// Offsets into the v85 image's fixed header
static const uint32 kV85SectionOffsets = 4;
static const uint32 kV85InfoLength = 60;

static bool ReadWholeFile(const char *pszPath, std::vector<uint8> &out)
{
	FILE *f = fopen(pszPath, "rb");
	if (!f) return false;

	fseek(f, 0, SEEK_END);
	long nLen = ftell(f);
	fseek(f, 0, SEEK_SET);

	out.resize(nLen > 0 ? (size_t)nLen : 0);
	const bool bOk = nLen > 0 && fread(&out[0], 1, (size_t)nLen, f) == (size_t)nLen;
	fclose(f);
	return bOk;
}

// A bounds-checked uint32 read, leaving bOk false once any read has fallen outside the buffer
static uint32 U32At(const std::vector<uint8> &buf, size_t nOffset, bool &bOk)
{
	uint32 v = 0;
	if (nOffset + 4 > buf.size() || nOffset + 4 < nOffset)
		bOk = false;
	else
		memcpy(&v, &buf[nOffset], 4);
	return v;
}

// Every texture reports 64x64, against the loader's 128x128 fallback when there is no callback
static bool FixedTexDims(const char * /*pszName*/, uint32 *pW, uint32 *pH, void * /*pUser*/)
{
	*pW = 64;
	*pH = 64;
	return true;
}

static LTRESULT Convert(const std::vector<uint8> &src, size_t nLen, WorldV56TexDimsFn pTexDims, uint32 nMaxBlockVerts, std::vector<uint8> &out)
{
	out.clear();
	if (nLen == 0) return LT_INVALIDFILE;

	uint8 *pImage = NULL;
	uint32 nImageSize = 0;
	const LTRESULT nResult = world_v56_BuildV85Image(&src[0], (uint32)nLen, &pImage, &nImageSize, pTexDims, NULL, NULL, NULL, nMaxBlockVerts);
	if (pImage)
	{
		out.assign(pImage, pImage + nImageSize);
		world_v56_FreeImage(pImage);
	}
	return nResult;
}

class CRuleCheck
{
public:
	CRuleCheck() : m_nPassed(0), m_nFailed(0) {}

	bool Rule(bool bOk, const char *pszRule)
	{
		printf("%s %s\n", bOk ? "ok  " : "FAIL", pszRule);
		if (bOk) ++m_nPassed; else ++m_nFailed;
		return bOk;
	}

	int m_nPassed;
	int m_nFailed;
};

static void CheckWorld(const std::vector<uint8> &src, CRuleCheck &check)
{
	std::vector<uint8> image;
	LTRESULT nResult = Convert(src, src.size(), NULL, kWorldV56MaxBlockVerts, image);

	bool bRead = true;
	if (!check.Rule(nResult == LT_OK && U32At(image, 0, bRead) == 85 && bRead, "converts to a version 85 image"))
		return;

	// The six section offsets increase and all fall inside the image
	{
		bool bOk = true;
		uint32 nPrev = kV85InfoLength;
		for (uint32 i = 0; i < 6; ++i)
		{
			const uint32 nOffset = U32At(image, kV85SectionOffsets + i * 4, bOk);
			if (nOffset <= nPrev || nOffset >= image.size()) bOk = false;
			nPrev = nOffset;
		}
		check.Rule(bOk, "section offsets increase and fall inside the image");
	}

	// v56 header: version, object block offset, world model section offset, info length, info string
	bool bSrcOk = true;
	const uint32 nObjectPos = U32At(src, 4, bSrcOk);
	const uint32 nWorldModelPos = U32At(src, 8, bSrcOk);
	const uint32 nSrcInfoLen = U32At(src, 12, bSrcOk);

	{
		bool bOk = bSrcOk;
		const uint32 nInfoLen = U32At(image, kV85InfoLength, bOk);
		bOk = bOk && nInfoLen == nSrcInfoLen && 16u + (size_t)nSrcInfoLen <= src.size() && (size_t)kV85InfoLength + 4 + nInfoLen <= image.size();
		bOk = bOk && memcmp(&src[16], &image[kV85InfoLength + 4], nInfoLen) == 0;
		check.Rule(bOk, "world info string is copied unchanged");
	}

	// The object block runs from the first section offset to the second
	{
		bool bOk = bSrcOk;
		const uint32 nStart = U32At(image, kV85SectionOffsets, bOk);
		const uint32 nEnd = U32At(image, kV85SectionOffsets + 4, bOk);
		bOk = bOk && nObjectPos < nWorldModelPos && nWorldModelPos <= src.size() && nStart < nEnd && nEnd <= image.size();
		bOk = bOk && nEnd - nStart == nWorldModelPos - nObjectPos && memcmp(&src[nObjectPos], &image[nStart], nEnd - nStart) == 0;
		check.Rule(bOk, "object block is copied unchanged");
	}

	// Source models: the root model's leading offset points past it to a count of the rest, and each of those points to the next
	{
		bool bOk = bSrcOk;
		uint32 nPos = U32At(src, nWorldModelPos, bOk);
		const uint32 nMore = U32At(src, nPos, bOk);
		nPos += 4;
		for (uint32 i = 0; bOk && i < nMore; ++i)
		{
			const uint32 nNext = U32At(src, nPos, bOk);
			if (nNext <= nPos) bOk = false;
			nPos = nNext;
		}

		// v85: the world tree follows the info string and three vectors then the model count
		size_t nTree = (size_t)kV85InfoLength + 4 + U32At(image, kV85InfoLength, bOk) + 36 + 24;
		const uint32 nTreeNodes = U32At(image, nTree, bOk);
		const uint32 nModels = U32At(image, nTree + 8 + (nTreeNodes + 7) / 8, bOk);
		check.Rule(bOk && nModels == nMore + 1, "world model count matches the source");
	}

	{
		std::vector<uint8> again;
		nResult = Convert(src, src.size(), NULL, kWorldV56MaxBlockVerts, again);
		check.Rule(nResult == LT_OK && again == image, "converting twice gives identical bytes");
	}

	// Texture sizes only scale UVs
	// So they change bytes, but never the layout
	{
		std::vector<uint8> sized;
		nResult = Convert(src, src.size(), FixedTexDims, kWorldV56MaxBlockVerts, sized);
		check.Rule(nResult == LT_OK && sized.size() == image.size() && sized != image, "texture sizes change UVs and nothing else");
	}

	// Splitting into smaller render blocks can only add blocks
	{
		std::vector<uint8> split;
		nResult = Convert(src, src.size(), NULL, 300, split);
		check.Rule(nResult == LT_OK && split.size() >= image.size(), "a 300 vertex block limit still converts");
	}

	{
		std::vector<uint8> scratch;
		const bool bHalf = Convert(src, src.size() / 2, NULL, kWorldV56MaxBlockVerts, scratch) != LT_OK;
		const bool bHeader = Convert(src, 8, NULL, kWorldV56MaxBlockVerts, scratch) != LT_OK;
		check.Rule(bHalf && bHeader, "a truncated source is rejected");
	}

	check.Rule(!world_v56_IsV56(&image[0], (uint32)image.size()), "the converted image is not taken for a v56 world");
}

static int RunCheck(int nWorlds, char **ppWorlds)
{
	CRuleCheck check;
	for (int i = 0; i < nWorlds; ++i)
	{
		std::vector<uint8> src;
		if (!ReadWholeFile(ppWorlds[i], src))
		{
			fprintf(stderr, "cannot read %s\n", ppWorlds[i]);
			return 2;
		}

		printf("%s (%u bytes)\n", ppWorlds[i], (unsigned)src.size());
		if (!check.Rule(world_v56_IsV56(&src[0], (uint32)src.size()), "source is a v56 world"))
			continue;

		CheckWorld(src, check);
	}

	printf("passed %d, failed %d\n", check.m_nPassed, check.m_nFailed);
	return check.m_nFailed ? 1 : 0;
}


int main(int argc, char **argv)
{
	if (argc >= 3 && strcmp(argv[1], "--check") == 0)
		return RunCheck(argc - 2, argv + 2);

	if (argc < 3)
	{
		fprintf(stderr, "usage: %s --check <world.dat> [more worlds...]\n", argv[0]);
		fprintf(stderr, "       %s <world.dat> <out.dat> [textures.txt] [max-block-verts]\n", argv[0]);
		return 2;
	}

	if (argc > 3 && !LoadTexDims(argv[3])) return 2;

	uint32 nMaxBlockVerts = kWorldV56MaxBlockVerts;
	if (argc > 4)
	{
		long n = atol(argv[4]);
		if (n < 3 || n > (long)kWorldV56MaxBlockVerts)
		{
			fprintf(stderr, "max-block-verts must be between 3 and %u\n", (unsigned)kWorldV56MaxBlockVerts);
			return 2;
		}
		nMaxBlockVerts = (uint32)n;
		printf("forcing a %u vertex block limit\n", (unsigned)nMaxBlockVerts);
	}

	FILE *f = fopen(argv[1], "rb");
	if (!f)
	{
		fprintf(stderr, "cannot open %s\n", argv[1]);
		return 2;
	}

	fseek(f, 0, SEEK_END);
	long nLen = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (nLen <= 0)
	{
		fprintf(stderr, "%s is empty\n", argv[1]);
		fclose(f);
		return 2;
	}

	uint8 *pSrc = new uint8[nLen];
	if (fread(pSrc, 1, (size_t)nLen, f) != (size_t)nLen)
	{
		fprintf(stderr, "short read on %s\n", argv[1]);
		fclose(f);
		delete[] pSrc;
		return 2;
	}
	fclose(f);

	printf("%s (%ld bytes)\n", argv[1], nLen);

	if (!world_v56_IsV56(pSrc, (uint32)nLen))
	{
		fprintf(stderr, "not a v56 world\n");
		delete[] pSrc;
		return 1;
	}
	printf("source is v56 (LithTech 1.0)\n");

	uint8 *pImage = NULL;
	uint32 nImageSize = 0;
	LTRESULT nResult = world_v56_BuildV85Image(pSrc, (uint32)nLen, &pImage, &nImageSize, g_TexDims.empty() ? NULL : TexDims, NULL, Warn, NULL, nMaxBlockVerts);
	delete[] pSrc;

	if (nResult != LT_OK || !pImage)
	{
		fprintf(stderr, "conversion failed (%d)\n", (int)nResult);
		return 1;
	}

	FILE *out = fopen(argv[2], "wb");
	if (!out)
	{
		fprintf(stderr, "cannot write %s\n", argv[2]);
		world_v56_FreeImage(pImage);
		return 2;
	}

	fwrite(pImage, 1, nImageSize, out);
	fclose(out);
	printf("wrote %s (%u bytes)\n", argv[2], (unsigned)nImageSize);

	world_v56_FreeImage(pImage);
	return 0;
}
