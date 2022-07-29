#!/bin/bash

################################################################################
#  This program and the accompanying materials are
#  made available under the terms of the Eclipse Public License v2.0 which accompanies
#  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
#  SPDX-License-Identifier: EPL-2.0
#
#  Copyright Contributors to the Zowe Project.
################################################################################

COMMON=../deps/zowe-common-c
ZSS=..

CFLAGS=(-S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"'
-DHAVE_METALIO=1
-qreserved_reg=r12
-Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" 
-I $COMMON/h -I $ZSS/h -I $ZSS/h -I .)

ASFLAGS=(-mgoff -mobject -mflag=nocont --TERM --RENT)

LDFLAGS=(-V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus)

xlc "${CFLAGS[@]}" -DRCVR_CPOOL_STATES \
$COMMON/c/alloc.c \
$COMMON/c/cmutils.c \
$COMMON/c/collections.c \
$COMMON/c/cpool64.c \
$COMMON/c/crossmemory.c \
$COMMON/c/logging.c \
$COMMON/c/metalio.c \
$COMMON/c/qsam.c \
$COMMON/c/timeutls.c \
$COMMON/c/utils.c \
$COMMON/c/xlate.c \
$COMMON/c/le.c \
$COMMON/c/logging.c \
$COMMON/c/recovery.c \
$COMMON/c/scheduling.c \
$COMMON/c/pause-element.c \
$COMMON/c/shrmem64.c \
$COMMON/c/zosfile.c \
$COMMON/c/zos.c \
$COMMON/c/zvt.c \
$COMMON/c/cellpool.c \
$COMMON/c/nametoken.c \
$COMMON/c/lpa.c \
$COMMON/c/mtlskt.c \
$COMMON/c/stcbase.c \
$COMMON/c/isgenq.c \
$COMMON/c/resmgr.c \
$ZSS/c/zis/plugin.c \
$ZSS/c/zis/service.c \
$ZSS/c/zis/client.c \
$ZSS/c/zis/parm.c \
$ZSS/c/zis/zisdynamic.c

as "${ASFLAGS[@]}" -aegimrsx=alloc.asm alloc.s
as "${ASFLAGS[@]}" -aegimrsx=cmutils.asm cmutils.s
as "${ASFLAGS[@]}" -aegimrsx=collections.asm collections.s
as "${ASFLAGS[@]}" -aegimrsx=cpool64.asm cpool64.s
as "${ASFLAGS[@]}" -aegimrsx=crossmemory.asm crossmemory.s
as "${ASFLAGS[@]}" -aegimrsx=logging.asm logging.s
as "${ASFLAGS[@]}" -aegimrsx=metalio.asm metalio.s
as "${ASFLAGS[@]}" -aegimrsx=qsam.asm qsam.s
as "${ASFLAGS[@]}" -aegimrsx=timeutls.asm timeutls.s
as "${ASFLAGS[@]}" -aegimrsx=utils.asm utils.s
as "${ASFLAGS[@]}" -aegimrsx=zos.asm zos.s
as "${ASFLAGS[@]}" -aegimrsx=zosfile.asm zosfile.s
as "${ASFLAGS[@]}" -aegimrsx=le.asm le.s
as "${ASFLAGS[@]}" -aegimrsx=recovery.asm recovery.s
as "${ASFLAGS[@]}" -aegimrsx=scheduling.asm scheduling.s
as "${ASFLAGS[@]}" -aegimrsx=pause-element.asm pause-element.s
as "${ASFLAGS[@]}" -aegimrsx=shrmem64.asm shrmem64.s
as "${ASFLAGS[@]}" -aegimrsx=xlate.asm xlate.s
as "${ASFLAGS[@]}" -aegimrsx=zvt.asm zvt.s
as "${ASFLAGS[@]}" -aegimrsx=cellpool.asm cellpool.s
as "${ASFLAGS[@]}" -aegimrsx=nametoken.asm nametoken.s
as "${ASFLAGS[@]}" -aegimrsx=lpa.asm lpa.s
as "${ASFLAGS[@]}" -aegimrsx=mtlskt.asm mtlskt.s
as "${ASFLAGS[@]}" -aegimrsx=stcbase.asm stcbase.s
as "${ASFLAGS[@]}" -aegimrsx=isgenq.asm isgenq.s
as "${ASFLAGS[@]}" -aegimrsx=resmgr.asm resmgr.s
as "${ASFLAGS[@]}" -aegimrsx=plugin.asm plugin.s
as "${ASFLAGS[@]}" -aegimrsx=service.asm service.s
as "${ASFLAGS[@]}" -aegimrsx=client.asm client.s
as "${ASFLAGS[@]}" -aegimrsx=parm.asm parm.s
as "${ASFLAGS[@]}" -aegimrsx=zisdynamic.asm zisdynamic.s

export _LD_SYSLIB="//'SYS1.CSSLIB'://'CEE.SCEELKEX'://'CEE.SCEELKED'://'CEE.SCEERUN'://'CEE.SCEERUN2'://'CSF.SCSFMOD0'"

ld "${LDFLAGS[@]}" -e getPluginDescriptor \
-o "//'$USER.DEV.OMDS.TEST.LOADLIB(ZWESISDL)'" \
alloc.o \
cmutils.o \
collections.o \
cpool64.o \
crossmemory.o \
le.o \
logging.o \
metalio.o \
qsam.o \
pause-element.o \
recovery.o \
shrmem64.o \
scheduling.o \
timeutls.o \
utils.o \
xlate.o \
zos.o \
zosfile.o \
zvt.o \
cellpool.o \
nametoken.o \
lpa.o \
mtlskt.o \
stcbase.o \
isgenq.o \
resmgr.o \
plugin.o \
service.o \
client.o \
parm.o \
zisdynamic.o \
> ZWESISDL.link