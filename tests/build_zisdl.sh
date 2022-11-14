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
echo "Building ZIS Dynamic Linkage test..."

mkdir -p "${WORKING_DIR}/tmp-zisdl"
cd ${WORKING_DIR}/tmp-zisdl

xlc -S -M -qmetal -q64 -DMETTLE=1 \
  -qreserved_reg=r12 \
  -Wc,arch\(8\),agg,exp,list\(\),so\(\),off,xref,roconst,longname,lp64 \
  -I ${COMMON}/h -I ${ZSS}/h ../zisdl.c

cp ${ZSS}/c/zis/stubs.s .

for file in \
    zisdl stubs
do
  as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=${file}.asm ${file}.s
done


ld -V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus -e main \
-o "//'$USER.DEV.LOADLIB(ZISDLTST)'" \
zisdl.o \
stubs.o \
> ZISDLTST.link

echo "Build successful"
