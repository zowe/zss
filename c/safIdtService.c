
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "safIdtService.h"
#include "httpserver.h"
#include "json.h"
#include "http.h"
#include "zssLogging.h"
#include "zis/client.h"
#include "utils.h"

#define _STRINGIFY(s) #s
#define STRINGIFY(s) _STRINGIFY(s)

static void respondWithInvalidMethod(HttpResponse *response) {
  jsonPrinter *p = respondWithJsonPrinter(response);

  setResponseStatus(response, 405, "Method Not Allowed");
  setDefaultJSONRESTHeaders(response);
  addStringHeader(response, "Allow", "POST"); 
  writeHeader(response);

  finishResponse(response);
}

Json *parseContentBody(HttpRequest *request) {

  char *inPtr = request->contentBody;
  char *nativeBody = copyStringToNative(request->slh, inPtr, strlen(inPtr));
  int inLen = nativeBody == NULL ? 0 : strlen(nativeBody);
  char errBuf[1024];

  if (nativeBody == NULL) {
    return NULL;
  }
  return jsonParseUnterminatedString(request->slh, nativeBody, inLen, errBuf, sizeof(errBuf));
}

static int authenticate(HttpResponse *response, CrossMemoryServerName *privilegedServerName) {

  ZISAuthServiceStatus status = {0};
  char safIdt[ZIS_AUTH_SERVICE_PARMLIST_SAFIDT_LENGTH + 1] = {0};
  HttpRequest *request = response->request;  

  Json *body = parseContentBody(request);
  if (body == NULL) {
    respondWithJsonStatus(response, "No body found", HTTP_STATUS_BAD_REQUEST, "Bad Request");
    return HTTP_SERVICE_FAILED;
  }

  JsonObject *jsonObject = jsonAsObject(body);
  char *username = jsonObjectGetString(jsonObject, "username");
  char *pass = jsonObjectGetString(jsonObject, "pass");

  if (username == NULL || strlen(username) == 0) {
    respondWithJsonStatus(response, "No username provided", HTTP_STATUS_BAD_REQUEST, "Bad Request");
    return HTTP_SERVICE_FAILED;
  }

  if (pass == NULL || strlen(pass) == 0) {
    respondWithJsonStatus(response, "No pass provided", HTTP_STATUS_BAD_REQUEST, "Bad Request");
    return HTTP_SERVICE_FAILED;
  }

  strupcase(username);

  if (isLowerCasePasswordAllowed() == FALSE && isPassPhrase(pass) == FALSE) {
    strupcase(pass); /* upfold password */
  }

  int zisRC = zisGenerateOrValidateSafIdt(privilegedServerName, username,
                                          pass,
                                          safIdt,
                                          &status);

  if (zisRC != RC_ZIS_SRVC_OK) {

    if (status.baseStatus.serviceRC == RC_ZIS_AUTHSRV_SAF_ERROR) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO,
              "Authetication request for %s failed: safRC=%d, racfRC=%d, racfRSN=%d\n",
              username, status.safStatus.safRC, status.safStatus.racfRC, status.safStatus.racfRSN);
      respondWithJsonError(response, "Not authenticated", HTTP_STATUS_UNAUTHORIZED, "Unauthorized");
      return HTTP_SERVICE_FAILED;
    }
    else if (status.baseStatus.serviceRC == RC_ZIS_AUTHSRV_SAF_ABENDED) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, "Auth service abend: system CC=%X, reason=%X\n", 
              status.abendInfo.completionCode, status.abendInfo.reasonCode);
      respondWithJsonError(response, "Check zssServer log for more details", HTTP_STATUS_INTERNAL_SERVER_ERROR, "Internal Server Error");
      return HTTP_SERVICE_FAILED;
    }
    else {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, 
              "zis auth service failed: zisRC=%d, serviceRC=%d, serviceRSN=%d, cmsRC=%d, cmsRSN=%d\n",
              zisRC, 
              status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
              status.baseStatus.cmsRC, status.baseStatus.cmsRSN);
      respondWithJsonError(response, "Check zssServer log for more details", HTTP_STATUS_INTERNAL_SERVER_ERROR, "Internal Server Error");
      return HTTP_SERVICE_FAILED;
    }
  }

  a2e(safIdt, sizeof(safIdt) - 1);
  jsonPrinter *p = respondWithJsonPrinter(response);

  setResponseStatus(response, HTTP_STATUS_CREATED, "Created");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);

  jsonStart(p);
  {
      jsonAddString(p, "jwt", safIdt);
  }
  jsonEnd(p);

  finishResponse(response);

  return HTTP_SERVICE_SUCCESS;
}

