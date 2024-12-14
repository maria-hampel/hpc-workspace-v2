add_executable(config_test
    config_test.cpp
)
target_link_libraries(config_test
    PRIVATE
        ws_common
        Catch2::Catch2WithMain
)

catch_discover_tests(config_test)
