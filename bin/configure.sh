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

# this is to resolve (builtin) plugins that use ZLUX_ROOT_DIR as a relative path. if it doesnt exist, the plugins shouldn't either, so no problem
if [ -z "${ZLUX_ROOT_DIR}" ]; then
  if [ -d "${ZWE_zowe_runtimeDirectory}/components/app-server/share" ]; then
    export ZLUX_ROOT_DIR="${ZWE_zowe_runtimeDirectory}/components/app-server/share"
  fi
fi

# This location will have init and utils folders inside and is used to gather env vars and do plugin initialization
helper_script_location="${ZLUX_ROOT_DIR}/zlux-app-server/bin"

if [ -d "${helper_script_location}" ]; then
  cd "${helper_script_location}/init"
  . ../utils/convert-env.sh

  # Register/deregister plugins according to their enabled status
  NO_NODE=1
  . ./plugins-init.sh $NO_NODE

  cd "${COMPONENT_HOME}/bin"
fi

