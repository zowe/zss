#!/bin/sh
# ab-compare.sh - run the same test against two zssServer64 builds and prove
# which one answered.
#
#   ./ab-compare.sh --before bin/zssServer64.old --after bin/zssServer64.new \
#                   -- node suites/my-test.js --user MYUSERID
#
# Everything after -- is the test command, run once per build with the server
# already up. Its exit status is reported but not interpreted: what this script
# guarantees is that each run really was served by the build you named.
#
# The two images are builds YOU made; they are not in the repository and never
# will be. The usual recipe is to build once before a change and once after,
# keeping both, so a fix can be demonstrated rather than asserted.
#
# WHY THIS EXISTS, AND WHY IT IS PARANOID: a harness that cannot say which build
# answered is worse than none. An earlier version of this logic silently tested
# the SAME server twice and reported success both times, because a process check
# used an unresolved ".." path that never matched what ps prints, nothing was
# stopped, and start-zss.sh's "already running" refusal went to /dev/null. So
# this script proves the swap happened before it believes anything:
#
#   1. the server restarted      - a ZWES1013I line appeared, else refuse to test
#   2. the builds really differ  - the two runs reported different build stamps,
#                                  else the comparison is declared INVALID
#
# Exit: 0 both runs completed and the builds differed; 2 invalid comparison or a
# setup failure. The TEST's own verdict is its business - read its output.
set -u

BEFORE=""; AFTER=""
while [ $# -gt 0 ]; do
  case "$1" in
    --before) BEFORE="${2:?--before needs a path}"; shift 2 ;;
    --after)  AFTER="${2:?--after needs a path}";  shift 2 ;;
    --)       shift; break ;;
    -h|--help)
      sed -n '2,28p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done

[ -n "$BEFORE" ] && [ -n "$AFTER" ] || { echo "ERROR: --before and --after are both required" >&2; exit 2; }
[ $# -gt 0 ] || { echo "ERROR: no test command given after --" >&2; exit 2; }

HERE="$(cd "$(dirname "$0")" && pwd)"
ZSS_TEST_SUPPORT="$HERE"; . "$HERE/test-env.sh"
# Resolve to a PHYSICAL path: ps prints the resolved path, so an unresolved
# "$HERE/../bin" never matches and every process check silently finds nothing.
BIN="$(cd "$HERE/../bin" && pwd)" || { echo "ERROR: $HERE/../bin not found" >&2; exit 2; }
LIVE="$BIN/zssServer64"
LOG="$ZSS_TEST_INST/logs/zss.out"

for f in "$BEFORE" "$AFTER"; do
  [ -f "$f" ] || { echo "ERROR: $f not found. Build it first." >&2; exit 2; }
done

ours() { ps -ef | grep "$LIVE" | grep -v grep | awk '{print $2}'; }

stop_ours() {
  p="$(ours)"; [ -z "$p" ] && return 0
  kill $p 2>/dev/null
  i=0
  while [ $i -lt 6 ]; do sleep 2; [ -z "$(ours)" ] && return 0; i=$((i+1)); done
  kill -9 $(ours) 2>/dev/null; sleep 2
  [ -z "$(ours)" ] || { echo "ERROR: could not stop $(ours)" >&2; return 1; }
}

VERSION=""
run_one() {   # $1 = label  $2 = image ; then "$@" shifted twice = the test command
  label="$1"; image="$2"; shift 2
  echo ""
  echo "================================================================"
  echo " $label"
  echo " $image"
  echo "================================================================"

  stop_ours || exit 2
  cp -p "$image" "$LIVE" || exit 2
  extattr +p "$LIVE" 2>/dev/null
  mv "$LOG" "$LOG.prev" 2>/dev/null

  # Never discard this: the refusal is the failure that made the old version lie.
  if ! "$HERE/start-zss.sh" > /tmp/abcmp.$$ 2>&1; then
    echo "ERROR: start-zss.sh failed:"; sed 's/^/     /' /tmp/abcmp.$$; rm -f /tmp/abcmp.$$; exit 2
  fi
  if grep -i 'ERROR' /tmp/abcmp.$$ >/dev/null 2>&1; then
    echo "ERROR: start-zss.sh reported:"; sed 's/^/     /' /tmp/abcmp.$$; rm -f /tmp/abcmp.$$; exit 2
  fi
  rm -f /tmp/abcmp.$$

  i=0; VERSION=""
  while [ $i -lt 10 ]; do
    sleep 3
    VERSION="$(grep 'ZWES1013I' "$LOG" 2>/dev/null | head -1)"
    [ -n "$VERSION" ] && break
    i=$((i+1))
  done
  if [ -z "$VERSION" ]; then
    echo "ERROR: no ZWES1013I in $LOG after 30s - the server did not start."
    echo "       Refusing to run the test: any answer would come from whatever"
    echo "       else is listening, not from $image."
    exit 2
  fi
  echo "  $VERSION"

  zis="$(grep 'ZWES1014I' "$LOG" 2>/dev/null | head -1)"
  [ -n "$zis" ] && echo "  $zis"
  case "$zis" in
    *"cmsRC='0'"*) ;;
    *) echo "  WARNING: ZIS is not Ok, so ZSS cannot authenticate anyone. Any"
       echo "           authenticated case will be 401 and will say nothing." ;;
  esac

  echo ""
  "$@"
  echo "  (test exit $?)"
}

run_one "BEFORE" "$BEFORE" "$@"
V_BEFORE="$VERSION"
run_one "AFTER"  "$AFTER"  "$@"
V_AFTER="$VERSION"

echo ""
echo "================================================================"
if [ "$V_BEFORE" = "$V_AFTER" ]; then
  echo " INVALID COMPARISON"
  echo "================================================================"
  echo "Both runs reported the SAME build stamp:"
  echo "  $V_BEFORE"
  echo "The same server answered twice, so the two result blocks above mean"
  echo "nothing. Check that the two images really differ, that nothing else is"
  echo "holding port $ZSS_TEST_PORT, and that $LIVE is writable."
  exit 2
fi
echo " comparison was genuine - two different builds answered"
echo "================================================================"
echo "  before: $V_BEFORE"
echo "  after : $V_AFTER"
echo ""
echo "The test server is left running on the AFTER build."
echo "Stop it with:  kill \$(ps -ef | grep $LIVE | grep -v grep | awk '{print \$2}')"
