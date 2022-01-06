This program and the accompanying materials are
made available under the terms of the Eclipse Public License v2.0 which accompanies
this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

SPDX-License-Identifier: EPL-2.0

Copyright Contributors to the Zowe Project.

# ZSS - Zowe System Services Server for enabling low-level microservices

change or remove this line

## How to build ZSS and the cross-memory server

To build ZSS and the cross-memory server, make sure submodules are updated, go to the build 
directory and run ant:

```
git submodule update --init
./build/build.sh
``` 

zssServer will be placed in the `bin` directory, the cross-memory server's load module will be placed 
in the `user-id.DEV.LOADLIB` dataset.

## How to submit a pull request

The [zowe-common-c](https://github.com/zowe/zowe-common-c) library is a dependency of ZSS.
If a ZSS pull request implies some changes of `zowe-common-c`, the changes should be merged
first, and the reference to the correspondent `zowe-common-c` commit should be included.  

For example:

```
cd deps/zowe-common-c
git checkout <ZOWE_COMMON_C_GIT_REFERENCE>
cd ../..
git add deps/zowe-common-c/
```  


This program and the accompanying materials are
made available under the terms of the Eclipse Public License v2.0 which accompanies
this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

SPDX-License-Identifier: EPL-2.0

Copyright Contributors to the Zowe Project.

