# WSL / Linux-clang zssServer dev sandbox

A zero-cost, off-platform build of ZSS common code with `clang`, for fast local
iteration (repro bugs, run sanitizers, drive HTTP requests) without touching a
metered z/OS system. **Not a production build**: no real TLS/GSK, no ZIS, no
APIML — plaintext loopback only.

This directory is **committed on purpose**. An earlier working version of this
sandbox lived in an *uncommitted* sibling `zowe-common-c` tree that got deleted,
taking its whole port with it. Keeping the recipe in-repo is the fix for that.

## Pieces

| File | Role |
|---|---|
| `build.sh` | compile-sweep of common-c + zss under clang; z/OS-only TUs self-exclude; link what compiled + `wsl-stubs.o`. |
| `run.sh` | write a plaintext `zowe.yaml` and launch `build/zssServer` on `127.0.0.1:8544`. |
| `wsl-stubs.c` | hand-written Linux stubs for z/OS-only symbols the linker still needs (BPX, GSK, ZIS, rusermap…). |
| `wsl-include/gskcms.h` | minimal stub for the one z/OS system header `jwk.h` pulls (`x509_public_key_info`), so `zss.c` parses under clang. |
| `common-c-wsl-overlay.patch` | the common-c side of the port (apply to the sibling `zowe-common-c` before building). 12 files. |
| `chunk640/` | a **working** focused harness: links just the real `processHttpFragment` + drives real chunked HTTP to verify PR #640 (`chunk640-test.sh` = WSL/curl, `chunk640-zos.sh` = z/OS/self-drive). |

## Status (2026-07-27) — honest

- **`chunk640/` harness: WORKS.** `sh chunk640/chunk640-test.sh` builds and drives
  real chunked requests through the real parser, ASan-clean.
- **Full `zssServer`: LINKS, does not yet fully boot.** The GSK header wall is
  down (`wsl-include/gskcms.h` + `wsl-stubs.c`), but the real `zss.c` `main`
  still needs the remainder of the lost port re-derived — currently ~9 z/OS
  symbols (`crossmemory.h`/`zos.h` reachability, `ECVT`, `BPXYSTAT`, a couple of
  message `#define`s), plus a probable undefined-symbol link layer behind that.
- **Faster route to a serving smoke test (recommended):** a *minimal* main that
  makes an `HttpServer`, registers just the `/unixfile` dataservice, and calls
  `mainHttpLoop` — this sidesteps `zss.c`'s z/OS startup (ZIS connect, ECVT,
  privileged server) entirely, which a plaintext smoke test does not need.

## Build / run (once the overlay is applied)

```sh
# sibling zowe-common-c must have common-c-wsl-overlay.patch applied
export DEPS=/path/to/zowe-common-c/deps/configmgr        # quickjs + libyaml
sh build.sh          # -> build/zssServer
sh run.sh            # -> serves 127.0.0.1:8544 (plaintext, auth=fallback)
```
