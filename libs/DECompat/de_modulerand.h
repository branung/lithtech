#ifndef __DE_MODULERAND_H__
#define __DE_MODULERAND_H__

#ifdef __cplusplus

#include <stdlib.h>
#include <cstdlib>

#if defined(_MSC_VER)
#define DE_MODULERAND_LOCAL
#else
#define DE_MODULERAND_LOCAL __attribute__((visibility("hidden")))
#endif

DE_MODULERAND_LOCAL inline unsigned int &DE_ModuleRandState()
{
    static unsigned int s_nHold = 1;
    return s_nHold;
}

// Draws since the last seed for traces that ask which draw a caller got
DE_MODULERAND_LOCAL inline unsigned int &DE_ModuleRandCount()
{
    static unsigned int s_nCount = 0;
    return s_nCount;
}

inline int DE_ModuleRand()
{
    unsigned int &nHold = DE_ModuleRandState();
    ++DE_ModuleRandCount();
    nHold = nHold * 214013u + 2531011u;
    return (int)((nHold >> 16) & 0x7fff);
}

inline void DE_ModuleSRand(unsigned int nSeed)
{
    DE_ModuleRandState() = nSeed;
    DE_ModuleRandCount() = 0;
}

// An optional trace of every draw and seed, with the call site
typedef void (*DE_ModuleRandHookFn)(const char *pFile, int nLine, unsigned nCount, int nValue);
DE_MODULERAND_LOCAL inline DE_ModuleRandHookFn &DE_ModuleRandHook()
{
    static DE_ModuleRandHookFn s_pHook = 0;
    return s_pHook;
}

inline int DE_ModuleRandAt(const char *pFile, int nLine)
{
    const int nValue = DE_ModuleRand();
    if (DE_ModuleRandHook())
        DE_ModuleRandHook()(pFile, nLine, DE_ModuleRandCount(), nValue);
    return nValue;
}

inline void DE_ModuleSRandAt(unsigned int nSeed, const char *pFile, int nLine)
{
    DE_ModuleSRand(nSeed);
    if (DE_ModuleRandHook())
        DE_ModuleRandHook()(pFile, nLine, nSeed, -1);
}

#define rand() DE_ModuleRandAt(__FILE__, __LINE__)
#define srand(n) DE_ModuleSRandAt((n), __FILE__, __LINE__)

#if RAND_MAX != 0x7fff
#undef RAND_MAX
#define RAND_MAX 0x7fff
#endif

#endif // __cplusplus

#endif // __DE_MODULERAND_H__
