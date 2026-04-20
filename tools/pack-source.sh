#!/bin/bash

(
cd ..
export P="hpc-workspace-v2"
echo creating /tmp/hpc-workspace-v2-src.tgz
tar czf /tmp/hpc-workspace-v2-src.tgz $P/LICENSE $P/CMake* $P/src $P/documentation $P/*.adoc $P/bats $P/man $P/tests $P/tools $P/bin
echo done
)
