
// The seven ILTClient font entry points for Linux rasterised with stb_truetype.
// Not a port of the GDI version: font names are resolved to installed files here,
// glyphs are antialiased and extra space is kept per font

#include "bdefs.h"
#include "ltmodule.h"

#include "clientmgr.h"
#include "iltclient.h"
#include "interface_helpers.h"
#include "text_mgr.h"
#include "stringmgr.h"

#include <SDL.h>

#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>

#include <algorithm>
#include <string>
#include <vector>

// Our own copy, so this does not depend on LIB_UI's
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

static ILTClient *ilt_client;
define_holder(ILTClient, ilt_client);


// HLTFONT is opaque, so what it points at is ours
struct LinuxFont
{
    std::vector<unsigned char> m_FileData;   // The whole .ttf; stb indexes into it
    stbtt_fontinfo             m_Info;

    float m_ScaleX;
    float m_ScaleY;

    int   m_Ascent;        // Pixels above the baseline, positive
    int   m_Descent;       // Pixels below the baseline, positive
    int   m_Height;        // m_Ascent + m_Descent, the Win32 tmHeight

    int   m_ExtraSpace;    // Set/GetFontExtraSpace, added after every glyph
    bool  m_bUnderline;

    std::string m_sFile;
};

static std::vector<LinuxFont*> g_Fonts;


// Case and punctuation removed, so "Times New Roman" matches "times-new-roman"
static std::string NormalizeFontName(const char* pszName)
{
    std::string s;
    for (const char* p = pszName; p && *p; ++p)
    {
        if (isalnum((unsigned char)*p)) s += (char)tolower((unsigned char)*p);
    }
    return s;
}

struct FontFile
{
    std::string m_sFamily;
    std::string m_sStyle;
    std::string m_sPath;
};

static std::vector<FontFile> g_FontFiles;
static bool                  g_bFontsScanned = false;

static void ScanFontDir(const std::string& sDir, int nDepth)
{
    // Capped so a symlink loop can't recurse forever
    if (nDepth > 6) return;

    DIR* pDir = opendir(sDir.c_str());
    if (!pDir) return;

    struct dirent* pEntry;
    while ((pEntry = readdir(pDir)) != NULL)
    {
        if (strcmp(pEntry->d_name, ".") == 0 || strcmp(pEntry->d_name, "..") == 0)
            continue;

        std::string sPath = sDir + "/" + pEntry->d_name;

        struct stat statBuf;
        if (stat(sPath.c_str(), &statBuf) != 0) continue;

        if (S_ISDIR(statBuf.st_mode))
        {
            ScanFontDir(sPath, nDepth + 1);
            continue;
        }

        const char* pDot = strrchr(pEntry->d_name, '.');
        if (!pDot) continue;
        // The first face of a .ttc is the regular weight
        if (strcasecmp(pDot, ".ttf") != 0 &&
            strcasecmp(pDot, ".otf") != 0 &&
            strcasecmp(pDot, ".ttc") != 0)
            continue;

        std::string sStem(pEntry->d_name, pDot - pEntry->d_name);
        std::string sFamily = sStem, sStyle;
        size_t nDash = sStem.find('-');
        if (nDash != std::string::npos)
        {
            sFamily = sStem.substr(0, nDash);
            sStyle  = sStem.substr(nDash + 1);
        }

        FontFile ff;
        ff.m_sFamily = NormalizeFontName(sFamily.c_str());
        ff.m_sStyle  = NormalizeFontName(sStyle.c_str());
        ff.m_sPath   = sPath;
        g_FontFiles.push_back(ff);
    }

    closedir(pDir);
}

