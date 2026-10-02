#!/bin/sh
# provision-keyring-cert.sh — create a SAF key ring + a Zowe-compatible server cert
# for the standalone test ZSS, via RACDCERT (TSO). zwe-independent; distilled from
# ZWESAMP/ZWEKRING. Run on z/OS as a userid with CONTROL on
# IRR.DIGTCERT.{GENCERT,ADDRING,CONNECT} (ZOWEAD5 has it). Re-runnable.
#
# Usage: sh provision-keyring-cert.sh [host] [ip] [ringuser] [ring] [label] [calabel]
set -u
HOST="${1:-zzow11.zowe.marist.cloud}"
IP="${2:-127.0.0.1}"
USER_ID="${3:-ZOWEAD5}"
RING="${4:-ZWESRING}"
LABEL="${5:-ZOWECERT}"
CALABEL="${6:-ZOWELOCALCA}"
NOTAFTER="2031-12-31"
DN="OU('ZOWE') O('Zowe Test') L('Poughkeepsie') SP('NY') C('US')"

run(){ echo ">> $1"; tsocmd "$1" 2>&1; echo; }

echo "=== 1. key ring (owned by $USER_ID) ==="
run "RACDCERT ADDRING($RING) ID($USER_ID)"

echo "=== 2. local CA (CERTAUTH) ==="
run "RACDCERT GENCERT CERTAUTH SUBJECTSDN(CN('ZOWE LOCAL CA') $DN) SIZE(2048) NOTAFTER(DATE($NOTAFTER)) WITHLABEL('$CALABEL') KEYUSAGE(CERTSIGN)"
run "RACDCERT CONNECT(CERTAUTH LABEL('$CALABEL') RING($RING) USAGE(CERTAUTH)) ID($USER_ID)"

echo "=== 3. server cert: HANDSHAKE usage, SAN = ip + domain, signed by local CA ==="
run "RACDCERT GENCERT ID($USER_ID) SUBJECTSDN(CN('$HOST') $DN) SIZE(2048) NOTAFTER(DATE($NOTAFTER)) WITHLABEL('$LABEL') KEYUSAGE(HANDSHAKE) ALTNAME(IP($IP) DOMAIN('$HOST')) SIGNWITH(CERTAUTH LABEL('$CALABEL'))"
run "RACDCERT CONNECT(ID($USER_ID) LABEL('$LABEL') RING($RING) USAGE(PERSONAL) DEFAULT) ID($USER_ID)"

echo "=== 4. make it effective (may need RACF SPECIAL; ok if it warns) ==="
run "SETROPTS RACLIST(DIGTCERT,DIGTRING) REFRESH"

echo "=== 5. verify ==="
run "RACDCERT LISTRING($RING) ID($USER_ID)"

echo "DONE. In zowe.yaml: components.zss.agent.https.keyring = \"$USER_ID/$RING\", label = \"$LABEL\""
echo "Node client trust anchor (export the CA when needed):"
echo "  tsocmd \"RACDCERT CERTAUTH EXPORT(LABEL('$CALABEL')) DSN('$USER_ID.ZOWECA.PEM') FORMAT(CERTB64)\""
