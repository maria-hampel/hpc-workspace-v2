setup() {
    load 'test_helper/common-setup'
    _common_setup
}


@test "ws_stat present" {
    which ws_stat
}

# bats test_tags=broken:v1-5-0
@test "ws_stat print version" {
    run ws_stat --version
    assert_output --partial "ws_stat"
    assert_success
}

@test "ws_stat print help" {
    run ws_stat --help
    assert_output --partial "Usage"
    assert_success
}

@test "ws_stat count files" {
    run ws_release --config bats/ws.conf TESTSTAT
    run ws_allocate --config bats/ws.conf TESTSTAT
    assert_success
    run ws_stat --config bats/ws.conf TESTSTAT
    assert_output --partial "files               : 0"
    assert_success
    touch $(ws_find --config bats/ws.conf TESTSTAT)/TESTFILE
    run ws_stat --config bats/ws.conf TESTSTAT
    assert_output --partial "files               : 1"
    assert_success
    rm -f $(ws_find --config bats/ws.conf TESTSTAT)/TESTFILE
}

# Test counting multiple files
@test "ws_stat count multiple files" {
    ws_allocate --config bats/ws.conf MULTIFILE
    WSPATH=$(ws_find --config bats/ws.conf MULTIFILE)
    touch "$WSPATH"/file1.txt
    touch "$WSPATH"/file2.txt
    touch "$WSPATH"/file3.txt
    run ws_stat --config bats/ws.conf MULTIFILE
    assert_output --partial "files               : 3"
    assert_success
    ws_release --config bats/ws.conf MULTIFILE
}

# Test counting directories
@test "ws_stat count directories" {
    ws_allocate --config bats/ws.conf DIRTEST
    WSPATH=$(ws_find --config bats/ws.conf DIRTEST)
    mkdir -p "$WSPATH"/dir1
    mkdir -p "$WSPATH"/dir2
    run ws_stat --config bats/ws.conf DIRTEST
    assert_output --partial "directories         : 2"
    assert_success
    ws_release --config bats/ws.conf DIRTEST
}

# Test counting softlinks
@test "ws_stat count softlinks" {
    ws_allocate --config bats/ws.conf LINKTEST
    WSPATH=$(ws_find --config bats/ws.conf LINKTEST)
    touch "$WSPATH"/target.txt
    ln -fs "$WSPATH"/target.txt "$WSPATH"/link1
    ln -fs "$WSPATH"/target.txt "$WSPATH"/link2
    run ws_stat --config bats/ws.conf LINKTEST
    assert_output --partial "softlinks           : 2"
    assert_output --partial "files               : 1"
    assert_success
    ws_release --config bats/ws.conf LINKTEST
}

# Test counting nested directories
@test "ws_stat count nested structure" {
    ws_allocate --config bats/ws.conf NESTEDTEST
    WSPATH=$(ws_find --config bats/ws.conf NESTEDTEST)
    mkdir -p "$WSPATH"/dir1/subdir1
    mkdir -p "$WSPATH"/dir1/subdir2
    touch "$WSPATH"/dir1/file1.txt
    touch "$WSPATH"/dir1/subdir1/file2.txt
    run ws_stat --config bats/ws.conf NESTEDTEST
    assert_output --partial "directories         : 3"
    assert_output --partial "files               : 2"
    assert_success
    ws_release --config bats/ws.conf NESTEDTEST
}

# Test byte counting
@test "ws_stat count bytes" {
    ws_allocate --config bats/ws.conf BYTETEST
    WSPATH=$(ws_find --config bats/ws.conf BYTETEST)
    echo "Hello World" > "$WSPATH"/test.txt
    run ws_stat --config bats/ws.conf BYTETEST
    assert_output --regexp "bytes               : [1-9][0-9]*"
    assert_success
    ws_release --config bats/ws.conf BYTETEST
}

# Test blocks counting
@test "ws_stat count blocks" {
    ws_allocate --config bats/ws.conf BLOCKTEST
    WSPATH=$(ws_find --config bats/ws.conf BLOCKTEST)
    echo "Some content" > "$WSPATH"/test.txt
    run ws_stat --config bats/ws.conf BLOCKTEST
    assert_output --regexp "blocks              : [0-9]+"
    assert_success
    ws_release --config bats/ws.conf BLOCKTEST
}

