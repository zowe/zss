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

if [ -e "../bin/app-server.sh" ]
then
  in_app_server=true
else
  in_app_server=false
fi
if $in_app_server
then
  ZSS_FILE=../bin/zssServer
  ZSS_COMPONENT=${ROOT_DIR}/components/zss/bin
  if test -f "$ZSS_FILE"; then
    ZSS_SCRIPT_DIR=$(cd `dirname $0`/../bin && pwd)
  elif [ -d "$ZSS_COMPONENT" ]; then
    ZSS_SCRIPT_DIR=$ZSS_COMPONENT
  fi
  ../bin/convert-env.sh
else
  ZSS_SCRIPT_DIR=$(cd `dirname $0` && pwd)
  . ${ZSS_SCRIPT_DIR}/../../app-server/share/zlux-app-server/bin/convert-env.sh
  yamlConverter="${ZSS_SCRIPT_DIR}/../../app-server/share/zlux-server-framework/utils/yamlConfig.js"
  env_converted_from_yaml=$(node $yamlConverter --components 'zss app-server' 2>/dev/null)
  if [ -n "$env_converted_from_yaml" ]; then
    eval "$env_converted_from_yaml"
  fi
fi
if [ -e "$ZSS_CONFIG_FILE" ]
then
    CONFIG_FILE=$ZSS_CONFIG_FILE
elif [ -e "$ZLUX_CONFIG_FILE" ]
then
    CONFIG_FILE=$ZLUX_CONFIG_FILE
elif [ -d "$WORKSPACE_DIR" ]
then
  CONFIG_FILE="${WORKSPACE_DIR}/app-server/serverConfig/server.json"
elif [ -d "$INSTANCE_DIR" ]
then
  CONFIG_FILE="${INSTANCE_DIR}/workspace/app-server/serverConfig/server.json"
elif $in_app_server
then
  CONFIG_FILE="../defaults/serverConfig/server.json"
elif [ -d "$ROOT_DIR" ]
then
# This conditional and the else conditional are here for backup purposes, INSTANCE_DIR and WORKSPACE_DIR are defined
# in the initialization of the app-server if they are not defined but in the case of zss development they might not be defined
# and will use the default server configuration
    CONFIG_FILE="${ROOT_DIR}/components/app-server/share/zlux-app-server/defaults/serverConfig/server.json"
else
    echo "No config file specified, using default"
    CONFIG_FILE="${ZSS_SCRIPT_DIR}/../../app-server/share/zlux-app-server/defaults/serverConfig/server.json"
fi
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
    if [ -d "$INSTANCE_DIR" ]
    then
      ZWES_LOG_DIR=${INSTANCE_DIR}/logs
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
_BPX_SHAREAS=NO _BPX_JOBNAME=${ZOWE_PREFIX}SZ1 ./zssServer "${CONFIG_FILE}" 2>&1 | tee $ZWES_LOG_FILE
# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
# SPDX-License-Identifier: EPL-2.0
#
# Copyright Contributors to the Zowe Project.
