This program and the accompanying materials are
made available under the terms of the Eclipse Public License v2.0 which accompanies
this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

SPDX-License-Identifier: EPL-2.0

Copyright Contributors to the Zowe Project.

# ZIS plugin development

<!-- TOC -->
* [ZIS plugin development](#zis-plugin-development)
  * [Preface](#preface)
  * [Terminology](#terminology)
  * [Overview](#overview)
    * [What is Zowe Cross-Memory Server?](#what-is-zowe-cross-memory-server)
    * [When do I need ZIS?](#when-do-i-need-zis)
    * [Extending ZIS](#extending-zis)
  * [Plugin structure](#plugin-structure)
    * [Module](#module)
    * [Services](#services)
    * [Callbacks](#callbacks)
  * [Plugin lifecycle](#plugin-lifecycle)
    * [ZIS startup](#zis-startup)
    * [ZIS termination](#zis-termination)
    * [Service call](#service-call)
    * [z/OS operator modify command invocation](#zos-operator-modify-command-invocation)
  * [Making a plugin](#making-a-plugin)
    * [Plugin descriptor](#plugin-descriptor)
        * [Plugin name](#plugin-name)
        * [Plugin nickname](#plugin-nickname)
        * [Plugin flags](#plugin-flags)
        * [Plugin version](#plugin-version)
    * [Plugin services](#plugin-services)
        * [Service name](#service-name)
        * [Service flags](#service-flags)
        * [Service serve function](#service-serve-function)
    * [Putting it all together](#putting-it-all-together)
  * [Building a plugin](#building-a-plugin)
    * [Compilation](#compilation)
      * [Compiler requirements](#compiler-requirements)
    * [Assembly](#assembly)
      * [Assembler requirements](#assembler-requirements)
      * [Recommended options](#recommended-options)
    * [Linking](#linking)
      * [Linker requirement](#linker-requirement)
    * [Dynamic linkage stub](#dynamic-linkage-stub)
  * [Deploying a plugin](#deploying-a-plugin)
    * [Dynamic linkage considerations](#dynamic-linkage-considerations)
  * [Plugin details](#plugin-details)
    * [Serve function environment](#serve-function-environment)
    * [Other callbacks' environment](#other-callbacks-environment)
      * [User parameter list](#user-parameter-list)
      * [Accessing caller's data](#accessing-callers-data)
      * [Function return codes](#function-return-codes)
    * [PC-ss vs PC-cp service](#pc-ss-vs-pc-cp-service)
        * [Notes](#notes)
    * [Plugin security](#plugin-security)
  * [Dev mode](#dev-mode)
    * [General ZIS dev mode](#general-zis-dev-mode)
    * [LPA dev mode](#lpa-dev-mode)
<!-- TOC -->

## Preface

**IMPORTANT:** Zowe Cross-Memory Server plugins execute with elevated 
privileges, writing code for plugins requires you to be familiar with the
operating system, principles of cross memory communication and implications of 
running your code in an authorized environment.

Before making a new plugin, we recommend that extenders read the following
sources:
* [Synchronous cross memory communication](https://www.ibm.com/docs/en/zos/2.5.0?topic=guide-synchronous-cross-memory-communication)

## Terminology

Before we start, we need to clarify some abbreviations and terminology.
* A/S - address space
* HASN/PASN/SASN - the home/primary/secondary address space
* HLASM - High Level Assembler
* LPA - the link pack area
* PC - program call
  * PC-ss - space-switch PC
  * PC-cp - current-primary PC
* ZIS - Zowe Cross-Memory Server

## Overview

### What is Zowe Cross-Memory Server?
* Zowe Cross-Memory Server (also known as ZIS) is an authorized server 
application that provides both privileged services and cross-memory services to unprivileged
applications on z/OS in a secure manner.
* Each ZIS instance is identified by a unique 16-character name (LPAR wide)
* ZIS runs as a started task

### When do I need ZIS?
* Your service code requires a system key, supervisor state and/or 
APF-authorization
* You want to provide services to applications that can’t use HTTP or the
standard IPC mechanisms

### Extending ZIS
ZIS has the ability to be extended with third-party plugins. Usually, a plugin 
consists of a single load module and a line of configuration. This document 
will specifically go into details on how to develop and use ZIS plugins.

Throughout this document we'll use snippets from the sample plugin code in [zss/plugins/zis/sample/](https://github.com/zowe/zss/tree/974fc01526b49f1c692f96a71a22c7dc11eb36e8/plugins/zis/sample).
Refer to that source for the full plugin sample.

## Plugin structure

### Module
A ZIS plugin is a load module with the following characteristics:
* AMODE 64
* Reentrant
* The result of call a ZIS plugin lod module is a plugin descriptor data 
structure mapped by the C struct [`ZISPlugin`](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/plugin.h#L81-L112)
 
### Services
Each plugin consists of 0 or more services which are invoked via the PC 
instruction (program call).

### Callbacks

There are several callbacks plugin developers can set to better control
the lifecycle of plugins and services. All the callbacks except for the "serve"
service callback are invoked inside the ZIS address space. Depending on the 
service configuration the "serve" callback is invoked in either the caller's 
address space or the ZIS address space

Plugin callbacks:
* **init**: invoked upon ZIS startup
* **term**: invoked upon ZIS termination
* **command handler**: invoked when a ZIS modify command uses the plugin as its
target

Service callbacks:
* **init**: invoked upon ZIS startup
* **term**: invoked upon ZIS termination
* **serve**: invoked when the service is called

## Plugin lifecycle

### ZIS startup
For each plugin:
* Load the plugin module into the ZIS A/S private storage
* Call the entry point
* Process the result `ZISPlugin` struct and relocate the plugin module to LPA if 
required (controlled by plugin parameters)
* Call the plugin "init" function
* For each plugin service, call the service "init" function

When done ZIS sets the "ready" flag and accepts requests

### ZIS termination
First, ZIS unsets the "ready" flag and stops accepting requests.
Then, for each plugin:
* For each service, call the service "term" function
* Call the plugin "term" function

Any plugin module that has been loaded to LPA will remain in LPA. 

### Service call
* ZIS looks up the service using the caller-specified plugin and service name
* In the corresponding program call handler ZIS invokes the "serve" function of 
the service

### z/OS operator modify command invocation
* ZIS looks up the plugin using the modify command target, which is a plugin
nickname
* Call the command handler of the target plugin

There are additional steps in dev mode which are described in the 
[Dev mode](#dev-mode) section.

## Making a plugin

This section will describe how to create a ZIS plugin using snippets from
the sample plugin as examples. The sample plugin has a single service that 
returns the content of the control registers to a problem state caller (a simple
LE client application). The register content is read using the `STCTG` HLASM 
instruction, which is a privileged operation and cannot be performed inside 
problem state applications.

### Plugin descriptor

A valid plugin must return the address of the [`ZISPlugin`](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/plugin.h#L81-L112)
struct describing the plugin itself and its services. That struct is created 
using the [`zisCreatePlugin`](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/plugin.h#L139)
function.

Here's an example of how to use it:
```c
ZISPlugin *plugin = zisCreatePlugin(
  (ZISPluginName) {SAMPLE_PLUGIN_NAME},
  (ZISPluginNickname) {SAMPLE_PLUGIN_NICKNAME},
  NULL, // plugin "init" function
  NULL, // plugin "term" function
  NULL, // plugin "modify command" handler
  1, // plugin version
  1, // number of services
  ZIS_PLUGIN_FLAG_NONE // plugin flags
);
```

For now the most important parameters are the plugin name, nickname, flags and
version.

##### Plugin name
* Mapped by the [`ZISPluginName`](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/plugin.h#L40)
struct
* A 16-character printable EBCDIC string
* Uniquely identifies a plugin within ZIS
##### Plugin nickname
* Mapped by the [`ZISPluginNickname`](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/plugin.h#L45)
struct
* A 4-character printable EBCDIC string
* Uniquely identifies a plugin for modify commands within ZIS
##### Plugin flags
* Control various aspects of a plugin
* By default, plugin modules are only loaded into the private ZIS storage, but
when the [`ZIS_PLUGIN_FLAG_LPA`](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/plugin.h#L63) 
flag is set, the module will be loaded to LPA
##### Plugin version
* ZIS uses the plugin version value to refresh the plugin module in LPA if
needed, i.e. when a different version is detected, the current LPA module is
discarded and the new version is loaded

**IMPORTANT:** when any changes to a plugin or its service are made, the version
must be incremented, so that the LPA module gets refreshed. During development
process that can be avoided, see [LPA dev mode](#lpa-dev-mode).

Please refer to the [`zisCreatePlugin` doc](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/plugin.h#L124-L138) 
for more details.

### Plugin services

Once we have our plugin descriptor, we can start creating and adding services to
the plugin.

A service can be created with the [`zisCreateService`](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/service.h#L137) 
function.

```c
ZISService service = zisCreateService(
    (ZISServiceName) {GETCR_SERVICE_NAME},
    ZIS_SERVICE_FLAG_SPACE_SWITCH, // service flags
    NULL, // service "init" function
    NULL, // service "term" function
    serveControlRegisters, // service "serve" function
    1 // service version
);
```

The parameters we're interested in at the moment are the service name, flags
and the "serve" function (see more details in [`zisCreateService` doc](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/service.h#L126-L136)
).

##### Service name
* Mapped by the [`ZISServiceName`](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/service.h#L42)
struct
* A 16-character printable EBCDIC string
* Uniquely identifies a service within its plugin
##### Service flags
* Control various aspects of a service
* By default, services are invoked in the PC-cp handler, to make a service
space-switch, use `ZIS_SERVICE_FLAG_SPACE_SWITCH` (see details in [this](#pc-ss-vs-pc-cp-service) 
section)
* You can also protect a service with an additional SAF check by setting the
`ZIS_SERVICE_FLAG_SPECIFIC_AUTH` flag (see details in [this](#plugin-security) 
section)
##### Service serve function
* Invoked when a service is called
* Depending on the type of the service, it's either invoked in the PC-ss or 
PC-cp ZIS handler (i.e. either the ZIS A/S or caller's primary A/S)

After a service has been created, it should be added to the plugin descriptor 
using the [`zisPluginAddService`](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/plugin.h#L164) 
function.

### Putting it all together

When you have finished building the plugin descriptor and added all the 
services, the descriptor should be returned from the entry point function of 
your plugin. Below is a full example of such an entry point.

```c
ZISPlugin *getPluginDescriptor(void) {

  // this call will create a plugin data structure with the provided plugin 
  // name and nickname
  ZISPlugin *plugin = zisCreatePlugin(
      (ZISPluginName) {SAMPLE_PLUGIN_NAME},
      (ZISPluginNickname) {SAMPLE_PLUGIN_NICKNAME},
      NULL, // plugin "init" function
      NULL, // plugin "term" function
      NULL, // plugin "modify command" handler
      1, // plugin version
      1, // number of services
      ZIS_PLUGIN_FLAG_NONE // plugin flags
  );
  if (plugin == NULL) {
    return NULL;
  }

  // add a single service to our plugin
  ZISService service = zisCreateService(
      (ZISServiceName) {GETCR_SERVICE_NAME},
      ZIS_SERVICE_FLAG_SPACE_SWITCH, // service flags
      NULL, // service "init" function
      NULL, // service "term" function
      serveControlRegisters, // service "serve" function
      1 // service version
  );
  int addRC = zisPluginAddService(plugin, service);
  if (addRC != RC_ZIS_PLUGIN_OK) {
    zisDestroyPlugin(plugin);
    return NULL;
  }

  return plugin;
}
```

## Building a plugin

To build your plugin only the plugin sources will need to be compiled and
assembled. Considering the [dynamic linkage](#dynamic-linkage-stub) is used, all
the [zss](https://github.com/zowe/zss) and [zowe-common-c](https://github.com/zowe/zowe-common-c) 
functionality will be available dynamically and only the corresponding headers 
will be needed.

### Compilation

Plugin sources are compiler using the XL C/C++ compiler.

#### Compiler requirements
* Metal C (`-S -qmetal -DMETTLE=1`)
* 64-bit (`-q64`)
* Reserved GPR12 (`-qreserved_reg=r12`)
* Read-only constants (`-qroconst`)
* Long names (`-qlongname`)

### Assembly

Plugin objects are built using the `as` tool.

#### Assembler requirements
* GOFF (`-mgoff`)
* Produce object (`-mobject`)

#### Recommended options
* Check reenterability (`--RENT`)

### Linking

Plugin modules are linked using the `ld` linker.

#### Linker requirement
* Reenterable (`-b reus=rent`)
* Mixed case (`-b case=mixed`)

**There is no requirement to link-edit plugin load modules with AC(1).**

You can find the complete build script with all the options in the sample plugin 
(see [`zss/plugins/zis/sample/build.sh`](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/plugins/zis/sample/build.sh)
).

### Dynamic linkage stub

Starting from Zowe v2.4 ZIS plugins can use dynamic linkage. That is achieved
by linking plugin modules with a special HLASM-based stub. The stub is
generated using the tools in [`zss/tools/dynzis`](https://github.com/zowe/zss/tree/974fc01526b49f1c692f96a71a22c7dc11eb36e8/tools/dynzis).

An example of how to generate the stub.
```shell
# build the tool
javac -encoding iso8859-1 zss/tools/dynzis/org/zowe/zis/ZISStubGenerator.java
# generate the stub HLASM
java  -cp zss/tools/dynzis org.zowe.zis.ZISStubGenerator asm zss/h/zis/zisstubs.h > dynzis.s
```

Once the stub has been generated, assemble and link it with your plugin objects.

See [zss/tools/dynzis/README.md](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/tools/dynzis/README.md) 
for more details.

## Deploying a plugin

Put the plugin module in a data set in the ZIS STEPLIB concatenation and add 
the following line to the ZIS PARMLIB member:
```
ZWES.PLUGIN.id=module_name
```

Where 
* `id` is a string with no blanks, which is unique within the `ZWES.PLUGIN`
scope in your config member
* `module_name` is the module name of your plugin

### Dynamic linkage considerations

If your plugin utilizes the dynamic linkage, the dynamic linkage plugin must 
also be added to the config.

Add the following line to the config member to enable the dynamic linkage 
plugin:
```
ZWES.PLUGIN.DYNL=ZWESISDL
```

## Plugin details

### Serve function environment

Depending on the caller, the "serve" function is invoked in the following 
environment:
* Authorization: key 4 and supervisor state
* Dispatchable unit mode: task or SRB
* Cross memory mode:
  * If PC-ss, PASN will point to ZIS A/S and SASN will point to the caller's A/S
  * If PC-cp, both PASN and SASN will point to the caller's A/S
* AMODE: 64-bit
* ASC mode: primary
* Interrupt status: enabled or disabled for I/O and external interrupts
* Locks: any lock except CMS or no locks held
* Active RLE environment
  * Recovery context is established (either ESTAE or FRR depending on the 
  environment) and recovery functionality is available using [`recoveryPush`](https://github.com/zowe/zowe-common-c/blob/4f2732d0e781aec89b39f862262d5014a97c09b7/h/recovery.h#L487) 
  and [`recoveryPop`](https://github.com/zowe/zowe-common-c/blob/4f2732d0e781aec89b39f862262d5014a97c09b7/h/recovery.h#L503)
  * No logging context is created but logging is available using [`cmsPrintf`](https://github.com/zowe/zowe-common-c/blob/4f2732d0e781aec89b39f862262d5014a97c09b7/h/crossmemory.h#L517)

### Other callbacks' environment

The plugin and service "init", "term" functions and the plugin 
"command handler" function are invoked in the following environment:
* Authorization: APF-authorized, key 4 and problem state
* Dispatchable unit mode: task
* Cross memory mode: HASN=PASN=SASN
* AMODE: 64-bit
* ASC mode: primary
* Interrupt status: enabled for I/O and external interrupts
* Locks: no locks held
* Active RLE environment
  * Recovery context is established (ESTAE) and recovery functionality is 
  available using [`recoveryPush`](https://github.com/zowe/zowe-common-c/blob/4f2732d0e781aec89b39f862262d5014a97c09b7/h/recovery.h#L487) 
  and [`recoveryPop`](https://github.com/zowe/zowe-common-c/blob/4f2732d0e781aec89b39f862262d5014a97c09b7/h/recovery.h#L503)
  * Logging context is created and logging is available using [`zowelog`](https://github.com/zowe/zowe-common-c/blob/4f2732d0e781aec89b39f862262d5014a97c09b7/h/logging.h#L337)

#### User parameter list

ZIS will pass the address of the caller's parameter list in the caller's primary 
address space to the service "serve" function. However, it will be opaque. The 
plugin developer and the client need to agree of the parameter list format.

Here are some guidelines:
* Include an eye-catcher field which can be later validated by the service
* Include a version field for compatibility
* Make sure the parameter list has the same layout and size in Metal C 31-bit, 
Metal C 64-bit, LE 31-bit, etc. That can be done using `__packed` or the [`ZOWE_PARGMA_PACK`/`ZOWE_PRAGMA_PACK_RESET` macros](https://github.com/zowe/zowe-common-c/blob/4f2732d0e781aec89b39f862262d5014a97c09b7/h/zowetypes.h#L178-L179).

The following is an example of a parameter list taken from the sample:
```c
typedef __packed struct GetCRServiceParm_tag {
#define GETCR_SERVICE_PARM_EYECATCHER "GETCRPRM"
#define GETCR_SERVICE_PARM_VERSION 1
  char eyecatcher[8];
  uint8_t version;
  ControlRegisters result;
} GetCRServiceParm;
```

#### Accessing caller's data

All the virtual storage from the caller's address space, including the parameter
list, must be read using the [`cmCopyFromSecondaryWithCallerKey`](https://github.com/zowe/zowe-common-c/blob/4f2732d0e781aec89b39f862262d5014a97c09b7/h/cmutils.h#L104)
function and written to using the [`cmCopyToSecondaryWithCallerKey`](https://github.com/zowe/zowe-common-c/blob/4f2732d0e781aec89b39f862262d5014a97c09b7/h/cmutils.h#L92)
function. Among other things, this will ensure that a malicious caller will not 
use ZIS to access storage using ZIS'es privileged key.

An example of how to copy a parameter list and return results:
```c

void *serviceParmList = ...; // parameter list in the caller's A/S 
GetCRServiceParm localParmList; // parameter list in the primary A/S

cmCopyFromSecondaryWithCallerKey(&localParmList, serviceParmList,
                                 sizeof(localParmList));sa
. . .
cmCopyToSecondaryWithCallerKey(serviceParmList, &localParmList,
                               sizeof(localParmList));
```

#### Function return codes

Your "serve" function must return [`RC_ZIS_SRVC_OK`](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/service.h#L201) 
in case of success and for any other return values the value larger than 
[`ZIS_MAX_GEN_SRVC_RC`](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/service.h#L215) 
must be used.

For example:
```c
#define RC_GETCR_OK                 RC_ZIS_SRVC_OK
#define RC_GETCR_BAD_EYECATCHER     (ZIS_MAX_GEN_SRVC_RC + 1)
#define RC_GETCR_BAD_VERSION        (ZIS_MAX_GEN_SRVC_RC + 2)
```

The values in the range (`RC_ZIS_SRVC_OK`, `ZIS_MAX_GEN_SRVC_RC`] are reserved.  

### PC-ss vs PC-cp service

Depending on the nature of your service, set the [`ZIS_SERVICE_FLAG_SPACE_SWITCH`](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/service.h#L96) 
flag so that the service is invoked in either the PC-ss or PC-cp ZIS program 
call handler. The main differences between PC-ss and PC-cp are described below.

| Feature                                                       | PC-ss | PC-cp | Examples                              |
|---------------------------------------------------------------|-------|-------|---------------------------------------|
| Access to resources inside or owned by ZIS A/S                | Yes   | No    | Allocating storage inside the ZIS A/S |
| Code isolation                                                | Yes   | No    |                                       |
| Service code in private storage <sup>1</sup>                  | Yes   | No    |                                       |
| Execute services that are not supported in cross-memory mode  | No    | Yes   | Calling an SVC, performing basic I/O  |

##### Notes
1. The code used by a PC-cp service must be in commonly addressable storage 
(to facilitate that ZIS can automatically place the plugin module in LPA, see
[`ZIS_PLUGIN_FLAG_LPA`](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/plugin.h#L90))

### Plugin security

Every ZIS caller must have READ access to the "ZWES.IS" profile in the "FACILITY"
class. In addition to that, plugin developers can add another SAF check via
the [`zisServiceUseSpecificAuth`](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/service.h#L187) function.

First, set the [`ZIS_SERVICE_FLAG_SPECIFIC_AUTH`](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/service.h#L97) 
flag when creating a service, then call `zisServiceUseSpecificAuth` to set the 
SAF profile to which you wish the callers' READ access to be checked
when calling the service.

The SAF profile must be in a RACLISTed class, that is, be present in virtual 
storage as opposed to just being in the RACF database on disk.

## Dev mode

When loading a new version of a plugin ZIS discards the previous version of
the plugin module and never deletes it from LPA. This is done for security and
integrity reasons. However, during development process you usually rebuild your 
plugin multiple times and that would be wasteful to always leave the old version
in LPA, because LPA is a very limited resource. To prevent LPA exhaustion and 
avoid wasting some other common resources ZIS has introduced various development
modes.

**IMPORTANT:** never use the development modes in production as they might lead
to system integrity exposures.

### General ZIS dev mode

This is a dev mode which when enabled will include the following modes:
* [LPA dev mode](#lpa-dev-mode)

To enable the general ZIS dev mode, add the following line to the config member:
```
ZWES.DEV_MODE=YES
```

### LPA dev mode

When the LPA dev mode is on, at startup and termination ZIS will remove the old 
version of the ZIS load module and any plugin modules from LPA.

To enable the LPA dev mode, add the following line to the config member:
```
ZWES.DEV_MODE.LPA=YES
```

You should see the following messages IDs when the dev mode is on:
* [ZWES0247W](https://github.com/zowe/zowe-common-c/blob/4f2732d0e781aec89b39f862262d5014a97c09b7/h/crossmemory.h#L1043-L1047)
* [ZWES0213W](https://github.com/zowe/zss/blob/974fc01526b49f1c692f96a71a22c7dc11eb36e8/h/zis/message.h#L303-L305) 
(if the dynamic linkage plugin is used)

Additionally, the dynamic linkage plugin will remove its old stub vector from
common storage, since it can no longer reference the old LPA plugin module. 
