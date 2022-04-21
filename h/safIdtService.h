/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __SAF_IDT_SERVICE_H__
#define __SAF_IDT_SERVICE_H__

#include "httpserver.h"

void installSAFIdtTokenService(HttpServer *server);

#define ZIS_AUTH_SERVICE_APPL_MAX_LENGTH     8

/* The #define for ZIS_AUTH_SERVICE_PARMLIST_OPTION_GENERATE_IDT has been deprecated, and will be removed
   in the future.
  */
typedef struct SafIdtService_tag{
  int safIdtServiceVersion;
  int safIdtLen;
  #define ZIS_AUTH_SERVICE_PARMLIST_SAFIDT_LENGTH (2 * IDTA_IDT_BUFFER_LEN_MIN)
  #define ZIS_AUTH_SERVICE_PARMLIST_OPTION_GENERATE_IDT 0x80
  #define ZIS_AUTH_SERVICE_PARMLIST_OPTION_IDT_APPL 0x40
  char safIdt[ZIS_AUTH_SERVICE_PARMLIST_SAFIDT_LENGTH];
  char applNullTerm[ZIS_AUTH_SERVICE_APPL_MAX_LENGTH + 1];
} SafIdtService;

#endif /* __SAF_IDT_SERVICE_H__ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/