static void ScanFontDirs()
{
    if (g_bFontsScanned) return;
    g_bFontsScanned = true;

    // LTFONTPATH first, so a game can ship its own fonts
    const char* pszEnv = getenv("LTFONTPATH");
    if (pszEnv)
    {
        std::string sList(pszEnv);
        size_t nStart = 0;
        while (nStart <= sList.size())
        {
            size_t nEnd = sList.find(':', nStart);
            if (nEnd == std::string::npos) nEnd = sList.size();
            if (nEnd > nStart) ScanFontDir(sList.substr(nStart, nEnd - nStart), 0);
            nStart = nEnd + 1;
        }
    }

    const char* pszHome = getenv("HOME");
    if (pszHome)
    {
        ScanFontDir(std::string(pszHome) + "/.local/share/fonts", 0);
        ScanFontDir(std::string(pszHome) + "/.fonts", 0);
    }
    ScanFontDir("/usr/local/share/fonts", 0);
    ScanFontDir("/usr/share/fonts", 0);
}

// Windows font names games ask for, with stand ins best first
struct FontAlias
{
    const char* m_pszAsked;
    const char* m_pszCandidates[5];
};

static const FontAlias kFontAliases[] = {
    { "arial",          { "liberationsans", "arimo", "dejavusans", "freesans", NULL } },
    { "helvetica",      { "liberationsans", "arimo", "dejavusans", "freesans", NULL } },
    { "verdana",        { "dejavusans", "liberationsans", "freesans", NULL, NULL } },
    { "tahoma",         { "dejavusans", "liberationsans", "freesans", NULL, NULL } },
    { "segoeui",        { "dejavusans", "liberationsans", "freesans", NULL, NULL } },
    { "mssansserif",    { "liberationsans", "dejavusans", "freesans", NULL, NULL } },
    { "timesnewroman",  { "liberationserif", "tinos", "dejavuserif", "freeserif", NULL } },
    { "times",          { "liberationserif", "tinos", "dejavuserif", "freeserif", NULL } },
    { "georgia",        { "liberationserif", "dejavuserif", "freeserif", NULL, NULL } },
    { "couriernew",     { "liberationmono", "cousine", "dejavusansmono", "freemono", NULL } },
    { "courier",        { "liberationmono", "cousine", "dejavusansmono", "freemono", NULL } },
    { "lucidaconsole",  { "dejavusansmono", "liberationmono", "freemono", NULL, NULL } },
};

static const char* const kFallbackFamilies[] = {
    "dejavusans", "liberationsans", "freesans", "dejavusansmono", NULL
};

// How well a file's style suffix matches; higher is better
static int ScoreStyle(const std::string& sStyle, bool bBold, bool bItalic)
{
    bool bFileBold   = sStyle.find("bold") != std::string::npos;
    bool bFileItalic = sStyle.find("italic")  != std::string::npos ||
                       sStyle.find("oblique") != std::string::npos;

    int nScore = 0;
    nScore += (bFileBold   == bBold)   ? 2 : -1;
    nScore += (bFileItalic == bItalic) ? 2 : -1;

    // Prefer the plainest name, so Regular beats Condensed
    nScore -= (int)(sStyle.size() / 8);
    return nScore;
}

static bool FindFamilyFile(const std::string& sFamily, bool bBold, bool bItalic, std::string& sOut)
{
    int nBest = -1000;
    const FontFile* pBest = NULL;

    for (size_t i = 0; i < g_FontFiles.size(); ++i)
    {
        if (g_FontFiles[i].m_sFamily != sFamily) continue;
        int nScore = ScoreStyle(g_FontFiles[i].m_sStyle, bBold, bItalic);
        if (nScore > nBest)
        {
            nBest = nScore;
            pBest = &g_FontFiles[i];
        }
    }

    if (!pBest) return false;
    sOut = pBest->m_sPath;
    return true;
}

