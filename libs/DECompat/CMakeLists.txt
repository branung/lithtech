project(LIB_DECompat)

# LithTech 1.0 compatibility layer
add_library(${PROJECT_NAME} STATIC
	de_engineobjects.cpp
	de_message.cpp
	de_server.cpp)

add_library(LIB_DECompatDefaults STATIC
	de_commandedges.cpp
	de_lt1defaults.cpp)
target_link_libraries(${PROJECT_NAME} PUBLIC LIB_DECompatDefaults)

if(LINUX)
    set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-fPIC")
    set_target_properties(LIB_DECompatDefaults PROPERTIES COMPILE_FLAGS "-fPIC")
endif(LINUX)

target_include_directories(LIB_DECompatDefaults PUBLIC
	.
	../../sdk/inc)

target_include_directories(${PROJECT_NAME} PUBLIC
	.
	lt1
	../../sdk/inc)

add_library(LIB_DECompatClient STATIC
	de_client.cpp)
target_link_libraries(LIB_DECompatClient PUBLIC LIB_DECompatDefaults)

if(LINUX)
    set_target_properties(LIB_DECompatClient PROPERTIES COMPILE_FLAGS "-fPIC")
endif(LINUX)

# iclientshell.h includes SDL.h
find_package(SDL2 REQUIRED)
target_include_directories(LIB_DECompatClient PUBLIC
	.
	lt1
	../../sdk/inc
	${SDL2_INCLUDE_DIRS})
