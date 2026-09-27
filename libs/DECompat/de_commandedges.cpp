#include "de_commandedges.h"
#include "iltserver.h"


// Set the first time this process fires a command
static bool s_bCommandEdgesSeen = false;

// Where the confirmation below prints.
static ILTServer *s_pReportTo = NULL;

void DECompat_SetCommandEdgeReportSink(ILTServer *pServerDE)
{
    s_pReportTo = pServerDE;
}

bool DECompat_CommandEdgesSeen()
{
    return s_bCommandEdgesSeen;
}

// One body for both widths with the shift derived from sizeof
template <typename T>
static void lt1_FireEdges(const T *pOld, const T *pNew, uint32 nWords,
                          DECompat_CommandEdgeFn pfnOn, void *pContext)
{
    if (!pOld || !pNew || !pfnOn)
        return;

    const int nBitsPerWord = (int)(sizeof(T) * 8);

    for (uint32 nWord = 0; nWord < nWords; ++nWord)
    {
        T nPressed = (T)(pNew[nWord] & ~pOld[nWord]);

        for (int nBit = 0; nPressed; ++nBit, nPressed >>= 1)
        {
            if (nPressed & 1)
                pfnOn(pContext, (int)nWord * nBitsPerWord + nBit);
        }
    }

    if (!s_bCommandEdgesSeen)
    {
        s_bCommandEdgesSeen = true;
        if (s_pReportTo)
            s_pReportTo->CPrint("DECompat: command edges active");
    }
}

void DECompat_FireCommandEdges(const uint32 *pOld, const uint32 *pNew,
                               uint32 nWords,
                               DECompat_CommandEdgeFn pfnOn, void *pContext) {
    lt1_FireEdges<uint32>(pOld, pNew, nWords, pfnOn, pContext);
}


void DECompat_FireCommandEdges(const uint64 *pOld, const uint64 *pNew,
                               uint32 nWords,
                               DECompat_CommandEdgeFn pfnOn, void *pContext)
{
    lt1_FireEdges<uint64>(pOld, pNew, nWords, pfnOn, pContext);
}
