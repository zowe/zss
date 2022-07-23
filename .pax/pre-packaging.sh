#!/bin/sh -e
set -xe

################################################################################
# This program and the accompanying materials are made available under the terms of the
# Eclipse Public License v2.0 which accompanies this distribution, and is available at
# https://www.eclipse.org/legal/epl-v20.html
#
# SPDX-License-Identifier: EPL-2.0
#
# Copyright Contributors to the Zowe Project.
################################################################################


# contants
SCRIPT_NAME=$(basename "$0")
SCRIPT_DIR=$(pwd)

# build
echo "$SCRIPT_NAME build zss ..."
STEPLIB=CBC.SCCNCMP "$SCRIPT_DIR/content/build/build.sh"

# clean up content folder
echo "$SCRIPT_NAME cleaning up pax folder ..."
cd "$SCRIPT_DIR"
mv content bak && mkdir -p content

# move real files to the content folder
echo "$SCRIPT_NAME coping files should be in pax ..."
cd "$SCRIPT_DIR/content"
mkdir LOADLIB SAMPLIB
cp -X "//DEV.LOADLIB(ZWESIS01)" LOADLIB/ZWESIS01
cp -X "//DEV.LOADLIB(ZWESAUX)" LOADLIB/ZWESAUX
cp ../bak/samplib/zis/* SAMPLIB
cp -r ../bak/bin .
cp ../bak/manifest.yaml .
cp ../bak/defaults.yaml .
cp -r ../bak/schemas .
cp ../bak/apiml-static-reg.yaml.template .
extattr +p bin/zssServer
extattr +p bin/zssServer64
