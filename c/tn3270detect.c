/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
  tn3270detect.c

  Detects whether a TCP endpoint is a TN3270 Telnet server as defined by
  RFC 1041 (Telnet 3270 Regime Option) and the supporting RFCs:
  - RFC 854  - Telnet Protocol Specification
  - RFC 856  - Telnet Binary Transmission (option 0)
  - RFC 885  - Telnet End Of Record Option (option 25)
  - RFC 1091 - Telnet Terminal-Type Option (option 24)
  - RFC 1041 - Telnet 3270 Regime Option  (option 29)
  - RFC 2355 - TN3270 Enhancements        (option 40)

  Detection strategy:
  1. Open a TCP connection to the target host:port (IPv4 or IPv6).
  2. Send initial Telnet DO/WILL negotiations for all TN3270 relevant
  options (TN3270E, BINARY, EOR, TERMINAL-TYPE, 3270-REGIME).
  3. Read server responses and record which options the server accepts
  (WILL) or rejects (WONT / DONT).
  4. A server is considered a TN3270 server when:
  - RFC 2355 mode : server sends IAC WILL TN3270E *or* IAC DO TN3270E
  (either signals TN3270E agreement, then proceeds
  with DEVICE-TYPE and FUNCTIONS sub-negotiation)
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
#include "xlate.h"
#include "utils.h"
#include "telnet.h"
#include "tn3270.h"

/* -------------------------------------------------------------------------
 * I/O buffer
 * ---------------------------------------------------------------------- */
#define RECV_BUFSIZE  4096
#define SEND_BUFSIZE   256
#define MAX_REGIME_LIST 512
#define TIMEOUT_SEC     5

/* -------------------------------------------------------------------------
 * TN3270E device-type name sent in DEVICE-TYPE REQUEST (RFC 2355 §7.1).
 * This literal is EBCDIC on z/OS; it must be passed through e2a() before
 * being placed in a Telnet frame (NVT ASCII on the wire).
 * ---------------------------------------------------------------------- */
#define TN3270E_DEVICE_TYPE_NAME  "IBM-3278-2"

/* -------------------------------------------------------------------------
 * Connection context: plain TCP socket plus optional TLS overlay.
 * Exactly one path is active at runtime: when tlsSock is non-NULL the
 * TLS path is used; otherwise the plain socket path is used.
 * Also tracks TN3270E device-type negotiation state.
 * ---------------------------------------------------------------------- */
typedef struct {
  Socket    *sock;                /* Underlying TCP socket (always valid)  */
  TlsSocket *tlsSock;             /* NULL when the connection is plain TCP */
} Connection;

/* -------------------------------------------------------------------------
 * Utility: send a buffer, retrying on short writes
 * ---------------------------------------------------------------------- */
