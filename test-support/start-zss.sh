#!/bin/sh
# start-zss.sh - launch the standalone multi-user test ZSS (zssServer64).
# Run on z/OS under a shell with node and the z/OS tools on PATH, after
# and running provision-keyring-cert.sh.
#
# Modes:
#   (default)  DETACHED via nohup -> SURVIVES LOGOFF; logs to $INST/logs/zss.out;
#              prints the PID and returns. This is what a long-lived test server wants.
#   --fg       FOREGROUND (exec); Ctrl-C stops it. Interactive/debug only - it DIES
#              when your shell/SSH session ends (that is how it died overnight).
#   --trace    System SSL handshake trace -> $INST/gskssl.<pid>.trc
#              (format with: gsktrace $INST/gskssl.<pid>.trc > $INST/gskssl.out)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ZSS_ROOT="$(cd "$HERE/.." && pwd)"
ZSS_TEST_SUPPORT="$HERE"; . "$HERE/test-env.sh"   # site values: INST, PORT, ADDR, keyring
BIN="$ZSS_ROOT/bin/zssServer64"
CFG="$HERE/zowe.yaml"
INST="$ZSS_TEST_INST"

# --schemas: zss's own two (zowe-schema is the root, zss-config) plus the two base
# schemas they refer to by $id (zowe-yaml-schema.json, server-common.json).
# Those two belong to Zowe and ship in every install under <runtime>/schemas/,
# but the installed copies are UNTAGGED, and the EBCDIC config manager reads
# untagged ASCII as EBCDIC and sees garbage. So this directory holds copies
# tagged IBM-1047. Root schema first; the config manager loads them all and
# resolves the cross-$id references itself.
# ZSS's own two come from the repo; the two Zowe base schemas come from the
# instance directory, put there by fetch-schemas.js. Nothing is kept as a copy
# in this repository: see fetch-schemas.js for why, and run
# configure-test-env.sh if they are missing.
BASE="$ZSS_TEST_INST/schemas"
SCHEMAS="$ZSS_ROOT/schemas/zowe-schema.json:$ZSS_ROOT/schemas/zss-config.json:$BASE/zowe-yaml-schema.json:$BASE/server-common.json"

for s in "$BASE/zowe-yaml-schema.json" "$BASE/server-common.json"; do
  [ -f "$s" ] || { echo "ERROR: $s missing. Run:  node $HERE/fetch-schemas.js"; exit 1; }
done

[ -f "$BIN" ] || { echo "ERROR: $BIN not found - build zssServer64 first."; exit 1; }
mkdir -p "$INST/logs" "$INST/plugins" "$INST/product" "$INST/instance"

# Refuse to stack a second copy of OUR instance - but scope the check to OUR binary
# by FULL PATH, so we NEVER match (or suggest killing) the shared ZWESVUSR
# production/staging zssServer64 on this box. Matching by the bare name "zssServer64"
# is a foot-gun here: as uid 0 you could kill shared infra. ($BIN has no regex
# metacharacters, so a plain grep matches it literally.)
running="$(ps -ef | grep "$BIN" | grep -v grep | awk '{print $2}')"
if [ -n "$running" ]; then
  echo "ERROR: our zssServer64 already running (PID: $running)."
  echo "       Stop just that one:  kill $running"
  exit 1
fi

MODE=bg
# NB: z/OS /bin/sh under `set -u` errors on "$@" when there are NO args (FSUM7730),
# unlike bash. Iterate via $#/shift so the no-arg case is safe.
while [ "$#" -gt 0 ]; do
  a="$1"; shift
  case "$a" in
    --fg)    MODE=fg ;;
    --trace) export GSK_TRACE=0xff; export GSK_TRACE_FILE="$INST/gskssl.%.trc"
             echo "  GSK_TRACE=0xff -> $INST/gskssl.<pid>.trc" ;;
    *)       echo "  (ignoring unknown arg: $a)" ;;
  esac
done

echo "Starting ZSS (multi-user, TLS, $ZSS_TEST_ADDR:$ZSS_TEST_PORT)"
echo "  bin:     $BIN"
echo "  config:  FILE($CFG)"
echo "  schemas: $SCHEMAS"

if [ "$MODE" = fg ]; then
  echo "  mode:    FOREGROUND (Ctrl-C to stop; dies with your session)"
  exec "$BIN" --schemas "$SCHEMAS" --configs "FILE($CFG)"
else
  LOG="$INST/logs/zss.out"
  nohup "$BIN" --schemas "$SCHEMAS" --configs "FILE($CFG)" > "$LOG" 2>&1 &
  PID=$!
  echo "  mode:    DETACHED (nohup) - survives logoff"
  echo "  pid:     $PID"
  echo "  log:     $LOG   (tail -f to watch; success = ZWES1014I ... cmsRC='0')"
  echo "  stop:    kill $PID"
fi
