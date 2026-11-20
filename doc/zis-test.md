# `zis-test`

`zis-test` checks the status of a named Zowe Cross-Memory Server (ZIS) using
`cmsGetStatus`. It helps determine whether ZIS is running and accessible to the
executing user. This is a cross-memory check, not a TCP connection test or a
test of every ZIS service.

Run this z/OS utility from `<zowe.runtimeDirectory>/bin/utils/zis-test` in a Zowe
installation, or from `bin/zis-test` after building it in this repository. The
examples assume the utility directory is on `PATH`.

## Usage

```sh
zis-test --zis <crossMemoryServerName>
zis-test --help
```

| Argument | Description |
|---|---|
| `--zis` | Required for a status check. The ZIS server name, corresponding to `components.zss.crossMemoryServerName` in Zowe YAML. This is the cross-memory server name, not the started task's jobname. |
| `-h`, `--help` | Print help when supplied as the only argument. No arguments also prints help. |

The utility does not read Zowe YAML itself or supply a default server name.
It rejects a missing `--zis` value and names longer than
`sizeof(CrossMemoryServerName)`; the latter error prints the supported maximum.
Run it as the intended ZSS user to check that user's access.

The repository's [ZSS defaults](../defaults.yaml) use `ZWESIS_STD` when neither
`zowe.environments.ZWED_privilegedServerName` nor
`zowe.environments.ZWES_XMEM_SERVER_NAME` supplies a name. Use the effective
configuration for your installation; `ZWESIS01` below is an example name.

## Example and output

```sh
zis-test --zis ZWESIS01
```

Output starts with:

```text
Checking ZIS named 'ZWESIS01'
```

The next line uses one of these formats. Angle-bracket values are placeholders:

```text
ZIS Ok (rc='0', description='<status description>', clientVersion='<client version>')
ZIS Error (rc='<CMS return code>', description='<status description>', clientVersion='<client version>')
```

The description comes from `cmsGetStatus`. `clientVersion` is the utility's
compiled `CROSS_MEMORY_SERVER_VERSION`, not the installed Zowe release or a
reported server version. The printed `rc` is the CMS status code; it is separate
from the utility's process exit code.

## Exit codes and troubleshooting

| Code | Meaning |
|---|---|
| `0` | The CMS status check succeeded, or help was printed. |
| `8` | A required name was missing or too long, or the CMS status check returned a nonzero code. |

For `RC_CMS_PERMISSION_DENIED`, the utility prints:

```text
Error: program lacks permission to use ZIS
```

It then advises checking READ access to `ZWES.IS` in the `FACILITY` class for
the Zowe STC identity. This profile name is hard-coded in the diagnostic;
confirm the security configuration of your ZIS instance. The `USER` environment
variable only supplies a possible username for that message.

For `RC_CMS_ZVT_NULL`, `RC_CMS_ZERO_PC_NUMBER`, `RC_CMS_GLOBAL_AREA_NULL`, or
`RC_CMS_SERVER_NOT_READY`, it prints:

```text
The ZIS STC does not appear to be running. Start the job (Default: ZWESISTC) before starting the rest of Zowe
```

Check that the intended ZIS started task is active and that `--zis` names the
correct instance. Other failures still print their CMS status and description,
but do not receive this additional guidance.

## ZSS validation integration

The [ZSS manifest](../manifest.template.yaml) registers
[`bin/validate.sh`](../bin/validate.sh) as `commands.validate`. On z/OS, that
script runs:

```sh
"${ZWE_zowe_runtimeDirectory}/bin/utils/zis-test" --zis "${ZWE_components_zss_crossMemoryServerName}"
```

The script returns the utility's exit code. On other operating systems it
reports that ZSS requires z/OS and exits with 1. The script itself has no
`zowe.launchScript.startupChecks` switch to skip the ZIS check.

## Implementation references

- [Source](../c/zisTest.c)
- [Build script](../build/build_zis_test.sh)