static bool FindFontFile(const char* pszFace, bool bBold, bool bItalic, std::string& sOut)
{
    // A path to a real file wins outright
    if (pszFace && pszFace[0] && strchr(pszFace, '/'))
    {
        struct stat statBuf;
        if (stat(pszFace, &statBuf) == 0 && S_ISREG(statBuf.st_mode))
        {
            sOut = pszFace;
            return true;
        }
    }

    ScanFontDirs();
    if (g_FontFiles.empty()) return false;

    std::string sAsked = NormalizeFontName(pszFace ? pszFace : "");

    if (!sAsked.empty() && FindFamilyFile(sAsked, bBold, bItalic, sOut))
        return true;

    for (size_t i = 0; i < sizeof(kFontAliases) / sizeof(kFontAliases[0]); ++i)
    {
        if (sAsked != kFontAliases[i].m_pszAsked) continue;
        for (int j = 0; kFontAliases[i].m_pszCandidates[j]; ++j)
        {
            if (FindFamilyFile(kFontAliases[i].m_pszCandidates[j], bBold, bItalic, sOut))
                return true;
        }
        break;
    }

    // Then anything, since a game with no text draws an empty menu rather than an error
    for (int i = 0; kFallbackFamilies[i]; ++i)
    {
        if (FindFamilyFile(kFallbackFamilies[i], bBold, bItalic, sOut))
            return true;
    }

    sOut = g_FontFiles[0].m_sPath;
    return true;
}

// The same search for the vector font, which on Windows GDI does by face name
bool tmgr_FindFontFile(const char* pszFace, std::string& sOut)
{
    return FindFontFile(pszFace, false, false, sOut);
}


static float GlyphAdvance(const LinuxFont* pFont, int ch)
{
    int nAdvance = 0, nLeftBearing = 0;
    stbtt_GetCodepointHMetrics(&pFont->m_Info, ch, &nAdvance, &nLeftBearing);
    return (float)nAdvance * pFont->m_ScaleX + (float)pFont->m_ExtraSpace;
}

static void MeasureString(const LinuxFont* pFont, const char* pszText, int nChars,
                          int* pWidth, int* pHeight)
{
    float fWidth = 0.0f;
    for (int i = 0; i < nChars; ++i)
    {
        fWidth += GlyphAdvance(pFont, (unsigned char)pszText[i]);
        if (i + 1 < nChars)
        {
            fWidth += (float)stbtt_GetCodepointKernAdvance(
                &pFont->m_Info,
                (unsigned char)pszText[i],
                (unsigned char)pszText[i + 1]) * pFont->m_ScaleX;
        }
    }

    // Rounded so a surface sized from it doesn't clip the last column
    if (pWidth)  *pWidth  = (int)(fWidth + 0.5f);
    if (pHeight) *pHeight = pFont->m_Height;
}

static bool GetStringText(HSTRING hString, const char** ppText, int* pChars)
{
    if (!hString) return false;

    const char* pszText = (const char*)str_GetStringBytes(hString, LTNULL);
    if (!pszText) return false;

    int nChars = str_GetNumStringCharacters(hString);
    if (nChars <= 0) return false;

    *ppText = pszText;
    *pChars = nChars;
    return true;
}


