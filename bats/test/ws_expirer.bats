setup() {
    load 'test_helper/common-setup'
    _common_setup
}

@test "ws_expirer present" {
    which ws_expirer
}

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

@test "ws_expirer dryrun" {
    run ws_expirer --config bats/ws.conf
    assert_output --partial "simulating cleaning - dryrun"
    assert_success
}

@test "ws_expirer not dryrun" {
    run ws_expirer --config bats/ws.conf -c
    assert_output --partial "really cleaning!"
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
    ws_editdb --config bats/ws.conf --not-kidding --add-time -5 EXPIRE_TEST
    run ws_expirer --config bats/ws.conf -c
    assert_output --regexp 'expiring .*-EXPIRE_TEST'
    assert_success
}

# This test does not work anymore since expied is now used
# @test "ws_expirer delete expired" {
#     ws_editdb --config bats/ws.conf --not-kidding --expired --add-time -5 EXPIRE_TES*
#     run ws_expirer --config bats/ws.conf -c
#     assert_output --regexp 'deleting DB.*-EXPIRE_TEST'
#     assert_output --regexp 'deleting directory.*-EXPIRE_TEST'
#     assert_success
# }

@test "ws_expirer released" {
    ws_allocate --config bats/ws.conf EXPIRE_TEST 1
    ws_release --config bats/ws.conf EXPIRE_TEST
    run ws_expirer --config bats/ws.conf --forcedeletereleased
    assert_output --regexp 'deleting DB.*-EXPIRE_TEST'
    assert_output --regexp 'deleting directory.*-EXPIRE_TEST'
    assert_success
    run ws_expirer --config bats/ws.conf --forcedeletereleased -c
    assert_output --regexp 'deleting DB.*-EXPIRE_TEST'
    assert_output --regexp 'deleting directory.*-EXPIRE_TEST'
    assert_success
    run ws_list -e --config bats/ws.conf EXPIRE_TEST*
    refute_output --partial EXPIRE_TEST
}

@test "ws_expirer broken DB entry" {
    cp /dev/null /tmp/ws/ws1-db/${USER}-broken
    run ws_expirer --config bats/ws.conf
    assert_output --partial "could not read"
    rm -f /tmp/ws/ws1-db/${USER}-broken
}

@test "ws_expirer with space" {
    run ws_expirer --config bats/ws.conf -s /tmp/ws/ws2/1
    assert_output --partial "given space not in filesystem ws1"
    assert_output --partial "only cleaning in space /tmp/ws/ws2/1"
}

@test "ws_expirer with filesystem" {
    run ws_expirer --config bats/ws.conf -F ws2
    refute_output --partial "ws1"
}

@test "ws_expirer with filesystems" {
    run ws_expirer --config bats/ws.conf -F ws1,ws2
    assert_output --partial "ws1"
    assert_output --partial "ws2"
}

@test "ws_expirer missing magic" {
    mv /tmp/ws/ws1-db/.ws_db_magic /tmp/ws/ws1-db/.ws_db_magiC
    run ws_expirer --config bats/ws.conf
    assert_output --partial "does not contain .ws_db_magic"
    assert_success
    mv /tmp/ws/ws1-db/.ws_db_magiC /tmp/ws/ws1-db/.ws_db_magic
}

@test "ws_expirer does not delete directly" {
    ws_allocate --config bats/ws.conf -F ws1 TestWS 1
    ws_editdb --config bats/ws.conf --add-time -20 -p "*TestWS*" --not-kidding
    run ws_expirer --config bats/ws.conf -c
    assert_output --regexp "keeping restorable.*-TestWS"
}

@test "ws_expirer clean stray directories" {
    mkdir -p /tmp/ws/ws1/stray-dir
    run ws_expirer --config bats/ws.conf -c
    assert_output --regexp "move .*stray-dir"
    assert_success
    #rm -rf /tmp/ws/ws1/stray-dir
}

