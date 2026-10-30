cmake_minimum_required(VERSION 3.16)
project(Lithtech)

if(NOT WIN32)
  find_package(PkgConfig)
  pkg_check_modules(DXVK dxvk-d3d9)
  if(DXVK_FOUND)
    set(GL OFF)
    set(VK ON)
  else()
    set(GL ON)
    set(VK OFF)
  endif()
endif()

option(BUILD_ENGINE "Build engine" ON)
option(BUILD_NOLF2 "Build No One Lives Forever 2 game code" ON)
option(BUILD_TRON "Build Tron 2.0 game code" OFF)
option(BUILD_BLOOD2 "Build Blood II game code" OFF)
option(BUILD_FEAR "Build F.E.A.R. game code" OFF)
option(BUILD_NOLF "Build No One Lives Forever game code" OFF)
option(BUILD_SHOGO "Build Shogo game code" OFF)
option(BUILD_TOOLS "Build tools" OFF)
option(BUILD_LAUNCHER "Build the game launcher" ON)
if(WIN32)
option(ENABLE_D3D "Enable Direct3D" ON)
option(ENABLE_DIRECTX "Enable DirectX" ON)
option(ENABLE_DIRECTSHOW "Enable DirectShow" ON)
option(ENABLE_OPENAL "Use OpenAL as audio backend" OFF)
option(ENABLE_DIRECTMUSIC "Use DirectMusic as music backend" ON)
else(WIN32)
option(ENABLE_OPENGL "Enable OpenGL 3.3" ${GL})
option(ENABLE_VULKAN "Enable Vulkan" OFF)
option(ENABLE_DXVK "Enable DXVK" ${VK})
option(ENABLE_OPENAL "Use OpenAL as audio backend" ON)
option(ENABLE_DIRECTMUSIC OFF)
endif(WIN32)
option(FINAL_BUILD "Strip the developer console, counters and diagnostics" OFF)
option(BUILD_TESTS "Build the offline loader checks (testDtxLT1, testModelABC, testWorldV56)" OFF)
option(USE_INTERNAL_ZLIB "Use bundled zlib" ON)			# eventually want default to be OFF
option(USE_INTERNAL_FREETYPE "Use bundled freetype" ON)	# eventually want default to be OFF

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/OUT")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED 17)

if (ENABLE_DXVK)
	option(ENABLE_D3D "Enable Direct3D" ON)
	pkg_check_modules(DXVK dxvk-d3d9)
	add_definitions(-DUSE_DXVK)
endif()

if (ENABLE_OPENAL AND NOT WIN32)
	pkg_check_modules(OPENAL REQUIRED openal)
	pkg_check_modules(MPG123 REQUIRED libmpg123)
endif()

if(ENABLE_D3D)
    add_definitions(-D__D3D -D__D3DREND)
endif(ENABLE_D3D)


if(WIN32)
    add_definitions(-D_WINDOWS)
else(WIN32)
    add_definitions(-D_LINUX -D__LINUX)
    set(LINUX 1)
endif(WIN32)

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
endif(MSVC)

if(DEBUG)
    add_definitions(-D_DEBUG)
elseif(FINAL_BUILD)
    add_definitions(-D_FINAL)
endif()


if(USE_INTERNAL_ZLIB)
    #set zlib vars
    add_subdirectory(libs/zlib)							# LIB_ZLib
else(USE_INTERNAL_ZLIB)
    find_package(ZLIB)
endif(USE_INTERNAL_ZLIB)

# move to Lib_UI CMakeLists.txt?
if(USE_INTERNAL_FREETYPE)
    #set freetype vars
else(USE_INTERNAL_FREETYPE)
    find_package(Freetype)
endif(USE_INTERNAL_FREETYPE)


add_subdirectory(libs/lith)								# LIB_Lith
add_subdirectory(libs/stdlith)							# LIB_StdLith
add_subdirectory(libs/ltmem)							# LIB_LTMem
add_subdirectory(libs/rezmgr)							# LIB_RezMgr
add_subdirectory(libs/RandomGen)                        # LIB_Random
add_subdirectory(libs/DECompat)                         # LIB_DECompat, the LithTech 1.0 compatibility layer

if(LINUX)
    set(JSON_BuildTests off)
    add_subdirectory(libs/DynRes)
    add_subdirectory(json)
