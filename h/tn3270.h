/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.

  ----------------------------------------------------------------------------
  tn3270.h

  TN3270E protocol constants and detection flag definitions:
    - RFC 2355 - TN3270 Enhancements (option 40)
*/

#ifndef TN3270_H_
#define TN3270_H_

/* -------------------------------------------------------------------------
 * TN3270E sub-negotiation command codes (RFC 2355 §3)
 * ---------------------------------------------------------------------- */
#define TN3270E_ASSOCIATE   0x00  /* Printer partner association             */
#define TN3270E_CONNECT     0x01  /* Request/confirm specific device-name    */
#define TN3270E_DEVICE_TYPE 0x02  /* Device-type negotiation command         */
#define TN3270E_FUNCTIONS   0x03  /* Functions negotiation command           */
#define TN3270E_IS          0x04  /* Positive acceptance                     */
#define TN3270E_REASON      0x05  /* Reason code follows                     */
#define TN3270E_REJECT      0x06  /* Rejection command                       */
#define TN3270E_REQUEST     0x07  /* Request command                         */
#define TN3270E_SEND        0x08  /* Server requests client information      */

/* -------------------------------------------------------------------------
 * TN3270E DEVICE-TYPE REJECT reason codes (RFC 2355 §7.1.5)
 * ---------------------------------------------------------------------- */
#define TN3270E_REASON_CONN_PARTNER    0x00
#define TN3270E_REASON_DEVICE_IN_USE   0x01
#define TN3270E_REASON_INV_ASSOCIATE   0x02
#define TN3270E_REASON_INV_NAME        0x03
#define TN3270E_REASON_INV_DEVICE_TYPE 0x04
#define TN3270E_REASON_TYPE_NAME_ERR   0x05
#define TN3270E_REASON_UNKNOWN_ERROR   0x06
#define TN3270E_REASON_UNSUPPORTED_REQ 0x07

/* -------------------------------------------------------------------------
 * Detection result flags
 * Set during Telnet option negotiation to record which capabilities the
 * server has confirmed.
 * ---------------------------------------------------------------------- */
#define DETECT_NONE              0x00
#define DETECT_3270_REGIME       0x01  /* Server agreed WILL/DO 3270-REGIME (RFC 1041) */
#define DETECT_BINARY            0x02  /* Server agreed WILL/DO BINARY                 */
#define DETECT_EOR               0x04  /* Server agreed WILL/DO EOR                    */
#define DETECT_TERMTYPE          0x08  /* Server agreed WILL/DO TERMINAL-TYPE          */
#define DETECT_SB_REGIME         0x10  /* Server sent SB 3270-REGIME ARE ...           */
#define DETECT_TN3270E           0x20  /* Server agreed TN3270E (RFC 2355)             */
#define DETECT_TN3270E_DEVTYPE   0x40  /* Server confirmed DEVICE-TYPE IS              */
#define DETECT_TN3270E_FUNCTIONS 0x80  /* Server confirmed FUNCTIONS IS                */

/*
 * IS_TN3270 -- true when any of the three TN3270 detection methods fired:
 *   RFC 2355 (TN3270E), RFC 1041 (3270-REGIME), or classic BINARY + EOR.
 */
#define IS_TN3270(flags) \
    (((flags) & DETECT_3270_REGIME) || \
     (((flags) & DETECT_BINARY) && ((flags) & DETECT_EOR)) || \
     ((flags) & DETECT_TN3270E))

#endif /* TN3270_H_ */
