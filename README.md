# hpc-workspace-v2

This is the construction side of next major version of hpc-workspace++ tool.

**DO NOT USE - it does not work**

This is very rough and not ment for usage

## Motivation/Goals

The codebase got harder to maintain, and needs a major cleanup and modernization.

Separation of configuration and database implementation from the client tools
is the first goal.

Better Testing is the second goal.

It is likely that all python tools will be replaced with C++ tools.

Functional extension is possible after this is achieved.


## Status

- basically working implementation of ws_list in C++ (that was proof of concept for the separation of tool and config and DB implementation)
- basically working implementation of config and DB reading for old format

## Unit Testing with catch2

mkdir build-debug
cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=1 ..
make 

ctest
