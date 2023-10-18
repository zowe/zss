# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.

COMMON=../../../../../zowe-common-c
ZSS=../../../../../zss
ZSSAUX=../../../

CFLAGS=(-S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"'
-qreserved_reg=r12
-Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64"
-I $COMMON/h -I $ZSS/h -I $ZSSAUX/src -I $ZSSAUX/include)

ASFLAGS=(-mgoff -mobject -mflag=nocont --TERM --RENT)

LDFLAGS=(-V -b rent -b case=mixed -b map -b xref -b reus)

xlc "${CFLAGS[@]}" \
$COMMON/c/alloc.c \
$COMMON/c/collections.c \
$COMMON/c/le.c \
$COMMON/c/logging.c \
$COMMON/c/metalio.c \
$COMMON/c/qsam.c \
$COMMON/c/recovery.c \
$COMMON/c/scheduling.c \
$COMMON/c/timeutls.c \
$COMMON/c/utils.c \
$COMMON/c/zos.c \
$ZSSAUX/src/aux-guest.c \
$ZSSAUX/test/aux-guest/src/testguest.c \

as "${ASFLAGS[@]}" -aegimrsx=alloc.asm alloc.s
as "${ASFLAGS[@]}" -aegimrsx=collections.asm collections.s
as "${ASFLAGS[@]}" -aegimrsx=le.asm le.s
as "${ASFLAGS[@]}" -aegimrsx=logging.asm logging.s
as "${ASFLAGS[@]}" -aegimrsx=metalio.asm metalio.s
as "${ASFLAGS[@]}" -aegimrsx=qsam.asm qsam.s
as "${ASFLAGS[@]}" -aegimrsx=recovery.asm recovery.s
as "${ASFLAGS[@]}" -aegimrsx=scheduling.asm scheduling.s
as "${ASFLAGS[@]}" -aegimrsx=timeutls.asm timeutls.s
as "${ASFLAGS[@]}" -aegimrsx=utils.asm utils.s
as "${ASFLAGS[@]}" -aegimrsx=zos.asm zos.s

as "${ASFLAGS[@]}" -aegimrsx=aux-guest.asm aux-guest.s
as "${ASFLAGS[@]}" -aegimrsx=testguest.asm testguest.s

export _LD_SYSLIB="//'SYS1.CSSLIB'://'CEE.SCEELKEX'://'CEE.SCEELKED'://'CEE.SCEERUN'://'CEE.SCEERUN2'://'CSF.SCSFMOD0'"

ld "${LDFLAGS[@]}" -e getModuleDescriptor \
-o "//'$USER.DEV.LOADLIB(ZWESAUXG)'" \
alloc.o \
collections.o \
le.o \
logging.o \
metalio.o \
qsam.o \
recovery.o \
scheduling.o \
timeutls.o \
utils.o \
zos.o \
aux-guest.o \
testguest.o \
> ZWESAUXG.link

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.

