cmake_minimum_required(VERSION 3.20)

# Handle newer CMake policies gracefully
if(CMAKE_VERSION GREATER_EQUAL 3.30)
    cmake_policy(SET CMP0167 NEW) # Prefer CMake configuration provided by the boost package itself
endif()

if(CMAKE_VERSION GREATER_EQUAL 3.31)
    cmake_policy(SET CMP0177 NEW) # Normalize all install DESTINATION values
endif()

project(workspace
    VERSION 2.0.0
    DESCRIPTION "Future successor of hpc-workspace"
    LANGUAGES C CXX
)

# Set version macro
add_compile_definitions(WS_VERSION="${workspace_VERSION}")

# Disable spdlog's internal fmt to use our own
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "" FORCE)

# Git commit hash if in repository
if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/.git")
    add_compile_definitions(IS_GIT_REPOSITORY=1)
    execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_COMMIT_HASH)
        add_compile_definitions(GIT_COMMIT_HASH="${GIT_COMMIT_HASH}")
        message(STATUS "Git commit: ${GIT_COMMIT_HASH}")
    endif()
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")


# Check for libcap (capability support)
find_library(LIBCAP NAMES cap libcap libcap.a libcap.so)
if(LIBCAP)
    message(STATUS "Found libcap: ${LIBCAP}. Building with capability support.")
    add_compile_definitions(WS_CAPA=1)
else()
    message(WARNING "libcap not found. Falling back to SUID.")
endif()

# Find Boost with program_options
find_package(Boost REQUIRED COMPONENTS program_options OPTIONAL_COMPONENTS system)

# External dependencies
set(YAML_CPP_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)

add_subdirectory(${workspace_SOURCE_DIR}/external/yaml-cpp)
add_subdirectory(${workspace_SOURCE_DIR}/external/rapidyaml ryml)
add_subdirectory(${workspace_SOURCE_DIR}/external/fmt)
add_subdirectory(${workspace_SOURCE_DIR}/external/GSL)
add_subdirectory(${workspace_SOURCE_DIR}/external/spdlog)

find_package(CURL REQUIRED)

# Set runtime output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${workspace_BINARY_DIR}/bin)

# Main source
add_subdirectory(src)

# Tests (optional)
if(BUILD_TESTS)
    add_subdirectory(${workspace_SOURCE_DIR}/external/Catch2)
    include(Catch)
    add_subdirectory(tests)
endif()

# Formatting target
file(GLOB_RECURSE ALL_SOURCE_FILES
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
     "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h")

add_custom_target(
  clang-format
  COMMAND clang-format -i -style=file ${ALL_SOURCE_FILES}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Running clang-format on source files"
)

message(STATUS "Project ${PROJECT_NAME} v${PROJECT_VERSION} configured successfully")