# Test empty workspace
@test "ws_stat empty workspace" {
    ws_allocate --config bats/ws.conf EMPTYTEST
    run ws_stat --config bats/ws.conf EMPTYTEST
    assert_output --partial "files               : 0"
    assert_output --partial "softlinks           : 0"
    assert_output --partial "directories         : 0"
    assert_output --partial "bytes               : 0"
    assert_success
    ws_release --config bats/ws.conf EMPTYTEST
}

# Test workspace path display
@test "ws_stat shows workspace path" {
    ws_allocate --config bats/ws.conf PATHTEST
    run ws_stat --config bats/ws.conf PATHTEST
    assert_output --partial "workspace directory"
    assert_output --partial "PATHTEST"
    assert_success
    ws_release --config bats/ws.conf PATHTEST
}

# Test workspace ID display
@test "ws_stat shows workspace ID" {
    ws_allocate --config bats/ws.conf IDTEST
    run ws_stat --config bats/ws.conf IDTEST
    assert_output --regexp "Id: .*-IDTEST"
    assert_success
    ws_release --config bats/ws.conf IDTEST
}

# Test pattern matching with wildcard
@test "ws_stat pattern matching wildcard" {
    ws_allocate --config bats/ws.conf PATTERN_A 1
    ws_allocate --config bats/ws.conf PATTERN_B 1
    ws_allocate --config bats/ws.conf OTHER_WS 1
    run ws_stat --config bats/ws.conf "PATTERN_*"
    assert_output --partial "PATTERN_A"
    assert_output --partial "PATTERN_B"
    refute_output --partial "OTHER_WS"
    assert_success
    ws_release --config bats/ws.conf PATTERN_A
    ws_release --config bats/ws.conf PATTERN_B
    ws_release --config bats/ws.conf OTHER_WS
}

# Test pattern matching exact name
@test "ws_stat pattern matching exact" {
    ws_allocate --config bats/ws.conf EXACT_NAME 1
    ws_allocate --config bats/ws.conf EXACT_NAME_2 1
    run ws_stat --config bats/ws.conf "EXACT_NAME"
    assert_output --partial "EXACT_NAME"
    refute_output --partial "EXACT_NAME_2"
    assert_success
    ws_release --config bats/ws.conf EXACT_NAME
    ws_release --config bats/ws.conf EXACT_NAME_2
}

# Test no pattern (match all)
@test "ws_stat no pattern matches all" {
    ws_allocate --config bats/ws.conf NOPATTERN1 1
    ws_allocate --config bats/ws.conf NOPATTERN2 1
    run ws_stat --config bats/ws.conf
    assert_output --partial "NOPATTERN1"
    assert_output --partial "NOPATTERN2"
    assert_success
    ws_release --config bats/ws.conf NOPATTERN1
    ws_release --config bats/ws.conf NOPATTERN2
}

# Test non-existent workspace
@test "ws_stat non-existent workspace" {
    run ws_stat --config bats/ws.conf DOESNOTEXIST99999
    refute_output --partial "Id: DOESNOTEXIST99999"
    assert_success
}

# Test multiple workspaces
@test "ws_stat multiple workspaces" {
    ws_allocate --config bats/ws.conf MULTI_A 1
    ws_allocate --config bats/ws.conf MULTI_B 1
    WSPATH_A=$(ws_find --config bats/ws.conf MULTI_A)
    WSPATH_B=$(ws_find --config bats/ws.conf MULTI_B)
    touch "$WSPATH_A"/file1.txt
    touch "$WSPATH_B"/file2.txt
    run ws_stat --config bats/ws.conf "MULTI_*"
    assert_output --partial "MULTI_A"
    assert_output --partial "MULTI_B"
    assert_success
    ws_release --config bats/ws.conf MULTI_A
    ws_release --config bats/ws.conf MULTI_B
}