static bool RasterizeString(const LinuxFont* pFont, const char* pszText, int nChars,
                            std::vector<uint8>& coverage, int& nWidth, int& nHeight)
{
    MeasureString(pFont, pszText, nChars, &nWidth, &nHeight);
    if (nWidth <= 0 || nHeight <= 0) return false;

    coverage.assign((size_t)nWidth * (size_t)nHeight, 0);

    float fX = 0.0f;
    for (int i = 0; i < nChars; ++i)
    {
        int ch = (unsigned char)pszText[i];

        // Subpixel positioning, since whole pixels bunch the letters at small sizes
        float fShift = fX - (float)floor(fX);

        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBoxSubpixel(&pFont->m_Info, ch,
            pFont->m_ScaleX, pFont->m_ScaleY, fShift, 0.0f, &x0, &y0, &x1, &y1);

        int nGlyphW = x1 - x0;
        int nGlyphH = y1 - y0;
        if (nGlyphW > 0 && nGlyphH > 0)
        {
            int nDestX = (int)floor(fX) + x0;
            int nDestY = pFont->m_Ascent + y0;

            // Clip to the bitmap: bearings can reach outside it
            int nSrcX = 0, nSrcY = 0;
            int nCopyW = nGlyphW, nCopyH = nGlyphH;

            if (nDestX < 0) { nSrcX = -nDestX; nCopyW -= nSrcX; nDestX = 0; }
            if (nDestY < 0) { nSrcY = -nDestY; nCopyH -= nSrcY; nDestY = 0; }
            if (nDestX + nCopyW > nWidth)  nCopyW = nWidth  - nDestX;
            if (nDestY + nCopyH > nHeight) nCopyH = nHeight - nDestY;

            if (nCopyW > 0 && nCopyH > 0)
            {
                std::vector<uint8> glyph((size_t)nGlyphW * (size_t)nGlyphH, 0);
                stbtt_MakeCodepointBitmapSubpixel(&pFont->m_Info, &glyph[0],
                    nGlyphW, nGlyphH, nGlyphW,
                    pFont->m_ScaleX, pFont->m_ScaleY, fShift, 0.0f, ch);

                for (int y = 0; y < nCopyH; ++y)
                {
                    const uint8* pSrc = &glyph[(size_t)(nSrcY + y) * nGlyphW + nSrcX];
                    uint8* pDst = &coverage[(size_t)(nDestY + y) * nWidth + nDestX];
                    for (int x = 0; x < nCopyW; ++x)
                    {
                        // Overlapping glyphs keep the stronger coverage
                        if (pSrc[x] > pDst[x]) pDst[x] = pSrc[x];
                    }
                }
            }
        }

        fX += GlyphAdvance(pFont, ch);
        if (i + 1 < nChars)
        {
            fX += (float)stbtt_GetCodepointKernAdvance(&pFont->m_Info, ch,
                    (unsigned char)pszText[i + 1]) * pFont->m_ScaleX;
        }
    }

    if (pFont->m_bUnderline)
    {
        // stb has no underline, so draw one below the baseline
        int nY = pFont->m_Ascent + 1;
        if (nY >= nHeight) nY = nHeight - 1;
        if (nY >= 0) memset(&coverage[(size_t)nY * nWidth], 255, (size_t)nWidth);
    }

    return true;
}


// An HSURFACE is an SDL_Surface in the SDL client and a CisSurface under
// ENABLE_DXVK, which links the Windows client. Only locking and packing differ
struct TextTarget
{
    uint8*  m_pPixels;
    long    m_Pitch;
    int     m_Width, m_Height;
    int     m_BytesPerPixel;
#ifdef USE_DXVK
    CisSurface*     m_pCis;
#else
    SDL_Surface*    m_pSurface;
#endif
};

static bool LockTarget(HSURFACE hSurface, TextTarget& target)
{
    if (!hSurface) return false;
#ifdef USE_DXVK
    CisSurface* pCis = (CisSurface*)hSurface;
    long nPitch = 0;
    uint8* pPixels = (uint8*)cis_LockSurface(pCis, nPitch, LTTRUE);
    if (!pPixels) return false;

    target.m_pPixels       = pPixels;
    target.m_Pitch         = nPitch;
    target.m_Width         = (int)pCis->m_Width;
    target.m_Height        = (int)pCis->m_Height;
    target.m_BytesPerPixel = (int)g_ScreenFormat.GetBytesPerPixel();
    target.m_pCis          = pCis;
#else
    SDL_Surface* pSurface = (SDL_Surface*)hSurface;
    if (SDL_MUSTLOCK(pSurface) && SDL_LockSurface(pSurface) != 0) return false;

    target.m_pPixels       = (uint8*)pSurface->pixels;
    target.m_Pitch         = pSurface->pitch;
    target.m_Width         = pSurface->w;
    target.m_Height        = pSurface->h;
    target.m_BytesPerPixel = pSurface->format->BytesPerPixel;
    target.m_pSurface      = pSurface;
#endif
    return true;
}

static void UnlockTarget(TextTarget& target)
{
#ifdef USE_DXVK
    cis_UnlockSurface(target.m_pCis);
#else
    if (SDL_MUSTLOCK(target.m_pSurface)) SDL_UnlockSurface(target.m_pSurface);
#endif
}

