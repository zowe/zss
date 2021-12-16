#!/bin/sh
# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.


# Required variables on shell (from start-app-server.sh):
# - ROOT_DIR
# - WORKSPACE_DIR
# - NODE_HOME
#
# Optional variables on shell (from start-app-server.sh):
# - APIML_ENABLE_SSO
# - GATEWAY_PORT
# - DISCOVERY_PORT
# - ZWED_SSH_PORT
# - ZWED_TN3270_PORT
# - ZWED_TN3270_SECURITY

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
export ZLUX_ROOT_DIR=${ZWE_zowe_runtimeDirectory}/components/app-server/share
export SSO_FALLBACK_TO_NATIVE_AUTH=true

OSNAME=$(uname)
if [[ "${OSNAME}" == "OS/390" ]]; then
  export _EDC_ZERO_RECLEN=Y # allow processing of zero-length records in datasets
  cd ${ROOT_DIR}/components/zss/bin
  ./zssServer.sh
else
  echo "Zowe ZSS server is unsupported on ${OSNAME}. Supported systems: OS/390"
fi