# Test sort by name
@test "ws_stat sort by name" {
    ws_allocate --config bats/ws.conf SORT_C 1
    ws_allocate --config bats/ws.conf SORT_A 1
    ws_allocate --config bats/ws.conf SORT_B 1
    run ws_stat --config bats/ws.conf --name "SORT_*"
    assert_success
    # Check that they appear in the output
    assert_output --partial "SORT_A"
    assert_output --partial "SORT_B"
    assert_output --partial "SORT_C"
    ws_release --config bats/ws.conf SORT_A
    ws_release --config bats/ws.conf SORT_B
    ws_release --config bats/ws.conf SORT_C
}

# Test sort by creation
@test "ws_stat sort by creation" {
    ws_allocate --config bats/ws.conf CREATION_A 1
    sleep 1
    ws_allocate --config bats/ws.conf CREATION_B 1
    run ws_stat --config bats/ws.conf --creation "CREATION_*"
    assert_output --partial "CREATION_A"
    assert_output --partial "CREATION_B"
    assert_success
    ws_release --config bats/ws.conf CREATION_A
    ws_release --config bats/ws.conf CREATION_B
}

# Test sort by remaining time
@test "ws_stat sort by remaining" {
    ws_allocate --config bats/ws.conf REMAINING_A 1
    ws_allocate --config bats/ws.conf REMAINING_B 2
    run ws_stat --config bats/ws.conf --remaining "REMAINING_*"
    assert_output --partial "REMAINING_A"
    assert_output --partial "REMAINING_B"
    assert_success
    ws_release --config bats/ws.conf REMAINING_A
    ws_release --config bats/ws.conf REMAINING_B
}

# Test reverted sort
@test "ws_stat reverted sort" {
    ws_allocate --config bats/ws.conf REV_A 1
    ws_allocate --config bats/ws.conf REV_B 1
    run ws_stat --config bats/ws.conf --name --reverted "REV_*"
    assert_output --partial "REV_A"
    assert_output --partial "REV_B"
    assert_success
    ws_release --config bats/ws.conf REV_A
    ws_release --config bats/ws.conf REV_B
}

# Test verbose output
@test "ws_stat verbose output" {
    ws_allocate --config bats/ws.conf VERBOSETEST
    WSPATH=$(ws_find --config bats/ws.conf VERBOSETEST)
    touch "$WSPATH"/test.txt
    run ws_stat --config bats/ws.conf --verbose VERBOSETEST
    assert_output --partial "time[msec]"
    assert_output --partial "KFiles/sec"
    assert_success
    ws_release --config bats/ws.conf VERBOSETEST
}

# Test filesystem option
@test "ws_stat with filesystem option" {
    ws_allocate --config bats/ws.conf FSTEST 1
    run ws_stat --config bats/ws.conf --filesystem lustre01 FSTEST
    assert_success
    ws_release --config bats/ws.conf FSTEST
}

# Test large file content
@test "ws_stat large file bytes" {
    ws_allocate --config bats/ws.conf LARGEFILE
    WSPATH=$(ws_find --config bats/ws.conf LARGEFILE)
    dd if=/dev/zero of="$WSPATH"/largefile.bin bs=1024 count=100 2>/dev/null
    run ws_stat --config bats/ws.conf LARGEFILE
    assert_output --regexp "102,400"
    assert_success
    ws_release --config bats/ws.conf LARGEFILE
}

# Test mixed content (files, dirs, links)
@test "ws_stat mixed content" {
    ws_allocate --config bats/ws.conf MIXEDTEST
    WSPATH=$(ws_find --config bats/ws.conf MIXEDTEST)
    mkdir -p "$WSPATH"/dir1
    touch "$WSPATH"/file1.txt
    touch "$WSPATH"/file2.txt
    ln -s "$WSPATH"/file1.txt "$WSPATH"/link1
    run ws_stat --config bats/ws.conf MIXEDTEST
    assert_output --partial "files               : 2"
    assert_output --partial "softlinks           : 1"
    assert_output --partial "directories         : 1"
    assert_success
    ws_release --config bats/ws.conf MIXEDTEST
}

