setup() {
    load 'test_helper/common-setup'
    _common_setup
}


@test "ws_editdb present" {
    which ws_editdb
}

@test "ws_editdb print version" {
    run ws_editdb --version
    assert_output --partial "ws_editdb"
    assert_success
}

@test "ws_editdb print help" {
    run ws_editdb --help
    assert_output --partial "Usage"
    assert_success
}