static inline uint32 ReadPixelValue(int nBytesPerPixel, const uint8* pPixel)
{
    switch (nBytesPerPixel)
    {
        case 1: return *pPixel;
        case 2: return *(const uint16*)pPixel;
        case 3:
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
                return (uint32)(pPixel[0] << 16 | pPixel[1] << 8 | pPixel[2]);
            return (uint32)(pPixel[0] | pPixel[1] << 8 | pPixel[2] << 16);
        default: return *(const uint32*)pPixel;
    }
}

static inline void WritePixelValue(int nBytesPerPixel, uint8* pPixel, uint32 nValue)
{
    switch (nBytesPerPixel)
    {
        case 1: *pPixel = (uint8)nValue; break;
        case 2: *(uint16*)pPixel = (uint16)nValue; break;
        case 3:
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
            {
                pPixel[0] = (uint8)(nValue >> 16);
                pPixel[1] = (uint8)(nValue >> 8);
                pPixel[2] = (uint8)nValue;
            }
            else
            {
                pPixel[0] = (uint8)nValue;
                pPixel[1] = (uint8)(nValue >> 8);
                pPixel[2] = (uint8)(nValue >> 16);
            }
            break;
        default: *(uint32*)pPixel = nValue; break;
    }
}

#ifdef USE_DXVK
// 8 bit channels to and from a PFormat plane, replicating high bits on the way back
static inline uint32 PackPlane(const PFormat& fmt, int iPlane, uint8 nValue)
{
    uint32 nBits = fmt.m_nBits[iPlane];
    if (nBits == 0) return 0;
    uint32 nPacked = (nBits >= 8) ? ((uint32)nValue << (nBits - 8)) : ((uint32)nValue >> (8 - nBits));
    return (nPacked << fmt.m_FirstBits[iPlane]) & fmt.m_Masks[iPlane];
}

static inline uint8 UnpackPlane(const PFormat& fmt, int iPlane, uint32 nPixel)
{
    int nBits = (int)fmt.m_nBits[iPlane];
    if (nBits == 0) return 0;
    uint32 nValue = (nPixel & fmt.m_Masks[iPlane]) >> fmt.m_FirstBits[iPlane];
    if (nBits >= 8) return (uint8)(nValue >> (nBits - 8));

    uint32 nOut = 0;
    for (int nShift = 8 - nBits; nShift > -nBits; nShift -= nBits)
        nOut |= (nShift >= 0) ? (nValue << nShift) : (nValue >> -nShift);
    return (uint8)nOut;
}
#endif

static inline void GetPixelRGB(const TextTarget& target, const uint8* pPixel,
                               uint8& r, uint8& g, uint8& b)
{
    uint32 nValue = ReadPixelValue(target.m_BytesPerPixel, pPixel);
#ifdef USE_DXVK
    r = UnpackPlane(g_ScreenFormat, CP_RED,   nValue);
    g = UnpackPlane(g_ScreenFormat, CP_GREEN, nValue);
    b = UnpackPlane(g_ScreenFormat, CP_BLUE,  nValue);
#else
    SDL_GetRGB(nValue, target.m_pSurface->format, &r, &g, &b);
#endif
}

static inline void PutPixelRGB(const TextTarget& target, uint8* pPixel,
                               uint8 r, uint8 g, uint8 b)
{
#ifdef USE_DXVK
    // Opaque, as PValueToFormatColor gives the Windows rasterisers
    uint32 nValue = PackPlane(g_ScreenFormat, CP_RED,   r)
                  | PackPlane(g_ScreenFormat, CP_GREEN, g)
                  | PackPlane(g_ScreenFormat, CP_BLUE,  b)
                  | g_ScreenFormat.m_Masks[CP_ALPHA];
#else
    uint32 nValue = SDL_MapRGB(target.m_pSurface->format, r, g, b);
#endif
    WritePixelValue(target.m_BytesPerPixel, pPixel, nValue);
}

