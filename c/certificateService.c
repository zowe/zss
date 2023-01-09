
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
#include "alloc.h"

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

typedef _Packed struct _RUsermapParamList {
    char workarea[1024];
    int safRcAlet, returnCode;
    int racfRcAlet, returnCodeRacf;
    int racfReasonAlet, reasonCodeRacf;
    int fcAlet;
    short functionCode;
    int  optionWord;
    char useridLengthRacf;
    char useridRacf[8];
    int certificateLength;
    char certificate[4096];
    short applicationIdLength;
    char applicationId[246];
    short distinguishedNameLength;
    char distinguishedName[246];
    short registryNameLength;
    char registryName[255];
} RUsermapParamList;

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

static int serveMappingService(HttpService *service, HttpResponse *response)
{
  HttpRequest *request = response->request;

  if (!strcmp(request->method, methodPOST))
  {

    int urlLength = strlen(request->uri);
    if(urlLength < 0 || urlLength > 17) {
        respondWithJsonError(response, "URI is longer than maximum length of 17 characters.", 400, "Bad Request");
        return 0;
    }

    char translatedURL[urlLength + 1];
    strcpy(translatedURL, request->uri);
    a2e(translatedURL, sizeof(translatedURL));
    char *x509URI = strstr(translatedURL,"x509");
    char *dnURI = strstr(translatedURL,"dn");
    ALLOC_STRUCT31(
      STRUCT31_NAME(parmlist31),
      STRUCT31_FIELDS(
        RUsermapParamList userMapStructure;
      )
    );
    if(x509URI != NULL) {
    //  Certificate to user mapping
        if(request->contentLength > sizeof(parmlist31->userMapStructure.certificate) || request->contentLength < 1) {
          respondWithJsonError(response, "The length of the certificate is longer than 4096 bytes", 400, "Bad Request");
          goto out;
        }

        parmlist31->userMapStructure.certificateLength = request->contentLength;
        memcpy(parmlist31->userMapStructure.certificate, request->contentBody, request->contentLength);

        parmlist31->userMapStructure.functionCode = MAP_CERTIFICATE_TO_USERNAME;
    } else if (dnURI != NULL) {
    //    Distinguished ID to user mapping
        char *bodyNativeEncoding = copyStringToNative(request->slh, request->contentBody, request->contentLength);
        char errorBuffer[2048];
        Json *body = jsonParseUnterminatedString(request->slh, bodyNativeEncoding, request->contentLength, errorBuffer, sizeof(errorBuffer));
        if ( body == NULL ) {
          respondWithJsonError(response, "JSON in request body has incorrect structure.", 400, "Bad Request");
          goto out;
        }
        JsonObject *jsonObject = jsonAsObject(body);
        if ( jsonObject == NULL ) {
          respondWithJsonError(response, "Object in request body is not a JSON type.", 400, "Bad Request");
          goto out;
        }
        char *distinguishedId = jsonObjectGetString(jsonObject, "dn");
        if(distinguishedId == NULL || strlen(distinguishedId) == 0) {
            respondWithJsonError(response, "dn field not included in request body", 400, "Bad Request");
            goto out;
        } else if(strlen(distinguishedId) > sizeof(parmlist31->userMapStructure.distinguishedName)) {
            respondWithJsonError(response, "The length of the distinguished name is more than 246 bytes", 400, "Bad Request");
            goto out;
        }

        parmlist31->userMapStructure.distinguishedNameLength = strlen(distinguishedId);
        memcpy(parmlist31->userMapStructure.distinguishedName, distinguishedId, strlen(distinguishedId));
        e2a(parmlist31->userMapStructure.distinguishedName, parmlist31->userMapStructure.distinguishedNameLength);
        char *registry = jsonObjectGetString(jsonObject, "registry");
        if(registry == NULL || strlen(registry) == 0) {
            respondWithJsonError(response, "registry field not included in request body", 400, "Bad Request");
            goto out;
        } else if(strlen(registry) > sizeof(parmlist31->userMapStructure.registryName)){
             respondWithJsonError(response, "The length of the registry name is more than 255 bytes", 400, "Bad Request");
             goto out;
        }

        parmlist31->userMapStructure.registryNameLength = strlen(registry);
        memset(parmlist31->userMapStructure.registryName, 0, strlen(registry));
        memcpy(parmlist31->userMapStructure.registryName, registry, strlen(registry));
        e2a(parmlist31->userMapStructure.registryName, parmlist31->userMapStructure.registryNameLength);

        parmlist31->userMapStructure.functionCode = MAP_DN_TO_USERNAME;

    } else {
        respondWithJsonError(response, "Endpoint not found.", 404, "Not Found");
        goto out;
    }

    int rc;
#ifdef _LP64
    __asm(ASM_PREFIX
	  /* We get the routine pointer for IRRSIM00 by an, *ahem*, direct approach.
	     These offsets are stable, and this avoids linker/pragma mojo */
	        " LLGT 15,X'10' \n"
          " LLGT 15,X'220'(,15) \n" /* CSRTABLE */
          " LLGT 15,X'28'(,15) \n" /* Some RACF Routin Vector */
          " LLGT 15,X'A0'(,15) \n" /* IRRSIM00 itself */
	  " LG 1,%0 \n"
	  " SAM31 \n"
	  " BALR 14,15 \n"
          " SAM64 \n"
          " ST 15,%0"
	  :
	  :"m"(parmlist31->userMapStructure),"m"(rc)
	  :"r14","r15");
#else
    rc = IRRSIM00(
        &parmlist31->userMapStructure.workarea, // WORKAREA
        &parmlist31->userMapStructure.safRcAlet  , // ALET
        &parmlist31->userMapStructure.returnCode,
        &parmlist31->userMapStructure.racfRcAlet,
        &parmlist31->userMapStructure.returnCodeRacf,
        &parmlist31->userMapStructure.racfReasonAlet,
        &parmlist31->userMapStructure.reasonCodeRacf,
        &parmlist31->userMapStructure.fcAlet,
        &parmlist31->userMapStructure.functionCode,
        &parmlist31->userMapStructure.optionWord,
        &parmlist31->userMapStructure.useridLengthRacf,
        &parmlist31->userMapStructure.certificateLength,
        &parmlist31->userMapStructure.applicationIdLength,
        &parmlist31->userMapStructure.distinguishedNameLength,
        &parmlist31->userMapStructure.registryNameLength
    );
#endif

    jsonPrinter *p = respondWithJsonPrinter(response);

    setValidResponseCode(response, rc, parmlist31->userMapStructure.returnCode, parmlist31->userMapStructure.returnCodeRacf, parmlist31->userMapStructure.reasonCodeRacf);
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);

    jsonStart(p);
    {
      jsonAddString(p, "userid", parmlist31->userMapStructure.useridRacf);
      jsonAddInt(p, "returnCode", rc);
      jsonAddInt(p, "safReturnCode", parmlist31->userMapStructure.returnCode);
      jsonAddInt(p, "racfReturnCode", parmlist31->userMapStructure.returnCodeRacf);
      jsonAddInt(p, "racfReasonCode", parmlist31->userMapStructure.reasonCodeRacf);
    }
    jsonEnd(p);

    finishResponse(response);
    out:
    FREE_STRUCT31(
      STRUCT31_NAME(parmlist31)
    );
  }
  else
  {
     respondWithInvalidMethod(response);
  }
  return 0;
}

void installUserMappingService(HttpServer *server)
{
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
