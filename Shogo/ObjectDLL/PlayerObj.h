//----------------------------------------------------------
//
// MODULE  : PlayerObj.h
//
// PURPOSE : Riot player object definition
//
// CREATED : 9/18/97
//
//----------------------------------------------------------

#ifndef __CPLAYER_OBJ_H__
#define __CPLAYER_OBJ_H__

#include "BaseCharacter.h"
#include "PlayerMode.h"
#include "GameStartPoint.h"
#include "RiotMsgIDs.h"
#include "CheatDefs.h"
#include "Music.h"
#include "TemplateList.h"
#include "CVarTrack.h"

#define BC_CFLG_SPECIAL_MOVE	(1<<31)

#define NET_NAME_LENGTH			33

class TractorBeam;
class CWeapon;

bool TractorBeamFilter (HOBJECT hObject, void* pUserData);

class CPlayerObj : public CBaseCharacter
{
	public :

		// True while SkipActiveCinematic runs a scene's timeline in one frame.
		// Static because CBaseCharacter::ProcessTriggerMsg reads it
		static DBOOL IsSkippingCinematic()	{ return s_bSkippingCinematic; }

		CPlayerObj();
		virtual ~CPlayerObj();

		virtual DBOOL IsMecha() { return !(m_playerMode.IsOnFoot()); }

		virtual void HandleWeaponChange();

		void StartLevel();
		void SetClient(HCLIENT hClient) { m_hClient = hClient; }
		HCLIENT GetClient() const		{ return m_hClient; }

		void SetClientSaveData(HMESSAGEREAD hClientData) { m_hClientSaveData = hClientData; }
		
		HMESSAGEWRITE StartMessageToMyClient(DBYTE msgID);
		void BuildKeepAlives(ObjectList* pList);

		// If the update had CLIENTUPDATE_ALLOWINPUT, then it returns FALSE because nothing
		// else will be in the packet.
		DBOOL ClientUpdate(HMESSAGEREAD hMessage);

		/*
			Is the client holding this command?
			Jupiter has no server IsCommandOn so the state comes in the player update (CLIENTUPDATE_COMMANDS).
		*/
		DBOOL IsCommandOn(int nCommand) const
		{
			if (nCommand < 0 || nCommand >= PLAYER_COMMAND_BITS)
				return DFALSE;
			return (m_nCommandBits[nCommand >> 6] &
					(((uint64)1) << (nCommand & 63))) ? DTRUE : DFALSE;
		}

		DBOOL MultiplayerInit(HMESSAGEREAD hMessage);

		void Respawn(HSTRING hStartPointName, DBYTE nServerLoadGameFlags=LOAD_NEW_LEVEL);
		void DropUpgrade();
		void ToggleOnFootMode();
		void SetSpectatorMode(DBOOL bOn);
		void ToggleVehicleMode();
		void IncMechaMode();
		void ToggleRunLock();
		void ToggleGodMode();
		void ToggleDebugCheat(CheatCode nCheatCode);

		void FullAmmoCheat();
		void FullWeaponCheat();
		void RepairArmorCheat();
		void HealCheat();

		void ChangeWeapon(DBYTE nCommandId, DBOOL bAuto=DFALSE);
		void SetRunLock(DBOOL bRunLock)		{ m_bRunLock = bRunLock; }
		void ChangeState(PlayerState eNewState);
		PlayerState GetState() const { return m_eState; }

		void HandlePlayerPositionMessage(HMESSAGEREAD hRead);
		void HandleWeaponFireMessage(HMESSAGEREAD hRead);
		void HandleWeaponStateMessage(HMESSAGEREAD hRead);
		void HandleWeaponSoundMessage(HMESSAGEREAD hRead);

		void  SetNetName(const char* sNetName) { strncpy(m_sNetName, sNetName, 32); }
		char* GetNetName() { return(m_sNetName); }

