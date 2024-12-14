setup() {
    load 'test_helper/common-setup'
    _common_setup
}

@test "ws_allocate present" {
    which ws_allocate
}
@test "ws_allocate print version" {
    run ws_allocate --version
    assert_output --partial "workspace"
}
@test "ws_allocate print help" {
    run ws_allocate --help
    assert_output --partial "Usage"
}
