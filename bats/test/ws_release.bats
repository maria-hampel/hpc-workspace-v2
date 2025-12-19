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

@test "ws_release invalid workspace name with special chars" {
    run ws_release --config bats/ws.conf "invalid/name"
    assert_output --partial "Illegal workspace name"
    assert_failure
}

@test "ws_release invalid workspace name with spaces" {
    run ws_release --config bats/ws.conf "invalid name"
    assert_output --partial "Illegal workspace name"
    assert_failure
}

@test "ws_release invalid workspace name with asterisk" {
    run ws_release --config bats/ws.conf "invalid*name"
    assert_output --partial "Illegal workspace name"
    assert_failure
}

@test "ws_release moved to deleted directory" {
    wsdir=$(ws_allocate --config bats/ws.conf -F ws1 released_ws)
    touch $wsdir/testfile
    ws_release --config bats/ws.conf -F ws1 released_ws
    # Check original location is gone
    assert_dir_not_exist $wsdir
    # Check workspace moved to deleted directory (contains .removed or similar)
    deleted_parent=$(dirname $wsdir)/.removed
    [ -d "$deleted_parent" ]
}

@test "ws_release data persists without delete-data flag" {
    wsdir=$(ws_allocate --config bats/ws.conf -F ws1 persist_ws)
    testfile=$wsdir/persistent_file
    echo "test content" > $testfile
    ws_release --config bats/ws.conf -F ws1 persist_ws
    # Original workspace should be gone
    assert_dir_not_exist $wsdir
    # But data should exist in deleted directory
    deleted_parent=$(dirname $wsdir)/.removed
    # Find the moved workspace (has timestamp suffix)
    moved_ws=$(find $deleted_parent -maxdepth 1 -name "*persist_ws-*" -type d 2>/dev/null | head -1)
    [ -n "$moved_ws" ]
    [ -f "$moved_ws/persistent_file" ]
    [ "$(cat $moved_ws/persistent_file)" = "test content" ]
}

@test "ws_release cannot release twice" {
    ws_allocate --config bats/ws.conf -F ws1 double_release
    ws_release --config bats/ws.conf -F ws1 double_release
    run ws_release --config bats/ws.conf -F ws1 double_release
    assert_output --partial "Non-existent workspace given"
    assert_failure
}

@test "ws_release with explicit filesystem" {
    ws_allocate --config bats/ws.conf -F ws1 explicit_fs
    run ws_release --config bats/ws.conf -F ws1 explicit_fs
    assert_output --partial "workspace explicit_fs released"
    assert_success
}

@test "ws_release workspace not in specified filesystem" {
    ws_allocate --config bats/ws.conf -F ws1 on_ws1
    run ws_release --config bats/ws.conf -F ws2 on_ws1
    assert_output --partial "Non-existent workspace given"
    assert_failure
    # Clean up
    ws_release --config bats/ws.conf -F ws1 on_ws1
}

@test "ws_release valid name with dash" {
    ws_allocate --config bats/ws.conf -F ws1 valid-name-test
    run ws_release --config bats/ws.conf -F ws1 valid-name-test
    assert_success
}

@test "ws_release valid name with underscore" {
    ws_allocate --config bats/ws.conf -F ws1 valid_name_test
    run ws_release --config bats/ws.conf -F ws1 valid_name_test
    assert_success
}

@test "ws_release valid name with dot" {
    ws_allocate --config bats/ws.conf -F ws1 valid.name.test
    run ws_release --config bats/ws.conf -F ws1 valid.name.test
    assert_success
}

@test "ws_release workspace with nested data" {
    wsdir=$(ws_allocate --config bats/ws.conf -F ws1 nested_data)
    mkdir -p $wsdir/level1/level2/level3
    echo "deep content" > $wsdir/level1/level2/level3/deepfile
    ws_release --config bats/ws.conf -F ws1 nested_data
    # Check moved to deleted
    deleted_parent=$(dirname $wsdir)/.removed
    moved_ws=$(find $deleted_parent -maxdepth 1 -name "*nested_data-*" -type d 2>/dev/null | head -1)
    [ -f "$moved_ws/level1/level2/level3/deepfile" ]
}

@test "ws_release missing workspace name argument" {
    run ws_release --config bats/ws.conf
    assert_output --partial "Usage"
    assert_failure
}

@test "ws_release with username option as non-root" {
    ws_allocate --config bats/ws.conf -F ws1 useropt_test
    run ws_release --config bats/ws.conf -F ws1 -u otheruser useropt_test
    assert_output --partial "Ignoring username option"
    assert_success
}

@test "ws_release delete-data removes all traces" {
    wsdir=$(ws_allocate --config bats/ws.conf -F ws1 delete_all)
    mkdir -p $wsdir/data
    echo "content" > $wsdir/data/file
    # Note: --delete-data waits 5 seconds by default, may need timeout handling
    run timeout 7 bash -c "echo | ws_release --config bats/ws.conf -F ws1 --delete-data delete_all"
    # Verify workspace is completely gone
    assert_dir_not_exist $wsdir
    deleted_parent=$(dirname $wsdir)/.removed
    # Should not exist in deleted directory either
    moved_ws=$(find $deleted_parent -maxdepth 1 -name "*delete_all-*" -type d 2>/dev/null | head -1)
    [ -z "$moved_ws" ]
}

@test "ws_release filesystem argument short form" {
    ws_allocate --config bats/ws.conf -F ws1 shortform
    run ws_release --config bats/ws.conf -F ws1 shortform
    assert_output --partial "workspace shortform released"
    assert_success
}

@test "ws_release config file short form" {
    ws_allocate --config bats/ws.conf -F ws1 configshort
    run ws_release -c bats/ws.conf -F ws1 configshort
    assert_output --partial "workspace configshort released"
    assert_success
}

@test "ws_release name argument short form" {
    ws_allocate --config bats/ws.conf -F ws1 nameshort
    run ws_release --config bats/ws.conf -F ws1 -n nameshort
    assert_output --partial "workspace nameshort released"
    assert_success
}

cleanup() {
    ws_release $ws_name
}
