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
    assert_output --partial "workspace"
    assert_success
}

@test "ws_restore print help" {
    run ws_restore --help
    assert_output --partial "Usage"
    assert_success
}

@test "ws_restore list" {
    wsdir=$(ws_allocate  $ws_name)
    ws_release $ws_name
    run ws_restore -l
    assert_output --partial $ws_name
    assert_success
}

@test "ws_restore workspace" {
    ws_name=cap-restore-$RANDOM
    wsdir=$(ws_allocate  $ws_name)
    touch $wsdir/TESTFILE
    ws_release  $ws_name
    wsid=$( ws_restore -l | grep $ws_name | head -1)
    wsdir=$(ws_allocate $ws_name)
    ws_restore_notest $wsid $ws_name
    assert_file_exists $wsdir/$wsid/TESTFILE
}
