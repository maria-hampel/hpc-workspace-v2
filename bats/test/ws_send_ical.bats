setup() {
    load 'test_helper/common-setup'
    ws_name="bats_workspace_test"
    _common_setup
}

@test "ws_send_ical present" {
    which ws_send_ical
}

@test "ws_send_ical print version" {
    run ws_send_ical --version
    assert_output --partial "workspace"
    assert_success
}

@test "ws_send_ical print help" {
    run ws_send_ical --help
    assert_output --partial "Usage"
    assert_success
}

# @test "ws_send_ical valid ws.conf" {
#     ws_allocate --config bats/ws.conf -F ws1 my_ws 
#     run ws_send_ical --config bats/ws.conf my_ws -m test@test.de
#     assert_output --partial "Success: Calendar invitation sent to"
#     assert_success
# }

@test "ws_send_ical no mail_from" { 
    ws_allocate --config bats/no_mailfrom_ws.conf -F ws1 my_ws
    run ws_send_ical --config bats/no_mailfrom_ws.conf my_ws -m test@test.de
    assert_output --partial "warning: no mail_from in global config"
}

@test "ws_send_ical no usermail" {
    ws_allocate --config bats/no_mailfrom_ws.conf -F ws1 my_ws
    run ws_send_ical --config bats/ws.conf my_ws 
    assert_output "error: You can't use the ws_send_ical without a mailadress (-m)."
}

@test "ws_send_ical multiple workspaces no Filesystem" {
    ws_allocate --config bats/no_mailfrom_ws.conf -F ws1 my_ws
    ws_allocate --config bats/no_mailfrom_ws.conf -F ws2 my_ws
    run ws_send_ical --config bats/no_mailfrom_ws.conf my_ws -m test@test.de
    assert_output --partial "error: multiple workspaces found, use the -F option to specify Filesystem"
}

@test "ws_send_ical invalid email" {
    ws_allocate --config bats/no_mailfrom_ws.conf -F ws1 my_ws
    run ws_send_ical --config bats/no_mailfrom_ws.conf my_ws -m this-is-no-email.de
    assert_output "error: Invalid email address, abort"
}

cleanup() {
    rm -fr /tmp/ws
}
