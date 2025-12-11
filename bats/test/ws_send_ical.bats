setup() {
    load 'test_helper/common-setup'
    _common_setup
}


@test "ws_send_ical present" {
    which ws_send_ical
}

@test "ws_send_ical print help" {
    run ws_send_ical --help
    assert_output --partial "Usage"
    assert_success
}

@test "ws_send_ical no valid config file" {
    run ws_send_ical --config bats/bad_ws.conf lala
    assert_output --partial "No adminmail in config!"
    assert_failure
}

@test "ws_send_ical invalid WORKSPACE" {
    run ws_send_ical --config bats/ws.conf -m tester@localhost ICALTEST$$
    assert_output --partial "no workspace"
    assert_failure
}

@test "ws_send_ical valid WORKSPACE" {
    run ws_allocate --config bats/ws.conf -m tester@localhost ICALTEST
    assert_success
    run ws_send_ical --config bats/ws.conf ICALTEST
    assert_output --partial "Calendar invitation sent"
    assert_success
}
