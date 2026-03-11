/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.

  ----------------------------------------------------------------------------
  tn3270detect.c

  Detects whether a TCP endpoint is a TN3270 Telnet server as defined by
  RFC 1041 (Telnet 3270 Regime Option) and the supporting RFCs:
    - RFC 854  - Telnet Protocol Specification
    - RFC 856  - Telnet Binary Transmission (option 0)
    - RFC 885  - Telnet End Of Record Option (option 25)
    - RFC 1091 - Telnet Terminal-Type Option (option 24)
    - RFC 1041 - Telnet 3270 Regime Option  (option 29)

  Detection strategy:
    1. Open a TCP connection to the target host:port (IPv4 or IPv6).
    2. Send initial Telnet DO/WILL negotiations for all TN3270 relevant
       options (BINARY, EOR, TERMINAL-TYPE, 3270-REGIME).
    3. Read server responses and record which options the server accepts
       (WILL) or rejects (WONT / DONT).
    4. A server is considered a TN3270 server when:
         - RFC 1041 mode : server sends IAC WILL 3270-REGIME
         - Classic  mode : server sends WILL BINARY *and* WILL EOR
                           (the prerequisite pair for TN3270 data streams)
    5. The program also handles inbound IAC SB (sub-negotiation) frames so
       that the server is not left waiting for a response before sending its
       own capability advertisements.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zowetypes.h"
#include "bpxnet.h"
#include "tls.h"

/* -------------------------------------------------------------------------
 * Telnet protocol constants (RFC 854)
 * ---------------------------------------------------------------------- */
#define TELNET_IAC    255  /* Interpret As Command                         */
#define TELNET_DONT   254  /* You must stop using this option              */
#define TELNET_DO     253  /* Please use this option                       */
#define TELNET_WONT   252  /* I will not use this option                   */
#define TELNET_WILL   251  /* I will use this option                       */
#define TELNET_SB     250  /* Subnegotiation begin                         */
#define TELNET_SE     240  /* Subnegotiation end                           */
#define TELNET_EOR_CMD 239 /* End-of-Record command (RFC 885)              */
#define TELNET_NOP    241  /* No-operation                                 */
#define TELNET_DM     242  /* Data Mark                                    */
#define TELNET_BRK    243  /* Break                                        */
#define TELNET_IP     244  /* Interrupt Process                            */
#define TELNET_AO     245  /* Abort Output                                 */
#define TELNET_AYT    246  /* Are You There                                */
#define TELNET_EC     247  /* Erase Character                              */
#define TELNET_EL     248  /* Erase Line                                   */
#define TELNET_GA     249  /* Go Ahead                                     */

/* -------------------------------------------------------------------------
 * Telnet option codes
 * ---------------------------------------------------------------------- */
#define OPT_BINARY         0   /* RFC 856  – Binary Transmission            */
#define OPT_ECHO           1   /* RFC 857  – Echo                           */
#define OPT_SGA            3   /* RFC 858  – Suppress Go Ahead              */
#define OPT_TERMINAL_TYPE 24   /* RFC 1091 – Terminal-Type                  */
#define OPT_EOR           25   /* RFC 885  – End Of Record                  */
#define OPT_3270_REGIME   29   /* RFC 1041 – 3270 Regime                    */

/* Sub-negotiation IS/ARE codes (RFC 1041) */
#define SB_IS   0
#define SB_ARE  1

/* -------------------------------------------------------------------------
 * Programme-wide result flags
 * ---------------------------------------------------------------------- */
#define DETECT_NONE         0x00
#define DETECT_3270_REGIME  0x01   /* Server sent WILL 3270-REGIME (RFC 1041) */
#define DETECT_BINARY       0x02   /* Server sent WILL BINARY                 */
#define DETECT_EOR          0x04   /* Server sent WILL EOR                    */
#define DETECT_TERMTYPE     0x08   /* Server sent WILL TERMINAL-TYPE          */
#define DETECT_SB_REGIME    0x10   /* Server sent SB 3270-REGIME ARE ...      */

/* A server is a TN3270 server when it presents RFC 1041 *or* the classic
   BINARY+EOR combination (which together enable binary TN3270 data streams). */
#define IS_TN3270(flags) \
    (((flags) & DETECT_3270_REGIME) || \
     (((flags) & DETECT_BINARY) && ((flags) & DETECT_EOR)))

