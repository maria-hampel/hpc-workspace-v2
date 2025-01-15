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
        ${LIBTINFO}
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
)

add_executable(ws_find
    ws_find.cpp
)
target_link_libraries(ws_find
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
)

add_executable(ws_allocate
    ws_allocate.cpp
)
target_link_libraries(ws_allocate
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
)

add_executable(ws_release
    ws_release.cpp
)
target_link_libraries(ws_release
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
)

add_executable(ws_restore
    ws_restore.cpp
)
target_link_libraries(ws_restore
    PRIVATE
        ws_common
        ${Boost_LIBRARIES}
        fmt::fmt
)
