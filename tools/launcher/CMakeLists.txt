cmake_minimum_required(VERSION 3.10)
project(LithLauncher CXX)

find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
	message(STATUS "SDL2 not found, so the launcher is not built")
	return()
endif()

set(LT_LAUNCHER_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "")

# lt_launcher_target(<target> [<profile>]): a launcher for <profile>'s game (LithLauncherProfile by default)
function(lt_launcher_target target)
	add_executable(${target}
		${LT_LAUNCHER_DIR}/launcher_core.cpp
		${LT_LAUNCHER_DIR}/launcher_sdl2.cpp
		${LT_LAUNCHER_DIR}/launcher_splash.cpp)
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

lt_launcher_target(LithLauncher)

if(WIN32 AND TARGET SDL2::SDL2)
	add_custom_command(TARGET LithLauncher POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:LithLauncher>)
endif()
