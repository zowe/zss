
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include "metalio.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "certificateService.h"
#include "httpserver.h"
#include "json.h"
#include "http.h"
#include "rusermap.h"

#pragma linkage(IRRSIM00, OS)

#define MAP_CERTIFICATE_TO_USERNAME 0x0006
#define MAP_DN_TO_USERNAME 0x0008
#define SUCCESS_RC 0
#define SUCCESS_RC_SAF 0
#define SUCCESS_RC_RACF 0
#define SUCCESS_REASON_CODE_RACF 0

#define SAF_FAILURE_RC 8
#define RACF_FAILURE_RC 8

// Reason codes representing potential errors within R_Usermap macro
#define PARAMETER_LIST_ERROR_RC 4
#define NO_MAPPING_TO_USERID_RC 16
#define NOT_AUTHORIZED_RC 20
#define CERTIFICATE_NOT_VALID_RC 28
#define NOTRUST_CERTIFICATE_RC 32
#define NO_IDENTITY_FILTER_RC 48

#define MAX_URL_LENGTH 21

static void setValidResponseCode(HttpResponse *response, int rc, int returnCode, int returnCodeRacf, int reasonCodeRacf) {
  if (rc == SUCCESS_RC && returnCode == SUCCESS_RC_SAF && returnCodeRacf == SUCCESS_RC_RACF && reasonCodeRacf == SUCCESS_REASON_CODE_RACF) {
    setResponseStatus(response, 200, "OK");
    return;
  } else if (rc != SUCCESS_RC) {
    if (returnCode == SAF_FAILURE_RC && returnCodeRacf == RACF_FAILURE_RC) {
      if (reasonCodeRacf == PARAMETER_LIST_ERROR_RC) {
        setResponseStatus(response, 400, "Bad request");
        return;
      } else if (
                 reasonCodeRacf == NO_MAPPING_TO_USERID_RC ||
                 reasonCodeRacf == NOT_AUTHORIZED_RC ||
                 reasonCodeRacf == CERTIFICATE_NOT_VALID_RC ||
                 reasonCodeRacf == NOTRUST_CERTIFICATE_RC ||
                 reasonCodeRacf == NO_IDENTITY_FILTER_RC
                 ) {
        setResponseStatus(response, 401, "Unauthorized");
        return;
      }
    }
  }

  setResponseStatus(response, 500, "Internal server error");
  return;
}

static void respondWithInvalidMethod(HttpResponse *response) {
    jsonPrinter *p = respondWithJsonPrinter(response);

    setResponseStatus(response, 405, "Method Not Allowed");
    setDefaultJSONRESTHeaders(response);
    addStringHeader(response, "Allow", "POST");
    writeHeader(response);

    jsonStart(p);
    {
      jsonAddString(p, "error", "Only POST requests are supported");
    }
    jsonEnd(p);

    finishResponse(response);
}

static int serveMappingService(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;

  if (!strcmp(request->method, methodPOST))
  {

    int urlLength = strlen(request->uri);
    if(urlLength < 0 || urlLength > MAX_URL_LENGTH) {
        respondWithJsonError(response, "URI exceeded maximum number of characters.", 400, "Bad Request");
        return 0;
    }

    char translatedURL[urlLength + 1];
    strcpy(translatedURL, request->uri);
    a2e(translatedURL, sizeof(translatedURL));
    char *x509URI = strstr(translatedURL, "x509/map");
    char *distinguishedNameURI = strstr(translatedURL, "dn");

    char useridRacf[9];
    int returnCodeRacf = 0;
    int reasonCodeRacf = 0;
    int rc;
    if(x509URI != NULL) {
    //  Certificate to user mapping
        if (request->contentLength < 1) {
          respondWithJsonError(response, "The length of the certificate is less then 1", 400, "Bad Request");
          return 0;
        }
        rc = getUseridByCertificate(request->contentBody, request->contentLength, useridRacf, &returnCodeRacf, &reasonCodeRacf);
    } else if (distinguishedNameURI != NULL) {
    //    Distinguished name to user mapping
        char *bodyNativeEncoding = copyStringToNative(request->slh, request->contentBody, request->contentLength);
        char errorBuffer[2048];
        Json *body = jsonParseUnterminatedString(request->slh, bodyNativeEncoding, request->contentLength, errorBuffer, sizeof(errorBuffer));
        if (body == NULL) {
            respondWithJsonError(response, "JSON in request body has incorrect structure.", 400, "Bad Request");
            return 0;
        }
        JsonObject *jsonObject = jsonAsObject(body);
        if (jsonObject == NULL) {
            respondWithJsonError(response, "Object in request body is not a JSON type.", 400, "Bad Request");
            return 0;
        }
        char *distinguishedId = jsonObjectGetString(jsonObject, "dn");
        char *registry = jsonObjectGetString(jsonObject, "registry");
        if (distinguishedId == NULL || registry == NULL) {
            respondWithJsonError(response, "Object in request is missing dn or registry parameter.", 400, "Bad Request");
            return 0;
        }
        rc = getUseridByDN(distinguishedId, strlen(distinguishedId), registry, strlen(registry), useridRacf, &returnCodeRacf, &reasonCodeRacf);

    } else {
        respondWithJsonError(response, "Endpoint not found.", 404, "Not Found");
        return 0;
    }

    jsonPrinter *p = respondWithJsonPrinter(response);

    setValidResponseCode(response, rc, rc, returnCodeRacf, reasonCodeRacf);
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);

    jsonStart(p);
    {
      jsonAddString(p, "userid", useridRacf);
      jsonAddInt(p, "returnCode", rc);
      jsonAddInt(p, "safReturnCode", rc);
      jsonAddInt(p, "racfReturnCode", returnCodeRacf);
      jsonAddInt(p, "racfReasonCode", reasonCodeRacf);
    }
    jsonEnd(p);

    finishResponse(response);
  }
  else
  {
     respondWithInvalidMethod(response);
  }
  return 0;
}

void installUserMappingService(HttpServer *server) {
  HttpService *httpService = makeGeneratedService("UserMappingService", "/certificate/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = serveMappingService;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;

  registerHttpService(server, httpService);
}

// TestProperReturnCodesBasedOnTheSAFAnswers
// Test SAF with different certs?

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
