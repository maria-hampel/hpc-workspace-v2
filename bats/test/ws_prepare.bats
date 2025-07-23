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

# bats test_tags=sudo
@test "ws_prepare print version" {
    if [ "$EUID" -ne 0 ]; then
        skip "Needs to be run as root"
    fi
    run ws_prepare --version
    assert_output --partial "ws_prepare"
    assert_success
}

# bats test_tags=sudo
@test "ws_prepare print help" {
    if [ "$EUID" -ne 0 ]; then
        skip "Needs to be run as root"
    fi
    run ws_prepare --help
    assert_output --partial "Usage"
    assert_success
}

# bats test_tags=sudo
@test "ws_prepare no valid config file" {
    if [ "$EUID" -ne 0 ]; then
        skip "Needs to be run as root"
    fi
    rm -fr /tmp/ws
    run ws_prepare --config "bats/bad_ws.conf"
    assert_output --partial "warning: No adminmail in config!"
    assert_file_not_exist /tmp/ws
}

# bats test_tags=sudo
@test "ws_prepare create directories" {
    if [ "$EUID" -ne 0 ]; then
        skip "Needs to be run as root"
    fi
    rm -fr /tmp/ws
    env ws_prepare --config "bats/ws.conf"
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
    rm -fr /tmp/ws
}

# bats test_tags=sudo
@test "ws_prepare check existing directories - no change in permissions" {
    if [ "$EUID" -ne 0 ]; then
        skip "Needs to be run as root"
    fi
    run ws_prepare --config "bats/ws.conf"
    assert_output --partial "existed already"
    run rm -fr /tmp/ws
    assert_success
}
