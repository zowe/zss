

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
#include <regex.h> 

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
#define STRING_BUFFER_SIZE 1024
#define SAF_SUB_URL_SIZE 16

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

static int serveAuthCheck(HttpService *service, HttpResponse *response);

int serveAuthCheckByParams(HttpService *service, char *userName, char *Class, char* entity, int access);

const char* getProfileNameFromRequest(char *profileName, StringList *parsedFile, char *method, int instanceID, HttpResponse *response);

static const char* makeProfileName(
  const char *type,
  const char *productCode, 
  int instanceID, 
  const char *pluginID, 
  const char *rootServiceName, 
  const char *serviceName,
  const char *method,
  const char *scope,
  char subUrl[SAF_SUB_URL_SIZE][STRING_BUFFER_SIZE]);
  
static void setProfileNameAttribs(
  char *pluginID,
  char *serviceName,
  char *type,
  char *scope,
  char subUrl[SAF_SUB_URL_SIZE][STRING_BUFFER_SIZE]);

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

static int serveAuthCheck(HttpService *service, HttpResponse *res) {
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

  privilegedServerName = getConfiguredProperty(service->server,
      HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY);
  rc = zisCheckEntity(privilegedServerName, userName, class, entity, access,
      &reqStatus);

  respond(res, rc, &reqStatus);
  return 0;
}

int serveAuthCheckByParams(HttpService *service, char *userName, char *Class, char *entity, int access) {
  int rc = 0;
  JsonObject *dataserviceAuth = jsonObjectGetObject(service->server->sharedServiceMem, "dataserviceAuthentication");
  int rbacParm = jsonObjectGetBoolean(dataserviceAuth, "rbac");
  CrossMemoryServerName *privilegedServerName = getConfiguredProperty(service->server,
      HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY);
  ZISAuthServiceStatus reqStatus = {0};
  if (!rbacParm) {
    return rc; // When rbac isn't enabled, we don't try to check the auth query
  }
  rc = zisCheckEntity(privilegedServerName, userName, Class, entity, access,
      &reqStatus);
  return rc;
}

const char* getProfileNameFromRequest(char *profileName, StringList *parsedFile, char *method, int instanceID, HttpResponse *response) {
  char type[8]; // core || config || service
  char productCode[STRING_BUFFER_SIZE];
  char rootServiceName[STRING_BUFFER_SIZE];
  char subUrl[SAF_SUB_URL_SIZE][STRING_BUFFER_SIZE];
  char scope[STRING_BUFFER_SIZE];
  char placeHolder1[STRING_BUFFER_SIZE], pluginID[STRING_BUFFER_SIZE], placeHolder2[STRING_BUFFER_SIZE], 
    serviceName[STRING_BUFFER_SIZE], placeHolder3[STRING_BUFFER_SIZE];
  char urlSegment[STRING_BUFFER_SIZE];
  int subUrlIndex = 0;
  
  snprintf(urlSegment, STRING_BUFFER_SIZE, "%s", stringListPrint(parsedFile, 1, 1, "/", 0));
  StringListElt *pathSegment = firstStringListElt(parsedFile);

  if (profileName == NULL) {
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_SEVERE,
           "safeMalloc failed. Not enough memory");
    respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Not enough memory");
    return NULL;
  }
  strupcase(urlSegment);
  if (instanceID < 0) { // Set instanceID
    instanceID = 0;
  }
  if (strcmp(urlSegment, "PLUGINS") != 0) {
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2,
           "parsedFile urlSegment check didn't match.");
    subUrlIndex = -1;
    while (pathSegment != NULL) {
      snprintf(urlSegment, STRING_BUFFER_SIZE, "%s", pathSegment->string);
      strupcase(urlSegment);
      if (rootServiceName == NULL)
      {
        snprintf(rootServiceName, STRING_BUFFER_SIZE, urlSegment);
      } else {
        if (subUrlIndex < SAF_SUB_URL_SIZE) {
          snprintf(subUrl[subUrlIndex], STRING_BUFFER_SIZE, urlSegment);
        }
      }
      subUrlIndex++;
      pathSegment = pathSegment->next;
    }
    snprintf(productCode, STRING_BUFFER_SIZE, "ZLUX");
    snprintf(type, STRING_BUFFER_SIZE, "core");
  } else {
    subUrlIndex = 0;
    
    while (pathSegment != NULL) {
      snprintf(urlSegment, STRING_BUFFER_SIZE, "%s", pathSegment->string);
      strupcase(urlSegment);
      switch(subUrlIndex) {
        case 0:
          snprintf(productCode, STRING_BUFFER_SIZE, urlSegment);
          break;
        case 1:
          snprintf(placeHolder1, STRING_BUFFER_SIZE, urlSegment);
          break;
        case 2:
          snprintf(pluginID, STRING_BUFFER_SIZE, urlSegment);
          break;
        case 3:
          snprintf(placeHolder2, STRING_BUFFER_SIZE, urlSegment);
          break;
        case 4:
          snprintf(serviceName, STRING_BUFFER_SIZE, urlSegment);
          break;
        case 5:
          snprintf(placeHolder3, STRING_BUFFER_SIZE, urlSegment);
          break;
        default:
          snprintf(subUrl[subUrlIndex-6], STRING_BUFFER_SIZE, urlSegment); // subtract 6 from maximum index to begin init subUrl array at 0
      }
      subUrlIndex++;
      pathSegment = pathSegment->next;
    }
    
    setProfileNameAttribs(pluginID, serviceName, type, scope, subUrl);
    char* ch; 
    char* chReplace;
    ch = ".";
    chReplace = "_";
    int index = 0;
    for (index = 0; index <= strlen(pluginID); index++)
  	{
  		if (pluginID[index] == *ch)  
      {
        pluginID[index] = *chReplace;
      }
    }
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2,
           "parsedFile urlSegment check OK.");
  }
  snprintf(profileName, STRING_BUFFER_SIZE, makeProfileName(type, 
    productCode, 
    instanceID, 
    pluginID, 
    rootServiceName,
    serviceName,
    method,
    scope,
    subUrl));
    
  return profileName;
}

