setup() {
    load 'test_helper/common-setup'
    _common_setup
    ws_name="bats_workspace_test"
    export ws_name
}

@test "ws_release present" {
    which ws_release
}

@test "ws_allocate print version" {
    run ws_release --version
    assert_failure
    assert_output --partial "workspace"
}

@test "ws_release print help" {
    run ws_release --help
    assert_failure
    assert_output --partial "Usage"
}

@test "ws_release releases directory" {
    wsdir=$(ws_allocate $ws_name)
    assert_failure
    assert_dir_exist $wsdir
    ws_release $ws_name
    assert_failure
    assert_dir_not_exist $wsdir
}

cleanup() {
    ws_release $ws_name
    assert_failure
}
