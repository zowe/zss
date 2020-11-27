

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <iconv.h>
#include <dirent.h>
#include <pthread.h>

#include "authService.h"
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
#include "zis/client.h"
#include "httpserver.h"
#include "zssLogging.h"

#define SAF_CLASS "ZOWE"
#define JSON_ERROR_BUFFER_SIZE 1024

#define SAF_PASSWORD_RESET_RC_OK                0
#define SAF_PASSWORD_RESET_RC_WRONG_PASSWORD    111
#define SAF_PASSWORD_RESET_RC_WRONG_USER        143
#define SAF_PASSWORD_RESET_RC_TOO_MANY_ATTEMPTS 163
#define SAF_PASSWORD_RESET_RC_NO_NEW_PASSSWORD  168
#define SAF_PASSWORD_RESET_RC_WRONG_FORMAT      169

#define RESPONSE_MESSAGE_LENGTH             100

/*
 * A handler performing the SAF_AUTH check: checks if the user has the
 * specified access to the specified entity in the specified class
 *
 * URL format:
 *   GET .../saf-auth/<USERID>/<CLASS>/<ENTITY>/<READ|UPDATE|ALTER|CONTROL>
 * Example: /saf-auth/PDUSR/FACILITY/CQM.CAE.ADMINISTRATOR/READ
 *
 * Response examples:
 *  - The user is authorized: { "authorized": true }
 *  - Not authorized: { "authorized": false, message: "..." }
 *  - Error: {
 *      "error": true,
 *      "message": "..."
 *    }
 */

static int serveAuthCheck(HttpService *service, HttpResponse *response, ...);

int installAuthCheckService(HttpServer *server) {
//  zowelog(NULL, 0, ZOWE_LOG_DEBUG2, "begin %s\n",
//  __FUNCTION__);
  HttpService *httpService = makeGeneratedService("SAF_AUTH service",
      "/saf-auth/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = &serveAuthCheck;
  httpService->runInSubtask = FALSE;
  registerHttpService(server, httpService);
//  zowelog(NULL, 0, ZOWE_LOG_DEBUG2, "end %s\n",
//  __FUNCTION__);
  return 0;
}

static int extractQuery(StringList *path, char **entity, char **access) {
  const StringListElt *pathElt;

#define TEST_NEXT_AND_SET($ptr) do { \
  pathElt = pathElt->next;           \
  if (pathElt == NULL) {             \
    return -1;                       \
  }                                  \
  *$ptr = pathElt->string;           \
} while (0)

  pathElt = firstStringListElt(path);
  while (pathElt && (strcmp(pathElt->string, "saf-auth") != 0)) {
    pathElt = pathElt->next;
  }
  if (pathElt == NULL) {
    return -1;
  }
  TEST_NEXT_AND_SET(entity);
  TEST_NEXT_AND_SET(access);
  return 0;
#undef TEST_NEXT_AND_SET
}

static int parseAcess(const char inStr[], int *outNum) {
  int rc;

  if (strcasecmp("ALTER", inStr) == 0) {
    *outNum = SAF_AUTH_ATTR_ALTER;
  } else if (strcasecmp("CONTROL", inStr) == 0) {
    *outNum = SAF_AUTH_ATTR_CONTROL;
  } else if (strcasecmp("UPDATE", inStr) == 0) {
    *outNum = SAF_AUTH_ATTR_UPDATE;
  } else if (strcasecmp("READ", inStr) == 0) {
    *outNum = SAF_AUTH_ATTR_READ;
  } else {
    return -1;
  }
  return 0;
}

