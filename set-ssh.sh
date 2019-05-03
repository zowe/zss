#! /bin/sh
sed 's/https:\/\/github.com\/zowe\//git@github.com:zowe\//' .gitmodules > .gitmodules2 && mv .gitmodules2 .gitmodules
git add .gitmodules

git submodule deinit deps/zowe-common-c
git submodule update --init deps/zowe-common-c
git reset HEAD .gitmodules
git checkout -- .gitmodules
