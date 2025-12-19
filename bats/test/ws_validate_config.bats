setup() {
    load 'test_helper/common-setup'
    _common_setup
    test_config_dir="/tmp/ws-validate-test-$$"
    mkdir -p "$test_config_dir"
}

teardown() {
    if [ -d "$test_config_dir" ]; then
        rm -rf "$test_config_dir"
    fi
}

@test "ws_validate_config present" {
    which ws_validate_config
}

@test "ws_validate_config print help" {
    run ws_validate_config --help
    assert_output --partial "Usage"
    assert_output --partial "validate a config file"
    assert_success
}

@test "ws_validate_config valid config" {
    run ws_validate_config --config bats/ws.conf
    assert_output --partial "config is valid"
    assert_success
}

@test "ws_validate_config missing config file" {
    run ws_validate_config --config /nonexistent/config.yaml
    assert_failure
}

@test "ws_validate_config no workspaces defined" {
cat > "$test_config_dir/no_workspace.conf" <<EOF
admins:
- testadmin
adminmail:
- admin@test.com
default: ws1
EOF
    run ws_validate_config --config $test_config_dir/no_workspace.conf
    assert_output --partial "No Workspaces defined"
    assert_failure
}

@test "ws_validate_config missing clustername" {
    cat > "$test_config_dir/no_cluster.conf" <<EOF
admins:
  - testadmin
adminmail:
  - admin@test.com
default: ws1
workspaces:
  ws1:
    deleted: .removed
    spaces:
      - /tmp/ws/ws1
    database: /tmp/ws/ws1-db
EOF
    run ws_validate_config --config "$test_config_dir/no_cluster.conf"
    assert_output --partial "No clustername"
    assert_failure
}

@test "ws_validate_config missing admins" {
    cat > "$test_config_dir/no_admins.conf" <<EOF
clustername: testcluster
adminmail:
  - admin@test.com
default: ws1
workspaces:
  ws1:
    deleted: .removed
    spaces:
      - /tmp/ws/ws1
    database: /tmp/ws/ws1-db
EOF
    run ws_validate_config --config "$test_config_dir/no_admins.conf"
    assert_output --partial "No admins found"
    assert_failure
}

@test "ws_validate_config missing adminmail" {
    cat > "$test_config_dir/no_adminmail.conf" <<EOF
clustername: testcluster
admins:
  - testadmin
default: ws1
workspaces:
  ws1:
    deleted: .removed
    spaces:
      - /tmp/ws/ws1
    database: /tmp/ws/ws1-db
EOF
    run ws_validate_config --config "$test_config_dir/no_adminmail.conf"
    assert_output --partial "No adminmail found"
    assert_failure
}

@test "ws_validate_config missing default workspace" {
    cat > "$test_config_dir/no_default.conf" <<EOF
clustername: testcluster
admins:
  - testadmin
adminmail:
  - admin@test.com
workspaces:
  ws1:
    deleted: .removed
    spaces:
      - /tmp/ws/ws1
    database: /tmp/ws/ws1-db
EOF
    run ws_validate_config --config "$test_config_dir/no_default.conf"
    assert_output --partial "No default workspace found"
    assert_failure
}

@test "ws_validate_config default workspace not in list" {
    cat > "$test_config_dir/bad_default.conf" <<EOF
clustername: testcluster
admins:
  - testadmin
adminmail:
  - admin@test.com
default: nonexistent
workspaces:
  ws1:
    deleted: .removed
    spaces:
      - /tmp/ws/ws1
    database: /tmp/ws/ws1-db
EOF
    run ws_validate_config --config "$test_config_dir/bad_default.conf"
    assert_output --partial "default workspace is not defined"
    assert_failure
}

@test "ws_validate_config missing smtphost warning" {
    cat > "$test_config_dir/no_smtp.conf" <<EOF
clustername: testcluster
admins:
  - testadmin
adminmail:
  - admin@test.com
default: ws1
workspaces:
  ws1:
    deleted: .removed
    spaces:
      - /tmp/ws/ws1
    database: /tmp/ws/ws1-db
EOF
    run ws_validate_config --config "$test_config_dir/no_smtp.conf"
    assert_output --partial "No smtphost found"
    assert_output --partial "config is valid"
    assert_success
}

