project(Test_DtxLT1)

# Regression test for the LithTech 1.0 texture loader
remove_definitions(-D__D3D -D__D3DREND)

set(exec_src
	main.cpp
	../../runtime/shared/src/bdefs.cpp
	../../runtime/shared/src/dtxmgr.cpp
	../../runtime/shared/src/dtxmgr_lt1.cpp
	../../runtime/shared/src/genltstream.cpp
	../../runtime/shared/src/pixelformat.cpp)

find_package(SDL2 REQUIRED)

include_directories(
	${CMAKE_SOURCE_DIR}/runtime/shared/src
	${CMAKE_SOURCE_DIR}/runtime/kernel/src
	${CMAKE_SOURCE_DIR}/runtime/kernel/mem/src
	${CMAKE_SOURCE_DIR}/sdk/inc
	${CMAKE_SOURCE_DIR}/libs/stdlith
	${SDL2_INCLUDE_DIRS})


if(WIN32)
	include_directories(
		${CMAKE_SOURCE_DIR}/runtime/kernel/src/sys/win
		${CMAKE_SOURCE_DIR}/runtime/shared/src/sys/win)
else(WIN32)
	include_directories(
		${CMAKE_SOURCE_DIR}/runtime/kernel/src/sys/linux
		${CMAKE_SOURCE_DIR}/runtime/shared/src/sys/linux)
endif(WIN32)

add_executable(${PROJECT_NAME} ${exec_src})

set_target_properties(${PROJECT_NAME}
	PROPERTIES OUTPUT_NAME testDtxLT1)