void extractUsernameFromJwt(HttpResponse *response, char *jwt, char *username) {
  char *startPayload, *endPayload;
  int payLoadLength;
  int allocateLength;
  int decodedLength;
  char base64Padding[3] = {0};
  char errBuf[1024];

  // locate JWT payload
  if ((startPayload = strchr(jwt, '.')) == NULL) {
    return;
  }

  startPayload++; // move at the beginning of the JWT payload

  if ((endPayload = strchr(startPayload, '.')) == NULL) {
    return;
  }

  payLoadLength = endPayload - startPayload;
  allocateLength = payLoadLength + 1; // count on '\0' terminating character

  // count on '=' padding characters that are omitted in JWT
  if (payLoadLength % 4 == 3) {
    allocateLength = payLoadLength + 1;
    base64Padding[0] = '=';
  }
  else if (payLoadLength % 4 == 2) {
    allocateLength = payLoadLength + 2;
    base64Padding[0] = '=';
    base64Padding[1] = '=';
  }

  // prepare for base64 decoding
  char *encodedPayload = (char *) SLHAlloc(response->request->slh, allocateLength);
  char *decodedPayload = (char *) SLHAlloc(response->request->slh, allocateLength);
  memset(encodedPayload, 0, allocateLength);
  memset(decodedPayload, 0, allocateLength);

  memcpy(encodedPayload, startPayload, payLoadLength);
  strcat(encodedPayload, base64Padding);

  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2,"Encoded JWT payload: %s\n", encodedPayload);

  // decode JWT payload
  if ((decodedLength = decodeBase64(encodedPayload, decodedPayload)) < 0) {
    return;
  }

  // convert to native encoding
  a2e(decodedPayload, decodedLength);

  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2,"Decoded JWT payload: %s\n", decodedPayload);

  // Parse the username out from the JWT
  Json *payload = jsonParseUnterminatedString(response->request->slh, decodedPayload, decodedLength, errBuf, sizeof(errBuf));
  if (payload == NULL) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING,"Error parsing JWT payload.\n");
    return;
  }

  JsonObject *jsonObject = jsonAsObject(payload);
  char *sub = jsonObjectGetString(jsonObject, "sub");

  if (sub == NULL || strlen(sub) == 0) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING,"No 'sub' claim provided.\n");
    return;
  }

  if (strlen(sub) <= 8) {
    strcpy(username, sub);
  }

  return;
}

