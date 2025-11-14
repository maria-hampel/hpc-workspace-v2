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

@test "ws_editdb dryrun" {
    ws_allocate --config bats/ws.conf EDITTEST 1
    run ws_editdb --config bats/ws.conf --add-time 1 EDITTEST
    assert_output --partial "change expiration"
    assert_success
}

@test "ws_editdb not kidding" {
    run ws_editdb --config bats/ws.conf --not-kidding --add-time 1 EDITTEST
    assert_output --partial "change expiration"
    assert_success
    run ws_list --config bats/ws.conf  EDITTEST
    assert_output --regexp "(2 days)|1 days, 23"
    assert_success
}

@test "ws_editdb ensure-until" {
    run ws_editdb --config bats/ws.conf --not-kidding --ensure-until 2050-12-31 EDITTEST
    assert_output --partial "change expiration"
    assert_success
    run ws_list --config bats/ws.conf EDITTEST
    assert_output --partial "expiration time      : Sat Dec 31 00:00:00 2050"
    assert_success
}

@test "ws_editdb expire-by" {
    run ws_editdb --config bats/ws.conf --not-kidding --expire-by 2049-12-31 EDITTEST
    assert_output --partial "change expiration"
    assert_success
    run ws_list --config bats/ws.conf EDITTEST
    assert_output --partial "expiration time      : Fri Dec 31 00:00:00 2049"
    assert_success
    ws_release --config bats/ws.conf EDITTEST
}