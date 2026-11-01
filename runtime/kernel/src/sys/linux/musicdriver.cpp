#include "bdefs.h"

#include "musicdriver.h"
#include "ltpvalue.h"
#include "console.h"
#ifdef ENABLE_LT1_MUSIC
#include "music_ima.h"
#endif

// No music DLL can be loaded on Linux, so only the built in ima driver works.
// Anything else answers CANTLOADLIBRARY, as Windows does when a DLL fails to load
static SMusicMgr *g_pMusicMgr = LTNULL;


void music_ConsolePrint( const char *pMsg, ... )
{
    va_list marker;
    char    str[500];

    va_start(marker, pMsg);
    LTVSNPrintF(str, sizeof(str), pMsg, marker);
    va_end(marker);

    con_PrintString(CONRGB(100,255,100), 0, str);
}


musicdriver_status music_InitDriver( char *pMusicDLLName, SMusicMgr *pMusicMgr )
{
    if( !pMusicMgr || !pMusicDLLName )
        return MUSICDRIVER_INVALIDOPTIONS;

    music_TermDriver( );
    pMusicMgr->m_bValid = 0;

#ifdef ENABLE_LT1_MUSIC
    if( music_ima_IsBuiltinName( pMusicDLLName ))
    {
        music_ima_Install( pMusicMgr );
        pMusicMgr->ConsolePrint = music_ConsolePrint;

        if( !pMusicMgr->Init( pMusicMgr ))
            return MUSICDRIVER_INVALIDOPTIONS;

        g_pMusicMgr = pMusicMgr;
        return MUSICDRIVER_OK;
    }
#endif

    return MUSICDRIVER_CANTLOADLIBRARY;
}

void music_TermDriver()
{
    if( g_pMusicMgr )
    {
        g_pMusicMgr->Term( );
        g_pMusicMgr->m_bValid = 0;
        g_pMusicMgr = LTNULL;
    }
}
