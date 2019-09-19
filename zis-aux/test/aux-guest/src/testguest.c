

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/string.h>

#ifndef METTLE
#error Non-metal C code is not supported
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "crossmemory.h"
#include "logging.h"
#include "metalio.h"

#include "aux-guest.h"
#include "aux-host.h"
#include "aux-utils.h"

#define AUX_TEST_GUEST_MSG_ID "AUXTESTG0"

static int init(struct ZISAUXContext_tag *context,
                void *moduleData, int traceLevel) {

  char *moduleMessage = moduleData;

  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO,
          AUX_TEST_GUEST_MSG_ID" Hello from test guest module, message = \'%s\'\n",
          moduleMessage);

  return RC_ZISAUX_GUEST_OK;
}

static int term(struct ZISAUXContext_tag *context,
                void *moduleData, int traceLevel) {

  char *moduleMessage = moduleData;

  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO,
          AUX_TEST_GUEST_MSG_ID" Bye from test guest module, message = \'%s\'\n",
          moduleMessage);

  return RC_ZISAUX_GUEST_OK;
}

static int handleCancel(struct ZISAUXContext_tag *context, void *moduleData,
                        int traceLevel) {

  /* There is no logging context at this point, using printf. */

  printf(AUX_TEST_GUEST_MSG_ID" Hello from ZISAUX resource manager!\n");

  return RC_ZISAUX_GUEST_OK;
}

static int handleCommand(struct ZISAUXContext_tag *context,
                         const ZISAUXCommand *rawCommand,
                         const CMSModifyCommand *parsedCommand,
                         ZISAUXCommandReponse *response,
                         int traceLevel) {

  if (strcmp(parsedCommand->commandVerb, "DISPLAY") == 0) {
    snprintf(response->text, sizeof(response->text), "Hello from ASID=0X%02X",
             getASCB()->ascbasid);
    response->length = strlen(response->text);
    response->status = CMS_MODIFY_COMMAND_STATUS_PROCESSED;
  }

  return RC_ZISAUX_GUEST_OK;
}

static int handleWork(struct ZISAUXContext_tag *context,
                      const ZISAUXRequestPayload *requestPayload,
                      ZISAUXRequestResponse *response,
                      int traceLevel) {

  const char *stringToRevert = requestPayload->data;
  size_t stringLength = strlen(stringToRevert);
  char *responseBuffer = response->data;

  for (int i = stringLength - 1; i >= 0; i--) {
    responseBuffer[stringLength - 1 - i] = stringToRevert[i];
  }

  return RC_ZISAUX_GUEST_OK;
}

ZISAUXModuleDescriptor *getModuleDescriptor(void) {

  char *messageFromModule = "TEST_MODULE_DATA";

  ZISAUXModuleDescriptor *descriptor =
      zisAUXMakeModuleDescriptor(init, term, handleCancel,
                                 handleCommand, handleWork,
                                 messageFromModule, 1);

  return descriptor;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
