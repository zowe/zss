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

WORKING_DIR=$(dirname "$0")
ZSS="../.."
COMMON="../../deps/zowe-common-c"

echo "********************************************************************************"
echo "Building ZIS core services SAF test..."

mkdir -p "${WORKING_DIR}/tmp-zissaft"
cd ${WORKING_DIR}/tmp-zissaft

xlc -DCMS_CLIENT -D_OPEN_THREADS=1 \
  "-Wc,langlvl(extc99),asm,asmlib('SYS1.MACLIB'),asmlib('CEE.SCEEMAC')" \
  -I ${COMMON}/h -I ${ZSS}/h \
 ../zis-saf-test.c \
 ${COMMON}/c/crossmemory.c \
 ${COMMON}/c/zos.c \
 ${COMMON}/c/zvt.c \
 ${COMMON}/c/timeutls.c \
 ${COMMON}/c/alloc.c \
 ${COMMON}/c/utils.c \
 ${COMMON}/c/le.c \
 ${COMMON}/c/recovery.c \
 ${COMMON}/c/scheduling.c \
 ${COMMON}/c/collections.c \
 ${COMMON}/c/logging.c \
 -o zis-saf-test

echo "Build successful"
