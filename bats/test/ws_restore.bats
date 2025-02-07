setup() {
    load 'test_helper/common-setup'
    ws_name="bats_workspace_test"
    _common_setup
}

@test "ws_restore present" {
    which ws_restore
}

@test "ws_restore print version" {
    run ws_restore --version
    assert_output --partial "workspace"
    assert_success
}

@test "ws_restore list" {
    wsdir=$(ws_allocate --config bats/ws.conf $ws_name)
    ws_release --config bats/ws.conf $ws_name
    run ws_restore --config bats/ws.conf -l
    assert_output --partial $ws_name
    assert_success
}

@test "ws_restore print help" {
    run ws_restore --help
    assert_output --partial "Usage"
    assert_success
}
