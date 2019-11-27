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
echo "Building ZIS..."

mkdir -p "${WORKING_DIR}/tmp-zis" && cd "$_"
xlc -S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX=\"ZWE\" \
  -DRADMIN_XMEM_MODE \
  -qreserved_reg=r12 \
  -Wc,arch\(8\),agg,exp,list\(\),so\(\),off,xref,roconst,longname,lp64 \
  -I ${COMMON}/h -I ${ZSS}/h -I ${ZSS}/zis-aux/include -I ${ZSS}/zis-aux/src \
   ${COMMON}/c/alloc.c \
   ${COMMON}/c/as.c \
   ${COMMON}/c/cmutils.c \
   ${COMMON}/c/collections.c \
   ${COMMON}/c/crossmemory.c \
   ${COMMON}/c/isgenq.c \
   ${COMMON}/c/le.c \
   ${COMMON}/c/logging.c \
   ${COMMON}/c/lpa.c \
   ${COMMON}/c/metalio.c \
   ${COMMON}/c/mtlskt.c \
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
   ${COMMON}/c/xlate.c \
   ${COMMON}/c/zos.c \
   ${COMMON}/c/zvt.c \
   ${ZSS}/c/zis/parm.c \
   ${ZSS}/c/zis/plugin.c \
   ${ZSS}/c/zis/server.c \
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
    alloc as cmutils collections crossmemory isgenq le logging lpa metalio mtlskt nametoken \
    pause-element pc qsam radmin recovery resmgr scheduling shrmem64 stcbase timeutls utils xlate \
    zos zvt \
    parm plugin server service \
    auth nwm snarfer secmgmt tssparsing secmgmttss secmgmtUtils \
    aux-utils aux-host
do
  as -mgoff -mobject -mflag=nocont --TERM --RENT -aegimrsx=${file}.asm ${file}.s
done


ld -V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus \
  -e main -o "//'${USER}.DEV.LOADLIB(ZWESIS01)'" \
  alloc.o \
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

echo "Build successfull"
