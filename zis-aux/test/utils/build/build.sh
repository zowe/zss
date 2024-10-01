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

mkdir ../bin 2>/dev/null

# Shared 64-bit memory

xlc "-Wa,goff" \
"-Wc,LANGLVL(EXTC99),FLOAT(HEX),agg,exp,list(),so(),goff,xref,gonum,roconst,gonum,ASM,ASMLIB('SYS1.MACLIB'),LP64,XPLINK" \
"-Wl,ac=1" -I $COMMON/h -I $ZSSAUX/src -I $ZSSAUX/include \
-o ../bin/shrmem64-test \
$ZSSAUX/test/utils/src/shrmem64-test.c \
$COMMON/c/alloc.c \
$COMMON/c/shrmem64.c \
$COMMON/c/utils.c \
$COMMON/c/timeutls.c \
$COMMON/c/zos.c ; extattr +a ../bin/shrmem64-test

xlc "-Wa,goff" -DSHRMEM64_TEST_TARGET \
"-Wc,LANGLVL(EXTC99),FLOAT(HEX),agg,exp,list(),so(),goff,xref,gonum,roconst,gonum,ASM,ASMLIB('SYS1.MACLIB'),LP64,XPLINK" \
"-Wl,ac=1" -I $COMMON/h -I $ZSSAUX/src -I $ZSSAUX/include \
-o ../bin/shrmem64-target-test \
$ZSSAUX/test/utils/src/shrmem64-test.c \
$COMMON/c/alloc.c \
$COMMON/c/shrmem64.c \
$COMMON/c/utils.c \
$COMMON/c/timeutls.c \
$COMMON/c/zos.c ; extattr +a ../bin/shrmem64-target-test

# Pause element

xlc "-Wa,goff" -DAS_TEST \
"-Wc,LANGLVL(EXTC99),FLOAT(HEX),agg,exp,list(),so(),goff,xref,gonum,roconst,gonum,ASM,ASMLIB('SYS1.MACLIB'),LP64,XPLINK" \
-I $COMMON/h -I $ZSSAUX/src -I $ZSSAUX/include \
-o ../bin/pe-test \
$ZSSAUX/test/utils/src/pe-test.c \
$COMMON/c/alloc.c \
$COMMON/c/pause-element.c \
$COMMON/c/utils.c \
$COMMON/c/timeutls.c \
$COMMON/c/zos.c \

# PC (program call)

xlc "-Wa,goff" -DAS_TEST \
"-Wc,LANGLVL(EXTC99),FLOAT(HEX),agg,exp,list(),so(),goff,xref,gonum,roconst,gonum,ASM,ASMLIB('SYS1.MACLIB'),LP64,XPLINK" \
"-Wl,ac=1" -I $COMMON/h -I $ZSSAUX/src -I $ZSSAUX/include \
-o ../bin/pc-test \
$ZSSAUX/test/utils/src/pc-test.c \
$COMMON/c/alloc.c \
$COMMON/c/pc.c \
$COMMON/c/utils.c \
$COMMON/c/timeutls.c \
$COMMON/c/zos.c ; extattr +a ../bin/pc-test

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.

