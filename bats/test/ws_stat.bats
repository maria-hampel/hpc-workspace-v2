setup() {
    load 'test_helper/common-setup'
    _common_setup
}


@test "ws_stat present" {
    which ws_stat
}

# bats test_tags=broken:v1-5-0
@test "ws_stat print version" {
    run ws_stat --version
    assert_output --partial "ws_stat"
    assert_success
}

@test "ws_stat print help" {
    run ws_stat --help
    assert_output --partial "Usage"
    assert_success
}

@test "ws_stat count files" {
    run ws_stat --config bats/ws.conf TEST
    assert_output --partial "files               : 0"
    assert_success
    touch $(ws_find --config bats/ws.conf TEST)/TESTFILE
    run ws_stat --config bats/ws.conf TEST
    assert_output --partial "files               : 1"
    assert_success
    rm -f $(ws_find --config bats/ws.conf TEST)/TESTFILE
}
