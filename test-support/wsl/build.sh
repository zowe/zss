#!/bin/sh
#
# build.sh -- build zssServer natively on Linux/WSL with clang, for a
# zero-cost dev/test sandbox (reproduce #828, run sanitizers/fuzzers, iterate
# without touching a metered z/OS system). NOT a production build.
#
# Strategy: compile every candidate translation unit; the z/OS-only ones fail
# to compile (BPX/ICSF/gsk headers) and self-exclude, leaving the portable set.
# Link what compiled + the OSS deps + wsl-stubs.o; the linker then enumerates
# any remaining undefined symbols, which we satisfy with explicit stubs rather
# than by dragging in z/OS machinery.
#
# Prereqs: clang, plus zoweclang's already-ported deps (quickjs-portable +
# libyaml) at $DEPS below. See test-support/wsl/README.md.
#
set -u

# ---- paths ----------------------------------------------------------------
HERE="$(cd "$(dirname "$0")" && pwd)"
ZSS="$(cd "$HERE/../.." && pwd)"                    # .../repos/zss
CC_COMMON="${CC_COMMON:-$(cd "$ZSS/../zowe-common-c" && pwd)}"    # sibling zowe-common-c; override with CC_COMMON=
# QuickJS + libyaml sources. zowe-common-c fetches these under
# deps/configmgr via build/dependencies.sh; point DEPS at that dir. Override
# with DEPS=/path/to/zowe-common-c/deps/configmgr for your checkout.
DEPS="${DEPS:-$CC_COMMON/deps/configmgr}"
# Optional: a dir of prebuilt libyaml+quickjs .o to reuse instead of compiling
# them from source. Empty by default; set DEPS_PREBUILT=/path if you have them.
DEPS_PREBUILT="${DEPS_PREBUILT:-}"
BUILD="${BUILD_DIR:-$HERE/build}"
OBJ="$BUILD/obj"
BIN="$BUILD/zssServer"

CLANG="${CLANG:-clang}"

# ---- flags (mirror zoweclang's native-linux configmgr recipe) -------------
DEFS="-D__ZOWE_OS_LINUX=1 -D_GNU_SOURCE=1 -DNOIBMHTTP=1"
# -fms-extensions: anonymous member of a named union/struct (HttpConversation
#   uses it; z/OS xlclang enables this by default, clang needs the flag).
CFLAGS="$DEFS -std=gnu99 -fms-extensions -O0 -g -fno-omit-frame-pointer -Wno-implicit-function-declaration"
INC="-I $ZSS/h -I $CC_COMMON/h -I $CC_COMMON/platform/posix -I $CC_COMMON/jwt/jwt \
     -I $DEPS/libyaml/include -I $DEPS/quickjs"
LDLIBS="-lpthread -lm -ldl -lcrypto"  # -lcrypto: OpenSSL MD5_*/SHA1_* (crypto.c non-z/OS path); also HS256 later

# ASAN=1 -> AddressSanitizer (memory-bug detection + clean backtraces). The
# reused OSS-dep objects aren't instrumented, which is fine for linking.
if [ "${ASAN:-0}" = "1" ]; then
  CFLAGS="$CFLAGS -fsanitize=address"
  LDLIBS="$LDLIBS -fsanitize=address"
  echo "   (AddressSanitizer ON)"
fi

mkdir -p "$OBJ"
: > "$BUILD/compiled.txt"; : > "$BUILD/excluded.txt"

echo "== zssServer WSL build =="
echo "   zss       = $ZSS"
echo "   common-c  = $CC_COMMON"
echo "   deps      = $DEPS"
echo "   build dir = $BUILD"
echo

# ---- 1. OSS deps (libyaml + quickjs) --------------------------------------
# Reuse zoweclang's prebuilt Linux objects if present; they are the exact
# libyaml+quickjs .o that its working configmgr links.
if ls "$DEPS_PREBUILT"/*.o >/dev/null 2>&1; then
  echo "-- deps: reusing prebuilt objects from $DEPS_PREBUILT"
  cp "$DEPS_PREBUILT"/*.o "$OBJ"/
else
  echo "-- deps: prebuilt objects not found; building libyaml+quickjs from source"
  YAML_DEFS="-DYAML_VERSION_MAJOR=0 -DYAML_VERSION_MINOR=2 -DYAML_VERSION_PATCH=5 \
             -DYAML_VERSION_STRING='\"0.2.5\"' -DYAML_DECLARE_STATIC=1"
  QJS_DEFS="-DCONFIG_BIGNUM=1 -DCONFIG_VERSION='\"2021-03-27\"' -D_GNU_SOURCE=1"
  for y in api reader scanner parser loader writer emitter dumper; do
    $CLANG -c $CFLAGS $YAML_DEFS -I "$DEPS/libyaml/include" \
        "$DEPS/libyaml/src/$y.c" -o "$OBJ/$y.o" 2>>"$BUILD/deps.err" \
        && echo "   yaml/$y.o" || echo "   FAILED yaml/$y"
  done
  for q in cutils quickjs quickjs-libc libunicode libbf libregexp; do
    $CLANG -c $CFLAGS $QJS_DEFS -I "$DEPS/quickjs" \
        "$DEPS/quickjs/$q.c" -o "$OBJ/$q.o" 2>>"$BUILD/deps.err" \
        && echo "   quickjs/$q.o" || echo "   FAILED quickjs/$q"
  done
fi
echo

# ---- 2. compile sweep -----------------------------------------------------
# Try every candidate TU. Successes land in $OBJ; failures (z/OS-only files)
# are recorded and skipped. This is the automatic portable/z/OS partition.
echo "-- compile sweep (z/OS-only TUs will self-exclude by failing to compile)"
compile_one() {
  src="$1"; base="$(basename "${src%.c}")"
  if $CLANG -c $CFLAGS $INC "$src" -o "$OBJ/$base.o" 2>"$OBJ/$base.err"; then
    echo "$base" >> "$BUILD/compiled.txt"
  else
    first="$(grep -m1 'error:' "$OBJ/$base.err" | sed 's/^[^ ]* //' | cut -c1-70)"
    printf '%-24s %s\n' "$base" "$first" >> "$BUILD/excluded.txt"
    rm -f "$OBJ/$base.o"
  fi
}
for src in "$CC_COMMON"/c/*.c "$CC_COMMON"/platform/posix/*.c "$ZSS"/c/*.c; do
  compile_one "$src"
done
# optional hand-written Linux stubs for excluded-symbol satisfaction
[ -f "$HERE/wsl-stubs.c" ] && $CLANG -c $CFLAGS $INC "$HERE/wsl-stubs.c" -o "$OBJ/wsl-stubs.o" 2>"$OBJ/wsl-stubs.err"

nc=$(wc -l < "$BUILD/compiled.txt"); nx=$(wc -l < "$BUILD/excluded.txt")
echo "   compiled: $nc TUs   excluded: $nx TUs"
echo "   (excluded list -> $BUILD/excluded.txt)"
echo

# ---- 3. link --------------------------------------------------------------
echo "-- link zssServer"
if $CLANG -o "$BIN" "$OBJ"/*.o $LDLIBS 2>"$BUILD/link.err"; then
  echo "   LINKED: $BIN"
  file "$BIN"
else
  echo "   link incomplete -- undefined symbols (candidates for wsl-stubs.c or inclusion):"
  grep -oE "undefined reference to \`[^']+'" "$BUILD/link.err" | sed 's/.*`/     /; s/.$//' | sort -u | head -80
  echo
  echo "   (full linker errors -> $BUILD/link.err)"
fi
