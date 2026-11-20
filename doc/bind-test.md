# `bind-test`

`bind-test` checks whether the current user and jobname can bind a TCP socket to
a local IPv4 address and port. It helps diagnose port conflicts and TCP/IP bind
permissions before starting a server. It closes the socket after the check and
does not exchange application data or validate TLS.

Run this z/OS utility from `<zowe.runtimeDirectory>/bin/utils/bind-test` in a Zowe
installation, or from `bin/bind-test` after building it in this repository. The
examples below assume the utility directory is on `PATH`.

## Usage

```sh
bind-test --host <hostname_or_ipv4> --port <tcp_port>
bind-test --help
```

| Argument or environment variable | Description |
|---|---|
| `--host` | Required. Local hostname or IPv4 address to bind. Hostnames are resolved to IPv4; IPv6 is not supported. Use `0.0.0.0` to bind on all local IPv4 interfaces. |
| `--port` | Required. TCP port to test. Supply a decimal port number from 1 to 65535. |
| `-h`, `--help` | Print help when supplied as the only argument. No arguments also prints help. |
| `_BPX_JOBNAME` | Optional environment variable to select the jobname used for the test. Use the intended server's jobname when checking jobname-dependent TCP/IP rules. |
| `USER` | Used only to label permission guidance in the output. Setting it does not change the executing user's identity. |

Run under the intended server user as well as its jobname for a representative
permission check. The result describes availability at the time of the test;
the utility does not reserve the port for a later server startup.

## Examples

Check port 7554 on all local IPv4 interfaces using the Gateway's jobname:

```sh
_BPX_JOBNAME=ZWE1AG bind-test --host 0.0.0.0 --port 7554
```

Successful output has this form (the pointer value varies):

```text
Bind succeeded (pointer=0x..., rc=0x0, rsn=0x0)
```

If the port is occupied, the output includes:

```text
Error: Port 7554 was already occupied
```

For other bind failures, the utility prints the socket return and reason codes
and suggests checking TCP/IP permissions. This suggestion does not establish
that permissions caused the failure. For example, the address must also belong
to the local system.

## Exit codes

| Code | Meaning |
|---|---|
| `0` | Bind succeeded, or help was printed. |
| `8` | Address validation, port validation, or socket setup failed. |

The `rc` and `rsn` printed for a socket operation are separate from the process
exit code. In particular, an occupied port returns **8**, not 4.

## Implementation references

- [Source](../c/bindTest.c)
- [Build script](../build/build_bind_test.sh)
