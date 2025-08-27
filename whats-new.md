# what's new in v2 vs v1

A short overview about the differences between old v1 hpc-workspace and this version,
including internals.

## new tools

- `ws_stat` allow to see how much diskspace a workspace uses
- `ws_editdb` allows the administrator to change DB entries, currently implemented is mass extension

## new functionality

- `ws_list` is a lot faster due to no python startup and faster listing and reading of DB
- `ws_list -g` is a lot faster as it does only read DB entries of owners sharing a group
- `ws_list -L` shows information aboit workspaces, including permissions and a comment given bu administrator
- `/etc/ws.d` is primary location of config, files are read in alphabetical order and merged, if no files there,
`/etc/ws.conf` is read
- `ws_release --delete-data` to wipe data while releasing a workspace (also in v1 since a while)
- `ws_restore --delete-data` to wipe data from an released or expired workspaces to reclaim disk space
- in config file: `filesystems` can be used as alias for `workspaces`, to match `-F` option of tools
- in config file: `default_workspace` is an alias for `default`
- in config file: `maxduration` is an alias for `duration`
- in config file: ACL syntax has `-` and `+`
- in config file: extended ACL syntax `[+|-]id[:[permission{,permission}]]` with permission in `list,use,create,extend,release,restore`
  allows to restrict single users or groups e.g. to use old workspaces in a filesystem but not extend them
- same executable can be used with setuid or capabilities (if capability support is detected at build time)
- `--version` switch can be used to see if capability or setuid is available and used
- most tools have `--config` option, which allows using workspace tools without privileges in users own directories and with own config file. This is usefull for testing.

## changed behaviour

- field `adminmail` as a list of email addresses is required in config (gives warning when not present)
- ws_release bails out if workspace name is not unique
- ws_allocate -x bails out if workspace name is not unique
- ws_allocate -g can take an groupname as well, -G can have no groupname
- all group workspaces have group sticky bit
- lua callout for path building is no longer supported, allocation options should fully replace its functionality
- user needs access to default workspace (was giving a warning in v1 for some years already)
- each tool does some checks on config validity and can bail out if config is bad, even for fiels it does not use
- new human check in `ws_restore`, avoid dependency on `terminfo` or `ncurses`

## what's new under the hood

- more tests
- CI pipeline
- no python dependency, more C++ tools with higher speed and consistent behaviour
- some tools use OpenMP parallelism
- abstraction of the DB, allowing easier tool development and will allow new functionality in DB in a coming version, planned is more privacy through better isolation of users/groups
- added dependencies to Catch2, curl, fmt, GSL, rapidyaml, spdlog
- curl and boost have to be installed from distribution, all others are compiled as part of building hpc-workspace-v2