/* -------------------------------------------------------------------------
 * I/O buffer
 * ---------------------------------------------------------------------- */
#define RECV_BUFSIZE  4096
#define SEND_BUFSIZE   256
#define MAX_REGIME_LIST 512

/* -------------------------------------------------------------------------
 * Connection context: plain TCP socket plus optional TLS overlay.
 * Exactly one path is active at runtime: when tlsSock is non-NULL the
 * TLS path is used; otherwise the plain socket path is used.
 * ---------------------------------------------------------------------- */
typedef struct {
    Socket    *sock;    /* Underlying TCP socket (always valid)  */
    TlsSocket *tlsSock; /* NULL when the connection is plain TCP */
} Connection;

/* -------------------------------------------------------------------------
 * Utility: send a buffer, retrying on short writes
 * ---------------------------------------------------------------------- */
static int send_all(Connection *connection, const unsigned char *buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = 0;
        if (connection->tlsSock != NULL) {
            int out = 0;
            int rc = tlsWrite(connection->tlsSock, (const char *)(buf + sent), len - sent, &out);
            if (rc != 0 || out <= 0) {
                return -1;
            }
            n = out;
        } else {
            int rc = 0, rsn = 0;
            n = socketWrite(connection->sock, (const char *)(buf + sent), len - sent, &rc, &rsn);
            if (n <= 0) {
                return -1;
            }
        }
        sent += n;
    }
    return sent;
}

/* -------------------------------------------------------------------------
 * Build and send the initial TN3270 negotiation burst.
 *
 * We send:
 *   IAC WILL 3270-REGIME  – we are willing to negotiate 3270 regimes
 *   IAC DO   3270-REGIME  – please send your 3270 regime list
 *   IAC DO   BINARY       – request binary transmission
 *   IAC WILL BINARY       – we support binary transmission
 *   IAC DO   EOR          – request end-of-record
 *   IAC WILL EOR          – we support end-of-record
 *   IAC DO   TERMINAL-TYPE – request terminal type sub-negotiation
 * ---------------------------------------------------------------------- */
static int send_tn3270_probe(Connection *connection) {
    unsigned char probe[] = {
        /* RFC 1041 – 3270 Regime */
        TELNET_IAC, TELNET_WILL, OPT_3270_REGIME,
        TELNET_IAC, TELNET_DO,   OPT_3270_REGIME,
        /* RFC 856 – Binary */
        TELNET_IAC, TELNET_DO,   OPT_BINARY,
        TELNET_IAC, TELNET_WILL, OPT_BINARY,
        /* RFC 885 – EOR */
        TELNET_IAC, TELNET_DO,   OPT_EOR,
        TELNET_IAC, TELNET_WILL, OPT_EOR,
        /* RFC 1091 – Terminal-Type */
        TELNET_IAC, TELNET_DO,   OPT_TERMINAL_TYPE,
    };
    return send_all(connection, probe, (int)sizeof(probe));
}

/* -------------------------------------------------------------------------
 * Respond to a server SB 3270-REGIME ARE <list> IAC SE with
 *   IAC SB 3270-REGIME IS <first-regime> IAC SE
 * choosing the first regime from the list (preferred by RFC 1041 §6).
 * An empty list causes us to reply with an empty REGIME (NVT ASCII mode).
 * ---------------------------------------------------------------------- */
static int respond_to_regime_are(Connection *connection,
                                  const unsigned char *regime_list,
                                  int regime_list_len) {
    /* Pick the first terminal type name from the space-separated list. */
    char first[MAX_REGIME_LIST];
    int  first_len = 0;

    first[0] = '\0';
    if (regime_list_len > 0) {
        /* Copy until space or end. */
        for (int i = 0; i < regime_list_len && i < (int)(sizeof(first) - 1); i++) {
            if (regime_list[i] == ' ') {
                break;
            }
            first[first_len++] = (char)regime_list[i];
        }
        first[first_len] = '\0';
    }

    /* Build: IAC SB 3270-REGIME IS <name> IAC SE */
    unsigned char resp[MAX_REGIME_LIST + 8];
    int pos = 0;
    resp[pos++] = TELNET_IAC;
    resp[pos++] = TELNET_SB;
    resp[pos++] = OPT_3270_REGIME;
    resp[pos++] = SB_IS;
    for (int i = 0; i < first_len; i++) {
        unsigned char c = (unsigned char)first[i];
        resp[pos++] = c;
        if (c == TELNET_IAC) {
            resp[pos++] = TELNET_IAC; /* escape embedded IAC */
        }
    }
    resp[pos++] = TELNET_IAC;
    resp[pos++] = TELNET_SE;

    return send_all(connection, resp, pos);
}

