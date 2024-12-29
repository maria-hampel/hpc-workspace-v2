#!/bin/bash

command="$1"


run_bats_test=false
run_ctest_test=false

case "$command" in
  bats)
    run_bats_test=true
    ;;
  ctest)
    run_ctest_test=true
    ;;
  testall)
    run_bats_test=true
    run_ctest_test=true
    ;;
  *)
    echo "available commands are: bats, ctest, testall"
    exit 1
    ;;
esac


if [ "$run_bats_test" = true ] || [ "$run_ctest_test" = true ]; then

  git clone https://github.com/holgerBerger/hpc-workspace-v2.git
  cd hpc-workspace-v2/external
  ./get_externals.sh
  cd ..

  cmake --preset debug
  cmake --build --preset debug -j

  if [ "$run_ctest_test" = true ]; then
    ctest --preset debug
  fi
  if [ "$run_bats_test" = true ]; then
    bats bats/test
  fi

fi
