project(SHOGO_ObjectDLL)

# Shogo's server objects built against libs/DECompat

find_package(SDL2 REQUIRED)

add_library(${PROJECT_NAME} SHARED
	AIKey.cpp
	AIKeyData.cpp
	AIPathMgr.cpp
	AISounds.cpp
	AVC.cpp
	Activation.cpp
	Admiral.cpp
	AdvSound.cpp
	Akuma.cpp
	Andra10.cpp
	Andra5.cpp
	ArmorBase.cpp
	ArmorRepair.cpp
	Assassin.cpp
	Baku.cpp
	BaseAI.cpp
	BaseCharacter.cpp
	BodyArmor.cpp
	BodyProp.cpp
	Bouncer.cpp
	CachedFiles.cpp
	Camera.cpp
	Cat.cpp
	CharacterAttributes.cpp
	CharacterMgr.cpp
	Civilian1.cpp
	Civilian2.cpp
	ClientCastLineSFX.cpp
	ClientDeathSFX.cpp
	ClientLightFX.cpp
	ClientSFX.cpp
	ClientWeaponSFX.cpp
	Cothineal.cpp
	DebrisFuncs.cpp
	Destructable.cpp
	DestructableDoor.cpp
	DestructableRotatingDoor.cpp
	DestructableWorldModel.cpp
	DialogTrigger.cpp
	Door.cpp
	Dropship.cpp
	EliteShockTrooper.cpp
	EliteTrooper.cpp
	Enforcer.cpp
	EnhancementItem.cpp
	Enhancements.cpp
	ExitTrigger.cpp
	Explosion.cpp
	FirstAid.cpp
	FirstAidBase.cpp
	Gabriel.cpp
	GameStartPoint.cpp
	GlassProp.cpp
	Hammerhead.cpp
	Hank.cpp
	Inventory.cpp
	InventoryItem.cpp
	Kathryn.cpp
	Key.cpp
	KeyData.cpp
	KeyFramer.cpp
	Kura.cpp
	LaserTrigger.cpp
	Lift.cpp
	LittleKid.cpp
	MajorCharacter.cpp
	Music.cpp
	ObjectiveTrigger.cpp
	Officer.cpp
	Ordog.cpp
	PVWeaponModel.cpp
	ParticleSystem.cpp
	PickupItem.cpp
	PlayerMode.cpp
	PlayerObj.cpp
	PolyGrid.cpp
	PowerSurge.cpp
	Powerup.cpp
	Predator.cpp
	Projectile.cpp
	Prop.cpp
	Raksha.cpp
	Rascal.cpp
	RiotObjectUtilities.cpp
	RiotProjectiles.cpp
	RiotServerShell.cpp
	RiotWeapons.cpp
	RotatingDoor.cpp
	RotatingWorldModel.cpp
	Ruin150.cpp
	Ryo.cpp
	SSFXReg.cpp
	Samantha.cpp
	Sanjuro.cpp
	ScaleSprite.cpp
	ShockTrooper.cpp
	ShogoSecret.cpp
	Sparam.cpp
	Sparrowhawk.cpp
	Spawner.cpp
	StoryTrigger.cpp
	Tenma.cpp
	Toshiro.cpp
	TractorBeam.cpp
	TransmissionTrigger.cpp
	Trigger.cpp
	TriggerSound.cpp
	Trooper.cpp
	UhlanA3.cpp
	UltraPowerupItem.cpp
	UltraPowerups.cpp
	UnlockKey.cpp
	UpgradeItem.cpp
	Upgrades.cpp
	Vandal.cpp
	Vehicle.cpp
	Vigilance.cpp
	VolumeBrush.cpp
	VolumeBrushTypes.cpp
	Weapon.cpp
	WeaponPowerups.cpp
	Weapons.cpp
	WorldModelDebris.cpp
	WorldProperties.cpp
	# object_list.cpp isn't built because ltengineobjects.cpp below already expands its DEFINE_CLASSES()
	#
	# Shared with ClientShellDLL
	../Shared/DebrisTypes.cpp
	../Shared/ModelFuncs.cpp
	../Shared/RiotCommonUtilities.cpp
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
