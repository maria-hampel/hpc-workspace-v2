setup() {
    load 'test_helper/common-setup'
    _common_setup
}

@test "ws_list present" {
    which ws_list
}
@test "ws_list print version" {
    run ws_list --version
    assert_output --partial "workspace"
}
@test "ws_list print help" {
    run ws_list --help
    assert_output --partial "Usage"
}
