#  This program and the accompanying materials are
#  made available under the terms of the Eclipse Public License v2.0 which accompanies
#  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#  
#  SPDX-License-Identifier: EPL-2.0
#  
#  Copyright Contributors to the Zowe Project.

#! /bin/sh
sed 's/https:\/\/github.com\/zowe\//git@github.com:zowe\//' .gitmodules > .gitmodules2 && mv .gitmodules2 .gitmodules
git add .gitmodules

git submodule deinit deps/zowe-common-c
git submodule update --init deps/zowe-common-c
git reset HEAD .gitmodules
git checkout -- .gitmodules
