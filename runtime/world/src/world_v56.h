#ifndef __WORLD_V56_H__
#define __WORLD_V56_H__

// Runtime support for LithTech 1.0 worlds (DAT version 56)

// CWorldSharedBSP::Load requires an exact version match, so an original level is rejected before the geometry is even read.
// This reads v56 and builds the v85 image the loader already consumes.

#include "ltbasetypes.h"
#include "ltcodes.h"

// Most vertices a render block may hold.
// The renderer stores each index as a uint16, so an index past this wraps and the block draws nonsense.
const uint32 kWorldV56MaxBlockVerts = 65536;

// True if the buffer starts with a v56 world header
bool world_v56_IsV56(const uint8 *pData, uint32 nSize);

// Supplies a texture's pixel dimensions, which the UV normalisation needs.
// Return false to accept the 128x128 default, which scales a texture of any other size.
typedef bool (*WorldV56TexDimsFn)(const char *pszName, uint32 *pWidth, uint32 *pHeight, void *pUser);

// Reports a condition the caller should see, such as a render block past the uint16 index limit
typedef void (*WorldV56WarnFn)(const char *pszMessage, void *pUser);

// Converts a whole v56 world into the v85 image the engine loads.
// The caller owns *ppImage and frees it with world_v56_FreeImage.
// pTexDims may be NULL, in which case every texture is treated as 128x128
LTRESULT world_v56_BuildV85Image(const uint8 *pSrc, uint32 nSrcSize, uint8 **ppImage, uint32 *pImageSize, WorldV56TexDimsFn pTexDims, 
    void *pUser, WorldV56WarnFn pWarn, void *pWarnUser, uint32 nMaxBlockVerts = kWorldV56MaxBlockVerts);

void world_v56_FreeImage(uint8 *pImage);

class ILTStream;

// Opens a game file by name for the texture lookup.
// Passed in since the client and server reach their file managers through different globals.
typedef ILTStream *(*WorldV56OpenFn)(const char *pszName);

// If pSrc holds a v56 world, returns a new read-only stream over the v85 image, otherwise NULL.
// The caller still owns pSrc. pOpen may be NULL, in which case every texture is treated as 128x128.
ILTStream *world_v56_WrapStream(ILTStream *pSrc, WorldV56OpenFn pOpen);

#endif // __WORLD_V56_H__
