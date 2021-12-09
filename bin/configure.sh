#!/bin/sh
# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.


# Required variables on shell:
# - ROOT_DIR
# - WORKSPACE_DIR

cd ${ROOT_DIR}/components/app-server/share/zlux-app-server/bin
. ./convert-env.sh

cd ${ROOT_DIR}/components/app-server/share/zlux-app-server/lib

if [ -n "$INSTANCE_DIR" ]
then
  INSTANCE_LOCATION=$INSTANCE_DIR
else
  INSTANCE_LOCATION=$HOME/.zowe
fi
if [ -n "$WORKSPACE_DIR" ]
then
  WORKSPACE_LOCATION=$WORKSPACE_DIR
else
  WORKSPACE_LOCATION=$INSTANCE_LOCATION/workspace
fi
DESTINATION=$WORKSPACE_LOCATION/app-server

currentJsonConfigPath=$DESTINATION/serverConfig/server.json

if [ ! -f "$currentJsonConfigPath" ]; then
  mkdir -p $DESTINATION/serverConfig
  cp ../defaults/serverConfig/server.json $DESTINATION/serverConfig/server.json
  PRODUCT_DIR=$(cd "$PWD/../defaults" && pwd)
  SITE_DIR=$DESTINATION/site
  SITE_PLUGIN_STORAGE=$SITE_DIR/ZLUX/pluginStorage
  INSTANCE_PLUGIN_STORAGE=$DESTINATION/ZLUX/pluginStorage
  INSTANCE_CONFIG=$DESTINATION/serverConfig
  GROUPS_DIR=$DESTINATION/groups
  USERS_DIR=$DESTINATION/users
  PLUGINS_DIR=$DESTINATION/plugins

  sed 's@"productDir":"../defaults"@"productDir":"'${PRODUCT_DIR}'"@g' $currentJsonConfigPath > ${currentJsonConfigPath}.zwetmp1
  sed 's@"siteDir":"../deploy/site"@"siteDir":"'${SITE_DIR}'"@g' ${currentJsonConfigPath}.zwetmp1 > ${currentJsonConfigPath}.zwetmp2
  sed 's@"instanceDir":"../deploy/instance"@"instanceDir":"'${DESTINATION}'"@g' ${currentJsonConfigPath}.zwetmp2 > ${currentJsonConfigPath}.zwetmp1
  sed 's@"groupsDir":"../deploy/instance/groups"@"groupsDir":"'${GROUPS_DIR}'"@g' ${currentJsonConfigPath}.zwetmp1 > ${currentJsonConfigPath}.zwetmp2
  sed 's@"usersDir":"../deploy/instance/users"@"usersDir":"'${USERS_DIR}'"@g' ${currentJsonConfigPath}.zwetmp2 > ${currentJsonConfigPath}.zwetmp1
  sed 's@"pluginsDir":"../defaults/plugins"@"pluginsDir":"'${PLUGINS_DIR}'"@g' ${currentJsonConfigPath}.zwetmp1 > ${currentJsonConfigPath}
  rm ${currentJsonConfigPath}.zwetmp1 ${currentJsonConfigPath}.zwetmp2

  mkdir -p $SITE_PLUGIN_STORAGE
  mkdir -p $INSTANCE_PLUGIN_STORAGE
  mkdir -m 700 -p $INSTANCE_CONFIG
  chmod 700 $INSTANCE_CONFIG
  chmod 700 $INSTANCE_CONFIG/server.json
  mkdir -p $GROUPS_DIR
  mkdir -p $USERS_DIR
  mkdir -p $PLUGINS_DIR
fi

APP_WORKSPACE_DIR=${INSTANCE_DIR}/workspace/app-server
if [ "${ZWES_SERVER_TLS}" = "false" ]; then
  PROTOCOL="http"
else
  PROTOCOL="https"
fi

HTTPS_PREFIX="ZWED_agent_https_"

if [ "${ZWES_SERVER_TLS}" = "false" ]
then
  # HTTP
  export "ZWED_agent_http_port=${ZWES_SERVER_PORT}"
else
  # HTTPS
  export "${HTTPS_PREFIX}port=${ZWES_SERVER_PORT}"
  IP_ADDRESSES_KEY_var="${HTTPS_PREFIX}ipAddresses"
  eval "IP_ADDRESSES_val=\"\$${IP_ADDRESSES_KEY_var}\""
  if [ -z "${IP_ADDRESSES_val}" ]; then
    export "${IP_ADDRESSES_KEY_var}"="${ZOWE_IP_ADDRESS}"
  fi
fi

# Keystore settings are used for both https server and Caching Service
export "${HTTPS_PREFIX}label=${KEY_ALIAS}"
if [ "${KEYSTORE_TYPE}" = "JCERACFKS" ]; then
  export "${HTTPS_PREFIX}keyring=${KEYRING_OWNER}/${KEYRING_NAME}"
else
  export "${HTTPS_PREFIX}keyring=${KEYSTORE}"
  export "${HTTPS_PREFIX}password=${KEYSTORE_PASSWORD}"
fi

# xmem name customization
if [ -z "$ZWED_privilegedServerName" ]
then
  if [ -n "$ZWES_XMEM_SERVER_NAME" ]
  then
    export ZWED_privilegedServerName=$ZWES_XMEM_SERVER_NAME
  fi 
fi

# this is to resolve (builtin) plugins that use ZLUX_ROOT_DIR as a relative path. if it doesnt exist, the plugins shouldn't either, so no problem
export ZLUX_ROOT_DIR=${ROOT_DIR}/components/app-server/share
