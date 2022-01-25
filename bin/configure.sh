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
convert_v2_to_v1() {
  while read old_name new_name; do
    old_val=$(eval echo "\$${old_name}")
    new_val=$(eval echo "\$${new_name}")
    if [ -z "${old_val}" -a -n "${new_val}" ]; then
      export "${old_name}=${new_val}"
    fi
  done <<EOF
CATALOG_PORT ZWE_components_api_catalog_port 
DISCOVERY_PORT ZWE_components_discovery_port
GATEWAY_HOST ZWE_haInstance_hostname
GATEWAY_PORT ZWE_components_gateway_port
KEY_ALIAS ZWE_zowe_certificate_keystore_alias
KEYSTORE ZWE_zowe_certificate_keystore_file
KEYSTORE_CERTIFICATE ZWE_zowe_certificate_pem_certificate
KEYSTORE_CERTIFICATE_AUTHORITY ZWE_zowe_certificate_pem_certificateAuthority
KEYSTORE_CERTIFICATE_AUTHORITY ZWE_zowe_certificate_truststore_certificateAuthorities
KEYSTORE_DIRECTORY ZWE_zowe_setup_certificate_pkcs12_directory
KEYSTORE_KEY ZWE_zowe_certificate_pem_key
KEYSTORE_PASSWORD ZWE_zowe_certificate_keystore_password
KEYSTORE_TYPE ZWE_zowe_certificate_keystore_type
ROOT_DIR ZWE_zowe_runtimeDirectory
TRUSTSTORE ZWE_zowe_certificate_truststore_file
VERIFY_CERTIFICATES ZWE_zowe_verifyCertificates
WORKSPACE_DIR ZWE_zowe_workspaceDirectory
ZOWE_EXPLORER_HOST ZWE_haInstance_hostname
ZOWE_PREFIX ZWE_zowe_job_prefix
ZOWE_ZLUX_SERVER_HTTPS_PORT ZWE_components_app_server_port
ZOWE_ZSS_XMEM_SERVER_NAME ZWE_components_zss_crossMemoryServerName
ZWED_agent_https_keyring ZWE_zowe_certificate_keystore_file
ZWED_agent_https_label ZWE_zowe_certificate_keystore_alias
ZWED_agent_https_password ZWE_zowe_certificate_keystore_password
ZWED_node_https_port ZWE_components_app_server_port
ZWES_SERVER_PORT ZWE_components_zss_port
ZWES_SERVER_TLS ZWE_components_zss_tls
EOF
}

# where do we get??
# ZOWE_IP_ADDRESS
# ZWES_ZIS_LOADLIB TS3105.SZWEAUTH
# ZWES_ZIS_PLUGINLIB TS3105.SZWEPLUG
# ZWES_ZIS_PARMLIB TS3105.SZWESAMP
# ZWES_ZIS_PARMLIB_MEMBER ZWESIP00

convert_v2_to_v1

export ZWED_agent_https_ipAddresses="0.0.0.0"

cd ${ZWE_zowe_runtimeDirectory}/components/app-server/share/zlux-app-server/bin
. ./convert-env.sh

cd ${ZWE_zowe_runtimeDirectory}/components/app-server/share/zlux-app-server/lib

if [ -n "$ZWE_zowe_workspaceDirectory" ]
then
  WORKSPACE_LOCATION=$ZWE_zowe_workspaceDirectory
else
  WORKSPACE_LOCATION="$HOME/.zowe/workspace"
fi
DESTINATION="$WORKSPACE_LOCATION/app-server"

PRODUCT_DIR=$(cd "$PWD/../defaults" && pwd)
SITE_DIR=$DESTINATION/site
SITE_PLUGIN_STORAGE=$SITE_DIR/ZLUX/pluginStorage
INSTANCE_PLUGIN_STORAGE=$DESTINATION/ZLUX/pluginStorage
GROUPS_DIR=$DESTINATION/groups
USERS_DIR=$DESTINATION/users
PLUGINS_DIR=$DESTINATION/plugins


mkdir -p $SITE_PLUGIN_STORAGE
mkdir -p $INSTANCE_PLUGIN_STORAGE
mkdir -p $GROUPS_DIR
mkdir -p $USERS_DIR
mkdir -p $PLUGINS_DIR

export ZWED_productDir=$PRODUCT_DIR
export ZWED_siteDir=$SITE_DIR
export ZWED_groupsDir=$GROUPS_DIR
export ZWED_usersDir=$USERS_DIR
export ZWED_pluginsDir=$PLUGINS_DIR

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
