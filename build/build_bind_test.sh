#!/bin/sh
set -e

################################################################################
#  This program and the accompanying materials are
#  made available under the terms of the Eclipse Public License v2.0 which accompanies
#  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
#  SPDX-License-Identifier: EPL-2.0
#
#  Copyright Contributors to the Zowe Project.
################################################################################

WORKING_DIR=$(cd $(dirname "$0") && pwd)
ZSS="../.."
COMMON="../../deps/zowe-common-c"

echo "********************************************************************************"
echo "Building bind test ..."

mkdir -p "${WORKING_DIR}/tmp-bind-test" && cd "$_"

xlclang \
  -q64 \
  -v \
  -qascii \
  -D_XOPEN_SOURCE=600 \
  -DNEW_CAA_LOCATIONS=1 \
  "-Wc,langlvl(extc11),gonum,goff,hgpr,roconst,ASM,asmlib('SYS1.MACLIB')" \
  -I ${COMMON}/h \
  -o ${ZSS}/bin/bind-test \
  ${COMMON}/c/alloc.c \
  ${COMMON}/c/bpxskt.c \
  ${COMMON}/c/timeutls.c \
  ${COMMON}/c/utils.c \
  ${ZSS}/c/bindTest.c ;

rc=$?
if [ $rc -eq 0 ]; then
  echo "Build bind-test successfully"
  exit 0
else
  # remove bindTest in case the linker had RC=4 and produced the binary
  rm -f ${ZSS}/bin/bind-test
  echo "Build bind-test failed"
  exit 8
fi
