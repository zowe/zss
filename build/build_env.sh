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


export _C89_LSYSLIB="CEE.SCEELKED:SYS1.CSSLIB:CSF.SCSFMOD0"

WORKING_DIR=$(dirname "$0")
ZSS="../.."
COMMON="../../deps/zowe-common-c"


echo "********************************************************************************"
echo "Building ENV Service..."

mkdir -p "${WORKING_DIR}/tmp-env" && cd "$_"

IFS='.' read -r major minor micro < "${ZSS}/version.txt"
date_stamp=$(date +%Y%m%d)
echo "Version: $major.$minor.$micro"
echo "Date stamp: $date_stamp"

c89 \
  -DPRODUCT_MINOR_VERSION="$major" \
  -DPRODUCT_MAJOR_VERSION="$minor" \
  -DPRODUCT_REVISION="$micro" \
  -DPRODUCT_VERSION_DATE_STAMP="$date_stamp" \
  -D_XOPEN_SOURCE=600 \
  -DNOIBMHTTP=1 \
  -D_OPEN_THREADS=1 \
  -DHTTPSERVER_BPX_IMPERSONATION=1 \
  -DAPF_AUTHORIZED=0 \
  -Wc,dll,expo,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) \
  -Wl,ac=1 \
  -I ${COMMON}/h \
  -I ${ZSS}/h \
  -o ${ZSS}/bin/envService \
  ${COMMON}/c/alloc.c \
  ${COMMON}/c/charsets.c \
  ${COMMON}/c/json.c \
  ${COMMON}/c/utils.c \
  ${COMMON}/c/timeutls.c \
  ${COMMON}/c/zosfile.c \
  ${ZSS}/c/envService.c \
  
extattr +p ${ZSS}/bin/envService

echo "Build successfull"
