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
# - NODE_HOME
if [ -n "$NODE_HOME" ]
then
  NODE_BIN=${NODE_HOME}/bin/node
else
  NODE_BIN=node
fi

cd ${ROOT_DIR}/components/app-server/share/zlux-app-server/bin
. ./convert-env.sh

cd ${ROOT_DIR}/components/app-server/share/zlux-app-server/lib
export NODE_PATH=../..:../../zlux-server-framework/node_modules:$NODE_PATH
__UNTAGGED_READ_MODE=V6 $NODE_BIN initInstance.js

APP_WORKSPACE_DIR=${INSTANCE_DIR}/workspace/app-server

# Add static definition for zss
cat <<EOF >${STATIC_DEF_CONFIG_DIR}/zss.ebcidic.yml
#
services:
  - serviceId: zss
    title: Zowe System Services Server
    description: Zowe System Services Server (ZSS) is used for enabling low-level microservices and other privileged services
    catalogUiTileId: zss
    instanceBaseUrls:
      - https://${ZOWE_EXPLORER_HOST}:${JOBS_API_PORT}/
    homePageRelativeUrl:
    routedServices:
      - gatewayUrl: api/v1
        serviceRelativeUrl: api/v1/jobs
      - gatewayUrl: api/v2
        serviceRelativeUrl: api/v2/jobs
 apiInfo:
      - apiId: com.ibm.jobs.v1
        gatewayUrl: api/v1
        version: 1.0.0
        swaggerUrl: https://${ZOWE_EXPLORER_HOST}:${JOBS_API_PORT}/v2/api-docs
        documentationUrl: https://${ZOWE_EXPLORER_HOST}:${JOBS_API_PORT}/swagger-ui.html
      - apiId: com.ibm.jobs.v2
        gatewayUrl: api/v2
        version: 2.0.0
        swaggerUrl: https://${ZOWE_EXPLORER_HOST}:${JOBS_API_PORT}/v2/api-docs
        documentationUrl: https://${ZOWE_EXPLORER_HOST}:${JOBS_API_PORT}/swagger-ui.html
catalogUiTiles:
  jobs:
    title: Zowe System Services Server (ZSS)
    description:  Zowe System Services Server for enabling low-level microservices and other privileged services
EOF
iconv -f IBM-1047 -t IBM-850 ${STATIC_DEF_CONFIG_DIR}/zss.ebcidic.yml > $STATIC_DEF_CONFIG_DIR/zss.yml
rm ${STATIC_DEF_CONFIG_DIR}/zss.ebcidic.yml
chmod 770 $STATIC_DEF_CONFIG_DIR/zss.yml