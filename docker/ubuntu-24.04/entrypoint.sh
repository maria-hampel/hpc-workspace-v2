#!/bin/bash

command="$1"


run_bats_test=false
run_ctest_test=false
run_coverage=false

case "$command" in
  bats)
    run_bats_test=true
    ;;
  ctest)
    run_ctest_test=true
    ;;
  lcov)
    run_bats_test=true
    run_ctest_test=true
    run_coverage=true
    ;;
  testall)
    run_bats_test=true
    run_ctest_test=true
    ;;
  *)
    echo "available commands are: bats, ctest, lcov, testall"
    echo "for lcov please mount the container directory /ws/coverage_report to get your results"
    exit 1
    ;;
esac


if [ "$run_bats_test" = true ] || [ "$run_ctest_test" = true ]; then

  cd /tmp

  git clone https://github.com/holgerBerger/hpc-workspace-v2.git
  cd hpc-workspace-v2/external
  ./get_externals.sh
  cd ..

  if [ "$run_coverage" = true ]; then
    cmake --preset debug -DCMAKE_CXX_FLAGS=--coverage
  else
    cmake --preset debug
  fi

  cmake --build --preset debug -j

  # prepare setuid executable
  mkdir /tmp/setuid
  cp build/debug/bin/ws_allocate /tmp/setuid
  sudo chown root /tmp/setuid/ws_allocate
  sudo chmod u+s /tmp/setuid/ws_allocate

  # prepare capability executable
  mkdir /tmp/cap
  cp build/debug/bin/ws_allocate /tmp/cap
  sudo setcap "CAP_DAC_OVERRIDE=p CAP_CHOWN=p CAP_FOWNER=p" /tmp/cap/ws_allocate

  # create a config for setuid 
  export MYUID=$(id -u)
  export MYGID=$(id -g)
  sudo tee /etc/ws.conf >/dev/null <<SUDO
dbuid: $MYUID
dbgid: $MYGID
admins: [root]
adminmail: [root@a.com]
clustername: bats
duration: 10
maxextensions: 3
smtphost: mailhost
default: ws2
workspaces:
  ws1:
    database: /tmp/ws/ws1-db
    deleted: .removed
    keeptime: 7
    spaces: [/tmp/ws/ws1]
  ws2:
    database: /tmp/ws/ws2-db
    deleted: .removed
    keeptime: 7
    spaces: [/tmp/ws/ws2/1, /tmp/ws/ws2/2]
SUDO

  if [ "$run_coverage" = true ]; then
    lcov --capture --initial --config-file /tmp/ws/.lcovrc --directory . -o ws_base.info
  fi

  if [ "$run_ctest_test" = true ]; then
    ctest --preset debug
  fi
  if [ "$run_bats_test" = true ]; then
    OLDPATH=$PATH

    echo "running user tests"
    export PATH=build/debug/bin:$PATH
    bats bats/test
    export PATH=$OLDPATH

    # silence leak errors for setuid as it does not work
    export ASAN_OPTIONS=detect_leaks=0

    echo "running setuid tests"
    export PATH=/tmp/setuid:build/debug/bin:$PATH
    bats bats/test_setuid
    export PATH=$OLDPATH

    echo "running capability tests"
    export PATH=/tmp/cap:build/debug/bin:$PATH
    bats bats/test_cap
    export PATH=$OLDPATH

  fi

  if [ "$run_coverage" = true ]; then
    lcov --capture --config-file /tmp/ws/.lcovrc --directory . -o ws_test.info
    lcov -a ws_base.info -a ws_test.info -o ws_total.info
    genhtml  --ignore-errors source --output-directory /tmp/ws/coverage_report ws_total.info
  fi
fi
