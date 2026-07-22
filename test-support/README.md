# ZSS test-support: service-level web-service testing

Boot a real `zssServer` locally (no z/OS, no TLS, no APIML) and drive its REST
services with assertion suites. Runs on plain Linux/WSL, so contributors can
write and run integration tests for a change without a mainframe - and the same
suites run unchanged against a real z/OS ZSS by changing only environment
variables.

See **[TESTING.md](TESTING.md)** for how to attach a test to your PR.

## Quick start (Linux/WSL)

```sh
# 1. Build a native zssServer (compiles the portable subset of zss +
#    zowe-common-c; z/OS-only translation units self-exclude, gaps are stubbed).
sh wsl/build.sh

# 2. Boot it: plaintext on 127.0.0.1:8544, serving a scratch dir with sample
#    files. Fallback auth accepts any credential on this loopback dev build.
sh wsl/run.sh

# 3. Run a suite against it. Output is TAP; exit code is red/green.
SERVED=wsl/build/instance/served
ZSS_URL=http://127.0.0.1:8544 ZSS_USER=me ZSS_PASS=me ZSS_TEST_DIR=$SERVED \
  node zsstest-run.js suites/unixfile-charset.js

sh wsl/run.sh --stop
```

## Layout

- `zsstest-run.js` - the assertion runner. Loads a suite, issues each request,
  checks `expect` (status, exact/`hex` body, includes/excludes), prints TAP.
  Supports `xfail` cases that reproduce a known-open bug: they report as an
  expected failure until the fix lands, then `XPASS` so the stale repro is noticed.
- `suites/*.js` - suites. Each exports `{ name, cases }`. `unixfile-charset.js`
  covers the zss#828 charset family.
- `wsl/` - the local sandbox: `build.sh` (native build), `run.sh` (boot/stop +
  generated dev `zowe.yaml`), `wsl-stubs.c` (Linux stand-ins for z/OS services).

## Running against z/OS

Point the same suite at a real ZSS: set `ZSS_URL` (its https URL), real
`ZSS_USER`/`ZSS_PASS`, `ZSS_CA` (PEM to verify TLS), and `ZSS_TEST_DIR` (a USS
dir holding the suite's files). No suite changes.

A fuller z/OS tier - standing up a multi-user, TLS, SAF-authenticated ZSS with
impersonation (certs, keyrings, ZIS) - is deferred to a follow-up; this PR is the
self-contained Linux/WSL service-testing core.