static int sendAll(Connection *connection, const unsigned char *buf, int len) {
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
static int sendTn3270Probe(Connection *connection) {
  unsigned char probe[] = {
    /* RFC 2355 – TN3270E (advertise both sides; a server may confirm
       with WILL TN3270E *or* DO TN3270E before DEVICE-TYPE negotiation) */
    TELNET_IAC, TELNET_WILL, OPT_TN3270E,
    TELNET_IAC, TELNET_DO,   OPT_TN3270E,
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
  return sendAll(connection, probe, (int)sizeof(probe));
}

/* -------------------------------------------------------------------------
 * Respond to a server SB 3270-REGIME ARE <list> IAC SE with
 *   IAC SB 3270-REGIME IS <first-regime> IAC SE
 * choosing the first regime from the list (preferred by RFC 1041 §6).
 * An empty list causes us to reply with an empty REGIME (NVT ASCII mode).
 * ---------------------------------------------------------------------- */
static int respondToRegimeAre(Connection *connection,
                              const unsigned char *regimeList,
                              int regimeListLen) {
  /* Pick the first terminal type name from the space-separated list. */
  char first[MAX_REGIME_LIST];
  int  firstLen = 0;

  first[0] = '\0';
  if (regimeListLen > 0) {
    /* Copy until space or end. */
    for (int i = 0; i < regimeListLen && i < (int)(sizeof(first) - 1); i++) {
      if (regimeList[i] == ' ') {
        break;
      }
      first[firstLen++] = (char)regimeList[i];
    }
    first[firstLen] = '\0';
  }

  /* Build: IAC SB 3270-REGIME IS <name> IAC SE */
  unsigned char resp[MAX_REGIME_LIST + 8];
  int pos = 0;
  resp[pos++] = TELNET_IAC;
  resp[pos++] = TELNET_SB;
  resp[pos++] = OPT_3270_REGIME;
  resp[pos++] = SB_IS;
  for (int i = 0; i < firstLen; i++) {
    unsigned char c = (unsigned char)first[i];
    resp[pos++] = c;
    if (c == TELNET_IAC) {
      resp[pos++] = TELNET_IAC; /* escape embedded IAC */
    }
  }
  resp[pos++] = TELNET_IAC;
  resp[pos++] = TELNET_SE;

  return sendAll(connection, resp, pos);
}

/* -------------------------------------------------------------------------
 * Respond to a server TERMINAL-TYPE subneg SEND with
 *   IAC SB TERMINAL-TYPE IS "IBM-3279-2-E" IAC SE
 * ---------------------------------------------------------------------- */
static int respondToTermtypeSend(Connection *connection) {
  /* Copy to a local buffer and convert EBCDIC→ASCII before sending;
     the TERMINAL-TYPE IS string must be NVT ASCII (RFC 1091 §2). */
  const char *tname = "IBM-3279-2-E";
  char tnameAscii[16];
  int tnameLen = 0;
  while (tname[tnameLen] && tnameLen < (int)(sizeof(tnameAscii) - 1)) {
    tnameAscii[tnameLen] = tname[tnameLen];
    tnameLen++;
  }
  tnameAscii[tnameLen] = '\0';
  e2a(tnameAscii, tnameLen);
  unsigned char resp[64];
  int pos = 0;
  resp[pos++] = TELNET_IAC;
  resp[pos++] = TELNET_SB;
  resp[pos++] = OPT_TERMINAL_TYPE;
  resp[pos++] = SB_IS;  /* code 0 = IS */
  for (int i = 0; tnameAscii[i]; i++) {
    resp[pos++] = (unsigned char)tnameAscii[i];
  }
  resp[pos++] = TELNET_IAC;
  resp[pos++] = TELNET_SE;
  return sendAll(connection, resp, pos);
}

/* -------------------------------------------------------------------------
 * Respond to a server SB TN3270E SEND DEVICE-TYPE with
 *   IAC SB TN3270E DEVICE-TYPE REQUEST IBM-3278-2 IAC SE
 * (RFC 2355 §7.1 – DEVICE-TYPE negotiation)
 * ---------------------------------------------------------------------- */
static int respondToTn3270eSendDeviceType(Connection *connection, int verbose) {
  if (verbose) {
    printf("  [tn3270e] Sending DEVICE-TYPE REQUEST %s\n", TN3270E_DEVICE_TYPE_NAME);
  }
  /* Copy to a local buffer and convert EBCDIC→ASCII before sending;
     device-type names are NVT ASCII on the wire (RFC 2355 §7.1). */
  char deviceTypeAscii[32];
  int dtLen = 0;
  const char *deviceType = TN3270E_DEVICE_TYPE_NAME;
  while (deviceType[dtLen] && dtLen < (int)(sizeof(deviceTypeAscii) - 1)) {
    deviceTypeAscii[dtLen] = deviceType[dtLen];
    dtLen++;
  }
  deviceTypeAscii[dtLen] = '\0';
  e2a(deviceTypeAscii, dtLen);
  unsigned char response[64];
  int position = 0;
  response[position++] = TELNET_IAC;
  response[position++] = TELNET_SB;
  response[position++] = OPT_TN3270E;
  response[position++] = TN3270E_DEVICE_TYPE;
  response[position++] = TN3270E_REQUEST;
  for (int i = 0; deviceTypeAscii[i]; i++) {
    response[position++] = (unsigned char)deviceTypeAscii[i];
  }
  response[position++] = TELNET_IAC;
  response[position++] = TELNET_SE;
  return sendAll(connection, response, position);
}

/* -------------------------------------------------------------------------
 * After receiving SB TN3270E DEVICE-TYPE IS, advance the negotiation by
 * sending a FUNCTIONS REQUEST with an empty function list, which requests
 * "basic TN3270E" (RFC 2355 §9 – Basic TN3270E).
 * ---------------------------------------------------------------------- */
static int respondToTn3270eDeviceTypeIs(Connection *connection) {
  /* Empty function list = basic TN3270E */
  unsigned char response[] = {
    TELNET_IAC, TELNET_SB,  OPT_TN3270E,
    TN3270E_FUNCTIONS, TN3270E_REQUEST,
    TELNET_IAC, TELNET_SE,
  };
  return sendAll(connection, response, (int)sizeof(response));
}

/* -------------------------------------------------------------------------
 * Parse a block of received Telnet data.
 * Returns a bitmask of DETECT_* flags indicating what was found.
 * Sends appropriate responses back on sock.
 * ---------------------------------------------------------------------- */
static unsigned int parseTelnetData(Connection *connection,
                                    const unsigned char *buf,
                                    int len,
                                    int verbose) {
  unsigned int flags = DETECT_NONE;
  int i = 0;

  while (i < len) {
    if (buf[i] != TELNET_IAC) {
      i++;
      continue; //Plain data byte – skip.
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
      const char *cmdStr = (cmd == TELNET_WILL) ? "WILL" :
        (cmd == TELNET_WONT) ? "WONT" :
        (cmd == TELNET_DO)   ? "DO"   : "DONT";
      if (verbose) {
        printf("  [telnet] IAC %s %d (0x%02X)\n", cmdStr, (int)opt, (int)opt);
      }

      if (cmd == TELNET_WILL) {
        switch (opt) {
        case OPT_TN3270E:       flags |= DETECT_TN3270E;    break;
        case OPT_3270_REGIME:   flags |= DETECT_3270_REGIME; break;
        case OPT_BINARY:        flags |= DETECT_BINARY;      break;
        case OPT_EOR:           flags |= DETECT_EOR;         break;
        case OPT_TERMINAL_TYPE: flags |= DETECT_TERMTYPE;    break;
        default: break;
        }
      }

      /* IAC DO TN3270E from the server means it accepted our
         IAC WILL TN3270E offer (RFC 2355 §6) – treat as TN3270E
         confirmation.  For any other DO, we either already sent
         WILL in the probe (acknowledged implicitly) or we re-send
         WILL now so the server knows we agree. */
      if (cmd == TELNET_DO) {
        switch (opt) {
        case OPT_TN3270E:
          flags |= DETECT_TN3270E;
          /* FALLTHROUGH */
        case OPT_3270_REGIME:
          if (opt == OPT_3270_REGIME) flags |= DETECT_3270_REGIME;
          /* FALLTHROUGH */
        default: {
          /* Echo WILL back for options we support so the server
             can proceed (covers the case where the server sends
             DO before our probe arrives, or initiates DO itself). */
          if (opt == OPT_TN3270E || opt == OPT_3270_REGIME ||
              opt == OPT_BINARY   || opt == OPT_EOR) {
            unsigned char willReply[] = { TELNET_IAC, TELNET_WILL, opt };
            sendAll(connection, willReply, 3);
          }
          break;
        }
        }
      }

      /* If the server sends DONT for something, we reply WONT. */
      if (cmd == TELNET_DONT) {
        unsigned char wont[] = { TELNET_IAC, TELNET_WONT, opt };
        sendAll(connection, wont, 3);
      }
      break;
    }

    case TELNET_SB: {
      /* Consume the subnegotiation up to IAC SE */
      if (i >= len) break;
      unsigned char sbOpt = buf[i++];

      /* Collect SB payload into a local buffer */
      unsigned char sbData[MAX_REGIME_LIST];
      int sbLen = 0;
      while (i < len - 1) {
        if (buf[i] == TELNET_IAC && buf[i + 1] == TELNET_SE) {
          i += 2; /* consume IAC SE */
          break;
        }
        if (buf[i] == TELNET_IAC) {
          i++; /* skip escape prefix */
          if (i < len) {
            if (sbLen < (int)sizeof(sbData) - 1) {
              sbData[sbLen++] = buf[i++];
            } else {
              i++;
            }
          }
        } else {
          if (sbLen < (int)sizeof(sbData) - 1) {
            sbData[sbLen++] = buf[i++];
          } else {
            i++;
          }
        }
      }
      sbData[sbLen] = '\0';

      if (verbose) {
        printf("  [telnet] IAC SB opt=%d len=%d\n", (int)sbOpt, sbLen);
      }

      if (sbOpt == OPT_3270_REGIME && sbLen >= 1) {
        unsigned char subcode = sbData[0];
        if (subcode == SB_ARE) {
          /* Server is offering a list of supported 3270 regimes */
          flags |= DETECT_SB_REGIME;
          int payloadLen = sbLen - 1;
          if (verbose) {
            printf("  [telnet] SB 3270-REGIME ARE '%.*s'\n",
                   payloadLen, (char *)(sbData + 1));
          }
          /* Reply with IS choosing the first regime */
          respondToRegimeAre(connection, sbData + 1, payloadLen);
        }
      } else if (sbOpt == OPT_TERMINAL_TYPE && sbLen >= 1) {
        /* code 1 = SEND */
        if (sbData[0] == 1) {
          if (verbose) {
            printf("  [telnet] SB TERMINAL-TYPE SEND – replying with IBM-3279-2-E\n");
          }
          respondToTermtypeSend(connection);
        }
      } else if (sbOpt == OPT_TN3270E && sbLen >= 2) {
        /* TN3270E sub-negotiation (RFC 2355 §7) */
        if (sbData[0] == TN3270E_SEND &&
            sbData[1] == TN3270E_DEVICE_TYPE) {
          /* Server requesting device-type; reply DEVICE-TYPE REQUEST */
          respondToTn3270eSendDeviceType(connection, verbose);
        } else if (sbData[0] == TN3270E_DEVICE_TYPE &&
                   sbData[1] == TN3270E_IS) {
          /* Server confirmed device type; send FUNCTIONS REQUEST */
          flags |= DETECT_TN3270E_DEVTYPE;
          if (verbose) {
            printf("  [telnet] SB TN3270E DEVICE-TYPE IS – sending FUNCTIONS REQUEST\n");
          }
          respondToTn3270eDeviceTypeIs(connection);
        } else if (sbData[0] == TN3270E_DEVICE_TYPE &&
                   sbData[1] == TN3270E_REJECT) {
          unsigned char reason = (sbLen >= 4) ? sbData[3] : 0xFF;
          if (verbose) {
            printf("  [telnet] SB TN3270E DEVICE-TYPE REJECT reason=0x%02X\n",
                   (int)reason);
          }
        } else if (sbData[0] == TN3270E_FUNCTIONS &&
                   sbData[1] == TN3270E_IS) {
          /* Server accepted functions list; TN3270E fully negotiated */
          flags |= DETECT_TN3270E_FUNCTIONS;
          if (verbose) {
            printf("  [telnet] SB TN3270E FUNCTIONS IS – TN3270E fully negotiated\n");
          }
        }
      }
      break;
    }

    default:
      break; // Unknown command – skip
    }
  }
  return flags;
}

/* -------------------------------------------------------------------------
 * Usage / help
 * ---------------------------------------------------------------------- */
static void printUsage(const char *prog) {
  printf(
         "tn3270detect - Detect whether a TCP endpoint is a TN3270 or TN3270E server\n"
         "               (RFC 2355 – TN3270E, RFC 1041 – 3270 Regime Option).\n"
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
         "  --keyring <value> SAF key ring (user/ring or ring) or PKCS#12 file (.p12).\n"
         "                    Required when --tls is specified.\n"
         "  --label <value>   Certificate label to use for the TLS handshake.\n"
         "                    Optional; uses the default certificate when omitted.\n"
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
         "  %s --host mainframe.example.com --port 992 --tls --keyring IBMUSER/TN3270KR\n",
         prog, prog, prog, prog, prog);
}

int main(int argc, char **argv) {
  const char *host        = NULL;
  const char *portStr     = NULL;
  int         verbose     = 0;
  int         useTls      = 0;
  const char *keyring     = NULL;
  const char *label       = NULL;

  if (argc < 2) {
    printUsage(argv[0]);
    return 2;
  }

  for (int i = 1; i < argc; i++) {
    if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
      printUsage(argv[0]);
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
      portStr = argv[++i];
    } else if (strcmp(argv[i], "--tls") == 0) {
      useTls = 1;
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
    printUsage(argv[0]);
    return 2;
  }
  if (!portStr) {
    fprintf(stderr, "Error: --port is required.\n");
    printUsage(argv[0]);
    return 2;
  }

  if (useTls && keyring == NULL) {
    fprintf(stderr, "Error: --keyring is required when --tls is specified.\n");
    return 2;
  }

  /* Strip optional surrounding brackets from IPv6 addresses, e.g. [::1] */
  char hostBuf[256];
  strncpy(hostBuf, host, sizeof(hostBuf) - 1);
  hostBuf[sizeof(hostBuf) - 1] = '\0';
  if (hostBuf[0] == '[') {
    size_t hl = strlen(hostBuf);
    if (hl >= 2 && hostBuf[hl - 1] == ']') {
      memmove(hostBuf, hostBuf + 1, hl - 2);
      hostBuf[hl - 2] = '\0';
    }
  }

  /* Validate port */
  int port = atoi(portStr);
  if (port <= 0 || port > 65535) {
    fprintf(stderr, "Error: port must be between 1 and 65535, got '%s'.\n", portStr);
    return 2;
  }

  /* ------------------------------------------------------------------ */
  /* Resolve hostname supporting both IPv4 and IPv6 via getAddressInfoList */
  /* ------------------------------------------------------------------ */
  int gaiRc = 0, gaiRsn = 0;
  AddrInfoList addrList = getAddressInfoList(hostBuf, portStr, NULL,
                                             &gaiRc, &gaiRsn);
  if (addrList == NULL) {
    fprintf(stderr, "Error: cannot resolve '%s' (rc=%d rsn=0x%08X)\n",
            hostBuf, gaiRc, gaiRsn);
    return 2;
  }

  void       *addrInfo      = NULL;
  SocketAddress *socketAddress = NULL;
  if (AddrInfoList_getAtIndex(addrList, 0, &addrInfo) != ANSI_OK ||
      AddrInfo_convert(addrInfo, &socketAddress, NULL) != ANSI_OK ||
      socketAddress == NULL) {
    fprintf(stderr, "Error: failed to convert address for '%s'\n", hostBuf);
    freeAddressInfoList(addrList);
    return 2;
  }

  /* Print resolved address */
  char addrStr[64] = {0};
  int  addrStrLen = (int)sizeof(addrStr);
  if (SocketAddress_toString(socketAddress, addrStr, &addrStrLen) == ANSI_OK) {
    printf("Connecting to %s (%s) port %d ...\n", hostBuf, addrStr, port);
  } else {
    printf("Connecting to %s port %d ...\n", hostBuf, port);
  }

  /* ------------------------------------------------------------------ */
  /* Connect with timeout via tcpClient2                                 */
  /* ------------------------------------------------------------------ */
  int connectRc = 0, connectRsn = 0;
  Socket *sock = tcpClient2(socketAddress, TIMEOUT_SEC * 1000,
                            &connectRc, &connectRsn);
  freeSocketAddr(socketAddress);
  freeAddressInfoList(addrList);
  if (sock == NULL) {
    fprintf(stderr, "Error: connect() failed (rc=%d rsn=0x%08X)\n",
            connectRc, connectRsn);
    return 2;
  }

  /* ------------------------------------------------------------------ */
  /* Optionally upgrade the plain TCP socket to TLS via IBM GSK          */
  /* ------------------------------------------------------------------ */
  TlsEnvironment *tlsEnv  = NULL;
  TlsSocket      *tlsSock = NULL;

  if (useTls) {
    printf("Performing TLS handshake (keyring: %s%s%s) ...\n",
           keyring,
           label ? ", label: " : "",
           label ? label      : "");

    TlsSettings settings;
    memset(&settings, 0, sizeof(settings));
    settings.keyring = (char *)keyring;
    settings.label   = (char *)label;  /* NULL is acceptable */

    int tlsRc = tlsInit(&tlsEnv, &settings);
    if (tlsRc != 0) {
      fprintf(stderr, "Error: tlsInit() failed: %s (rc=%d)\n",
              tlsStrError(tlsRc), tlsRc);
      int clRc = 0, clRsn = 0;
      socketClose(sock, &clRc, &clRsn);
      return 2;
    }

    tlsRc = tlsSocketInit(tlsEnv, &tlsSock, sock->sd, 0 /*isServer=false*/);
    if (tlsRc != 0) {
      fprintf(stderr, "Error: TLS handshake failed: %s (rc=%d)\n",
              tlsStrError(tlsRc), tlsRc);
      tlsDestroy(tlsEnv);
      int clRc = 0, clRsn = 0;
      socketClose(sock, &clRc, &clRsn);
      return 2;
    }

    printf("TLS handshake successful.\n");
  }

  printf("Connected%s. Sending TN3270 Telnet probe (RFC 1041 + RFC 854/856/885/1091)...\n",
         useTls ? " (TLS)" : "");

  /* Combine the plain socket and optional TLS socket into one context. */
  Connection connection;
  connection.sock    = sock;
  connection.tlsSock = tlsSock;

  /* ------------------------------------------------------------------ */
  /* Send TN3270 negotiation probe                                       */
  /* ------------------------------------------------------------------ */
  if (sendTn3270Probe(&connection) < 0) {
    fprintf(stderr, "Error: failed to send probe.\n");
    if (connection.tlsSock != NULL) { tlsSocketClose(connection.tlsSock); tlsDestroy(tlsEnv); }
    int clRc = 0, clRsn = 0;
    socketClose(connection.sock, &clRc, &clRsn);
    return 2;
  }

  /* ------------------------------------------------------------------ */
  /* Read server responses for up to TIMEOUT_SEC seconds                */
  /* Use tcpStatus to poll for readability, then socketRead to receive. */
  /* ------------------------------------------------------------------ */
  unsigned int detectFlags = DETECT_NONE;
  unsigned char recvBuf[RECV_BUFSIZE];
  int  totalReceived = 0;
  int  rounds        = 0;
  int  maxRounds     = 10;

  while (rounds < maxRounds) {
    int n = 0;

    if (connection.tlsSock != NULL) {
      /* TLS: gsk_secure_socket_read handles its own blocking via
         the I/O callbacks registered in tlsSocketInit.            */
      int out = 0;
      int tlsRc = tlsRead(connection.tlsSock, (char *)recvBuf,
                          (int)(sizeof(recvBuf) - 1), &out);
      if (tlsRc != 0) {
        if (verbose) printf("[recv] tlsRead() error: %s (rc=%d)\n",
                            tlsStrError(tlsRc), tlsRc);
        break;
      }
      n = out;
      if (n == 0) {
        if (verbose) printf("[recv] server closed TLS connection.\n");
        break;
      }
    } else {
      int stRc = 0, stRsn = 0;
      int status = tcpStatus(connection.sock, TIMEOUT_SEC * 1000, 0 /* checkWrite */,
                             &stRc, &stRsn);
      if (status & SD_STATUS_TIMEOUT) {
        if (verbose) printf("[recv] tcpStatus() timed out after %d s\n", TIMEOUT_SEC);
        break;
      }
      if ((status & SD_STATUS_FAILED) || !(status & SD_STATUS_RD_RDY)) {
        if (verbose) printf("[recv] tcpStatus() error (status=0x%X rc=%d rsn=0x%X)\n",
                            status, stRc, stRsn);
        break;
      }

      int rdRc = 0, rdRsn = 0;
      n = socketRead(connection.sock, (char *)recvBuf, sizeof(recvBuf) - 1,
                     &rdRc, &rdRsn);
      if (n <= 0) {
        if (n == 0) {
          if (verbose) printf("[recv] server closed connection.\n");
        } else {
          if (verbose) printf("[recv] socketRead() error (rc=%d rsn=0x%X)\n",
                              rdRc, rdRsn);
        }
        break;
      }
    }

    totalReceived += n;
    if (verbose) {
      printf("[recv] received %d bytes (total %d):\n", n, totalReceived);
      dumpbuffer((const char *)recvBuf, n);
    }

    detectFlags |= parseTelnetData(&connection, recvBuf, n, verbose);
    rounds++;

    /* Once we have enough evidence, stop waiting */
    if (detectFlags & DETECT_TN3270E_FUNCTIONS) break;
    if (detectFlags & DETECT_3270_REGIME) break;
    if ((detectFlags & DETECT_BINARY) && (detectFlags & DETECT_EOR)) break;
  }

  if (connection.tlsSock != NULL) {
    tlsSocketClose(connection.tlsSock);
    tlsDestroy(tlsEnv);
  }
  int clRc = 0, clRsn = 0;
  socketClose(connection.sock, &clRc, &clRsn);

  printf("\n--- TN3270 Detection Results ---\n");
  printf("  Bytes received             : %d\n", totalReceived);
  printf("  IAC WILL TN3270E           : %s  (RFC 2355)\n",
         (detectFlags & DETECT_TN3270E)          ? "YES" : "no");
  printf("  SB  TN3270E DEVICE-TYPE IS : %s  (RFC 2355 device-type)\n",
         (detectFlags & DETECT_TN3270E_DEVTYPE)  ? "YES" : "no");
  printf("  SB  TN3270E FUNCTIONS IS   : %s  (RFC 2355 functions)\n",
         (detectFlags & DETECT_TN3270E_FUNCTIONS)? "YES" : "no");
  printf("  IAC WILL 3270-REGIME       : %s  (RFC 1041)\n",
         (detectFlags & DETECT_3270_REGIME) ? "YES" : "no");
  printf("  IAC SB  3270-REGIME ARE    : %s  (RFC 1041 subneg)\n",
         (detectFlags & DETECT_SB_REGIME)   ? "YES" : "no");
  printf("  IAC WILL BINARY            : %s  (RFC 856)\n",
         (detectFlags & DETECT_BINARY)      ? "YES" : "no");
  printf("  IAC WILL EOR               : %s  (RFC 885)\n",
         (detectFlags & DETECT_EOR)         ? "YES" : "no");
  printf("  IAC WILL TERMINAL-TYPE     : %s  (RFC 1091)\n",
         (detectFlags & DETECT_TERMTYPE)    ? "YES" : "no");
  printf("\n");

  if (IS_TN3270(detectFlags)) {
    /* Determine flavour */
    if (detectFlags & DETECT_TN3270E) {
      if (detectFlags & DETECT_TN3270E_FUNCTIONS) {
        printf("RESULT: TN3270E server detected (RFC 2355 – full negotiation complete).\n");
      } else if (detectFlags & DETECT_TN3270E_DEVTYPE) {
        printf("RESULT: TN3270E server detected (RFC 2355 – device-type negotiated).\n");
      } else {
        printf("RESULT: TN3270E server detected (RFC 2355 – WILL TN3270E received).\n");
      }
    } else if ((detectFlags & DETECT_3270_REGIME) || (detectFlags & DETECT_SB_REGIME)) {
      printf("RESULT: TN3270 server detected (RFC 1041 – 3270 Regime Option).\n");
    } else {
      printf("RESULT: TN3270 server detected (classic mode – BINARY + EOR negotiated).\n");
    }

    if (detectFlags & DETECT_TERMTYPE) {
      printf("        Terminal-type negotiation also supported.\n");
    }
    return 0;
  }

  if (totalReceived == 0) {
    printf("RESULT: No Telnet data received – server does not speak TN3270 (or is not a Telnet server).\n");
  } else {
    printf("RESULT: Server responded but did not accept TN3270 negotiation.\n");
    printf("        This is not a TN3270 Telnet server.\n");
  }

  return 1;
}
