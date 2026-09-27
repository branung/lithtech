// ----------------------------------------------------------------------- //
//
// MODULE  : RiotServerShell.cpp
//
// PURPOSE : Riot's Server Shell - Definition
//
// CREATED : 9/18/97
//
// ----------------------------------------------------------------------- //

#ifndef __RIOTSERVERSHELL_H__
#define __RIOTSERVERSHELL_H__


#include "cpp_servershell_de.h"
#include "ClientServerShared.h"
#include "CheatDefs.h"
#include "CharacterMgr.h"
#include "GameStartPoint.h"
#include "NetDefs.h"
#include "CVarTrack.h"
#include "DialogQueue.h"

#define MAX_CLIENTS		MAX_MULTI_PLAYERS

class CPlayerObj;


// No SShellMaker list as Jupiter picks the shell from define_interface


class CRiotServerShell : public CServerShellDE
{
	public :

		declare_interface(CRiotServerShell);

		CRiotServerShell();
		~CRiotServerShell();

		// Initialization that needs the engine since the constructor runs before g_pServerDE exists
		virtual LTRESULT OnServerInitialized();

		void			SetStartPointName(HSTRING hString);
		GameType		GetGameType()	const { return (GameType)m_GameInfo.m_byType; }
		GameDifficulty	GetDifficulty() const { return m_eDifficulty; }

		void		SetUpdateShogoServ() { m_bUpdateShogoServ = DTRUE; }
		DRESULT		ServerAppMessageFn(char* sMsg);
		void		SendShogoServConsoleMessage(char* sMsg);

		void		PauseGame(DBOOL b=DTRUE);

		CPlayerObj * GetFirstPlayer( )
		{
			if( m_aClients[0] )
			{
				CPlayerObj* pPlayer = (CPlayerObj*)g_pServerDE->GetClientUserData(m_aClients[0]);
				return(pPlayer);
			}

			return DNULL;
		}
	
	protected :

		void		OnAddClient(HCLIENT hClient);
		void		OnRemoveClient(HCLIENT hClient);
		DEBaseClass*	OnClientEnterWorld(HCLIENT hClient, void *pClientData, DDWORD clientDataLen);
		void		OnClientExitWorld(HCLIENT hClient);

		void		SendPlayerInfoMsgToClients(HCLIENT hClients, CPlayerObj *pPlayer);
		void		OnMessage(HCLIENT hSender, DBYTE messageID, HMESSAGEREAD hMessage);

	public :

		// Public because CPlayerObj::ClientUpdate fires it from the command mask
		void		OnCommandOn(HCLIENT hClient, int command);

	protected :
		void		PreStartWorld(DBOOL bSwitchingWorlds);
		void		PostStartWorld();
		void		CacheFiles();
		void		UpdateClientPingTimes();
		void		Update(DFLOAT timeElapsed);


	private :

		void HandleUpdatePlayerMsg(HCLIENT hSender, HMESSAGEREAD hMessage);
		void HandleCheatCode(CPlayerObj* pPlayer, CheatCode nCheatCode, DBYTE nData);
		void HandleCheatSpectatorMsg(HCLIENT hSender, HMESSAGEREAD hMessage);
		void HandlePlayerMsg(HCLIENT hSender, DBYTE messageID, HMESSAGEREAD hMessage);
		void HandleCheatBigGuns(DBYTE nData);
		void HandleCheatRemoveAI(DBYTE nData);
		void HandleTriggerBoxCheat(DBYTE nData);

		void HandleLoadGameMsg(HCLIENT hSender, HMESSAGEREAD hMessage);
		void HandleSaveGameMsg(HCLIENT hSender, HMESSAGEREAD hMessage);

		DEBaseClass* CreatePlayer(HCLIENT hClient);
		void RespawnPlayer(DEBaseClass* pClass, HCLIENT hClient);

		float			m_ClientPingSendCounter;

		// Time of day variables.
		CVarTrack		m_SayTrack;
		CVarTrack		m_ShowTimeTrack;
		CVarTrack		m_WorldTimeTrack;
		CVarTrack		m_WorldTimeSpeedTrack;
		CVarTrack		m_WorldColorDayTrack;
		CVarTrack		m_WorldColorNightTrack;
		float			m_TODSeconds;	// Current time of day
		float			m_TODCounter;
		DBYTE			m_WorldTimeColor[3]; // 0-64 so we don't send updates all the time.

		HSTRING			m_hstrStartPointName;

		CCharacterMgr	m_charMgr;
		HCLIENT			m_aClients[MAX_CLIENTS];

		NetGame			m_GameInfo;
		int				m_nCurLevel;
		DBOOL			m_bHitFragLimit;
		DBOOL			m_bUpdateShogoServ;
		DBOOL			m_bShogoServHosted;

		DBYTE			m_nLastLGFlags;
		GameDifficulty	m_eDifficulty;

		HCONVAR			m_hSwitchWorldVar;
		DBOOL			m_bFirstUpdate;

		void CacheModels();
		void CacheTextures();
		void CacheSprites();
		void CacheSounds();
		void SetupGameInfo();
		void UpdateMultiplayer();
		void CheckSwitchWorldCommand();
		DRESULT SwitchToWorld(const char *pWorldName, const char *pNextWorldName);
		void StartNextMultiplayerLevel();
		void ReportError(HCLIENT hClient, DBYTE nErrorType);

		DBOOL UpdateSessionName();
		DBOOL UpdateShogoServer();

		void		ClearClientList() { for (int i = 0; i < MAX_CLIENTS; i++) { m_aClients[i] = DNULL; } }
		DBOOL		AddClientToList(HCLIENT hClient);
		DBOOL		RemoveClientFromList(HCLIENT hClient);
		DBOOL		IsClientInList(HCLIENT hClient);
		CPlayerObj*	GetPlayerFromClientList(HCLIENT hClient);

};


#endif  // __RIOTSERVERSHELL_H__