static void respond(HttpResponse *res, int rc, const ZISAuthServiceStatus
    *reqStatus) {
  jsonPrinter* p = respondWithJsonPrinter(res);

  setResponseStatus(res, HTTP_STATUS_OK, "OK");
  setDefaultJSONRESTHeaders(res);
  writeHeader(res);
  if (rc == RC_ZIS_SRVC_OK) {
    jsonStart(p); {
      jsonAddBoolean(p, "authorized", true);
    }
    jsonEnd(p);
  } else {
    char errBuf[0x100];

#define FORMAT_ERROR($fmt, ...) snprintf(errBuf, sizeof (errBuf), $fmt, \
    ##__VA_ARGS__)

    ZIS_FORMAT_AUTH_CALL_STATUS(rc, reqStatus, FORMAT_ERROR);
#undef FORMAT_ERROR
    jsonStart(p); {
      if (rc == RC_ZIS_SRVC_SERVICE_FAILED
          && reqStatus->safStatus.safRC != 0) {
        jsonAddBoolean(p, "authorized", false);
      } else {
        jsonAddBoolean(p, "error", true);
      }
      jsonAddString(p, "message", errBuf);
    }
    jsonEnd(p);
  }
  finishResponse(res);
}

static int serveAuthCheck(HttpService *service, HttpResponse *res, ...) {
  HttpRequest *req = res->request;
  char *entity, *accessStr;
  int access = 0;
  int rc = 0, rsn = 0, safStatus = 0;
  ZISAuthServiceStatus reqStatus = {0};
  CrossMemoryServerName *privilegedServerName;
  const char *userName = req->username, *class = SAF_CLASS;
  rc = extractQuery(req->parsedFile, &entity, &accessStr);
  if (rc != 0) {
    respondWithError(res, HTTP_STATUS_BAD_REQUEST, "Broken auth query");
    return 0;
  }
  rc = parseAcess(accessStr, &access);
  if (rc != 0) {
    respondWithError(res, HTTP_STATUS_BAD_REQUEST, "Unexpected access level");
    return 0;
  }
  /* printf("query: user %s, class %s, entity %s, access %d\n", userName, class,
      entity, access); */
  privilegedServerName = getConfiguredProperty(service->server,
      HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY);
  rc = zisCheckEntity(privilegedServerName, userName, class, entity, access,
      &reqStatus);
  respond(res, rc, &reqStatus);
  return 0;
}

void respondWithJsonStatus(HttpResponse *response, const char *status, int statusCode, const char *statusMessage) {
    jsonPrinter *out = respondWithJsonPrinter(response);
    setResponseStatus(response,statusCode,(char *)statusMessage);
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);

    jsonStart(out);
    jsonAddString(out, "status", (char *)status);
    jsonEnd(out);

    finishResponse(response);
}

