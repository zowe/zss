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

export _LD_SYSLIB="//'SYS1.CSSLIB'://'CEE.SCEELKEX'://'CEE.SCEELKED'://'CEE.SCEERUN'://'CEE.SCEERUN2'://'CSF.SCSFMOD0'"

WORKING_DIR=$(dirname "$0")
ZSS="../.."
COMMON="../../deps/zowe-common-c"


echo "********************************************************************************"
echo "Building ZIS Dynamic Linkage Base plugin..."

mkdir -p "${WORKING_DIR}/tmp-zisdyn" && cd "$_"

IFS='.' read -r major minor micro < "${ZSS}/version.txt"
date_stamp=$(date +%Y%m%d)
echo "Version: $major.$minor.$micro"
echo "Date stamp: $date_stamp"

xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX=\"ZWE\" \
  -DZISDYN_MAJOR_VERSION="$major" \
  -DZISDYN_MINOR_VERSION="$minor" \
  -DZISDYN_REVISION="$micro" \
  -DZISDYN_VERSION_DATE_STAMP="$date_stamp" \
  -qreserved_reg=r12 \
  -DRCVR_CPOOL_STATES \
  -DHAVE_METALIO=1 \
  -Wc,arch\(8\),agg,exp,list\(\),so\(\),off,xref,roconst,longname,lp64 \
  -I ${COMMON}/h -I ${ZSS}/h \
  ${COMMON}/c/alloc.c \
  ${COMMON}/c/cellpool.c \
  ${COMMON}/c/cmutils.c \
  ${COMMON}/c/collections.c \
  ${COMMON}/c/cpool64.c \
  ${COMMON}/c/crossmemory.c \
  ${COMMON}/c/logging.c \
  ${COMMON}/c/metalio.c \
  ${COMMON}/c/qsam.c \
  ${COMMON}/c/timeutls.c \
  ${COMMON}/c/utils.c \
  ${COMMON}/c/xlate.c \
  ${COMMON}/c/le.c \
  ${COMMON}/c/logging.c \
  ${COMMON}/c/recovery.c \
  ${COMMON}/c/scheduling.c \
  ${COMMON}/c/pause-element.c \
  ${COMMON}/c/shrmem64.c \
  ${COMMON}/c/zos.c \
  ${COMMON}/c/zvt.c \
  ${COMMON}/c/nametoken.c \
  ${COMMON}/c/lpa.c \
  ${COMMON}/c/mtlskt.c \
  ${COMMON}/c/stcbase.c \
  ${COMMON}/c/isgenq.c \
  ${COMMON}/c/resmgr.c \
  ${ZSS}/c/zis/plugin.c \
  ${ZSS}/c/zis/server-api.c \
  ${ZSS}/c/zis/service.c \
  ${ZSS}/c/zis/client.c \
  ${ZSS}/c/zis/parm.c \
  ${ZSS}/c/zis/zisdynamic.c

for file in \
    alloc cellpool cmutils collections cpool64 crossmemory isgenq le logging lpa metalio mtlskt nametoken \
    pause-element qsam recovery resmgr scheduling shrmem64 stcbase timeutls utils xlate \
    zos zvt \
    client parm plugin server-api service \
    zisdynamic
do
  as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=${file}.asm ${file}.s
done


ld -V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus -e getPluginDescriptor \
-o "//'$USER.DEV.OMDS.TEST.LOADLIB(ZWESISDL)'" \
alloc.o \
cellpool.o \
cmutils.o \
collections.o \
cpool64.o \
crossmemory.o \
isgenq.o \
le.o \
logging.o \
lpa.o \
metalio.o \
mtlskt.o \
nametoken.o \
pause-element.o \
qsam.o \
recovery.o \
scheduling.o \
shrmem64.o \
stcbase.o \
timeutls.o \
utils.o \
xlate.o \
zos.o \
zvt.o \
resmgr.o \
client.o \
parm.o \
plugin.o \
server-api.o \
service.o \
zisdynamic.o \
> ZWESISDL.link

echo "Build successful"
