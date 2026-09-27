project(SHOGO_ClientShellDLL)

# Shogo's client shell

find_package(SDL2 REQUIRED)

add_library(${PROJECT_NAME} SHARED
	AnimeLineFX.cpp
	BaseLineSystemFX.cpp
	BaseMenu.cpp
	BaseParticleSystemFX.cpp
	BaseScaleFX.cpp
	BitmapFont.cpp
	BulletTrailFX.cpp
	CastLineFX.cpp
	client_physics.cpp
	ClientInfoMgr.cpp
	ClientUtilities.cpp
	ClientWeaponUtils.cpp
	CMoveMgr.cpp
	Credits.cpp
	CreditsWin.cpp
	CriticalHitFX.cpp
	DeathFX.cpp
	DebrisFX.cpp
	DetailSettingsMenu.cpp
	DisplayModeMenu.cpp
	DisplayOptionsMenu.cpp
	DynamicLightFX.cpp
	ExplosionFX.cpp
	GibFX.cpp
	InfoDisplay.cpp
	IpMgr.cpp
	JoystickMenu.cpp
	JuggernautFX.cpp
	KeyboardMenu.cpp
	KeyFixes.cpp
	LaserCannonFX.cpp
	LightFX.cpp
	LightScaleMgr.cpp
	LineBallFX.cpp
	LoadLevelMenu.cpp
	LoadSavedLevelMenu.cpp
	MainMenu.cpp
	MarkSFX.cpp
	MessageBox.cpp
	MessageMgr.cpp
	MissionObjectives.cpp
	MouseMenu.cpp
	Music.cpp
	NetStart.cpp
	NewGameMenu.cpp
	OptionsMenu.cpp
	ParticleExplosionFX.cpp
	ParticleSystemFX.cpp
	ParticleTrailFX.cpp
	ParticleTrailSegmentFX.cpp
	PickupItemFX.cpp
	# plasma1-4 define the same symbols
	plasma4.cpp
	PlayerCamera.cpp
	PlayerFX.cpp
	PlayerInventory.cpp
	PlayerStats.cpp
	PolyGridFX.cpp
	PopupMenu.cpp
	ProjectileFX.cpp
	RiotClientShell.cpp
	RiotMenu.cpp
	RiotSettings.cpp
	RocketFlareFX.cpp
	SaveLevelMenu.cpp
	SFXMgr.cpp
	SFXReg.cpp
	ShellCasingFX.cpp
	SinglePlayerMenu.cpp
	Slider.cpp
	SmokeFX.cpp
	SmokeSphereFX.cpp
	SoundOptionsMenu.cpp
	Sparam.cpp
	SparksFX.cpp
	SpriteFX.cpp
	TextHelper.cpp
	TracerFX.cpp
	VKDefs.cpp
	WeaponFX.cpp
	WeaponFXLow.cpp
	WeaponFXMed.cpp
	WeaponModel.cpp
	WeaponSoundFX.cpp
	WinUtil.cpp
	../Shared/DebrisTypes.cpp
	../Shared/ModelFuncs.cpp
	../Shared/RiotCommonUtilities.cpp
	../Shared/WeaponDefs.cpp
	../../sdk/inc/ltmodule.cpp
	../../sdk/inc/iltbaseclass.cpp
	../../sdk/inc/ltobjref.cpp
	../../sdk/inc/ltquatbase.cpp
	../../sdk/inc/LTEulerAngles.cpp)

if(WIN32)
	# MSVC boilerplate that only includes windows.h and resource.h
	target_sources(${PROJECT_NAME} PRIVATE ClientShellDLL.rc)

	# NetInfo.cpp backs NetStart.cpp's multiplayer wizard which only exists on Windows
	target_sources(${PROJECT_NAME} PRIVATE NetInfo.cpp)
endif(WIN32)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME CShell)

add_definitions(-D_CRT_SECURE_NO_WARNINGS -DRIOT_BUILD)

include_directories(.
	../Shared
	../ClientRes
	../../sdk/inc
	${SDL2_INCLUDE_DIRS})

# Only the shared LT1 engine variable defaults since this shell is converted
target_link_libraries(${PROJECT_NAME}
	LIB_DECompatDefaults
	${SDL2_LIBRARIES})

if(WIN32)
	target_link_libraries(${PROJECT_NAME} winmm)
endif(WIN32)

if(LINUX)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		BUILD_WITH_INSTALL_RPATH TRUE
		INSTALL_RPATH "$ORIGIN/")

	# Same link rule as Shogo/ObjectDLL
	target_link_options(${PROJECT_NAME} PRIVATE -Wl,--no-undefined)
endif(LINUX)

# Macro redefinitions are build errors here too
if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /we4005)
endif(MSVC)
