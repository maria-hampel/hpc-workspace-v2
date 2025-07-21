setup() {
    load 'test_helper/common-setup'
    _common_setup
}


@test "ws_expirer present" {
    which ws_expirer
}

# bats test_tags=broken:v1-5-0
@test "ws_expirer print version" {
    run ws_expirer --version
    assert_output --partial "ws_expirer"
    assert_success
}

@test "ws_expirer print help" {
    run ws_expirer --help
    assert_output --partial "Usage"
    assert_success
}
