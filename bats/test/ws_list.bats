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


cleanup() {
    ws_release --config bats/ws.conf $ws_name
    assert_failure
}
