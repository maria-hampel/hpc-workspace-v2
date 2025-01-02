setup() {
    load 'test_helper/common-setup'
    _common_setup
    ws_name="bats_workspace_test"
    export ws_name
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

@test "ws_allocate creates directory" {
    wsdir=$(ws_allocate --config bats/ws.conf $ws_name)
    assert_file_exist $wsdir
}

cleanup() {
    ws_release --config bats/ws.conf $ws_name
}
