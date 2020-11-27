# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.

ZSSAUX=../../../
COMMON=../../../../../zowe-common-c

# Build master

mkdir ../bin 2>/dev/null

xlc "-Wa,goff" -DAS_TEST \
"-Wc,LANGLVL(EXTC99),FLOAT(HEX),agg,exp,list(),so(),goff,xref,gonum,roconst,gonum,ASM,ASMLIB('SYS1.MACLIB'),LP64,XPLINK" \
"-Wl,ac=1" -I $COMMON/h -I $ZSSAUX/src -I $ZSSAUX/include -o ../bin/master \
$ZSSAUX/test/address-space/src/master.c $COMMON/c/alloc.c $COMMON/c/as.c \
$COMMON/c/utils.c $COMMON/c/timeutls.c $COMMON/c/zos.c \
; extattr +a ../bin/master

# Build target

CFLAGS=(-S -M -qmetal -q64 -DSUBPOOL=132 -DMETTLE=1 -DMSGPREFIX='"IDX"'
-qreserved_reg=r12
-Wc,"arch(8),agg,exp,list(),so(),off,xref,roconst,longname,lp64"
-I $COMMON/h -I $ZSSAUX/src -I $ZSSAUX/include)

ASFLAGS=(-mgoff -mobject -mflag=nocont --TERM --RENT)

LDFLAGS=(-V -b ac=1 -b rent -b case=mixed -b map -b xref -b reus)

xlc "${CFLAGS[@]}" -DCMS_CLIENT \
$COMMON/c/alloc.c \
$COMMON/c/as.c \
$COMMON/c/metalio.c \
$COMMON/c/qsam.c \
$COMMON/c/timeutls.c \
$COMMON/c/utils.c \
$COMMON/c/zos.c \
$ZSSAUX/test/address-space/src/target.c \
$ZSSAUX/test/address-space/src/termexit.c \

as "${ASFLAGS[@]}" -aegimrsx=alloc.asm alloc.s
as "${ASFLAGS[@]}" -aegimrsx=as.asm as.s
as "${ASFLAGS[@]}" -aegimrsx=metalio.asm metalio.s
as "${ASFLAGS[@]}" -aegimrsx=qsam.asm qsam.s
as "${ASFLAGS[@]}" -aegimrsx=timeutls.asm timeutls.s
as "${ASFLAGS[@]}" -aegimrsx=utils.asm utils.s
as "${ASFLAGS[@]}" -aegimrsx=zos.asm zos.s
as "${ASFLAGS[@]}" -aegimrsx=target.asm target.s
as "${ASFLAGS[@]}" -aegimrsx=termexit.asm termexit.s

export _LD_SYSLIB="//'SYS1.CSSLIB'://'CEE.SCEELKEX'://'CEE.SCEELKED'://'CEE.SCEERUN'://'CEE.SCEERUN2'://'PP.CSF.ZOS203.SCSFMOD0'"

ld "${LDFLAGS[@]}" -e main \
-o "//'$USER.DEV.LOADLIB(ASTARGET)'" \
alloc.o \
as.o \
metalio.o \
qsam.o \
timeutls.o \
utils.o \
zos.o \
target.o \
> ASTARGET.link

ld "${LDFLAGS[@]}" -e main \
-o ../bin/termexit \
alloc.o \
as.o \
metalio.o \
qsam.o \
timeutls.o \
utils.o \
zos.o \
termexit.o \
> TERMEXIT.link

extattr +a ../bin/termexit

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.

