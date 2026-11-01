//------------------------------------------------------------------
//	FILE	  : console_impl.cpp
//	PURPOSE	  : Implementation for the console.
//	CREATED	  : November 4 1999
// ------------------------------------------------------------------ //

#include "bdefs.h"

#include "winconsole_impl.h"
#include "consolecommands.h"
#include "dsys_interface.h"
#include "streamsim.h"
#include "load_pcx.h"
#include "resource.h"
#include "sysclientde_impl.h"
#include "clientmgr.h"
#include "clientde_impl.h"
#include "iltclient.h"
#include "renderstruct.h"
#include "iltfontmanager.h"
#include "ilttexinterface.h"
#include "debuggeometry.h"
#include "sysdrawprim.h"

//------------------------------------------------------------------
//------------------------------------------------------------------
// Holders and their headers.
//------------------------------------------------------------------
//------------------------------------------------------------------

//a IClientFormatMgr interface
#include "client_formatmgr.h"
static IClientFormatMgr *format_mgr;
define_holder(IClientFormatMgr, format_mgr);

//the ILTClient game interface
#include "iltclient.h"
static ILTClient *ilt_client;
define_holder(ILTClient, ilt_client);

static ILTDrawPrim* g_pILTDrawPrim;
define_holder_to_instance(ILTDrawPrim, g_pILTDrawPrim, Internal);
static ILTTexInterface* g_ITexInterface;
define_holder(ILTTexInterface, g_ITexInterface);
static ILTFontManager*  g_IFontManager;
define_holder(ILTFontManager, g_IFontManager);

#include "iclientshell.h"
static IClientShell *i_client_shell;
define_holder(IClientShell, i_client_shell);

// Constants
#define SEPERATOR_CHARACTERS " .()\""
#define SCROLL_INDICATOR_UP " \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/  \\/ "

extern int32 g_CV_TraceConsole;
extern const char* g_CV_Console_FontTexFile;
extern const char* g_CV_Console_FontFace;
extern int32 g_CV_ConsoleFontSize;
	
// The global console variable	
CConsole g_Console;

// Default empty iterator
CConIterator g_ConEmptyIterator;

// Convenience functions...
inline bool IsKeyDown( uint16 vkKey ) 
{
	return ((GetAsyncKeyState( VK_CONTROL ) & 0x8000) != 0);
}

inline ConsoleState *GetConsoleState()
{
	return &g_ClientConsoleState;
}

// Console Variables
extern int32 g_CV_ConsoleHistoryLen;
extern int32 g_CV_ConsoleBufferLen;
extern float g_CV_ConsoleAlpha;
extern float g_CV_ConsoleFillAlpha;
extern int32 g_CV_ConsoleLeft;
extern int32 g_CV_ConsoleTop;
extern int32 g_CV_ConsoleRight;
extern int32 g_CV_ConsoleBottom;

// ------------------------------------------------------------------ //
// CConIterator functionality
// ------------------------------------------------------------------ //
bool CConIterator::First( const char *pValue )
{
	// Go to the beginning
	if ( !Begin() )
		return false;

	// Jump out on an empty string
	if ( (!pValue) || (!*pValue) )
		return true;

	// Check for the first one being the one we're looking for
	const char *pCur = Get();
	if ( !pCur )
		return false;

	if ( strnicmp( pValue, pCur, strlen( pValue ) ) == 0 )
		return true;

	// Find the first value
	return Next( pValue );
}

bool CConIterator::Last( const char *pValue )
{
	// Go to the ending
	if ( !End() )
		return false;

	// Jump out on an empty string
	if ( (!pValue) || (!*pValue) )
		return true;

	// Check for the last one being the one we're looking for
	const char *pCur = Get();
	if ( !pCur )
		return false;

	if ( strnicmp( pValue, pCur, strlen( pValue ) ) == 0 )
		return true;

	// Find the first value
	return Prev( pValue );
}

bool CConIterator::Next( const char *pValue )
{
	if ( !pValue )
		return NextItem();

	int iLength = (int)strlen( pValue );

	bool bFound = false;

	while ( !bFound )
	{
		if ( !NextItem() )
			break;

		const char *pCur = Get();
		if ( !pCur )
			break;

		bFound = strnicmp( pValue, pCur, iLength ) == 0;
	}

	return bFound;
}

bool CConIterator::Prev( const char *pValue )
{
	if ( !pValue )
		return PrevItem();

	int iLength = (int)strlen( pValue );

	bool bFound = false;

	while ( !bFound )
	{
		if ( !PrevItem() )
			break;

		const char *pCur = Get();
		if ( !pCur )
			break;

		bFound = strnicmp( pValue, pCur, iLength ) == 0;
	}

	return bFound;
}

// ------------------------------------------------------------------ //
// CConCommandBox functionality
// ------------------------------------------------------------------ //
CConCommandBox::CConCommandBox()
{
	m_TextColor = CONRGB( 180, 180, 255 );
	
	m_BackColor = CONRGB( 30, 30, 30 );
	m_BorderColor = CONRGB( 255, 255, 255 );

	m_pConsole = LTNULL;
	
	m_FlashTime = 500;

	Clear();
}

void CConCommandBox::Term()
{
	SetConsole( LTNULL );
}

void CConCommandBox::Draw()
{
	LTRect rect;
	uint32 flashTime;
	char *caretStr = "_";
	COLORREF caretColor;

	if ( !GetConsole() || !GetConsole()->GetRenderStruct() )
		return;

	rect = GetRect(); 
	//rect.left += 5;
	//rect.right -= 5;

	// Draw the text line.
	GetConsole()->DrawTextLine( GetCommand(), &rect, GetTextColor() );

	// Draw the caret.
	rect.left += GetConsole()->CalcStringOffset( GetCommand(), m_iCursorPos );

	flashTime = timeGetTime() % (GetFlashTime() * 2);
	if ( flashTime > GetFlashTime() )
	{
		flashTime = GetFlashTime() - (flashTime - GetFlashTime());
	}

	caretColor = CONRGB( (flashTime * 255) / GetFlashTime(), 100, 100 );

	GetConsole()->DrawTextLine( caretStr, &rect, caretColor );
}

void CConCommandBox::Clear()
{
	m_CurCommand[0] = 0;
	m_iCurLength = 0;
	MoveCursor(0);
}

void CConCommandBox::SetCommand( const char *pCommand )
{ 
	if ( !pCommand )
		Clear();
	else
	{
		// Copy the string
		LTStrCpy( m_CurCommand, pCommand, MAX_CONSOLE_TEXTLEN );
		m_iCurLength = (int)strlen( m_CurCommand );
		// Move to the end of the string
		MoveCursor( m_iCurLength );
	}
}

void CConCommandBox::DeleteChar( int iOffset )
{
	// Find the position
	int iPosition = m_iCursorPos + iOffset;

	// Restrict to the contents of the string
	if ( (iPosition < 0) || (iPosition >= m_iCurLength) )
		return;

	// Delete the character
	strcpy( &m_CurCommand[iPosition], &m_CurCommand[iPosition + 1] );
	m_iCurLength--;
}

