#include "launcher_splash.h"
#include "rezmgr.h"

#include <cstring>

namespace ltlaunch {

// Through the engine's own archive reader
bool ReadRezEntry(const std::string &sArchive, const std::string &sPath,
                  std::vector<unsigned char> &out)
{
    out.clear();
    CRezMgr mgr;
    if (!mgr.Open(sArchive.c_str())) return false;

    CRezItm *pItem = mgr.GetRezFromDosPath(sPath.c_str());
    const BYTE *pData = pItem ? pItem->Load() : NULL;
    if (pData) out.assign(pData, pData + pItem->GetSize());
    mgr.Close();
    return pData != NULL;
}

bool DecodePcx(const std::vector<unsigned char> &in, Image &out)
{
    out.Clear();
    if (in.size() < 128) return false;
    if (in[0] != 0x0A) return false;
    const int nEncoding = in[2];
    const int nBitsPerPlane = in[3];
    if (nEncoding != 1 || nBitsPerPlane != 8) return false;

    const int xmin = in[4] | (in[5] << 8);
    const int ymin = in[6] | (in[7] << 8);
    const int xmax = in[8] | (in[9] << 8);
    const int ymax = in[10] | (in[11] << 8);
    const int nPlanes = in[65];
    const int nBytesPerLine = in[66] | (in[67] << 8);

    const int w = xmax - xmin + 1;
    const int h = ymax - ymin + 1;
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return false;
    if (nPlanes != 1 && nPlanes != 3) return false;
    if (nBytesPerLine < w) return false;

    // The 256 color palette is the last 769 bytes
    const unsigned char *pPal = 0;
    if (in.size() >= 769 && in[in.size() - 769] == 0x0C)
        pPal = &in[in.size() - 768];
    if (nPlanes == 1 && !pPal) return false;

    const size_t nRowBytes = (size_t)nBytesPerLine * nPlanes;
    std::vector<unsigned char> row(nRowBytes);

    out.m_nWidth = w;
    out.m_nHeight = h;
    out.m_RGB.assign((size_t)w * h * 3, 0);

    // Decodes the RLE stream one scanline at a time
    size_t src = 128;
    const size_t nEnd = pPal ? (in.size() - 769) : in.size();
    for (int y = 0; y < h; ++y)
    {
        size_t n = 0;
        while (n < nRowBytes)
        {
            if (src >= nEnd) return false;
            unsigned char b = in[src++];
            size_t nRun = 1;
            if ((b & 0xC0) == 0xC0)
            {
                nRun = (size_t)(b & 0x3F);
                if (src >= nEnd) return false;
                b = in[src++];
                if (nRun == 0) continue;
            }
            if (nRun > nRowBytes - n) nRun = nRowBytes - n;
            std::memset(&row[n], b, nRun);
            n += nRun;
        }

        unsigned char *pOut = &out.m_RGB[(size_t)y * w * 3];
        if (nPlanes == 1)
        {
            for (int x = 0; x < w; ++x)
            {
                const unsigned char *p = pPal + (size_t)row[x] * 3;
                pOut[x * 3 + 0] = p[0];
                pOut[x * 3 + 1] = p[1];
                pOut[x * 3 + 2] = p[2];
            }
        }
        else
        {
            for (int x = 0; x < w; ++x)
            {
                pOut[x * 3 + 0] = row[(size_t)x];
                pOut[x * 3 + 1] = row[(size_t)nBytesPerLine + x];
                pOut[x * 3 + 2] = row[(size_t)nBytesPerLine * 2 + x];
            }
        }
    }
    return true;
}

} // namespace ltlaunch
