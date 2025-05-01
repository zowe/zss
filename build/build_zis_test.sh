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
echo "Building zis check ..."

mkdir -p "${WORKING_DIR}/tmp-zis-test" && cd "$_"

c89 \
  -D_XOPEN_SOURCE=600 \
  -DCMS_CLIENT \
  -DNEW_CAA_LOCATIONS=1 \
  -Wc,lp64,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('SYS1.MACLIB'\) \
  -Wc,agg,exp,list,so\(\),off,xref \
  -Wl,lp64 \
  -I ${COMMON}/h \
  -I ${ZSS}/h \
  -o ${ZSS}/bin/zis-test \
  ${COMMON}/c/alloc.c \
  ${COMMON}/c/crossmemory.c \
  ${COMMON}/c/zos.c \
  ${COMMON}/c/timeutls.c \
  ${COMMON}/c/utils.c \
  ${COMMON}/c/zvt.c \
  ${ZSS}/c/zisTest.c ;

rc=$?
if [ $rc -eq 0 ]; then
  echo "Build zis-test successfully"
  exit 0
else
  # remove zisTest in case the linker had RC=4 and produced the binary
  rm -f ${ZSS}/bin/zis-test
  echo "Build zis-test failed"
  exit 8
fi
