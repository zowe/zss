

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
#include "qsam.h"
#else
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "bpxnet.h"
#include "zos.h"
#include "utils.h"
#include "socketmgmt.h"
#include "stcbackground.h"


#include "httpserver.h"
#include "datasetlock.h"
#include "dataservice.h"
#include "json.h"
#include "envService.h"
#include "datasetjson.h"
#include "datasetlock.h"
#include "logging.h"
#include "zssLogging.h"
#include "datasetService.h"

#ifdef __ZOWE_OS_ZOS

static const char DATASET_SERVICE_BG_NAME[] = "DATASET_HEARTBEAT_TASK";
static DatasetLockService* lockService = NULL;

static DatasetLockService* getLockService(void* userPointer) {
  return (DatasetLockService*)userPointer;
};

static void readDatasetSettings(int* heartbeat, int* expiry) {
  JsonObject *datasetLockSettings = readEnvSettings("ZSS_DATASET");
  if (datasetLockSettings == NULL) {
    *heartbeat = 0;
    *expiry = 0;
  }
  *heartbeat = jsonObjectGetNumber(datasetLockSettings, "ZSS_DATASET_HEARTBEAT");
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG,"heartbeat_loop_time %d\n", *heartbeat);
  *expiry = jsonObjectGetNumber(datasetLockSettings, "ZSS_DATASET_EXPIRY");
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG,"heartbeat_expiry_time %d\n", *expiry);
};

static void initLockService() {
  if(lockService == NULL) {
    int heartbeat;
    int expiry;
    // read environment settings
    readDatasetSettings(&heartbeat, &expiry);
    //initialize lock tables
    lockService = initLockResources(heartbeat, expiry);
  }
};

static int serveDatasetMetadata(HttpService *service, HttpResponse *response) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);
  HttpRequest *request = response->request;
  if (!strcmp(request->method, methodGET)) {
    if (service->userPointer == NULL) {
      MetadataQueryCache *userData = (MetadataQueryCache*)safeMalloc(sizeof(MetadataQueryCache),"Pointer to metadata cache");
      service->userPointer = userData;
    }

    char *l1 = stringListPrint(request->parsedFile, 1, 1, "/", 0); //expect name or hlq
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "L1=%s\n", l1);
    if (!strcmp(l1, "name")){
      respondWithDatasetMetadata(response);
    }
    else if (!strcmp(l1, "hlq")) {
      respondWithHLQNames(response,(MetadataQueryCache*)service->userPointer);
    }
    else {
      respondWithJsonError(response, "Invalid Subpath", 400, "Bad Request");
    }
  }
  else{
    jsonPrinter *out = respondWithJsonPrinter(response);

    setContentType(response, "text/json");
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
    addStringHeader(response, "Allow", "GET");
    writeHeader(response);

    jsonStart(out);
    jsonEnd(out);

    finishResponse(response);
  }

  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "end %s\n", __FUNCTION__);
  return 0;
}

static int serveDatasetContents(HttpService *service, HttpResponse *response){
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);  
  HttpRequest *request = response->request;
  if (!strcmp(request->method, methodGET)) {
    char *l1 = stringListPrint(request->parsedFile, 1, 1, "/", 0);
    char *percentDecoded = cleanURLParamValue(response->slh, l1);
    char *filenamep1 = stringConcatenate(response->slh, "//'", percentDecoded);
    char *filename = stringConcatenate(response->slh, filenamep1, "'");
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Serving: %s\n", filename);
    fflush(stdout);
    respondWithDataset(response, filename, TRUE);
  }
  else if (!strcmp(request->method, methodPOST)){
    char *l1 = stringListPrint(request->parsedFile, 1, 1, "/", 0);
    char *percentDecoded = cleanURLParamValue(response->slh, l1);

     /* prepend  //'  to DSN*/
    char *filenamep1 = stringConcatenate(response->slh, "//'", percentDecoded);
    /* append   '    to DSN */    
    char *filename = stringConcatenate(response->slh, filenamep1, "'");             

    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Updating if exists: %s\n", filename);
    fflush(stdout);
    updateDataset(response, filename, TRUE, getLockService(service->userPointer));
  }
  else if (!strcmp(request->method, methodDELETE)) {
    char *l1 = stringListPrint(request->parsedFile, 1, 1, "/", 0);
    char *percentDecoded = cleanURLParamValue(response->slh, l1);
    char *filenamep1 = stringConcatenate(response->slh, "//'", percentDecoded);
    char *filename = stringConcatenate(response->slh, filenamep1, "'");
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Deleting if exists: %s\n", filename);
    fflush(stdout);
    deleteDatasetOrMember(response, filename);
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setContentType(response, "text/json");
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
    addStringHeader(response, "Allow", "GET, DELETE, POST");
    writeHeader(response);

    jsonStart(out);
    jsonEnd(out);

    finishResponse(response);
  }
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "end %s\n", __FUNCTION__);
  return 0;
}

