#!/bin/sh
set -e

################################################################################
#  This program and the accompanying materials are
#  made available under the terms of the Eclipse Public License v2.0 which accompanies
#  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
#  SPDX-License-Identifier: EPL-2.0
#
#  Copyright Contributors to the Zowe Project.
################################################################################


WORKING_DIR=$(dirname "$0")


if [ -n "$1" ] && [ "$1" != "zss" ] && [ "$1" != "zis" ] || [ -n "$2" ]; then
  echo "Unknown options"
  echo "Usage: build.sh [zis|zss]"
  exit 2
fi

if [ "$1" = "zss" ] || [ "$1" = "" ]; then
  "${WORKING_DIR}/build_zss.sh"
fi

if [ "$1" = "zis" ] || [ "$1" = "" ]; then
  "${WORKING_DIR}/build_zis.sh"
fi
