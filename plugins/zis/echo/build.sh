# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.

COMMON=../../../../zowe-common-c
ZSS=../../../../zss

CFLAGS=(-S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"'
-qreserved_reg=r12
-Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64" 
-I $COMMON/h -I $ZSS/h -I .)

ASFLAGS=(-mgoff -mobject -mflag=nocont --TERM --RENT)

LDFLAGS=(-V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus)

xlc "${CFLAGS[@]}" -DCMS_CLIENT \
$COMMON/c/alloc.c \
$COMMON/c/cmutils.c \
$COMMON/c/crossmemory.c \
$COMMON/c/metalio.c \
$COMMON/c/qsam.c \
$COMMON/c/timeutls.c \
$COMMON/c/utils.c \
$COMMON/c/zos.c \
$COMMON/c/zvt.c \
$ZSS/c/zis/plugin.c \
$ZSS/c/zis/service.c \
$ZSS/plugins/zis/echo/services/echoservice.c \
$ZSS/plugins/zis/echo/main.c \

as "${ASFLAGS[@]}" -aegimrsx=alloc.asm alloc.s
as "${ASFLAGS[@]}" -aegimrsx=cmutils.asm cmutils.s
as -I RSRTE.ROCKET.MACLIB "${ASFLAGS[@]}" -aegimrsx=crossmemory.asm crossmemory.s
as "${ASFLAGS[@]}" -aegimrsx=metalio.asm metalio.s
as "${ASFLAGS[@]}" -aegimrsx=qsam.asm qsam.s
as "${ASFLAGS[@]}" -aegimrsx=timeutls.asm timeutls.s
as "${ASFLAGS[@]}" -aegimrsx=utils.asm utils.s
as "${ASFLAGS[@]}" -aegimrsx=zos.asm zos.s
as "${ASFLAGS[@]}" -aegimrsx=zvt.asm zvt.s
as "${ASFLAGS[@]}" -aegimrsx=plugin.asm plugin.s
as "${ASFLAGS[@]}" -aegimrsx=service.asm service.s

as "${ASFLAGS[@]}" -aegimrsx=echoservice.asm echoservice.s
as "${ASFLAGS[@]}" -aegimrsx=main.asm main.s

export _LD_SYSLIB="//'SYS1.CSSLIB'://'CEE.SCEELKEX'://'CEE.SCEELKED'://'CEE.SCEERUN'://'CEE.SCEERUN2'://'CSF.SCSFMOD0'"

ld "${LDFLAGS[@]}" -e getPluginDescriptor \
-o "//'$USER.DEV.LOADLIB(ECHOPL01)'" \
alloc.o \
cmutils.o \
crossmemory.o \
metalio.o \
qsam.o \
timeutls.o \
utils.o \
zos.o \
zvt.o \
plugin.o \
service.o \
echoservice.o \
main.o \
> ECHOPL01.link

# Test client

xlc "-Wa,goff" \
"-Wc,LANGLVL(EXTC99),FLOAT(HEX),agg,exp,list(),so(),goff,xref,gonum,roconst,gonum,ASM,ASMLIB('SYS1.MACLIB'),LP64,XPLINK" \
-DCMS_CLIENT -D_OPEN_THREADS=1  -I $COMMON/h -I $ZSS/h -o test test.c \
$COMMON/c/alloc.c \
$COMMON/c/cmutils.c \
$COMMON/c/crossmemory.c \
$COMMON/c/timeutls.c \
$COMMON/c/utils.c \
$COMMON/c/zos.c \
$COMMON/c/zvt.c \
$ZSS/c/zis/client.c \


# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.