static void setProfileNameAttribs(
  char *pluginID,
  char *serviceName,
  char *type,
  char *scope,
  char subUrl[SAF_SUB_URL_SIZE][STRING_BUFFER_SIZE]) {
  if ((strcmp(pluginID, "ORG.ZOWE.CONFIGJS") == 0) && (strcmp(serviceName, "DATA") == 0))
  {
    snprintf(type, STRING_BUFFER_SIZE, "config");
    snprintf(pluginID, STRING_BUFFER_SIZE, subUrl[0]);
    snprintf(scope, STRING_BUFFER_SIZE, subUrl[1]);
    
  } else {
    snprintf(type, STRING_BUFFER_SIZE, "service");
  }
}

static const char* makeProfileName(
  const char *type,
  const char *productCode, 
  int instanceID, 
  const char *pluginID, 
  const char *rootServiceName, 
  const char *serviceName,
  const char *method,
  const char *scope,
  char subUrl[SAF_SUB_URL_SIZE][STRING_BUFFER_SIZE]) {
  char *profileName = safeMalloc(STRING_BUFFER_SIZE, "profileNameInner");
  if (profileName == NULL) {
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_SEVERE,
           "safeMalloc failed. Not enough memory");
    return NULL;
  }
  if (productCode == NULL) {
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "Broken SAF query. Missing product code.");
    return NULL;
  }
  if (instanceID == -1) {
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "Broken SAF query. Missing instance ID.");
    return NULL;
  }
  if (method == NULL) {
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "Broken SAF query. Missing method.");
    return NULL;
  }
  // char someString[STRING_BUFFER_SIZE] = { snprintf(*someString, STRING_BUFFER_SIZE, type) };
  if (strcmp(type, "service") == 0) {
    if (pluginID == NULL) {
      zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
       "Broken SAF query. Missing plugin ID.");
      return NULL;
    }
    if (serviceName == NULL) {
      zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
       "Broken SAF query. Missing service name.");
      return NULL;
    }
    snprintf(profileName, STRING_BUFFER_SIZE, "%s.%d.SVC.%s.%s.%s", productCode, instanceID, pluginID, serviceName, method);
  } else if (strcmp(type, "config") == 0) {
    if (pluginID == NULL) {
      zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
       "Broken SAF query. Missing plugin ID.");
      return NULL;
    }
    if (scope == NULL) {
      zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
       "Broken SAF query. Missing scope.");
      return NULL;
    }
    snprintf(profileName, STRING_BUFFER_SIZE, "%s.%d.CFG.%s.%s.%s", productCode, instanceID, pluginID, method, scope); 
  } else if (strcmp(type, "core") == 0) {
    if (rootServiceName == NULL) {
      zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
       "Broken SAF query. Missing root service name.");
      return NULL;
    }
    snprintf(profileName, STRING_BUFFER_SIZE, "%s.%d.COR.%s.%s", productCode, instanceID, method, rootServiceName); 
  }
  // Child endpoints housed via subUrl
  int index = 0;
  while (index < SAF_SUB_URL_SIZE && strcmp(subUrl[index], "") != 0) {
    snprintf(profileName, STRING_BUFFER_SIZE, "%s.%s", profileName, subUrl[index]);
    index++;
  }
  return profileName;
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

static int resetPassword(HttpService *service, HttpResponse *response) {
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

