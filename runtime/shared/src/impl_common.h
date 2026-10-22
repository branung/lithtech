    
// This module defines common routines to ClientDE and ServerDE as well as 
// helper functions for both modules.

#ifndef __IMPL_COMMON_H__
#define __IMPL_COMMON_H__


#ifndef __ILTCOMMON_H__
#include "iltcommon.h"
#endif

#ifndef __COMPRESS_H__
#include "compress.h"
#endif

#include "lt1_vars.h"


class ILTMath;
class Node;
class WorldTree;
typedef void* HLTFileTree; //class HLTFileTree;



inline LTObject* HObjToLTObj(HOBJECT hObj) {ASSERT(hObj); return (LTObject*)hObj;}


// ------------------------------------------------------------------ //
// Helpers.
// ------------------------------------------------------------------ //

// Get the position and rotation out of a matrix.   
inline void ic_GetTransform(LTMatrix &mat, LTVector &pos, LTRotation &rot) {
    Mat_GetTranslation(mat, pos);
    quat_ConvertFromMatrix((float*)&rot, mat.m);
}


// Tells if the point is inside or outside the BSP.  (Inside being the 
// front side of the nodes).
bool ci_IsPointInsideBSP(const Node *pRoot, const LTVector &P);

// Returns LTTRUE if the point is not outside any BSPs that it intersects.
bool ic_IsPointInsideWorld(WorldTree *pWorldTree, const LTVector *pPoint);

// ------------------------------------------------------------------ //
// Interface implementation functions.
// ------------------------------------------------------------------ //

void ic_StartCounter(LTCounter *pCounter);
uint32 ic_EndCounter(LTCounter *pCounter);

bool ic_UpperStrcmp(const char *pStr1, const char *pStr2);

// Get an animation index from the name.
HMODELANIM ic_GetAnimIndex(HOBJECT hObj, const char *pAnimName);
// Get the animation name from the index.
const char *ic_GetAnimName (HOBJECT hObj, HMODELANIM hAnim);

void ic_FreeString(HSTRING hString);

FileEntry* ic_GetFileList(HLTFileTree **trees, int nTrees, const char *pDirName);
void ic_FreeFileList(FileEntry *pList);

float ic_Random(LTFLOAT min, LTFLOAT max);

// LT1's engine random stream (LT1EngineRand).
// ic_LT1Rand returns 0 to 0x7fff, and ic_LT1Unit is rand() / 32767 in float
void ic_LT1SRand(uint32 nSeed);
int ic_LT1Rand();
float ic_LT1Unit();
uint32 ic_LT1RandState(); // For DemoTrace


#endif  // __IMPL_COMMON_H__



