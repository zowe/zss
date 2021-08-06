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
#include <stdarg.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "charsets.h"
#include "bpxnet.h"
#include "socketmgmt.h"
#include "httpserver.h"
#include "json.h"

#include "passTicketService.h"
#include "serviceUtils.h"

#define TICKET_LEN 8
#define APPLICATION_NAME_KEY "applicationName"
#define APPLICATION_NAME_LEN 8

static int servePassTicket(HttpService *service, HttpResponse *response);
static void respondWithPassTicket(HttpResponse *response, const char *applicationName, const char *ticket);
static int generatePassTicket(const char *applicationName, const char *userId, char *ticketBuf, size_t ticketBufSize, char *errorBuf, size_t errorBufSize);
static void respondWithPassTicketError(HttpResponse *response, int code, const char *status, const char *fmt, ...);

int installPassTicketService(HttpServer *server) {
  HttpService *httpService = makeGeneratedService("PassTicket_Service", "/passticket");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = &servePassTicket;
  httpService->runInSubtask = FALSE;
  httpService->doImpersonation = FALSE;
  registerHttpService(server, httpService);

  return 0;
}

static int servePassTicket(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  char applicationName[APPLICATION_NAME_LEN + 1] = {0};
  char ticket[TICKET_LEN + 1] = {0};
  char errorBuf[1024] = {0};

  if (0 != strcmp(request->method, methodPOST)) {
    respondWithPassTicketError(response, HTTP_STATUS_METHOD_NOT_FOUND, "Method Not Allowed", "Only POST method allowed");
    return 0;
  }

  if (0 != extractApplicationName(request, applicationName, sizeof(applicationName), errorBuf, sizeof(errorBuf))) {
    respondWithPassTicketError(response, HTTP_STATUS_BAD_REQUEST, "Bad Request", "Failed to get %s - %s", APPLICATION_NAME_KEY, errorBuf);
    return 0;
  }

  if (0 != generatePassTicket(applicationName, request->username, ticket, sizeof(ticket), errorBuf, sizeof(errorBuf))) {
    respondWithPassTicketError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Internal Server Error", "Failed to generate PassTicket - %s", errorBuf);
    return 0;
  }

  respondWithPassTicket(response, applicationName, ticket);
  return 0;
}

static void respondWithPassTicket(HttpResponse *response, const char *applicationName, const char *ticket) {
  HttpRequest *request = response->request;
  jsonPrinter *p = respondWithJsonPrinter(response);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(p);
  jsonAddString(p, "userId", request->username);
  jsonAddString(p, APPLICATION_NAME_KEY, (char*)applicationName);
  jsonAddString(p, "ticket", (char*)ticket);
  jsonEnd(p);
  finishResponse(response);
}

static int generatePassTicket(const char *applicationName, const char *userId, char *ticketBuf, size_t ticketBufSize, char *errorBuf, size_t errorBufSize) {
  snprintf (ticketBuf, ticketBufSize, "%s", "LZTKEEDQ");
  return 0;
}

static void respondWithPassTicketError(HttpResponse *response, int code, const char *status, const char *fmt, ...) {
  HttpRequest *request = response->request;
  jsonPrinter *p = respondWithJsonPrinter(response);
  char buffer[4096];

  setResponseStatus(response, code, (char*)status);
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);

  va_list args;
  va_start (args, fmt);
  vsnprintf (buffer, sizeof(buffer), fmt, args);
  va_end (args);

  jsonStart(p);
  jsonAddString(p, "error", buffer);
  jsonEnd(p);
  finishResponse(response);
}

static int extractApplicationName(HttpRequest *request, char *applicationNameBuf, size_t applicationNameBufSize, char *errorBuf, size_t errorBufSize) {
  char *body = request->contentBody;
  int len = body ? strlen(body) : 0;
  if (len == 0) {
    snprintf (errorBuf, errorBufSize, "POST body is empty");
    return -1;
  }

  char *nativeBody = copyStringToNative(request->slh, body, strlen(body));
  len = nativeBody ? strlen(nativeBody) : 0;
  if (len == 0) {
    snprintf (errorBuf, errorBufSize, "Failed to convert POST body to EBCDIC");
    return -1;
  }

  char errBuf[512];
  Json *jsonBody = jsonParseUnterminatedString(request->slh, nativeBody, len, errBuf, sizeof(errBuf));
  if (!jsonBody) {
    snprintf (errorBuf, errorBufSize, "Failed to parse POST body JSON - %s", errBuf);
    return -1;
  }

  JsonObject *jsonBodyObject = jsonAsObject(jsonBody);
  if (!jsonBodyObject) {
    snprintf (errorBuf, errorBufSize, "POST body is not a JSON object");
    return -1;
  }

  char *appName = jsonObjectGetString(jsonBodyObject, APPLICATION_NAME_KEY);
  if (!appName) {
    snprintf (errorBuf, errorBufSize, "%s key not found in JSON POST body", APPLICATION_NAME_KEY);
    return -1;
  }

  size_t appNameLen = strlen(appName);

  if (appNameLen > APPLICATION_NAME_LEN) {
    snprintf (errorBuf, errorBufSize, "%s is too long", APPLICATION_NAME_KEY);
    return -1;
  }

  if (appNameLen == 0) {
    snprintf (errorBuf, errorBufSize, "%s is empty", APPLICATION_NAME_KEY);
    return -1;
  }

  snprintf (applicationNameBuf, applicationNameBufSize, "%s", appName);

  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/