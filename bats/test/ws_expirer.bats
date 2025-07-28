setup() {
    load 'test_helper/common-setup'
    _common_setup
}


@test "ws_expirer present" {
    which ws_expirer
}

# bats test_tags=broken:v1-5-0
@test "ws_expirer print version" {
    run ws_expirer --version
    assert_output --partial "ws_expirer"
    assert_success
}

@test "ws_expirer print help" {
    run ws_expirer --help
    assert_output --partial "Usage"
    assert_success
}

@test "ws_expirer keep" {
    ws_allocate --config bats/ws.conf EXPIRE_TEST 1
    ws_editdb --config bats/ws.conf --not-kidding --add-time -1 EXPIRE_TEST
    run ws_expirer --config bats/ws.conf
    assert_output --regexp 'keeping .*-EXPIRE_TEST'
    assert_success
    ws_release --config bats/ws.conf EXPIRE_TEST
}

@test "ws_expirer expire" {
    ws_allocate --config bats/ws.conf EXPIRE_TEST 1
    ws_editdb --config bats/ws.conf --not-kidding --add-time -10 EXPIRE_TEST
    run ws_expirer --config bats/ws.conf
    assert_output --regexp 'expiring .*-EXPIRE_TEST'
    assert_success
    ws_release --config bats/ws.conf EXPIRE_TEST
}

#@test "ws_expirer released" {
#    ws_allocate --config bats/ws.conf EXPIRE_TEST 1
#    ws_release --config bats/ws.conf EXPIRE_TEST
#    run ws_expirer --config bats/ws.conf -c
#    assert_output --regexp 'expiring .*-EXPIRE_TEST'
#    assert_success
#    ws_editdb --config bats/ws.conf --not-kidding -e --add-time -20 "EXPIRE_TEST*"
#    run ws_expirer --config bats/ws.conf
#    assert_output --regexp 'expiring .*-EXPIRE_TEST'
#    assert_success
#    ws_release --config bats/ws.conf EXPIRE_TEST
#}