/* -------------------------------------------------------------------------
 * Respond to a server TERMINAL-TYPE subneg SEND with
 *   IAC SB TERMINAL-TYPE IS "IBM-3279-2-E" IAC SE
 * ---------------------------------------------------------------------- */
static int respond_to_termtype_send(Connection *connection) {
    const char *tname = "IBM-3279-2-E";
    unsigned char resp[64];
    int pos = 0;
    resp[pos++] = TELNET_IAC;
    resp[pos++] = TELNET_SB;
    resp[pos++] = OPT_TERMINAL_TYPE;
    resp[pos++] = SB_IS;  /* code 0 = IS */
    for (int i = 0; tname[i]; i++) {
        resp[pos++] = (unsigned char)tname[i];
    }
    resp[pos++] = TELNET_IAC;
    resp[pos++] = TELNET_SE;
    return send_all(connection, resp, pos);
}

/* -------------------------------------------------------------------------
 * Parse a block of received Telnet data.
 * Returns a bitmask of DETECT_* flags indicating what was found.
 * Sends appropriate responses back on sock.
 * ---------------------------------------------------------------------- */
static unsigned int parse_telnet_data(Connection *connection,
                                       const unsigned char *buf,
                                       int len,
                                       int verbose) {
    unsigned int flags = DETECT_NONE;
    int i = 0;

    while (i < len) {
        if (buf[i] != TELNET_IAC) {
            /* Plain data byte – skip. */
            i++;
            continue;
        }

        /* IAC byte found */
        i++;
        if (i >= len) {
            break; /* truncated – ignore */
        }

        unsigned char cmd = buf[i++];

        switch (cmd) {
        case TELNET_IAC:
            /* Escaped IAC (data 0xFF) – ignore in probe context */
            break;

        case TELNET_NOP:
        case TELNET_DM:
        case TELNET_BRK:
        case TELNET_IP:
        case TELNET_AO:
        case TELNET_AYT:
        case TELNET_EC:
        case TELNET_EL:
        case TELNET_GA:
        case TELNET_EOR_CMD:
            /* Single-byte commands with no option byte */
            break;

        case TELNET_WILL:
        case TELNET_WONT:
        case TELNET_DO:
        case TELNET_DONT: {
            if (i >= len) break;
            unsigned char opt = buf[i++];
            const char *cmd_str = (cmd == TELNET_WILL) ? "WILL" :
                                   (cmd == TELNET_WONT) ? "WONT" :
                                   (cmd == TELNET_DO)   ? "DO"   : "DONT";
            if (verbose) {
                printf("  [telnet] IAC %s %d (0x%02X)\n", cmd_str, (int)opt, (int)opt);
            }

            if (cmd == TELNET_WILL) {
                switch (opt) {
                case OPT_3270_REGIME: flags |= DETECT_3270_REGIME; break;
                case OPT_BINARY:      flags |= DETECT_BINARY;      break;
                case OPT_EOR:         flags |= DETECT_EOR;         break;
                case OPT_TERMINAL_TYPE: flags |= DETECT_TERMTYPE;  break;
                default: break;
                }
            }

            /* If the server asks us to DO an option we already announced
               WILL for, acknowledge silently (no duplicate needed).
               If the server sends DONT for something, we reply WONT. */
            if (cmd == TELNET_DONT) {
                unsigned char wont[] = { TELNET_IAC, TELNET_WONT, opt };
                send_all(connection, wont, 3);
            }
            break;
        }

        case TELNET_SB: {
            /* Consume the subnegotiation up to IAC SE */
            if (i >= len) break;
            unsigned char sb_opt = buf[i++];

            /* Collect SB payload into a local buffer */
            unsigned char sb_data[MAX_REGIME_LIST];
            int sb_len = 0;
            while (i < len - 1) {
                if (buf[i] == TELNET_IAC && buf[i + 1] == TELNET_SE) {
                    i += 2; /* consume IAC SE */
                    break;
                }
                if (buf[i] == TELNET_IAC) {
                    i++; /* skip escape prefix */
                    if (i < len) {
                        if (sb_len < (int)sizeof(sb_data) - 1) {
                            sb_data[sb_len++] = buf[i++];
                        } else {
                            i++;
                        }
                    }
                } else {
                    if (sb_len < (int)sizeof(sb_data) - 1) {
                        sb_data[sb_len++] = buf[i++];
                    } else {
                        i++;
                    }
                }
            }
            sb_data[sb_len] = '\0';

            if (verbose) {
                printf("  [telnet] IAC SB opt=%d len=%d\n", (int)sb_opt, sb_len);
            }

            if (sb_opt == OPT_3270_REGIME && sb_len >= 1) {
                unsigned char subcode = sb_data[0];
                if (subcode == SB_ARE) {
                    /* Server is offering a list of supported 3270 regimes */
                    flags |= DETECT_SB_REGIME;
                    int payload_len = sb_len - 1;
                    if (verbose) {
                        printf("  [telnet] SB 3270-REGIME ARE '%.*s'\n",
                               payload_len, (char *)(sb_data + 1));
                    }
                    /* Reply with IS choosing the first regime */
                    respond_to_regime_are(connection, sb_data + 1, payload_len);
                }
            } else if (sb_opt == OPT_TERMINAL_TYPE && sb_len >= 1) {
                /* code 1 = SEND */
                if (sb_data[0] == 1) {
                    if (verbose) {
                        printf("  [telnet] SB TERMINAL-TYPE SEND – replying with IBM-3279-2-E\n");
                    }
                    respond_to_termtype_send(connection);
                }
            }
            break;
        }

        default:
            /* Unknown command – skip */
            break;
        }
    }
    return flags;
}