static int serveDatasetEnqueue(HttpService *service, HttpResponse *response){
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);
  HttpRequest *request = response->request;

  if (!strcmp(request->method, methodPOST)) {
    char *l1 = stringListPrint(request->parsedFile, 1, 1, "/", 0);
    char *percentDecoded = cleanURLParamValue(response->slh, l1);
    char *filenamep1 = stringConcatenate(response->slh, "//'", percentDecoded);
    char *filename = stringConcatenate(response->slh, filenamep1, "'");
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Taking enqueue on: %s username: %s\n", filename, request->username);
    respondWithEnqueue(response, filename, TRUE, getLockService(service->userPointer));
  }
  else if (!strcmp(request->method, methodDELETE)) {
    char *l1 = stringListPrint(request->parsedFile, 1, 1, "/", 0);
    char *percentDecoded = cleanURLParamValue(response->slh, l1);
    char *filenamep1 = stringConcatenate(response->slh, "//'", percentDecoded);
    char *filename = stringConcatenate(response->slh, filenamep1, "'");
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Releasing enqueue on: %s , username: %s \n", filename, request->username);
    respondWithDequeue(response, filename, TRUE, getLockService(service->userPointer));
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setContentType(response, "text/json");
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
    addStringHeader(response, "Allow", "DELETE, POST");
    writeHeader(response);

    jsonStart(out);
    jsonEnd(out);

    finishResponse(response);
  }
  return 0;
}

static int serveVSAMDatasetContents(HttpService *service, HttpResponse *response){
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);
  HttpRequest *request = response->request;
  serveVSAMCache *cache = (serveVSAMCache *)service->userPointer;
  if (!strcmp(request->method, methodGET)) {
    char *filename = stringListPrint(request->parsedFile, 1, 1, "/", 0);
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Serving: %s\n", filename);
    fflush(stdout);
    respondWithVSAMDataset(response, filename, cache->acbTable, TRUE);
  }
  else if (!strcmp(request->method, methodPOST)){
    char *filename = stringListPrint(request->parsedFile, 1, 1, "/", 0);
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Updating if exists: %s\n", filename);
    fflush(stdout);
    updateVSAMDataset(response, filename, cache->acbTable, TRUE);
  }
  else if (!strcmp(request->method, methodDELETE)) {
    char *l1 = stringListPrint(request->parsedFile, 1, 1, "/", 0);
    char *percentDecoded = cleanURLParamValue(response->slh, l1);
    char *filenamep1 = stringConcatenate(response->slh, "//'", percentDecoded);
    char *filename = stringConcatenate(response->slh, filenamep1, "'");
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Deleting if exists: %s\n", filename);
    fflush(stdout);
    deleteVSAMDataset(response, filename);
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setContentType(response, "text/json");
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
    addStringHeader(response, "Allow", "GET, DELETE, POST");
    writeHeader(response);

    jsonStart(out);
    jsonEnd(out);

    finishResponse(response);
  }
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "end %s\n", __FUNCTION__);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Returning from serveVSAMdatasetcontents\n");
  return 0;
}

void installDatasetContentsService(HttpServer *server) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_INSTALL_MSG, "dataset contents");
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);

  initLockService();

  HttpService *httpService = makeGeneratedService("datasetContents", "/datasetContents/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  httpService->serviceFunction = serveDatasetContents;
  httpService->userPointer = lockService;
  registerHttpService(server, httpService);
}

