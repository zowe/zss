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
# - ZWE_zowe_workspaceDirectory
# - NODE_HOME
#
# Optional variables on shell (from start-app-server.sh):
# - APIML_ENABLE_SSO
# - GATEWAY_PORT
# - DISCOVERY_PORT
# - ZWED_SSH_PORT
# - ZWED_TN3270_PORT
# - ZWED_TN3270_SECURITY

OSNAME=$(uname)
if [[ "${OSNAME}" == "OS/390" ]]; then
  export _EDC_ZERO_RECLEN=Y # allow processing of zero-length records in datasets
  cd ${ZWE_zowe_runtimeDirectory}/components/zss/bin
  ./zssServer.sh
else
  echo "Zowe ZSS server is unsupported on ${OSNAME}. Supported systems: OS/390"
fi