@test "ws_validate_config missing mail_from warning" {
    cat > "$test_config_dir/no_mailfrom.conf" <<EOF
clustername: testcluster
smtphost: localhost
admins:
  - testadmin
adminmail:
  - admin@test.com
default: ws1
workspaces:
  ws1:
    deleted: .removed
    spaces:
      - /tmp/ws/ws1
    database: /tmp/ws/ws1-db
EOF
    run ws_validate_config --config "$test_config_dir/no_mailfrom.conf"
    assert_output --partial "No mail_from found"
    assert_output --partial "config is valid"
    assert_success
}

@test "ws_validate_config filesystem missing deleted" {
    cat > "$test_config_dir/no_deleted.conf" <<EOF
clustername: testcluster
admins:
  - testadmin
adminmail:
  - admin@test.com
default: ws1
workspaces:
  ws1:
    spaces:
      - /tmp/ws/ws1
    database: /tmp/ws/ws1-db
EOF
    run ws_validate_config --config "$test_config_dir/no_deleted.conf"
    assert_output --partial "No deleted directory found"
    assert_failure
}

@test "ws_validate_config filesystem missing spaces" {
    cat > "$test_config_dir/no_spaces.conf" <<EOF
clustername: testcluster
admins:
  - testadmin
adminmail:
  - admin@test.com
default: ws1
workspaces:
  ws1:
    deleted: .removed
    database: /tmp/ws/ws1-db
EOF
    run ws_validate_config --config "$test_config_dir/no_spaces.conf"
    assert_output --partial "No spaces found"
    assert_failure
}

@test "ws_validate_config filesystem missing database" {
    cat > "$test_config_dir/no_database.conf" <<EOF
clustername: testcluster
admins:
  - testadmin
adminmail:
  - admin@test.com
default: ws1
workspaces:
  ws1:
    deleted: .removed
    spaces:
      - /tmp/ws/ws1
EOF
    run ws_validate_config --config "$test_config_dir/no_database.conf"
    assert_output --partial "No database location define"
    assert_failure
}

@test "ws_validate_config shows all config values" {
    run ws_validate_config --config bats/ws.conf
    assert_output --partial "clustername:"
    assert_output --partial "maxduration:"
    assert_output --partial "durationdefault:"
    assert_output --partial "reminderdefault:"
    assert_output --partial "maxextensions:"
    assert_output --partial "dbuid:"
    assert_output --partial "dbgid:"
    assert_output --partial "admins:"
    assert_output --partial "adminmail:"
    assert_success
}

@test "ws_validate_config shows filesystem details" {
    run ws_validate_config --config bats/ws.conf
    assert_output --partial "checking config for filesystem"
    assert_output --partial "deleted:"
    assert_output --partial "spaces:"
    assert_output --partial "workspace database directory:"
    assert_output --partial "keeptime:"
    assert_output --partial "allocatable:"
    assert_output --partial "extendable:"
    assert_output --partial "restorable:"
    assert_success
}

@test "ws_validate_config warns about missing directories" {
    mkdir -p "$test_config_dir/testdb"
    touch "$test_config_dir/testdb/.ws_db_magic"
    mkdir -p "$test_config_dir/testdb/.removed"

    cat > "$test_config_dir/missing_dirs.conf" <<EOF
clustername: testcluster
admins:
  - testadmin
adminmail:
  - admin@test.com
default: ws1
workspaces:
  ws1:
    deleted: .removed
    spaces:
      - /nonexistent/space1
      - /nonexistent/space2
    database: $test_config_dir/testdb
EOF
    run ws_validate_config --config "$test_config_dir/missing_dirs.conf"
    assert_output --partial "does not exist"
    assert_success
}

@test "ws_validate_config warns about missing .ws_db_magic" {
    mkdir -p "$test_config_dir/db_no_magic"
    mkdir -p "$test_config_dir/db_no_magic/.removed"
    mkdir -p "$test_config_dir/space1"
    mkdir -p "$test_config_dir/space1/.removed"

    cat > "$test_config_dir/no_magic.conf" <<EOF
clustername: testcluster
admins:
  - testadmin
adminmail:
  - admin@test.com
default: ws1
workspaces:
  ws1:
    deleted: .removed
    spaces:
      - $test_config_dir/space1
    database: $test_config_dir/db_no_magic
EOF
    run ws_validate_config --config "$test_config_dir/no_magic.conf"
    assert_output --partial "does not contain .ws_db_magic"
    assert_success
}

@test "ws_validate_config validates bad_ws.conf" {
    run ws_validate_config --config bats/bad_ws.conf
    assert_output --partial "No adminmail"
    assert_failure
}

@test "ws_validate_config help shows usage" {
    run ws_validate_config --help
    assert_output --partial "[filename]"
    assert_success
}
