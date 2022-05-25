#!/bin/sh
# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
# SPDX-License-Identifier: EPL-2.0
#
# Copyright Contributors to the Zowe Project.
## launch the Zowe Secure Services Server

export _BPXK_AUTOCVT=ON

COMPONENT_HOME="${ZWE_zowe_runtimeDirectory}/components/zss"
ZSS_SCRIPT_DIR="${COMPONENT_HOME}/bin"

# include . in LIBPATH if not present already
if [ -n "$LIBPATH" ]
then
  if [ "$(echo "${LIBPATH}" | grep ":.:")" != "${LIBPATH}" ] && [ "$(echo "${LIBPATH}" | cut -c1-2)" != ".:" ] && [ "$(echo "${LIBPATH}" | tail -c 3)" != ":." ];
  then
    export LIBPATH="${LIBPATH}:."
  fi
fi
# this is to resolve (builtin) plugins that use ZLUX_ROOT_DIR as a relative path. if it doesnt exist, the plugins shouldn't either, so no problem
if [ -z "${ZLUX_ROOT_DIR}" ]; then
  if [ -d "${ZWE_zowe_runtimeDirectory}/components/app-server/share" ]; then
    export ZLUX_ROOT_DIR="${ZWE_zowe_runtimeDirectory}/components/app-server/share"
  fi
fi

# This location will have init and utils folders inside and is used to gather env vars and do plugin initialization
helper_script_location="${ZLUX_ROOT_DIR}/zlux-app-server/bin"

if [ -d "${helper_script_location}" ]; then
  . ${helper_script_location}/utils/convert-env.sh
  cd "${COMPONENT_HOME}/bin"
fi

# TODO this depends on nodejs but shouldn't. It should go away when config manager is available.
# Read yaml file, convert it to env vars loading same way as env vars
yamlConverter="${ZLUX_ROOT_DIR}/zlux-server-framework/utils/yamlConfig.js"
env_converted_from_yaml=$(node $yamlConverter --components 'zss app-server' 2>/dev/null)
if [ -n "$env_converted_from_yaml" ]; then
  eval "$env_converted_from_yaml"
fi

 # It's still possible to provide a json config
if [ -e "$ZSS_CONFIG_FILE" ]
then
  CONFIG_FILE=$ZSS_CONFIG_FILE