void CConCommandBox::DeleteWord( int iOffset )
{
	bool bSeperator = (iOffset < 0);
	for ( int iLoop = 0; iLoop < 2; iLoop++)
	{
		// Skip over the next set of seperators or characters
		while ( ((m_iCursorPos + iOffset) >= 0) && ((m_iCursorPos + iOffset) < m_iCurLength) &&
			((strchr( SEPERATOR_CHARACTERS, m_CurCommand[m_iCursorPos + iOffset] ) == 0) ^ bSeperator) )
		{
			DeleteChar( iOffset );
			m_iCursorPos += iOffset;
		}
		bSeperator = !bSeperator;
	}
}

void CConCommandBox::AddChar( char key )
{
	// Don't add past the end of the string
	if ( m_iCursorPos >= (MAX_CONSOLE_TEXTLEN - 1) )
		return;

	// Don't allow the string to get too big
	if ( m_iCurLength >= (MAX_CONSOLE_TEXTLEN - 1) )
	{
		m_iCurLength--;
		m_CurCommand[m_iCurLength] = 0;
	}

	// Make room if necessary
	if ( m_iCursorPos < m_iCurLength )
		memmove( &m_CurCommand[m_iCursorPos + 1], &m_CurCommand[m_iCursorPos], (m_iCurLength - m_iCursorPos) + 1 );
	// Otherwise make sure it's terminated
	else
		m_CurCommand[m_iCurLength + 1] = 0;

	// Put the character in the string
	m_CurCommand[m_iCursorPos] = key;

	// Update the length
	m_iCurLength++;
}

void CConCommandBox::MoveCursor(int iOffset)
{
	m_iCursorPos = max(min(m_iCursorPos + iOffset, m_iCurLength), 0);
}

void CConCommandBox::MoveWord( int iOffset )
{
	bool bSeperator = (iOffset < 0);
	for ( int iLoop = 0; iLoop < 2; iLoop++)
	{
		// Skip over the next set of seperators or characters
		while ( ((m_iCursorPos + iOffset) >= 0) && ((m_iCursorPos + iOffset) <= m_iCurLength) &&
			((strchr( SEPERATOR_CHARACTERS, m_CurCommand[m_iCursorPos + iOffset] ) == 0) ^ bSeperator) )
			m_iCursorPos += iOffset;
		bSeperator = !bSeperator;
	}
	// Make sure we end up on a non-seperator
	while ( ((m_iCursorPos + iOffset) >= 0) && ((m_iCursorPos + iOffset) <= m_iCurLength) &&
		(strchr( SEPERATOR_CHARACTERS, m_CurCommand[m_iCursorPos] ) != 0) )
		m_iCursorPos += iOffset;
}

char CConCommandBox::TranslateKey(uint32 key) const
{
	char aResult[2];
	uint8 aKeyState[256];
	if ( !GetKeyboardState( aKeyState ) )
		return 0;

	switch ( ToAscii( key, 0, aKeyState, (LPWORD)aResult, 0 ) )
	{
		// No translation available
		case 0 : 
			return 0;
		// 1 character translated
		case 1 :
			return aResult[0];
		// 2 characters required for translation
		case 2 : 
			return aResult[1];
		// Documentation says this should never happen...
		default :
			return 0;
	}
}

void CConCommandBox::OnKeyPress(uint32 key)
{
	switch ( key )
	{
		case VK_ESCAPE :
			Clear();
			break;
		case VK_LEFT :
			if ( IsKeyDown( VK_CONTROL ) )
				// Ctrl+Left = word left
				MoveWord( -1 );
			else
				MoveCursor( -1 );
			break;
		case VK_RIGHT :
			if ( IsKeyDown( VK_CONTROL ) )
				// Ctrl+Right = word right
				MoveWord( 1 );
			else
				MoveCursor( 1 );
			break;
		case VK_HOME :
			MoveCursor( -m_iCursorPos );
			break;
		case VK_END :
			if ( IsKeyDown( VK_CONTROL ) )
				// Ctrl+End = delete the rest of the buffer
				AddChar( 0 );
			else
				MoveCursor( m_iCurLength );
			break;
		case VK_BACK : 
			if ( IsKeyDown( VK_CONTROL ) )
				// Ctrl+Backspace = delete the next word
				DeleteWord( -1 );
			else
			{
				DeleteChar( -1 );
				MoveCursor( -1 );
			}
			break;
		case VK_DELETE :
			if ( IsKeyDown( VK_CONTROL ) )
				// Ctrl+Delete = delete the next word
				DeleteWord( 0 );
			else
				DeleteChar( 0 );
			break;
		default :
			char chChar = TranslateKey( key );
			if (chChar)
			{
				AddChar( chChar );
				MoveCursor( 1 );
			}
			break;
	}
}

// ------------------------------------------------------------------ //
// CConHistory functionality
// ------------------------------------------------------------------ //
CConHistory::CConHistory( int iListSize ) :
	m_pLines(LTNULL),
	m_iIndex(0),
	m_iListSize(0),
	m_iListStart(0),
	m_iListEnd(0)
{
	Resize( iListSize );
}

CConHistory::~CConHistory()
{
	Resize( 0 );
}

bool CConHistory::Begin()
{
	m_iIndex = m_iListStart;
	return m_iIndex != m_iListEnd;
}

bool CConHistory::End()
{
	m_iIndex = m_iListEnd;
	return PrevItem();
}

bool CConHistory::NextItem()
{
	if ( m_iIndex == m_iListEnd )
		return false;
	m_iIndex++;
	if ( m_iIndex >= m_iListSize )
		m_iIndex = 0;
	return m_iIndex != m_iListEnd;
}

bool CConHistory::PrevItem()
{
	if ( m_iIndex == m_iListStart )
		return false;
	m_iIndex--;
	if ( m_iIndex < 0 )
		m_iIndex = m_iListSize - 1;
	return true;
}

int CConHistory::Remove( const char *pString )
{
	// You haven't removed any yet...
	int iResult = 0;
	// Begin at the beginning
	int iCurEntry = m_iListStart;
	// Continue until the ending
	while ( iCurEntry != m_iListEnd )
	{
		// Is this the droid you're looking for?
		if ( strnicmp( pString, m_pLines[iCurEntry].m_Text, MAX_CONSOLE_TEXTLEN ) == 0 )
		{
			// Remove this entry
			Remove( iCurEntry );
			// Count it
			++iResult;
		}
		else
			// Move along
			iCurEntry = ( iCurEntry + 1 ) % m_iListSize;
	}

	// Tell the interested parties what happened
	return iResult;
}

void CConHistory::Remove( int iIndex )
{
	// Go over the list...
	for ( int iCurEntry = iIndex; iCurEntry != m_iListEnd; iCurEntry = (iCurEntry + 1) % m_iListSize )
	{
		// Who's next?
		int iNextEntry = (iCurEntry + 1) % m_iListSize;
		// Clean cup, move down..
		LTStrnCpy( m_pLines[iCurEntry].m_Text, m_pLines[iNextEntry].m_Text, MAX_CONSOLE_TEXTLEN, MAX_CONSOLE_TEXTLEN-1 );
	}
	// Adjust the end
	m_iListEnd = ((m_iListEnd > 0) ? m_iListEnd : m_iListSize) - 1;
}


void CConHistory::Add( const char *pString, bool bUnique )
{
	// Make sure the list exists
	if ( !m_iListSize )
		return;
	// Make sure it's a unique entry in the history
	if ( bUnique )
		Remove( pString );
	// Copy the string
	LTStrCpy( m_pLines[m_iListEnd].m_Text, pString, MAX_CONSOLE_TEXTLEN );
	// Move the end of the list
	m_iListEnd++;
	if ( m_iListEnd >= m_iListSize )
		m_iListEnd = 0;
	// Move the beginning of the list if necessary
	if ( m_iListStart == m_iListEnd )
	{
		m_iListStart++;
		if ( m_iListStart >= m_iListSize )
			m_iListStart = 0;
	}
}

