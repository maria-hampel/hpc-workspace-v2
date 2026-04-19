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

find_package(CURL REQUIRED)

# spdlog requires external fmt before it
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)

include(FetchContent)

# Speed up subsequent configures: skip re-fetching if deps are already present
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

FetchContent_Declare(yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG yaml-cpp-0.9.0
)
FetchContent_MakeAvailable(yaml-cpp)

FetchContent_Declare(rapidyaml
    GIT_REPOSITORY https://github.com/biojppm/rapidyaml.git
    GIT_TAG v0.11.1
)
FetchContent_MakeAvailable(rapidyaml)

FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 12.1.0
)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(GSL
    GIT_REPOSITORY https://github.com/microsoft/GSL.git
    GIT_TAG v4.2.1
)
FetchContent_MakeAvailable(GSL)

FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.1
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(thread_pool
    GIT_REPOSITORY https://github.com/bshoshany/thread-pool.git
    GIT_TAG v5.1.0
)
FetchContent_MakeAvailable(thread_pool)

if(BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
    )
    FetchContent_MakeAvailable(Catch2)
endif()

# Set runtime output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${workspace_BINARY_DIR}/bin)

# Main source
add_subdirectory(src)

# Tests (optional)
if(BUILD_TESTS)
    include(Catch)
    enable_testing()
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
