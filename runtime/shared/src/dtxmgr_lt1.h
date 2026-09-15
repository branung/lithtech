#ifndef __DTXMGR_LT1_H__
#define __DTXMGR_LT1_H__

// Runtime support for LithTech 1.0 textures (DTX version -2)
//
// Jupiter's dtx_Create demands CURRENT_DTX_VERSION (-5), so the original data is rejected outright.  
// This reads the older layout directly, meaning the engine can be pointed at an original install.

class ILTStream;
class TextureData;

// The m_Version value LithTech 1.0 wrote
#define LT1_DTX_VERSION -2

// LithTech 1.0 m_IFlags bits
#define LT1_DTX_FULLBRITE   (1<<0)  // format + run the fullbright pass
#define LT1_DTX_ALPHAMASK   (1<<1)  // 4-bit alpha mask follows the pixel data
#define LT1_DTX_SECTIONSFIXED (1<<3)

// Reads a version -2 texture and hands back what dtx_Create would have produced from an equivalent v-5 file
LTRESULT dtx_CreateFromLT1(ILTStream *pStream, TextureData **ppOut,
                           uint32 &nBaseWidth, uint32 &nBaseHeight);

#endif  // __DTXMGR_LT1_H__
