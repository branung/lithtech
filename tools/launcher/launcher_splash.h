#ifndef LTLAUNCH_SPLASH_H
#define LTLAUNCH_SPLASH_H

// The game's banner, read out of its own archives

#include <string>
#include <vector>

namespace ltlaunch {

struct Image
{
    int m_nWidth;
    int m_nHeight;
    std::vector<unsigned char> m_RGB;

    Image() : m_nWidth(0), m_nHeight(0) {}
    bool Ok() const { return m_nWidth > 0 && m_nHeight > 0 &&
                             m_RGB.size() == (size_t)m_nWidth * m_nHeight * 3; }
    void Clear() { m_nWidth = m_nHeight = 0; m_RGB.clear(); }
};

// One resource out of a .rez by its path inside the archive ("INTERFACE\\SPLASH.PCX")
bool ReadRezEntry(const std::string &sArchive, const std::string &sPath,
                  std::vector<unsigned char> &out);

bool DecodePcx(const std::vector<unsigned char> &in, Image &out);

} // namespace ltlaunch

#endif
