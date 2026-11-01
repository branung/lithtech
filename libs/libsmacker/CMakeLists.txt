# libsmacker 1.2.0 (Greg Kennedy, LGPL 2.1), the decoder SmackerVideoMgr uses.
project(LIB_Smacker C)

add_library(${PROJECT_NAME} STATIC smacker.c)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if (MSVC)
	target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
