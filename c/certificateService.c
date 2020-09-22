
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

typedef _Packed struct _R_datalib_parm_list_64 { 
   double workarea[128]; 
    int saf_rc_ALET, return_code;
    int racf_rc_ALET, RACF_return_code;
    int racf_rsn_ALET, RACF_reason_code;
    int fc_ALET;
    short function_code;
    int  option_word;
    char RACF_userid_len; 
    char RACF_userid[8];
    int certificate_len;
    char certificate[4096];
    short application_id_len;
    char application_id[246];
    short distinguished_name_len;
    char distinguished_name[246];
    short registry_name_len;
    char registry_name[255];
    
} R_datalib_parm_list_64;

void setValidResponseCode(HttpResponse *response, int rc, int return_code, int RACF_return_code, int RACF_reason_code) {
  if(rc == 0 && return_code == 0 && RACF_return_code == 0 && RACF_reason_code == 0) {
    setResponseStatus(response, 200, "OK");
  } else if ((rc != 0 && return_code == 8 && RACF_return_code == 8 && RACF_reason_code == 4)
  || (rc != 0 && return_code == 8 && RACF_return_code == 8 && RACF_reason_code == 40)
  || (rc != 0 && return_code == 8 && RACF_return_code == 8 && RACF_reason_code == 44) ) {
    setResponseStatus(response, 400, "Bad request");
  } else if ((rc != 0 && return_code == 8 && RACF_return_code == 8 && RACF_reason_code == 16)
  || (rc != 0 && return_code == 8 && RACF_return_code == 8 && RACF_reason_code == 20)
  || (rc != 0 && return_code == 8 && RACF_return_code == 8 && RACF_reason_code == 24)
  || (rc != 0 && return_code == 8 && RACF_return_code == 8 && RACF_reason_code == 28)
  || (rc != 0 && return_code == 8 && RACF_return_code == 8 && RACF_reason_code == 32)
  || (rc != 0 && return_code == 8 && RACF_return_code == 8 && RACF_reason_code == 48)) {
    setResponseStatus(response, 401, "Unauthorized");
  } else {
    setResponseStatus(response, 500, "Internal server error");
  }
}

void handleInvalidMethod(HttpResponse *response) {
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
  char *routeFragment = stringListPrint(request->parsedFile, 1, 1000, "/", 0);
  char *route = stringConcatenate(response->slh, "/", routeFragment);
    
  if (!strcmp(request->method, methodPOST))
  {
    char *inPtr = request->contentBody;
    if(request->contentLength >= 4096) {
      setResponseStatus(response, 400, "Bad request");
      finishResponse(response);
    }
    
    R_datalib_parm_list_64 example;
    memset(&example, 0, sizeof(R_datalib_parm_list_64));

    example.certificate_len = request->contentLength;
    memcpy(example.certificate, inPtr, request->contentLength);

    example.function_code = 0x0006;
    int rc; 

    rc = IRRSIM00(
        &example.workarea, // WORKAREA 
        &example.saf_rc_ALET  , // ALET 
        &example.return_code, 
        &example.racf_rc_ALET, 
        &example.RACF_return_code,
        &example.racf_rsn_ALET,
        &example.RACF_reason_code,
        &example.fc_ALET,
        &example.function_code,
        &example.option_word,
        &example.RACF_userid_len,
        &example.certificate_len,
        &example.application_id_len,
        &example.distinguished_name_len,
        &example.registry_name_len
    );
      
    jsonPrinter *p = respondWithJsonPrinter(response);
    
    setValidResponseCode(response, rc, example.return_code, example.RACF_return_code, example.RACF_reason_code);
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);
    
    jsonStart(p);
    {
      jsonAddString(p, "userid", example.RACF_userid);
      jsonAddInt(p, "rc", rc);
      jsonAddInt(p, "saf_rc", example.return_code);
      jsonAddInt(p, "racf_rc", example.RACF_return_code);
      jsonAddInt(p, "reason_code", example.RACF_reason_code);
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
