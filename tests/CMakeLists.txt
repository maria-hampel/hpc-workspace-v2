add_executable(config_test
    config_test.cpp
)
target_link_libraries(config_test
    PRIVATE
        ws_common
        Catch2::Catch2WithMain
)
catch_discover_tests(config_test)


add_executable(caps_test
    caps_test.cpp
)
target_link_libraries(caps_test
    PRIVATE
        ws_common
        Catch2::Catch2WithMain
)
catch_discover_tests(caps_test)

add_executable(UserConfig_test
    UserConfig_test.cpp
)
target_link_libraries(UserConfig_test
    PRIVATE
        ws_common
        Catch2::Catch2WithMain
)
catch_discover_tests(UserConfig_test)
