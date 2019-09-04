

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __SERVER_STATUS_SERVICE_H__
#define __SERVER_STATUS_SERVICE_H__
#include "json.h"
#include "httpserver.h"
#include "timeutls.h"
#include "time.h"

typedef struct ServerAgentContext_tag{
  char productVersion[40];
  JsonObject *serverConfig;
} ServerAgentContext;

int installServerStatusService(HttpServer *server, JsonObject* serverSettings, char* productVer);

#endif /* __SERVER_STATUS_H__ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

