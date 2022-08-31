
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

#pragma linkage(IRRSIM00, OS)

#define MAP_CERTIFICATE_TO_USERNAME 0x0006
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
    short applicationIdLength;
    char applicationId[246];
    short distinguishedNameLength;
    char distinguishedName[246];
    short registryNameLength;
    char registryName[255];
    int certificateLength;
    char certificate[4096];
} RUsermapParamList;

static void setValidResponseCode(HttpResponse *response, int rc, int returnCode, int returnCodeRacf, int reasonCodeRacf) {
  if (rc == SUCCESS_RC && returnCode == SUCCESS_RC_SAF && returnCodeRacf == SUCCESS_RC_RACF && reasonCodeRacf == SUCCESS_REASON_CODE_RACF) {
    setResponseStatus(response, 200, "OK");
    return;
  } else if(rc != SUCCESS_RC) {
    if(returnCode == SAF_FAILURE_RC && returnCodeRacf == RACF_FAILURE_RC) {
      if(reasonCodeRacf == PARAMETER_LIST_ERROR_RC) {
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

static void respondWithBadRequest(HttpResponse *response) {
    jsonPrinter *p = respondWithJsonPrinter(response);

    setResponseStatus(response, 400, "Bad Request");
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);

    jsonStart(p);
    {
      jsonAddString(p, "error", "The length of the certificate is longer than 4096 bytes");
    }
    jsonEnd(p);

    finishResponse(response);
}

static int serveMappingService(HttpService *service, HttpResponse *response)
{
  HttpRequest *request = response->request;

  if (!strcmp(request->method, methodPOST))
  {
    RUsermapParamList *userMapCertificateStructure 
      = (RUsermapParamList*)safeMalloc31(sizeof(RUsermapParamList),"RUsermapParamList");
    memset(userMapCertificateStructure, 0, sizeof(RUsermapParamList));

    if(request->contentLength > sizeof(userMapCertificateStructure->certificate) || request->contentLength < 0) {
      respondWithBadRequest(response);
      return 0;
    }

    userMapCertificateStructure->certificateLength = request->contentLength;
    memset(userMapCertificateStructure->certificate, 0, request->contentLength);
    memcpy(userMapCertificateStructure->certificate, request->contentBody, request->contentLength);

    userMapCertificateStructure->functionCode = MAP_CERTIFICATE_TO_USERNAME;
    int rc;

#ifdef _LP64 
    __asm(ASM_PREFIX
	  /* We get the routine pointer for IRRSIM00 by an, *ahem*, direct approach.
	     These offsets are stable, and this avoids linker/pragma mojo */
	  " LA 15,X'10' \n"
          " LG 15,X'220'(,15) \n" /* CSRTABLE */
          " LG 15,X'28'(,15) \n" /* Some RACF Routin Vector */
          " LG 15,X'A0'(,15) \n" /* IRRSIM00 itself */
	  " LG 1,%0 \n"
	  " SAM31 \n"
	  " BALR 14,15 \n"
          " SAM64 \n"
          " ST 15,%0"
	  :
	  :"m"(userMapCertificateStructure),"m"(rc)
	  :"r14","r15");
#else
    rc = IRRSIM00(
        &userMapCertificateStructure->workarea, // WORKAREA
        &userMapCertificateStructure->safRcAlet  , // ALET
        &userMapCertificateStructure->returnCode,
        &userMapCertificateStructure->racfRcAlet,
        &userMapCertificateStructure->returnCodeRacf,
        &userMapCertificateStructure->racfReasonAlet,
        &userMapCertificateStructure->reasonCodeRacf,
        &userMapCertificateStructure->fcAlet,
        &userMapCertificateStructure->functionCode,
        &userMapCertificateStructure->optionWord,
        &userMapCertificateStructure->useridLengthRacf,
        &userMapCertificateStructure->certificateLength,
        &userMapCertificateStructure->applicationIdLength,
        &userMapCertificateStructure->distinguishedNameLength,
        &userMapCertificateStructure->registryNameLength
    );
#endif

    jsonPrinter *p = respondWithJsonPrinter(response);

    setValidResponseCode(response, rc, userMapCertificateStructure->returnCode, userMapCertificateStructure->returnCodeRacf, userMapCertificateStructure->reasonCodeRacf);
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);
    
    jsonStart(p);
    {
      jsonAddString(p, "userid", userMapCertificateStructure->useridRacf);
      jsonAddInt(p, "returnCode", rc);
      jsonAddInt(p, "safReturnCode", userMapCertificateStructure->returnCode);
      jsonAddInt(p, "racfReturnCode", userMapCertificateStructure->returnCodeRacf);
      jsonAddInt(p, "racfReasonCode", userMapCertificateStructure->reasonCodeRacf);
    }
    jsonEnd(p);

    safeFree31((char*)userMapCertificateStructure,sizeof(RUsermapParamList));
    finishResponse(response);
  }
  else
  {
     respondWithInvalidMethod(response);
  }

  return 0;
}

void installCertificateService(HttpServer *server)
{
  HttpService *httpService = makeGeneratedService("CertificateService", "/certificate/x509/**");
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
