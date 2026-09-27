project(SHOGO_ServerRes)

# Server resource DLL, whose only real content is the .rc (MAIN.CPP gives it a DllMain)

set(exec_src MAIN.CPP)

if(LINUX)
	include_directories(../../libs/DynRes)
	list(APPEND exec_src ServerRes.rc.cpp)
	add_custom_command(OUTPUT ServerRes.rc.cpp
		COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/DynRes/rc.parser.py
			${CMAKE_CURRENT_SOURCE_DIR}/ServerRes.rc
		# DEPENDS re-runs the generator when the .rc changes
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ServerRes.rc
			${CMAKE_CURRENT_SOURCE_DIR}/../../libs/DynRes/rc.parser.py)
endif(LINUX)

list(APPEND exec_src ServerRes.rc)

add_library(${PROJECT_NAME} SHARED ${exec_src})

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME SRes)

include_directories(. ../Shared ../../sdk/inc)

if(LINUX)
	target_link_libraries(${PROJECT_NAME} LIB_DynRes)

	# Same link rule as Shogo/ObjectDLL
	target_link_options(${PROJECT_NAME} PRIVATE -Wl,--no-undefined)
endif(LINUX)

# Macro redefinitions are build errors here too
if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /we4005)
endif(MSVC)
