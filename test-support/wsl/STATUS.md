# WSL zssServer build -- where it stands

Reproduced 2026-08-15 against `zowe-common-c` `dev/wsl-build` and this branch.

```
compiled: 46 TUs   excluded: 54 TUs
link incomplete -- undefined symbols:
    fileChangeTag
    main
```

Two symbols. That is the whole remaining gap between here and a zssServer
binary on Linux/WSL.

## How to reproduce

```sh
# 1. zowe-common-c on dev/wsl-build (= v3.x/staging + PR #666 + the port guards)
git clone git@github.com:zowe/zowe-common-c.git && cd zowe-common-c
git checkout dev/wsl-build

# 2. configmgr dependencies (quickjs + libyaml, cloned by the project script)
cd build && sh -c '. ./dependencies.sh; check_dependencies "$(cd .. && pwd)" ./configmgr.proj.env'
cd ../..

# 3. zss on dev/wsl-build
git clone git@github.com:zowe/zss.git && cd zss
git checkout dev/wsl-build

# 4. build
CC_COMMON=../zowe-common-c sh test-support/wsl/build.sh
```

`ASAN=1` adds AddressSanitizer. `build.sh` compiles every candidate TU and
records the ones that fail in `excluded.txt`, so the portable/z/OS partition is
discovered rather than maintained by hand.

## Blocker 1 -- `fileChangeTag`

`h/unixfile.h:448` declares it for every platform; only `c/zosfile.c` implements
it. `unixFileService.c` calls it unconditionally from `tagFileForUploading()` on
every upload, so pulling that service into the build pulls in the symbol.

POSIX has no per-file tags, so there is nothing to implement faithfully. The
options are:

- **a documented no-op returning success.** Matches the precedent psxfile.c
  already sets with `fileInfoCCSID()` ("CCSID is meaningless on a POSIX host").
  Cost: the tagging step of an upload silently does nothing, and any test that
  believes it exercised tagging is wrong.
- **return -1.** Honest, but `tagFileForUploading()` treats that as a failure
  and aborts the upload, which blocks exactly the `/unixfile` tests this build
  exists to run.
- **implement over an xattr** (`user.zowe.ccsid`). Faithful and testable, but
  it is a real feature with real surface, and it invents a tag concept the
  platform does not have.

Not decided. Needs a call before the ZSS testing PR goes up.

## Blocker 2 -- `main` (`zss.c` excluded)

```
zss.c:1094: error: use of undeclared identifier 'CrossMemoryServerName'
```

`zss.c` reaches ZIS cross-memory types directly. Those are z/OS-only by
construction -- cross-memory services, PC routines, metal C. This is not a
portability oversight the way `info.inode` was; it is ZSS's actual dependency on
ZIS at startup.

Likely shapes, roughly in order of how much they distort the thing under test:

- guard the ZIS block in `zss.c` and start with ZIS disabled off-platform,
  which is close to what a `--singleUser` sandbox already does
- provide the ZIS types and a stub client in `wsl-include`, so `zss.c` compiles
  unmodified and the calls fail at runtime
- link a `main` from the test harness instead and drive the server's
  initialisation directly, leaving `zss.c` out entirely

The first keeps the real startup path; the third gets a running server soonest
but stops testing startup.

## What this branch already fixed

- `build.sh` was passing the libyaml and quickjs version macros through an
  unquoted expansion, so the escaped quotes arrived as literal backslashes.
  `YAML_VERSION_STRING` became an int that `api.c` returns as a `const char *`,
  and `CONFIG_VERSION` became a bare token that broke `quickjs.c`'s string
  concatenation. Both dependencies failed to compile, which is where 124 of the
  original 126 unresolved symbols came from.
- The captured `common-c-wsl-overlay.patch` no longer applied. Rebased onto
  current staging on `zowe-common-c` `dev/wsl-build`; the `charsets.c`/`h`
  portion was dropped entirely because it was our own streaming work and merged
  upstream as #630.
- `unixFileService.c` now reaches file identity through `fileGetINode()` /
  `fileGetDeviceID()` (zowe-common-c#666) instead of BPXYSTAT field names.

## The part that needs eyes

`zowe-common-c` `dev/wsl-build` commit **"httpserver.c: non-z/OS behaviour for
the four z/OS-only auth paths"** is the only place any of this changes what the
server does about security. All four changes are inside `#ifdef` and no z/OS
path moves, but one of them -- `startImpersonating` returning TRUE off-platform
-- makes an operation that cannot happen report that it happened. Read that
commit on its own before trusting any authentication result from this build.
