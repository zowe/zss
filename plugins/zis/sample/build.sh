#!/bin/bash
set -e

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.

ZSS=../../../../../zss
COMMON=$ZSS/deps/zowe-common-c

# compile, assembler and linker flags for the sample plugin
CFLAGS=(-S -qmetal -DMETTLE=1 -q64 -qreserved_reg=r12 -qroconst -qlongname)
ASFLAGS=(-mgoff -mobject --RENT)
LDFLAGS=(-b reus=rent -b case=mixed)
CFLAGS_INFO=(-qlist= -qaggregate -qexpmac -qsource -qoffset -qxref)
LDFLAGS_INFO=(-V -b map -b xref)

PLUGIN_INCLUDES=(-I $COMMON/h -I $ZSS/h -I .)

# build the sample plugin
# - build the java generator tool and generate the HLASM stubs for dynamic linkage
# - build the plugin itself
mkdir -p tmp_plugin
cd tmp_plugin

javac -encoding iso8859-1 ${ZSS}/tools/dynzis/org/zowe/zis/ZISStubGenerator.java
java  -cp ${ZSS}/tools/dynzis org.zowe.zis.ZISStubGenerator asm \
 ${ZSS}/h/zis/zisstubs.h > dynzis.s

xlc "${CFLAGS[@]}" "${CFLAGS_INFO[@]}" "${PLUGIN_INCLUDES[@]}" \
 $ZSS/plugins/zis/sample/sample_plugin.c
as "${ASFLAGS[@]}" -aegimrsx=sample_plugin.asm sample_plugin.s
as "${ASFLAGS[@]}" -aegimrsx=dynzis.asm dynzis.s
ld "${LDFLAGS[@]}" "${LDFLAGS_INFO[@]}" -e getPluginDescriptor \
-o "//'$USER.DEV.LOADLIB(ZISSAMPL)'" \
sample_plugin.o dynzis.o > sample_plugin.link
cd ..

# build the sample client
mkdir -p tmp_client
mkdir -p bin
cd tmp_client
xlc "-Wa,goff" \
"-Wc,langlvl(extc99),asm,asmlib('SYS1.MACLIB'),lp64,xplink" \
-DCMS_CLIENT -D_OPEN_THREADS=1 -I $COMMON/h -I $ZSS/h \
${COMMON}/c/alloc.c \
${COMMON}/c/cmutils.c \
${COMMON}/c/crossmemory.c \
${COMMON}/c/timeutls.c \
${COMMON}/c/utils.c \
${COMMON}/c/zos.c \
${COMMON}/c/zvt.c \
${ZSS}/c/zis/client.c \
-o ../bin/sample_client ../sample_client.c
cd ..

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.
