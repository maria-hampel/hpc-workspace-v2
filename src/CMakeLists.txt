find_package(Threads REQUIRED)

# Check for C++17 thread support
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++17" COMPILER_SUPPORTS_CXX17)
if(NOT COMPILER_SUPPORTS_CXX17)
    message(FATAL_ERROR "Compiler does not support C++17")
endif()

add_library(ws_common
    build_info.h
    caps.cpp
    caps.h
    config.cpp
    config.h
    db.h
    dbv1.cpp
    dbv1.h
    user.cpp
    user.h
    utils.cpp
    utils.h
    UserConfig.cpp
)
target_compile_features(ws_common PUBLIC cxx_std_17)

target_link_libraries(ws_common
    PUBLIC
        fmt::fmt
        ryml::ryml
        yaml-cpp::yaml-cpp
        ${LIBCAP}
        spdlog
    PRIVATE
        Microsoft.GSL::GSL
)

# Mail library with curl dependency
add_library(mail
    mail.cpp
    mail.h
)
target_compile_features(mail PUBLIC cxx_std_17)
target_link_libraries(mail
    PUBLIC
        CURL::libcurl
        spdlog
    PRIVATE
        fmt::fmt
)
target_include_directories(mail PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_executable(ws_list
    ws_list.cpp
)
target_include_directories(ws_list PRIVATE ${thread_pool_SOURCE_DIR}/include)
target_link_libraries(ws_list
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
        Threads::Threads
)

add_executable(ws_find
    ws_find.cpp
)
target_link_libraries(ws_find
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
)

add_executable(ws_allocate
    ws_allocate.cpp
)
target_link_libraries(ws_allocate
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
)

add_executable(ws_release
    ws_release.cpp
)
target_link_libraries(ws_release
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
)

add_executable(ws_restore
    ws_restore.cpp
)
target_link_libraries(ws_restore
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
)

# create copy with different name for tests
# this file should not get installed!
add_custom_command(
 TARGET ws_restore
 POST_BUILD
 COMMAND cp ${CMAKE_BINARY_DIR}/bin/ws_restore ${CMAKE_BINARY_DIR}/bin/ws_restore_notest
 COMMENT "cp ${CMAKE_BINARY_DIR}/ws_restore ${CMAKE_BINARY_DIR}/ws_restore_notest"
)

add_executable(ws_prepare
    ws_prepare.cpp
)
target_link_libraries(ws_prepare
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
)

add_executable(ws_register
    ws_register.cpp
)
target_link_libraries(ws_register
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
)

add_executable(ws_expirer
    ws_expirer.cpp
)
target_link_libraries(ws_expirer
    PRIVATE
        ws_common
        mail
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
)

add_executable(ws_stat
    ws_stat.cpp
)
target_include_directories(ws_stat PRIVATE ${thread_pool_SOURCE_DIR}/include)
target_link_libraries(ws_stat
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
        Threads::Threads
)

add_executable(ws_editdb
    ws_editdb.cpp
)
target_include_directories(ws_editdb PRIVATE ${thread_pool_SOURCE_DIR}/include)
target_link_libraries(ws_editdb
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
        Threads::Threads
)

add_executable(ws_send_ical
    ws_send_ical.cpp
)
target_link_libraries(ws_send_ical
    PRIVATE
        ws_common
        mail
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
)

add_executable(ws_validate_config
    ws_validate_config.cpp
)
target_link_libraries(ws_validate_config
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
)

# Installation rules for user commands (BINDIR)
install(TARGETS
    ws_list
    ws_find
    ws_allocate
    ws_release
    ws_restore
    ws_register
    ws_send_ical
    ws_stat
    ws_validate_config
    COMPONENT Runtime
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Installation rules for admin commands (SBINDIR)
install(TARGETS
    ws_expirer
    ws_editdb
    ws_prepare
    COMPONENT Runtime
    RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR})

# Post-install privilege setup
set(WS_INSTALL_SET_PRIVILEGES ON CACHE BOOL "Set capabilities or setuid on privileged executables during install")

if(WS_INSTALL_SET_PRIVILEGES)
    if(LIBCAP)
        install(CODE "
file(TO_NATIVE_PATH \"\${CMAKE_INSTALL_PREFIX}/bin/ws_allocate\" _WS_CAPA_ALLOCATE)
execute_process(
    COMMAND setcap \"CAP_DAC_OVERRIDE=p CAP_CHOWN=p CAP_FOWNER=p\" \"\${_WS_CAPA_ALLOCATE}\"
    RESULT_VARIABLE _WS_CAPA_ALLOCATE_RC)
if(_WS_CAPA_ALLOCATE_RC)
    message(WARNING \"setcap failed for ws_allocate\")
else()
    message(STATUS \"setcap applied to ws_allocate\")
endif()")

        install(CODE "
file(TO_NATIVE_PATH \"\${CMAKE_INSTALL_PREFIX}/bin/ws_release\" _WS_CAPA_RELEASE)
execute_process(
    COMMAND setcap \"CAP_DAC_OVERRIDE=p CAP_CHOWN=p CAP_FOWNER=p\" \"\${_WS_CAPA_RELEASE}\"
    RESULT_VARIABLE _WS_CAPA_RELEASE_RC)
if(_WS_CAPA_RELEASE_RC)
    message(WARNING \"setcap failed for ws_release\")
else()
    message(STATUS \"setcap applied to ws_release\")
endif()")

        install(CODE "
file(TO_NATIVE_PATH \"\${CMAKE_INSTALL_PREFIX}/bin/ws_restore\" _WS_CAPA_RESTORE)
execute_process(
    COMMAND setcap \"CAP_DAC_OVERRIDE=p CAP_DAC_READ_SEARCH=p\" \"\${_WS_CAPA_RESTORE}\"
    RESULT_VARIABLE _WS_CAPA_RESTORE_RC)
if(_WS_CAPA_RESTORE_RC)
    message(WARNING \"setcap failed for ws_restore\")
else()
    message(STATUS \"setcap applied to ws_restore\")
endif()")
    else()
        install(CODE "
file(TO_NATIVE_PATH \"\${CMAKE_INSTALL_PREFIX}/bin/ws_allocate\" _WS_SUID_ALLOCATE)
file(TO_NATIVE_PATH \"\${CMAKE_INSTALL_PREFIX}/bin/ws_release\" _WS_SUID_RELEASE)
file(TO_NATIVE_PATH \"\${CMAKE_INSTALL_PREFIX}/bin/ws_restore\" _WS_SUID_RESTORE)
execute_process(
    COMMAND chmod u+s \"\${_WS_SUID_ALLOCATE}\" \"\${_WS_SUID_RELEASE}\" \"\${_WS_SUID_RESTORE}\"
    RESULT_VARIABLE _WS_SUILD_RC)
if(_WS_SUILD_RC)
    message(WARNING \"chmod u+s failed for privileged executables\")
else()
    message(STATUS \"setuid bit applied to ws_allocate, ws_release, ws_restore\")
endif()")
    endif()
endif()