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
    assert_output --partial "ws_list"
    assert_success
}

@test "ws_list print help" {
    run ws_list --help
    assert_output --partial "Usage"
    assert_success
}

@test "ws_list shows created workspace" {
    ws_allocate --config bats/ws.conf $ws_name
    ws_list --config bats/ws.conf -s | grep $ws_name
}

@test "ws_list shows created workspace with times" {
    # create a temporary workspace for 3 days
    # prepare expected output for diff
    wsdir=$(ws_allocate --config bats/ws.conf ${ws_name}_timestamped 3)
cat <<EOF >/tmp/$$ref.txt
Id: ${ws_name}_timestamped
    workspace directory  : $wsdir
    remaining time       : 2 days, 23 hours
    creation time        : $(date +%F)
    filesystem name      : FS
    available extensions : 3
EOF
    sleep 1
    TMP=/tmp/$$tmp.txt
    # get ws_list output and parse and modify (check only day accuracy)
    ws_list --config bats/ws.conf ${ws_name}_timestamped | grep -v expiration > $TMP
    ctime=$(date +%F $(cat tmp.txt | grep "/creation time/{ print $3 }"))
    etime=$(date +%F $(cat tmp.txt | grep "/expiration date/{ print $3 }"))
    sed -i -e "s/\(.*creation time\s*:\) .*/\1 $ctime/" $TMP
    sed -i -e "s/\(.*expiration date\s*:\) .*/\1 $etime/" $TMP
    sed -i -e "s/\(.*filesystem name\s*:\) .*/\1 FS/" $TMP

    diff $TMP /tmp/$$ref.txt
    #rm tmp.txt ref.txt
#    ws_release ${ws_name}_timestamped
    rm /tmp/ws/ws2-db/${USER}-${ws_name}_timestamped
}

@test "ws_list sorting by name" {
    ws_allocate --config bats/ws.conf sortTestB 3
    sleep 1
    ws_allocate --config bats/ws.conf sortTestA 1
    sleep 1
    ws_allocate --config bats/ws.conf sortTestC 2

    run ws_list --config bats/ws.conf -s -N "sortTest*"
    assert_output "${USER}-sortTestA
${USER}-sortTestB
${USER}-sortTestC"

    run ws_list --config bats/ws.conf -s -r -N "sortTest?"
    assert_output "${USER}-sortTestC
${USER}-sortTestB
${USER}-sortTestA"

}

@test "ws_list sorting by creation" {
    #ws_allocate --config bats/ws.conf sortTestB 3
    #ws_allocate --config bats/ws.conf sortTestA 1
    #ws_allocate --config bats/ws.conf sortTestC 2

    run ws_list --config bats/ws.conf -s -C "sortTest*"
    assert_output "${USER}-sortTestB
${USER}-sortTestA
${USER}-sortTestC"


    run ws_list --config bats/ws.conf -s -r -C "sortTest*"
    assert_output "${USER}-sortTestC
${USER}-sortTestA
${USER}-sortTestB"
}

@test "ws_list sorting by remaining time" {
    #ws_allocate --config bats/ws.conf sortTestB 3
    #ws_allocate --config bats/ws.conf sortTestA 1
    #ws_allocate --config bats/ws.conf sortTestC 2

    run ws_list --config bats/ws.conf -s -R "sortTest[A-C]"
    assert_output "${USER}-sortTestA
${USER}-sortTestC
${USER}-sortTestB"

    run ws_list --config bats/ws.conf -s -r -R "sortTest*"
    assert_output "${USER}-sortTestB
${USER}-sortTestC
${USER}-sortTestA"
}

@test "ws_list -T sorting by remaining time" {
    run ws_list --config bats/ws.conf -T -R "sortTest[A-C]"
    assert_output --regexp ".*sortTestA.*
sortTestC.*
sortTestB.*$"

    run ws_list --config bats/ws.conf -T -r -R "sortTest*"
    assert_output --regexp ".*sortTestB.*
sortTestC.*
sortTestA.*$"
}

@test "ws_list -T sorting by remaining time with config directory" {
    run ws_list --config bats/ws.d -T -R "sortTest[A-C]"
    assert_output --regexp ".*sortTestA.*
sortTestC.*
sortTestB.*$"

    run ws_list --config bats/ws.d -T -r -R "sortTest*"
    assert_output --regexp ".*sortTestB.*
sortTestC.*
sortTestA.*$"
}

@test "ws_list pattern" {
    run ws_list --config bats/ws.conf -s "sort*B"
    echo "${USER}-sortTestB" | assert_output
}

@test "ws_list other fs" {
    ws_allocate --config bats/ws.conf -F ws1 WS1TEST
    run ws_list --config bats/ws.conf -s -F ws1 WS1TEST
    assert_output ${USER}-WS1TEST
}

@test "ws_list list fs" {
    run ws_list --config bats/ws.conf -l
    assert_output "available filesystems (sorted according to priority):
ws2
ws1"
}

@test "ws_list list fs detailed" {
    run ws_list --config bats/ws.conf -L
    assert_output --partial "ws2          32           3         7        true        true        true   one hell of a comment"
    assert_output --partial "ws1          31           3         7        true        true        true"
}

# check if sorting of config files is correct and later definition overwrites previous definition
@test "ws_list list fs detailed with config directory" {
    run ws_list --config bats/ws.d -L
    assert_output --partial "ws2          32           3         7        true        true        true   one hell of a comment"
    assert_output --partial "ws1          31           3         7        true        true        true"
    refute_output --partial "this should not"
}

@test "ws_list error handling" {
    cp /dev/null /tmp/ws/ws1-db/${USER}-WS1TEST
    run ws_list --config bats/ws.conf -F ws1
    assert_output  --partial "error"
    rm -f  /tmp/ws/ws1-db/${USER}-WS1TEST
}

@test "ws_list invalid fs" {
    run ws_list --config bats/ws.conf -F ws3
    assert_output  --partial "error"
}

@test "ws_list invalid option" {
    run ws_list --config bats/ws.conf -X
    assert_output "Usage: ws_list [options] [pattern]

Options:
  -h [ --help ]                   produce help message
  -V [ --version ]                show version
  -F [ --filesystem ] arg         filesystem to list workspaces from
  -g [ --group ]                  enable listing of group workspaces
  -l [ --listfilesystems ]        list available filesystems
  -L [ --listfilesystemdetails ]  list available filesystems with details
  -s [ --short ]                  short listing, only workspace names
  -u [ --user ] arg               only show workspaces for selected user
  -e [ --expired ]                show expired workspaces
  -N [ --name ]                   sort by name
  -C [ --creation ]               sort by creation date
  -R [ --remaining ]              sort by remaining time
  -r [ --reverted ]               revert sort
  -t [ --terse ]                  terse listing
  -T [ --table ]                  table format
  --config arg                    config file
  -p [ --pattern ] arg            pattern matching name (glob syntax)
  -P [ --permissions ]            list permissions of workspace directory
  -v [ --verbose ]                verbose listing"
}

@test "ws_list warn about missing adminmail in config" {
    run ws_list --config bats/bad_ws.conf
    assert_output  --partial "warning: No adminmail in config!"
    assert_success
}



cleanup() {
    ws_release --config bats/ws.conf $ws_name
    assert_failure
}
