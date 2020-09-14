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

# Add static definition for zss. TODO: Needs routedServices + apiInfo
cat <<EOF >${STATIC_DEF_CONFIG_DIR}/zss.ebcidic.yml
#
services:
  - serviceId: zss
    title: Zowe System Services Server (ZSS)
    description: Zowe System Services Server (ZSS) is used for enabling low-level microservices and other privileged services
    catalogUiTileId: zss
    instanceBaseUrls:
      - https://${ZOWE_EXPLORER_HOST}:${ZOWE_ZSS_SERVER_PORT}/
    homePageRelativeUrl:
    routedServices:
      - gatewayUrl: /server/agent/environment # Displays ZSS host environment info
        serviceRelativeUrl: /server/agent/environment 
      - gatewayUrl: /server/agent/services  # Returns list of ZSS services
        serviceRelativeUrl: /server/agent/services
      - gatewayUrl: /plugins
        serviceRelativeUrl: /plugins
      - gatewayUrl: /datasetMetadata/
        serviceRelativeUrl: /datasetMetadata/
      - gatewayUrl: /datasetContents/
        serviceRelativeUrl: /datasetContents/
      - gatewayUrl: /security-mgmt/classes/
        serviceRelativeUrl: /security-mgmt/classes/
      - gatewayUrl: /security-mgmt/access/
        serviceRelativeUrl: /security-mgmt/access/
      - gatewayUrl: /security-mgmt/groups/
        serviceRelativeUrl: /security-mgmt/groups/
      - gatewayUrl: /security-mgmt/user-profiles/
        serviceRelativeUrl: /security-mgmt/user-profiles/
      - gatewayUrl: /saf-auth/
        serviceRelativeUrl: /saf-auth/
      - gatewayUrl: /VSAMdatasetContents/
        serviceRelativeUrl: /VSAMdatasetContents/
      - gatewayUrl: /unixfile/contents
        serviceRelativeUrl: /unixfile/contents
      - gatewayUrl: /unixfile/chmod/
        serviceRelativeUrl: /unixfile/chmod/
      - gatewayUrl: /unixfile/chtag/
        serviceRelativeUrl: /unixfile/chtag/
      - gatewayUrl: /unixfile/chown/
        serviceRelativeUrl: /unixfile/chown/
      - gatewayUrl: /unixfile/metadata/
        serviceRelativeUrl: /unixfile/metadata/
      - gatewayUrl: /unixfile/touch/
        serviceRelativeUrl: /unixfile/touch/
      - gatewayUrl: /unixfile/mkdir/
        serviceRelativeUrl: /unixfile/mkdir/
      - gatewayUrl: /unixfile/copy/
        serviceRelativeUrl: /unixfile/copy/
      - gatewayUrl: /unixfile/rename/
        serviceRelativeUrl: /unixfile/rename/
      - gatewayUrl: /login/
        serviceRelativeUrl: /login/
      - gatewayUrl: /logout/
        serviceRelativeUrl: /logout/
      - gatewayUrl: /ras/
        serviceRelativeUrl: /ras/
      - gatewayUrl: /password/
        serviceRelativeUrl: /password/
      - gatewayUrl: /omvs/
        serviceRelativeUrl: /omvs/
      - gatewayUrl: /unixfile/
        serviceRelativeUrl: /unixfile/
    apiInfo:
      - apiId: org.zowe.zss
        gatewayUrl: server/agent/
        version: 1.15.0+20200901
        # swaggerUrl: TODO
        # documentationUrl: TODO
catalogUiTiles:
  zss:
    title: Zowe System Services Server (ZSS)
    description:  Zowe System Services Server for enabling low-level microservices and other privileged services
EOF
iconv -f IBM-1047 -t IBM-850 ${STATIC_DEF_CONFIG_DIR}/zss.ebcidic.yml > $STATIC_DEF_CONFIG_DIR/zss.yml
rm ${STATIC_DEF_CONFIG_DIR}/zss.ebcidic.yml
chmod 770 $STATIC_DEF_CONFIG_DIR/zss.yml