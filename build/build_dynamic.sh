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


echo "********************************************************************************"
echo "Building ZIS Dynamic Linkage Base plugin..."

mkdir -p "${WORKING_DIR}/tmp-zisdyn" && cd "$_"

. ${ZSS}/build/zis.proj.env
. ${ZSS}/build//dependencies.sh
check_dependencies "${ZSS}" "${ZSS}/build/zis.proj.env"
DEPS_DESTINATION=$(get_destination "${ZSS}" "${PROJECT}")


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
  -qreserved_reg=r12 \
  -DRCVR_CPOOL_STATES \
  -DHAVE_METALIO=1 \
  -Wc,langlvl\(extc99\),arch\(8\),agg,exp,list\(\),so\(\),off,xref,roconst,longname,lp64 \
  -I ${DEPS_DESTINATION}/${ZCC}/h -I ${ZSS}/h \
  -I ${ZSS}/zis-aux/include -I ${ZSS}/zis-aux/src \
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
  ${DEPS_DESTINATION}/${ZCC}/c/alloc.c \
  ${DEPS_DESTINATION}/${ZCC}/c/as.c \
  ${DEPS_DESTINATION}/${ZCC}/c/mtlskt.c \
  ${DEPS_DESTINATION}/${ZCC}/c/cellpool.c \
  ${DEPS_DESTINATION}/${ZCC}/c/cmutils.c \
  ${DEPS_DESTINATION}/${ZCC}/c/collections.c \
  ${DEPS_DESTINATION}/${ZCC}/c/cpool64.c \
  ${DEPS_DESTINATION}/${ZCC}/c/crossmemory.c \
  ${DEPS_DESTINATION}/${ZCC}/c/dynalloc.c \
  ${DEPS_DESTINATION}/${ZCC}/c/idcams.c \
  ${DEPS_DESTINATION}/${ZCC}/c/isgenq.c \
  ${DEPS_DESTINATION}/${ZCC}/c/le.c \
  ${DEPS_DESTINATION}/${ZCC}/c/logging.c \
  ${DEPS_DESTINATION}/${ZCC}/c/lpa.c \
  ${DEPS_DESTINATION}/${ZCC}/c/metalio.c \
  ${DEPS_DESTINATION}/${ZCC}/c/nametoken.c \
  ${DEPS_DESTINATION}/${ZCC}/c/pause-element.c \
  ${DEPS_DESTINATION}/${ZCC}/c/pc.c \
  ${DEPS_DESTINATION}/${ZCC}/c/qsam.c \
  ${DEPS_DESTINATION}/${ZCC}/c/radmin.c \
  ${DEPS_DESTINATION}/${ZCC}/c/recovery.c \
  ${DEPS_DESTINATION}/${ZCC}/c/resmgr.c \
  ${DEPS_DESTINATION}/${ZCC}/c/scheduling.c \
  ${DEPS_DESTINATION}/${ZCC}/c/shrmem64.c \
  ${DEPS_DESTINATION}/${ZCC}/c/stcbase.c \
  ${DEPS_DESTINATION}/${ZCC}/c/timeutls.c \
  ${DEPS_DESTINATION}/${ZCC}/c/utils.c \
  ${DEPS_DESTINATION}/${ZCC}/c/vsam.c \
  ${DEPS_DESTINATION}/${ZCC}/c/xlate.c \
  ${DEPS_DESTINATION}/${ZCC}/c/zos.c \
  ${DEPS_DESTINATION}/${ZCC}/c/zvt.c \


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


ld -V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus -e getPluginDescriptor \
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
