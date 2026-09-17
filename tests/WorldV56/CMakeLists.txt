project(Test_WorldV56)

# Regression test for the LithTech 1.0 world loader

set(exec_src
	main.cpp
	../../runtime/world/src/world_v56.cpp)

include_directories(
	${CMAKE_SOURCE_DIR}/runtime/world/src
	${CMAKE_SOURCE_DIR}/sdk/inc)

add_executable(${PROJECT_NAME} ${exec_src})

set_target_properties(${PROJECT_NAME}
	PROPERTIES OUTPUT_NAME testWorldV56)
