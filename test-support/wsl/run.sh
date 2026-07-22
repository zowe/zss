#!/bin/sh
#
# run.sh -- launch the WSL-native zssServer (plaintext, no TLS/APIML/ZIS) and
# serve a UNIX directory, for local #828 charset work and general ZSS dev.
# The committed source of truth for the dev config is THIS script (it writes
# build/zowe.yaml); nothing is hand-pasted at run time.
#
#   sh run.sh          # start detached, tail a few lines, print curl hints
#   sh run.sh --fg     # foreground (Ctrl-C to stop)
#   sh run.sh --stop   # stop a running instance
#
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ZSS="$(cd "$HERE/../.." && pwd)"
BUILD="$HERE/build"
BIN="$BUILD/zssServer"
INST="$BUILD/instance"
CFG="$BUILD/zowe.yaml"
SERVEDIR="$INST/served"
PORT=8544
PIDFILE="$BUILD/zss.pid"
LOG="$INST/logs/zss.out"

# schemas: zss root schema + zss-config, plus the base schemas they $ref (vendored
# under test-support/schemas/). Root first; configmgr resolves cross-$id refs.
SCHEMAS="$ZSS/schemas/zowe-schema.json:$ZSS/schemas/zss-config.json:$HERE/../schemas/zowe-yaml-schema.json:$HERE/../schemas/server-common.json"

case "${1:-}" in
  --stop)
    [ -f "$PIDFILE" ] && kill "$(cat "$PIDFILE")" 2>/dev/null && echo "stopped $(cat "$PIDFILE")" && rm -f "$PIDFILE" || echo "no pidfile / not running"
    exit 0 ;;
esac

[ -f "$BIN" ] || { echo "ERROR: $BIN not found -- run build.sh first."; exit 1; }

mkdir -p "$INST/logs" "$INST/plugins" "$INST/product" "$SERVEDIR"

# A UTF-8 sample with characters beyond U+00FF (the #828 trigger class):
# e-acute (mappable to 819), em-dash, coffee-cup, CJK (unmappable). Octal
# escapes: POSIX printf interprets \NNN portably; \xNN is not portable and
# can land as literal text. Bytes must match the expected values in
# test-support/suites/unixfile-charset.js. On z/OS this file would be tagged;
# on Linux there are no tags, so charset behavior is driven by the request.
printf 'caf\303\251\342\200\224au\342\230\225 \346\227\245\346\234\254\350\252\236 ok\n' > "$SERVEDIR/utf8-multibyte.txt"
printf 'plain ascii line\n' > "$SERVEDIR/ascii.txt"

# ---- generated plaintext dev config (source of truth = this heredoc) -------
cat > "$CFG" <<YAML
zowe:
  logDirectory: $INST/logs
  verifyCertificates: DISABLED
components:
  zss:
    tls: false
    port: $PORT
    agent:
      "64bit": true
      http:
        ipAddresses: ["127.0.0.1"]
        port: $PORT
      jwt:
        fallback: true
      mediationLayer:
        enabled: false          # kills the APIML JWK fetch task (#843) entirely
    logLevels:
      "_zss.socketTrace": ${SOCKTRACE:-0}
      "_zss.httpSocketTrace": ${HTTPTRACE:-0}
      "_zss.httpParseTrace": ${PARSETRACE:-0}
      "_zss.httpDispatchTrace": ${DISPTRACE:-0}
      "_zss.httpAuthTrace": ${AUTHTRACE:-0}
    dataserviceAuthentication:
      defaultAuthentication: fallback
    pluginsDir:  $INST/plugins
    productDir:  $INST/product
    instanceDir: $INST
YAML

echo "== zssServer (WSL, plaintext) =="
echo "  bin:     $BIN"
echo "  config:  FILE($CFG)"
echo "  serve:   $SERVEDIR   (utf8-multibyte.txt, ascii.txt)"
echo "  url:     http://127.0.0.1:$PORT"
echo

if [ "${1:-}" = "--fg" ]; then
  exec "$BIN" --schemas "$SCHEMAS" --configs "FILE($CFG)"
else
  nohup "$BIN" --schemas "$SCHEMAS" --configs "FILE($CFG)" > "$LOG" 2>&1 &
  echo $! > "$PIDFILE"
  echo "  pid:     $(cat "$PIDFILE")  (log: $LOG)"
  sleep 2
  echo "  --- first log lines ---"
  head -30 "$LOG" 2>/dev/null | sed 's/^/    /'
  echo "  stop:    sh run.sh --stop"
fi