// Resize the history list
bool CConHistory::Resize( int iSize )
{
	// Jump out if it's already the right size
	if ( m_iListSize == (iSize + 1) )
		return true;

	CConTextLine *pNewList = LTNULL;

	// Allocate a new buffer
	if ( iSize )
	{
		iSize++;
		LT_MEM_TRACK_ALLOC(pNewList = new CConTextLine[iSize],LT_MEM_TYPE_CONSOLE);
		if ( !pNewList )
			return false;

		// Move over the old lines
		if ( End() )
		{
			int iIndex = iSize - 1;
			do
			{
				const char *pLine = Get();
				if ( !pLine )
					break;
				LTStrCpy( pNewList[iIndex].m_Text, pLine, MAX_CONSOLE_TEXTLEN );
			} while ( (--iIndex) && (PrevItem()) );
			m_iListStart = iIndex + 1;
			m_iListEnd = 0;
		}
		else
			m_iListStart = m_iListEnd = 0;
	}
	else
		m_iListStart = m_iListEnd = 0;

	// Get rid of the old lines
	if ( m_iListSize )
		delete [] m_pLines;

	// Update the list buffer variables
	m_iListSize = iSize;
	m_pLines = pNewList;

	return true;
}

const char *CConHistory::Get() const 
{ 
	if ( (m_iIndex < 0) || (m_iIndex >= m_iListSize) || (m_iIndex == m_iListEnd) )
		return LTNULL;

	return &(m_pLines[m_iIndex].m_Text[0]); 
}

int CConHistory::GetIndex() const
{
	int iResult = m_iIndex - m_iListStart;
	if ( iResult < 0 )
		iResult += m_iListSize;
	return iResult;
}

bool CConHistory::MoveTo( int iIndex )
{
	// Guard against the obvious
	if ( iIndex < 0 )
		return false;

	bool bResult = false;

	iIndex += m_iListStart;
	// Handle the new index being before the bottom of the list or the list not wrapping
	if ( (iIndex < m_iListSize) || (m_iListEnd > m_iListStart) )
		bResult = (iIndex < m_iListEnd) || (m_iListEnd < m_iListStart);
	// Handle the new index being after the bottom of the list
	else
	{
		iIndex -= m_iListSize;
		bResult = iIndex < m_iListEnd;
	}
	if ( bResult )
		m_iIndex = iIndex;
	return bResult;
}

// ------------------------------------------------------------------ //
// CConsole functionality
// ------------------------------------------------------------------ //

CConsole::CConsole()
{
	m_pFontBitmapData = LTNULL;

	m_hWnd = 0;
	m_CommandHandler = LTNULL;
	m_FilterLevel = 1;
	
	m_BackColor = CONRGB( 0, 0, 0 );
	m_BorderColor = CONRGB( 255, 255, 255 );

	m_FontHeight = 1;
	
	m_pStruct = LTNULL;
	m_ErrorLogFn = LTNULL;
	m_hBackground = LTNULL;
	m_fBackgroundAlpha = 1.0f;
	m_bBackgroundOptimized = false;
	m_BackTex = NULL;
	m_bBackgroundTried = false;

	m_pCompletionIterator = &g_ConEmptyIterator;

	m_bInitialized = true;
	m_bInitTerminate = false;

	m_Tex = NULL;
	m_Font = NULL;
	m_bFontCreateFailed = false;
	m_nFontPixels = 0;
}

CConsole::~CConsole()
{ 
	Term( true ); 

	m_bInitialized = false;
}

bool CConsole::Init(const LTRect *pRect, CommandHandler handler, RenderStruct *pStruct, CConIterator *pCompletionIterator )
{
	HWND hWnd = (HWND)dsi_GetMainWindow();

	if ( m_bInitTerminate )
		Term( false );
	m_bInitTerminate = true;

	if (!InitFont()) {
		Term(true);
		return false; }

	m_hWnd = hWnd;
	// Note : This has no effect now that the window position is in a console variable
	m_ScrRect = *pRect;
	m_CommandHandler = handler;
	m_pStruct = pStruct;

	m_iScrollOffset = 0;

	if ( !m_TextLines.GetSize() )
		ResizeTextLines( 200 );

	// Setup the command box.
	GetCommandBox()->SetConsole( this );

	if ( !m_cHistory.GetSize() )
		m_cHistory.Resize( 20 );

	if ( pCompletionIterator )
		SetCompletionIterator( pCompletionIterator );

	// Read in the variable states
	CheckVariables();

	return true;
}

bool CConsole::InitBare()
{
	if ( !m_TextLines.GetSize() )
		ResizeTextLines( 200 );
	return true;
}

void CConsole::Term(bool bDeleteTextLines)
{
	FreeBackground();

	if ( bDeleteTextLines )
	{
		GDeleteAndRemoveElements( m_TextLines );
		m_cHistory.Resize( 0 );
	}

	TermFont();
	GetCommandBox()->Term();
	
	m_hWnd = 0;
	m_CommandHandler = LTNULL;
	m_pStruct = LTNULL;

	if (g_IFontManager && m_Font)	{ 
		CDebugGeometry& dgPerf = getDebugGeometry(); dgPerf.clear();	// Make sure all the debug text (shares this font) is destroyed...
		g_IFontManager->DestroyFont(m_Font); m_Font = 0; }
	m_nFontPixels = 0;
	if (m_Tex)						{ g_ITexInterface->ReleaseTextureHandle(m_Tex); m_Tex = 0; }

	m_bBackgroundTried = false;
	m_bInitTerminate = false;
}

void CConsole::FreeBackground()
{
	if ( m_BackTex )
	{
		g_ITexInterface->ReleaseTextureHandle( m_BackTex );
		m_BackTex = NULL;
	}

	if ( !m_hBackground )
		return;

	cis_DeleteSurface( m_hBackground );
	m_hBackground = LTNULL;
}


bool CConsole::InitFont()
{
	m_FontHeight = 9;

	for (uint32 i = 0; i < NUM_CONSOLE_CHARACTERS; ++i)
		m_CharWidths[i] = (uint16)7;

	return true;
}

void CConsole::TermFont()
{
	if (m_pFontBitmapData) {
		delete m_pFontBitmapData;
		m_pFontBitmapData = LTNULL; }

	if (m_Font) {
		g_IFontManager->DestroyFont(m_Font);
		m_Font = NULL;
	}
}

bool CConsole::ResizeHistory( int iNewSize )
{
	if ( m_cHistory.GetSize() == iNewSize )
		return true;

	bool bResult = m_cHistory.Resize( iNewSize );
	return bResult;
}

bool CConsole::ResizeTextLines( uint32 nNewSize )
{
	// Jump out if the number of lines hasn't changed
	if ( m_TextLines.GetSize() == nNewSize )
		return true;

	CConTextLine	*pLine;

	while ( m_TextLines.GetSize() < nNewSize )
	{
		LT_MEM_TRACK_ALLOC(pLine = new CConTextLine,LT_MEM_TYPE_CONSOLE);
		
		memset( pLine, 0, sizeof(CConTextLine) );
		m_TextLines.AddHead( pLine );
	}

	while ( m_TextLines.GetSize() > nNewSize )
		delete m_TextLines.RemoveHead();

	return true;
}