// Blends the coverage onto the surface with the same transparency cases as Windows
static void BlitCoverage(HSURFACE hSurface, const std::vector<uint8>& coverage,
                         int nCovW, int nCovH, int nDestX, int nDestY,
                         const LTRect* pClip, HLTCOLOR hForeColor, HLTCOLOR hBackColor)
{
    bool bForeTransparent = IsColorTransparent(hForeColor) ? true : false;
    bool bBackTransparent = IsColorTransparent(hBackColor) ? true : false;
    if (bForeTransparent && bBackTransparent) return;    // nothing would get drawn

    TextTarget target;
    if (!LockTarget(hSurface, target)) return;

    uint8 nForeR = (uint8)GETR(hForeColor), nForeG = (uint8)GETG(hForeColor), nForeB = (uint8)GETB(hForeColor);
    uint8 nBackR = (uint8)GETR(hBackColor), nBackG = (uint8)GETG(hBackColor), nBackB = (uint8)GETB(hBackColor);

    int nLeft   = nDestX;
    int nTop    = nDestY;
    int nRight  = nDestX + nCovW;
    int nBottom = nDestY + nCovH;

    if (pClip)
    {
        nLeft   = std::max(nLeft,   (int)pClip->left);
        nTop    = std::max(nTop,    (int)pClip->top);
        nRight  = std::min(nRight,  (int)pClip->right);
        nBottom = std::min(nBottom, (int)pClip->bottom);
    }
    nLeft   = std::max(nLeft, 0);
    nTop    = std::max(nTop, 0);
    nRight  = std::min(nRight,  target.m_Width);
    nBottom = std::min(nBottom, target.m_Height);

    if (nLeft >= nRight || nTop >= nBottom) { UnlockTarget(target); return; }

    const int nBytesPerPixel = target.m_BytesPerPixel;

    for (int y = nTop; y < nBottom; ++y)
    {
        const uint8* pCov = &coverage[(size_t)(y - nDestY) * nCovW + (nLeft - nDestX)];
        uint8* pPixel = target.m_pPixels + (size_t)y * target.m_Pitch
                                         + (size_t)nLeft * nBytesPerPixel;

        for (int x = nLeft; x < nRight; ++x, ++pCov, pPixel += nBytesPerPixel)
        {
            uint32 nCoverage = *pCov;

            if (bBackTransparent)
            {
                if (nCoverage == 0) continue;
                if (nCoverage == 255)
                {
                    PutPixelRGB(target, pPixel, nForeR, nForeG, nForeB);
                }
                else
                {
                    uint8 r, g, b;
                    GetPixelRGB(target, pPixel, r, g, b);
                    PutPixelRGB(target, pPixel,
                        (uint8)((nForeR * nCoverage + r * (255 - nCoverage)) / 255),
                        (uint8)((nForeG * nCoverage + g * (255 - nCoverage)) / 255),
                        (uint8)((nForeB * nCoverage + b * (255 - nCoverage)) / 255));
                }
            }
            else if (bForeTransparent)
            {
                if (nCoverage == 0)
                    PutPixelRGB(target, pPixel, nBackR, nBackG, nBackB);
            }
            else
            {
                PutPixelRGB(target, pPixel,
                    (uint8)((nForeR * nCoverage + nBackR * (255 - nCoverage)) / 255),
                    (uint8)((nForeG * nCoverage + nBackG * (255 - nCoverage)) / 255),
                    (uint8)((nForeB * nCoverage + nBackB * (255 - nCoverage)) / 255));
            }
        }
    }

    UnlockTarget(target);
}


