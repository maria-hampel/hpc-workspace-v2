setup() {
    load 'test_helper/common-setup'
    _common_setup
    register_dir="/tmp/ws-register-test-$RANDOM"
}

teardown() {
    # Clean up test directory
    if [ -d "$register_dir" ]; then
        rm -rf "$register_dir"
    fi
}

@test "ws_register present" {
    which ws_register
}

# bats test_tags=broken:v1-5-0
@test "ws_register print version" {
    run ws_register --version
    assert_output --partial "ws_register"
    assert_success
}

@test "ws_register print help" {
    run ws_register --help
    assert_output --partial "Usage"
    assert_success
}

@test "ws_register create links" {
    ws_allocate --config bats/ws.conf REGISTERTEST
    run ws_register --config bats/ws.conf /tmp/WS-REGISTERTEST
    assert_output --partial "creating link"
    assert_success
    assert_link_exists /tmp/WS-REGISTERTEST/*/*-REGISTERTEST
}

@test "ws_register remove link" {
    ws_release --config bats/ws.conf REGISTERTEST
    run ws_register --config bats/ws.conf /tmp/WS-REGISTERTEST
    assert_output --partial "removing link"
    assert_success
    assert_link_not_exists /tmp/WS-REGISTERTEST/*/*-REGISTERTEST
}

@test "ws_register creates directory if not exists" {
    new_dir="/tmp/ws-register-new-$RANDOM"
    ws_allocate --config bats/ws.conf NEWDIRTEST
    run ws_register --config bats/ws.conf $new_dir
    assert_success
    assert_dir_exist $new_dir
    rm -rf $new_dir
    ws_release --config bats/ws.conf NEWDIRTEST
}

@test "ws_register creates filesystem subdirectories" {
    ws_allocate --config bats/ws.conf -F ws1 SUBDIRTEST
    run ws_register --config bats/ws.conf $register_dir
    assert_success
    assert_dir_exist $register_dir/ws1
    ws_release --config bats/ws.conf -F ws1 SUBDIRTEST
}

