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
echo "Building ZIS..."

mkdir -p "${WORKING_DIR}/tmp-zis" && cd "$_"

. ${ZSS}/build/zis.proj.env
. ${ZSS}/build//dependencies.sh
check_dependencies "${ZSS}" "${ZSS}/build/zis.proj.env"
DEPS_DESTINATION=$(get_destination "${ZSS}" "${PROJECT}")


# Split version into parts
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
  -DZIS_VERSION_DATE_STAMP="$date_stamp" \
  -DRADMIN_XMEM_MODE \
  -DRCVR_CPOOL_STATES \
  -qreserved_reg=r12 \
  -Wc,arch\(8\),agg,exp,list\(\),so\(\),off,xref,roconst,longname,lp64 \
  -I ${DEPS_DESTINATION}/${ZCC}/h -I ${ZSS}/h -I ${ZSS}/zis-aux/include -I ${ZSS}/zis-aux/src \
   ${DEPS_DESTINATION}/${ZCC}/c/alloc.c \
   ${DEPS_DESTINATION}/${ZCC}/c/as.c \
   ${DEPS_DESTINATION}/${ZCC}/c/cellpool.c \
   ${DEPS_DESTINATION}/${ZCC}/c/cmutils.c \
   ${DEPS_DESTINATION}/${ZCC}/c/collections.c \
   ${DEPS_DESTINATION}/${ZCC}/c/crossmemory.c \
   ${DEPS_DESTINATION}/${ZCC}/c/isgenq.c \
   ${DEPS_DESTINATION}/${ZCC}/c/le.c \
   ${DEPS_DESTINATION}/${ZCC}/c/logging.c \
   ${DEPS_DESTINATION}/${ZCC}/c/lpa.c \
   ${DEPS_DESTINATION}/${ZCC}/c/metalio.c \
   ${DEPS_DESTINATION}/${ZCC}/c/mtlskt.c \
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
   ${DEPS_DESTINATION}/${ZCC}/c/xlate.c \
   ${DEPS_DESTINATION}/${ZCC}/c/zos.c \
   ${DEPS_DESTINATION}/${ZCC}/c/zvt.c \
   ${ZSS}/c/zis/parm.c \
   ${ZSS}/c/zis/plugin.c \
   ${ZSS}/c/zis/server.c \
   ${ZSS}/c/zis/server-api.c \
   ${ZSS}/c/zis/service.c \
   ${ZSS}/c/zis/services/auth.c \
   ${ZSS}/c/zis/services/nwm.c \
   ${ZSS}/c/zis/services/secmgmt.c \
   ${ZSS}/c/zis/services/snarfer.c \
   ${ZSS}/c/zis/services/tssparsing.c \
   ${ZSS}/c/zis/services/secmgmttss.c \
   ${ZSS}/c/zis/services/secmgmtUtils.c \
   ${ZSS}/zis-aux/src/aux-utils.c \
   ${ZSS}/zis-aux/src/aux-host.c

for file in \
    alloc as cellpool cmutils collections crossmemory isgenq le logging lpa metalio mtlskt nametoken \
    pause-element pc qsam radmin recovery resmgr scheduling shrmem64 stcbase timeutls utils xlate \
    zos zvt \
    parm plugin server server-api service \
    auth nwm snarfer secmgmt tssparsing secmgmttss secmgmtUtils \
    aux-utils aux-host
do
  as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=${file}.asm ${file}.s
done


ld -V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus \
  -e main -o "//'${USER}.DEV.LOADLIB(ZWESIS01)'" \
  alloc.o \
  cellpool.o \
  cmutils.o \
  collections.o \
  crossmemory.o \
  isgenq.o \
  le.o \
  logging.o \
  lpa.o \
  metalio.o \
  mtlskt.o \
  nametoken.o \
  qsam.o \
  radmin.o \
  recovery.o \
  resmgr.o \
  scheduling.o \
  stcbase.o \
  timeutls.o \
  utils.o \
  xlate.o \
  zos.o \
  zvt.o \
  parm.o \
  plugin.o \
  server.o \
  server-api.o \
  service.o \
  auth.o \
  nwm.o \
  secmgmt.o \
  snarfer.o \
  tssparsing.o \
  secmgmttss.o \
  secmgmtUtils.o \
  > ZWESIS01.link

ld -V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus \
  -e main -o "//'${USER}.DEV.LOADLIB(ZWESAUX)'" \
  alloc.o \
  as.o \
  cellpool.o \
  cmutils.o \
  collections.o \
  crossmemory.o \
  isgenq.o \
  le.o \
  logging.o \
  lpa.o \
  metalio.o \
  mtlskt.o \
  nametoken.o \
  pause-element.o \
  pc.o \
  qsam.o \
  recovery.o \
  resmgr.o \
  scheduling.o \
  shrmem64.o \
  stcbase.o \
  timeutls.o \
  utils.o \
  xlate.o \
  zos.o \
  zvt.o \
  parm.o \
  aux-utils.o \
  aux-host.o \
  > ZWESAUX.link

echo "Build successful"
