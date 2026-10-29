#ifndef __MUSIC_IMA_H__
#define __MUSIC_IMA_H__

// A built in replacement for LT1's ima.dll.
// Both LT1 games call InitMusic("ima.dll"), and the 1998 DLL is 32-bit, so this installs
// an SMusicMgr of its own when a game asks for that name. No game code changes.
// Playback is the sound system's: CSoundBuffer renders a .sec into a wave (music_segment.cpp),
// so a song here is a filename and playing it is one PlaySound.
// What's left here is ima.dll's bookkeeping: songs, playlists, which section is playing, and what happens when it ends.
// LT1's IMUSIC25 called the driver back when a section changed. There's no sequencer to do that here,
// so music_ima_Update polls from CClientMgr::Update

#ifndef __MUSICMGR_H__
#include "musicmgr.h"
#endif

// True for the DLL name that means the LT1 adaptive music driver
bool music_ima_IsBuiltinName(const char *pszMusicDLLName);

// Fill pMusicMgr with this driver, as the DLL's MusicDLLSetup did. Init() is what starts anything
void music_ima_Install(SMusicMgr *pMusicMgr);

// Per frame. Advances the playlist when a section ends and applies a MUSIC_NEXTSONG change.
// Returns at once unless something is playing or pending
void music_ima_Update();

#endif  // __MUSIC_IMA_H__
