// ----------------------------------------------------------------------- //
//
// MODULE  : Music.cpp
//
// PURPOSE : Music helper class.  Handles all music triggers
//
// CREATED : 12/18/97
//
// ----------------------------------------------------------------------- //

#include "Music.h"
#include "clientheaders.h"
#include "ClientServerShared.h"
#include "iltclient.h"
#include "RiotMsgIDs.h"
#include "iltcommon.h"
#include "iltmessage.h"
#include "ClientUtilities.h"

// Transitions arrays are organized so index 0 is the intro from silence,
// index 1 is the ending to silence, and the other indices are transitions
// from the other music levels.  Example:  m_szAmbientTrans index 0=
// intro from silence, index=1 ending to silence, index 2=transition from
// cruising, index 3=transition from harddriving.

static char g_szAmbientTrans[4][PARSE_MAXTOKENSIZE] = 
{
	"sta.sec", "ats.sec", "cta.sec", "hta.sec"
};

static char g_szCruisingTrans[4][PARSE_MAXTOKENSIZE] = 
{
	"stc.sec", "cts.sec", "atc.sec", "htc.sec"
};

static char g_szHarddrivingTrans[4][PARSE_MAXTOKENSIZE] = 
{
	"sth.sec", "cts.sec", "ath.sec", "cth.sec"
};


 
// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMusic::CMusic
//
//	PURPOSE:	Constructor
//
// ----------------------------------------------------------------------- //
CMusic::CMusic( )
{
	m_pClientDE = LTNULL;
	m_bUseIma = LTFALSE;
	m_eMusicLevel = MUSICLEVEL_SILENCE;
	m_bPlayListInitialized = LTFALSE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMusic::Init
//
//	PURPOSE:	Initialize the music
//
// ----------------------------------------------------------------------- //
LTBOOL CMusic::Init( ILTClient *pClientDE, LTBOOL bUseIma )
{
	m_pClientDE = pClientDE;
	m_bUseIma = bUseIma;

	if( m_bUseIma )
	{
		if( !pClientDE->InitMusic( "ima.dll" ))
			return LTFALSE;
		m_pClientDE->RunConsoleString( "+musictype ima" );
	}
	else
	{
		if( !pClientDE->InitMusic( "cdaudio.dll" ))
			return LTFALSE;
		m_pClientDE->RunConsoleString( "+musictype cdaudio" );
	}

	return LTTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMusic::Term
//
//	PURPOSE:	Terminate the music
//
// ----------------------------------------------------------------------- //
void CMusic::Term( )
{
	TermPlayLists( );

	if( m_pClientDE )
	{
		m_pClientDE->StopMusic( MUSIC_IMMEDIATE );
		m_pClientDE = NULL;
	}
}

// ----------------------------------------------------------------------- //
//
//	[MUSIC] Why the playlists didn't build
//	Turned on by 'MusicReport 1'
//
// ----------------------------------------------------------------------- //
static int MusicReport( ILTClient *pClientDE )
{
	if( !pClientDE )
		return 0;

	HCONSOLEVAR hVar = pClientDE->GetConsoleVar( "MusicReport" );
	return hVar ? (int)pClientDE->GetVarValueFloat( hVar ) : 0;
}

static LTBOOL MusicStop( ILTClient *pClientDE, int nReport, const char *pStep )
{
	if( nReport && pClientDE )
		pClientDE->CPrint( "[MUSIC] stopped at %s", pStep );

	return LTFALSE;
}

// The six world properties as the client sees them (ABSENT if never received)
static void MusicDumpProperties( ILTClient *pClientDE )
{
	static const char *s_pProps[] =
	{
		"MusicDirectory", "InstrumentFiles", "AmbientList",
		"CruisingList", "HarddrivingList", "CDTrack"
	};

	for( int i = 0; i < (int)( sizeof( s_pProps ) / sizeof( s_pProps[0] )); i++ )
	{
		char *pValue = GetServerConVarString( s_pProps[i] );
		if( !pValue )
		{
			pClientDE->CPrint( "[MUSIC] %-16s ABSENT from the server console mirror", s_pProps[i] );
			continue;
		}

		pClientDE->CPrint( "[MUSIC] %-16s len=%d '%.72s%s'", s_pProps[i],
			(int)strlen( pValue ), pValue, strlen( pValue ) > 72 ? "..." : "" );
	}
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMusic::Init
//
//	PURPOSE:	Initialize the music
//
// ----------------------------------------------------------------------- //
LTBOOL CMusic::InitPlayLists( )
{
	char tokenSpace[PARSE_MAXTOKENS*(PARSE_MAXTOKENSIZE + 1)];
	char *pTokens[PARSE_MAXTOKENS], *pCommandPos, *pCommand;
	int nArgs;

	int nReport = MusicReport( m_pClientDE );
	if( nReport )
	{
		m_pClientDE->CPrint( "[MUSIC] InitPlayLists: initialised=%d useIma=%d",
			(int)IsInitialized( ), (int)m_bUseIma );
		MusicDumpProperties( m_pClientDE );
	}

	if( !IsInitialized( ))
		return MusicStop( m_pClientDE, nReport, "IsInitialized (CMusic::Init failed! InitMusic could not load the music DLL)" );

	m_bPlayListInitialized = LTFALSE;
	m_eMusicLevel = MUSICLEVEL_SILENCE;

	if( m_bUseIma )
	{
		// The IMA path through ILTClient.
		// It can't fall through to CDTrack, which is empty in every level
		char *pMusicDir = GetServerConVarString( "MusicDirectory" );
		if( !pMusicDir )
			return MusicStop( m_pClientDE, nReport, "MusicDirectory (property never reached the client)" );
		if( !m_pClientDE->SetMusicDirectory( pMusicDir ))
			return MusicStop( m_pClientDE, nReport, "SetMusicDirectory (the engine's music manager refused it)" );

		char *pInstruments = GetServerConVarString( "InstrumentFiles" );
		if( !pInstruments )
			return MusicStop( m_pClientDE, nReport, "InstrumentFiles (property never reached the client)" );

		// LT1's Parse maps onto ConParse.
		// InstrumentFiles is the .dls bank and the first .sty style
		ConParse instParse( pInstruments );
		if( m_pClientDE->Common()->Parse( &instParse ) != LT_OK || instParse.m_nArgs != 2 )
			return MusicStop( m_pClientDE, nReport, "InstrumentFiles parse (expected exactly two tokens)" );
		if( !m_pClientDE->InitInstruments( instParse.m_Args[0], instParse.m_Args[1] ))
			return MusicStop( m_pClientDE, nReport, "InitInstruments" );

		if( !InitTrans( g_szAmbientTrans ))
			return MusicStop( m_pClientDE, nReport, "InitTrans(ambient)" );
		if( !InitTrans( g_szCruisingTrans ))
			return MusicStop( m_pClientDE, nReport, "InitTrans(cruising)" );
		if( !InitTrans( g_szHarddrivingTrans ))
			return MusicStop( m_pClientDE, nReport, "InitTrans(harddriving)" );

		if( !m_pClientDE->AddSongToPlayList( "SilenceList", "s.sec" ))
			return MusicStop( m_pClientDE, nReport, "AddSongToPlayList(SilenceList, s.sec)" );

		if( !InitList( "AmbientList", tokenSpace, pTokens ))
			return MusicStop( m_pClientDE, nReport, "InitList(AmbientList)" );
		if( !InitList( "CruisingList", tokenSpace, pTokens ))
			return MusicStop( m_pClientDE, nReport, "InitList(CruisingList)" );
		if( !InitList( "HarddrivingList", tokenSpace, pTokens ))
			return MusicStop( m_pClientDE, nReport, "InitList(HarddrivingList)" );
	}
	else
	{
		if( !InitList( "CDTrack", tokenSpace, pTokens ))
			return MusicStop( m_pClientDE, nReport, "InitList(CDTrack)" );
	}

	if( nReport )
		m_pClientDE->CPrint( "[MUSIC] InitPlayLists: all playlists built" );

	m_bPlayListInitialized = LTTRUE;
	return LTTRUE;
}

LTBOOL CMusic::InitTrans( char szTransitions[4][PARSE_MAXTOKENSIZE] )
{
	int i;

	for( i = 3; i >= 0; i-- )
	{
		if( !m_pClientDE->LoadSong( szTransitions[i] ))
			return LTFALSE;
	}

	return LTTRUE;
}

LTBOOL CMusic::InitList( char *szWorldProp,
	char argBuffer[PARSE_MAXTOKENS*(PARSE_MAXTOKENSIZE+1)], char * argPointers[PARSE_MAXTOKENS] )
{
	char *pCommand;

	pCommand = GetServerConVarString( szWorldProp );
	if( !pCommand )
		return LTFALSE;

	ConParse parse( pCommand );
	while( m_pClientDE->Common()->Parse( &parse ) == LT_OK )
	{
		for( int i = 0; i < parse.m_nArgs; i++ )
		{
			if( !m_pClientDE->AddSongToPlayList( szWorldProp, parse.m_Args[i] ))
				return LTFALSE;
		}
	}

	return LTTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMusic::TermPlayLists
//
//	PURPOSE:	Unload the playlists and the songs...
//
// ----------------------------------------------------------------------- //
LTBOOL CMusic::TermPlayLists( )
{
	m_bPlayListInitialized = LTFALSE;

	if( !IsInitialized( ))
		return LTFALSE;

	m_pClientDE->DestroyAllSongs( );
	m_eMusicLevel = MUSICLEVEL_SILENCE;
	return LTTRUE;
}
// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMusic::HandleMusicMessage
//
//	PURPOSE:	Parses music message.  The original message was of the format
//				"Music <message_data>".  This function must be passed the string
//				after the "Music " and only the "<message_data>" portion.
//
// ----------------------------------------------------------------------- //
LTBOOL CMusic::HandleMusicMessage( ILTMessage_Read* hMessage )
{
	char msg[51];

	uint8 nCommand = hMessage->Readuint8();

	// One line per music message under 'MusicReport 1'
	if( MusicReport( m_pClientDE ))
		m_pClientDE->CPrint( "[MUSIC] message: command=%d", (int)nCommand );

	switch( nCommand )
	{
		// Handle music level change...
		case MUSICCMD_LEVEL:
		{
			uint8 nLevel;

			nLevel = hMessage->Readuint8();

			return PlayMusicLevel(( EMusicLevel )nLevel );
		}
		// Handle motif
		case MUSICCMD_MOTIF:
		{
			hMessage->ReadString(msg, 50);

			return PlayMotif( msg, LTFALSE );
		}
		case MUSICCMD_MOTIF_LOOP:
		{
			hMessage->ReadString(msg, 50);

			return PlayMotif( msg, LTTRUE );
		}
		case MUSICCMD_MOTIF_STOP:
		{
			hMessage->ReadString(msg, 50);

			StopMotif( msg );
			return LTTRUE;
		}
		case MUSICCMD_BREAK:
		{
			hMessage->ReadString(msg, 50);

			return PlayBreak( msg );
		}
	}
	
	return LTFALSE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMusic::PlayMusicLevel
//
//	PURPOSE:	Sets music level
//
// ----------------------------------------------------------------------- //
LTBOOL CMusic::PlayMusicLevel( EMusicLevel nLevel )
{
	switch( nLevel )
	{
		case MUSICLEVEL_SILENCE:
		{
			switch( m_eMusicLevel )
			{
				case MUSICLEVEL_AMBIENT:
				{
					return TransitionToLevel( g_szAmbientTrans[1], "SilenceList", MUSICLEVEL_SILENCE, LTFALSE );
					break;
				}
				case MUSICLEVEL_CRUISING:
				{
					return TransitionToLevel( g_szCruisingTrans[1], "SilenceList", MUSICLEVEL_SILENCE, LTFALSE );
					break;
				}
				case MUSICLEVEL_HARDDRIVING:
				{
					return TransitionToLevel( g_szHarddrivingTrans[1], "SilenceList", MUSICLEVEL_SILENCE, LTFALSE );
					break;
				}
				case MUSICLEVEL_SILENCE:
				{
					return ( LTBOOL )m_pClientDE->PlayList( "SilenceList", LTNULL, LTTRUE, MUSIC_NEXTSONG );
					break;
				}
			}
			break;
		}
		case MUSICLEVEL_AMBIENT:
		{
			switch( m_eMusicLevel )
			{
				case MUSICLEVEL_SILENCE:
				{
					return TransitionToLevel( g_szAmbientTrans[0], "AmbientList", MUSICLEVEL_AMBIENT, LTTRUE );
					break;
				}
				case MUSICLEVEL_CRUISING:
				{
					return TransitionToLevel( g_szAmbientTrans[2], "AmbientList", MUSICLEVEL_AMBIENT, LTFALSE );
					break;
				}
				case MUSICLEVEL_HARDDRIVING:
				{
					return TransitionToLevel( g_szAmbientTrans[3], "AmbientList", MUSICLEVEL_AMBIENT, LTFALSE );
					break;
				}
			}
			break;
		}
		case MUSICLEVEL_CRUISING:
		{
			switch( m_eMusicLevel )
			{
				case MUSICLEVEL_SILENCE:
				{
					return TransitionToLevel( g_szCruisingTrans[0], "CruisingList", MUSICLEVEL_CRUISING, LTTRUE );
					break;
				}
				case MUSICLEVEL_AMBIENT:
				{
					return TransitionToLevel( g_szCruisingTrans[2], "CruisingList", MUSICLEVEL_CRUISING, LTFALSE );
					break;
				}
				case MUSICLEVEL_HARDDRIVING:
				{
					return TransitionToLevel( g_szCruisingTrans[3], "CruisingList", MUSICLEVEL_CRUISING, LTFALSE );
					break;
				}
			}
			break;
		}
		case MUSICLEVEL_HARDDRIVING:
		{
			switch( m_eMusicLevel )
			{
				case MUSICLEVEL_SILENCE:
				{
					return TransitionToLevel( g_szHarddrivingTrans[0], "HarddrivingList", MUSICLEVEL_HARDDRIVING, LTTRUE );
					break;
				}
				case MUSICLEVEL_AMBIENT:
				{
					return TransitionToLevel( g_szHarddrivingTrans[2], "HarddrivingList", MUSICLEVEL_HARDDRIVING, LTFALSE );
					break;
				}
				case MUSICLEVEL_CRUISING:
				{
					return TransitionToLevel( g_szHarddrivingTrans[3], "HarddrivingList", MUSICLEVEL_HARDDRIVING, LTFALSE );
					break;
				}
			}
			break;
		}
	}

	return LTFALSE;
}

LTBOOL CMusic::TransitionToLevel( char *szTransition, char *szList, EMusicLevel eLevel, LTBOOL bImmediate )
{
	m_eMusicLevel = eLevel;

	if( !m_bUseIma || !IsPlaylistInitialized( ))
		return LTTRUE;

	if( szTransition[0] == 0 )
		return LTFALSE;

	return ( LTBOOL )m_pClientDE->PlayList( szList, szTransition, LTTRUE, bImmediate ? MUSIC_IMMEDIATE : MUSIC_NEXTSONG );
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMusic::PlayMotif
//
//	PURPOSE:	Plays motif
//
// ----------------------------------------------------------------------- //
LTBOOL CMusic::PlayMotif( char *pMotifName, LTBOOL bLoop )
{
	if( !m_bUseIma )
		return LTTRUE;

	return LTTRUE;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMusic::StopMotif
//
//	PURPOSE:	Stops a looping motif
//
// ----------------------------------------------------------------------- //
void CMusic::StopMotif( char *pMotifName )
{
	if( !m_bUseIma )
		return;
}

// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CMusic::PlayBreak
//
//	PURPOSE:	Plays break
//
// ----------------------------------------------------------------------- //
LTBOOL CMusic::PlayBreak( char *pBreakName )
{
	if( !m_bUseIma )
		return LTTRUE;

	return LTTRUE;
}
