setup() {
    load 'test_helper/common-setup'
    _common_setup
}


@test "ws_editdb present" {
    which ws_editdb
}

# bats test_tags=sudo
@test "ws_editdb print version" {
    if [ "$EUID" -ne 0 ]; then
        skip "Needs to be run as root"
    fi
    run ws_editdb --version
    assert_output --partial "ws_editdb"
    assert_success
}

# bats test_tags=sudo
@test "ws_editdb print help" {
    if [ "$EUID" -ne 0 ]; then
        skip "Needs to be run as root"
    fi
    run ws_editdb --help
    assert_output --partial "Usage"
    assert_success
}
