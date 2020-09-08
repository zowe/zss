
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

typedef struct HelloServiceData_t {
  int timesVisited;
  uint64 loggingId;
} HelloServiceData;

static int serveHelloWorldDataService(HttpService *service, HttpResponse *response)
{
  printf("Serve Hello World Data Service");
  HttpRequest *request = response->request;
  char *routeFragment = stringListPrint(request->parsedFile, 1, 1000, "/", 0);
  char *route = stringConcatenate(response->slh, "/", routeFragment);
  
  HelloServiceData *serviceData = service->userPointer;
  serviceData->timesVisited++;

  zowelog(NULL, serviceData->loggingId, ZOWE_LOG_WARNING,
          "Inside serveHelloWorldDataService\n");
  
  if (!strcmp(request->method, methodGET)) 
  {
    jsonPrinter *p = respondWithJsonPrinter(response);
    
    setResponseStatus(response, 200, "OK");
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);
    
    jsonStart(p);
    {
      jsonAddString(p, "message", "Hello World!");
      jsonAddInt(p, "timesVisited", serviceData->timesVisited);
    }
    jsonEnd(p);
  }
  else if (!strcmp(request->method, methodPOST))
  {
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
    
    Json *timesVisited = jsonObjectGetPropertyValue(inputMessage, "timesVisited");
    if (timesVisited == NULL) {
      respondWithJsonError(response, "timesVisited is missing from request body", 400, "Bad Request");
      return 0;
    }
    
    serviceData->timesVisited = jsonAsNumber(timesVisited);
    
    jsonPrinter *p = respondWithJsonPrinter(response);
    
    setResponseStatus(response, 200, "OK");
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);
    
    jsonStart(p);
    {
      jsonAddInt(p, "timesVisited", serviceData->timesVisited);
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
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = serveHelloWorldDataService;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;

  HelloServiceData *serviceData = (HelloServiceData*)safeMalloc(sizeof(HelloServiceData), "HelloServiceData");
  httpService->userPointer = serviceData;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

