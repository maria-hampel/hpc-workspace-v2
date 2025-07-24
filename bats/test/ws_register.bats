setup() {
    load 'test_helper/common-setup'
    _common_setup
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
