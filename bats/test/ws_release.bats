setup() {
    load 'test_helper/common-setup'
    _common_setup
    ws_name="bats_workspace_test"
    ws_name2="bats_workspace_test2"
    export ws_name
}

@test "ws_release present" {
    which ws_release
}

@test "ws_release print version" {
    run ws_release --version
    assert_output --partial "ws_release"
    assert_success
}

@test "ws_release print help" {
    run ws_release --help
    assert_output --partial "Usage"
    assert_success
}

@test "ws_release bad fs" {
    run ws_release --config bats/ws.conf -F wsX NONEXISTING
    assert_output --partial "aborting, no valid filesystem given."
    assert_failure
}

@test "ws_release non-existing" {
    run ws_release -u someuser --config bats/ws.conf NONEXISTING
    assert_output --partial "Ignoring username option"
    assert_output --partial "Non-existent workspace given"
    assert_failure
}

@test "ws_release existing" {
    ws_allocate --config bats/ws.conf -F ws1 EXISTING
    run ws_release --config bats/ws.conf EXISTING
    assert_output --partial "workspace EXISTING released"
    assert_success
}

@test "ws_release non-unique" {
    ws_allocate --config bats/ws.conf -F ws1 DOUBLENAME
    ws_allocate --config bats/ws.conf -F ws2 DOUBLENAME
    run ws_release --config bats/ws.conf DOUBLENAME
    assert_output --partial "there is 2 workspaces with that name"
    assert_failure
    run ws_release --config bats/ws.conf -F ws1 DOUBLENAME
    run ws_release --config bats/ws.conf -F ws2 DOUBLENAME
}

@test "ws_release releases directory" {
    wsdir=$(ws_allocate --config bats/ws.conf -F ws1 $ws_name)
    assert_dir_exists $wsdir
    ws_release --config bats/ws.conf  -F ws1 $ws_name
    assert_dir_not_exists $wsdir
}

@test "ws_release delete directory with data" {
    wsdir=$(ws_allocate --config bats/ws.conf -F ws1  $ws_name2)
    assert_dir_exists $wsdir
    mkdir $wsdir/DATA
    touch $wsdir/DATA/file
    ws_release --config bats/ws.conf --delete-data -F ws1 $ws_name2
    assert_dir_not_exists $wsdir
}

cleanup() {
    ws_release $ws_name
}
