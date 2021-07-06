

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

#define ZOWE_PROFILE_NAME_LEN 246

int installAuthCheckService(HttpServer *server);
void installZosPasswordService(HttpServer *server);

/**
 * @brief The function uses makeProfileName function to generate profile name for SAF query
 * @param profileName Generated profile name goes here
 * @param parsedFile Refers to the StringList object which contains URL stripped of args
 * @param instanceID Refers to instanceID for query. If none specified, or negative, then 0
 * @param HttpResponse Describes the HttpResponse object to return if error encountered
 *
 * @return Return non-zero if error
 */
int getProfileNameFromRequest(char *profileName, StringList *parsedFile, char *method, int instanceID, HttpResponse *response);

/**
 * @brief The function satisfies RBAC, by first checking if RBAC is enabled, then executing
 * a ZIS check.
 * @param service The calling HttpService
 * @param userName Username to use in ZIS check
 * @param Class Class to use in ZIS check i.e. "ZOWE"
 * @param entity Describes the SAF query itself i.e. "ZLUX.0.COR.GET.SERVER.AGENT.CONFIG"
 * @param access Describes the access type i.e. "READ"
 * @param envSettings JSON object that holds environment variables, if any
 *
 * @return Return code where != 0 is a failed RBAC check
 */
int serveAuthCheckByParams(HttpService *service, char *userName, char *Class, char *entity, int access, JsonObject *envSettings);


#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