void CConsole::FinishCommand()
{
	EndNav();

	char *pCurCommand = (char *)(GetCommandBox()->GetCommand());
	if (pCurCommand[0] == 0)
		return;

	PrintString( GetCommandBox()->GetTextColor(), 0, pCurCommand );

	m_cHistory.Add( pCurCommand );

	if ( m_CommandHandler )
		m_CommandHandler( pCurCommand );

	GetCommandBox()->Clear();
}

void CConsole::CheckVariables()
{
	// Read from the console's variables
	SetBackgroundAlpha( g_CV_ConsoleAlpha );
	// Note : This makes sure the console buffer always has at least 10 lines in it..
	ResizeTextLines( (uint32)LTMAX( g_CV_ConsoleBufferLen , 10 ) );
	m_cHistory.Resize( (uint32)LTMAX( g_CV_ConsoleHistoryLen, 5 ) );
	// Update the window rectangle
	LTRect cRect;
	cRect.top = g_CV_ConsoleTop;
	cRect.left = g_CV_ConsoleLeft;
	cRect.bottom = g_CV_ConsoleBottom;
	cRect.right = g_CV_ConsoleRight;
	SetRect( cRect );
}

void CConsole::CycleCommands(int iCount)
{
	if (!iCount)
		return;

	bool bForward = iCount > 0;
	iCount = abs( iCount );

	// Pull up the first entry in the history list
	if ( GetState() != STATE_HISTORY )
	{
		// Go to the first match in the history list
		bool bMatch;
		if ( bForward )
			bMatch = m_cHistory.First( GetCommandBox()->GetCommand() );
		else
			bMatch = m_cHistory.Last( GetCommandBox()->GetCommand() );

		if ( !bMatch )
			return;
	
		StartNav( STATE_HISTORY );
		iCount--;
	}

	bool bContinue = true;
	// Pull up the next entry in the history list
	while ( iCount && bContinue )
	{
		if ( bForward )
			bContinue = m_cHistory.Next( m_aNavCommand );
		else
			bContinue = m_cHistory.Prev( m_aNavCommand );
		iCount--;
	}

	if ( !bContinue )
	{
		// End navigation mode
		GetCommandBox()->SetCommand( m_aNavCommand );
		EndNav();
	}
	else
		GetCommandBox()->SetCommand( m_cHistory.Get() );
}

void CConsole::SetCompletionIterator(CConIterator *pIterator)
{
	if ( !pIterator )
		m_pCompletionIterator = &g_ConEmptyIterator;
	else
		m_pCompletionIterator = pIterator;
}

void CConsole::NextCommand()
{
	// Start the iterator if we're not navigating yet
	if ( GetState() != STATE_COMPLETE )
	{
		// Find the first match
		if ( !GetCompletionIterator()->First( GetCommandBox()->GetCommand() ) )
			return;
	
		// Go into navigation mode
		StartNav( STATE_COMPLETE );
	}
	// Otherwise find the next one
	else if ( !GetCompletionIterator()->Next( m_aNavCommand ) )
	{
		// Start over if we get to the end
		GetCommandBox()->SetCommand( m_aNavCommand );
		EndNav();
		return;
	}

	// Show the new item
	GetCommandBox()->SetCommand( GetCompletionIterator()->Get() );
}

void CConsole::PrevCommand()
{
	// Start the iterator if we're not navigating yet
	if ( GetState() != STATE_COMPLETE )
	{
		// Find the first match
		if ( !GetCompletionIterator()->Last( GetCommandBox()->GetCommand() ) )
			return;
	
		// Go into navigation mode
		StartNav( STATE_COMPLETE );
	}
	// Otherwise find the previous one
	else if ( !GetCompletionIterator()->Prev( m_aNavCommand ) )
	{
		// Start over if we get to the beginning
		GetCommandBox()->SetCommand( m_aNavCommand );
		EndNav();
		return;
	}

	// Show the new item
	GetCommandBox()->SetCommand( GetCompletionIterator()->Get() );
}

void CConsole::MatchCommands()
{
	// Display a list of what matches
	bool bContinue = GetCompletionIterator()->First( GetCommandBox()->GetCommand() );
	if ( bContinue )
		Printf( GetCommandBox()->GetTextColor(), 0, ">%s*>", GetCommandBox()->GetCommand() );
	while ( bContinue )
	{
		char *pString = (char *)GetCompletionIterator()->Get();
		if ( !pString )
			break;
		PrintString( GetCommandBox()->GetTextColor(), 0, pString );
		bContinue = GetCompletionIterator()->Next( GetCommandBox()->GetCommand() );
	}
}

void CConsole::StartNav( EConState eState )
{
	// Jump out if we're already navigating
	if ( GetState() == eState )
		return;

	// Make a copy of whatever's in the command buffer
	LTStrCpy( m_aNavCommand, GetCommandBox()->GetCommand(), MAX_CONSOLE_TEXTLEN );

	// Change our state
	SetState( eState );
}

void CConsole::EndNav()
{
	// Jump out if we're not navigating
	if ( GetState() == STATE_NORMAL )
		return;

	// Change our state
	SetState( STATE_NORMAL );
}

LTRESULT CConsole::LoadBackground()
{
	ILTStream *pStream;
	LoadedBitmap bitmap;
	LTRESULT dResult;
	
	FreeBackground();

	dResult = LT_ERROR;
	if ( (pStream = streamsim_Open("console.pcx", "rb")) == LTNULL )
	{
		// Else the one in the launcher's data folder, a standard install
		extern const char* g_CV_GameDataDir;
		if ( !g_CV_GameDataDir || !g_CV_GameDataDir[0] )
			return dResult;
		char szPath[512];
		LTSNPrintF(szPath, sizeof(szPath), "%s\\console.pcx", g_CV_GameDataDir);
		if ( (pStream = streamsim_Open(szPath, "rb")) == LTNULL )
			return dResult;
	}

	// A texture rather than LT1's surface. The console draws inside End3D, where a scaled surface blit drops
	// its size, while a textured quad scales anywhere and can be blended.
	// The shipped images are 8 bit paletted (Shogo, Blood 2, KISS and AVP2), and anything else keeps the plain fill
	if ( pcx_Create2(pStream, &bitmap) && bitmap.m_Format.GetType() == BPP_8P &&
		 bitmap.m_Width && bitmap.m_Height && g_ITexInterface )
	{
		const uint32 nWidth = bitmap.m_Width, nHeight = bitmap.m_Height;
		uint32 *pPixels = LTNULL;
		LT_MEM_TRACK_ALLOC(pPixels = new uint32[nWidth * nHeight], LT_MEM_TYPE_MISC);
		if ( pPixels )
		{
			for ( uint32 y = 0; y < nHeight; ++y )
			{
				for ( uint32 x = 0; x < nWidth; ++x )
				{
					const RPaletteColor &c = bitmap.m_Palette[ bitmap.Pixel(x, y) ];
					pPixels[y * nWidth + x] = 0xFF000000 | ((uint32)c.rgb.r << 16) | ((uint32)c.rgb.g << 8) | (uint32)c.rgb.b;
				}
			}

			if ( g_ITexInterface->CreateTextureFromData(m_BackTex, TEXTURETYPE_ARGB8888, 0,
					(uint8*)pPixels, nWidth, nHeight, 1) == LT_OK && m_BackTex )
				dResult = LT_OK;
			else
				m_BackTex = NULL;

			delete [] pPixels;
		}
	}

	pStream->Release();

	return dResult;
}

