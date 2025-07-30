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
    assert_output --partial "simulate cleaning - dryrun"
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
    ws_release --config bats/ws.conf EXPIRE_TEST
}

@test "ws_expirer delete expired" {
    ws_editdb --config bats/ws.conf --not-kidding --expired --add-time -5 EXPIRE_TES*
    run ws_expirer --config bats/ws.conf -c
    assert_output --regexp 'deleting DB.*-EXPIRE_TEST'
    assert_output --regexp 'deleting directory.*-EXPIRE_TEST'
    assert_success
}

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
