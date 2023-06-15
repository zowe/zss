#!/bin/sh
# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.


# Required variables on shell (from start-app-server.sh):
# - ZWE_zowe_runtimeDirectory

OSNAME=$(uname)
if [[ "${OSNAME}" == "OS/390" ]]; then
  # we use this as an indication that zwe exists
  if [ -n ${ZWE_VERSION} ]; then
    ZWES_PROD=true
  else
    ZWES_PROD=false
  fi

  export _EDC_ZERO_RECLEN=Y # allow processing of zero-length records in datasets
  cd ${ZWE_zowe_runtimeDirectory}/components/zss/bin

  export _BPXK_AUTOCVT=ON
  
  # Get component home dir if defined, otherwise set it based on runtimeDirectory
  if [ -z "${ZWES_COMPONENT_HOME}" ]; then
    if [ -z "${ZWE_zowe_runtimeDirectory}" ]; then
      echo 'ZWE_zowe_runtimeDirectory is not defined. Run with ZWES_COMPONENT_HOME defined to the path of the zss root folder or ZWE_zowe_runtimeDirectory defined to the path of a zowe install that includes zowe core schemas and zss within /components/zss'
      exit 1
    else
      ZWES_COMPONENT_HOME="${ZWE_zowe_runtimeDirectory}/components/zss"
    fi
  fi
  
  # Get schema paths if defined, otherwise set it based on runtimeDirectory
  if [ -z "${ZWES_SCHEMA_PATHS}" ]; then
    if [ -z "${ZWE_zowe_runtimeDirectory}" ]; then
      echo "ZWE_zowe_runtimeDirectory is not defined. Run with ZWES_SCHEA_PATHS defined to the colon-separated file paths of zowe json schemas used to validate the ZSS configuration yaml file, or have ZWE_zowe_runtimeDirectory defined to the path of a zowe install that includes zowe core schemas and zss within /components/zss"
      exit 1
    else
      ZWES_SCHEMA_PATHS="${ZWES_COMPONENT_HOME}/schemas/zowe-schema.json:${ZWE_zowe_runtimeDirectory}/schemas/zowe-yaml-schema.json:${ZWE_zowe_runtimeDirectory}/schemas/server-common.json:${ZWES_COMPONENT_HOME}/schemas/zss-config.json"
    fi
  fi
  
  # Get config path or fail
  if [ -z "${ZWE_CLI_PARAMETER_CONFIG}" ]; then
    echo "ZWE_CLI_PARAMETER_CONFIG is not defined. Rerun script with it defined to a list of paths to zowe.yaml files such as /path/to/zowe.yaml or FILE(/yaml1.yaml):LIB(other.yaml):FILE(/path/to/yaml3.yaml)"
    exit 1
  fi

  if [ "${ZWES_PROD}" = "true" ]; then
    # defaults already given as a file on disk
    ZWES_CONFIG="FILE(${ZWE_CLI_PARAMETER_CONFIG})"
  else 
    # Take in our defaults
    case "$ZWE_CLI_PARAMETER_CONFIG" in
    FILE*)
      ZWES_CONFIG="${ZWE_CLI_PARAMETER_CONFIG}:FILE(${ZWES_COMPONENT_HOME}/defaults.yaml)"
      ;;
    *)
      ZWES_CONFIG="FILE(${ZWE_CLI_PARAMETER_CONFIG}):FILE(${ZWES_COMPONENT_HOME}/defaults.yaml)"
      ;;
    esac
  fi

  # Essential parameters now set up.
  
  ZSS_SCRIPT_DIR="${ZWES_COMPONENT_HOME}/bin"
  
  # this is to resolve ZSS bin path in LIBPATH variable.
  LIBPATH="${LIBPATH}:${ZSS_SCRIPT_DIR}"
  
  #### Log file initialization ####
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
    ZWES_ROTATE_LOGS=0
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
        ZWES_ROTATE_LOGS=1
      fi
    fi
    #Clean up excess logs, if appropriate.
    if [ $ZWES_ROTATE_LOGS -ne 0 ]
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
  #### Log file determined ####
  
  # Startup zss
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
  
  if [ "$ZWES_LOG_FILE" = "/dev/null" ]; then
    _BPX_SHAREAS=NO _BPX_JOBNAME=${ZWE_zowe_job_prefix}SZ ${ZSS_SERVER} --schemas "${ZWES_SCHEMA_PATHS}" --configs "${ZWES_CONFIG}" 2>&1
  else
    _BPX_SHAREAS=NO _BPX_JOBNAME=${ZWE_zowe_job_prefix}SZ ${ZSS_SERVER} --schemas "${ZWES_SCHEMA_PATHS}" --configs "${ZWES_CONFIG}" 2>&1 | tee $ZWES_LOG_FILE
  fi  
else
  echo "Zowe ZSS server is unsupported on ${OSNAME}. Supported systems: OS/390"
fi
