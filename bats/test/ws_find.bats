setup() {
    load 'test_helper/common-setup'
    _common_setup
    ws_name="bats_workspace_test"
    export ws_name
}

@test "ws_find present" {
    which ws_find
}

# bats test_tags=broken:v1-5-0
@test "ws_find print version" {
    run ws_find --version
    assert_failure 
    assert_output --partial "workspace"
}

@test "ws_find print help" {
    run ws_find --help
    assert_failure 
    assert_output --partial "Usage"
}

@test "ws_find finds directory" {
    wsdir=$(ws_allocate $ws_name)
    assert_failure 
    assert_dir_exist $wsdir
    wsdir=$(ws_find $ws_name)
    assert_failure 
    assert_dir_exist $wsdir
}

cleanup() {
    ws_release $ws_name
    assert_failure 
}