		int   GetFragCount() { return(m_nFragCount); }
		void  SetFragCount(int nFrags) { m_nFragCount = nFrags; }
		void  AddFragCount(int nAdd) { m_nFragCount += nAdd; }
		void  IncFragCount() { m_nFragCount++; }
		void  DecFragCount() { m_nFragCount--; }

		CTList<DVector*>* GetScentBiscuitList() { return &m_scentBiscuits; }

		void	DoWeaponChange(DBYTE nWeaponId);

		DList *		GetDialogQueue( ) { return &m_DialogQueue; }
		DBOOL		IsDialogActive( ) { return m_bDialogActive; }
		void		SetDialogActive( DBOOL bDialogActive ) { m_bDialogActive = bDialogActive; }

		// Public data members...

	protected :

		DDWORD EngineMessageFn(DDWORD messageID, void *pData, DFLOAT lData);
		DDWORD ObjectMessageFn(HOBJECT hSender, DDWORD messageID, HMESSAGEREAD hRead);
	
		void	WeaponCheat(DBYTE nWeaponId);

		void	Reset();
		void	StartDeath();
		void	HandleDead(DBOOL bRemoveObj);
		void	HandleSpecialMove(DBOOL bSpecialMoveKeyDown);
		void	InitializeWeapons();
		char*	GetDamageSound(DamageType eType);
		char*	GetDeathSound();

		virtual void SetDeathAnimation();
		virtual void ProcessDamageMsg(HMESSAGEREAD hRead);

		virtual void SetAnimationIndexes();
		virtual void OnTimedPowerupExpiration (PickupItemType eType);

		virtual DVector	GetHeadOffset() { return m_playerMode.GetCameraOffset(); }
		virtual DBOOL ProcessCommand(const char** pTokens, int nArgs, const char* pNextCommand);

		virtual void	KillDlgSnd();

		void	TransmissionEnded( );
		virtual void    PlayDialogSound(const char* pSound, CharacterSoundType eType=CST_DIALOG, DBOOL bAtObjectPos=DFALSE);

	private :

		void	PostPropRead(ObjectCreateStruct *pStruct);
		DBOOL	InitialUpdate(int nInfo);
		DBOOL	Update();
	
		void	CreatePVWeapon();

		void	AddBiscuitModel(DVector* pvPos);
		void	RemoveBiscuitModel();

		GameStartPoint* FindStartPoint(HSTRING hStartPointName);

		virtual void UpdateControlFlags();		// Update our movement/firing flags
		virtual void UpdateAnimation();			// Update current animation
		virtual void UpdateMovement();			// Update player movement
		
		void UpdateClientPhysics();		// Update client physics state.
		void UpdateSpecialMove();		// Update special player movement
		void UpdateAirLevel();			// Update player's air
		void ProcessInput();			// Process input
		void UpdateClientEffects();		// Update client side effects

		void UpdateInterface(DBOOL bForceUpdate=DFALSE);		 // Send client interface info.
		void SetPlayerMode(int nMode, DBOOL bSetDamage=DFALSE);  // Set our player mode

		void TweakCameraOffset();

		void Save(HMESSAGEWRITE hWrite, DDWORD dwSaveFlags);
		void Load(HMESSAGEREAD hRead, DDWORD dwLoadFlags);

		void DoAutoSave();

		DBOOL PlayerTriggerMsg(HMESSAGEREAD hRead);

		void  HandleGameRestore();

		void SetForceUpdateList(ForceUpdate* pFU);
		void CreateSpecialFX();
		void UpdateSpecialFX();

		void UpdateClientViewPos();
		void UpdateScentBiscuits();

		void UpdateConsoleVars();

		// Cutscene skipping, run when the client asks
		void SkipActiveCinematic();

		// One step of the fast forward.
		// Returns DTRUE if it changed anything
		DBOOL SkipOneCinematicStep(DFLOAT* pfVirtualTime);

		// Is a cutscene camera still on screen? (the fast forward's stop condition)
		DBOOL IsCinematicCameraLive();

		static DBOOL s_bSkippingCinematic;

