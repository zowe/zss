# `attls-test`

`attls-test` checks the AT-TLS policy reported for a socket on a connection that
the utility creates on the local z/OS system. It binds a server socket, connects
a client socket to that server, exchanges a short test message, and queries one
side of the connection using `SIOCTTLSCTL` with `TTLS_QUERY_ONLY`.

The utility needs an available local port. It does not inspect an already
running server or connect to an arbitrary remote service. Both ends of the test
connection run in the same process, under the same user and jobname.

The [build script](../build/build_attls_test.sh) produces `bin/attls-test` in this
repository. If your Zowe distribution includes the utility, look in
`<zowe.runtimeDirectory>/bin/utils`. The examples assume its directory is on `PATH`.

## Usage

```sh
attls-test --serverHost <local_hostname_or_ipv4> --serverPort <tcp_port> --direction <1_or_2>
attls-test --help
```

| Argument or environment variable | Description |
|---|---|
| `--serverHost` | Required. Local hostname or IPv4 address used both to bind the server and to connect the client. IPv6 is not supported. Use a concrete local address reachable by the test client. |
| `--serverPort` | Required. Available local TCP port from 1 to 65535. |
| `--direction` | Required. `1` queries the accepted server socket (inbound); `2` queries the client socket (outbound). Pass the number, not the label. |
| `-h`, `--help` | Print help when supplied as the only argument. No arguments also prints help. |
| `_BPX_JOBNAME` | Optional environment variable to select the jobname used by the test, including for jobname-dependent TCP/IP and AT-TLS rules. |
| `USER` | Labels diagnostics; changing it does not change the executing user's identity. |

Supply plain decimal numbers for the port and direction.

## Examples

Check inbound policy on port 7554, replacing the address and jobname with the
ones used by your server. Run this before the server occupies the port:

```sh
_BPX_JOBNAME=ZWE1AG attls-test --serverHost 192.0.2.10 --serverPort 7554 --direction 1
```

Query the client side of the same kind of local test connection:

```sh
_BPX_JOBNAME=ZWE1AG attls-test --serverHost 192.0.2.10 --serverPort 7554 --direction 2
```

When a policy is enabled, output ends with a line of this form:

```text
AT-TLS rule identified on 192.0.2.10:7554 for user ZWESVUSR and jobname ZWE1AG
```

When AT-TLS is off, no policy matches, or the matching policy is not enabled:

```text
No AT-TLS rule identified on 192.0.2.10:7554 for user ZWESVUSR and jobname ZWE1AG
```

The actual address, port, user, and jobname reflect the invocation and environment.

## Exit codes

| Code | Meaning |
|---|---|
| `0` | The queried socket reports `TTLS_POL_ENABLED` or `TTLS_POL_APPLCNTRL`, or help was printed. |
| `4` | The queried socket reports `TTLS_POL_OFF`, `TTLS_POL_NO_POLICY`, or `TTLS_POL_NOT_ENABLED`. |
| `8` | The test write failed, or the test read failed or returned no data. This can include TLS errors. |
| `12` | Argument/address validation, bind, connect, poll, accept, or AT-TLS query failed, or the query returned an unknown policy state. |

The built-in help currently says that other errors return 8. The implementation
also returns **12** for the failures listed above; scripts should handle it.

## Interpreting the result

Use the intended user, jobname, local address, and port to exercise the relevant
policy selectors. Since both endpoints are local, a policy that depends on a
remote peer address or a different client identity may match differently in
production. Selecting outbound mode changes the socket queried; the utility
still creates a local server and connects to it.

Success identifies an enabled or application-controlled policy after the test
exchange. The utility does not inspect the negotiated cipher, peer certificate,
or TLS connection state, and does not request that application-controlled TLS
be started. A successful result alone is not proof that the connection is
encrypted or that a production TLS handshake will succeed.

If bind fails, check port availability and TCP/IP permissions. If the data
exchange fails, review the printed socket diagnostics and the AT-TLS setup for
both ends. For query failures, inspect the printed `bpxrc` and `bpxrsn` values.

## Implementation references

- [Source](../c/attlsTest.c)
- [Build script](../build/build_attls_test.sh)
