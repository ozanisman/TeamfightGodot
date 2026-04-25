cmake_minimum_required(VERSION 3.24)
project(teamfight_simulation_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(GODOT_CPP_PATH "" CACHE PATH "Path to the godot-cpp checkout")
if(NOT GODOT_CPP_PATH AND DEFINED ENV{GODOT_CPP_PATH})
    set(GODOT_CPP_PATH "$ENV{GODOT_CPP_PATH}")
endif()

if(NOT GODOT_CPP_PATH)
    foreach(candidate
        "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/godot-cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/godot-cpp"
        "${CMAKE_SOURCE_DIR}/../third_party/godot-cpp"
    )
        if(EXISTS "${candidate}/include/godot_cpp/core/class_db.hpp")
            set(GODOT_CPP_PATH "${candidate}")
            break()
        endif()
    endforeach()
endif()

if(NOT EXISTS "${GODOT_CPP_PATH}/include/godot_cpp/core/class_db.hpp")
    message(FATAL_ERROR
        "godot-cpp headers were not found. Set GODOT_CPP_PATH to a godot-cpp checkout "
        "or place one at third_party/godot-cpp.")
endif()

set(GODOTCPP_BUILD_PROFILE "" CACHE PATH "" FORCE)
set(GODOTCPP_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(GODOTCPP_TARGET "template_debug" CACHE STRING "" FORCE)
set(GODOTCPP_GDEXTENSION_DIR "${GODOT_CPP_PATH}/gdextension" CACHE PATH "" FORCE)
add_subdirectory("${GODOT_CPP_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/godot-cpp" EXCLUDE_FROM_ALL)

add_library(teamfight_simulation_core SHARED
    src/register_types.cpp
    src/teamfight_simulation_core.cpp
)

set_target_properties(teamfight_simulation_core PROPERTIES
    OUTPUT_NAME "teamfight_simulation_core"
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)

target_include_directories(teamfight_simulation_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GODOT_CPP_PATH}/include
    ${GODOT_CPP_PATH}/gen/include
)

target_compile_definitions(teamfight_simulation_core PRIVATE
    GD_EXTENSION_BUILD
)

target_link_libraries(teamfight_simulation_core PRIVATE godot-cpp)
