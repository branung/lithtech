cmake_minimum_required(VERSION 3.10)
project(LTCallisto CXX)

find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
	message(STATUS "SDL2 not found, so the launcher is not built")
	return()
endif()

set(LT_LAUNCHER_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "")

# The build stamp in the launchers' footer, and in the zip's name (stamp.cmake), taken again on every build.
# The source exists from configure on, so a launcher target in another folder can list it
set(LT_LAUNCHER_STAMP_CPP ${CMAKE_BINARY_DIR}/tools/launcher/ltcallisto_stamp.cpp CACHE INTERNAL "")
set(LT_LAUNCHER_STAMP_TXT ${CMAKE_BINARY_DIR}/tools/launcher/ltcallisto_stamp.txt CACHE INTERNAL "")
if(NOT EXISTS ${LT_LAUNCHER_STAMP_CPP})
	file(WRITE ${LT_LAUNCHER_STAMP_CPP} "const char *g_szLauncherBuild = \"unstamped\";\n")
endif()
add_custom_target(LTCallisto_Stamp
	COMMAND ${CMAKE_COMMAND} -DSTAMP_CPP=${LT_LAUNCHER_STAMP_CPP} -DSTAMP_TXT=${LT_LAUNCHER_STAMP_TXT}
		-P ${LT_LAUNCHER_DIR}/stamp.cmake
	BYPRODUCTS ${LT_LAUNCHER_STAMP_CPP} ${LT_LAUNCHER_STAMP_TXT}
	VERBATIM)

# lt_launcher_target(<target> [<profile>]): a launcher for <profile>'s game, or without one the combined launcher listing every game
function(lt_launcher_target target)
	add_executable(${target}
		${LT_LAUNCHER_DIR}/launcher_core.cpp
		${LT_LAUNCHER_DIR}/launcher_sdl2.cpp
		${LT_LAUNCHER_DIR}/launcher_splash.cpp
		${LT_LAUNCHER_STAMP_CPP})
	add_dependencies(${target} LTCallisto_Stamp)
	target_include_directories(${target} PRIVATE ${LT_LAUNCHER_DIR} ${SDL2_INCLUDE_DIRS})
	target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/libs/rezmgr ${CMAKE_SOURCE_DIR}/libs/lith ${CMAKE_SOURCE_DIR}/sdk/inc)
	target_link_libraries(${target} ${SDL2_LIBRARIES} LIB_RezMgr)

	if(ARGC GREATER 1)
		target_compile_definitions(${target} PRIVATE LTLAUNCH_PROFILE=${ARGV1})
	endif()

	if(NOT MSVC)
		target_compile_options(${target} PRIVATE -Wall -Wextra)
	endif()

	if(WIN32)
		set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE TRUE)
		target_link_libraries(${target} ole32 uuid) # The folder dialog
	endif()
endfunction()

# lt_add_launcher(<target> <name> <dir>): <name>.exe in <dir>, launching <name>Profile's game from that folder
function(lt_add_launcher target name dir)
	lt_launcher_target(${target} ${name}Profile)
	set_target_properties(${target} PROPERTIES
		OUTPUT_NAME ${name}
		RUNTIME_OUTPUT_DIRECTORY "${dir}"
		PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/launcher")
endfunction()

lt_launcher_target(LTCallisto)

if(WIN32)
	target_sources(LTCallisto PRIVATE ${LT_LAUNCHER_DIR}/ltcallisto.rc) # Its icon
endif()

if(WIN32 AND TARGET SDL2::SDL2)
	add_custom_command(TARGET LTCallisto POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:LTCallisto>)
endif()
