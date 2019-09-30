

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/string.h>

#ifndef METTLE
#error Non-metal C code is not supported
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "crossmemory.h"

#include "aux-guest.h"

#ifndef _LP64
#error LP64 is supported only
#endif

ZISAUXModuleDescriptor *
zisAUXMakeModuleDescriptor(ZISAUXInitFunction *initFunction,
                           ZISAUXTermFunction *termFunction,
                           ZISAUXCancelHandlerFunction *handleCancelFunciton,
                           ZISAUXCommandHandlerFunction *handleCommandFunction,
                           ZISAUXWorkHandlerFunction *handleWorkFunction,
                           void *moduleData, int moduleVersion) {

  unsigned allocSize = sizeof(ZISAUXModuleDescriptor);

  ZISAUXModuleDescriptor *descriptor =
      (ZISAUXModuleDescriptor *)safeMalloc(allocSize, "AUX module descriptor");

  if (descriptor == NULL) {
    return NULL;
  }

  memset(descriptor, 0, allocSize);
  memcpy(descriptor->eyecatcher, ZISAUX_HOST_DESCRIPTOR_EYE,
         sizeof(descriptor->eyecatcher));
  descriptor->version = ZISAUX_HOST_DESCRIPTOR_VERSION;
  descriptor->key = CROSS_MEMORY_SERVER_KEY; /* we use same key as xmem server */
  descriptor->subpool = SUBPOOL; /* same as the one used in alloc.c */
  descriptor->size = allocSize;

  descriptor->moduleVersion = moduleVersion;
  descriptor->init = initFunction;
  descriptor->term = termFunction;
  descriptor->handleCancel = handleCancelFunciton;
  descriptor->handleCommand = handleCommandFunction;
  descriptor->handleWork = handleWorkFunction;
  descriptor->moduleData = moduleData;

  return descriptor;
}

void zisAUXDestroyModuleDescriptor(ZISAUXModuleDescriptor *descriptor) {
  safeFree((char *)descriptor, descriptor->size);
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
