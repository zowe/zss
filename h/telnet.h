/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.

  ----------------------------------------------------------------------------
  telnet.h

  Telnet protocol constants covering:
    - RFC 854  - Telnet Protocol Specification
    - RFC 856  - Telnet Binary Transmission        (option  0)
    - RFC 857  - Telnet Echo Option                (option  1)
    - RFC 858  - Suppress Go Ahead                 (option  3)
    - RFC 885  - Telnet End Of Record Option       (option 25)
    - RFC 1091 - Telnet Terminal-Type Option       (option 24)
    - RFC 1041 - Telnet 3270 Regime Option         (option 29)
    - RFC 2355 - TN3270 Enhancements               (option 40)
*/

#ifndef TELNET_H_
#define TELNET_H_

/* -------------------------------------------------------------------------
 * Telnet command bytes (RFC 854)
 * ---------------------------------------------------------------------- */
#define TELNET_SE     240  /* Subnegotiation end                           */
#define TELNET_NOP    241  /* No-operation                                 */
#define TELNET_DM     242  /* Data Mark                                    */
#define TELNET_BRK    243  /* Break                                        */
#define TELNET_IP     244  /* Interrupt Process                            */
#define TELNET_AO     245  /* Abort Output                                 */
#define TELNET_AYT    246  /* Are You There                                */
#define TELNET_EC     247  /* Erase Character                              */
#define TELNET_EL     248  /* Erase Line                                   */
#define TELNET_GA     249  /* Go Ahead                                     */
#define TELNET_SB     250  /* Subnegotiation begin                         */
#define TELNET_WILL   251  /* I will use this option                       */
#define TELNET_WONT   252  /* I will not use this option                   */
#define TELNET_DO     253  /* Please use this option                       */
#define TELNET_DONT   254  /* You must stop using this option              */
#define TELNET_IAC    255  /* Interpret As Command                         */

/* End-of-Record command byte (RFC 885) */
#define TELNET_EOR_CMD 239

/* -------------------------------------------------------------------------
 * Telnet option codes
 * ---------------------------------------------------------------------- */
#define OPT_BINARY         0   /* RFC 856  – Binary Transmission            */
#define OPT_ECHO           1   /* RFC 857  – Echo                           */
#define OPT_SGA            3   /* RFC 858  – Suppress Go Ahead              */
#define OPT_TERMINAL_TYPE 24   /* RFC 1091 – Terminal-Type                  */
#define OPT_EOR           25   /* RFC 885  – End Of Record                  */
#define OPT_3270_REGIME   29   /* RFC 1041 – 3270 Regime                    */
#define OPT_TN3270E       40   /* RFC 2355 – TN3270 Enhancements            */

/* -------------------------------------------------------------------------
 * Sub-negotiation IS / ARE codes (RFC 1041)
 * ---------------------------------------------------------------------- */
#define SB_IS   0
#define SB_ARE  1

#endif /* TELNET_H_ */
