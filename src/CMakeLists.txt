find_package(CURL)

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

add_executable(ws_list
    ws_list.cpp
)
target_link_libraries(ws_list
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
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
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
)

add_executable(ws_stat
    ws_stat.cpp
)
target_link_libraries(ws_stat
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
)

add_executable(ws_editdb
    ws_editdb.cpp
)
target_link_libraries(ws_editdb
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
)

add_executable(ws_send_ical
    ws_send_ical.cpp
)
target_link_libraries(ws_send_ical
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
        spdlog
        CURL::libcurl
)
