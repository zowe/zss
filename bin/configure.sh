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

cd ${ROOT_DIR}/components/app-server/share/zlux-app-server/bin
. ./convert-env.sh

cd ${ROOT_DIR}/components/app-server/share/zlux-app-server/lib

if [ -n "$INSTANCE_DIR" ]
then
  INSTANCE_LOCATION=$INSTANCE_DIR
else
  INSTANCE_LOCATION=$HOME/.zowe
fi
if [ -n "$WORKSPACE_DIR" ]
then
  WORKSPACE_LOCATION=$WORKSPACE_DIR
else
  WORKSPACE_LOCATION=$INSTANCE_LOCATION/workspace
fi
DESTINATION=$WORKSPACE_LOCATION/app-server

currentJsonConfigPath=$DESTINATION/serverConfig/server.json

if [ ! -f "$currentJsonConfigPath" ]; then
  mkdir -p $DESTINATION/serverConfig
  cp ../defaults/serverConfig/server.json $DESTINATION/serverConfig/server.json
  PRODUCT_DIR=$PWD../defaults
  SITE_DIR=$DESTINATION/site
  SITE_PLUGIN_STORAGE=$SITE_DIR/ZLUX/pluginStorage
  INSTANCE_PLUGIN_STORAGE=$DESTINATION/ZLUX/pluginStorage
  INSTANCE_CONFIG=$DESTINATION/serverConfig
  GROUPS_DIR=$DESTINATION/groups
  USERS_DIR=$DESTINATION/users
  PLUGINS_DIR=$DESTINATION/plugins

  sed 's@"productDir":"../defaults"@"productDir":"$PRODUCT_DIR"@g' $currentJsonConfigPath
  sed 's@"siteDir":"../deploy/site"@"siteDir":"$SITE_DIR"@g' $currentJsonConfigPath
  sed 's@"instanceDir":"../deploy/instance"@"instanceDIR":"$DESTINATION"@g' $currentJsonConfigPath
  sed 's@"groupsDir":"../deploy/instance/groups"@"groupsDir":"$GROUPS_DIR"@g' $currentJsonConfigPath
  sed 's@"usersDir":"../deploy/instance/users"@"usersDir":"$USERS_DIR"@g' $currentJsonConfigPath
  sed 's@"pluginsDir":"../defaults/plugins"@"pluginsDir":"$PLUGINS_DIR"@g' $currentJsonConfigPath

  mkdir -p $SITE_PLUGIN_STORAGE
  mkdir -p $INSTANCE_PLUGIN_STORAGE
  mkdir -m 700 -p $INSTANCE_CONFIG
  mkdir -p $GROUPS_DIR
  mkdir -p $USERS_DIR
  mkdir -p $PLUGINS_DIR
fi

APP_WORKSPACE_DIR=${INSTANCE_DIR}/workspace/app-server

version=`grep "version" ${INSTANCE_DIR}/workspace/manifest.json |  head -1 | sed -e 's/"//g' | sed -e 's/.*: *//g' | sed -e 's/,.*//g'`

# Add static definition for zss. TODO: Needs documentation
cat <<EOF >${STATIC_DEF_CONFIG_DIR}/zss.ebcidic.yml
#
services:
  - serviceId: zss
    title: Zowe System Services (ZSS)
    description: Zowe System Services is used for enabling low-level microservices and other privileged mainframe services, like USS, MVS, authentication, and security management.
    catalogUiTileId: zss
    instanceBaseUrls:
      - http://${ZOWE_EXPLORER_HOST}:${ZOWE_ZSS_SERVER_PORT}/
    homePageRelativeUrl:
    routedServices:
      - gatewayUrl: api/v1
        serviceRelativeUrl: 
    apiInfo:
      - apiId: org.zowe.zss
        gatewayUrl: api/v1
        version: ${version}
        # swaggerUrl: TODO
        # documentationUrl: TODO
catalogUiTiles:
  zss:
    title: Zowe System Services (ZSS)
    description:  Zowe System Services is used for enabling low-level microservices and other privileged mainframe services, like USS, MVS, authentication, and security management.
EOF
iconv -f IBM-1047 -t IBM-850 ${STATIC_DEF_CONFIG_DIR}/zss.ebcidic.yml > $STATIC_DEF_CONFIG_DIR/zss.yml
rm ${STATIC_DEF_CONFIG_DIR}/zss.ebcidic.yml
chmod 770 $STATIC_DEF_CONFIG_DIR/zss.yml