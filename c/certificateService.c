
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
#include <metal/stdarg.h>  
#include "metalio.h"
#include "qsam.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>  
#include <pthread.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "charsets.h"
#include "bpxnet.h"
#include "socketmgmt.h"
#include "httpserver.h"
#include "json.h"
#include "logging.h"
#include "dataservice.h"
#include "http.h"

#pragma linkage(IRRSIM00, OS)

typedef _Packed struct _RUsermapParamList { 
   double workarea[128]; 
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
  if(rc == 0 && returnCode == 0 && returnCodeRacf == 0 && reasonCodeRacf == 0) {
    setResponseStatus(response, 200, "OK");
  } else if ((rc != 0 && returnCode == 8 && returnCodeRacf == 8 && reasonCodeRacf == 4)
  || (rc != 0 && returnCode == 8 && returnCodeRacf == 8 && reasonCodeRacf == 40)
  || (rc != 0 && returnCode == 8 && returnCodeRacf == 8 && reasonCodeRacf == 44) ) {
    setResponseStatus(response, 400, "Bad request");
  } else if ((rc != 0 && returnCode == 8 && returnCodeRacf == 8 && reasonCodeRacf == 16)
  || (rc != 0 && returnCode == 8 && returnCodeRacf == 8 && reasonCodeRacf == 20)
  || (rc != 0 && returnCode == 8 && returnCodeRacf == 8 && reasonCodeRacf == 24)
  || (rc != 0 && returnCode == 8 && returnCodeRacf == 8 && reasonCodeRacf == 28)
  || (rc != 0 && returnCode == 8 && returnCodeRacf == 8 && reasonCodeRacf == 32)
  || (rc != 0 && returnCode == 8 && returnCodeRacf == 8 && reasonCodeRacf == 48)) {
    setResponseStatus(response, 401, "Unauthorized");
  } else {
    setResponseStatus(response, 500, "Internal server error");
  }
}

static void handleInvalidMethod(HttpResponse *response) {
    jsonPrinter *p = respondWithJsonPrinter(response);
      
    setResponseStatus(response, 405, "Method Not Allowed");
    setDefaultJSONRESTHeaders(response);
    addStringHeader(response, "Allow", "GET");
    writeHeader(response);
    
    jsonStart(p);
    {
      jsonAddString(p, "error", "Only POST requests are supported");
    }
    jsonEnd(p);
}

static int serveMappingService(HttpService *service, HttpResponse *response)
{
  HttpRequest *request = response->request;
    
  if (!strcmp(request->method, methodPOST))
  {
    char *inPtr = request->contentBody;
    if(request->contentLength >= 4096) {
      setResponseStatus(response, 400, "Bad request");
      finishResponse(response);
    }
    
    RUsermapParamList userMapCertificateStructure;
    memset(&userMapCertificateStructure, 0, sizeof(RUsermapParamList));

    userMapCertificateStructure.certificateLength = request->contentLength;
    memcpy(userMapCertificateStructure.certificate, inPtr, request->contentLength);

    userMapCertificateStructure.functionCode = 0x0006;
    int rc; 

    rc = IRRSIM00(
        &userMapCertificateStructure.workarea, // WORKAREA 
        &userMapCertificateStructure.safRcAlet  , // ALET 
        &userMapCertificateStructure.returnCode, 
        &userMapCertificateStructure.racfRcAlet, 
        &userMapCertificateStructure.returnCodeRacf,
        &userMapCertificateStructure.racfReasonAlet,
        &userMapCertificateStructure.reasonCodeRacf,
        &userMapCertificateStructure.fcAlet,
        &userMapCertificateStructure.functionCode,
        &userMapCertificateStructure.optionWord,
        &userMapCertificateStructure.useridLengthRacf,
        &userMapCertificateStructure.certificateLength,
        &userMapCertificateStructure.applicationIdLength,
        &userMapCertificateStructure.distinguishedNameLength,
        &userMapCertificateStructure.registryNameLength
    );
      
    jsonPrinter *p = respondWithJsonPrinter(response);
    
    setValidResponseCode(response, rc, userMapCertificateStructure.returnCode, userMapCertificateStructure.returnCodeRacf, userMapCertificateStructure.reasonCodeRacf);
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);
    
    jsonStart(p);
    {
      jsonAddString(p, "userid", userMapCertificateStructure.useridRacf);
      jsonAddInt(p, "rc", rc);
      jsonAddInt(p, "saf_rc", userMapCertificateStructure.returnCode);
      jsonAddInt(p, "racf_rc", userMapCertificateStructure.returnCodeRacf);
      jsonAddInt(p, "reason_code", userMapCertificateStructure.reasonCodeRacf);
    }
    jsonEnd(p);
  }
  else 
  {
     handleInvalidMethod(response);
  }
  
  finishResponse(response);
    
  return 0;
}

void installCertificateService(HttpServer *server)
{
  HttpService *httpService = makeGeneratedService("CertificateService", "/certificate/x509/**");
  httpService->authType = SERVICE_AUTH_NONE;
  httpService->serviceFunction = serveMappingService;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = FALSE;

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
