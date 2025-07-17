setup() {
    load 'test_helper/common-setup'
    _common_setup
}


@test "ws_prepare present" {
    which ws_prepare
}

@test "ws_prepare user not root" {
    run ws_prepare
    assert_output --partial "error"
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
    assert_output --partial "warn"
    #run ls /tmp/ws
    #assert_output --partial "No such file or directory"
}

@test "ws_prepare create directories" {
    sudo rm -fr /tmp/ws
    sudo env PATH=$PATH ws_prepare --config "bats/ws.conf"
    run ls /tmp/ws
    assert_output <<EOF1
    ws1
    ws1-db
    ws2
    ws2-db
EOF1
    run ls -la /tmp/ws/ws1-db
    assert_output --partial ".removed"
    assert_output --partial ".ws_db_magic"
    sudo rm -fr /tmp/ws 
}

@test "ws_prepare check existing directories - no change in permissions" {
    run sudo env PATH=$PATH ws_prepare --config "bats/ws.conf"
    assert_output --partial "existed already"
    run rm -fr /tmp/ws
    assert_success
}
