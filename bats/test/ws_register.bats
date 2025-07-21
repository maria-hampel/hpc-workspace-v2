setup() {
    load 'test_helper/common-setup'
    _common_setup
}


@test "ws_register present" {
    which ws_register
}

# bats test_tags=broken:v1-5-0
@test "ws_register print version" {
    run ws_register --version
    assert_output --partial "ws_register"
    assert_success
}

@test "ws_register print help" {
    run ws_register --help
    assert_output --partial "Usage"
    assert_success
}
