# hpc-workspace-v2

This is the construction site of next major version of hpc-workspace++ tool.

**DO NOT USE - it does not work - it is incomplete - it might even not compile - it might eat your dog**

This is very rough and not ment for usage, and I will for the time being also
not expect or accept contributions, until some things are settled.

Please use the discussion tab if you would like to share input.

## Motivation/Goals

The codebase got harder and scarier to maintain and needed a major cleanup and modernization.

- Separation of configuration and database implementation from the client tools
is the first goal.

- Better testing is the second goal.

- It is likely that all python tools will be replaced with C++ tools on the long run.

- backwards compatibility will be maintained, might have some restrictions.

Functional extension is possible after this is achieved.

## Environment

at the moment main development platform is

- Ubuntu 22.04.5 LTS
  - CMake 3.22.1
  - gcc 11.4.0

future test platforms:

- Ubuntu 22.04.5 LTS
  - CMake 3.22.1
  - gcc 11.4.0
  - clang 14.0.0

- Ubuntu 24.4 LTS
  - CMake 3.28.3
  - gcc 13.3.0
  - clang 18.1.3

- Rocky Linux 8.10
  - CMake 3.26.5
  - gcc 8.5.0

- Rocky Linux 9.4
  - Cmake 3.26.5
  - gcc 11.4.1

this list can be extended.

no intention to support old platforms like centos7, but it might work.

language level might evolve from c++17 to c++20 if there is reasons.

ws_list has a dependency to -fopenmp, can be removed from CMakeList.txt
if not available.

## Dependencies

source is fetched and build as part of this tool:
- {fmt} 
- yaml-cpp 
- rapidyaml
- Guidelines Support Library (GSL)

library taken from distribution
- boost program_options + boost system
- libcap (optional if capability support is wanted instead of setuid)

for testing:
- Catch2 
- bats

## Status

- working implementation of config and DB reading for old format
- working implementation of ws_list in C++ (that was proof of concept for the separation of tool and config and DB implementation)
- working implementation of ws_find in C++
- working implementation of ws_allocate in C++
- working implementation of ws_release in C++
- working implementation of ws_restore in C++
- working implementation of ws_prepare in C++
 
## Todo

- [x] move from single file ws.conf to multifile ws.d
- [x] migrate config from yaml-cpp to ryaml and remove yaml-cpp dependency (on hold for the moment, uses both)
- [x] move to compiletime+runtime detected capability/setuid/usermode switch (usermode is for testing mainly, does not elevate privileges)
- [x] add more unit tests to existing code
- [x] build/select a better test framework for the tools
- [x] debug what is there
- [ ] migrate more tools: migrate ws_expirer, ws_validate
- [x] ws_list 
- [x] ws_allocate (testing ongoing)
- [x] ws_release (testing ongoing)
- [x] ws_restore (testing ongoing)
- [x] ws_find
- [x] ws_prepare
- [ ] add tests for new tools
- [ ] debug what is there
- [x] get CMake setup in better shape
- [x] remove tbb dependency
- [ ] migrate and check/correct/add documentation, guides and man pages
- [ ] test with more compilers and distributions
- [ ] do real live tests
- [ ] define and implement new DB formart

## Input and ideas and contributions needed

- how to automate testing? VMs? container?
- missing features in current version?

## Howto Build

for developers

```
cmake --preset debug
cmake --build --preset debug  -j 12
```

for mold users:
```
cmake --build --preset debug -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=mold" -j 12
```

for production

```
cmake --preset release
cmake --build --preset release  -j 12
```

## Testing

unit tests:

```
ctest --preset debug .
```

higher level tests (user mode only)

```
bats bats/test
```

## testing with docker

Tests in docker will use setuid and setcap mode as well and cover more corner cases.

```
cd docker
sudo docker build ubuntu-24.04 -t hpcwsv2
sudo docker run hpcwsv2 testall
```

note: setcap tests will fail with ASAN error messages if sysctl `fs.suid_dumpable = 2`
Update: turned out that capability version seems to have restrictions in docker

### testing with VM

A first Vagrant file is provided to allow testing with rocky linux 8,
this should allow to test capability version as well as setup with root_squash
filesystems.

