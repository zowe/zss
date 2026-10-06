# test-support - a standalone ZSS (and ZIS) for tests

A private, throwaway ZSS you can start, hit and tear down, with its own ZIS. No
APIML, no app-server, no launcher, no systems programmer, and no Zowe install.
Everything lives under your own userid and is disposable.

The point is to make server behaviour testable. Before this, a change to a ZSS
service could only be reviewed by reading it. Now a defect can be demonstrated
and a fix shown to refuse what it should refuse.

## One command

```sh
./zss-test env                  # show the resolved configuration
./zss-test configure            # generate zowe.yaml, make the instance tree, fetch schemas
./zss-test cert                 # SAF key ring and server certificate (once)
./zss-test zis configure        # build and set up your own ZIS (once)
./zss-test zis apf              # APF-authorize it (again after every IPL)
./zss-test zis start            # want: EXECUTING
./zss-test server start         # want: ZWES1014I ... cmsRC='0'
./zss-test check                # is all of the above actually true?
```

Then run a test, and to compare two builds:

```sh
./zss-test compare --before bin/zssServer64.old --after bin/zssServer64.new \
                   -- node my-test.js --user MYUSERID
```

Exit codes suit a pipeline: 0 success, 1 the thing under test failed, 2 the
harness could not run or could not trust its own result.

## Asking whether you are ready

`./zss-test check` is read-only. It creates nothing, starts nothing and submits
nothing, and it answers in one pass: whether `zowe.yaml` is generated and still
agrees with the configuration, whether the instance tree and the Zowe base
schemas are present and correctly tagged, whether the key ring holds both
certificates, whether the ZIS load library, parmlib and JCL members exist,
whether this userid has `ZWES.IS` access, whether ZIS is executing, and whether
the server is answering and reached ZIS.

Each failure names the command that fixes it, and the report ends with those
commands in dependency order. Checks that need z/OS report `skip` on a
workstation, so the configuration half of the report is still useful there.

It exists because readiness used to be discovered one failure at a time, and
the worst case was silent: with no ZIS the server starts normally and answers
401 to everything, which reads as a broken test rather than a missing server.

## How a test uses this

A test is any program. It imports nothing from here and need not be TypeScript.
The harness starts the server, exports where it is, and runs the test:

```
ZSS_TEST_URL      https://127.0.0.1:17557
ZSS_TEST_ADDR     127.0.0.1
ZSS_TEST_PORT     17557
ZSS_TEST_USERID   ZOWEAD5
ZSS_TEST_INST     /u/zowead5/zsstest
ZSS_TEST_LOG      /u/zowead5/zsstest/logs/zss.out
```

So read `ZSS_TEST_URL` rather than hardcoding a port. That is what makes it
safe to give each CI worker its own.

```sh
./zss-test run -- node my-test.js          # start the server if needed, then run
./zss-test compare --before A --after B -- node my-test.js
```

The exit code belongs to the test and the harness passes it through: 0 success,
1 the defect is present, 2 inconclusive.

## ZIS is required for any authenticated test

Not just for privileged services. `httpserver.c safAuthenticate()` compiled with
`-DAPF_AUTHORIZED=0`, which is what `build/build_zss64.sh` uses, verifies
passwords through ZIS:

```c
pwdCheckRC = zisCheckUsernameAndPassword(privilegedServerName, ...);
```

With no ZIS, ZSS cannot authenticate anyone, every request returns 401, and a
correct password is indistinguishable from a wrong one. Budget for ZIS from the
start. `singleUserMode` is not an alternative: `zss.c` enables it only on the
pipe transport with the TCP port disabled.

## It is TypeScript, with one shell file

`zss-test` is a shell bootstrap and nothing else: it finds node, builds the
TypeScript if the build is missing or stale, and hands over. Everything the
harness actually does is in `src/*.ts`.

That split is deliberate. An earlier version of this was eleven shell scripts,
and measuring them showed only 9% of their lines invoked a z/OS facility at all.
The other 91% was templating, polling, parsing command output and deciding what a
result meant, which are not things shell should be asked to do, and which nobody
can review quickly.

`src/zos.ts` is the only module that shells out. Every z/OS facility this harness
needs is wrapped exactly once there: TSO commands, writing a PDS member,
submitting and watching a job, operator commands, file tagging and code-page
conversion. The rest is ordinary TypeScript.

Node is often not on the default PATH on z/OS, which is the only reason the
bootstrap exists. Set `ZSS_TEST_NODE_BIN` if it is somewhere unusual. The build
needs `typescript`, which the bootstrap installs on first use; with no outbound
network, build `lib/` on a workstation and copy it across.

## Porting it to your system

