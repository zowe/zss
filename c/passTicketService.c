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
#include <stdint.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "charsets.h"
#include "httpserver.h"
#include "json.h"
#include "zssLogging.h"
#include "passTicketService.h"
#include "serviceUtils.h"

#define TICKET_LEN 8
#define APPLICATION_NAME_KEY "applicationName"
#define APPLICATION_NAME_LEN 8

#ifdef _LP64
#pragma linkage(IRRSGS64, OS)
#define IRRSGS IRRSGS64
#else
#pragma linkage(IRRSGS00, OS)
#define IRRSGS IRRSGS00
#endif

typedef struct PassTicketAPIStatus_tag {
  uint32_t safRC;
  uint32_t racfRC;
  uint32_t racfRSN;
} PassTicketAPIStatus;

typedef struct PassTicketResult_tag {
  char ticket[TICKET_LEN+1];
  PassTicketAPIStatus status;
} PassTicketResult;

static int servePassTicket(HttpService *service, HttpResponse *response);
static void respondWithPassTicket(HttpResponse *response, const char *applicationName, const char *ticket);
static int generatePassTicket(const char *applicationName, const char *userId, PassTicketResult *result);
static void respondWithPassTicketError(HttpResponse *response, int code, const char *status, const char *fmt, ...);
static void stringifyPassTicketAPIStatus(PassTicketAPIStatus *status, char *buffer, size_t bufferSize);

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
  PassTicketResult passTicketResult = {0};
  char errorBuf[1024] = {0};

  if (0 != strcmp(request->method, methodPOST)) {
    respondWithPassTicketError(response, HTTP_STATUS_METHOD_NOT_FOUND, "Method Not Allowed", "Only POST method allowed");
    return 0;
  }

  if (0 != getApplicationName(request, applicationName, sizeof(applicationName), errorBuf, sizeof(errorBuf))) {
    respondWithPassTicketError(response, HTTP_STATUS_BAD_REQUEST, "Bad Request", "Failed to get %s - %s", APPLICATION_NAME_KEY, errorBuf);
    return 0;
  }

  if (0 != generatePassTicket(applicationName, request->username, &passTicketResult)) {
    stringifyPassTicketAPIStatus(&passTicketResult.status, errorBuf, sizeof(errorBuf));
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, ZSS_LOG_PASSTICKET_GEN_FAILED_MSG, request->username, applicationName, errorBuf);
    respondWithPassTicketError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Internal Server Error", "Failed to generate PassTicket");
    return 0;
  }

  respondWithPassTicket(response, applicationName, passTicketResult.ticket);
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

typedef struct StringBlock_tag {
    uint32_t length;
#ifdef _LP64
    uint32_t zeros;
#endif
    void *address;
} StringBlock;

static int generatePassTicket(const char *applicationName, const char *userId, PassTicketResult *result) {
  uint8_t workArea[1024];
  void *fnParm[4];
  StringBlock userBlock = {0};
  StringBlock applBlock = {0};
  StringBlock ticketBlock = {0};

  userBlock.length     = strlen(userId);
  userBlock.address    = (void*)userId;
  applBlock.length     = strlen(applicationName);
  applBlock.address    = (void*)applicationName;
  ticketBlock.length   = TICKET_LEN;
  ticketBlock.address  = result->ticket;

  uint32_t nParms = 12;
  uint32_t alet1 = 0;
  uint32_t alet2 = 0;
  uint32_t alet3 = 0;
  uint32_t optionWord = 0; /* reserved */
  uint16_t fnCode = 3;     /* Passticket function */
  uint32_t subFnCode = 1;  /* Generate passticket */
  uint32_t fnParmCount  = 4;
  fnParm[0] = &subFnCode;
  fnParm[1] = &ticketBlock;
  fnParm[2] = &userBlock;
  fnParm[3] = &applBlock;

#ifdef _LP64
#define PASS_AS_DOUBLE_WORD(x) (x) // 64-bit pointer is already a double word
#else
#define PASS_AS_DOUBLE_WORD(x) NULL, x // pass as two parameters, first word filled with zeros and the second word filled with the 31 bit address
#endif
    IRRSGS(PASS_AS_DOUBLE_WORD(&nParms),
           PASS_AS_DOUBLE_WORD(workArea),
           PASS_AS_DOUBLE_WORD(&alet1),
           PASS_AS_DOUBLE_WORD(&result->status.safRC),
           PASS_AS_DOUBLE_WORD(&alet2),
           PASS_AS_DOUBLE_WORD(&result->status.racfRC),
           PASS_AS_DOUBLE_WORD(&alet3),
           PASS_AS_DOUBLE_WORD(&result->status.racfRSN),
           PASS_AS_DOUBLE_WORD(&optionWord),
           PASS_AS_DOUBLE_WORD(&fnCode),
           PASS_AS_DOUBLE_WORD(&fnParmCount),
           PASS_AS_DOUBLE_WORD(fnParm)
#ifndef _LP64
         , PASS_AS_DOUBLE_WORD(NULL) // finish the list for safety in 31bit mode
#endif
          );
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "passticket generation safRc=%d, racfRc=%d, racfReason=%d\n", result->status.safRC, result->status.racfRC, result->status.racfRSN);
  int rc = (0 == result->status.safRC) ? 0 : -1;
  if (rc == 0) {
    result->ticket[TICKET_LEN] = '\0';
  }
  return rc;
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

static void stringifyPassTicketAPIStatus(PassTicketAPIStatus *status, char *buffer, size_t bufferSize) {
  const char *errorMessage = NULL;
  uint32_t safRC = status->safRC;
  uint32_t racfRC = status->racfRC;
  uint32_t racfRSN = status->racfRSN;
  if (safRC == 8 && racfRC == 8 && racfRSN == 16) {
    errorMessage = "Not authorized to generate PassTicket";
  } else if (safRC == 8 && racfRC == 16 && racfRSN == 28) {
    errorMessage = "PassTicket generation failure. Verify that the secured signon (PassTicket) function and application ID is configured properly";
  }
  snprintf (buffer, bufferSize, "safRc=%d, racfRc=%d, racfReason=%d %s", safRC, racfRC, racfRSN, errorMessage ? errorMessage : "");
}

static int getApplicationName(HttpRequest *request, char *applicationNameBuf, size_t applicationNameBufSize, char *errorBuf, size_t errorBufSize) {
  char *body = request->contentBody;
  size_t len = body ? strlen(body) : 0;
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