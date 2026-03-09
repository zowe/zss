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

COMPONENT_HOME=${ZWE_zowe_runtimeDirectory}/components/zss

# Some builtin plugins reference themselves via this, so we need to know where app-server is even if not running
export ZLUX_ROOT_DIR=${ZWE_zowe_runtimeDirectory}/components/app-server/share

if [ -n "${ZWE_components_zss_instanceDir}" ]; then
  mkdir -p "${ZWE_components_zss_instanceDir}"
fi
if [ -n "${ZWE_components_zss_pluginsDir}" ]; then
  mkdir -p "${ZWE_components_zss_pluginsDir}"
fi

if [ "${ZWE_components_app_server_enabled}" != "true" ]; then
  _CEE_RUNOPTS="XPLINK(ON),HEAPPOOLS(OFF),HEAPPOOLS64(OFF)" ${ZWE_zowe_runtimeDirectory}/bin/utils/configmgr -script "${ZWE_zowe_runtimeDirectory}/components/zss/bin/plugins-init.js"  
fi

# Register ZSS as a static APIML service when components.zss.agent.mediationLayer.static is enabled.
# ZWE_components_zss_agent_mediationLayer_static defaults to false; only
# proceed when it is explicitly set to true.
if [ "${ZWE_components_zss_agent_mediationLayer_static}" = "true" ]; then
  if [ -n "${ZWE_STATIC_DEFINITIONS_DIR}" ]; then
    apiml_static_def="${ZWE_STATIC_DEFINITIONS_DIR}/zss.apiml_static_reg_yaml_template.${ZWE_CLI_PARAMETER_HA_INSTANCE}.yml"
    apiml_static_src="${COMPONENT_HOME}/apiml-static-reg.yaml.template"
    parsed_def=$( ( echo "cat <<EOF" ; cat "${apiml_static_src}" ; echo ; echo EOF ) | sh 2>&1)
    echo "${parsed_def}" > "${apiml_static_def}"
    chmod 770 "${apiml_static_def}"
  fi
else
  if [ -n "${ZWE_STATIC_DEFINITIONS_DIR}" ]; then
    apiml_static_def="${ZWE_STATIC_DEFINITIONS_DIR}/zss.apiml_static_reg_yaml_template.${ZWE_CLI_PARAMETER_HA_INSTANCE}.yml"
    if [ -f "${apiml_static_def}" ]; then
      rm -f "${apiml_static_def}"
    fi
  fi
fi