/* -------------------------------------------------------------------------
 * Print a hex + ASCII dump of raw bytes (verbose mode)
 * ---------------------------------------------------------------------- */
static void hex_dump(const unsigned char *buf, int len) {
    for (int i = 0; i < len; i += 16) {
        printf("    %04X  ", i);
        for (int j = i; j < i + 16; j++) {
            if (j < len) printf("%02X ", buf[j]);
            else         printf("   ");
            if (j == i + 7) printf(" ");
        }
        printf(" |");
        for (int j = i; j < i + 16 && j < len; j++) {
            unsigned char c = buf[j];
            printf("%c", (c >= 0x20 && c < 0x7F) ? c : '.');
        }
        printf("|\n");
    }
}

/* -------------------------------------------------------------------------
 * Usage / help
 * ---------------------------------------------------------------------- */
static void print_usage(const char *prog) {
    printf(
        "tn3270detect - Detect whether a TCP endpoint is a TN3270 Telnet server\n"
        "               as defined by RFC 1041 (Telnet 3270 Regime Option).\n"
        "\n"
        "Usage:\n"
        "  %s --host <hostname|IPv4|IPv6> --port <port> [options]\n"
        "\n"
        "Required:\n"
        "  --host <value>    Hostname, IPv4 address, or IPv6 address of the server.\n"
        "                    IPv6 addresses may optionally be enclosed in brackets,\n"
        "                    e.g. [::1] or ::1.\n"
        "  --port <value>    TCP port number (1-65535). The standard TN3270 port\n"
        "                    is 23 (Telnet) and TN3270E commonly uses 992 (TLS).\n"
        "\n"
        "Options:\n"
        "  --tls             Establish a TLS-secured connection using IBM GSK.\n"
        "  --keyring <value> SAF key ring (user/ring or ring), key database (.kdb),\n"
        "                    PKCS#11 token (*TOKEN*/name), or PKCS#12 file (.p12).\n"
        "                    Required when --tls is specified.\n"
        "  --label <value>   Certificate label to use for the TLS handshake.\n"
        "                    Optional; uses the default certificate when omitted.\n"
        "  --timeout <sec>   Read timeout in seconds (default: 5).\n"
        "  --verbose         Print detailed Telnet negotiation trace.\n"
        "  -h, --help        Show this help message and exit.\n"
        "\n"
        "Exit codes:\n"
        "  0  Server is a TN3270 server (RFC 1041 or classic BINARY+EOR mode).\n"
        "  1  Server is not a TN3270 server, or the negotiation was inconclusive.\n"
        "  2  Connection failed or other fatal error.\n"
        "\n"
        "Examples:\n"
        "  %s --host mainframe.example.com --port 23\n"
        "  %s --host 192.168.1.10 --port 23 --verbose\n"
        "  %s --host ::1 --port 23 --timeout 10\n"
        "  %s --host mainframe.example.com --port 992 --tls --keyring IBMUSER/TN3270KR\n"
        "  %s --host mainframe.example.com --port 992 --tls --keyring /etc/zowe.kdb --label myLabel\n",
        prog, prog, prog, prog, prog, prog);
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */
int main(int argc, char **argv) {
    const char *host        = NULL;
    const char *portstr     = NULL;
    int         timeout_sec = 5;
    int         verbose     = 0;
    int         use_tls     = 0;
    const char *keyring     = NULL;
    const char *label       = NULL;

    /* ------------------------------------------------------------------ */
    /* Parse command-line arguments                                        */
    /* ------------------------------------------------------------------ */
    if (argc < 2) {
        print_usage(argv[0]);
        return 2;
    }

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--host") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --host requires an argument.\n");
                return 2;
            }
            host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --port requires an argument.\n");
                return 2;
            }
            portstr = argv[++i];
        } else if (strcmp(argv[i], "--timeout") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --timeout requires an argument.\n");
                return 2;
            }
            timeout_sec = atoi(argv[++i]);
            if (timeout_sec <= 0) {
                fprintf(stderr, "Error: --timeout must be a positive integer.\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--tls") == 0) {
            use_tls = 1;
        } else if (strcmp(argv[i], "--keyring") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --keyring requires an argument.\n");
                return 2;
            }
            keyring = argv[++i];
        } else if (strcmp(argv[i], "--label") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --label requires an argument.\n");
                return 2;
            }
            label = argv[++i];
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else {
            fprintf(stderr, "Error: Unknown argument '%s'. Use --help for usage.\n", argv[i]);
            return 2;
        }
    }

    if (!host) {
        fprintf(stderr, "Error: --host is required.\n");
        print_usage(argv[0]);
        return 2;
    }
    if (!portstr) {
        fprintf(stderr, "Error: --port is required.\n");
        print_usage(argv[0]);
        return 2;
    }

    if (use_tls && keyring == NULL) {
        fprintf(stderr, "Error: --keyring is required when --tls is specified.\n");
        return 2;
    }

    /* Strip optional surrounding brackets from IPv6 addresses, e.g. [::1] */
    char host_buf[256];
    strncpy(host_buf, host, sizeof(host_buf) - 1);
    host_buf[sizeof(host_buf) - 1] = '\0';
    if (host_buf[0] == '[') {
        size_t hl = strlen(host_buf);
        if (hl >= 2 && host_buf[hl - 1] == ']') {
            memmove(host_buf, host_buf + 1, hl - 2);
            host_buf[hl - 2] = '\0';
        }
    }

    /* Validate port */
    int port = atoi(portstr);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Error: port must be between 1 and 65535, got '%s'.\n", portstr);
        return 2;
    }

    /* ------------------------------------------------------------------ */
    /* Resolve hostname supporting both IPv4 and IPv6 via getAddressInfoList */
    /* ------------------------------------------------------------------ */
    int gai_rc = 0, gai_rsn = 0;
    AddrInfoList addrList = getAddressInfoList(host_buf, portstr, NULL,
                                               &gai_rc, &gai_rsn);
    if (addrList == NULL) {
        fprintf(stderr, "Error: cannot resolve '%s' (rc=%d rsn=0x%08X)\n",
                host_buf, gai_rc, gai_rsn);
        return 2;
    }

    void       *addrInfo      = NULL;
    SocketAddress *socketAddress = NULL;
    if (AddrInfoList_getAtIndex(addrList, 0, &addrInfo) != ANSI_OK ||
        AddrInfo_convert(addrInfo, &socketAddress, NULL) != ANSI_OK ||
        socketAddress == NULL) {
        fprintf(stderr, "Error: failed to convert address for '%s'\n", host_buf);
        freeAddressInfoList(addrList);
        return 2;
    }

    /* Print resolved address */
    char addr_str[64] = {0};
    int  addr_str_len = (int)sizeof(addr_str);
    if (SocketAddress_toString(socketAddress, addr_str, &addr_str_len) == ANSI_OK) {
        printf("Connecting to %s (%s) port %d ...\n", host_buf, addr_str, port);
    } else {
        printf("Connecting to %s port %d ...\n", host_buf, port);
    }

    /* ------------------------------------------------------------------ */
    /* Connect with timeout via tcpClient2                                 */
    /* ------------------------------------------------------------------ */
    int connect_rc = 0, connect_rsn = 0;
    Socket *sock = tcpClient2(socketAddress, timeout_sec * 1000,
                              &connect_rc, &connect_rsn);
    freeSocketAddr(socketAddress);
    freeAddressInfoList(addrList);
    if (sock == NULL) {
        fprintf(stderr, "Error: connect() failed (rc=%d rsn=0x%08X)\n",
                connect_rc, connect_rsn);
        return 2;
    }

    /* ------------------------------------------------------------------ */
    /* Optionally upgrade the plain TCP socket to TLS via IBM GSK          */
    /* ------------------------------------------------------------------ */
    TlsEnvironment *tlsEnv  = NULL;
    TlsSocket      *tlsSock = NULL;

    if (use_tls) {
        printf("Performing TLS handshake (keyring: %s%s%s) ...\n",
               keyring,
               label ? ", label: " : "",
               label ? label      : "");

        TlsSettings settings;
        memset(&settings, 0, sizeof(settings));
        settings.keyring = (char *)keyring;
        settings.label   = (char *)label;  /* NULL is acceptable */

        int tls_rc = tlsInit(&tlsEnv, &settings);
        if (tls_rc != 0) {
            fprintf(stderr, "Error: tlsInit() failed: %s (rc=%d)\n",
                    tlsStrError(tls_rc), tls_rc);
            int cl_rc = 0, cl_rsn = 0;
            socketClose(sock, &cl_rc, &cl_rsn);
            return 2;
        }

        tls_rc = tlsSocketInit(tlsEnv, &tlsSock, sock->sd, 0 /*isServer=false*/);
        if (tls_rc != 0) {
            fprintf(stderr, "Error: TLS handshake failed: %s (rc=%d)\n",
                    tlsStrError(tls_rc), tls_rc);
            tlsDestroy(tlsEnv);
            int cl_rc = 0, cl_rsn = 0;
            socketClose(sock, &cl_rc, &cl_rsn);
            return 2;
        }

        printf("TLS handshake successful.\n");
    }

    printf("Connected%s. Sending TN3270 Telnet probe (RFC 1041 + RFC 854/856/885/1091)...\n",
           use_tls ? " (TLS)" : "");

    /* Combine the plain socket and optional TLS socket into one context. */
    Connection connection;
    connection.sock    = sock;
    connection.tlsSock = tlsSock;

    /* ------------------------------------------------------------------ */
    /* Send TN3270 negotiation probe                                       */
    /* ------------------------------------------------------------------ */
    if (send_tn3270_probe(&connection) < 0) {
        fprintf(stderr, "Error: failed to send probe.\n");
        if (connection.tlsSock != NULL) { tlsSocketClose(connection.tlsSock); tlsDestroy(tlsEnv); }
        int cl_rc = 0, cl_rsn = 0;
        socketClose(connection.sock, &cl_rc, &cl_rsn);
        return 2;
    }

    /* ------------------------------------------------------------------ */
    /* Read server responses for up to timeout_sec seconds                */
    /* Use tcpStatus to poll for readability, then socketRead to receive. */
    /* ------------------------------------------------------------------ */
    unsigned int detect_flags = DETECT_NONE;
    unsigned char recv_buf[RECV_BUFSIZE];
    int  total_received = 0;
    int  rounds         = 0;
    int  max_rounds     = 8;   /* guard against servers that keep sending    */

    while (rounds < max_rounds) {
        int n = 0;

        if (connection.tlsSock != NULL) {
            /* TLS: gsk_secure_socket_read handles its own blocking via
               the I/O callbacks registered in tlsSocketInit.            */
            int out = 0;
            int tls_rc = tlsRead(connection.tlsSock, (char *)recv_buf,
                                 (int)(sizeof(recv_buf) - 1), &out);
            if (tls_rc != 0) {
                if (verbose) printf("[recv] tlsRead() error: %s (rc=%d)\n",
                                    tlsStrError(tls_rc), tls_rc);
                break;
            }
            n = out;
            if (n == 0) {
                if (verbose) printf("[recv] server closed TLS connection.\n");
                break;
            }
        } else {
            int st_rc = 0, st_rsn = 0;
            int status = tcpStatus(connection.sock, timeout_sec * 1000, 0 /* checkWrite */,
                                   &st_rc, &st_rsn);
            if (status & SD_STATUS_TIMEOUT) {
                if (verbose) printf("[recv] tcpStatus() timed out after %d s\n", timeout_sec);
                break;
            }
            if ((status & SD_STATUS_FAILED) || !(status & SD_STATUS_RD_RDY)) {
                if (verbose) printf("[recv] tcpStatus() error (status=0x%X rc=%d rsn=0x%X)\n",
                                    status, st_rc, st_rsn);
                break;
            }

            int rd_rc = 0, rd_rsn = 0;
            n = socketRead(connection.sock, (char *)recv_buf, sizeof(recv_buf) - 1,
                           &rd_rc, &rd_rsn);
            if (n <= 0) {
                if (n == 0) {
                    if (verbose) printf("[recv] server closed connection.\n");
                } else {
                    if (verbose) printf("[recv] socketRead() error (rc=%d rsn=0x%X)\n",
                                        rd_rc, rd_rsn);
                }
                break;
            }
        }

        total_received += n;
        if (verbose) {
            printf("[recv] received %d bytes (total %d):\n", n, total_received);
            hex_dump(recv_buf, n);
        }

        detect_flags |= parse_telnet_data(&connection, recv_buf, n, verbose);
        rounds++;

        /* Once we have enough evidence, stop waiting */
        if (detect_flags & DETECT_3270_REGIME) break;
        if ((detect_flags & DETECT_BINARY) && (detect_flags & DETECT_EOR)) break;
    }

    if (connection.tlsSock != NULL) {
        tlsSocketClose(connection.tlsSock);
        tlsDestroy(tlsEnv);
    }
    int cl_rc = 0, cl_rsn = 0;
    socketClose(connection.sock, &cl_rc, &cl_rsn);

    /* ------------------------------------------------------------------ */
    /* Report results                                                      */
    /* ------------------------------------------------------------------ */
    printf("\n--- TN3270 Detection Results ---\n");
    printf("  Bytes received             : %d\n", total_received);
    printf("  IAC WILL 3270-REGIME       : %s  (RFC 1041)\n",
           (detect_flags & DETECT_3270_REGIME) ? "YES" : "no");
    printf("  IAC SB  3270-REGIME ARE    : %s  (RFC 1041 subneg)\n",
           (detect_flags & DETECT_SB_REGIME)   ? "YES" : "no");
    printf("  IAC WILL BINARY            : %s  (RFC 856)\n",
           (detect_flags & DETECT_BINARY)      ? "YES" : "no");
    printf("  IAC WILL EOR               : %s  (RFC 885)\n",
           (detect_flags & DETECT_EOR)         ? "YES" : "no");
    printf("  IAC WILL TERMINAL-TYPE     : %s  (RFC 1091)\n",
           (detect_flags & DETECT_TERMTYPE)    ? "YES" : "no");
    printf("\n");

    if (IS_TN3270(detect_flags)) {
        /* Determine flavour */
        if ((detect_flags & DETECT_3270_REGIME) || (detect_flags & DETECT_SB_REGIME)) {
            printf("RESULT: TN3270 server detected (RFC 1041 – 3270 Regime Option).\n");
        } else {
            printf("RESULT: TN3270 server detected (classic mode – BINARY + EOR negotiated).\n");
        }

        if (detect_flags & DETECT_TERMTYPE) {
            printf("        Terminal-type negotiation also supported.\n");
        }
        return 0;
    }

    if (total_received == 0) {
        printf("RESULT: No Telnet data received – server does not speak TN3270 (or is not a Telnet server).\n");
    } else {
        printf("RESULT: Server responded but did not accept TN3270 negotiation.\n");
        printf("        This is not a TN3270 Telnet server.\n");
    }

    return 1;
}
