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
    ws_release --config bats/ws.conf EDITTEST
}

@test "ws_editdb rename workspaces" {
    ws_allocate --config bats/ws.conf -F ws1 TestWS 1
    ws_release --config bats/ws.conf -F ws1 TestWS
    OLDTIMEDB=$(ls /tmp/ws/ws1-db/.removed | grep "TestWS" | sed 's/.*-//' | sort -n | tail -1)
    OLDTIMEDIR=$(ls /tmp/ws/ws1/.removed | grep "TestWS" | sed 's/.*-//' | sort -n | tail -1)
    run echo $OLDTIMEDB
    assert_output --partial "1"
    run echo $OLDTIMEDIR
    assert_output --partial "1"
    ws_editdb --config bats/ws.conf -e -r --add-time 20 --not-kidding
    NEWTIMEDB=$(ls /tmp/ws/ws1-db/.removed | grep "TestWS" | sed 's/.*-//' | sort -n | tail -1)
    NEWTIMEDIR=$(ls /tmp/ws/ws1/.removed | grep "TestWS" | sed 's/.*-//' | sort -n | tail -1)
    run echo $NEWTIMEDB
    assert_output --partial "1"
    run echo $NEWTIMEDIR
    assert_output --partial "1"
    DIFF=$((NEWTIMEDB-OLDTIMEDB))
    assert_equal "$DIFF" 1728000
    DIFF=$((NEWTIMEDIR-OLDTIMEDIR))
    assert_equal "$DIFF" 1728000
}