static HLTFONT tmgr_CreateFont(const char *pFontName, int width, int height,
                               bool bItalic, bool bUnderline, bool bBold)
{
    if (height <= 0) return LTNULL;

    std::string sFile;
    if (!FindFontFile(pFontName, bBold, bItalic, sFile))
        return LTNULL;

    FILE* pFile = fopen(sFile.c_str(), "rb");
    if (!pFile) return LTNULL;

    fseek(pFile, 0, SEEK_END);
    long nLen = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);
    if (nLen <= 0) { fclose(pFile); return LTNULL; }

    LinuxFont* pFont = new LinuxFont;
    pFont->m_FileData.resize((size_t)nLen);
    size_t nRead = fread(&pFont->m_FileData[0], 1, (size_t)nLen, pFile);
    fclose(pFile);
    if (nRead != (size_t)nLen) { delete pFont; return LTNULL; }

    int nOffset = stbtt_GetFontOffsetForIndex(&pFont->m_FileData[0], 0);
    if (nOffset < 0 || !stbtt_InitFont(&pFont->m_Info, &pFont->m_FileData[0], nOffset))
    {
        delete pFont;
        return LTNULL;
    }

    pFont->m_ScaleY = stbtt_ScaleForPixelHeight(&pFont->m_Info, (float)height);
    pFont->m_ScaleX = pFont->m_ScaleY;

    // A width scales the font horizontally, as Windows takes it as the average
    // character width. Clamped so a bad request stays readable
    if (width > 0)
    {
        float fTotalAdvance = 0.0f;
        int nCount = 0;
        for (int ch = 'a'; ch <= 'z'; ++ch)
        {
            int nAdvance = 0, nLeftBearing = 0;
            stbtt_GetCodepointHMetrics(&pFont->m_Info, ch, &nAdvance, &nLeftBearing);
            fTotalAdvance += (float)nAdvance * pFont->m_ScaleY;
            ++nCount;
        }
        if (nCount > 0 && fTotalAdvance > 0.0f)
        {
            float fNatural = fTotalAdvance / (float)nCount;
            float fRatio   = (float)width / fNatural;
            if (fRatio < 0.5f) fRatio = 0.5f;
            if (fRatio > 2.0f) fRatio = 2.0f;
            pFont->m_ScaleX = pFont->m_ScaleY * fRatio;
        }
    }

    int nAscent = 0, nDescent = 0, nLineGap = 0;
    stbtt_GetFontVMetrics(&pFont->m_Info, &nAscent, &nDescent, &nLineGap);
    pFont->m_Ascent  = (int)((float)nAscent  * pFont->m_ScaleY + 0.5f);
    pFont->m_Descent = (int)((float)(-nDescent) * pFont->m_ScaleY + 0.5f);
    pFont->m_Height  = pFont->m_Ascent + pFont->m_Descent;
    if (pFont->m_Height <= 0) pFont->m_Height = height;

    pFont->m_ExtraSpace = 0;
    pFont->m_bUnderline = bUnderline;
    pFont->m_sFile      = sFile;

    g_Fonts.push_back(pFont);
    return (HLTFONT)pFont;
}

static void tmgr_DeleteFont(HLTFONT hFont)
{
    LinuxFont* pFont = (LinuxFont*)hFont;
    if (!pFont) return;

    std::vector<LinuxFont*>::iterator it = std::find(g_Fonts.begin(), g_Fonts.end(), pFont);
    if (it == g_Fonts.end()) return;   // not ours, or already deleted

    g_Fonts.erase(it);
    delete pFont;
}

static LTRESULT tmgr_SetFontExtraSpace(HLTFONT hFont, int pixels)
{
    LinuxFont* pFont = (LinuxFont*)hFont;
    if (!pFont) return LT_ERROR;

    // Per font. Windows keeps it on a shared HDC, where the last font set wins
    pFont->m_ExtraSpace = pixels;
    return LT_OK;
}

static LTRESULT tmgr_GetFontExtraSpace(HLTFONT hFont, int &pixels)
{
    LinuxFont* pFont = (LinuxFont*)hFont;
    if (!pFont) return LT_ERROR;

    pixels = pFont->m_ExtraSpace;
    return LT_OK;
}

static void tmgr_GetStringDimensions(HLTFONT hFont, HSTRING hString, int *sizeX, int *sizeY)
{
    LinuxFont* pFont = (LinuxFont*)hFont;

    const char* pszText;
    int nChars;
    if (!pFont || !GetStringText(hString, &pszText, &nChars))
    {
        if (sizeX) *sizeX = 0;
        if (sizeY) *sizeY = 0;
        return;
    }

    MeasureString(pFont, pszText, nChars, sizeX, sizeY);
}