void CConsole::SetBackgroundAlpha( float fValue )
{
	if ( !m_hBackground )
		return;

	m_fBackgroundAlpha = fValue; 

	if ( m_bBackgroundOptimized != LT_OK )
		m_bBackgroundOptimized = ilt_client->OptimizeSurface(m_hBackground, RGB(0,0,0)) != 0;

	ilt_client->SetSurfaceAlpha( m_hBackground, fValue );
}

int CConsole::Scroll( int iOffset )
{
	m_iScrollOffset = min( max( m_iScrollOffset + iOffset, 0 ), (int)(m_TextLines.GetSize() - m_nTextLines + 1));
	return m_iScrollOffset;
}

inline bool CConsole::GetTextLineBox(uint32 iLine, LTRect *pRect, bool bScreen)
{
	LTRect cRect;
	if (bScreen)
		cRect = m_ScrRect;
	else
		CalcRect( cRect );
	pRect->top = cRect.top + (iLine * m_FontHeight) + CONSOLE_TOP_BORDER;
	pRect->bottom = cRect.top + ((iLine+1) * m_FontHeight) + 1;
	pRect->left = cRect.left + CONSOLE_LEFT_BORDER;
	pRect->right = cRect.right - 1;

	if ( ((pRect->bottom - pRect->top) > 0) && ((pRect->right - pRect->left) > 0) )
		return true;
	else
		return false;
}


void CConsole::DrawTextLines(int nTextLines, bool bScreen)
{
	if ( !m_pStruct )
		return;
	
	GPOS	pos;
	LTRect	rect;
	uint32	iLine=nTextLines;
	CConTextLine	*pLine;

	if ( m_pStruct )
	{
		pos = m_TextLines.GetTail();
		// Skip the scrolling offset
		int iCount = m_iScrollOffset;
		while ( pos && iCount )
		{
			m_TextLines.GetPrev( pos );
			iCount--;
		}

		// Draw a scroll indicator if necessary
		if ( m_iScrollOffset )
		{
			iLine--;
			if ( GetTextLineBox( iLine, &rect, bScreen ) )
				DrawTextLine( SCROLL_INDICATOR_UP, &rect, CONRGB( 192, 192, 255 ) );
		}

		// Draw the lines
		while ( pos && iLine )
		{
			pLine = m_TextLines.GetPrev( pos );

			iLine--;

			if ( GetTextLineBox( iLine, &rect, bScreen ) )
				DrawTextLine( pLine->m_Text, &rect, pLine->m_Color );
		}
	}
}

#ifndef RGBA_GETA
#define RGBA_GETA(color)			((uint8)( color               >> 24))
#define RGBA_GETR(color)			((uint8)((color & 0x00FF0000) >> 16))
#define RGBA_GETG(color)			((uint8)((color & 0x0000FF00) >> 8 ))
#define RGBA_GETB(color)			((uint8)((color & 0x000000FF)      ))
#endif

// The game's console.pcx stretched over the console as LT1 drew it, but blended at ConsoleFillAlpha
// where LT1's was opaque
void CConsole::TexturedRectangle(SharedTexture* pTex, float fAlpha, LTRect rect)
{
	if (fAlpha < 0.0f) fAlpha = 0.0f;
	if (fAlpha > 1.0f) fAlpha = 1.0f;

	g_pILTDrawPrim->SetTexture(pTex);
	g_pILTDrawPrim->SetTransformType(DRAWPRIM_TRANSFORM_SCREEN);
	g_pILTDrawPrim->SetColorOp(DRAWPRIM_MODULATE);
	g_pILTDrawPrim->SetAlphaBlendMode((fAlpha < 1.0f) ? DRAWPRIM_BLEND_MOD_SRCALPHA : DRAWPRIM_NOBLEND);
	g_pILTDrawPrim->SetZBufferMode(DRAWPRIM_NOZ);
	g_pILTDrawPrim->SetAlphaTestMode(DRAWPRIM_NOALPHATEST);
	g_pILTDrawPrim->SetClipMode(DRAWPRIM_FASTCLIP);
	g_pILTDrawPrim->SetFillMode(DRAWPRIM_FILL);
	g_pILTDrawPrim->SetCullMode(DRAWPRIM_CULL_NONE);

	LT_POLYFT4 Poly;
	Poly.rgba.r = 255; Poly.rgba.g = 255; Poly.rgba.b = 255;
	Poly.rgba.a = (uint8)((fAlpha * 255.0f) + 0.5f);
	Poly.verts[0].x = (float)rect.left;  Poly.verts[0].y = (float)rect.top;    Poly.verts[0].z = SCREEN_NEAR_Z; Poly.verts[0].u = 0.0f; Poly.verts[0].v = 0.0f;
	Poly.verts[1].x = (float)rect.right; Poly.verts[1].y = (float)rect.top;    Poly.verts[1].z = SCREEN_NEAR_Z; Poly.verts[1].u = 1.0f; Poly.verts[1].v = 0.0f;
	Poly.verts[2].x = (float)rect.right; Poly.verts[2].y = (float)rect.bottom; Poly.verts[2].z = SCREEN_NEAR_Z; Poly.verts[2].u = 1.0f; Poly.verts[2].v = 1.0f;
	Poly.verts[3].x = (float)rect.left;  Poly.verts[3].y = (float)rect.bottom; Poly.verts[3].z = SCREEN_NEAR_Z; Poly.verts[3].u = 0.0f; Poly.verts[3].v = 1.0f;
	g_pILTDrawPrim->DrawPrim(&Poly, 1);

	g_pILTDrawPrim->SetTexture(NULL);
	g_pILTDrawPrim->SetAlphaBlendMode(DRAWPRIM_NOBLEND);
}

// The console's plain background, with no border and blended at ConsoleFillAlpha.
// The color's own alpha is ignored, since the console colors are built opaque
void CConsole::FilledRectangle(uint32 fillColor, float fAlpha, LTRect rect)
{
	if (fAlpha < 0.0f) fAlpha = 0.0f;
	if (fAlpha > 1.0f) fAlpha = 1.0f;

	g_pILTDrawPrim->SetZBufferMode(DRAWPRIM_NOZ);
	g_pILTDrawPrim->SetAlphaBlendMode((fAlpha < 1.0f) ? DRAWPRIM_BLEND_MOD_SRCALPHA : DRAWPRIM_NOBLEND);
	g_pILTDrawPrim->SetAlphaTestMode(DRAWPRIM_NOALPHATEST);
	g_pILTDrawPrim->SetTransformType(DRAWPRIM_TRANSFORM_SCREEN);
	g_pILTDrawPrim->SetClipMode(DRAWPRIM_FASTCLIP);
	g_pILTDrawPrim->SetFillMode(DRAWPRIM_FILL);
	g_pILTDrawPrim->SetTexture(NULL);

	LT_POLYF4 FillPoly;
	FillPoly.rgba.a = (uint8)((fAlpha * 255.0f) + 0.5f); FillPoly.rgba.r = RGBA_GETR(fillColor); FillPoly.rgba.g = RGBA_GETG(fillColor); FillPoly.rgba.b = RGBA_GETB(fillColor);
	FillPoly.verts[0].x = rect.left; FillPoly.verts[0].y = rect.top; FillPoly.verts[0].z = SCREEN_NEAR_Z;
	FillPoly.verts[1].x = rect.right; FillPoly.verts[1].y = rect.top; FillPoly.verts[1].z = SCREEN_NEAR_Z;
	FillPoly.verts[2].x = rect.right; FillPoly.verts[2].y = rect.bottom; FillPoly.verts[2].z = SCREEN_NEAR_Z;
	FillPoly.verts[3].x = rect.left; FillPoly.verts[3].y = rect.bottom; FillPoly.verts[3].z = SCREEN_NEAR_Z;
	g_pILTDrawPrim->DrawPrim(&FillPoly,1);

	// Leave blending off for whatever draws next, as the old box did
	g_pILTDrawPrim->SetAlphaBlendMode(DRAWPRIM_NOBLEND);
}

