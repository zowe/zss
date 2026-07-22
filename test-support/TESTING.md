# Testing your zowe-common-c / ZSS change

This is the short version of "what do I attach to my PR so it can be reviewed and
merged." Two kinds of tests, pick the one that fits your change. Both run on a
plain Linux box (WSL is fine) under AddressSanitizer/UBSanitizer, and both also
run on z/OS. You do not need a mainframe to write or run either one.

## 1. Function-level test (most bug fixes)

If your fix is inside one function or module - a length check, an off-by-one, a
bad calculation - write a small C program that calls it and asserts.

- Put it under `tests/<area>/` (e.g. `tests/httpserver/`), one `.c` per case set.
- No mainframe-only build lines. Use the shared recipe: `tests/build.sh <area>`
  compiles your test plus the common-c TUs it needs with `clang` and
  `-fsanitize=address,undefined`, and runs it. (Mirrors how the existing
  `tests/charset-streaming` and `tests/clang_readiness` cases build.)
- Assert with a nonzero exit on failure. Print `ok`/`FAIL` lines.
- If the code path is z/OS-only, guard the mainframe pieces with
  `#ifdef __ZOWE_OS_ZOS` so the portable part still builds and runs under the
  sanitizers on Linux. The sanitizer run is the point - it is where the buffer
  overrun / use-after-free actually gets caught.

You almost certainly already write these (see #602, #603, #607). The only change
is: use the shared build recipe instead of a hand-rolled `c89` Makefile, so it
runs on Linux under ASan and does not need `_C89_L6SYSLIB` or a copied `zis`
folder.

## 2. Service-level test (behavior seen through a running server)

If your change is only observable by making a request to ZSS - a `/unixfile`
response, a status code, a charset - write a suite for the runner instead.

- Boot a local ZSS: `test-support/wsl/run.sh` (plaintext, loopback, no TLS/APIML,
  fallback auth accepts any credential). No z/OS needed.
- Add a suite in `test-support/suites/<name>.js` exporting `{ name, cases }`.
  Each case is a request + an `expect` (status, exact bytes, hex includes/excludes).
  `/unixfile` success bodies are whole-body base64; the runner decodes and asserts
  on the decoded bytes.
- Run it: `ZSS_URL=... ZSS_USER=x ZSS_PASS=x ZSS_TEST_DIR=<served dir> \`
  `node zsstest-run.js suites/<name>.js`. Output is TAP; exit code is red/green.
- The identical suite runs against a real z/OS ZSS by changing only the env vars
  (`ZSS_URL`, real credentials, a `ZSS_CA` for TLS). Write once, run on any tier.

The `#828` charset fix is the worked example: `suites/unixfile-charset.js` boots
the sandbox and proves the fix - and fails on an unfixed build, which is what a
regression test is for.

## What to attach to your PR

- The test (function-level or a suite), committed under `tests/` or
  `test-support/suites/`.
- One line in the PR body: the command you ran and that it passed
  (`tests/build.sh httpserver` -> ok, or the `node zsstest-run.js ...` line).
- If it is a bug fix: the test must **fail on the code before your change** and
  pass after. That is the artifact a reviewer checks in a minute instead of
  reading a description.

CI wiring (running these automatically on every PR) is separate and owned by the
testing lead; it is not something you set up. Your job is only to make the test
runnable by the recipe above so that when CI arrives, it just works.

## Not covered yet

- Privileged / cross-memory paths that require ZIS and APF authorization run only
  on z/OS (see `test-support/zis/`). Write those as function-level tests guarded
  by `__ZOWE_OS_ZOS`; the sanitizer coverage of the portable half still helps.
- TLS/keyring and impersonation (multi-user) are a z/OS-only tier; the runner
  suites work there unchanged, but standing that server up needs the cert +
  BPX.SERVER setup documented in `test-support/README.md`.