@test "ws_register multiple workspaces create multiple links" {
    ws_allocate --config bats/ws.conf -F ws1 MULTI1
    ws_allocate --config bats/ws.conf -F ws1 MULTI2
    ws_allocate --config bats/ws.conf -F ws1 MULTI3
    run ws_register --config bats/ws.conf $register_dir
    assert_output --partial "creating link"
    assert_success
    assert_link_exists $register_dir/ws1/*-MULTI1
    assert_link_exists $register_dir/ws1/*-MULTI2
    assert_link_exists $register_dir/ws1/*-MULTI3
    ws_release --config bats/ws.conf -F ws1 MULTI1
    ws_release --config bats/ws.conf -F ws1 MULTI2
    ws_release --config bats/ws.conf -F ws1 MULTI3
}

@test "ws_register keeps existing valid links" {
    ws_allocate --config bats/ws.conf -F ws1 KEEPTEST
    # First registration creates link
    ws_register --config bats/ws.conf $register_dir
    # Second registration should keep existing link
    run ws_register --config bats/ws.conf $register_dir
    assert_output --partial "keeping link"
    refute_output --partial "creating link"
    assert_success
    ws_release --config bats/ws.conf -F ws1 KEEPTEST
}

@test "ws_register handles multiple filesystems" {
    ws_allocate --config bats/ws.conf -F ws1 MULTIFS1
    ws_allocate --config bats/ws.conf -F ws2 MULTIFS2
    run ws_register --config bats/ws.conf $register_dir
    assert_success
    assert_dir_exist $register_dir/ws1
    assert_dir_exist $register_dir/ws2
    assert_link_exists $register_dir/ws1/*-MULTIFS1
    assert_link_exists $register_dir/ws2/*-MULTIFS2
    ws_release --config bats/ws.conf -F ws1 MULTIFS1
    ws_release --config bats/ws.conf -F ws2 MULTIFS2
}

@test "ws_register missing directory argument" {
    run ws_register --config bats/ws.conf
    assert_output --partial "Usage"
    assert_success
}

@test "ws_register with config file option" {
    ws_allocate --config bats/ws.conf -F ws1 CONFIGTEST
    run ws_register -c bats/ws.conf $register_dir
    assert_success
    assert_link_exists $register_dir/ws1/*-CONFIGTEST
    ws_release -c bats/ws.conf -F ws1 CONFIGTEST
}

@test "ws_register symlink points to correct workspace" {
    wsdir=$(ws_allocate --config bats/ws.conf -F ws1 TARGETTEST)
    ws_register --config bats/ws.conf $register_dir
    link=$(find $register_dir/ws1 -name "*-TARGETTEST" -type l)
    target=$(readlink $link)
    assert_equal "$target" "$wsdir"
    ws_release --config bats/ws.conf -F ws1 TARGETTEST
}

@test "ws_register mixed operations create keep remove" {
    # Create initial workspace and register
    ws_allocate --config bats/ws.conf -F ws1 KEEP1
    ws_allocate --config bats/ws.conf -F ws1 REMOVE1
    ws_register --config bats/ws.conf $register_dir

    # Release one workspace and add a new one
    ws_release --config bats/ws.conf -F ws1 REMOVE1
    ws_allocate --config bats/ws.conf -F ws1 CREATE1

    # Register again - should keep, remove, and create
    run ws_register --config bats/ws.conf $register_dir
    assert_output --partial "keeping link"
    assert_output --partial "removing link"
    assert_output --partial "creating link"
    assert_success

    assert_link_exists $register_dir/ws1/*-KEEP1
    assert_link_exists $register_dir/ws1/*-CREATE1
    assert_link_not_exists $register_dir/ws1/*-REMOVE1

    ws_release --config bats/ws.conf -F ws1 KEEP1
    ws_release --config bats/ws.conf -F ws1 CREATE1
}

@test "ws_register removes only invalid links" {
    ws_allocate --config bats/ws.conf -F ws1 VALID1
    ws_allocate --config bats/ws.conf -F ws1 VALID2
    ws_allocate --config bats/ws.conf -F ws1 INVALID1
    ws_register --config bats/ws.conf $register_dir

    # Release one workspace
    ws_release --config bats/ws.conf -F ws1 INVALID1

    # Register again - should keep valid, remove invalid
    run ws_register --config bats/ws.conf $register_dir
    assert_success
    assert_link_exists $register_dir/ws1/*-VALID1
    assert_link_exists $register_dir/ws1/*-VALID2
    assert_link_not_exists $register_dir/ws1/*-INVALID1

    ws_release --config bats/ws.conf -F ws1 VALID1
    ws_release --config bats/ws.conf -F ws1 VALID2
}

@test "ws_register short form directory option" {
    ws_allocate --config bats/ws.conf -F ws1 SHORTDIR
    run ws_register --config bats/ws.conf -d $register_dir
    assert_success
    assert_link_exists $register_dir/ws1/*-SHORTDIR
    ws_release --config bats/ws.conf -F ws1 SHORTDIR
}

@test "ws_register creates links for all user workspaces" {
    # Create multiple workspaces without pattern
    ws_allocate --config bats/ws.conf -F ws1 ALLWS1
    ws_allocate --config bats/ws.conf -F ws1 ALLWS2
    ws_allocate --config bats/ws.conf -F ws1 ALLWS3

    run ws_register --config bats/ws.conf $register_dir
    assert_success

    # Should create links for all workspaces
    assert_link_exists $register_dir/ws1/*-ALLWS1
    assert_link_exists $register_dir/ws1/*-ALLWS2
    assert_link_exists $register_dir/ws1/*-ALLWS3

    ws_release --config bats/ws.conf -F ws1 ALLWS1
    ws_release --config bats/ws.conf -F ws1 ALLWS2
    ws_release --config bats/ws.conf -F ws1 ALLWS3
}

@test "ws_register help shows directory argument" {
    run ws_register --help
    assert_output --partial "DIRECTORY"
    assert_success
}

@test "ws_register version shows correct tool name" {
    run ws_register --version
    assert_output --regexp "(ws_register|ws_regiter)"
    assert_success
}

@test "ws_register handles workspace with dashes in name" {
    ws_allocate --config bats/ws.conf -F ws1 test-with-dashes
    run ws_register --config bats/ws.conf $register_dir
    assert_success
    assert_link_exists $register_dir/ws1/*-test-with-dashes
    ws_release --config bats/ws.conf -F ws1 test-with-dashes
}

@test "ws_register handles workspace with underscores in name" {
    ws_allocate --config bats/ws.conf -F ws1 test_with_underscores
    run ws_register --config bats/ws.conf $register_dir
    assert_success
    assert_link_exists $register_dir/ws1/*-test_with_underscores
    ws_release --config bats/ws.conf -F ws1 test_with_underscores
}

@test "ws_register handles workspace with dots in name" {
    ws_allocate --config bats/ws.conf -F ws1 test.with.dots
    run ws_register --config bats/ws.conf $register_dir
    assert_success
    assert_link_exists $register_dir/ws1/*-test.with.dots
    ws_release --config bats/ws.conf -F ws1 test.with.dots
}

@test "ws_register preserves existing directory structure" {
    # Create directory with existing content
    mkdir -p $register_dir/existing_subdir
    touch $register_dir/existing_file

    ws_allocate --config bats/ws.conf -F ws1 PRESERVE
    run ws_register --config bats/ws.conf $register_dir
    assert_success

    # Existing content should remain
    assert_dir_exist $register_dir/existing_subdir
    assert_file_exists $register_dir/existing_file
    # New link should be created
    assert_link_exists $register_dir/ws1/*-PRESERVE

    ws_release --config bats/ws.conf -F ws1 PRESERVE
}

@test "ws_register idempotent - multiple runs same result" {
    ws_allocate --config bats/ws.conf -F ws1 IDEMPOTENT

    # First run
    ws_register --config bats/ws.conf $register_dir
    link1=$(find $register_dir/ws1 -name "*-IDEMPOTENT" -type l)

    # Second run
    ws_register --config bats/ws.conf $register_dir
    link2=$(find $register_dir/ws1 -name "*-IDEMPOTENT" -type l)

    # Should be the same link
    assert_equal "$link1" "$link2"

    ws_release --config bats/ws.conf -F ws1 IDEMPOTENT
}

@test "ws_register different filesystems separate directories" {
    ws_allocate --config bats/ws.conf -F ws1 FS1TEST
    ws_allocate --config bats/ws.conf -F ws2 FS2TEST

    ws_register --config bats/ws.conf $register_dir

    # Links should be in separate subdirectories
    assert_link_exists $register_dir/ws1/*-FS1TEST
    assert_link_exists $register_dir/ws2/*-FS2TEST

    # Each link should be in its own filesystem directory
    ws1_link=$(find $register_dir/ws1 -name "*-FS1TEST" -type l)
    ws2_link=$(find $register_dir/ws2 -name "*-FS2TEST" -type l)

    assert_regex "$ws1_link" ".*/ws1/.*"
    assert_regex "$ws2_link" ".*/ws2/.*"

    ws_release --config bats/ws.conf -F ws1 FS1TEST
    ws_release --config bats/ws.conf -F ws2 FS2TEST
}

@test "ws_register reports correct actions" {
    ws_allocate --config bats/ws.conf -F ws1 ACTION1

    # First registration - should create
    run ws_register --config bats/ws.conf $register_dir
    assert_output --partial "creating link"
    refute_output --partial "keeping link"

    # Second registration - should keep
    run ws_register --config bats/ws.conf $register_dir
    assert_output --partial "keeping link"
    refute_output --partial "creating link"

    ws_release --config bats/ws.conf -F ws1 ACTION1

    # Third registration - should remove
    run ws_register --config bats/ws.conf $register_dir
    assert_output --partial "removing link"

    assert_success
}
