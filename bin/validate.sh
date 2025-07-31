#!/bin/sh
# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.

RUN_ON_ZOS=$(test `uname` = "OS/390" && echo "true")
if [ ! "${RUN_ON_ZOS}" = "true" ]; then
  echo "Error: ZSS can only be run on z/OS, but validation detected a different OS from uname."
  exit 1
else
  check_proclib=false
  if [ -n "${ZWE_zowe_setup_dataset_proclib}" ]; then
    if [ -n "${ZWE_zowe_setup_security_stcs_zis}" ]; then
      check_proclib=true
    fi
  fi

  if [ "${check_proclib}" = "true" ]; then
    "${ZWE_zowe_runtimeDirectory}/bin/utils/zis-test" --zis "${ZWE_components_zss_crossMemoryServerName}" --proclib "${ZWE_zowe_setup_dataset_proclib}" --stc "${ZWE_zowe_setup_security_stcs_zis}"
  else            
    "${ZWE_zowe_runtimeDirectory}/bin/utils/zis-test" --zis "${ZWE_components_zss_crossMemoryServerName}"
  fi
  rc=$?
  exit $rc
fi
