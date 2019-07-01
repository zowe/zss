

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
#include <strings.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "bpxnet.h"
#include "zos.h"
#include "utils.h"
#include "socketmgmt.h"

#include "httpserver.h"
#include "dataservice.h"
#include "json.h"
#include "datasetjson.h"
#include "logging.h"
#include "zssLogging.h"

#include "datasetService.h"

#ifdef __ZOWE_OS_ZOS
static bool isDatasetMember(char* dsPath, HttpResponse* response) {
  int lParenIndex = lastIndexOf(dsPath, strlen(dsPath), '(');
  int rParenIndex = lastIndexOf(dsPath, strlen(dsPath), ')');
  int isMember = FALSE;
  
  /* Check for (MEMBER) path format */
  if (lParenIndex > -1 && rParenIndex > -1){ 
    if (rParenIndex > lParenIndex){
      if (rParenIndex == strlen(dsPath) - 1) {
        if (rParenIndex - lParenIndex > MEMBER_MAXLEN + 1 || rParenIndex - lParenIndex <= 1) {
          respondWithJsonError(response,"Member name must be at most 8 characters and cannot be blank", 400, "Bad Request");
        }
        return TRUE;
      }
    }
  }
  return FALSE;
}

static int serveDatasetMetadata(HttpService *service, HttpResponse *response) {
  zowelog(NULL, LOG_COMP_ID_DATASET, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);
  HttpRequest *request = response->request;
  if (!strcmp(request->method, methodGET)) {
    if (service->userPointer == NULL){
      MetadataQueryCache *userData = (MetadataQueryCache*)safeMalloc(sizeof(MetadataQueryCache),"Pointer to metadata cache");
      service->userPointer = userData;
    }

    char *l1 = stringListPrint(request->parsedFile, 2, 1, "/", 0); //expect csi or hlq
    zowelog(NULL, LOG_COMP_ID_DATASET, ZOWE_LOG_DEBUG2, "l1=%s\n", l1);
    if (!strcmp(l1, "csi")){
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

  zowelog(NULL, LOG_COMP_ID_DATASET, ZOWE_LOG_DEBUG2, "end %s\n", __FUNCTION__);
  return 0;
}

static int serveDataset(HttpService *service, HttpResponse *response){
  zowelog(NULL, LOG_COMP_ID_DATASET, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);
  HttpRequest *request = response->request;
  char *dsName = stringListPrint(request->parsedFile, 2, 1, "/", 0);
  char *percentDecoded = cleanURLParamValue(response->slh, dsName);
  bool isMember = isDatasetMember(percentDecoded, response);
  char *datasetp1 = stringConcatenate(response->slh, "//'", percentDecoded);
  char *fullPath = stringConcatenate(response->slh, datasetp1, "'");
  if (!strcmp(request->method, methodGET)) {
    zowelog(NULL, LOG_COMP_ID_DATASET, ZOWE_LOG_DEBUG, "Getting dataset records if exists: %s\n", fullPath);
    respondWithDataset(response, percentDecoded, TRUE, service);
  }
  else if (!strcmp(request->method, methodPUT)){
    zowelog(NULL, LOG_COMP_ID_DATASET, ZOWE_LOG_DEBUG, "Updating dataset if exists: %s\n", fullPath);
    updateDataset(response, dsName, TRUE, service);
  }
  else if (!strcmp(request->method, methodPOST)){
    if (!isMember) {
      respondWithJsonError(response, "Dataset allocation is not supported", 501, "Not Implemented");
      return -1;
    }
    else {
      respondWithJsonError(response, "Dataset Member allocation is not supported", 501, "Not Implemented");
      return -1;
    }
  }
  else if (!strcmp(request->method, methodDELETE)){
    if (!isMember) {
      respondWithJsonError(response, "Dataset deletion is not supported", 501, "Not Implemented");
      return -1;
    }
    else {
      respondWithJsonError(response, "Dataset member deletion is not supported", 501, "Not Implemented");
      return -1;
    }
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setContentType(response, "text/json");
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
    addStringHeader(response, "Allow", "GET, PUT, POST, DELETE");
    writeHeader(response);

    jsonStart(out);
    jsonEnd(out);

    finishResponse(response);
  }
  zowelog(NULL, LOG_COMP_ID_DATASET, ZOWE_LOG_DEBUG2, "end %s\n", __FUNCTION__);
  zowelog(NULL, LOG_COMP_ID_DATASET, ZOWE_LOG_DEBUG2, "Returning from serveDataset\n");
  return 0;
}


void installServeDatasetService(HttpServer *server) {
  logConfigureComponent(NULL, LOG_COMP_ID_DATASET, "DATASET SERVICE", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  zowelog(NULL, LOG_COMP_ID_DATASET, ZOWE_LOG_DEBUG2, "Installing dataset contents service\n");
  zowelog(NULL, LOG_COMP_ID_DATASET, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);

  HttpService *httpService = makeGeneratedService("datasetContents", "/datasets/dsn/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  httpService->serviceFunction = serveDataset;
  /* TODO: add VSAM params */
  httpService->paramSpecList = makeStringParamSpec("closeAfter",SERVICE_ARG_OPTIONAL, NULL);
  serveVSAMCache *cache = (serveVSAMCache *) safeMalloc(sizeof(serveVSAMCache), "Pointer to VSAM Cache");
  cache->acbTable = htCreate(0x2000,stringHash,stringCompare,NULL,NULL);
  httpService->userPointer = cache;
  registerHttpService(server, httpService);
}

void installDatasetMetadataService(HttpServer *server) {
  logConfigureComponent(NULL, LOG_COMP_ID_DATASET, "DATASET SERVICE", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  zowelog(NULL, LOG_COMP_ID_DATASET, ZOWE_LOG_DEBUG2, "Installing dataset metadata service\n");
  zowelog(NULL, LOG_COMP_ID_DATASET, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);

  HttpService *httpService = makeGeneratedService("datasetMetadata", "/datasets/detail/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  httpService->serviceFunction = serveDatasetMetadata;
  httpService->paramSpecList =
    makeStringParamSpec("types", SERVICE_ARG_OPTIONAL,
      makeStringParamSpec("detail", SERVICE_ARG_OPTIONAL,
        makeStringParamSpec("listMembers", SERVICE_ARG_OPTIONAL,
          makeStringParamSpec("includeMigrated", SERVICE_ARG_OPTIONAL,
            makeStringParamSpec("updateCache", SERVICE_ARG_OPTIONAL,
              makeStringParamSpec("resumeName", SERVICE_ARG_OPTIONAL,
                makeStringParamSpec("resumeCatalogName", SERVICE_ARG_OPTIONAL,
                  makeStringParamSpec("includeUnprintable", SERVICE_ARG_OPTIONAL,
                    makeIntParamSpec("workAreaSize", SERVICE_ARG_OPTIONAL, 0,0,0,0, NULL)))))))));
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

