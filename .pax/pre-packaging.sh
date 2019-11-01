#!/bin/sh -e
set -x

################################################################################
# This program and the accompanying materials are made available under the terms of the
# Eclipse Public License v2.0 which accompanies this distribution, and is available at
# https://www.eclipse.org/legal/epl-v20.html
#
# SPDX-License-Identifier: EPL-2.0
#
# Copyright IBM Corporation 2018, 2019
################################################################################

# contants
SCRIPT_NAME=$(basename "$0")
SCRIPT_DIR=$(dirname "$0")
# these are folders on build server
export JAVA_HOME=/usr/lpp/java/J8.0_64
export ANT_HOME=/ZOWE/apache-ant-1.10.5
export STEPLIB=CBC.SCCNCMP

# build
echo "$SCRIPT_NAME build zss ..."
cd "$SCRIPT_DIR/content/build" && ${ANT_HOME}/bin/ant

# clean up content folder
echo "$SCRIPT_NAME cleaning up pax folder ..."
cd "$SCRIPT_DIR"
mv content bak && mkdir -p content

# move real files to the content folder
echo "$SCRIPT_NAME coping files should be in pax ..."
cd content
mkdir LOADLIB SAMPLIB &&
  cp -X "//DEV.LOADLIB(ZWESIS01)" LOADLIB/ZWESIS01  &&
  cp -X "//DEV.LOADLIB(ZWESAUX)" LOADLIB/ZWESAUX  &&
  cp ../bak/samplib/zis/* SAMPLIB/  &&
  cp ../bak/bin/zssServer ./  &&
  extattr +p zssServer
