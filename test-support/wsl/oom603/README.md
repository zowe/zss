# oom603 — WSL smoke test for PR #603 (avoid ABEND on oversized request body)

Portable version of ChongZhou's z/OS-only `tests/httpserver/bigreqtest.c`.
Drives the REAL `processHttpFragment` with a small SLH (2x64K) and an oversized
body; #603 must return `httpReasonCode 500` instead of the NULL-write ABEND
(a SEGV on WSL).

Verified 2026-07-27:
- base tree  -> SEGV (exit 139) at the ABEND on the first OOM
- #603 tree  -> httpReasonCode 500, no crash, normal request still served

To run: apply #603 to the sibling ported `zowe-common-c` (drop in #603's
`c/utils.c` + `h/utils.h`; change the two `SLHAlloc(...)` body-allocs in
`c/httpserver.c` to `SLHAlloc2(..., true)` + a NULL->500 check), then build like
the chunk640 harness (link `oom603-test.o` + `build/obj/*.o` incl. `wsl-stubs.o`).
Automating the graft (as `chunk640-test.sh` does) is a follow-up.
