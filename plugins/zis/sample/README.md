This program and the accompanying materials are
made available under the terms of the Eclipse Public License v2.0 which accompanies
this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

SPDX-License-Identifier: EPL-2.0

Copyright Contributors to the Zowe Project.
___

# ZIS sample plugin

## Build

Run the following command to build both the plugin and the client application:
```shell
> ./build.sh
```

The plugin module `ZISSAMPL` will be placed into the `user_name.DEV.LOADLIB` 
data set and the client binary will be stored in `./bin`.

## Deploy

* Make sure the plugin module is in a data sets which is part of the STEPLIB
concatenation of your ZIS.
* Add the following line to your ZIS PARMLIB config member:
```
ZWES.PLUGIN.SMPL=ZISSAMPL
```
* Additionally, make sure you have the dynamic linkage plugin in your config:
```
ZWES.PLUGIN.DYNL=ZWESISDL
```

## Run

* Restart your ZIS
* Run the client with the ZIS name as the argument
```shell
> ./bin/sample_client <zis_name>
```
___

This program and the accompanying materials are
made available under the terms of the Eclipse Public License v2.0 which accompanies
this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

SPDX-License-Identifier: EPL-2.0

Copyright Contributors to the Zowe Project.
