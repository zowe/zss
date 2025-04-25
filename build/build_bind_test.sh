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


export _C89_LSYSLIB="CEE.SCEELKED:SYS1.CSSLIB:CSF.SCSFMOD0"
export _C89_L6SYSLIB="CEE.SCEEBND2:SYS1.CSSLIB:CSF.SCSFMOD0"

WORKING_DIR=$(cd $(dirname "$0") && pwd)
ZSS="../.."
COMMON="../../deps/zowe-common-c"

echo "********************************************************************************"
echo "Building bind test ..."

mkdir -p "${WORKING_DIR}/tmp-bind-test" && cd "$_"

export _C89_ACCEPTABLE_RC=0

c89 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -DAPF_AUTHORIZED=0 \
  -DNEW_CAA_LOCATIONS=1 \
  -Wc,lp64,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) \
  -Wc,agg,exp,list,so\(\),off,xref \
  -Wl,lp64 \
  -I ${COMMON}/h \
  -o ${ZSS}/bin/bind-test \
  ${COMMON}/c/alloc.c \
  ${COMMON}/c/bpxskt.c \
  ${COMMON}/c/collections.c \
  ${COMMON}/c/le.c \
  ${COMMON}/c/logging.c \
  ${COMMON}/c/zos.c \
  ${COMMON}/c/recovery.c \
  ${COMMON}/c/scheduling.c \
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
