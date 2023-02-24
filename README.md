This program and the accompanying materials are
made available under the terms of the Eclipse Public License v2.0 which accompanies
this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

SPDX-License-Identifier: EPL-2.0

Copyright Contributors to the Zowe Project.

# ZSS - Zowe System Services Server for enabling low-level microservices for z/OS

## How to build ZSS and the cross-memory server

To build ZSS and ZIS(cross-memory server), make sure submodules are updated, go to the build 
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

## Troubleshooting

When using ZSS as the agent to host files and folders, for example: for the Zowe Editor Desktop app by the App server, may lead to '401 Impersonator Error'
Fix: Make sure the program-controlled bit is set for your ZSS binary `extattr +p zssServer`

ZSS in V2 takes advantage of V2 by using schemas and the Zowe configuration YAML. May see an error on start like `ZSS 2.x requires schemas and config`
Fix: Make sure all relevant V2 environment variables are set. Here are some common ones:
`ZWES_COMPONENT_HOME=<your-zss-root-directory>`
`ZWE_zowe_runtimeDirectory=<your-zowe-runtime-directory>`
`ZWE_CLI_PARAMETER_CONFIG=<your-zowe-configuration-yaml>`

When starting ZSS, you may encounter a schema validation issue i.e. `Configuration has validity exceptions: Schema at '' invalid [...]`. 
Fix: To read these errors, consult: https://docs.zowe.org/stable/user-guide/configmgr-using/#validation-error-reporting
Note: ZSS has a default schema in `$ZWES_COMPONENT_HOME/schemas` and default configuration YAML in `$ZWES_COMPONENT_HOME/defaults.yaml`

## Mock server

If you don't have access to z/OS, or want to help expand the Mock server, find it at: https://github.com/zowe/zss/tree/v2.x/staging/mock

This program and the accompanying materials are
made available under the terms of the Eclipse Public License v2.0 which accompanies
this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

SPDX-License-Identifier: EPL-2.0

Copyright Contributors to the Zowe Project.

