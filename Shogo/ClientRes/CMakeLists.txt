project(SHOGO_ClientRes)

# Client resource DLL

set(exec_src main.cpp)

if(LINUX)
	include_directories(../../libs/DynRes)
	list(APPEND exec_src ClientRes.rc.cpp)
	add_custom_command(OUTPUT ClientRes.rc.cpp
		COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/DynRes/rc.parser.py
			${CMAKE_CURRENT_SOURCE_DIR}/ClientRes.rc
			# DEPENDS reruns the generator when the .rc changes
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ClientRes.rc
			${CMAKE_CURRENT_SOURCE_DIR}/../../libs/DynRes/rc.parser.py)
endif(LINUX)

list(APPEND exec_src ClientRes.rc)

add_library(${PROJECT_NAME} SHARED ${exec_src})

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME CRes)

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