@test "ws_expirer send reminder mail" {
    ws_allocate --config bats/ws.conf -m $USER@localhost -r 2 REMINDER_TEST 2
    ws_editdb --config bats/ws.conf --not-kidding --add-time -1 REMINDER_TEST
    run ws_expirer --config bats/ws.conf -c
    assert_output --regexp "sending reminder mail"
    assert_success
    ws_release --config bats/ws.conf REMINDER_TEST
}

@test "ws_expirer handle bad database entries" {
    echo "invalid_entry" > /tmp/ws/ws1-db/${USER}-BAD_ENTRY
    run ws_expirer --config bats/ws.conf
    assert_output --partial "Empty file?"
    assert_failure
    rm -f /tmp/ws/ws1-db/${USER}-BAD_ENTRY
}

@test "ws_expirer process multiple workspaces" {
    ws_allocate --config bats/ws.conf MULTI_TEST1 1
    ws_allocate --config bats/ws.conf MULTI_TEST2 1
    ws_editdb --config bats/ws.conf --not-kidding --add-time -5 MULTI_TEST1
    ws_editdb --config bats/ws.conf --not-kidding --add-time -5 MULTI_TEST2
    run ws_expirer --config bats/ws.conf -c
    assert_output --regexp "expiring .*-MULTI_TEST1"
    assert_output --regexp "expiring .*-MULTI_TEST2"
    assert_success
}

# ========== Additional comprehensive tests ==========

@test "ws_expirer shows run start and end timestamps" {
    run ws_expirer --config bats/ws.conf
    assert_output --regexp "WS_EXPIRER RUN START"
    assert_output --regexp "WS_EXPIRER RUN END"
    assert_success
}

@test "ws_expirer shows expiration summary" {
    run ws_expirer --config bats/ws.conf
    assert_output --regexp "Expiration summary:.*kept.*expired.*deleted.*reminders sent.*bad db entries"
    assert_success
}

@test "ws_expirer shows stray removal summary" {
    run ws_expirer --config bats/ws.conf
    assert_output --regexp "Stray removal summary:.*valid.*invalid"
    assert_success
}

@test "ws_expirer dryrun does not move expired workspace" {
    ws_allocate --config bats/ws.conf DRYRUN_EXP_TEST 1
    ws_editdb --config bats/ws.conf --not-kidding --add-time -5 DRYRUN_EXP_TEST
    local ws_path=$(ws_find --config bats/ws.conf DRYRUN_EXP_TEST)
    run ws_expirer --config bats/ws.conf
    # Workspace should still exist in original location in dryrun mode
    [ -d "$ws_path" ]
    assert_success
    ws_release --config bats/ws.conf DRYRUN_EXP_TEST
}

