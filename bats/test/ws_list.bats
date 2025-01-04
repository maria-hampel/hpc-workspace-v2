setup() {
    load 'test_helper/common-setup'
    _common_setup
    ws_name="bats_workspace_test"
    export ws_name
}


@test "ws_list present" {
    which ws_list
}

# bats test_tags=broken:v1-5-0
@test "ws_list print version" {
    run ws_list --version
    assert_output --partial "workspace"
}

@test "ws_list print help" {
    run ws_list --help
    assert_output --partial "Usage"
}

@test "ws_list shows created workspace" {
    ws_allocate --config bats/ws.conf $ws_name
    ws_list --config bats/ws.conf -s | grep $ws_name
}

@test "ws_list shows created workspace with times" {
    # create a temporary workspace for 3 days
    # prepare expected output for diff
    wsdir=$(ws_allocate --config bats/ws.conf ${ws_name}_timestamped 3)
cat <<EOF >ref.txt
Id: ${USER}-${ws_name}_timestamped
    workspace directory  : $wsdir
    remaining time       : 2 days, 23 hours
    creation time        : $(date +%F)
    filesystem name      : FS
    available extensions : 3
EOF
    sleep 1
    # get ws_list output and parse and modify (check only day accuracy)
    ws_list --config bats/ws.conf ${ws_name}_timestamped | grep -v expiration > tmp.txt
    ctime=$(date +%F $(cat tmp.txt | grep "/creation time/{ print $3 }"))
    etime=$(date +%F $(cat tmp.txt | grep "/expiration date/{ print $3 }"))
    sed -i -e "s/\(.*creation time\s*:\) .*/\1 $ctime/" tmp.txt
    sed -i -e "s/\(.*expiration date\s*:\) .*/\1 $etime/" tmp.txt
    sed -i -e "s/\(.*filesystem name\s*:\) .*/\1 FS/" tmp.txt

    diff tmp.txt ref.txt
    #rm tmp.txt ref.txt
#    ws_release ${ws_name}_timestamped
}

@test "ws_list sorting by name" {
    ws_allocate --config bats/ws.conf sortTestB 3
    sleep 1
    ws_allocate --config bats/ws.conf sortTestA 1
    sleep 1
    ws_allocate --config bats/ws.conf sortTestC 2

    run ws_list --config bats/ws.conf -s -N "sortTest*"
    assert_output <<EOF1
${USER}-sortTestA
${USER}-sortTestB
${USER}-sortTestC
EOF1

    run ws_list --config bats/ws.conf -s -r -N "sortTest*"
    assert_output <<EOF2
${USER}-sortTestC
${USER}-sortTestB
${USER}-sortTestA
EOF2
}

@test "ws_list sorting by creation" {
    #ws_allocate --config bats/ws.conf sortTestB 3
    #ws_allocate --config bats/ws.conf sortTestA 1
    #ws_allocate --config bats/ws.conf sortTestC 2

    run ws_list --config bats/ws.conf -s -C "sortTest*"
    assert_output <<EOF3
${USER}-sortTestB
${USER}-sortTestA
${USER}-sortTestC
EOF3
}

@test "ws_list sorting by remaining time" {
    #ws_allocate --config bats/ws.conf sortTestB 3
    #ws_allocate --config bats/ws.conf sortTestA 1
    #ws_allocate --config bats/ws.conf sortTestC 2

    run ws_list --config bats/ws.conf -s -R "sortTest*"
    assert_output <<EOF4
${USER}-sortTestA
${USER}-sortTestC
${USER}-sortTestB
EOF4
}

@test "ws_list other fs" {
    ws_allocate --config bats/ws.conf -F ws1 WS1TEST
    run ws_list --config bats/ws.conf -s -F ws1 
    assert_output <<EOF5
${USER}-WS1TEST
EOF5
}

@test "ws_list error handling" {
    chmod 0000 /tmp/ws/ws1-db/${USER}-WS1TEST
    run ws_list --config bats/ws.conf -F ws1 
    assert_output  --partial "Error"
}

cleanup() {
    ws_release --config bats/ws.conf $ws_name
    assert_failure
}
