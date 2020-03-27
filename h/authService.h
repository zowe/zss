

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __AUTHSERVICE__
#define __AUTHSERVICE__

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif
#include "logging.h"
#include "json.h"
#include "bpxnet.h"
#include "socketmgmt.h"
#include "httpserver.h"
#include "dataservice.h"

#define PASSWORD_RESET_OK                0
#define PASSWORD_RESET_WRONG_PASSWORD    111
#define PASSWORD_RESET_WRONG_USER        143
#define PASSWORD_RESET_TOO_MANY_ATTEMPTS 163
#define PASSWORD_RESET_NO_NEW_PASSSWORD  168
#define PASSWORD_RESET_WRONG_FORMAT      169

#define RESPONSE_MESSAGE_LENGTH          100

int installAuthCheckService(HttpServer *server);
int resetPassword(HttpService *service, HttpResponse *response);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

