# hpc-workspace-v2

This is the construction site of next major version of hpc-workspace++ tool.

**DO NOT USE - it does not work - it is incomplete - it might even not compiler - it might eat your dog**

This is very rough and not ment for usage, and I will for the time being also
not expect or accept contributions, until some things are settled.

Please use the discussion tab if you would like to share input.

## Motivation/Goals

The codebase got harder and scarier to maintain and needs a major cleanup and modernization.

- Separation of configuration and database implementation from the client tools
is the first goal.

- Better Testing is the second goal.

- It is likely that all python tools will be replaced with C++ tools on the long run.

Functional extension is possible after this is achieved.

## Environment

at the moment main development platform is

- gcc (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0
- Description:    Ubuntu 22.04.5 LTS

future test platforms:

- Ubuntu 23.4 LTS
- Rocky Linux 9.4+

no intention to support old platforms like centos7, but it might work.

language level might evolve from c++17 to c++20 if there is reasons.

## Dependencies

yaml-cpp (to be removed)
{fmt} 
rapidyaml
boost program_options + boost system
Catch2 (if tests are used)
Guidelines Support Library (GSL)

## Status

- basically working implementation of ws_list in C++ (that was proof of concept for the separation of tool and config and DB implementation)
- basically working implementation of config and DB reading for old format
 
## Todo

- [ ] move from single file ws.conf to multifile ws.d
- [ ] migrate config from yaml-cpp to ryaml and remove yaml-cpp dependency
- [x] move to compiletime+runtime detected capability/setuid/usermode switch (usermode is for testing mainly, does not elevate privileges)
- [ ] add more unit tests to existing code
- [ ] build a better test framework for the tools
- [ ] debug what is there
- [ ] migrate more tools: finish ws_allocate, migrate ws_release, ws_restore, ws_find, ws_expirer, ws_validate
- [ ] add tests for new tools
- [ ] debug what is there
- [ ] get CMake setup in better shape
- [ ] migrate and check/correct/add documentation, guides and man pages
- [ ] test with more compilers and distributions
- [ ] do real live tests
- [ ] define and implement new DB formart

## Input and ideas and contributions needed

- how to automate testing? VMs? container?
- missing features in current version?

## Howto Build

stay tuned.

## Howto Unit Testing with catch2

mkdir build-debug
cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=1 ..
make 

ctest

