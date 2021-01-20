# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.

COMMON=../../../zowe-common-c

CFLAGS=(-S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"'
-DRADMIN_XMEM_MODE
-DCMS_LPA_DEV_MODE
-qreserved_reg=r12
-Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" -I ../../h
-I $COMMON/h)

ASFLAGS=(-mgoff -mobject -mflag=nocont --TERM --RENT)

LDFLAGS=(-V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus)

xlc "${CFLAGS[@]}" \
$COMMON/c/alloc.c \
$COMMON/c/cmutils.c \
$COMMON/c/collections.c \
$COMMON/c/crossmemory.c \
$COMMON/c/isgenq.c \
$COMMON/c/le.c \
$COMMON/c/logging.c \
$COMMON/c/lpa.c -DLPA_LOG_DEBUG_MSG_ID='"ZIS00100I"' \
$COMMON/c/metalio.c \
$COMMON/c/mtlskt.c \
$COMMON/c/nametoken.c \
$COMMON/c/zos.c \
$COMMON/c/qsam.c \
$COMMON/c/radmin.c \
$COMMON/c/recovery.c \
$COMMON/c/resmgr.c \
$COMMON/c/scheduling.c \
$COMMON/c/stcbase.c \
$COMMON/c/timeutls.c \
$COMMON/c/utils.c \
$COMMON/c/xlate.c \
$COMMON/c/zvt.c \
parm.c \
plugin.c \
server.c \
service.c \
services/auth.c \
services/nwm.c \
services/secmgmt.c \
services/snarfer.c

as "${ASFLAGS[@]}" -aegimrsx=alloc.asm alloc.s
as "${ASFLAGS[@]}" -aegimrsx=cmutils.asm cmutils.s
as "${ASFLAGS[@]}" -aegimrsx=collections.asm collections.s
as "${ASFLAGS[@]}" -aegimrsx=crossmemory.asm crossmemory.s
as "${ASFLAGS[@]}" -aegimrsx=isgenq.asm isgenq.s
as "${ASFLAGS[@]}" -aegimrsx=le.asm le.s
as "${ASFLAGS[@]}" -aegimrsx=logging.asm logging.s
as "${ASFLAGS[@]}" -aegimrsx=lpa.asm lpa.s
as "${ASFLAGS[@]}" -aegimrsx=metalio.asm metalio.s
as "${ASFLAGS[@]}" -aegimrsx=mtlskt.asm mtlskt.s
as "${ASFLAGS[@]}" -aegimrsx=nametoken.asm nametoken.s
as "${ASFLAGS[@]}" -aegimrsx=zos.asm zos.s
as "${ASFLAGS[@]}" -aegimrsx=qsam.asm qsam.s
as "${ASFLAGS[@]}" -aegimrsx=radmin.asm radmin.s
as "${ASFLAGS[@]}" -aegimrsx=recovery.asm recovery.s
as "${ASFLAGS[@]}" -aegimrsx=resmgr.asm resmgr.s
as "${ASFLAGS[@]}" -aegimrsx=scheduling.asm scheduling.s
as "${ASFLAGS[@]}" -aegimrsx=stcbase.asm stcbase.s
as "${ASFLAGS[@]}" -aegimrsx=timeutls.asm timeutls.s
as "${ASFLAGS[@]}" -aegimrsx=utils.asm utils.s
as "${ASFLAGS[@]}" -aegimrsx=xlate.asm xlate.s
as "${ASFLAGS[@]}" -aegimrsx=zvt.asm zvt.s

as "${ASFLAGS[@]}" -aegimrsx=parm.asm parm.s
as "${ASFLAGS[@]}" -aegimrsx=plugin.asm plugin.s
as "${ASFLAGS[@]}" -aegimrsx=server.asm server.s
as "${ASFLAGS[@]}" -aegimrsx=service.asm service.s

as "${ASFLAGS[@]}" -aegimrsx=auth.asm auth.s
as "${ASFLAGS[@]}" -aegimrsx=nwm.asm nwm.s
as "${ASFLAGS[@]}" -aegimrsx=snarfer.asm snarfer.s

as "${ASFLAGS[@]}" -aegimrsx=auth.asm auth.s
as "${ASFLAGS[@]}" -aegimrsx=nwm.asm nwm.s
as "${ASFLAGS[@]}" -aegimrsx=secmgmt.asm secmgmt.s
as "${ASFLAGS[@]}" -aegimrsx=snarfer.asm snarfer.s

export _LD_SYSLIB="//'SYS1.CSSLIB'://'CEE.SCEELKEX'://'CEE.SCEELKED'://'CEE.SCEERUN'://'CEE.SCEERUN2'://'CSF.SCSFMOD0'"

ld "${LDFLAGS[@]}" -e main \
-o "//'$USER.DEV.LOADLIB(ZWESIS01)'" \
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
zos.o \
qsam.o \
radmin.o \
recovery.o \
resmgr.o \
scheduling.o \
stcbase.o \
timeutls.o \
utils.o \
xlate.o \
zvt.o \
parm.o \
plugin.o \
server.o \
service.o \
auth.o \
nwm.o \
secmgmt.o \
snarfer.o \
> ZISSRV01.link


# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.
