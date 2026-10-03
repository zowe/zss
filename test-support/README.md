# test-support — running a standalone ZSS (and ZIS) for tests

A private, throwaway ZSS you can start, hit and tear down, with its own ZIS. No
APIML, no app-server, no launcher, no sysprog. Everything lives under your own
userid.

The point is to make server behaviour testable. Before this, a change to a ZSS
service could only be reviewed by reading it; now a defect can be demonstrated
and a fix shown to refuse what it should refuse.

## ZIS is required for any authenticated test

Not just for privileged services. `httpserver.c safAuthenticate()` compiled with
`-DAPF_AUTHORIZED=0`, which is what `build/build_zss64.sh` uses, verifies
passwords through ZIS:

```c
pwdCheckRC = zisCheckUsernameAndPassword(privilegedServerName, ...);
```

With no ZIS, ZSS cannot authenticate anyone, every request is 401, and a correct
password is indistinguishable from a wrong one. Budget for ZIS from the start.

## Setup

```sh
cd test-support

sh configure-test-env.sh          # generate zowe.yaml, create the instance tree
sh provision-keyring-cert.sh      # SAF keyring + server certificate (once)

sh zis/zis-configure.sh           # build and configure your own ZIS (once)
sh zis/zis-apf.sh                 # APF-authorize it (again after every IPL)
sh zis/zis-start.sh               # submit the ZIS job
tsocmd "STATUS ${USER}Z"          # want: EXECUTING

sh start-zss.sh                   # want: ZWES1014I ... cmsRC='0'
```

Then run whatever test you have. To compare two builds:

```sh
./ab-compare.sh --before bin/zssServer64.old --after bin/zssServer64.new \
                -- node suites/my-test.js --user MYUSERID
```

Run these under plain `/bin/sh`, not a zopen/ASCII shell: they handle EBCDIC
samplib members and MVS datasets with native tools.

## Porting it to your system

Everything site-specific is in **`test-env.sh`**, and every value defaults from
your userid or this directory's location. On a system laid out like ours it needs
no editing.

To pin anything, create **`test-env.local.sh`** (git-ignored) and set what
differs:

```sh
ZSS_TEST_INST=/my/scratch/zsstest       # default: $HOME/zsstest
ZSS_TEST_PORT=17600                     # default: 17557
ZSS_TEST_MSGCLASS=X                     # must be a HELD class on your system
ZSS_TEST_NODE_BIN=/path/to/node/bin     # if node is not already on PATH
```

Then `sh configure-test-env.sh --force` to regenerate `zowe.yaml`.

`zowe.yaml` and `test-env.local.sh` are git-ignored: they are site state. Edit
`zowe.yaml.template` for changes everyone should get.

Two values are genuinely system-dependent and worth checking first:

- **`ZSS_TEST_MSGCLASS` must be a held output class.** `TSO OUTPUT` can only
  retrieve held output, so without this a job failure is invisible unless you
  have SDSF.
- **Job names must be your userid plus at least one character.** TSO `STATUS`
  and `CANCEL` refuse anything else, so a server named otherwise starts and
  cannot be stopped. The scripts derive `<userid>Z` and `<userid>C`.

## What is here

| | |
|---|---|
| `test-env.sh` | the one place that knows anything site-specific |
| `configure-test-env.sh` | generates `zowe.yaml`, creates the instance tree |
| `zowe.yaml.template` | test ZSS configuration, `@TOKEN@` placeholders |
| `provision-keyring-cert.sh` | SAF keyring and server certificate |
| `start-zss.sh` | launch the test ZSS, detached by default |
| `zis/` | the private test ZIS: configure, apf, start, stop, status |
| `ab-compare.sh` | run any test against two builds and prove which answered |
| `schemas/` | vendored base schemas the config `$ref`s by `$id` |

Tests themselves live beside the change they belong to. The convention is a Node
script using built-in modules only, taking `--help`, and exiting 0 on success,
1 on the defect being present and 2 on an inconclusive run, so a pipeline can
gate on it. The first example is `ras-rbac-test.js`, added with the fix for #855.

## Writing a test that cannot lie to you

Learned the hard way, repeatedly. **Assert the identity of the thing under test,
not just the result.** `ab-compare.sh` enforces the first two of these for you,
and a test should do the third itself:

1. the server actually restarted (a `ZWES1013I` line appeared), otherwise it
   refuses to run rather than letting something else answer;
2. the two runs reported **different** build stamps, otherwise it declares the
   comparison invalid and exits 2;
3. the decisive cases carry the expected **reason** in the response body, not
   merely the expected status code. A status code alone is not evidence: a build
   without the fix can return the same code for an unrelated reason.

Each of those was added after that exact failure produced a confident, wrong
answer. An earlier version tested one binary twice and reported success both
times, because a process check used an unresolved `..` path that never matched
what `ps` prints.

Related traps, all of which report success while doing nothing: `// COMMAND` in
JCL never executes, `build_zss64.sh` prints success on a failed build, and a TSO
console activation fails with what looks like an authority error when the real
cause is that SDSF already holds a console of the same name.
