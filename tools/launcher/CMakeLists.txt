cmake_minimum_required(VERSION 3.10)
project(LithLauncher CXX)

find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
	message(STATUS "SDL2 not found, so the launcher is not built")
	return()
endif()

add_executable(LithLauncher
	launcher_core.cpp
	launcher_sdl2.cpp)
target_include_directories(LithLauncher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${SDL2_INCLUDE_DIRS})
target_link_libraries(LithLauncher ${SDL2_LIBRARIES})

if(NOT MSVC)
	target_compile_options(LithLauncher PRIVATE -Wall -Wextra)
endif()

if(WIN32)
	set_target_properties(LithLauncher PROPERTIES WIN32_EXECUTABLE TRUE)

	if(TARGET SDL2::SDL2)
		add_custom_command(TARGET LithLauncher POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				$<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:LithLauncher>)
	endif()
endif()
