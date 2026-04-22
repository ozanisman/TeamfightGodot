cmake_minimum_required(VERSION 3.20)

project(teamfight_batch_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

if(NOT DEFINED GODOT_CPP_DIR AND DEFINED ENV{GODOT_CPP_DIR})
	set(GODOT_CPP_DIR "$ENV{GODOT_CPP_DIR}")
endif()

if(NOT DEFINED GODOT_CPP_LIB AND DEFINED ENV{GODOT_CPP_LIB})
	set(GODOT_CPP_LIB "$ENV{GODOT_CPP_LIB}")
endif()

if(NOT DEFINED GODOT_CPP_DIR OR GODOT_CPP_DIR STREQUAL "")
	message(FATAL_ERROR "Set GODOT_CPP_DIR to a godot-cpp checkout that contains include/ and gen/include/")
endif()

if(NOT EXISTS "${GODOT_CPP_DIR}/include" OR NOT EXISTS "${GODOT_CPP_DIR}/gen/include")
	message(FATAL_ERROR "GODOT_CPP_DIR must contain include/ and gen/include/")
endif()

if(NOT DEFINED GODOT_CPP_LIB OR GODOT_CPP_LIB STREQUAL "")
	file(GLOB GODOT_CPP_LIB_CANDIDATES
		"${GODOT_CPP_DIR}/bin/godot-cpp*.lib"
		"${GODOT_CPP_DIR}/bin/godot-cpp*.a"
	)
	if(GODOT_CPP_LIB_CANDIDATES STREQUAL "")
		message(FATAL_ERROR "Could not find a godot-cpp library under ${GODOT_CPP_DIR}/bin")
	endif()
	list(SORT GODOT_CPP_LIB_CANDIDATES)
	list(GET GODOT_CPP_LIB_CANDIDATES 0 GODOT_CPP_LIB)
endif()

if(NOT EXISTS "${GODOT_CPP_LIB}")
	message(FATAL_ERROR "GODOT_CPP_LIB does not exist: ${GODOT_CPP_LIB}")
endif()

set(TEAMFIGHT_BATCH_CORE_SOURCES
	src/batch_match_engine.cpp
	src/batch_match_engine_native.cpp
	src/register_types.cpp
)

add_library(batch_core SHARED ${TEAMFIGHT_BATCH_CORE_SOURCES})

target_include_directories(batch_core PRIVATE
	"${GODOT_CPP_DIR}/include"
	"${GODOT_CPP_DIR}/gen/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(batch_core PRIVATE "${GODOT_CPP_LIB}")

target_compile_definitions(batch_core PRIVATE GDEXTENSION TYPED_METHOD_BIND WINDOWS_ENABLED NOMINMAX)

set_target_properties(batch_core PROPERTIES
	PREFIX ""
	SUFFIX ".dll"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin"
	ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin"
	ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin"
	LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin"
	LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin"
	OUTPUT_NAME_DEBUG "batch_core.windows.debug.x86_64"
	OUTPUT_NAME_RELEASE "batch_core.windows.release.x86_64"
)
