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

. ${ZSS}/build/zis.proj.env
OLDIFS=$IFS
IFS="."
for part in ${VERSION}; do
  if [ -z "$major" ]; then
    major=$part
  elif [ -z "$minor" ]; then
    minor=$part
  else
    micro=$part
  fi
done
IFS=$OLDIFS

date_stamp=$(date +%Y%m%d)
echo "Version: $major.$minor.$micro"
echo "Date stamp: $date_stamp"

xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX=\"ZWE\" \
  -DZIS_MAJOR_VERSION="$major" \
  -DZIS_MINOR_VERSION="$minor" \
  -DZIS_REVISION="$micro" \
  -DZISDYN_MAJOR_VERSION="$major" \
  -DZISDYN_MINOR_VERSION="$minor" \
  -DZISDYN_REVISION="$micro" \
  -DZISDYN_VERSION_DATE_STAMP="$date_stamp" \
  -DZISDYN_PLUGIN_VERSION=${DYNLINK_PLUGIN_VERSION} \
  -DLPA_LOG_DEBUG_MSG_ID=\"ZWES0100I\" \
  -DMODREG_LOG_DEBUG_MSG_ID=\"ZWES0100I\" \
  -qreserved_reg=r12 \
  -DRCVR_CPOOL_STATES \
  -DHAVE_METALIO=1 \
  -Wc,langlvl\(extc99\),arch\(8\),agg,exp,list\(\),so\(\),off,xref,roconst,longname,lp64 \
  -I ${COMMON}/h -I ${ZSS}/h \
  -I ${ZSS}/zis-aux/include -I ${ZSS}/zis-aux/src \
  -I /usr/include/zos \
  ${ZSS}/c/zis/zisdynamic.c \
  ${ZSS}/c/zis/server-api.c \
  ${ZSS}/c/zis/client.c \
  ${ZSS}/c/zis/parm.c \
  ${ZSS}/c/zis/plugin.c \
  ${ZSS}/c/zis/service.c \
  ${ZSS}/c/zis/service.c \
  ${ZSS}/zis-aux/src/aux-guest.c \
  ${ZSS}/zis-aux/src/aux-manager.c \
  ${ZSS}/zis-aux/src/aux-utils.c \
  ${COMMON}/c/alloc.c \
  ${COMMON}/c/as.c \
  ${COMMON}/c/mtlskt.c \
  ${COMMON}/c/cellpool.c \
  ${COMMON}/c/cmutils.c \
  ${COMMON}/c/collections.c \
  ${COMMON}/c/cpool64.c \
  ${COMMON}/c/crossmemory.c \
  ${COMMON}/c/dynalloc.c \
  ${COMMON}/c/idcams.c \
  ${COMMON}/c/isgenq.c \
  ${COMMON}/c/le.c \
  ${COMMON}/c/logging.c \
  ${COMMON}/c/lpa.c \
  ${COMMON}/c/metalio.c \
  ${COMMON}/c/modreg.c \
  ${COMMON}/c/nametoken.c \
  ${COMMON}/c/pause-element.c \
  ${COMMON}/c/pc.c \
  ${COMMON}/c/qsam.c \
  ${COMMON}/c/radmin.c \
  ${COMMON}/c/recovery.c \
  ${COMMON}/c/resmgr.c \
  ${COMMON}/c/scheduling.c \
  ${COMMON}/c/shrmem64.c \
  ${COMMON}/c/stcbase.c \
  ${COMMON}/c/timeutls.c \
  ${COMMON}/c/utils.c \
  ${COMMON}/c/vsam.c \
  ${COMMON}/c/xlate.c \
  ${COMMON}/c/zos.c \
  ${COMMON}/c/zvt.c \


for file in \
    zisdynamic \
    server-api \
    client \
    parm \
    plugin \
    service \
    service \
    aux-guest \
    aux-manager \
    aux-utils \
    alloc \
    as \
    mtlskt \
    cellpool \
    cmutils \
    collections \
    cpool64 \
    crossmemory \
    dynalloc \
    idcams \
    isgenq \
    le \
    logging \
    lpa \
    metalio \
    modreg \
    nametoken \
    pause-element \
    pc \
    qsam \
    radmin \
    recovery \
    resmgr \
    scheduling \
    shrmem64 \
    stcbase \
    timeutls \
    utils \
    vsam \
    xlate \
    zos \
    zvt
do
  as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=${file}.asm ${file}.s
done


ld -V -b rent -b case=mixed -b map -b xref -b reus -e getPluginDescriptor \
-o "//'${USER}.DEV.LOADLIB(ZWESISDL)'" \
zisdynamic.o \
server-api.o \
client.o \
parm.o \
plugin.o \
service.o \
service.o \
aux-guest.o \
aux-manager.o \
aux-utils.o \
alloc.o \
as.o \
mtlskt.o \
cellpool.o \
cmutils.o \
collections.o \
cpool64.o \
crossmemory.o \
dynalloc.o \
idcams.o \
isgenq.o \
le.o \
logging.o \
lpa.o \
metalio.o \
modreg.o \
nametoken.o \
pause-element.o \
pc.o \
qsam.o \
radmin.o \
recovery.o \
resmgr.o \
scheduling.o \
shrmem64.o \
stcbase.o \
timeutls.o \
utils.o \
vsam.o \
xlate.o \
zos.o \
zvt.o \
> ZWESISDL.link

echo "Build successful"
