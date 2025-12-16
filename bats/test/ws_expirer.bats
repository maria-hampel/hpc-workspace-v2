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
