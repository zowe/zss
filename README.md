This program and the accompanying materials are
made available under the terms of the Eclipse Public License v2.0 which accompanies
this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

SPDX-License-Identifier: EPL-2.0

Copyright Contributors to the Zowe Project.

# ZSS - Zowe System Services Server for enabling low-level microservices for z/OS

## (Quick start) How to build ZSS and the cross-memory server

```
git clone git@github.com:zowe/zss.git
cd zss
git submodule update --init
./build/build.sh
``` 

Note: zssServer will be placed in the `bin` directory, the cross-memory server's load module will be placed 
in the `user-id.DEV.LOADLIB` dataset.

## (Quick run) How to start ZSS

Zowe core schemas are needed to run, so the quickest way to get them as a developer is to clone the repo they come from.

```
git clone git@github.com:zowe/zowe-install-packaging.git
```
Then, tell ZSS where to find the schemas and the configs and then start it up, like so:
```
cd /path/to/zss/bin
export ZWES_COMPONENT_HOME=/path/to/zss
export ZWE_zowe_runtimeDirectory=/path/to/zowe-install-packaging
ZWE_CLI_PARAMETER_CONFIG="FILE(/my/zowe.yaml)" ./zssServer.sh
```

Note: ZSS defaults are in [defaults.yaml](https://github.com/zowe/zss/blob/v3.x/staging/defaults.yaml) so you only need to provide customizations in your own zowe.yaml.

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

## Troubleshooting

When using ZSS as the agent to host files and folders, for example: for the Zowe Editor Desktop app by the App server, may lead to '401 Impersonator Error'
Fix: Make sure the program-controlled bit is set for your ZSS binary `extattr +p zssServer`

ZSS in V3 takes advantage of V3 by using schemas and the Zowe configuration YAML. If you're running `zssServer` accidentally, instead of `zssServer.sh` or your `zssServer.sh` is out of date, you may see an error on start like `ZSS 2.x requires schemas and config`

When starting ZSS, you may encounter a schema validation issue i.e. `Configuration has validity exceptions: Schema at '' invalid [...]`. 
Fix: To read these errors, consult: https://docs.zowe.org/stable/user-guide/configmgr-using/#validation-error-reporting
Note: ZSS has a default schema in `$ZWES_COMPONENT_HOME/schemas` and default configuration YAML in `$ZWES_COMPONENT_HOME/defaults.yaml`

## Mock server

If you don't have access to z/OS, or want to help expand the Mock server, find it at: https://github.com/zowe/zss/tree/v3.x/staging/mock

This program and the accompanying materials are
made available under the terms of the Eclipse Public License v2.0 which accompanies
this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

SPDX-License-Identifier: EPL-2.0

Copyright Contributors to the Zowe Project.

