setup() {
    load 'test_helper/common-setup'
    _common_setup
    ws_name="bats_workspace_test"
    export ws_name
}


@test "ws_prepare present" {
    which ws_prepare
}

@test "ws_prepare user not root" {
    run ws_prepare
    assert_output --partial "Error"
}

@test "ws_prepare print version" {
    run sudo env PATH=$PATH ws_prepare --version
    assert_output --partial "workspace"
    assert_success
}

@test "ws_prepare print help" {
    run sudo env PATH=$PATH ws_prepare --help
    assert_output --partial "Usage"
    assert_success
}

@test "ws_prepare no valid config file" {
    sudo rm -fr /tmp/ws
    run sudo env PATH=$PATH ws_prepare --config "bats/bad_ws.conf"
    assert_output --partial "Error"
    run ls /tmp/ws
    assert_output --partial "No such file or directory"
}

@test "ws_prepare create directorys" {
    sudo rm -fr /tmp/ws
    sudo env PATH=$PATH ws_prepare --config "bats/ws.conf"
    run ls /tmp/ws
    assert_output <<EOF1
    ws1
    ws2
    ws2-db
EOF1
}

cleanup() {
    ws_release --config bats/ws.conf $ws_name
    assert_failure
}