		// Drops every queued transmission and any line in flight
		void ClearDialogQueue();

		void TeleportClientToServerPos();
		void TeleFragObjects(DVector & vPos);

	private :  // Data members

		CPlayerMode m_playerMode;

		CVarTrack	m_TractorBeamSpeedTrack;

		CVarTrack	m_LeashLenTrack;
		float		m_fLeashLen;

		DBOOL		m_bCurClientTractorBeam;
		float		m_TBCount;

		HOBJECT		m_CurContainers[MAX_TRACKED_CONTAINERS];
		DDWORD		m_nCurContainers;
		
		DBOOL		m_bFirstUpdate;
		DFLOAT		m_fOldHitPts;
		DFLOAT		m_fOldArmor;
		DFLOAT		m_fOldAirLevel;
		DFLOAT		m_fAirLevel;
		DVector		m_vOldModelColor;
		DFLOAT		m_fOldModelAlpha;

		// When we respawn, this is incremented so we ignore packets from an older teleport.
		DBYTE		m_ClientMoveCode;

		DFLOAT		m_fOnFootArmor;
		DFLOAT		m_fOnFootHealth;
		DFLOAT		m_fMCAArmor;
		DFLOAT		m_fMCAHealth;

		PlayerState m_eState;

		DBYTE		m_nOldMode;
		DBYTE		m_nCurrentMcaMode;
		DBOOL		m_bZoomView;
		DRotation	m_rWeaponModelRot;

		DBOOL		m_b3rdPersonView;

		DDWORD		m_nSavedFlags;
		GameType	m_eGameType;


		// Special move related variables...

		DBOOL		m_bSpecialMoveOn;		// Currently trying to use special move?


		// Info about cheats...

		DBOOL		m_bRunLock;
		DBOOL		m_bTweakingMovement;
		DBOOL		m_bGodMode;

		DBOOL		m_bAllowInput;

		
		CTList<DVector*>	m_scentBiscuits;	// Where player has been
		DFLOAT				m_fDropBiscuitTime;	// When should we drop a biscuit


		// External camera data...

		DBOOL		m_bUseExternalCameraPos;
		DVector		m_vExternalCameraPos;


		D_WORD		m_nClientChangeFlags;

		// Client command state, a bit per COMMAND_ID_*
		uint64		m_nCommandBits[2];

		HOBJECT		m_hTractorBeam;		// Tractor beam object


		HMESSAGEREAD	m_hClientSaveData;  // Client data to save

		HSTRING			m_hstrStartLevelTriggerTarget;
		HSTRING			m_hstrStartLevelTriggerMessage;

	// NOTE:  The following data members do not need to be saved / loaded
	// when saving games.  Any data members that don't need to be saved
	// should be added here (to keep them together)...


		CTList<DEBaseClass*>	m_biscuitModels;	// Temp for testing

		int				m_nOldAmmo[GUN_MAX_NUMBER];

		HCLIENT			m_hClient;

		HMODELANIM		m_hSpecialMoveAni;

		// Music data...

		CMusic		m_Music;


		// Data members used in load/save...

		DDWORD		m_dwLastLoadFlags;


		// Multiplayer data

		char		m_sNetName[NET_NAME_LENGTH];
		int			m_nFragCount;

		DDWORD		m_dwMultiplayerMechaMode;


		// Console variables used to tweak player movement...

		HCONVAR		m_hMoveVelMulVar;
		HCONVAR		m_hJumpVelMulVar;
		HCONVAR		m_hMoveAccelMulVar;
		HCONVAR		m_hTractorBeamVar;
		HCONVAR		m_hSwimVelVar;
		HCONVAR		m_hLadderVelVar;


		DBOOL		m_bTractorBeamAvailable;
		DBOOL		m_bLevelStarted;
		DBOOL		m_bWaitingForAutoSave;

		DList			m_DialogQueue;
		DBOOL			m_bDialogActive;

};


#endif  // __CPLAYER_OBJ_H__