// The console's text size in pixels: ConsoleFontSize, or when that's 0 the screen height / 54, at least 9.
// 9 is the original size, kept at 480 lines, and 1080 lines give 20
static uint32 ConsoleFontPixels(const RenderStruct *pStruct)
{
	if (g_CV_ConsoleFontSize > 0)
		return (uint32)LTCLAMP(g_CV_ConsoleFontSize, 6, 96);

	const uint32 nHeight = pStruct ? pStruct->m_Height : 0;
	const uint32 nPixels = (nHeight + 27) / 54;
	return (nPixels < 9) ? 9 : nPixels;
}

void CConsole::EnsureFont()
{
	const uint32 nPixels = ConsoleFontPixels(m_pStruct);

	// Rebuild at a new size when the screen or ConsoleFontSize changed.
	// The debug text shares this font, so it goes first, as in Term
	if (m_Font && nPixels != m_nFontPixels)
	{
		CDebugGeometry& dgPerf = getDebugGeometry(); dgPerf.clear();
		g_IFontManager->DestroyFont(m_Font); m_Font = NULL;
		if (m_Tex) { g_ITexInterface->ReleaseTextureHandle(m_Tex); m_Tex = NULL; }
		m_bFontCreateFailed = false;
	}

	if (m_Font || m_bFontCreateFailed)
		return;

	// Only latched once the font and texture managers exist as the console can draw before they do
	if (!g_IFontManager || !g_ITexInterface)
		return;

	m_nFontPixels = nPixels;

	if (nPixels <= 9)
	{
		// The original console: 9 pixel lines, a 7 pixel advance, and the engine's bitmap font where the game has one
		const uint32 iCharWidth = 7;
		m_FontHeight = m_FullFontHeight = 9;
		for (uint32 i = 0; i < NUM_CONSOLE_CHARACTERS; ++i)
			m_CharWidths[i] = (uint16)iCharWidth;

		assert(!m_Tex);
		m_Tex = NULL;

		if (g_CV_Console_FontTexFile)
		{
			g_ITexInterface->CreateTextureFromName(m_Tex,g_CV_Console_FontTexFile);
		}
		else
		{
			g_ITexInterface->CreateTextureFromName(m_Tex,"console\\console_font.dtx");
		}

		if (m_Tex)
		{
			m_Font = g_IFontManager->CreateFont(m_Tex,9,9);
			m_Font->SetDefCharWidth(iCharWidth); m_Font->SetDefCharHeight(9);
		}
		else
		{
			// The console font texture ships within a Jupiter game's engine archive, which an LT1 game won't have.
			// This falls back to an installed face for these cases, monospaced as the fixed character width assumes
			const char* pszFace = g_CV_Console_FontFace ? g_CV_Console_FontFace : "Courier New";
			m_Font = g_IFontManager->CreateFont(LTNULL, pszFace, m_FontHeight, 32, 127);
			if (m_Font)
			{
				m_Font->SetDefCharWidth(iCharWidth); m_Font->SetDefCharHeight(9);
			}
		}
	}
	else
	{
		// Larger sizes use an installed monospaced face rendered at that size, one texel per pixel rather than
		// the 9 pixel font magnified: Console_FontFace if set, then Consolas, then Courier New
		const char* aFaces[] = { g_CV_Console_FontFace, "Consolas", "Courier New" };
		for (uint32 i = 0; i < sizeof(aFaces) / sizeof(aFaces[0]) && !m_Font; ++i)
		{
			if (aFaces[i] && aFaces[i][0])
				m_Font = g_IFontManager->CreateFont(LTNULL, aFaces[i], nPixels, 32, 126);
		}

		if (m_Font)
		{
			// The poly string draws a glyph (tex height + 2) tall from a slot that tall, so this is one texel per pixel
			// and each glyph comes out at its table width
			const uint32 nTexHeight = m_Font->GetCharTexHeight();
			m_Font->SetDefCharHeight((uint8)(nTexHeight + 2));
			m_FontHeight = m_FullFontHeight = (uint16)(nTexHeight + 3);

			// The layout uses the widths the poly string advances by (a glyph's table width plus the spacing, and the
			// font's space width for a blank), so the caret and the clip land on the drawn text
			const uint16* pTable = m_Font->GetFontTable();
			const uint8* pMap = m_Font->GetFontMap();
			const uint16 nSpacing = m_Font->GetDefSpacingH();
			const uint16 nBlank = (uint16)(m_Font->GetDefCharScreenWidth() + nSpacing);
			for (uint32 i = 0; i < NUM_CONSOLE_CHARACTERS; ++i)
				m_CharWidths[i] = nBlank;
			if (pTable)
			{
				for (uint32 c = 33; c < NUM_CONSOLE_CHARACTERS; ++c)
				{
					const uint32 nIndex = pMap ? pMap[c] : (c - 33);
					m_CharWidths[c] = (uint16)(pTable[nIndex * 3] + nSpacing);
				}
			}
		}
	}

	if (!m_Font)
		m_bFontCreateFailed = true;
}

extern uint32	g_ScreenWidth, g_ScreenHeight;
void CConsole::Draw() {
#ifndef DE_HEADLESS_CLIENT
	//	if ((!m_pFontBitmapData) || (!m_pStruct)) return;
	if (!m_pStruct) return;

	// Preserve current viewport
	D3DVIEWPORT9 oldViewportData;
    PD3DDEVICE->GetViewport(&oldViewportData);

	D3DVIEWPORT9 viewportData;
	viewportData.X		= 0;	
	viewportData.Y		= 0;
	viewportData.Width	= g_ScreenWidth;
	viewportData.Height = g_ScreenHeight;
	viewportData.MinZ	= 0;
	viewportData.MaxZ	= 1.0f;
	HRESULT hResult = D3D_CALL(PD3DDEVICE->SetViewport(&viewportData));

	// The font first, since its size sets the line height
	EnsureFont();

	// Check the console variables
	CheckVariables();

	LTRect cRect;

	CalcRect( cRect );

	SetBackgroundAlpha( g_CV_ConsoleAlpha );

	// The game's console.pcx when it ships one, as LT1 drew it, else the plain fill.
	// It's loaded on the first draw, and again after the next Init
	if ( !m_bBackgroundTried )
	{
		m_bBackgroundTried = true;
		LoadBackground();
	}

	if ( m_BackTex )
		TexturedRectangle( m_BackTex, g_CV_ConsoleFillAlpha, cRect );
	else
		FilledRectangle( m_BackColor, g_CV_ConsoleFillAlpha, cRect );

	// Grab the current filter states.
	bool bValidFilterStates = true;
	DWORD nMinFilter, nMagFilter, nMipFilter;
	if( FAILED(PD3DDEVICE->GetSamplerState(0, D3DSAMP_MINFILTER, &nMinFilter)) )
		bValidFilterStates = false;
	if( FAILED(PD3DDEVICE->GetSamplerState(0, D3DSAMP_MAGFILTER, &nMagFilter)) )
		bValidFilterStates = false;
	if( FAILED(PD3DDEVICE->GetSamplerState(0, D3DSAMP_MIPFILTER, &nMipFilter)) )
		bValidFilterStates = false;

	if( !bValidFilterStates )
	{
		PD3DDEVICE->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		PD3DDEVICE->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		PD3DDEVICE->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
	}

	// Draw all the text.
	DrawTextLines( m_nTextLines );

	// Draw the command box.
	GetCommandBox()->Draw();

	// Restore the previous filter states.
	if( bValidFilterStates )
	{
		PD3DDEVICE->SetSamplerState(0, D3DSAMP_MINFILTER, nMinFilter);
		PD3DDEVICE->SetSamplerState(0, D3DSAMP_MAGFILTER, nMagFilter);
		PD3DDEVICE->SetSamplerState(0, D3DSAMP_MIPFILTER, nMipFilter);
	}

	// Restore previous viewport
    PD3DDEVICE->SetViewport(&oldViewportData);

#endif // !DE_HEADLESS_CLIENT
}