static void tmgr_DrawStringToSurface(HSURFACE hDest, HLTFONT hFont, HSTRING hString,
                                     LTRect *pRect, HLTCOLOR hForeColor, HLTCOLOR hBackColor)
{
    LinuxFont* pFont = (LinuxFont*)hFont;

    const char* pszText;
    int nChars;
    if (!pFont || !hDest || !GetStringText(hString, &pszText, &nChars))
        return;

    std::vector<uint8> coverage;
    int nWidth = 0, nHeight = 0;
    if (!RasterizeString(pFont, pszText, nChars, coverage, nWidth, nHeight))
        return;

    // At the rect's top left and clipped to it, or the origin without one
    int nDestX = pRect ? pRect->left : 0;
    int nDestY = pRect ? pRect->top  : 0;

    BlitCoverage(hDest, coverage, nWidth, nHeight, nDestX, nDestY,
                 pRect, hForeColor, hBackColor);
}

static HSURFACE tmgr_CreateSurfaceFromString(HLTFONT hFont, HSTRING hString,
                                             HLTCOLOR hForeColor, HLTCOLOR hBackColor,
                                             int extraPixelsX, int extraPixelsY)
{
    LinuxFont* pFont = (LinuxFont*)hFont;
    if (!pFont) return LTNULL;

    const char* pszText = NULL;
    int nChars = 0;
    bool bHaveText = GetStringText(hString, &pszText, &nChars);

    int nWidth = 1, nHeight = 1;
    if (bHaveText)
        MeasureString(pFont, pszText, nChars, &nWidth, &nHeight);

    nWidth  += extraPixelsX;
    nHeight += extraPixelsY;
    if (nWidth <= 0 || nHeight <= 0) return LTNULL;

    HSURFACE hSurface = ilt_client->CreateSurface((uint32)nWidth, (uint32)nHeight);
    if (!hSurface) return LTNULL;

    // A new surface holds garbage, so lay the background everywhere
    TextTarget target;
    if (!IsColorTransparent(hBackColor) && LockTarget(hSurface, target))
    {
        const uint8 r = (uint8)GETR(hBackColor), g = (uint8)GETG(hBackColor), b = (uint8)GETB(hBackColor);
        for (int y = 0; y < target.m_Height; ++y)
        {
            uint8* pPixel = target.m_pPixels + (size_t)y * target.m_Pitch;
            for (int x = 0; x < target.m_Width; ++x, pPixel += target.m_BytesPerPixel)
                PutPixelRGB(target, pPixel, r, g, b);
        }
        UnlockTarget(target);
    }

    if (bHaveText)
    {
        std::vector<uint8> coverage;
        int nCovW = 0, nCovH = 0;
        if (RasterizeString(pFont, pszText, nChars, coverage, nCovW, nCovH))
        {
            // Background is down; draw only the glyphs
            BlitCoverage(hSurface, coverage, nCovW, nCovH, 0, 0, LTNULL,
                         hForeColor, hBackColor | COLOR_TRANSPARENCY_MASK);
        }
    }

    return hSurface;
}


void tmgr_Init()
{
    ilt_client->CreateFont              = tmgr_CreateFont;
    ilt_client->DeleteFont              = tmgr_DeleteFont;
    ilt_client->SetFontExtraSpace       = tmgr_SetFontExtraSpace;
    ilt_client->GetFontExtraSpace       = tmgr_GetFontExtraSpace;
    ilt_client->GetStringDimensions     = tmgr_GetStringDimensions;
    ilt_client->DrawStringToSurface     = tmgr_DrawStringToSurface;
    ilt_client->CreateSurfaceFromString = tmgr_CreateSurfaceFromString;

}

void tmgr_Term()
{
    for (size_t i = 0; i < g_Fonts.size(); ++i)
        delete g_Fonts[i];
    g_Fonts.clear();

    g_FontFiles.clear();
    g_bFontsScanned = false;
}
