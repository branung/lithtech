project(BLOOD2_ClientShellDLL)

# Blood II's client shell. The sources are ClientShellDLL.dsp's

find_package(SDL2 REQUIRED)

add_library(${PROJECT_NAME} SHARED
	BaseLineSystemFX.cpp
	BaseParticleSystemFX.cpp
	BloodClientShell.cpp
	BloodSplatFX.cpp
	BloodTrailFX.cpp
	BloodTrailSegmentFX.cpp
	CastLineFX.cpp
	client_physics.cpp
	ClientUtilities.cpp
	CommLink.cpp
	ConfigMgr.cpp
	Credits.cpp
	CreditsWin.cpp
	debugline.cpp
	ExplosionFX.cpp
	FlashlightFX.cpp
	FragInfo.cpp
	GameStates.cpp
	GameWeapons.cpp
	gibfx.cpp
	IKChainFX.cpp
	IpMgr.cpp
	KeyFixes.cpp
	LaserBeamFX.cpp
	LightFX.cpp
	LightningFX.cpp
	LightningSegmentFX.cpp
	LoadSave.cpp
	LoadScreenData.cpp
	MainMenus.cpp
	MarkSFX.cpp
	MenuBase.cpp
	MenuBloodBath.cpp
	MenuCharacter.cpp
	MenuCharacterFiles.cpp
	MenuCharacterSelect.cpp
	MenuControls.cpp
	MenuCustomLevel.cpp
	MenuDifficulty.cpp
	MenuDisplay.cpp
	MenuJoystick.cpp
	MenuJoystickAxis.cpp
	MenuKeyboard.cpp
	MenuLoadGame.cpp
	MenuMain.cpp
	MenuMouse.cpp
	MenuOptions.cpp
	MenuSaveGame.cpp
	MenuSinglePlayer.cpp
	MenuSound.cpp
	MessageBoxHandler.cpp
	MessageMgr.cpp
	MoveMgr.cpp
	Music.cpp
	NetInfo.cpp
	NetStart.cpp
	NewStatusBar.cpp
	ObjectFX.cpp
	ParticleExplosionFX.cpp
	ParticleStreamFX.cpp
	ParticleSystemFX.cpp
	PickupObjectFX.cpp
	plasma1.cpp
	PlayerCamera.cpp
	PolyGridFX.cpp
	RainFX.cpp
	RippleFX.cpp
	SFXMgr.cpp
	SharedResourceMgr.cpp
	ShellCasingFX.cpp
	SmokeFX.cpp
	SmokeImpactFX.cpp
	SmokePuffFX.cpp
	SmokeTrailFX.cpp
	SmokeTrailSegmentFX.cpp
	Sparam.cpp
	SparksFX.cpp
	SpecialFXList.cpp
	Splash.cpp
	SplashFX.cpp
	SurfaceFragmentFX.cpp
	TheVoice.cpp
	TracerFX.cpp
	TripLaserFX.cpp
	viewcreature.cpp
	ViewWeapon.cpp
	VoiceMgr.cpp
	WarpGateFX.cpp
	WeaponFX.cpp
	WeaponPowerupFX.cpp
	WinUtil.cpp
	# Shared with ObjectDLL
	../Shared/TeamMgr.cpp
	../Shared/WeaponDefs.cpp
	../Misc/BareList.cpp
	../../sdk/inc/ltmodule.cpp
	../../sdk/inc/iltbaseclass.cpp
	../../sdk/inc/ltobjref.cpp
	../../sdk/inc/ltquatbase.cpp
	../../sdk/inc/LTEulerAngles.cpp)

if(WIN32)
	target_sources(${PROJECT_NAME} PRIVATE ClientShellDLL.rc)
endif(WIN32)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME CShell)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

include_directories(.
	../Shared
	../Misc
	../ClientRes
	../../sdk/inc
	${SDL2_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME}
	${SDL2_LIBRARIES})

if(WIN32)
	target_link_libraries(${PROJECT_NAME} winmm)
endif(WIN32)

if(LINUX)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		BUILD_WITH_INSTALL_RPATH TRUE
		INSTALL_RPATH "$ORIGIN/")
	target_link_options(${PROJECT_NAME} PRIVATE -Wl,--no-undefined)
endif(LINUX)

# Macro redefinitions are build errors here, as in ObjectDLL
if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /we4005)
endif(MSVC)
