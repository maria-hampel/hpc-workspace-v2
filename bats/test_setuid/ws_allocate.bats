setup() {
    load 'test_helper/common-setup'
    _common_setup
    ws_name="bats_workspace_test"
    export ws_name
}

@test "ws_allocate present" {
    which ws_allocate
}

@test "ws_allocate print version" {
    run ws_allocate --version
    assert_output --partial "ws_allocate"
    assert_output --partial "issetuid=true"
}

@test "ws_allocate with reminder, no email" {
    [ -f ~/.ws_user.conf ] && mv -f ~/.ws_user.conf ~/.ws_user.conf_testbackup
    run ws_allocate --config bats/ws.conf -r 7 REMINDER 10
    assert_output --partial "reminder email will be sent to local user account"
    assert_output --partial "ignored config file option!"
    assert_success
    rm -f ~/.ws_user.conf
    [ -f ~/.ws_user.conf_testbackup ] && mv -f ~/.ws_user.conf_testbackup ~/.ws_user.conf
    ws_release REMINDER
}

@test "ws_allocate with reminder, invalid email" {
    run ws_allocate -r 1 -m a@b REMINDER
    assert_output --partial "Invalid email address"
    assert_success
    ws_release REMINDER
}

@test "ws_allocate with reminder, valid email" {
    run ws_allocate -r 1 -m a@b.c REMINDER 10
    assert_output --partial "remaining time in days: 10"
    assert_success
    run ws_list -v REMINDER
    assert_output --partial "a@b.c"
    ws_release REMINDER
}

@test "ws_allocate with user config for email and duration" {
    [ -f ~/.ws_user.conf ] && mv -f ~/.ws_user.conf ~/.ws_user.conf_testbackup
    echo "mail: mail@valid.domain" > ~/.ws_user.conf
    echo "duration: 14" >> ~/.ws_user.conf
    run ws_allocate -r 1 REMINDER
    assert_output --partial "Took email address"
    assert_output --partial "remaining time in days: 14"
    assert_success
    rm -f ~/.ws_user.conf
    [ -f ~/.ws_user.conf_testbackup ] && mv -f ~/.ws_user.conf_testbackup ~/.ws_user.conf
    ws_release REMINDER
}

@test "ws_allocate with group" {
    run ws_allocate -g WS2 10
    assert_success
    wsdir=$(ws_find WS2)
    run stat $wsdir
    assert_output --partial "drwxr-x---"
    ws_release WS2
}

@test "ws_allocate with invalid group" {
    run ws_allocate -G INVALID_GROUP WS2 10
    assert_output --partial "invalid group specified!"
    assert_failure
}

@test "ws_allocate -x with wrong group" {
    # create as userb a workspace
    export WS_ALLOCATE=$(which ws_allocate)
    run sudo -u userb --preserve-env=ASAN_OPTIONS $WS_ALLOCATE -G userb WS3 10
    assert_success

    run ws_allocate -u userb -x WS3 20
    assert_failure
    assert_output --partial "you are not owner"
}

@test "ws_allocate -x with correct group" {
    export WS_ALLOCATE=$(which ws_allocate)
    sudo -u userb --preserve-env=ASAN_OPTIONS $WS_ALLOCATE -G usera WS4 10
    run ws_allocate -u userb -x WS4 20
    assert_success
}

@test "ws_allocate -x with correct group but bad workspace" {
    run ws_allocate -u userb -x DOES_NOT_EXIST 20
    assert_failure
    assert_output --partial "can not be extended"
}

@test "ws_allocate with -x, invalid extension, too many extensions, changing comment" {
    run ws_allocate -x DOES_NOT_EXIST 10
    assert_failure
    assert_output --partial "workspace does not exist, can not be extended!"

    run ws_allocate extensiontest 10
    assert_success
    assert_output --partial "remaining time in days: 10"

    run ws_allocate -x extensiontest 20
    assert_success
    assert_output --partial "extending workspace"
    assert_output --partial "remaining extensions  : 2"
    assert_output --partial "remaining time in days: 20"

    run ws_allocate -c "add a comment" -x extensiontest 1
    assert_success
    assert_output --partial "changed comment"
    assert_output --partial "remaining extensions  : 2"
    # FIXME: is 2 correct here??

    run ws_allocate -x extensiontest 5
    assert_success
    assert_output --partial "remaining extensions  : 1"

    run ws_allocate -x extensiontest 10
    assert_success
    assert_output --partial "remaining extensions  : 0"

    run ws_allocate -x extensiontest 15
    assert_failure
    assert_output --partial "no more extensions!"

    ws_release extensiontest
}

cleanup() {
    ws_release $ws_name
}
