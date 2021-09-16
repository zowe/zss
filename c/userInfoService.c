/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <errno.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "charsets.h"
#include "httpserver.h"
#include "json.h"
#include "zssLogging.h"
#include "serviceUtils.h"
#include "userInfoService.h"

static int serveUserInfo(HttpService *service, HttpResponse *response);
static void respondWithUserInfo(HttpResponse *response, struct passwd *pw);
static void respondWithUserInfoError(HttpResponse *response, int code, const char *status, const char *message);

int installUserInfoService(HttpServer *server) {
  HttpService *httpService = makeGeneratedService("User info service", "/user-info");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = serveUserInfo;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  registerHttpService(server, httpService);

  return 0;
}

static int serveUserInfo(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  const char *userId = request->username;

  if (0 != strcmp(request->method, methodGET)) {
    respondWithUserInfoError(response, HTTP_STATUS_METHOD_NOT_FOUND, "Method Not Allowed", "Only GET method allowed");
    return 0;
  }

  struct passwd pw = {0};
  struct passwd *pwPtr = NULL;
  char buf[1024] = {0};
  int rc = getpwnam_r(userId, &pw, buf, sizeof(buf), &pwPtr);
  if (rc != 0) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "failed to get user info for %s - %s\n", userId, strerror(errno));
    respondWithUserInfoError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Internal Server Error", "Failed to get user info");
    return 0;
  }

  respondWithUserInfo(response, &pw);
  return 0;
}

static void respondWithUserInfo(HttpResponse *response, struct passwd *pw) {
  HttpRequest *request = response->request;
  jsonPrinter *p = respondWithJsonPrinter(response);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);

  jsonStart(p);
  jsonAddString(p, "userId", pw->pw_name);
  jsonAddUInt(p, "uid", pw->pw_uid);
  jsonAddUInt(p, "gid", pw->pw_gid);
  jsonAddString(p, "home", pw->pw_dir);
  jsonAddString(p, "shell", pw->pw_shell);
  jsonEnd(p);

  finishResponse(response);
}

static void respondWithUserInfoError(HttpResponse *response, int code, const char *status, const char *message) {
  HttpRequest *request = response->request;
  jsonPrinter *p = respondWithJsonPrinter(response);

  setResponseStatus(response, code, (char*)status);
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);

  jsonStart(p);
  jsonAddString(p, "error", (char*)message);
  jsonEnd(p);
  finishResponse(response);
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
