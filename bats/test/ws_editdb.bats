setup() {
    load 'test_helper/common-setup'
    _common_setup
}

@test "ws_editdb present" {
    which ws_editdb
}

@test "ws_editdb print version" {
    run ws_editdb --version
    assert_output --partial "ws_editdb"
    assert_success
}

@test "ws_editdb print help" {
    run ws_editdb --help
    assert_output --partial "Usage"
    assert_success
}

@test "ws_editdb dryrun" {
    ws_allocate --config bats/ws.conf EDITTEST 1
    run ws_editdb --config bats/ws.conf --add-time 1 EDITTEST
    assert_output --partial "change expiration"
    assert_success
}

@test "ws_editdb not kidding" {
    run ws_editdb --config bats/ws.conf --not-kidding --add-time 1 EDITTEST
    assert_output --partial "change expiration"
    assert_success
    run ws_list --config bats/ws.conf  EDITTEST
    assert_output --regexp "(2 days)|1 days, 23"
    assert_success
    ws_release --config bats/ws.conf EDITTEST
}

@test "ws_editdb rename workspaces +20 days" {
    ws_allocate --config bats/ws.conf -F ws1 TestWS-p20 1
    ws_release --config bats/ws.conf -F ws1 TestWS-p20
    OLDTIMEDB=$(ls /tmp/ws/ws1-db/.removed | grep "TestWS-p20" | sed 's/.*-//' | sort -n | tail -1)
    OLDTIMEDIR=$(ls /tmp/ws/ws1/.removed | grep "TestWS-p20" | sed 's/.*-//' | sort -n | tail -1)
    run echo $OLDTIMEDB
    assert_output --partial "1"
    run echo $OLDTIMEDIR
    assert_output --partial "1"
    ws_editdb --config bats/ws.conf -e -r --add-time 20 --not-kidding
    NEWTIMEDB=$(ls /tmp/ws/ws1-db/.removed | grep "TestWS-p20" | sed 's/.*-//' | sort -n | tail -1)
    NEWTIMEDIR=$(ls /tmp/ws/ws1/.removed | grep "TestWS-p20" | sed 's/.*-//' | sort -n | tail -1)
    run echo $NEWTIMEDB
    assert_output --partial "1"
    run echo $NEWTIMEDIR
    assert_output --partial "1"
    DIFF=$((NEWTIMEDB-OLDTIMEDB))
    assert_equal "$DIFF" 1728000
    DIFF=$((NEWTIMEDIR-OLDTIMEDIR))
    assert_equal "$DIFF" 1728000
}

@test "ws_editdb rename workspaces -20 days" {
    ws_allocate --config bats/ws.conf -F ws1 TestWS-20 1
    ws_release --config bats/ws.conf -F ws1 TestWS-20
    OLDTIMEDB=$(ls /tmp/ws/ws1-db/.removed | grep "TestWS-20" | sed 's/.*-//' | sort -n | tail -1)
    OLDTIMEDIR=$(ls /tmp/ws/ws1/.removed | grep "TestWS-20" | sed 's/.*-//' | sort -n | tail -1)
    run echo $OLDTIMEDB
    assert_output --partial "1"
    run echo $OLDTIMEDIR
    assert_output --partial "1"
    ws_editdb --config bats/ws.conf -e -r --add-time -20 --not-kidding
    NEWTIMEDB=$(ls /tmp/ws/ws1-db/.removed | grep "TestWS-20" | sed 's/.*-//' | sort -n | tail -1)
    NEWTIMEDIR=$(ls /tmp/ws/ws1/.removed | grep "TestWS-20" | sed 's/.*-//' | sort -n | tail -1)
    run echo $NEWTIMEDB
    assert_output --partial "1"
    run echo $NEWTIMEDIR
    assert_output --partial "1"
    DIFF=$((NEWTIMEDB-OLDTIMEDB))
    assert_equal "$DIFF" -1728000
    DIFF=$((NEWTIMEDIR-OLDTIMEDIR))
    assert_equal "$DIFF" -1728000
}


@test "ws_editdb -r detect if ws is named the same" {
    ws_allocate --config bats/ws.conf -F ws1 samesame 1 
    ws_release --config bats/ws.conf -F ws1 samesame
    TIMESTAMPA=$(ls /tmp/ws/ws1-db/.removed | grep "samesame" | sed 's/.*-//' | sort -n | tail -1)
    WSNAME=$(ls /tmp/ws/ws1-db/.removed | grep "samesame" |sed 's/\(.*-.*-\)\([0-9]*\)/\1/' | sort -n | tail -1)
    TIMESTAMPB=$((TIMESTAMPA-86400))
    cp -r /tmp/ws/ws1/.removed/$WSNAME$TIMESTAMPA /tmp/ws/ws1/.removed/$WSNAME$TIMESTAMPB
    cat /tmp/ws/ws1-db/.removed/$WSNAME$TIMESTAMPA | sed 's/'"$TIMESTAMPA"'/'"$TIMESTAMPB"'/' > "/tmp/ws/ws1-db/.removed/$WSNAME$TIMESTAMPB"
    run ws_editdb --config bats/ws.conf -e -r --add-time 1 "$WSNAME$TIMESTAMPB"
    assert_output --partial "error:"
    rm -fr /tmp/ws/ws1/.removed/$WSNAME$TIMESTAMPB 
    rm /tmp/ws/ws1-db/.removed/$WSNAME$TIMESTAMPB
}

@test "ws_editdb -r get ws with regex" {
    run ws_editdb --config bats/ws.conf -e -r --add-time 20 "*samesame*"
    assert_success
    assert_output --partial "Id:"
}