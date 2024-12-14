cmake_minimum_required(VERSION 3.20)
if(CMAKE_VERSION GREATER_EQUAL 3.30)
    cmake_policy(SET CMP0167 NEW) # prefer cmake configuration provided by the boost package itself
endif()
if(CMAKE_VERSION GREATER_EQUAL 3.31)
    cmake_policy(SET CMP0177 NEW) # normalize all install DESTINATION values except for INCLUDES DESTINATION
endif()

project(workspace
    VERSION 2.0.0
    DESCRIPTION "Future sucessor of hpc-workspace"
    LANGUAGES C CXX
)
# set to version number of the latest release
add_definitions("-DWS_VERSION=\"${workspace_VERSION}\"")

if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/.git")
    add_definitions(-DIS_GIT_REPOSITORY)
    # git commit hash macro
    execute_process(
        COMMAND git log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    add_definitions("-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Use cmake -DCMAKE_BUILD_TYPE=Release for release build")

add_compile_options(-Wall -Wextra)
add_link_options($<$<CONFIG:Release>:-s>)
add_link_options($<$<CONFIG:MinSizeRel>:-s>)



OPTION(WS_ALLOW_USER_DEBUG "allow users to enable debugging and tracing" FALSE)
IF (WS_ALLOW_USER_DEBUG)
    ADD_DEFINITIONS("-DWS_ALLOW_USER_DEBUG")
ENDIF (WS_ALLOW_USER_DEBUG)


FIND_LIBRARY(HAVECAP NAMES libcap.a libcap.so)
IF (HAVECAP)
    MESSAGE("-- Found libcap")
	ADD_DEFINITIONS("-DWS_CAPA")
	SET(LIBCAP "cap")
ELSE (HAVECAP)
	SET(LIBCAP "")
ENDIF (HAVECAP)


list(APPEND BOOST_COMPONENTS system program_options)
find_package(Boost COMPONENTS ${BOOST_COMPONENTS} REQUIRED)


# find terminfo for "are you human" checker
FIND_LIBRARY(TERMINFO NAMES libtinfo.so libtinfo.a)
IF (TERMINFO)
    MESSAGE("-- Found terminfo")
    INCLUDE_DIRECTORIES(${TERMINFO_INCLUDE_DIRS})
    SET(LIBS ${LIBS} ${TERMINFO_LIBRARIES})
    SET(TLIB "tinfo")
ELSE (TERMINFO)
    MESSAGE("-- No terminfo, trying curses")
    #find curses for terminfo for "are you human" checker
    SET(CURSES_NEED_NCURSES TRUE)
    find_package(Curses REQUIRED)
    include_directories(${CURSES_INCLUDE_DIR})
    link_directories(${CURSES_LIBRARY_DIR})
    SET(TLIB ${CURSES_LIBRARIES})
ENDIF (TERMINFO)


OPTION(WS_PARALLEL "std::par usage, needs TBB" FALSE)
if (WS_PARALLEL)
    find_package(TBB REQUIRED tbb)
    ADD_DEFINITIONS("-DPARALLEL")
    SET(PARLIB "TBB::tbb")
else (WS_PARALLEL)
    SET(PARLIB "")
endif (WS_PARALLEL)

add_subdirectory(${workspace_SOURCE_DIR}/external/yaml-cpp)
add_subdirectory(${workspace_SOURCE_DIR}/external/rapidyaml ryml)
add_subdirectory(${workspace_SOURCE_DIR}/external/fmt)
add_subdirectory(${workspace_SOURCE_DIR}/external/GSL)

get_directory_property(LINKER_VAR LINK_DIRECTORIES)
message(STATUS "LINKER_VAR: ${LINKER_VAR}")

SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${workspace_BINARY_DIR}/bin)

ADD_LIBRARY(ws_common OBJECT 
	${workspace_SOURCE_DIR}/src/db.h
	${workspace_SOURCE_DIR}/src/dbv1.cpp 
	${workspace_SOURCE_DIR}/src/dbv1.h 
	${workspace_SOURCE_DIR}/src/utils.h
	${workspace_SOURCE_DIR}/src/utils.cpp 
	${workspace_SOURCE_DIR}/src/user.h
	${workspace_SOURCE_DIR}/src/user.cpp 
	${workspace_SOURCE_DIR}/src/config.h
	${workspace_SOURCE_DIR}/src/config.cpp
	${workspace_SOURCE_DIR}/src/caps.h
	${workspace_SOURCE_DIR}/src/caps.cpp
	)

target_link_libraries(ws_common
    PUBLIC
        fmt::fmt
        ryml::ryml
        yaml-cpp::yaml-cpp
    PRIVATE
        Microsoft.GSL::GSL
)

ADD_EXECUTABLE( ws_list ${workspace_SOURCE_DIR}/src/ws_list.cpp )
TARGET_LINK_LIBRARIES( ws_list "-L${LINKER_VAR}" ws_common ${Boost_LIBRARIES} ${EXTRA_STATIC_LIBS} ${LIBCAP} fmt::fmt ${PARLIB})

ADD_EXECUTABLE( ws_allocate ${workspace_SOURCE_DIR}/src/ws_allocate.cpp )
TARGET_LINK_LIBRARIES( ws_allocate "-L${LINKER_VAR}" ws_common ${Boost_LIBRARIES} ${EXTRA_STATIC_LIBS} ${LIBCAP} fmt::fmt ${PARLIB})



if(BUILD_TESTS)
    add_subdirectory(${workspace_SOURCE_DIR}/external/Catch2)

    include(CTest)
    include(Catch)

    add_subdirectory(tests)
endif()
