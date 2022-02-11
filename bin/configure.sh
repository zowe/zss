#!/bin/sh
# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.


# Required variables on shell:
# - ZWE_zowe_runtimeDirectory
# - ZWE_zowe_workspaceDirectory

cd ${ZWE_zowe_runtimeDirectory}/components/app-server/share/zlux-app-server/bin
. ./convert-env.sh

cd ${ZWE_zowe_runtimeDirectory}/components/app-server/share/zlux-app-server/lib

# ZWED_instanceDir, ZWED_siteDir are set by convert-env.sh
SITE_DIR="$ZWED_instanceDir/site"
SITE_PLUGIN_STORAGE="$ZWED_siteDir/ZLUX/pluginStorage"
INSTANCE_PLUGIN_STORAGE="$ZWED_instanceDir/ZLUX/pluginStorage"
GROUPS_DIR="$ZWED_instanceDir/groups"
USERS_DIR="$ZWED_instanceDir/users"
PLUGINS_DIR="$ZWED_instanceDir/plugins"

mkdir -p "$SITE_DIR"
mkdir -p "$SITE_PLUGIN_STORAGE"
mkdir -p "$INSTANCE_PLUGIN_STORAGE"
mkdir -p "$GROUPS_DIR"
mkdir -p "$USERS_DIR"
mkdir -p "$PLUGINS_DIR"

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
export ZLUX_ROOT_DIR=${ZWE_zowe_runtimeDirectory}/components/app-server/share
