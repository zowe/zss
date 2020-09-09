
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

static int serveHelloWorldDataService(HttpService *service, HttpResponse *response)
{
  printf("Serve Hello World Data Service");
  HttpRequest *request = response->request;
  char *routeFragment = stringListPrint(request->parsedFile, 1, 1000, "/", 0);
  char *route = stringConcatenate(response->slh, "/", routeFragment);
    
  if (!strcmp(request->method, methodPOST))
  {
    printf("POST Method");
    char *inPtr = request->contentBody;
    char *nativeBody = copyStringToNative(request->slh, inPtr, strlen(inPtr));
    int inLen = nativeBody == NULL ? 0 : strlen(nativeBody);
    char errBuf[1024];
    Json *body = jsonParseUnterminatedString(request->slh, nativeBody, inLen, errBuf, sizeof(errBuf));
    
    JsonObject *inputMessage = jsonAsObject(body);
    if (inputMessage == NULL) {
      respondWithJsonError(response, "Request body has no JSON object", 400, "Bad Request");
      return 0;
    }
    
    Json *pathToCert = jsonObjectGetPropertyValue(inputMessage, "pathToCert");
    if (pathToCert == NULL) {
      respondWithJsonError(response, "pathToCert is missing from request body", 400, "Bad Request");
      return 0;
    }
    
    // Probably JSON as array?
    char *pathToCertText = jsonAsString(pathToCert);
    printf("Path: %s", pathToCertText);
    
    FILE *fileptr;
    char *buffer;
    long filelen;

    fileptr = fopen("/a/apimtst/cert.der", "rb");  // Open the file in binary mode
    fseek(fileptr, 0, SEEK_END);          // Jump to the end of the file
    filelen = ftell(fileptr);             // Get the current byte offset in the file
    rewind(fileptr);                      // Jump back to the beginning of the file

    buffer = (char *)malloc(filelen * sizeof(char)); // Enough memory for the file
    fread(buffer, filelen, 1, fileptr); // Read in the entire file
    fclose(fileptr); // Close the file

    R_datalib_parm_list_64 example;
    memset(&example, 0, sizeof(R_datalib_parm_list_64));

    example.certificate_len = filelen;
    memcpy(example.certificate, buffer, filelen);

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
    printf("RC: %d, SAF: %d, RACF: %d. Reason: %d\n", rc, example.return_code, example.RACF_return_code, example.RACF_reason_code);
    printf("Application Id: %s \n", example.application_id);
    printf("RACF User id: %s\n", example.RACF_userid);
      
    jsonPrinter *p = respondWithJsonPrinter(response);
    
    setResponseStatus(response, 200, "OK");
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);
    
    jsonStart(p);
    {
      jsonAddString(p, "RACF_userid", example.RACF_userid);
    }
    jsonEnd(p);
  }
  else 
  {
    jsonPrinter *p = respondWithJsonPrinter(response);
      
    setResponseStatus(response, 405, "Method Not Allowed");
    setDefaultJSONRESTHeaders(response);
    addStringHeader(response, "Allow", "GET");
    writeHeader(response);
    
    jsonStart(p);
    {
      jsonAddString(p, "error", "Only GET requests are supported");
    }
    jsonEnd(p); 
  }
  
  finishResponse(response);
    
  return 0;
}

void installHelloWorldService(HttpServer *server)
{
  printf("Hello World Installed \n");

  HttpService *httpService = makeGeneratedService("HelloWorldService", "/hello/**");
  httpService->authType = SERVICE_AUTH_NONE;
  httpService->serviceFunction = serveHelloWorldDataService;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = FALSE;

  registerHttpService(server, httpService);
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