static int verify(HttpResponse *response, CrossMemoryServerName *privilegedServerName) {

  ZISAuthServiceStatus status = {0};
  char safIdt[ZIS_AUTH_SERVICE_PARMLIST_SAFIDT_LENGTH + 1] = {0};
  char username[9] = {0};
  HttpRequest *request = response->request;

  Json *body = parseContentBody(request);
  if (body == NULL) {
    respondWithJsonStatus(response, "No body found", HTTP_STATUS_BAD_REQUEST, "Bad Request");
    return HTTP_SERVICE_FAILED;
  }

  JsonObject *jsonObject = jsonAsObject(body);
  char *jwt = jsonObjectGetString(jsonObject, "jwt");

  if (jwt == NULL || strlen(jwt) == 0) {
    respondWithJsonStatus(response, "No jwt provided", HTTP_STATUS_BAD_REQUEST, "Bad Request");
    return HTTP_SERVICE_FAILED;
  }

  if (strlen(jwt) > ZIS_AUTH_SERVICE_PARMLIST_SAFIDT_LENGTH) {
    respondWithJsonStatus(response, "JWT size exceeds length of "STRINGIFY(ZIS_AUTH_SERVICE_PARMLIST_SAFIDT_LENGTH),
                          HTTP_STATUS_BAD_REQUEST, "Bad Request");
    return HTTP_SERVICE_FAILED;
  }

  memcpy(safIdt, jwt, strlen(jwt));
  e2a(safIdt, sizeof(safIdt) - 1);

  // A username variable will be set to a jwt 'sub' claim value or remains an empty string.
  // Extracting a username is done because ACF2 has a bug that requires the username to be specified
  // in the VERIFY macro along with the JWT SAF IDT token. The extractUsernameFromJwt() function can be removed once
  // the ACF2 bug is fixed.
  extractUsernameFromJwt(response, jwt, username);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "extracted username is: %s\n", username);

  int zisRC = zisGenerateOrValidateSafIdt(privilegedServerName, username,
                                          "",
                                          safIdt,
                                          &status);

  a2e(safIdt, sizeof(safIdt) - 1);
  if (zisRC != RC_ZIS_SRVC_OK) {

    if (status.baseStatus.serviceRC == RC_ZIS_AUTHSRV_SAF_ERROR) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO,
              "JWT verification failed: safRC=%d, racfRC=%d, racfRSN=%d\njwt=%s\n",
              status.safStatus.safRC, status.safStatus.racfRC, status.safStatus.racfRSN, safIdt);
      respondWithJsonError(response, "Not authenticated", HTTP_STATUS_UNAUTHORIZED, "Unauthorized");
      return HTTP_SERVICE_FAILED;
    }
    else if (status.baseStatus.serviceRC == RC_ZIS_AUTHSRV_SAF_ABENDED) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE, "Auth service abend: system CC=%X, reason=%X\n",
              status.abendInfo.completionCode, status.abendInfo.reasonCode);
      respondWithJsonError(response, "Check zssServer log for more details", HTTP_STATUS_INTERNAL_SERVER_ERROR, "Internal Server Error");
      return HTTP_SERVICE_FAILED;
    }
    else {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_SEVERE,
              "zis auth service failed: zisRC=%d, serviceRC=%d, serviceRSN=%d, cmsRC=%d, cmsRSN=%d\n",
              zisRC,
              status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
              status.baseStatus.cmsRC, status.baseStatus.cmsRSN);
      respondWithJsonError(response, "Check zssServer log for more details", HTTP_STATUS_INTERNAL_SERVER_ERROR, "Internal Server Error");
      return HTTP_SERVICE_FAILED;
    }
  }

  jsonPrinter *p = respondWithJsonPrinter(response);

  setResponseStatus(response, HTTP_STATUS_OK, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);

  finishResponse(response);

  return HTTP_SERVICE_SUCCESS;
}

static int serveSafIdt(HttpService *service, HttpResponse *response) {
  int rc = HTTP_SERVICE_SUCCESS;
  HttpRequest *request = response->request;
  CrossMemoryServerName *privilegedServerName = getConfiguredProperty(service->server,
                                                HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY);

  if (strcmp(request->method, methodPOST) == 0) {

    // Get the request function
    char *requestType = stringListPrint(request->parsedFile, service->parsedMaskPartCount, 1, "/", 0);

    if (strcasecmp("authenticate", requestType) == 0) {
      rc = authenticate(response, privilegedServerName);
    }
    else if (strcasecmp("verify", requestType) == 0) {
      rc = verify(response, privilegedServerName);
    }
    else {
      respondWithJsonError(response, "Endpoint not found.", 404, "Not Found");
      return HTTP_SERVICE_FAILED;
    }
  }
  else {
     respondWithInvalidMethod(response);
     return HTTP_SERVICE_FAILED;
  }

  return rc;
}

void installSAFIdtTokenService(HttpServer *server) {
  HttpService *httpService = makeGeneratedService("safIdtService", "saf/**");
  httpService->authType = SERVICE_AUTH_NONE;
  httpService->serviceFunction = serveSafIdt;
  httpService->runInSubtask = TRUE;

  registerHttpService(server, httpService);
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/