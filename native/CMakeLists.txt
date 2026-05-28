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
# godot-cpp template is chosen at configure time (single library per build tree).
# Default to template_release unless CMAKE_BUILD_TYPE is explicitly Debug.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(GODOTCPP_TARGET "template_debug" CACHE STRING "" FORCE)
else()
	set(GODOTCPP_TARGET "template_release" CACHE STRING "" FORCE)
endif()
set(GODOTCPP_GDEXTENSION_DIR "${GODOT_CPP_PATH}/gdextension" CACHE PATH "" FORCE)
add_subdirectory("${GODOT_CPP_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/godot-cpp" EXCLUDE_FROM_ALL)

set(TEAMFIGHT_SIMULATION_SOURCES
    src/register_types.cpp
    src/simulation/sim_stats.cpp
    src/simulation/sim_spatial.cpp
    src/simulation/sim_targeting.cpp
    src/simulation/sim_targeting_strategies.cpp
    src/simulation/sim_damage.cpp
    src/simulation/sim_status.cpp
    src/simulation/sim_periodic.cpp
    src/simulation/sim_channel.cpp
    src/simulation/sim_stats_modifiers.cpp
    src/simulation/sim_combat.cpp
    src/simulation/sim_movement.cpp
    src/simulation/sim_effects_compile.cpp
    src/simulation/sim_effects_host.cpp
    src/simulation/sim_effects_exec.cpp
    src/simulation/sim_effects_exec_damage.cpp
    src/simulation/sim_effects_exec_status.cpp
    src/simulation/sim_effects_exec_spawn.cpp
    src/simulation/sim_effects_exec_aoe.cpp
    src/simulation/sim_unit_tick.cpp
    src/simulation/sim_catalog.cpp
    src/simulation/sim_match.cpp
    src/simulation/sim_match_spawn.cpp
    src/simulation/sim_unit_builder.cpp
    src/simulation/sim_match_roster.cpp
    src/simulation/sim_match_lifecycle.cpp
    src/simulation/sim_match_loop.cpp
    src/teamfight_simulation_core.cpp
)

add_library(teamfight_simulation_core SHARED ${TEAMFIGHT_SIMULATION_SOURCES})

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(teamfight_simulation_core PRIVATE
        $<$<CONFIG:Release>:-flto>
    )
    target_link_options(teamfight_simulation_core PRIVATE
        $<$<CONFIG:Release>:-flto>
    )
elseif(MSVC)
    target_compile_options(teamfight_simulation_core PRIVATE
        $<$<CONFIG:Release>:/GL>
    )
    target_link_options(teamfight_simulation_core PRIVATE
        $<$<CONFIG:Release>:/LTCG>
    )
endif()

# Generate champion schema JSON from GDScript before building
set(CHAMPION_SCHEMA_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/goldens/champion_schema.json")
set(EXPORT_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/tools/export_champion_schema.gd")

add_custom_command(
    OUTPUT ${CHAMPION_SCHEMA_OUTPUT}
    COMMAND godot --headless --script ${EXPORT_SCRIPT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    COMMENT "Generating champion schema from GDScript"
    VERBATIM
)

add_custom_target(generate_champion_schema
    DEPENDS ${CHAMPION_SCHEMA_OUTPUT}
)

add_dependencies(teamfight_simulation_core generate_champion_schema)

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