int CConsole::CalcStringOffset(const char *pStr, int iLength)
{
	int offset;

	offset = 0;

	if ( iLength < 0)
	{
		// Calculate the width of the entire string
		while ( *pStr != 0 )
		{
			offset += m_CharWidths[*pStr];
			++pStr;
		}
	}
	else
	{
		// Calculate the width of a portion of the string
		while ( (*pStr) && (iLength) )
		{
			offset += m_CharWidths[*pStr];
			++pStr;
			--iLength;
		}
	}
	
	return offset;
}

void CConsole::DrawTextLine(const char *pText, const LTRect *pRect, COLORREF textColor)
{
	if ((m_Rect.right > 0) && (pRect->left > m_Rect.right)) return;

	EnsureFont();

	if (m_Font) 
	{
		m_Font->SetDefColor(textColor);

 		char* pCh = const_cast<char*>(pText); 
		char szBuff[256]; 
		uint32 iCh = 0; 
		char* pB = szBuff; 
		uint32 iW = 0;

 		while (*pCh && (iW + CharAdvance(*pCh) <= (uint32)(pRect->right - pRect->left)) && iCh < 255) 
		{ 
			iW += CharAdvance(*pCh); 
			*pB = *pCh; 
			++pCh; 
			++pB; 
			++iCh; 
		} 

		*pB = NULL;

 		m_Font->DrawString((float)pRect->left,(float)pRect->top,szBuff); 

	}


/*	long pitch;
	int stringLen;
	int	xCurPos, xEndPos, iChar, charWidth, xCounter;
	uint32 *pCurMonoPos;
	int	y;
	GenericColor gcTextColor;
	FormatMgr *pFormatMgr;
	uint8 *pBuf, *pBufLine;
	uint16 *pPos16;
	uint32 *pPos32;
	char theChar;

	if ((pRect->top < 0) || (pRect->bottom >= (int)m_pStruct->m_Height) ||
		((pRect->top + m_FullFontHeight) >= (int)m_pStruct->m_Height))
		return;

	pFormatMgr = format_mgr->Mgr();
	PFormat screenFormat; m_pStruct->GetScreenFormat(&screenFormat);
	pFormatMgr->PValueToFormatColor( &screenFormat, textColor, gcTextColor );

	if (m_pStruct->LockScreen(pRect->left, pRect->top, pRect->right, pRect->bottom, (void**)&pBuf, &pitch)) {
		xCurPos = 0;

		stringLen = strlen( pText );
		for (iChar = 0; iChar < stringLen; iChar++) {
			theChar = pText[iChar];
			if (theChar < 0) continue;

			charWidth = m_CharWidths[theChar];
			xEndPos = xCurPos + charWidth;

			// Clip on X.
			if ((xCurPos < 0) || (xEndPos >= (pRect->right - pRect->left))) continue;

			// Draw.
			pBufLine = pBuf + xCurPos * screenFormat.GetNumPixelBytes();
			pCurMonoPos = &m_pFontBitmapData[ theChar * m_FullFontHeight ];
			for (y = 0; y < m_FullFontHeight; ++y) {
				uint32 nLineData = *pCurMonoPos;
			
				// Draw one line
				xCounter = charWidth;
				if (screenFormat.m_BPP == BPP_16) {
					pPos16 = (uint16*)pBufLine;
					while (xCounter) {
						xCounter--;
						if (nLineData & 1) {
							*pPos16 = gcTextColor.wVal; }
						nLineData = nLineData >> 1;
						++pPos16; } }
				else if (screenFormat.m_BPP == BPP_32) {
					pPos32 = (uint32*)pBufLine;
					
					while (xCounter) {
						xCounter--;
						if (nLineData & 1) {
							*pPos32 = gcTextColor.dwVal; }
						nLineData = nLineData >> 1;
						++pPos32; } }

				// Move to the next line
				pBufLine += pitch;
				pCurMonoPos++; }			

			// Increment the X position.
			xCurPos += charWidth; }
		
		m_pStruct->UnlockScreen(); } */
}

void CConsole::DrawSmall(int nLines)
{
#ifndef DE_HEADLESS_CLIENT
	//	if (!m_pFontBitmapData) return;

	// Preserve current viewport
	D3DVIEWPORT9 oldViewportData;
    PD3DDEVICE->GetViewport(&oldViewportData);

	D3DVIEWPORT9 viewportData;
	viewportData.X		= 0;	
	viewportData.Y		= 0;
	viewportData.Width	= g_ScreenWidth;
	viewportData.Height = g_ScreenHeight;
	viewportData.MinZ	= 0;
	viewportData.MaxZ	= 1.0f;
	HRESULT hResult = D3D_CALL(PD3DDEVICE->SetViewport(&viewportData));

	// Grab the current filter states.
	bool bValidFilterStates = true;
	DWORD nMinFilter, nMagFilter, nMipFilter;
	if( FAILED(PD3DDEVICE->GetSamplerState(0, D3DSAMP_MINFILTER, &nMinFilter)) )
		bValidFilterStates = false;
	if( FAILED(PD3DDEVICE->GetSamplerState(0, D3DSAMP_MAGFILTER, &nMagFilter)) )
		bValidFilterStates = false;
	if( FAILED(PD3DDEVICE->GetSamplerState(0, D3DSAMP_MIPFILTER, &nMipFilter)) )
		bValidFilterStates = false;

	if( !bValidFilterStates )
	{
		PD3DDEVICE->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		PD3DDEVICE->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		PD3DDEVICE->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
	}

	// Draw the text.
	DrawTextLines( nLines, true );

	// Restore the previous filter states.
	if( bValidFilterStates )
	{
		PD3DDEVICE->SetSamplerState(0, D3DSAMP_MINFILTER, nMinFilter);
		PD3DDEVICE->SetSamplerState(0, D3DSAMP_MAGFILTER, nMagFilter);
		PD3DDEVICE->SetSamplerState(0, D3DSAMP_MIPFILTER, nMipFilter);
	}

	//Restore previous viewport
    PD3DDEVICE->SetViewport(&oldViewportData);

#endif // !DE_HEADLESS_CLIENT
}

