#!/bin/sh
# ras-rbac-ab.sh - run ras-rbac-test.js against the pre-fix and post-fix
# zssServer64 binaries in turn, so zowe/zss#855 is demonstrated rather than
# asserted. Run on z/OS as the owner of the test server.
#
#   cd /ZOWE/joezowe && . ./env.sh
#   cd /ZOWE/joezowe/git2026/zss/test-support
#   ./ras-rbac-ab.sh MYUSERID [on|off]
#
# You are prompted for the password twice, once per binary. It is never taken
# on the command line and never put in the environment.
#
# The two binaries differ ONLY in c/rasService.c: both were linked from the same
# tree with the same objects, one before the fix and one after.
#
# WHY THIS SCRIPT IS PARANOID: the first version silently tested the SAME server
# twice and reported FIXED for both, because $BIN held an unresolved "..", the
# process grep therefore never matched what ps prints, nothing was stopped, and
# start-zss.sh's refusal went to /dev/null. A test harness that cannot tell which
# build answered is worse than no harness, so this one now proves the swap
# happened before it believes a result.
set -u

[ $# -ge 1 ] || { echo "usage: $0 <userid> [on|off]   (second arg = the rbac setting in zowe.yaml, default off)"; exit 2; }
USERID="$1"
RBAC="${2:-off}"

HERE="$(cd "$(dirname "$0")" && pwd)"
# Resolve to a PHYSICAL path. ps prints the resolved path, so a "$HERE/../bin"
# style value never matches and every process check silently finds nothing.
BIN="$(cd "$HERE/../bin" && pwd)" || { echo "ERROR: $HERE/../bin not found"; exit 1; }
ZSS_TEST_SUPPORT="$HERE"; . "$HERE/test-env.sh"
LOG="$ZSS_TEST_INST/logs/zss.out"

for f in "$BIN/zssServer64.BEFORE" "$BIN/zssServer64.AFTER"; do
  [ -f "$f" ] || { echo "ERROR: $f not found."; exit 1; }
done
command -v node >/dev/null 2>&1 || { echo "ERROR: node not on PATH. Source your env.sh, or set ZSS_TEST_NODE_BIN in test-env.sh."; exit 1; }

ours() {   # pids of OUR test server, matched on the resolved path
  ps -ef | grep "$BIN/zssServer64" | grep -v grep | awk '{print $2}'
}

stop_ours() {
  p="$(ours)"
  [ -z "$p" ] && return 0
  kill $p 2>/dev/null
  i=0
  while [ $i -lt 6 ]; do
    sleep 2
    [ -z "$(ours)" ] && return 0
    i=$((i+1))
  done
  kill -9 $(ours) 2>/dev/null
  sleep 2
  [ -z "$(ours)" ] || { echo "ERROR: could not stop $(ours)"; return 1; }
}

VERSION=""      # set by run_one to the build stamp that actually answered

run_one() {
  label="$1"; image="$2"
  echo ""
  echo "================================================================"
  echo " $label"
  echo "================================================================"

  stop_ours || exit 1
  cp -p "$image" "$BIN/zssServer64" || exit 1
  extattr +p "$BIN/zssServer64"
  mv "$LOG" "$LOG.prev" 2>/dev/null

  # Do NOT discard this: its refusal ("already running") is exactly the failure
  # that made the first version of this script meaningless.
  if ! "$HERE/start-zss.sh" > /tmp/rasab.$$ 2>&1; then
    echo "ERROR: start-zss.sh failed:"; sed 's/^/     /' /tmp/rasab.$$; rm -f /tmp/rasab.$$; exit 1
  fi
  grep -i 'ERROR' /tmp/rasab.$$ >/dev/null 2>&1 && {
    echo "ERROR: start-zss.sh reported:"; sed 's/^/     /' /tmp/rasab.$$; rm -f /tmp/rasab.$$; exit 1; }
  rm -f /tmp/rasab.$$

  i=0; VERSION=""
  while [ $i -lt 10 ]; do
    sleep 3
    VERSION="$(grep 'ZWES1013I' "$LOG" 2>/dev/null | head -1)"
    [ -n "$VERSION" ] && break
    i=$((i+1))
  done
  if [ -z "$VERSION" ]; then
    echo "ERROR: no ZWES1013I in $LOG after 30s - the server did not start."
    echo "       Refusing to run the test, because any answer would come from"
    echo "       whatever else is listening, not from $image."
    exit 1
  fi
  echo "  $VERSION"

  zis="$(grep 'ZWES1014I' "$LOG" 2>/dev/null | head -1)"
  [ -n "$zis" ] && echo "  $zis"
  case "$zis" in
    *"cmsRC='0'"*) ;;
    *) echo "  WARNING: ZIS is not Ok. Authenticated cases will fail with 401 and"
       echo "           say nothing about authorization." ;;
  esac

  echo ""
  node "$HERE/ras-rbac-test.js" --user "$USERID" --rbac "$RBAC"
  echo "  (exit $?)"
}

run_one "BEFORE the fix" "$BIN/zssServer64.BEFORE"
V_BEFORE="$VERSION"
run_one "AFTER the fix"  "$BIN/zssServer64.AFTER"
V_AFTER="$VERSION"

echo ""
echo "================================================================"
if [ "$V_BEFORE" = "$V_AFTER" ]; then
  echo " INVALID COMPARISON"
  echo "================================================================"
  echo "Both runs reported the SAME build stamp:"
  echo "  $V_BEFORE"
  echo "So the same server answered twice and the two result tables above mean"
  echo "nothing. The binaries were not swapped. Check that nothing else is"
  echo "holding port $ZSS_TEST_PORT and that $BIN/zssServer64 is writable."
  exit 2
fi
echo " comparison was genuine - two different builds answered"
echo "================================================================"
echo "  before: $V_BEFORE"
echo "  after : $V_AFTER"
echo ""
echo "The test server is left running on the AFTER binary."
echo "Stop it with:  kill \$(ps -ef | grep $BIN/zssServer64 | grep -v grep | awk '{print \$2}')"
