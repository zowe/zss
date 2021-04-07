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
  PRODUCT_DIR=$(cd "$PWD/../defaults" && pwd)
  SITE_DIR=$DESTINATION/site
  SITE_PLUGIN_STORAGE=$SITE_DIR/ZLUX/pluginStorage
  INSTANCE_PLUGIN_STORAGE=$DESTINATION/ZLUX/pluginStorage
  INSTANCE_CONFIG=$DESTINATION/serverConfig
  GROUPS_DIR=$DESTINATION/groups
  USERS_DIR=$DESTINATION/users
  PLUGINS_DIR=$DESTINATION/plugins

  sed 's@"productDir":"../defaults"@"productDir":"'${PRODUCT_DIR}'"@g' $currentJsonConfigPath > ${currentJsonConfigPath}.zwetmp1
  sed 's@"siteDir":"../deploy/site"@"siteDir":"'${SITE_DIR}'"@g' ${currentJsonConfigPath}.zwetmp1 > ${currentJsonConfigPath}.zwetmp2
  sed 's@"instanceDir":"../deploy/instance"@"instanceDir":"'${DESTINATION}'"@g' ${currentJsonConfigPath}.zwetmp2 > ${currentJsonConfigPath}.zwetmp1
  sed 's@"groupsDir":"../deploy/instance/groups"@"groupsDir":"'${GROUPS_DIR}'"@g' ${currentJsonConfigPath}.zwetmp1 > ${currentJsonConfigPath}.zwetmp2
  sed 's@"usersDir":"../deploy/instance/users"@"usersDir":"'${USERS_DIR}'"@g' ${currentJsonConfigPath}.zwetmp2 > ${currentJsonConfigPath}.zwetmp1
  sed 's@"pluginsDir":"../defaults/plugins"@"pluginsDir":"'${PLUGINS_DIR}'"@g' ${currentJsonConfigPath}.zwetmp1 > ${currentJsonConfigPath}
  rm ${currentJsonConfigPath}.zwetmp1 ${currentJsonConfigPath}.zwetmp2

  mkdir -p $SITE_PLUGIN_STORAGE
  mkdir -p $INSTANCE_PLUGIN_STORAGE
  mkdir -m 700 -p $INSTANCE_CONFIG
  chmod 700 $INSTANCE_CONFIG
  chmod 700 $INSTANCE_CONFIG/server.json
  mkdir -p $GROUPS_DIR
  mkdir -p $USERS_DIR
  mkdir -p $PLUGINS_DIR
fi

APP_WORKSPACE_DIR=${INSTANCE_DIR}/workspace/app-server
if [ "${ZOWE_ZSS_SERVER_TLS}" = "false" ]; then
  PROTOCOL="http"
else
  PROTOCOL="https"
fi

#if [ -n "${STATIC_DEF_CONFIG_DIR}" ]
#then

#version=`grep "version" ${INSTANCE_DIR}/workspace/manifest.json |  head -1 | sed -e 's/"//g' | sed -e 's/.*: *//g' | sed -e 's/,.*//g'`

# Add static definition for zss. TODO: Needs documentation
# cat <<EOF >${STATIC_DEF_CONFIG_DIR}/zss.ebcidic.yml
# #
# services:
#   - serviceId: zss
#     title: Zowe System Services (ZSS)
#     description: 'Zowe System Services is an HTTPS and Websocket server that makes it easy to have secure, powerful web APIs backed by low-level z/OS constructs. It contains services for essential z/OS abilities such as working with files, datasets, and ESMs, but is also extensible by REST and Websocket "Dataservices" which are optionally present in App Framework "Plugins".'
#     catalogUiTileId: zss
#     instanceBaseUrls:
#       - ${PROTOCOL}://${ZOWE_EXPLORER_HOST}:${ZOWE_ZSS_SERVER_PORT}/
#     homePageRelativeUrl:
#     routedServices:
#       - gatewayUrl: api/v1
#         serviceRelativeUrl: 
#     apiInfo:
#       - apiId: org.zowe.zss
#         gatewayUrl: api/v1
#         version: ${version}
#         # swaggerUrl: TODO
#         # documentationUrl: TODO
# catalogUiTiles:
#   zss:
#     title: Zowe System Services (ZSS)
#     description:  Zowe System Services is an HTTPS and Websocket server that makes it easy to have secure, powerful web APIs backed by low-level z/OS constructs.
# EOF
# iconv -f IBM-1047 -t IBM-850 ${STATIC_DEF_CONFIG_DIR}/zss.ebcidic.yml > $STATIC_DEF_CONFIG_DIR/zss.yml
# rm ${STATIC_DEF_CONFIG_DIR}/zss.ebcidic.yml
# chmod 770 $STATIC_DEF_CONFIG_DIR/zss.yml
# fi

if [ "${ZOWE_ZSS_SERVER_TLS}" = "false" ]
then
  # HTTP
  export "ZWED_agent_http_port=${ZOWE_ZSS_SERVER_PORT}"
else
  # HTTPS
  PREFIX="ZWED_agent_https_"
  export "${PREFIX}port=${ZOWE_ZSS_SERVER_PORT}"
  export "${PREFIX}label=${KEY_ALIAS}"
  IP_ADDRESSES_KEY_var="${PREFIX}ipAddresses"
  eval "IP_ADDRESSES_val=\"\$${IP_ADDRESSES_KEY_var}\""
  if [ -z "${IP_ADDRESSES_val}" ]; then
    export "${IP_ADDRESSES_KEY_var}"="${ZOWE_IP_ADDRESS}"
  fi
  if [ "${KEYSTORE_TYPE}" = "JCERACFKS" ]; then
    export "${PREFIX}keyring=${KEYRING_OWNER}/${KEYRING_NAME}"
  else
    export "${PREFIX}keyring=${KEYSTORE}"
    export "${PREFIX}password=${KEYSTORE_PASSWORD}"
  fi
fi
