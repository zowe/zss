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
export _C89_L6SYSLIB="CEE.SCEEBND2:SYS1.CSSLIB:CSF.SCSFMOD0"

WORKING_DIR=$(cd $(dirname "$0") && pwd)
ZSS_ROOT="$WORKING_DIR/.."
ZSS="../.."

. ${ZSS_ROOT}/build/zss.proj.env
. ${ZSS_ROOT}/build/dependencies.sh
check_dependencies "${ZSS_ROOT}" "${ZSS_ROOT}/build/zss.proj.env"
DEPS_DESTINATION=$(get_destination "${ZSS}" "${PROJECT}")

YAML_MAJOR=0
YAML_MINOR=2
YAML_PATCH=5
YAML_VERSION="\"${YAML_MAJOR}.${YAML_MINOR}.${YAML_PATCH}\""


GSKDIR=/usr/lpp/gskssl
GSKINC="${GSKDIR}/include"
GSKLIB="${GSKDIR}/lib/GSKSSL64.x ${GSKDIR}/lib/GSKCMS64.x"

echo "********************************************************************************"
echo "Building ZSS (64 bit) ..."

mkdir -p "${WORKING_DIR}/tmp-zss" && cd "$_"

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

xlclang \
  -c \
  -q64 \
  -qascii \
  "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" \
  -DYAML_VERSION_MAJOR=${YAML_MAJOR} \
  -DYAML_VERSION_MINOR=${YAML_MINOR} \
  -DYAML_VERSION_PATCH=${YAML_PATCH} \
  -DYAML_VERSION_STRING="${YAML_VERSION}" \
  -DYAML_DECLARE_STATIC=1 \
  -D_OPEN_SYS_FILE_EXT=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -DCONFIG_VERSION=\"2021-03-27\" \
  -DNEW_CAA_LOCATIONS=1 \
  -I "${DEPS_DESTINATION}/${LIBYAML}/include" \
  -I "${DEPS_DESTINATION}/${QUICKJS}" \
  ${DEPS_DESTINATION}/${LIBYAML}/src/api.c \
  ${DEPS_DESTINATION}/${LIBYAML}/src/reader.c \
  ${DEPS_DESTINATION}/${LIBYAML}/src/scanner.c \
  ${DEPS_DESTINATION}/${LIBYAML}/src/parser.c \
  ${DEPS_DESTINATION}/${LIBYAML}/src/loader.c \
  ${DEPS_DESTINATION}/${LIBYAML}/src/writer.c \
  ${DEPS_DESTINATION}/${LIBYAML}/src/emitter.c \
  ${DEPS_DESTINATION}/${LIBYAML}/src/dumper.c \
  ${DEPS_DESTINATION}/${QUICKJS}/cutils.c \
  ${DEPS_DESTINATION}/${QUICKJS}/quickjs.c \
  ${DEPS_DESTINATION}/${QUICKJS}/quickjs-libc.c \
  ${DEPS_DESTINATION}/${QUICKJS}/libunicode.c \
  ${DEPS_DESTINATION}/${QUICKJS}/libregexp.c \
  ${DEPS_DESTINATION}/${QUICKJS}/porting/debugutil.c \
  ${DEPS_DESTINATION}/${QUICKJS}/porting/polyfill.c

STEP1CC="$?"

echo "STEP1CC $STEP1CC"

if [ STEP1CC="0" ]; then
  echo "Done with qascii-compiled open-source parts"
else
  echo "Build failed step1 code = $STEP1CC"
  exit 8
fi

echo "Done with ASCII mode 3rd party Objects"


xlclang \
  -c \
  -q64 \
  "-Wc,float(ieee),longname,langlvl(extc99),gonum,goff,ASM,asmlib('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN')" \
  -DYAML_VERSION_MAJOR=${YAML_MAJOR} \
  -DYAML_VERSION_MINOR=${YAML_MINOR} \
  -DYAML_VERSION_PATCH=${YAML_PATCH} \
  -DYAML_VERSION_STRING="${YAML_VERSION}" \
  -DYAML_DECLARE_STATIC=1 \
  -D_OPEN_SYS_FILE_EXT=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -DUSE_ZOWE_TLS=1 \
  -DCONFIG_VERSION=\"2021-03-27\" \
  -I "${DEPS_DESTINATION}/${LIBYAML}/include" \
  -I "${DEPS_DESTINATION}/${QUICKJS}" \
  -I ${DEPS_DESTINATION}/${ZCC}/h \
  ${DEPS_DESTINATION}/${ZCC}/c/embeddedjs.c \
  ${DEPS_DESTINATION}/${ZCC}/c/qjszos.c \
  ${DEPS_DESTINATION}/${ZCC}/c/qjsnet.c 

