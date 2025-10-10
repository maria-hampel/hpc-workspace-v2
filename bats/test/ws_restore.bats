setup() {
    load 'test_helper/common-setup'
    ws_name="bats-workspace-test"
    _common_setup
}

@test "ws_restore present" {
    which ws_restore
}

@test "ws_restore print version" {
    run ws_restore --version
    assert_output --partial "ws_restore"
    assert_success
}

@test "ws_restore print help" {
    run ws_restore --help
    assert_output --partial "Usage"
    assert_success
}

@test "ws_restore list" {
    wsdir=$(ws_allocate --config bats/ws.conf $ws_name)
    ws_release --config bats/ws.conf $ws_name
    run ws_restore --config bats/ws.conf -l
    assert_output --partial $ws_name
    assert_success
}

@test "ws_restore list with pattern" {
    wsdir=$(ws_allocate --config bats/ws.conf $ws_name)
    ws_allocate --config bats/ws.conf DONTSHOW
    ws_release --config bats/ws.conf $ws_name
    ws_release --config bats/ws.conf DONTSHOW
    run ws_restore --config bats/ws.conf -l "bats*"
    assert_output --partial $ws_name
    refute_output --partial DONTSHOW
    assert_success
    run ws_restore --config bats/ws.conf -l
    assert_output --partial DONTSHOW
    assert_output --partial $ws_name
    assert_success
    run ws_restore --config bats/ws.conf -l "*SHOW*"
    assert_output --partial DONTSHOW
}

@test "ws_restore workspace" {
    ws_name=restore-$RANDOM
    wsdir=$(ws_allocate --config bats/ws.conf $ws_name)
    touch $wsdir/TESTFILE
    ws_release --config bats/ws.conf $ws_name
    wsid=$( ws_restore --config bats/ws.conf -l | grep $ws_name | head -1)
    wsdir=$(ws_allocate --config bats/ws.conf $ws_name)
    ws_restore_notest --config bats/ws.conf $wsid $ws_name
    assert_file_exists $wsdir/$wsid/TESTFILE
}

@test "ws_restore delete-data" {
    ws_name=restore-$RANDOM
    wsdir=$(ws_allocate --config bats/ws.conf -F ws1 $ws_name)
    touch $wsdir/TESTFILE
    ws_release --config bats/ws.conf -F ws1 $ws_name
    wsid=$( ws_restore --config bats/ws.conf -F ws1 -l | grep $ws_name | head -1)
    assert_file_exists /tmp/ws/ws1/.removed/$wsid/TESTFILE
    ws_restore_notest --config bats/ws.conf --delete-data $wsid
    assert_file_not_exists /tmp/ws/ws1/.removed/$wsid/TESTFILE
}

@test "ws_restore workspace with username and - in name" {
    ws_name=$USER-restore
    wsdir=$(ws_allocate --config bats/ws.conf $ws_name)
    touch $wsdir/TESTFILE
    run ws_release --config bats/ws.conf $ws_name
    assert_output --partial "released"
    assert_success
    wsid=$(ws_restore --config bats/ws.conf -l | grep $ws_name | head -1)
    wsdir=$(ws_allocate --config bats/ws.conf $ws_name)
    run ws_restore_notest --config bats/ws.conf $wsid $ws_name
    assert_success
    assert_file_exists $wsdir/$wsid/TESTFILE
}