@test "ws_expirer moves expired workspace to deleted directory" {
    ws_allocate --config bats/ws.conf MOVED_TEST 1
    ws_editdb --config bats/ws.conf --not-kidding --add-time -2 MOVED_TEST
    local ws_path=$(ws_find --config bats/ws.conf MOVED_TEST )
    run ws_expirer --config bats/ws.conf -c
    assert_output --regexp 'expiring .*-MOVED_TEST'
    # Original workspace should not exist
    assert_dir_not_exists $ws_path
    # Should exist in deleted directory
    [ -d /tmp/ws/ws2/*/.removed/*MOVED_TEST* ]
    assert_success
}

@test "ws_expirer appends timestamp to moved workspace" {
    ws_allocate --config bats/ws.conf TIMESTAMP_TEST 1
    ws_editdb --config bats/ws.conf --not-kidding --add-time -5 TIMESTAMP_TEST
    run ws_expirer --config bats/ws.conf -c
    assert_output --regexp 'expiring .*-TIMESTAMP_TEST'
    # Check that a file with timestamp suffix exists in deleted directory
    run ls /tmp/ws/ws2/*/.removed/*TIMESTAMP_TEST*
    assert_success
}

@test "ws_expirer handles invalid filesystem gracefully" {
    run ws_expirer --config bats/ws.conf -F nonexistent_fs
    # Should not process non-existent filesystem
    refute_output --partial "Checking DB for workspaces to be expired for filesystem nonexistent_fs"
    assert_success
}

@test "ws_expirer filters multiple filesystems correctly" {
    run ws_expirer --config bats/ws.conf -F ws1
    assert_output --partial "ws1"
    refute_output --regexp "Checking DB.*ws2"
    assert_success
}

@test "ws_expirer keeps workspace within keeptime after expiration" {
    ws_allocate --config bats/ws.conf KEEPTIME_TEST 1
    # Expire it but within keeptime (7 days)
    ws_editdb --config bats/ws.conf --not-kidding --add-time -5 KEEPTIME_TEST
    run ws_expirer --config bats/ws.conf -c
    assert_output --regexp 'expiring .*-KEEPTIME_TEST'
    assert_success
    # Run again - should keep the restorable workspace
    run ws_expirer --config bats/ws.conf -c
    assert_output --regexp 'keeping restorable.*-KEEPTIME_TEST'
    assert_success
}

@test "ws_expirer deletes workspace beyond keeptime" {
    ws_allocate --config bats/ws.conf BEYOND_KEEPTIME 1
    # Expire and move beyond keeptime (7 days in config)
    ws_editdb --config bats/ws.conf --not-kidding --add-time -10 BEYOND_KEEPTIME
    run ws_expirer --config bats/ws.conf -c
    assert_output --regexp 'expiring .*-BEYOND_KEEPTIME'
    assert_success
    # Manually edit deleted DB entry to be beyond keeptime
    ws_editdb --config bats/ws.conf --not-kidding --expired --add-time-expired -10 "*BEYOND_KEEPTIME*"
    run ws_expirer --config bats/ws.conf -c
    assert_output --regexp 'deleting DB.*BEYOND_KEEPTIME'
    assert_success
}

@test "ws_expirer processes stray directories without dash" {
    # Create a stray directory without dash (won't match *-* pattern)
    mkdir -p /tmp/ws/ws1/nodash
    run ws_expirer --config bats/ws.conf -c
    # Should not process directories without dash in name
    refute_output --partial "nodash"
    [ -d "/tmp/ws/ws1/nodash" ]
    rm -rf /tmp/ws/ws1/nodash
}

@test "ws_expirer finds stray directory with dash" {
    mkdir -p /tmp/ws/ws1/user-stray
    run ws_expirer --config bats/ws.conf -c
    assert_output --regexp "stray workspace.*user-stray"
    assert_success
}

@test "ws_expirer counts valid and invalid workspaces" {
    ws_allocate --config bats/ws.conf VALID_COUNT1 1
    ws_allocate --config bats/ws.conf VALID_COUNT2 1
    mkdir -p /tmp/ws/ws1/stray-invalid
    run ws_expirer --config bats/ws.conf
    assert_output --regexp "valid.*invalid directories found"
    assert_success
    ws_release --config bats/ws.conf VALID_COUNT1
    ws_release --config bats/ws.conf VALID_COUNT2
}

@test "ws_expirer dryrun shows what would be moved" {
    mkdir -p /tmp/ws/ws1/test-dryrun-move
    run ws_expirer --config bats/ws.conf
    assert_output --regexp "would move.*test-dryrun-move"
    # Directory should still exist after dryrun
    [ -d "/tmp/ws/ws1/test-dryrun-move" ]
    rm -rf /tmp/ws/ws1/test-dryrun-move
}

@test "ws_expirer processes both active and deleted workspaces" {
    ws_allocate --config bats/ws.conf BOTH_TEST 1
    ws_editdb --config bats/ws.conf --not-kidding --add-time -5 BOTH_TEST
    run ws_expirer --config bats/ws.conf -c
    assert_output --partial "Checking DB for workspaces to be expired"
    assert_output --partial "Checking deleted DB for workspaces to be deleted"
    assert_success
}

@test "ws_expirer sends reminder before expiration" {
    ws_allocate --config bats/ws.conf -m $USER@localhost -r 3 REMIND_EARLY 3
    ws_editdb --config bats/ws.conf --not-kidding --add-time -2 REMIND_EARLY
    run ws_expirer --config bats/ws.conf -c
    # Should send reminder as we're within 3-day reminder window
    assert_output --regexp "sending reminder mail.*REMIND_EARLY"
    assert_success
    ws_release --config bats/ws.conf REMIND_EARLY
}

@test "ws_expirer no reminder when not in reminder window" {
    ws_allocate --config bats/ws.conf -m $USER@localhost -r 1 NO_REMIND_TEST 10
    run ws_expirer --config bats/ws.conf -c
    # Should not send reminder as expiration is far away
    refute_output --regexp "sending reminder mail.*NO_REMIND_TEST"
    assert_success
    ws_release --config bats/ws.conf NO_REMIND_TEST
}

@test "ws_expirer handles bad expiration value" {
    # Create a workspace and corrupt its expiration
    ws_allocate --config bats/ws.conf BAD_EXP_TEST 1
    local db_file=$(find /tmp/ws/ws2-db -name "*BAD_EXP_TEST" | head -1)
    # Inject bad expiration value (0 or negative)
    sed -i 's/expiration: [0-9]*/expiration: 0/' "$db_file"
    run ws_expirer --config bats/ws.conf
    assert_output --partial "bad expiration"
    assert_success
    ws_release --config bats/ws.conf BAD_EXP_TEST
}

@test "ws_expirer cleans stray deleted directories" {
    # Create a directory in deleted that has no DB entry
    mkdir -p "/tmp/ws/ws1/.removed/${USER}-orphan-deleted-123456"
    run ws_expirer --config bats/ws.conf -c
    assert_output --regexp "stray removed workspace.*orphan-deleted"
    assert_success
}

@test "ws_expirer logs deleted workspace information" {
    ws_allocate --config bats/ws.conf LOG_DELETE_TEST 1
    ws_editdb --config bats/ws.conf --not-kidding --add-time -5 LOG_DELETE_TEST
    run ws_expirer --config bats/ws.conf -c
    # First run expires it
    assert_output --regexp 'expiring .*LOG_DELETE_TEST'
    # Edit to be beyond keeptime
    ws_editdb --config bats/ws.conf --not-kidding --expired --add-time -10 "*LOG_DELETE_TEST*"
    run ws_expirer --config bats/ws.conf -c --forcedeletereleased
    # Should show deletion info
    assert_output --regexp 'deleting DB.*LOG_DELETE_TEST'
    assert_output --regexp 'deleting directory'
    assert_success
}

@test "ws_expirer handles missing config file gracefully" {
    run ws_expirer --config /nonexistent/config.conf
    assert_output --partial "No valid config file found"
    assert_failure
}

@test "ws_expirer respects space filter for stray cleanup" {
    mkdir -p /tmp/ws/ws2/1/space-filtered-stray
    mkdir -p /tmp/ws/ws2/2/space-unfiltered-stray
    run ws_expirer --config bats/ws.conf -s /tmp/ws/ws2/1 -c
    assert_output --regexp "stray.*space-filtered-stray"
    refute_output --partial "space-unfiltered-stray"
    assert_success
    # Cleanup
    rm -rf /tmp/ws/ws2/1/space-filtered-stray
    rm -rf /tmp/ws/ws2/2/space-unfiltered-stray
}

@test "ws_expirer reports correct kept and expired counts" {
    ws_allocate --config bats/ws.conf COUNT_KEEP1 10
    ws_allocate --config bats/ws.conf COUNT_KEEP2 10
    ws_allocate --config bats/ws.conf COUNT_EXP1 1
    ws_allocate --config bats/ws.conf COUNT_EXP2 1
    ws_editdb --config bats/ws.conf --not-kidding --add-time -5 COUNT_EXP1
    ws_editdb --config bats/ws.conf --not-kidding --add-time -5 COUNT_EXP2
    run ws_expirer --config bats/ws.conf -c
    assert_output --regexp "2 workspaces expired.*kept"
    assert_success
    ws_release --config bats/ws.conf COUNT_KEEP1
    ws_release --config bats/ws.conf COUNT_KEEP2
}