void CConsole::SetRect( const LTRect &cRect )
{
	// Jump out if it hasn't changed
	// Note: Taken out, because the screen extents may have changed... -JE
//	if ((m_Rect.left == cRect.left) && (m_Rect.top == cRect.top) && (m_Rect.right == cRect.right) && (m_Rect.bottom == cRect.bottom)) return;

	m_Rect = cRect;
	LTRect TempRect;

	CalcRect( TempRect );
	m_nTextLines = (TempRect.bottom - TempRect.top) / m_FontHeight;
	m_nTextLines -= 2;	// Leave 1 for the command box.

	LTRect cCommandRect;
	GetTextLineBox( m_nTextLines, &cCommandRect);
	GetCommandBox()->SetRect( cCommandRect );
}

void CConsole::CalcRect( LTRect &cRect )
{
	// Just default to the rendering rectangle if we don't have a rendering structure yet
	if ( !GetRenderStruct() )
	{
		cRect = m_Rect;
		return;
	}

	// Get the new rectangle based on the screen width and height for negative values
	if ( m_Rect.top == -1 )
		cRect.top = 0;
	else if ( m_Rect.top < 0 )
		cRect.top = (int)GetRenderStruct()->m_Height / (-m_Rect.top);
	else
		cRect.top = m_Rect.top;

	if ( m_Rect.left == -1 )
		cRect.left = 0;
	else if ( m_Rect.left < 0 )
		cRect.left = (int)GetRenderStruct()->m_Width / (-m_Rect.left);
	else
		cRect.left = m_Rect.left;

	if ( m_Rect.right == -1 )
		cRect.right = (int)GetRenderStruct()->m_Width;
	else if ( m_Rect.right < 0 )
		cRect.right = (int)GetRenderStruct()->m_Width - ((int)GetRenderStruct()->m_Width / (-m_Rect.right));
	else
		cRect.right = m_Rect.right;

	if ( m_Rect.bottom == -1 )
		cRect.bottom = (int)GetRenderStruct()->m_Height;
	else if ( m_Rect.bottom < 0 )
		cRect.bottom = (int)GetRenderStruct()->m_Height - ((int)GetRenderStruct()->m_Height / (-m_Rect.bottom));
	else
		cRect.bottom = m_Rect.bottom;

	// Clip the rectangle to the screen
	cRect.right = min( cRect.right, (int)GetRenderStruct()->m_Width - 3);
	cRect.bottom = min( cRect.bottom, (int)GetRenderStruct()->m_Height );
	// Restrict the rectangle to a minimum size
	cRect.right = max( cRect.right, 160 );
	cRect.bottom = max( cRect.bottom, m_FullFontHeight * 4 );
	cRect.left = min( cRect.left, cRect.right - 160 );
	cRect.top = min( cRect.top, cRect.bottom - (m_FullFontHeight * 4) );
}

void CConsole::PrintString(CONCOLOR theColor, int filterLevel, const char *pMsg)
{
	// Protect agains error messages that happen after the destructor has been called
	if (!m_bInitialized)
		return;

	//	Send the info we're using for output to the client shell so the 
	//	game code can do whatever it wants with it.
	if (i_client_shell != NULL) {
		CConsolePrintData	PrintData;

		//	turn the CONCOLOR into an LTRGB
		PrintData.m_Color.r	= RGBA_GETR (theColor);
		PrintData.m_Color.g	= RGBA_GETG (theColor);
		PrintData.m_Color.b	= RGBA_GETB (theColor);
		PrintData.m_Color.a	= RGBA_GETA (theColor);

		PrintData.m_pMessage		= pMsg;
		PrintData.m_nFilterLevel	= filterLevel;

		i_client_shell->OnConsolePrint (&PrintData);

		//	the client shell might have changed any of the data we sent it\
		//	so reset our parameters
		if (PrintData.m_pMessage == NULL)
			return;

		filterLevel	= PrintData.m_nFilterLevel;
		pMsg		= PrintData.m_pMessage;
		theColor	= CONRGB (PrintData.m_Color.r, PrintData.m_Color.g,
							  PrintData.m_Color.b);
	}

	if ( filterLevel > m_FilterLevel ) return; 

	CConTextLine*	pLine;

	m_CS.Enter();
			
	if ( m_ErrorLogFn )
		m_ErrorLogFn( pMsg );

	if ( g_CV_TraceConsole )
	{
		OutputDebugString( pMsg );
		OutputDebugString( "\n" );
	}
			
	if ( m_TextLines.GetSize() > 0 )
	{
		int iLength;
		const char *pNewLine;

		do
		{
			// Search for a newline character
			pNewLine = strchr( pMsg, '\n' );
			if ( pNewLine && (pNewLine[1]) )
				iLength = pNewLine - pMsg;
			else
			{
				pNewLine = LTNULL;
				iLength = MAX_CONSOLE_TEXTLEN - 1;
			}

			// Get the head of the text list
			pLine = m_TextLines.GetHead();
			// Make sure the list doesn't get too long
			m_TextLines.RemoveAt( pLine );

			// Copy the message
			pLine->m_Color = theColor;
			LTStrnCpy( pLine->m_Text, pMsg, MAX_CONSOLE_TEXTLEN, iLength );

			// Remove the newline character
			if ( pNewLine )
			{
				pLine->m_Text[iLength] = 0;
				pMsg = &pNewLine[1];
			}

			// Add it to the end of the list
			m_TextLines.AddTail( pLine );

		} while ( pNewLine );
	}
		
	m_CS.Leave();
}

void CConsole::vPrintf(CONCOLOR theColor, int filterLevel, const char *pMsg, va_list vaArgs)
{
	if ( FilterAction( filterLevel ) )
		return;

	char str[500];
	LTVSNPrintF(str, sizeof(str), pMsg, vaArgs);
	PrintString(theColor, filterLevel, str);
}

void CConsole::Printf(CONCOLOR theColor, int filterLevel, const char *pMsg, ...)
{
	va_list vaArgs;
	va_start(vaArgs, pMsg);
	vPrintf(theColor, filterLevel, pMsg, vaArgs);
	va_end(vaArgs);
}

void CConsole::OnKeyPress(uint32 key)
{
	switch (key)
	{
		case VK_RETURN :
			FinishCommand();
			break;
		case VK_UP : 
			if ( IsKeyDown( VK_CONTROL ) )
				Scroll( 1 );
			else
				CycleCommands( -1 );
			break;
		case VK_DOWN :
			if ( IsKeyDown( VK_CONTROL ) )
				Scroll( -1 );
			else
				CycleCommands( 1 );
			break;
		case VK_TAB :
			if ( IsKeyDown( VK_CONTROL ) )
				MatchCommands();
			else if ( IsKeyDown( VK_SHIFT ) )
				PrevCommand();
			else
				NextCommand();
			break;
		case VK_PRIOR :
			if ( IsKeyDown( VK_CONTROL ) )
				Scroll( (int)m_TextLines.GetSize() );
			else
				Scroll( (int)m_nTextLines );
			break;
		case VK_NEXT :
			if ( IsKeyDown( VK_CONTROL ) )
				Scroll( -GetScrollOffset() );
			else
				Scroll( -(int)m_nTextLines );
			break;
		default :
			GetCommandBox()->OnKeyPress( key );
			break;
	}
}