void installDatasetEnqueueService(HttpServer *server) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_INSTALL_MSG, "dataset enqueue");
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);

  initLockService();

  HttpService *httpService = makeGeneratedService("datasetEnqueue", "/datasetEnqueue/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->runInSubtask = TRUE;  
  httpService->doImpersonation = TRUE;
  httpService->serviceFunction = serveDatasetEnqueue;
  httpService->userPointer = lockService;
  registerHttpService(server, httpService);
}

void installVSAMDatasetContentsService(HttpServer *server) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_INSTALL_MSG, "VSAM dataset contents");
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);

  HttpService *httpService = makeGeneratedService("VSAMdatasetContents", "/VSAMdatasetContents/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  httpService->serviceFunction = serveVSAMDatasetContents;
  /* TODO: add VSAM params */
  httpService->paramSpecList = makeStringParamSpec("closeAfter",SERVICE_ARG_OPTIONAL, NULL);
  serveVSAMCache *cache = (serveVSAMCache *) safeMalloc(sizeof(serveVSAMCache), "Pointer to VSAM Cache");
  cache->acbTable = htCreate(0x2000,stringHash,stringCompare,NULL,NULL);
  httpService->userPointer = cache;
  registerHttpService(server, httpService);
}

void installDatasetMetadataService(HttpServer *server) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_INSTALL_MSG, "dataset metadata");
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);


  HttpService *httpService = makeGeneratedService("datasetMetadata", "/datasetMetadata/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  httpService->serviceFunction = serveDatasetMetadata;
  httpService->paramSpecList =
    makeStringParamSpec("addQualifiers", SERVICE_ARG_OPTIONAL,
      makeStringParamSpec("types", SERVICE_ARG_OPTIONAL,
        makeStringParamSpec("detail", SERVICE_ARG_OPTIONAL,
          makeStringParamSpec("listMembers", SERVICE_ARG_OPTIONAL,
            makeStringParamSpec("includeMigrated", SERVICE_ARG_OPTIONAL,
              makeStringParamSpec("updateCache", SERVICE_ARG_OPTIONAL,
                makeStringParamSpec("resumeName", SERVICE_ARG_OPTIONAL,
                  makeStringParamSpec("resumeCatalogName", SERVICE_ARG_OPTIONAL,
                    makeStringParamSpec("includeUnprintable", SERVICE_ARG_OPTIONAL,
                      makeIntParamSpec("workAreaSize", SERVICE_ARG_OPTIONAL, 0,0,0,0, NULL))))))))));
  registerHttpService(server, httpService);
}


static int datasetHeartbeatMonitor(STCBase* stcbase, STCModule* stcmodule, STCIntervalCallbackData* callbackData, void* lockService) {
  heartbeatBackgroundHandler(getLockService(lockService));
}

static int serveDatasetHeartbeat(HttpService *service, HttpResponse *response){
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__); 
  HttpRequest *request = response->request;
  if (!strcmp(request->method, methodPOST)) { 
    resetTimeInHbt(getLockService(service->userPointer), request->username);
    respondWithMessage(response, HTTP_STATUS_NO_CONTENT, "");
  } else {
    addStringHeader(response, "Allow", "POST");
    respondWithError(response, HTTP_STATUS_METHOD_NOT_FOUND, "Method Not Allowed");
  }
  return 0;
}



void installDatasetHeartbeatService(HttpServer *server, STCModule* backgroundModule) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, ZSS_LOG_INSTALL_MSG, "dataset Heartbeat");

  initLockService();
  // register background handler
  stcAddIntervalCallback(backgroundModule, &datasetHeartbeatMonitor,DATASET_SERVICE_BG_NAME, lockService->heartbeat, lockService);

  HttpService *httpService = makeGeneratedService("datasetHeartbeart", "/datasetHeartbeat/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->runInSubtask = TRUE;  
  httpService->doImpersonation = FALSE;
  httpService->serviceFunction = serveDatasetHeartbeat;
  httpService->userPointer = lockService;
  registerHttpService(server, httpService);
}

#endif /* __ZOWE_OS_ZOS */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

