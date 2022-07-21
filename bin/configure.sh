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

if [ -n "${ZWE_components_zss_instanceDir}" ]; then
  mkdir -p "${ZWE_components_zss_instanceDir}"
fi
if [ -n "${ZWE_components_zss_pluginsDir}" ]; then
  mkdir -p "${ZWE_components_zss_pluginsDir}"
fi

if [ "${ZWE_components_app_server_enabled}" != "true" ]; then
  _CEE_RUNOPTS="XPLINK(ON),HEAPPOOLS(OFF)" ${ZWE_zowe_runtimeDirectory}/bin/utils/configmgr -script "${ZWE_zowe_runtimeDirectory}/components/zss/bin/plugins-init.js"  
fi