static int resetPassword(HttpService *service, HttpResponse *response, ...) {
  int returnCode = 0, reasonCode = 0;
  HttpRequest *request = response->request;
  
  if (!strcmp(request->method, methodPOST)) {
    char *inPtr = request->contentBody;
    char *nativeBody = copyStringToNative(request->slh, inPtr, strlen(inPtr));
    int inLen = nativeBody == NULL ? 0 : strlen(nativeBody);
    char errBuf[JSON_ERROR_BUFFER_SIZE];
    char responseString[RESPONSE_MESSAGE_LENGTH];

    if (nativeBody == NULL) {
      respondWithJsonStatus(response, "No body found", HTTP_STATUS_BAD_REQUEST, "Bad Request");
      return HTTP_SERVICE_FAILED;
    }
    
    Json *body = jsonParseUnterminatedString(request->slh, nativeBody, inLen, errBuf, JSON_ERROR_BUFFER_SIZE);
    
    if (body == NULL) {
      respondWithJsonStatus(response, "No body found", HTTP_STATUS_BAD_REQUEST, "Bad Request");
      return HTTP_SERVICE_FAILED;
    }
    
    JsonObject *inputMessage = jsonAsObject(body);
    Json *username = jsonObjectGetPropertyValue(inputMessage,"username");
    Json *password = jsonObjectGetPropertyValue(inputMessage,"password");
    Json *newPassword = jsonObjectGetPropertyValue(inputMessage,"newPassword");
    int usernameLength = 0;
    int passwordLength = 0;
    int newPasswordLength = 0;
    if (username != NULL) {
      if (jsonAsString(username) != NULL) {
        usernameLength = strlen(jsonAsString(username));
      }
    }
    if (password != NULL) {
      if (jsonAsString(password) != NULL) {
        passwordLength = strlen(jsonAsString(password));
      }
    }
    if (newPassword != NULL) {
      if (jsonAsString(newPassword) != NULL) {
        newPasswordLength = strlen(jsonAsString(newPassword));
      }
    }
    if (usernameLength == 0) {
      respondWithJsonStatus(response, "No username provided",
                            HTTP_STATUS_BAD_REQUEST, "Bad Request");
      return HTTP_SERVICE_FAILED;
    }
    if (passwordLength == 0) {
      respondWithJsonStatus(response, "No password provided",
                            HTTP_STATUS_BAD_REQUEST, "Bad Request");
      return HTTP_SERVICE_FAILED;
    }
    if (newPasswordLength == 0) {
      respondWithJsonStatus(response, "No new password provided",
                            HTTP_STATUS_BAD_REQUEST, "Bad Request");
      return HTTP_SERVICE_FAILED;
    }
    resetZosUserPassword(jsonAsString(username),  jsonAsString(password), jsonAsString(newPassword), &returnCode, &reasonCode);

    switch (returnCode) {
      case SAF_PASSWORD_RESET_RC_OK:
        respondWithJsonStatus(response, "Password Successfully Reset", HTTP_STATUS_OK, "OK");
        return HTTP_SERVICE_SUCCESS;
      case SAF_PASSWORD_RESET_RC_WRONG_PASSWORD:
        respondWithJsonStatus(response, "Username or password is incorrect. Please try again.",
                              HTTP_STATUS_UNAUTHORIZED, "Unauthorized");
        return HTTP_SERVICE_FAILED;
      case SAF_PASSWORD_RESET_RC_WRONG_USER:
        respondWithJsonStatus(response, "Username or password is incorrect. Please try again.",
                              HTTP_STATUS_UNAUTHORIZED, "Unauthorized");
        return HTTP_SERVICE_FAILED;
      case SAF_PASSWORD_RESET_RC_NO_NEW_PASSSWORD:
        respondWithJsonStatus(response, "No new password provided",
                              HTTP_STATUS_BAD_REQUEST, "Bad Request");
        return HTTP_SERVICE_FAILED;
      case SAF_PASSWORD_RESET_RC_WRONG_FORMAT:
        respondWithJsonStatus(response, "The new password format is incorrect. Please try again.",
                              HTTP_STATUS_BAD_REQUEST, "Bad Request");
        return HTTP_SERVICE_FAILED;
      case SAF_PASSWORD_RESET_RC_TOO_MANY_ATTEMPTS:
        respondWithJsonStatus(response,
                              "Incorrect password or account is locked. Please contact your administrator.",
                              HTTP_STATUS_TOO_MANY_REQUESTS, "Bad Request");
        return HTTP_SERVICE_FAILED;
      default:
        snprintf(responseString, RESPONSE_MESSAGE_LENGTH, "Password reset FAILED with return code: %d reason code: %d", returnCode, reasonCode);
        respondWithJsonStatus(response, responseString, HTTP_STATUS_BAD_REQUEST, "Bad Request");
        return HTTP_SERVICE_FAILED;
    }
  } else {
    respondWithJsonStatus(response, "Method Not Allowed",
                          HTTP_STATUS_METHOD_NOT_FOUND, "Method Not Allowed");
    return HTTP_SERVICE_FAILED;
  }
}

void installZosPasswordService(HttpServer *server) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);

  HttpService *httpService = makeGeneratedService("password service", "/password/**");
  httpService->authType = SERVICE_AUTH_NONE;
  httpService->runInSubtask = TRUE;
  httpService->serviceFunction = resetPassword;
  registerHttpService(server, httpService);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "end %s\n", __FUNCTION__);
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