Everything site-specific is in `src/env.ts`, and every value defaults from the
invoking userid or from the directory layout. On a conventional system it needs
no editing at all, which `./zss-test env` will show you.

To pin anything, write `test-env.local.json` next to this file. It is
git-ignored:

```json
{
  "instance": "/my/scratch/zsstest",
  "port": 17600,
  "msgclass": "X"
}
```

Any value can also be overridden by an environment variable: `ZSS_TEST_INST`,
`ZSS_TEST_PORT`, `ZSS_TEST_MSGCLASS`, and so on.

Two values are genuinely system-dependent, and the harness refuses to start
rather than let you discover them the hard way:

- **The job message class must be a held output class.** `TSO OUTPUT` can only
  retrieve held output, so without this a job failure is invisible to anyone
  without SDSF.
- **Job names must be your userid plus at least one character.** TSO `STATUS` and
  `CANCEL` refuse anything else, so a server named otherwise starts and cannot be
  stopped. `env.ts` checks this and explains it.

## The Zowe base schemas are fetched, not copied

The ZSS config schema refers by `$id` to two schemas that belong to Zowe:
`zowe-yaml-schema.json` (server-base) and `server-common.json`.

They are **not** kept here. `./zss-test configure` fetches them into the instance
directory, or do it alone:

```sh
./zss-test fetch-schemas --ref v2.x/staging
./zss-test fetch-schemas --from-install      # no outbound network
```

A copy checked in would be a snapshot of one moment. These change between Zowe
releases, by about 11KB between the two installs on our test system, so a copy
drifts and the server ends up validated against rules Zowe no longer uses.

Whatever the source, the bytes need work on z/OS: every installed copy we have
seen is **untagged**, and the EBCDIC config manager reads untagged ASCII as
EBCDIC and sees garbage. The fetch converts to IBM-1047 and tags, and validates
the `$id` before converting, so a redirect or an error page cannot be faithfully
turned into EBCDIC and handed to the server.

## Writing a test that cannot lie to you

Learned the hard way, repeatedly. **Assert the identity of the thing under test,
not just the result.** `./zss-test compare` enforces the first two of these; the
third belongs in each test:

1. the server actually restarted, by finding its `ZWES1013I` line, otherwise it
   refuses to run rather than letting something else answer;
2. the two runs reported **different** build stamps, otherwise it declares the
   comparison invalid and exits 2;
3. a result carries the expected **reason**, not merely the expected status code.
   The same code can arise for an unrelated cause, which is how a build without
   a fix once looked fixed.

Every one of those was added after that exact failure produced a confident,
wrong answer. An earlier version compared one binary against itself and reported
success twice, because a process check contained an unresolved `..` that never
matched what `ps` prints.

Related traps, all of which report success while doing nothing: a JCL
`// COMMAND` statement never executes and the job waits forever while `SUBMIT`
reports success; `build_zss64.sh` prints success after a failed compile; and a
TSO console activation fails with what looks like an authority error when the
real cause is that SDSF already holds a console of that name. Each is commented
where the code works around it.

## Everything here stays ASCII

The repository `.gitattributes` says
`* git-encoding=iso8859-1 zos-working-tree-encoding=ibm-1047`, so every file is
converted to EBCDIC when checked out on z/OS. IBM-1047 has no em dash, curly
quote or ellipsis, so one typographic character anywhere makes
`git checkout` fail outright:

```
error: failed to encode 'test-support/README.md' from UTF-8 to ibm-1047
```

That is not a warning and it does not skip the file. Nothing checks out at all
until the character is gone. Keep source, comments and documentation to plain
ASCII, including in prose.

## What is here

| | |
|---|---|
| `zss-test` | the shell bootstrap: find node, build, hand over |
| `src/zos.ts` | the only module that shells out |
| `src/env.ts` | the only module that knows anything site-specific |
| `src/zis.ts` | the private test ZIS: configure, APF, start, stop, status |
| `src/zss.ts` | the test server: start, stop, status, swap builds |
| `src/cert.ts` | SAF key ring and server certificate |
| `src/schemas.ts` | fetch and convert the Zowe base schemas |
| `src/configure.ts` | generate `zowe.yaml` from the template |
| `src/abCompare.ts` | run a test against two builds and prove which answered |
| `src/check.ts` | read-only readiness: what is ready, and what command fixes the rest |
| `src/cli.ts` | the single entry point |
| `zowe.yaml.template` | test ZSS configuration |
| `zis/README.md` | why the test ZIS runs as a job, and what to verify |

`zowe.yaml`, `test-env.local.json`, `lib/` and `node_modules/` are git-ignored:
they are site state or build output, not source.
