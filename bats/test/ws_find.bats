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
    assert_output --partial "workspace"
}

@test "ws_find print help" {
    run ws_find --help
    assert_output --partial "Usage"
}

@test "ws_find finds directory" {
    wsdir=$(ws_allocate --config bats/ws.conf $ws_name)
    assert_file_exist $wsdir
    wsdir=$(ws_find --config bats/ws.conf $ws_name)
    assert_file_exist $wsdir
}

@test "ws_find with filesystem" {
    run ws_find --config bats/ws.conf -F ws2 WS1TEST
    assert_output --partial Error
    assert_failure
}

@test "ws_find with good filesystem" {
    ws_allocate --config bats/ws.conf -F ws1 WS1TEST_GOOD
    run ws_find --config bats/ws.conf -F ws1 WS1TEST_GOOD
    assert_success
    rm /tmp/ws/ws1-db/${USER}-WS1TEST_GOOD
}

@test "ws_find with bad filesystem" {
    run ws_find --config bats/ws.conf -F DOESNOTEXIST
    assert_output --partial Error
    assert_failure
}


cleanup() {
    ws_release --config bats/ws.conf $ws_name
}
