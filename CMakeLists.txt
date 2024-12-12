cmake_minimum_required(VERSION 3.20)

project(workspace
    VERSION 2.0.0
    DESCRIPTION "Future sucessor of hpc-workspace"
    LANGUAGES C CXX
)
# set to version number of the latest release
add_definitions("-DWS_VERSION=\"${workspace_VERSION}\"")

message(STATUS "build type: " ${CMAKE_BUILD_TYPE})
message(STATUS "use cmake -DCMAKE_BUILD_TYPE=debug for debug build")

SET(CMAKE_CXX_FLAGS "-Wall -Wno-deprecated-declarations -Wno-unused-variable -Wno-effc++ -std=c++17 -Os -DTBB_SUPPRESS_DEPRECATED_MESSAGES=1 ")
SET(CMAKE_CXX_FLAGS_DEBUG "-Wall -Wno-deprecated-declarations -Wno-unused-variable -Wno-effc++ -std=c++17 -g -Os -DTBB_SUPPRESS_DEPRECATED_MESSAGES=1 -fstack-protector-all -fcf-protection=full  -fsanitize=address -D_GLIBCXX_ASSERTIONS")
SET(CMAKE_CXX_FLAGS_RELEASE "-Wall -Wno-deprecated-declarations -Wno-unused-variable -Wno-effc++ -std=c++17 -Os -DTBB_SUPPRESS_DEPRECATED_MESSAGES=1 ")


OPTION(WS_STATIC "static linking" FALSE)
IF (WS_STATIC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
    set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")
    SET(EXTRA_STATIC_LIBS "pthread")
#    set(Boost_USE_STATIC_LIBS ON)
#    set(Boost_USE_STATIC_RUNTIME ON) 
ELSE (WS_STATIC)
    set(Boost_USE_STATIC_LIBS OFF)
    set(Boost_USE_STATIC_RUNTIME OFF) 
ENDIF (WS_STATIC)

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


# find termcap for "are you human" checker
#FIND_LIBRARY(TERMCAP NAMES libtermcap.so libtermcap.a)
#IF (TERMCAP)
    #MESSAGE("-- Found termcap")
    #INCLUDE_DIRECTORIES(${TERMCAP_INCLUDE_DIRS})
    #SET(LIBS ${LIBS} ${TERMCAP_LIBRARIES})
#ELSE (TERMCAP)
    #MESSAGE(FATAL_ERROR "-- No termcap, please get a termcap library!")
#ENDIF (TERMCAP)

OPTION(WS_PARALLEL "std::par usage, needs TBB" FALSE)
if (WS_PARALLEL)
    find_package(TBB REQUIRED tbb)
    ADD_DEFINITIONS("-DPARALLEL")
    SET(PARLIB "TBB::tbb")
else (WS_PARALLEL)
    SET(PARLIB "")
endif (WS_PARALLEL)

ADD_SUBDIRECTORY(${workspace_SOURCE_DIR}/external/yaml-cpp)
add_subdirectory(${workspace_SOURCE_DIR}/external/rapidyaml ryml)
include_directories(${RYAML_INCLUDE_DIR})
ADD_SUBDIRECTORY(${workspace_SOURCE_DIR}/external/fmt)

get_directory_property(LINKER_VAR LINK_DIRECTORIES)
message(STATUS "LINKER_VAR: ${LINKER_VAR}")

IF (IS_DIRECTORY "${PROJECT_SOURCE_DIR}/.git")
    add_definitions(-DIS_GIT_REPOSITORY)
    # git commit hash macro
    execute_process(
       COMMAND git log -1 --format=%h
       WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
       OUTPUT_VARIABLE GIT_COMMIT_HASH
       OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    add_definitions("-DGIT_COMMIT_HASH=\"${GIT_COMMIT_HASH}\"")
ENDIF ()

SET (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${workspace_BINARY_DIR}/bin)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")


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

TARGET_LINK_LIBRARIES(ws_common PUBLIC yaml-cpp::yaml-cpp fmt::fmt ryml::ryml c4core::c4core)

ADD_EXECUTABLE( ws_list ${workspace_SOURCE_DIR}/src/ws_list.cpp )
TARGET_LINK_LIBRARIES( ws_list "-L${LINKER_VAR}" ws_common ${Boost_LIBRARIES} ${EXTRA_STATIC_LIBS} ${LIBCAP} yaml-cpp::yaml-cpp fmt::fmt ryml::ryml c4core::c4core ${PARLIB})

ADD_EXECUTABLE( ws_allocate ${workspace_SOURCE_DIR}/src/ws_allocate.cpp )
TARGET_LINK_LIBRARIES( ws_allocate "-L${LINKER_VAR}" ws_common ${Boost_LIBRARIES} ${EXTRA_STATIC_LIBS} ${LIBCAP} yaml-cpp::yaml-cpp fmt::fmt ryml::ryml c4core::c4core ${PARLIB})



# Get install target
#set(PROGRAM_PERMISSIONS_DEFAULT
#    OWNER_WRITE OWNER_READ OWNER_EXECUTE
#    GROUP_READ GROUP_EXECUTE
#    WORLD_READ WORLD_EXECUTE)
#install (FILES bin/ws_extend bin/ws_find bin/ws_list bin/ws_register bin/ws_send_ical DESTINATION bin PERMISSIONS ${PROGRAM_PERMISSIONS_DEFAULT})
#install(TARGETS
#      ws_allocate ws_release ws_restore
#      DESTINATION bin
#      PERMISSIONS ${PROGRAM_PERMISSIONS_DEFAULT} SETUID)
#install (FILES sbin/ws_expirer sbin/ws_restore sbin/ws_validate_config DESTINATION sbin PERMISSIONS ${PROGRAM_PERMISSIONS_DEFAULT})
#
## Install man pages
#INSTALL(FILES man/ws_allocate.1 man/ws_find.1 man/ws_register.1
#              man/ws_restore.1 man/ws_extend.1 man/ws_list.1 man/ws_release.1
#              man/ws_send_ical.1             DESTINATION share/man/man1)


if(BUILD_TESTS)
	ADD_SUBDIRECTORY(${workspace_SOURCE_DIR}/external/Catch2)
	#find_package(Catch2 REQUIRED)
	#add_executable(config_test tests/config_test.cpp src/config.cpp src/utils.cpp src/dbv1.cpp src/capability.cpp)
	add_executable(config_test tests/config_test.cpp src/config.cpp src/config.h src/utils.h src/utils.cpp src/dbv1.cpp src/dbv1.h src/capability.cpp)
	target_link_libraries(config_test PRIVATE  Catch2::Catch2WithMain yaml-cpp::yaml-cpp fmt::fmt ryml::ryml c4core::c4core ${PARLIB})

	include(CTest)
	include(Catch)
	catch_discover_tests(config_test)
ENDIF (BUILD_TESTS)
