# test-support/zis - private, short-lived test ZIS (run as a JOB)

ZIS (ZWESIS) is the APF-authorized cross-memory (PC) server ZSS calls for privileged work
(impersonation, SAF/security services). The charset/#828 tests don't need it; impersonation
/ privileged-service tests do - and the bug under test might be **in ZIS itself**, so we
want our *own* build we can spin up, hit, and tear down.

**Model: our test ZIS runs as a private batch JOB, not a started task.** That makes it:
- **per-dev / self-service** - everything is `<userid>.*` datasets you can build yourself;
- **no sysprog in the loop** - no PROCLIB concatenation, no STARTED profile;
- **short-lived** - `submit` to start, `P`/cancel to stop;
- **ideal for ZIS development** - edit ZIS source -> `build_zis.sh` -> resubmit.

ZSS can't tell the difference: it connects to the cross-memory server by **name**
(`ZWESIS_TST`), not by how it was launched. Deliberately distinct from the shared
**production** ZIS (`ZWESIS_STD`, jobname `ZWE1SZ`) - we never touch it.

> Status: built from the repo samplib + `build_zis.sh` + `ZWESECUR`, **not yet run**.
> Validate on first execution (see VERIFY). Assumes RACF + operator authority.

## Two names to keep straight
- `ZIS_NAME` = `ZWESIS_TST` - the **cross-memory server** name (must match `zowe.yaml`
  `crossMemoryServerName`). System-wide, so make it unique if >1 instance runs at once.
- `ZIS_JOB` = `ZWESISTS` - the **MVS job** name (how you `P`/status it).

## Why no AUX / no proclib
ZIS's auxiliary address space (`ZWESAUX`) is created on demand via **`ASCRE`** (see
`zis-aux/src/aux-manager.c`), only when a **plugin** needs it. Our `ZWESIP00` keeps plugins
commented out, so the core test ZIS never starts an AUX - hence no `ZWESASTC`, no proclib.
(If a future test enables an AUX-using plugin, the AUX start needs handling then.)

## Files
- `zis-env.sh` - parameters (edit here) + `zis_provision`/`zis_dsn_exists`/`zis_opercmd`.
- `zis-configure.sh` - one-time: provision datasets, build, parmlib, **generate the server job**, APF, RACF.
- `zis-start.sh` - `SUBMIT` the server job.
- `zis-stop.sh` - `P` the job (cancel fallback).
- `zis-status.sh` - TSO `STATUS` + `D A`.

## Sequence (run under PLAIN /bin/sh, not env.sh)
```sh
sh test-support/zis/zis-configure.sh     # once (idempotent)
sh test-support/zis/zis-start.sh
sh test-support/zis/zis-status.sh
# point ZSS at it: zowe.yaml  crossMemoryServerName: ZWESIS_TST  -> restart ZSS
#   success = ZSS logs  ZWES1014I ZIS status 'Ok' ... cmsRC='0'
sh test-support/zis/zis-stop.sh          # tear down
```

## VERIFY on first run
1. **`ZWESIS01` runs happily as a JOB** (it's normally an STC) - the main thing to confirm. It should: same program, AC(1) authorizes a job step, PPT applies by program name.
2. **APF** - configure uses `,SMS`; switch to `,VOLUME=<vol>` if the loadlib is non-SMS.
3. **PPT** `PGMNAME(ZWESIS01) KEY(4) NOSWAP` - prod ZIS almost certainly set it; `D PPT` to confirm.
4. **JOB card** - set a real acct/class in `zis-env.sh` (`ZIS_JOBACCT`).
5. **STOP** - if `P ZWESISTS` doesn't stop a *job*, use `C ZWESISTS` (cancel); the server has recovery to clean up cross-memory resources either way.

## Charset / mechanism
`.sh` are EBCDIC (`IBM-1047`), run by `/bin/sh`; configure uses native `/bin` tools on the
EBCDIC samplib/datasets - don't run it under env.sh/zopen. Operator commands (`SETPROG`, `P`,
`D A`) are issued by submitting a `// COMMAND` job (`zis_opercmd`); their responses land in
SDSF/SYSLOG, not the shell. `STATUS`/`SUBMIT`/`LISTDS` go through `tsocmd` and return here.
