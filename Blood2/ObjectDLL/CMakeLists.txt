project(BLOOD2_ObjectDLL)

# Blood II's server objects built against libs/DECompat like Shogo's.
# The sources are ObjectDLL.dsp's

find_package(SDL2 REQUIRED)

add_library(${PROJECT_NAME} SHARED
	AI_Mgr.cpp
	AI_Shared.CPP
	AmmoPickups.cpp
	AncientOne.cpp
	AncientOneTentacle.cpp
	Anim_Sound.cpp
	B2BaseClass.cpp
	BaseCharacter.cpp
	behemoth.cpp
	BloodServerShell.cpp
	boneleech.cpp
	BulletImpactSFX.cpp
	CalebAI.cpp
	CameraObj.cpp
	CameraSpot.cpp
	CinematicTrigger.cpp
	CivilianAI.cpp
	clientbloodtrail.cpp
	ClientCastLineSFX.cpp
	clientdebugline.cpp
	ClientExplosionSFX.cpp
	ClientGibFX.cpp
	ClientLaserBeamSFX.cpp
	ClientLaserFX.cpp
	ClientLightFX.cpp
	ClientLightningSFX.cpp
	ClientMarkSFX.cpp
	ClientParticleStreamSFX.cpp
	ClientSFX.cpp
	ClientSmokeTrail.cpp
	ClientSparksSFX.cpp
	ClientSplashSFX.cpp
	ClientSplatFX.cpp
	ClientTracer.cpp
	ClientWarpGateSFX.cpp
	ClientWeaponSFX.cpp
	ConversationTrigger.cpp
	corpse.cpp
	CultistAI.cpp
	DeathShroud.cpp
	Debris.cpp
	Destructable.cpp
	DestructableBrush.cpp
	DestructableModel.cpp
	DetailSprite.cpp
	Door.cpp
	DrudgeLord.cpp
	drudgepriest.cpp
	EnhancedGideon.cpp
	ExitHint.cpp
	ExitTrigger.cpp
	Explosion.cpp
	Fanatic.cpp
	FireFX.cpp
	FlagObjects.cpp
	GabriellaAI.cpp
	GabriellaREV.cpp
	GameInvItems.cpp
	GameProjectiles.cpp
	GameStartPoint.cpp
	GameWeapons.cpp
	Gib.cpp
	Gideon.cpp
	GooSplat.cpp
	Gremlin.cpp
	HandWeaponModel.cpp
	Impacts.cpp
	InventoryMgr.cpp
	InvItem.cpp
	IshmaelAI.cpp
	IshmaelREV.cpp
	ItemPickups.cpp
	Key.cpp
	KeyData.cpp
	KeyFramer.cpp
	LightFX.cpp
	MadScientistAI.cpp
	ModelObject.cpp
	movement.cpp
	Music.cpp
	Naga.cpp
	NagaCeilingDebris.cpp
	nightmare.cpp
	ObjectivesTrigger.cpp
	ObjectUtilities.cpp
	OpheliaAI.cpp
	OpheliaREV.cpp
	ParticleSystem.cpp
	PathListData.cpp
	PathMgr.cpp
	pathpoint.cpp
	PickupObject.cpp
	PlayerObj.cpp
	PolyGrid.cpp
	PowerupPickups.cpp
	Projectile.cpp
	prophet.cpp
	Rain.cpp
	RandomSoundFX.cpp
	Rotating.cpp
	RotatingBrush.cpp
	RotatingDoor.cpp
	RotatingModel.cpp
	RotatingSprite.cpp
	Script.cpp
	SeeingEye.cpp
	ShellCasing.cpp
	ShikariAI.cpp
	SmellHint.cpp
	SoccerObjects.cpp
	SoulDrudge.cpp
	SoundFX.cpp
	Sparam.cpp
	SparksObj.cpp
	Spawner.cpp
	Tchernotronic.cpp
	TheHandAI.cpp
	thief.cpp
	Trigger.cpp
	TripLaser.cpp
	UndeadGideon.cpp
	ViewWeaponModel.cpp
	VoiceMgr.cpp
	VolumeBrush.cpp
	VolumeBrushTypes.cpp
	Weapon.cpp
	WeaponPickups.cpp
	WorldModelDebris.cpp
	WorldProperties.cpp
	WreckingBall.cpp
	ZealotAI.cpp
	# cpp_engineobjects_de.cpp isn't built because ltengineobjects.cpp below already defines BaseClass
	
	# Shared with ClientShellDLL
	../Shared/TeamMgr.cpp
	../Shared/WeaponDefs.cpp
	../../sdk/inc/ltmodule.cpp
	../../sdk/inc/iltbaseclass.cpp
	# Server object class registration (BaseClass and the BEGIN_CLASS/END_CLASS objects)
	../../sdk/inc/ltengineobjects.cpp
	../../sdk/inc/ltobjref.cpp
	../../sdk/inc/ltquatbase.cpp
	../../sdk/inc/LTEulerAngles.cpp)

include_directories(./
	../Shared
	../Misc                      # BareList.h, for ../Shared/TeamMgr.h
	../../sdk/inc
	../../libs/DECompat
	../../libs/DECompat/lt1      # LT1 include names
	${SDL2_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME}
	LIB_DECompat
	LIB_LTMem
	${SDL2_LIBRARIES})

set_target_properties(${PROJECT_NAME}
	PROPERTIES OUTPUT_NAME Object
	SUFFIX .lto)

if(LINUX)
	set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-fPIC")

	target_link_options(${PROJECT_NAME} PRIVATE -Wl,--no-undefined)
endif(LINUX)

# Macro redefinitions are build errors here.
# LT1 and Jupiter define MAX_CS_FILENAME_LEN differently, and ObjectCreateStruct is laid out from it.
if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /we4005)
endif(MSVC)
