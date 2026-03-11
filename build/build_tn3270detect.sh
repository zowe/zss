#!/bin/sh
set -e

################################################################################
#  This program and the accompanying materials are
#  made available under the terms of the Eclipse Public License v2.0 which
#  accompanies this distribution, and is available at
#  https://www.eclipse.org/legal/epl-v20.html
#
#  SPDX-License-Identifier: EPL-2.0
#
#  Copyright Contributors to the Zowe Project.
################################################################################
#
# build_tn3270detect.sh
#
# Builds the tn3270detect utility – a CLI tool that opens a TCP socket to
# detect whether the remote endpoint is a TN3270 Telnet server as defined
# by RFC 1041 (Telnet 3270 Regime Option).
#
# Usage (z/OS, using xlclang):
#   ./build_tn3270detect.sh
#
# The resulting binary is placed at:
#   <zss-root>/bin/tn3270detect
#
# Dependencies:
#   - xlclang (IBM z/OS XL C/C++ compiler)
#   - z/OS UNIX System Services socket libraries (linked automatically)
#
################################################################################

WORKING_DIR=$(cd "$(dirname "$0")" && pwd)
ZSS_ROOT="${WORKING_DIR}/.."
ZSS="../.."

echo "********************************************************************************"
echo "Building tn3270detect ..."
echo "Working dir : ${WORKING_DIR}"
echo "ZSS root    : ${ZSS_ROOT}"

mkdir -p "${WORKING_DIR}/tmp-tn3270detect" && cd "$_"

# ---------------------------------------------------------------------------
# z/OS: compile and link in a single xlclang invocation.
#
# Flags explained:
#   -q64           – 64-bit addressing mode
#   float(ieee)    – IEEE floating-point (harmless for this program)
#   longname       – allow identifiers longer than 8 characters
#   langlvl(extc99)– C99 with IBM extensions (needed for //comments, VLAs…)
#   gonum          – generate line number information for debugging
#   goff           – GOFF object format
#   -D_OPEN_SYS_FILE_EXT=1  – expose z/OS UNIX extensions
#   -D_XOPEN_SOURCE=600     – POSIX.1-2004 / SUSv3 (getaddrinfo, etc.)
#   -D_OPEN_THREADS=1       – enable threading-safe APIs
# ---------------------------------------------------------------------------
COMMON="${ZSS_ROOT}/deps/zowe-common-c"

GSKDIR=/usr/lpp/gskssl
GSKINC="${GSKDIR}/include"
GSKLIB="${GSKDIR}/lib/GSKSSL64.x ${GSKDIR}/lib/GSKCMS64.x"

xlclang \
  -q64 \
  "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" \
  -D_OPEN_SYS_FILE_EXT=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -DUSE_ZOWE_TLS=1 \
  -I "${COMMON}/h" \
  -I "${GSKINC}" \
  -o "${ZSS_ROOT}/bin/tn3270detect" \
  "${ZSS_ROOT}/c/tn3270detect.c" \
  "${COMMON}/c/alloc.c" \
  "${COMMON}/c/le.c" \
  "${COMMON}/c/utils.c" \
  "${COMMON}/c/collections.c" \
  "${COMMON}/c/bpxskt.c" \
  "${COMMON}/c/tls.c" \
  "${COMMON}/c/fdpoll.c" \
  "${COMMON}/c/logging.c" \
  "${COMMON}/c/recovery.c" \
  "${COMMON}/c/timeutls.c" \
  "${COMMON}/c/scheduling.c" \
  "${COMMON}/c/zos.c" \
  ${GSKLIB}

rc=$?
if [ $rc -eq 0 ]; then
  echo "Build tn3270detect successful: ${ZSS_ROOT}/bin/tn3270detect"
  exit 0
else
  rm -f "${ZSS_ROOT}/bin/tn3270detect"
  echo "Build tn3270detect FAILED (rc=$rc)"
  exit 8
fi
