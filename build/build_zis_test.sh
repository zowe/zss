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
ZSS_ROOT="$WORKING_DIR/.."
COMMON_BUILD="$WORKING_DIR/../deps/zowe-common-c/build"
ZSS="../.."
COMMON="../../deps/zowe-common-c"



echo "********************************************************************************"
echo "Building zis check ..."



mkdir -p "${WORKING_DIR}/tmp-zis-test" && cd "$_"

date_stamp=$(date +%Y%m%d)
echo "Date stamp: $date_stamp"

export _C89_ACCEPTABLE_RC=0

if ! c89 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -DAPF_AUTHORIZED=0 \
  -DNEW_CAA_LOCATIONS=1 \
  -Wc,lp64,expo,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) \
  -Wc,agg,exp,list,so\(\),off,xref \
  -Wl,lp64 \
  -I ${COMMON}/h \
  -I ${COMMON}/platform/posix \
  -I ${ZSS}/h \
  -o ${ZSS}/bin/zis-test \
  ${COMMON}/c/alloc.c \
  ${COMMON}/c/bpxskt.c \
  ${COMMON}/c/charsets.c \
  ${COMMON}/c/cmutils.c \
  ${COMMON}/c/collections.c \
  ${COMMON}/c/crossmemory.c \
  ${COMMON}/c/dynalloc.c \
  ${COMMON}/c/json.c \
  ${COMMON}/c/le.c \
  ${COMMON}/c/logging.c \
  ${COMMON}/c/zos.c \
  ${COMMON}/c/rawfd.c \
  ${COMMON}/c/recovery.c \
  ${COMMON}/c/scheduling.c \
  ${COMMON}/c/socketmgmt.c \
  ${COMMON}/c/timeutls.c \
  ${COMMON}/c/utils.c \
  ${COMMON}/c/zosfile.c \
  ${COMMON}/c/zvt.c \
  ${COMMON}/c/shrmem64.c \
  ${ZSS}/c/zisTest.c \
  ${ZSS}/c/zis/client.c ;
then
  echo "Build zis-test successfully"
  exit 0
else
  # remove zisTest in case the linker had RC=4 and produced the binary
  rm -f ${ZSS}/bin/zis-test
  echo "Build zis-test failed"
  exit 8
fi