# Test deep directory nesting
@test "ws_stat deep nesting" {
    ws_allocate --config bats/ws.conf DEEPTEST
    WSPATH=$(ws_find --config bats/ws.conf DEEPTEST)
    mkdir -p "$WSPATH"/a/b/c/d/e
    touch "$WSPATH"/a/b/c/d/e/deep.txt
    run ws_stat --config bats/ws.conf DEEPTEST
    assert_output --partial "directories         : 5"
    assert_output --partial "files               : 1"
    assert_success
    ws_release --config bats/ws.conf DEEPTEST
}

# Test pretty bytes formatting
@test "ws_stat pretty bytes format" {
    ws_allocate --config bats/ws.conf PRETTYTEST
    WSPATH=$(ws_find --config bats/ws.conf PRETTYTEST)
    dd if=/dev/zero of="$WSPATH"/test.bin bs=1024 count=10 2>/dev/null
    run ws_stat --config bats/ws.conf PRETTYTEST
    # Should show both numeric and human-readable format
    assert_output --regexp "bytes.*\\(.*\\)"
    assert_success
    ws_release --config bats/ws.conf PRETTYTEST
}

# Test multiple files in subdirectories
@test "ws_stat files in subdirectories" {
    ws_allocate --config bats/ws.conf SUBFILETEST
    WSPATH=$(ws_find --config bats/ws.conf SUBFILETEST)
    mkdir -p "$WSPATH"/subdir
    touch "$WSPATH"/file1.txt
    touch "$WSPATH"/subdir/file2.txt
    touch "$WSPATH"/subdir/file3.txt
    run ws_stat --config bats/ws.conf SUBFILETEST
    assert_output --partial "files               : 3"
    assert_output --partial "directories         : 1"
    assert_success
    ws_release --config bats/ws.conf SUBFILETEST
}

# Test zero-byte files
@test "ws_stat zero-byte files" {
    ws_allocate --config bats/ws.conf ZEROTEST
    WSPATH=$(ws_find --config bats/ws.conf ZEROTEST)
    touch "$WSPATH"/empty1.txt
    touch "$WSPATH"/empty2.txt
    run ws_stat --config bats/ws.conf ZEROTEST
    assert_output --partial "files               : 2"
    assert_success
    ws_release --config bats/ws.conf ZEROTEST
}

# Test broken symlinks
@test "ws_stat broken symlinks" {
    ws_allocate --config bats/ws.conf BROKENLINKTEST
    WSPATH=$(ws_find --config bats/ws.conf BROKENLINKTEST)
    ln -s "$WSPATH"/nonexistent.txt "$WSPATH"/brokenlink
    run ws_stat --config bats/ws.conf BROKENLINKTEST
    assert_output --partial "softlinks           : 1"
    assert_success
    ws_release --config bats/ws.conf BROKENLINKTEST
}

# Test output formatting consistency
@test "ws_stat output format" {
    ws_allocate --config bats/ws.conf FORMATTEST
    run ws_stat --config bats/ws.conf FORMATTEST
    assert_output --regexp "Id:.*FORMATTEST"
    assert_output --regexp "workspace directory.*:"
    assert_output --regexp "files.*:"
    assert_output --regexp "softlinks.*:"
    assert_output --regexp "directories.*:"
    assert_output --regexp "bytes.*:"
    assert_output --regexp "blocks.*:"
    assert_success
    ws_release --config bats/ws.conf FORMATTEST
}

# Test handling of special characters in filenames
@test "ws_stat special character filenames" {
    ws_allocate --config bats/ws.conf SPECIALTEST
    WSPATH=$(ws_find --config bats/ws.conf SPECIALTEST)
    touch "$WSPATH"/file_with_underscore.txt
    touch "$WSPATH"/file-with-dash.txt
    run ws_stat --config bats/ws.conf SPECIALTEST
    assert_output --partial "files               : 2"
    assert_success
    ws_release --config bats/ws.conf SPECIALTEST
}

# Test workspace with many files (performance)
@test "ws_stat many files" {
    ws_allocate --config bats/ws.conf MANYFILES
    WSPATH=$(ws_find --config bats/ws.conf MANYFILES)
    for i in {1..50}; do
        touch "$WSPATH"/file$i.txt
    done
    run ws_stat --config bats/ws.conf MANYFILES
    assert_output --partial "files               : 50"
    assert_success
    ws_release --config bats/ws.conf MANYFILES
}
