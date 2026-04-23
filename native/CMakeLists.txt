cmake_minimum_required(VERSION 3.24)
project(teamfight_simulation_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GODOT_CPP_PATH "" CACHE PATH "Path to the godot-cpp checkout")

add_library(teamfight_simulation_core SHARED
    src/register_types.cpp
    src/teamfight_simulation_core.cpp
)

target_include_directories(teamfight_simulation_core PRIVATE
    ${GODOT_CPP_PATH}/include
    ${GODOT_CPP_PATH}/gen/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(teamfight_simulation_core PRIVATE
    GD_EXTENSION_BUILD
)