endif(LINUX)

option(ENABLE_SMACKER "Play Smacker (.smk) movies with libsmacker" ON)
if (ENABLE_SMACKER)
	add_subdirectory(libsmacker)						# LIB_Smacker
endif()

# dmusic is static, as Windows ships its own dmusic.dll which a shared build would collide with
option(ENABLE_LT1_MUSIC "Play LT1 IMA music through the built-in ima.dll driver" ON)
if (ENABLE_LT1_MUSIC)
	set(DM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(DM_ENABLE_ASAN OFF CACHE BOOL "" FORCE)
	set(DM_BUILD_STATIC ON CACHE BOOL "" FORCE)
	add_subdirectory(libs/dmusic)						# dmusic, dmusic-tsf
endif()

if(BUILD_ENGINE)
    add_subdirectory(runtime/ui)						# LIB_UI
    add_subdirectory(runtime/info)						# LIB_Info
    add_subdirectory(runtime/sound)						# LIB_ILTSound
    add_subdirectory(runtime/server)					# DLL_Server
    add_subdirectory(runtime/client)					# EXE_Lithtech

	if(ENABLE_D3D)
		add_subdirectory(runtime/render_a/src/sys/d3d)	# LIB_D3DRender
	endif(ENABLE_D3D)
	 if(WIN32)
        if(NOT ENABLE_OPENAL)
            add_subdirectory(runtime/sound/src/sys/s_dx8)	# DLL_SndDrvDX8
        endif(NOT ENABLE_OPENAL)

        if(ENABLE_DIRECTSHOW)
            add_subdirectory(libs/dshow)					# LIB_DShow
        endif(ENABLE_DIRECTSHOW)

        add_subdirectory(runtime/winbuild/ltmsg)		# DLL_LTMsg
    endif(WIN32)
endif(BUILD_ENGINE)

if(BUILD_NOLF2)
    add_subdirectory(libs/MFCStub)						# LIB_MFCStub
    add_subdirectory(libs/ButeMgr)						# LIB_ButeMgr
    add_subdirectory(libs/CryptMgr)						# LIB_CryptMgr
    add_subdirectory(libs/RegMgr)						# LIB_RegMgr
    add_subdirectory(libs/LTGUIMgr)						# LIB_LTGUIMgr
    add_subdirectory(libs/ServerDir)					# ServerDir
    add_subdirectory(libs/WONAPI)						# LIB_WONAPI
    add_subdirectory(NOLF2/ClientFxDLL)					# ClientFXDLL
    add_subdirectory(NOLF2/ClientRes/TO2)				# ClientRes
    add_subdirectory(NOLF2/ClientShellDLL/TO2)			# ClientShellDLL
    add_subdirectory(NOLF2/ObjectDLL/TO2)				# ObjectDLL
    add_subdirectory(NOLF2/ServerRes/TO2)				# ServerRes
endif(BUILD_NOLF2)

if(BUILD_FEAR)
    if(NOT BUILD_NOLF2)
        add_subdirectory(libs/MFCStub)						# LIB_MFCStub
        add_subdirectory(libs/ButeMgr)						# LIB_ButeMgr
        add_subdirectory(libs/CryptMgr)						# LIB_CryptMgr
        add_subdirectory(libs/RegMgr)						# LIB_RegMgr
        add_subdirectory(libs/LTGUIMgr)						# LIB_LTGUIMgr
        add_subdirectory(libs/ServerDir)					# ServerDir
        add_subdirectory(libs/WONAPI)						# LIB_WONAPI
    endif(NOT BUILD_NOLF2)
    add_subdirectory(FEAR/ClientFxDLL)					# ClientFXDLL
#	add_subdirectory(NOLF2/ClientRes/TO2)				# ClientRes
    add_subdirectory(FEAR/ClientShellDLL)				# ClientShellDLL
#	add_subdirectory(NOLF2/ObjectDLL/TO2)				# ObjectDLL
#	add_subdirectory(NOLF2/ServerRes/TO2)				# ServerRes
endif(BUILD_FEAR)

if(BUILD_TRON)
    if(NOT BUILD_NOLF2)
        add_subdirectory(libs/MFCStub)
        add_subdirectory(libs/ButeMgr)
        add_subdirectory(libs/CryptMgr)
        add_subdirectory(libs/RegMgr)
        add_subdirectory(libs/LTGUIMgr)
        add_subdirectory(libs/ServerDir)
        add_subdirectory(libs/WONAPI)
        add_subdirectory(NOLF2/ClientFxDLL)		# ClientFXDLL
    endif(NOT BUILD_NOLF2)
    add_subdirectory(NOLF2/ClientRes/TRON)		# ClientRes
    add_subdirectory(NOLF2/ClientShellDLL/TRON)	# ClientShellDLL
    add_subdirectory(NOLF2/ObjectDLL/TRON)		# ObjectDLL
    add_subdirectory(NOLF2/ServerRes/TRON)		# ServerRes
endif(BUILD_TRON)
if(BUILD_SHOGO)
    # Shogo only needs game code and the engine SDK
    add_subdirectory(Shogo/ClientRes)           # CRes
    add_subdirectory(Shogo/ClientShellDLL)      # CShell
    add_subdirectory(Shogo/ServerRes)           # SRes

    # ObjectDLL builds against libs/DECompat and is off by default
    option(BUILD_SHOGO_OBJECTDLL "Build Shogo's server objects" OFF)
    if(BUILD_SHOGO_OBJECTDLL)
        add_subdirectory(Shogo/ObjectDLL)       # Object.lto
    endif(BUILD_SHOGO_OBJECTDLL)
endif(BUILD_SHOGO)
if(BUILD_BLOOD2)
    add_subdirectory(Blood2/ClientRes)          # CRes

    # Both DLLs are mid-port and off by default, so turning Blood II on doesn't break a tree that only wants CRes
    option(BUILD_BLOOD2_OBJECTDLL "Build Blood II's server objects (doesn't build yet)" OFF)
    if(BUILD_BLOOD2_OBJECTDLL)
        add_subdirectory(Blood2/ObjectDLL)      # Object.lto
    endif(BUILD_BLOOD2_OBJECTDLL)

    option(BUILD_BLOOD2_CLIENTSHELL "Build Blood II's client shell (doesn't build yet)" OFF)
    if(BUILD_BLOOD2_CLIENTSHELL)
        add_subdirectory(Blood2/ClientShellDLL) # CShell
    endif(BUILD_BLOOD2_CLIENTSHELL)
endif(BUILD_BLOOD2)

if(WIN32)
    function(lt_game_output subdir)
        foreach(target ${ARGN})
            if(TARGET ${target})
                set_target_properties(${target} PROPERTIES
                    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/OUT/$<CONFIG>/${subdir}")
            endif()
        endforeach()
    endfunction()

    lt_game_output(NOLF2      NOLF2_ClientShellDLL NOLF2_ClientRes NOLF2_ServerRes NOLF2_ObjectDLL)
    lt_game_output(NOLF2/Game NOLF2_ClientFXDLL)

    lt_game_output(Shogo      SHOGO_ClientShellDLL SHOGO_ClientRes SHOGO_ObjectDLL SHOGO_ServerRes)

    lt_game_output(Blood2     BLOOD2_ClientShellDLL BLOOD2_ClientRes BLOOD2_ObjectDLL)

endif(WIN32)

if(BUILD_TOOLS)
    add_subdirectory(tools/DtxView)			# TOOLS_DtxView
    add_subdirectory(tools/LithRez)			# TOOLS_LithRez
endif(BUILD_TOOLS)

if(BUILD_LAUNCHER)
    add_subdirectory(tools/launcher)		# LithLauncher
endif(BUILD_LAUNCHER)

if(NOT WIN32)
add_subdirectory(tests/RegMgr)
add_subdirectory(tests/CShell)
add_subdirectory(tests/DynRes)
add_subdirectory(tests/DynResDLL)
add_subdirectory(tests/rndgen)
endif(NOT WIN32)

if(BUILD_TESTS)
    add_subdirectory(tests/DtxLT1)
    add_subdirectory(tests/ModelABC)
    add_subdirectory(tests/WorldV56)
endif(BUILD_TESTS)

# Each game in a folder of its own under OUT/<config>_Builds
if(WIN32)
    find_package(SDL2 QUIET)

    # lt_game_package(<folder>
    #     TARGETS <targets>              Copied into the folder
    #     GAMEFOLDER_TARGETS <targets>   Copied into its Game folder (NOLF2's ClientFx.fxd)
    #     FILES <files>                  Copied when changed, so a rebuild resets autoexec.cfg to its default
    #     DIRS <names>                   Made in the folder
    #     LAUNCHER <name>                The launcher built into the folder as <name>.exe
    #     SERVER_NAME <name>)            ltserver copied as <name>.exe
    function(lt_game_package game)
        cmake_parse_arguments(PKG "" "LAUNCHER;SERVER_NAME" "TARGETS;GAMEFOLDER_TARGETS;FILES;DIRS" ${ARGN})

        set(dir "${CMAKE_BINARY_DIR}/OUT/$<CONFIG>_Builds/${game}")
        set(cmds COMMAND ${CMAKE_COMMAND} -E make_directory "${dir}")
        set(deps)

        foreach(t EXE_Lithtech DLL_Server DLL_SndDrvDX8 DLL_LTMsg ServerDir ${PKG_TARGETS})
            if(TARGET ${t})
                list(APPEND cmds COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${t}>" "${dir}")
                list(APPEND deps ${t})
            endif()
        endforeach()

        if(TARGET SDL2::SDL2)
            list(APPEND cmds COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:SDL2::SDL2>" "${dir}")
        endif()

        if(PKG_GAMEFOLDER_TARGETS)
            list(APPEND cmds COMMAND ${CMAKE_COMMAND} -E make_directory "${dir}/Game")
            foreach(t ${PKG_GAMEFOLDER_TARGETS})
                if(TARGET ${t})
                    list(APPEND cmds COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${t}>" "${dir}/Game")
                    list(APPEND deps ${t})
                endif()
            endforeach()
        endif()

        foreach(f ${PKG_FILES})
            list(APPEND cmds COMMAND ${CMAKE_COMMAND} -E copy_if_different "${f}" "${dir}")
        endforeach()

        foreach(d ${PKG_DIRS})
            list(APPEND cmds COMMAND ${CMAKE_COMMAND} -E make_directory "${dir}/${d}")
        endforeach()

        if(PKG_LAUNCHER AND COMMAND lt_add_launcher)
            string(TOUPPER ${game} prefix)
            lt_add_launcher(${prefix}_Launcher ${PKG_LAUNCHER} "${dir}")
        endif()

        if(PKG_SERVER_NAME AND TARGET ltserver)
            list(APPEND cmds COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:ltserver>"
                "${dir}/${PKG_SERVER_NAME}$<TARGET_FILE_SUFFIX:ltserver>")
            list(APPEND deps ltserver)
        endif()

        add_custom_target(${game}_Package ALL ${cmds} VERBATIM)
        if(deps)
            add_dependencies(${game}_Package ${deps})
        endif()
    endfunction()

    if(BUILD_SHOGO)
        lt_game_package(Shogo
            TARGETS SHOGO_ClientShellDLL SHOGO_ClientRes SHOGO_ObjectDLL SHOGO_ServerRes
            FILES ${CMAKE_SOURCE_DIR}/Shogo/Config/autoexec.cfg ${CMAKE_SOURCE_DIR}/Shogo/Config/defkeybd.cfg
                  ${CMAKE_SOURCE_DIR}/Shogo/Config/DetailHi.cfg ${CMAKE_SOURCE_DIR}/Shogo/Config/DetailMd.cfg
                  ${CMAKE_SOURCE_DIR}/Shogo/Config/DetailLo.cfg
            DIRS Custom
            LAUNCHER Shogo
            SERVER_NAME ShogoServer)
    endif()

    if(BUILD_BLOOD2)
        lt_game_package(Blood2
            TARGETS BLOOD2_ClientShellDLL BLOOD2_ClientRes BLOOD2_ObjectDLL
            LAUNCHER Blood2)
    endif()

    if(BUILD_NOLF2)
        lt_game_package(NOLF2
            TARGETS NOLF2_ClientShellDLL NOLF2_ClientRes NOLF2_ServerRes NOLF2_ObjectDLL
            GAMEFOLDER_TARGETS NOLF2_ClientFXDLL
            LAUNCHER NOLF2)
    endif()
endif(WIN32)