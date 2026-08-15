#!/bin/sh
#
# chunk640-test.sh -- WSL end-to-end verification of PR #640
# (zowe-common-c: chunked request-body size-overflow guard in
# processHttpFragment). Drives REAL chunked HTTP requests through the REAL
# parser and checks the reassembled body. See chunk640-harness.c for the why.
#
# Mechanism (all knowledge captured here, nothing hand-typed at run time):
#   1. Start from the WSL-ported common-c tree (repos/zowe-common-c, carrying
#      sandbox-common-c-wsl.patch so httpserver.c compiles under clang).
#   2. Apply the #640 change on top (chunk640-httpserver.patch, straight from
#      the PR); revert it on exit so the shared tree is left as found.
#   3. Rebuild httpserver.o from that tree; compile the harness; link with the
#      rest of build/obj plus auto-generated no-op stubs for symbols the chunk
#      parser never reaches (JWT/TLS/GSK/z-OS services).
#   4. Run curl (single chunk) + a raw multi-chunk sender (exercises #640's
#      carry-forward memcpy) + a liveness probe; optionally under ASan.
#
# Prereq: build.sh has been run once (populates build/obj with the compiled
# common-c/zss objects + OSS deps). CC_PORTED/DEPS overridable as for build.sh.
#
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
WSL="$(cd "$HERE/.." && pwd)"                       # test-support/wsl
ZSS="$(cd "$WSL/../.." && pwd)"                     # repos/zss
CC_PORTED="${CC_PORTED:-$(cd "$ZSS/../zowe-common-c" && pwd)}"
BUILD="$WSL/build"
OBJ="$BUILD/obj"
PATCH="$HERE/chunk640-httpserver.patch"
PORT=8610
CLANG="${CLANG:-clang}"
CFLAGS="-D__ZOWE_OS_LINUX=1 -D_GNU_SOURCE=1 -DNOIBMHTTP=1 -std=gnu99 -fms-extensions -O0 -g"
INC="-I $CC_PORTED/h -I $CC_PORTED/platform/posix"

[ -d "$OBJ" ] || { echo "ERROR: $OBJ missing -- run build.sh first."; exit 1; }
[ -f "$PATCH" ] || { echo "ERROR: $PATCH missing."; exit 1; }

# --- apply #640 on the ported tree, guarantee revert on any exit ------------
applied=0
cleanup() {
  [ "$applied" = "1" ] && git -C "$CC_PORTED" apply -R --include='c/httpserver.c' "$PATCH" 2>/dev/null || true
  [ -n "${HPID:-}" ] && kill "$HPID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "== chunk640-test =="
echo "   ported tree = $CC_PORTED"
if git -C "$CC_PORTED" apply --check --include='c/httpserver.c' "$PATCH" 2>/dev/null; then
  git -C "$CC_PORTED" apply --include='c/httpserver.c' "$PATCH"; applied=1
  echo "   #640 patch applied (will revert on exit)"
elif grep -q 'newContentLength' "$CC_PORTED/c/httpserver.c"; then
  echo "   #640 already present in tree (leaving as-is)"
else
  echo "ERROR: #640 patch will not apply and tree lacks the change."; exit 1
fi

# --- rebuild httpserver.o (now carries #640) + harness ----------------------
echo "-- compiling httpserver.o (with #640) + harness"
$CLANG -c $CFLAGS $INC "$CC_PORTED/c/httpserver.c" -o "$OBJ/httpserver.o"
$CLANG -c $CFLAGS $INC "$HERE/chunk640-harness.c"  -o "$OBJ/chunk640-harness.o"

# --- link: discover undefined -> generate no-op stubs -> relink -------------
LDLIBS="-lpthread -lm -ldl -lcrypto"
REST="$(ls "$OBJ"/*.o | grep -vE 'chunk640-(harness|stubs)\.o')"
if ! $CLANG -o "$BUILD/chunk640-harness" "$OBJ/chunk640-harness.o" $REST $LDLIBS 2>"$BUILD/h-link1.err"; then
  grep -oP "undefined reference to .\K[A-Za-z_][A-Za-z0-9_]*" "$BUILD/h-link1.err" | sort -u > "$BUILD/h-undef.txt"
  echo "-- stubbing $(wc -l < "$BUILD/h-undef.txt") symbols the parser never reaches"
  {
    echo "/* generated no-op stubs -- symbols off the chunk-parser path */"
    while read s; do [ -n "$s" ] && echo "__attribute__((weak)) long $s() { return 0; }"; done < "$BUILD/h-undef.txt"
  } > "$HERE/chunk640-stubs.c"
  $CLANG -c -std=gnu99 -O0 "$HERE/chunk640-stubs.c" -o "$OBJ/chunk640-stubs.o"
  $CLANG -o "$BUILD/chunk640-harness" "$OBJ/chunk640-harness.o" "$OBJ/chunk640-stubs.o" $REST $LDLIBS 2>"$BUILD/h-link2.err"
fi
echo "   linked: $BUILD/chunk640-harness"

# --- run and assert ---------------------------------------------------------
"$BUILD/chunk640-harness" $PORT >"$BUILD/harness.out" 2>&1 & HPID=$!
sleep 0.6
fail=0
check() { # <label> <expected-substr> <actual>
  if printf '%s' "$3" | grep -qF "$2"; then echo "   PASS $1: $3"
  else echo "   FAIL $1: expected '$2', got '$3'"; fail=1; fi
}
t1="$(printf 'AAABBBCCC' | curl -sS --data-binary @- -H 'Transfer-Encoding: chunked' -H 'Expect:' http://127.0.0.1:$PORT/)"
check "curl single-chunk"    "contentLength=9 body=AAABBBCCC"    "$t1"
t2="$(python3 -c '
import socket
s=socket.create_connection(("127.0.0.1",'"$PORT"'))
s.sendall(b"POST /e HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nAAA\r\n3\r\nBBB\r\n3\r\nCCC\r\n0\r\n\r\n")
print(s.recv(1024).decode(errors="replace").strip().splitlines()[-1]); s.close()')"
check "raw multi-chunk"      "contentLength=9 body=AAABBBCCC"    "$t2"
t3="$(printf 'hello#640!' | curl -sS --data-binary @- -H 'Transfer-Encoding: chunked' -H 'Expect:' http://127.0.0.1:$PORT/)"
check "liveness after loads" "contentLength=10 body=hello#640!" "$t3"

echo
[ "$fail" = "0" ] && echo "== RESULT: PASS (real #640 code reassembles chunked bodies end-to-end) ==" \
                  || echo "== RESULT: FAIL =="
exit $fail