fi
# in case when no server.json, zss can be configured with env vars 
# when zss is ready to read yaml config directly check whether CONFIG_FILE=~/.zowe/zowe.yaml exists
if [ -n "$ZWES_LOG_FILE" ]
then
  if [[ $ZWES_LOG_FILE == /* ]]
  then
    echo "Absolute log location given."
  else
    ZWES_LOG_FILE="${ZSS_SCRIPT_DIR}/${ZWES_LOG_FILE}"
    echo "Relative log location given, set to absolute path=$ZWES_LOG_FILE"
  fi
  if [ -n "$ZWES_LOG_DIR" ]
  then
    echo "ZWES_LOG_FILE set (value $ZWES_LOG_FILE).  Ignoring ZWES_LOG_DIR."
  fi
else
# _FILE was not specified; default filename, and check and maybe default _DIR
  if [ -z "$ZWES_LOG_DIR" ]
  then
    if [ -n "$ZWE_zowe_logDirectory" -a -d "$ZWE_zowe_logDirectory" ]
    then
      ZWES_LOG_DIR=${ZWE_zowe_logDirectory}
    else
      ZWES_LOG_DIR="../log"
    fi
  fi
  if [ -f "$ZWES_LOG_DIR" ]
  then
    ZWES_LOG_FILE=$ZWES_LOG_DIR
  elif [ ! -d "$ZWES_LOG_DIR" ]
  then
    echo "Will make log directory $ZWES_LOG_DIR"
    mkdir -p $ZWES_LOG_DIR
    if [ $? -ne 0 ]
    then
      echo "Cannot make log directory.  Logging disabled."
      ZWES_LOG_FILE=/dev/null
    fi
  fi
  ZLUX_ROTATE_LOGS=0
  if [ -d "$ZWES_LOG_DIR" ] && [ -z "$ZWES_LOG_FILE" ]
  then
    ZWES_LOG_FILE="$ZWES_LOG_DIR/zssServer-`date +%Y-%m-%d-%H-%M`.log"
    if [ -z "$ZWES_LOGS_TO_KEEP" ]
    then
      ZWES_LOGS_TO_KEEP=5
    fi
    echo $ZWES_LOGS_TO_KEEP|egrep '^\-?[0-9]+$' >/dev/null
    if [ $? -ne 0 ]
    then
      echo "ZWES_LOGS_TO_KEEP not a number.  Defaulting to 5."
      ZWES_LOGS_TO_KEEP=5
    fi
    if [ $ZWES_LOGS_TO_KEEP -ge 0 ]
    then
      ZLUX_ROTATE_LOGS=1
    fi
  fi
  #Clean up excess logs, if appropriate.
  if [ $ZLUX_ROTATE_LOGS -ne 0 ]
  then
    for f in `ls -r -1 $ZWES_LOG_DIR/zssServer-*.log 2>/dev/null | tail +$ZWES_LOGS_TO_KEEP`
    do
      echo zssServer.sh removing $f
      rm -f $f
    done
  fi
fi
ZSS_CHECK_DIR="$(dirname "$ZWES_LOG_FILE")"
if [ ! -d "$ZSS_CHECK_DIR" ]
then
  echo "ZWES_LOG_FILE contains nonexistent directories.  Creating $ZSS_CHECK_DIR"
  mkdir -p $ZSS_CHECK_DIR
  if [ $? -ne 0 ]
  then
    echo "Cannot make log directory.  Logging disabled."
    ZWES_LOG_FILE=/dev/null
  fi
fi
#Now sanitize final log filename: if it is relative, make it absolute before cd to js
if [ "$ZWES_LOG_FILE" != "/dev/null" ]
then
  ZSS_CHECK_DIR=$(cd "$(dirname "$ZWES_LOG_FILE")"; pwd)
  ZWES_LOG_FILE=$ZSS_CHECK_DIR/$(basename "$ZWES_LOG_FILE")
fi
echo ZWES_LOG_FILE=${ZWES_LOG_FILE}
export ZWES_LOG_FILE=$ZWES_LOG_FILE
if [ ! -e $ZWES_LOG_FILE ]
then
  touch $ZWES_LOG_FILE
  if [ $? -ne 0 ]
  then
    echo "Cannot make log file.  Logging disabled."
    ZWES_LOG_FILE=/dev/null
  fi
else
  if [ -d $ZWES_LOG_FILE ]
  then
    echo "ZWES_LOG_FILE is a directory.  Must be a file.  Logging disabled."
    ZWES_LOG_FILE=/dev/null
  fi
fi
if [ ! -w "$ZWES_LOG_FILE" ]
then
  echo file "$ZWES_LOG_FILE" is not writable. Logging disabled.
  ZWES_LOG_FILE=/dev/null
fi
#Determined log file.  Run zssServer.
export dir=`dirname "$0"`
cd $ZSS_SCRIPT_DIR

# determine amode for zssServer 
ZSS_SERVER_31="./zssServer"
ZSS_SERVER_64="./zssServer64"

if [ "$ZWE_components_zss_agent_64bit" = "true" ] && [ -x "${ZSS_SERVER_64}" ]; then
  ZSS_SERVER="${ZSS_SERVER_64}"
else
  ZSS_SERVER="${ZSS_SERVER_31}"
fi

if [ -f "$CONFIG_FILE" ]; then
  if [ "$ZWES_LOG_FILE" = "/dev/null" ]; then
    _BPX_SHAREAS=NO _BPX_JOBNAME=${ZOWE_PREFIX}SZ ${ZSS_SERVER} "${CONFIG_FILE}" 2>&1
  else
    _BPX_SHAREAS=NO _BPX_JOBNAME=${ZOWE_PREFIX}SZ ${ZSS_SERVER} "${CONFIG_FILE}" 2>&1 | tee $ZWES_LOG_FILE
  fi
else
  if [ "$ZWES_LOG_FILE" = "/dev/null" ]; then
    _BPX_SHAREAS=NO _BPX_JOBNAME=${ZOWE_PREFIX}SZ ${ZSS_SERVER} 2>&1
  else
    _BPX_SHAREAS=NO _BPX_JOBNAME=${ZOWE_PREFIX}SZ ${ZSS_SERVER} 2>&1 | tee $ZWES_LOG_FILE
  fi
fi
# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
# SPDX-License-Identifier: EPL-2.0
#
# Copyright Contributors to the Zowe Project.
