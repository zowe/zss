#!/bin/sh
#
# chunk640-zos.sh -- z/OS (USS) sibling of chunk640-test.sh. Verifies PR #640
# by driving the REAL processHttpFragment with a real chunked HTTP request,
# built with xlclang -- so it exercises the EBCDIC chunked-detection + charset
# conversion path that the ASCII-native WSL harness cannot.
#
# Same shape as the WSL run: link JUST the parser code path (httpserver.c #640 +
# its real deps) with a tiny socket main, auto-stub everything the parser never
# reaches. No zowe.yaml / certs / APIML -- it is a socket, not the server.
#
# RUN ON MARIST (not metered), from a #640 clone of zowe-common-c:
#   SRC=/ZOWE/joezowe/scratch202607/cc640 \
#   ZSS=/ZOWE/joezowe/git2026/zss \
#   sh chunk640-zos.sh
# then, from another shell / curl:
#   printf 'AAABBBCCC' | curl -s --data-binary @- -H 'Transfer-Encoding: chunked' -H 'Expect:' http://127.0.0.1:8610/
#
# NB first-run tweaks are expected (a missing dep to add to the sweep, or a link
# flag) -- Marist is the truth plane, iterate there, then fold back.
#
# Source the env FIRST (it references unset vars), THEN turn on nounset.
. /ZOWE/joezowe/env.sh >/dev/null 2>&1 || true
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="${SRC:?set SRC=path to the #640 zowe-common-c clone}"
ZSS="${ZSS:?set ZSS=path to the zss repo (for zis/client.h)}"
GSKINC="${GSKINC:-/usr/lpp/gskssl/include}"
BUILD="${BUILD:-$HERE/zbuild}"
OBJ="$BUILD/obj"
CC="${CC:-xlclang}"
PORT="${PORT:-8610}"
mkdir -p "$OBJ"

# xlclang flags mirror build_zss64.sh; -fms-extensions for httpserver.h's tagged
# anonymous union member (HttpConversation); limits.h is carried by #640 itself.
DEFS="-DPRODUCT_MAJOR_VERSION=3 -DPRODUCT_MINOR_VERSION=6 -DPRODUCT_REVISION=0 -DPRODUCT_VERSION_DATE_STAMP=0 \
 -D_XOPEN_SOURCE=600 -DNOIBMHTTP=1 -D_OPEN_THREADS=1 -DHTTPSERVER_BPX_IMPERSONATION=1 -DAPF_AUTHORIZED=0 \
 -DUSE_ZOWE_TLS=1 -DNEW_CAA_LOCATIONS=1"
WC="-Wc,lp64,dll,expo,langlvl(extc99),gonum,goff,hgpr,roconst,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')"
INC="-I $SRC/h -I $SRC/platform/posix -I $SRC/jwt/jwt -I $SRC/jwt/rscrypto -I $ZSS/h -I $GSKINC"
CFLAGS="-fms-extensions $DEFS $WC $INC"

echo "== chunk640-zos =="
echo "   SRC=$SRC   ZSS=$ZSS"
echo "   #640 present in tree? newContentLength=$(grep -c newContentLength "$SRC/c/httpserver.c")  (want >=1)"
[ "$(grep -c newContentLength "$SRC/c/httpserver.c")" -ge 1 ] || { echo "ERROR: apply the #640 change to $SRC first (git fetch the PR branch)."; exit 1; }

# ---- compile sweep: everything that compiles on z/OS; metal/gsk/quickjs TUs
#      self-exclude by failing, exactly like the WSL build.sh strategy ---------
: > "$BUILD/compiled.txt"; : > "$BUILD/excluded.txt"
echo "-- compile sweep (this is the CPU step; Marist is not metered but be a good citizen)"
for src in "$SRC"/c/*.c "$SRC"/platform/posix/*.c; do
  base="$(basename "${src%.c}")"
  if $CC -c $CFLAGS "$src" -o "$OBJ/$base.o" 2>"$OBJ/$base.err"; then
    echo "$base" >> "$BUILD/compiled.txt"
  else
    printf '%-22s %s\n' "$base" "$(grep -m1 'ERROR' "$OBJ/$base.err" | cut -c1-70)" >> "$BUILD/excluded.txt"
    rm -f "$OBJ/$base.o"
  fi
done
$CC -c $CFLAGS "$HERE/chunk640-harness.c" -o "$OBJ/chunk640-harness.o" || { echo "harness compile failed"; exit 1; }
echo "   compiled $(wc -l < "$BUILD/compiled.txt") TUs, excluded $(wc -l < "$BUILD/excluded.txt")"

# ---- link harness main (NOT zss main) + auto-stub the unreached symbols ------
link() { $CC -o "$BUILD/chunk640-harness" "$OBJ/chunk640-harness.o" \
  $(ls "$OBJ"/*.o | grep -vE 'chunk640-(harness|stubs)\.o|/zss\.o') "$@" 2>"$BUILD/link.err"; }
if ! link; then
  grep -oE "[A-Za-z_][A-Za-z0-9_]* *$" "$BUILD/link.err" | grep -iE 'undefined|unresolved' >/dev/null 2>&1
  # z/OS binder names the symbol on the 'UNRESOLVED' / 'IEW' lines; grab identifiers.
  grep -oE '\b[A-Za-z_][A-Za-z0-9_]{2,}\b' "$BUILD/link.err" | sort -u \
    | grep -viE '^(IEW|ERROR|WARNING|the|was|not|found|section|external|symbol|reference|during|processing)$' > "$BUILD/z-undef.txt"
  echo "-- stubbing $(wc -l < "$BUILD/z-undef.txt") unresolved symbols (verify none are on the parser path)"
  { echo "/* generated no-op stubs for symbols off the chunk-parser path */"
    while read s; do [ -n "$s" ] && echo "long $s() { return 0; }"; done < "$BUILD/z-undef.txt"; } > "$HERE/chunk640-stubs-zos.c"
  $CC -c $CFLAGS "$HERE/chunk640-stubs-zos.c" -o "$OBJ/chunk640-stubs.o"
  link "$OBJ/chunk640-stubs.o" || { echo "link still failing -- see $BUILD/link.err (first-run tweak expected)"; tail -20 "$BUILD/link.err"; exit 1; }
fi
echo "   linked: $BUILD/chunk640-harness"
echo
echo "== self-test: feed real chunked requests to the REAL #640 parser =="
echo "   (EBCDIC request literals -> ASCII wire bytes in-process; body shown re-widened to EBCDIC)"
"$BUILD/chunk640-harness" --self
rc=$?
echo
[ "$rc" = "0" ] && echo "== z/OS #640 VERIFIED ==" || echo "== z/OS #640 FAILED (rc=$rc) =="
echo
echo "(optional socket mode, if you want to drive it from node over a real TCP connection:"
echo "   $BUILD/chunk640-harness $PORT   then a node net.Socket client -- see chunk640-harness.c)"
exit $rc
