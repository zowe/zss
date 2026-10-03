#!/bin/sh
# provision-keyring-cert.sh - create a SAF key ring and a Zowe-compatible server
# certificate for the standalone test ZSS, via RACDCERT. zwe-independent;
# distilled from ZWESAMP/ZWEKRING. Re-runnable.
#
#   sh provision-keyring-cert.sh                 # all defaults, from test-env.sh
#   sh provision-keyring-cert.sh myhost.example.com
#
# Usage: provision-keyring-cert.sh [host] [ip] [ringuser] [ring] [label] [calabel]
#
# Needs CONTROL on IRR.DIGTCERT.GENCERT, .ADDRING and .CONNECT. Step 4 needs
# RACF SPECIAL and is allowed to fail: the ring still works for this userid.
#
# WHAT IT CREATES, all under the invoking userid and all disposable:
#   a local CA (CERTAUTH), a server certificate signed by it, and a key ring
#   holding both. There is no password anywhere: a SAF key ring is protected by
#   SAF, which is the reason to prefer one over a PKCS#12 file for testing.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ZSS_TEST_SUPPORT="$HERE"; . "$HERE/test-env.sh"

# The certificate's CN and SAN must name the host a client will actually connect
# to, so default to THIS system rather than to a hostname baked into the script.
_host="$(hostname 2>/dev/null || uname -n 2>/dev/null)"
HOST="${1:-${_host:-localhost}}"
IP="${2:-$ZSS_TEST_ADDR}"
USER_ID="${3:-$ZSS_TEST_USERID}"
RING="${4:-$ZSS_TEST_RING}"
LABEL="${5:-$ZSS_TEST_LABEL}"
CALABEL="${6:-ZOWELOCALCA}"
NOTAFTER="${NOTAFTER:-2031-12-31}"
# Cosmetic for a throwaway test CA. Override with ZSS_TEST_CERT_DN if it matters.
DN="${ZSS_TEST_CERT_DN:-OU('ZOWE') O('Zowe Test') C('US')}"

[ -n "$HOST" ] || { echo "ERROR: cannot determine a hostname; pass one as \$1" >&2; exit 1; }

echo "=== what this will create ==="
echo "  ring      $USER_ID/$RING"
echo "  server    CN=$HOST  label $LABEL  SAN ip=$IP domain=$HOST"
echo "  local CA  label $CALABEL"
echo "  expires   $NOTAFTER"
echo ""

run(){ echo ">> $1"; tsocmd "$1" 2>&1; echo; }

echo "=== 1. key ring (owned by $USER_ID) ==="
run "RACDCERT ADDRING($RING) ID($USER_ID)"

echo "=== 2. local CA (CERTAUTH) ==="
run "RACDCERT GENCERT CERTAUTH SUBJECTSDN(CN('ZOWE LOCAL CA') $DN) SIZE(2048) NOTAFTER(DATE($NOTAFTER)) WITHLABEL('$CALABEL') KEYUSAGE(CERTSIGN)"
run "RACDCERT CONNECT(CERTAUTH LABEL('$CALABEL') RING($RING) USAGE(CERTAUTH)) ID($USER_ID)"

echo "=== 3. server cert: HANDSHAKE usage, SAN = ip + domain, signed by the local CA ==="
run "RACDCERT GENCERT ID($USER_ID) SUBJECTSDN(CN('$HOST') $DN) SIZE(2048) NOTAFTER(DATE($NOTAFTER)) WITHLABEL('$LABEL') KEYUSAGE(HANDSHAKE) ALTNAME(IP($IP) DOMAIN('$HOST')) SIGNWITH(CERTAUTH LABEL('$CALABEL'))"
run "RACDCERT CONNECT(ID($USER_ID) LABEL('$LABEL') RING($RING) USAGE(PERSONAL) DEFAULT) ID($USER_ID)"

echo "=== 4. make it effective (needs RACF SPECIAL; a warning here is survivable) ==="
run "SETROPTS RACLIST(DIGTCERT,DIGTRING) REFRESH"

echo "=== 5. verify ==="
run "RACDCERT LISTRING($RING) ID($USER_ID)"

echo "DONE. zowe.yaml wants: keyring \"$USER_ID/$RING\", label \"$LABEL\""
echo "      configure-test-env.sh already generates those from test-env.sh."
echo ""
echo "To let a client trust it, export the CA and add it to that client's"
echo "trust store (the test drivers here do not verify, so they do not need it):"
echo "  tsocmd \"RACDCERT CERTAUTH EXPORT(LABEL('$CALABEL')) DSN('$USER_ID.ZOWECA.PEM') FORMAT(CERTB64)\""