echo "Done with embedded JS interface"

export _C89_ACCEPTABLE_RC=0

# Hacking around weird link issue:
ar x /usr/lpp/cbclib/lib/libibmcmp.a z_atomic.LIB64R.o

if ! c89 \
  -DPRODUCT_MAJOR_VERSION="$major" \
  -DPRODUCT_MINOR_VERSION="$minor" \
  -DPRODUCT_REVISION="$micro" \
  -DPRODUCT_VERSION_DATE_STAMP="$date_stamp" \
  -D_XOPEN_SOURCE=600 \
  -DNOIBMHTTP=1 \
  -D_OPEN_THREADS=1 \
  -DHTTPSERVER_BPX_IMPERSONATION=1 \
  -DAPF_AUTHORIZED=0 \
  -DUSE_ZOWE_TLS=1 \
  -DNEW_CAA_LOCATIONS=1 \
  -Wc,lp64,dll,expo,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) \
  -Wc,agg,exp,list,so\(\),off,xref \
  -Wl,lp64,ac=1 \
  -I ${DEPS_DESTINATION}/${ZCC}/h \
  -I ${DEPS_DESTINATION}/${ZCC}/platform/posix \
  -I ${DEPS_DESTINATION}/${ZCC}/jwt/jwt \
  -I ${DEPS_DESTINATION}/${ZCC}/jwt/rscrypto \
  -I ${ZSS}/h \
  -I ${GSKINC} \
  -I ${DEPS_DESTINATION}/${LIBYAML}/include \
  -o ${ZSS}/bin/zssServer64 \
  api.o \
  reader.o \
  scanner.o \
  parser.o \
  loader.o \
  writer.o \
  emitter.o \
  dumper.o \
  cutils.o \
  quickjs.o \
  quickjs-libc.o \
  libunicode.o \
  libregexp.o \
  debugutil.o \
  polyfill.o \
  embeddedjs.o \
  qjszos.o  \
  qjsnet.o \
  z_atomic.LIB64R.o \
  ${DEPS_DESTINATION}/${ZCC}/c/alloc.c \
  ${DEPS_DESTINATION}/${ZCC}/c/bpxskt.c \
  ${DEPS_DESTINATION}/${ZCC}/c/charsets.c \
  ${DEPS_DESTINATION}/${ZCC}/c/cmutils.c \
  ${DEPS_DESTINATION}/${ZCC}/c/collections.c \
  ${DEPS_DESTINATION}/${ZCC}/c/configmgr.c \
  ${DEPS_DESTINATION}/${ZCC}/c/crossmemory.c \
  ${DEPS_DESTINATION}/${ZCC}/c/crypto.c \
  ${DEPS_DESTINATION}/${ZCC}/c/dataservice.c \
  ${DEPS_DESTINATION}/${ZCC}/c/discovery.c \
  ${DEPS_DESTINATION}/${ZCC}/c/dynalloc.c \
  ${DEPS_DESTINATION}/${ZCC}/c/fdpoll.c \
  ${DEPS_DESTINATION}/${ZCC}/c/http.c \
  ${DEPS_DESTINATION}/${ZCC}/c/httpclient.c \
  ${DEPS_DESTINATION}/${ZCC}/c/httpserver.c \
  ${DEPS_DESTINATION}/${ZCC}/c/httpfileservice.c \
  ${DEPS_DESTINATION}/${ZCC}/c/icsf.c \
  ${DEPS_DESTINATION}/${ZCC}/c/idcams.c \
  ${DEPS_DESTINATION}/${ZCC}/c/impersonation.c \
  ${DEPS_DESTINATION}/${ZCC}/c/jcsi.c \
  ${DEPS_DESTINATION}/${ZCC}/c/json.c \
  ${DEPS_DESTINATION}/${ZCC}/c/jsonschema.c \
  ${DEPS_DESTINATION}/${ZCC}/jwt/jwt/jwt.c \
  ${DEPS_DESTINATION}/${ZCC}/c/le.c \
  ${DEPS_DESTINATION}/${ZCC}/c/logging.c \
  ${DEPS_DESTINATION}/${ZCC}/c/nametoken.c \
  ${DEPS_DESTINATION}/${ZCC}/c/zos.c \
  ${DEPS_DESTINATION}/${ZCC}/c/parsetools.c \
  ${DEPS_DESTINATION}/${ZCC}/c/pause-element.c \
  ${DEPS_DESTINATION}/${ZCC}/c/pdsutil.c \
  ${DEPS_DESTINATION}/${ZCC}/c/qsam.c \
  ${DEPS_DESTINATION}/${ZCC}/c/radmin.c \
  ${DEPS_DESTINATION}/${ZCC}/c/rawfd.c \
  ${DEPS_DESTINATION}/${ZCC}/c/recovery.c \
  ${DEPS_DESTINATION}/${ZCC}/c/rusermap.c \
  ${DEPS_DESTINATION}/${ZCC}/jwt/rscrypto/rs_icsfp11.c \
  ${DEPS_DESTINATION}/${ZCC}/jwt/rscrypto/rs_rsclibc.c \
  ${DEPS_DESTINATION}/${ZCC}/c/scheduling.c \
  ${DEPS_DESTINATION}/${ZCC}/c/signalcontrol.c \
  ${DEPS_DESTINATION}/${ZCC}/c/socketmgmt.c \
  ${DEPS_DESTINATION}/${ZCC}/c/stcbase.c \
  ${DEPS_DESTINATION}/${ZCC}/c/storage.c \
  ${DEPS_DESTINATION}/${ZCC}/c/storage_mem.c \
  ${DEPS_DESTINATION}/${ZCC}/c/timeutls.c \
  ${DEPS_DESTINATION}/${ZCC}/c/tls.c \
  ${DEPS_DESTINATION}/${ZCC}/c/utils.c \
  ${DEPS_DESTINATION}/${ZCC}/c/vsam.c \
  ${DEPS_DESTINATION}/${ZCC}/c/xlate.c \
  ${DEPS_DESTINATION}/${ZCC}/c/xml.c \
  ${DEPS_DESTINATION}/${ZCC}/c/yaml2json.c \
  ${DEPS_DESTINATION}/${ZCC}/c/zosaccounts.c \
  ${DEPS_DESTINATION}/${ZCC}/c/zosfile.c \
  ${DEPS_DESTINATION}/${ZCC}/c/zvt.c \
  ${DEPS_DESTINATION}/${ZCC}/c/shrmem64.c \
  ${DEPS_DESTINATION}/${ZCC}/platform/posix/psxregex.c \
  ${ZSS}/c/zssLogging.c \
  ${ZSS}/c/zss.c \
  ${ZSS}/c/storageApiml.c \
  ${ZSS}/c/serviceUtils.c \
  ${ZSS}/c/authService.c \
  ${ZSS}/c/omvsService.c \
  ${ZSS}/c/certificateService.c \
  ${ZSS}/c/unixFileService.c \
  ${ZSS}/c/datasetService.c \
  ${ZSS}/c/datasetjson.c \
  ${ZSS}/c/envService.c \
  ${ZSS}/c/zosDiscovery.c \
  ${ZSS}/c/securityService.c \
  ${ZSS}/c/registerProduct.c \
  ${ZSS}/c/zis/client.c \
  ${ZSS}/c/serverStatusService.c \
  ${ZSS}/c/rasService.c \
  ${ZSS}/c/userInfoService.c \
  ${ZSS}/c/passTicketService.c \
  ${ZSS}/c/jwk.c \
  ${ZSS}/c/safIdtService.c \
  ${GSKLIB} ;
then
  extattr +p ${ZSS}/bin/zssServer64
  echo "Build zss64 successfully"
  exit 0
else
  # remove zssServer in case the linker had RC=4 and produced the binary
  rm -f ${ZSS}/bin/zssServer64
  echo "Build zss64 failed"
  exit 8
fi
