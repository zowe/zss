

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

#include "httpserver.h"
#include "dataservice.h"
#include "json.h"
#include "crossViewCopy.h"
#include "logging.h"
#include "zssLogging.h"

#include "crossViewCopyService.h"

#ifdef __ZOWE_OS_ZOS

static int serveDatasetToFileCopy(HttpService *service, HttpResponse *response){
  printf("-------------------INSIDE serveDatasetToFileCopy\n");
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);
  HttpRequest *request = response->request;

  if (!strcmp(request->method, methodPOST)){
    char *l1 = stringListPrint(request->parsedFile, 1, 1, "/", 0);
    char *percentDecoded = cleanURLParamValue(response->slh, l1);
    char *datasetNameP1 = stringConcatenate(response->slh, "//'", percentDecoded);
    char *datasetName = stringConcatenate(response->slh, datasetNameP1, "'");
    copyDatasetToUnixAndRespond(response, datasetName);
  } else {
    setContentType(response, "text/json");
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
    addStringHeader(response, "Allow", "POST");
    writeHeader(response);
    finishResponse(response);
  }
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "end %s\n", __FUNCTION__);
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Returning from serveDatasetToFileCopy\n");
  return 0;
}

void installDatasetToFileCopyService(HttpServer *server) {
  printf("-------------------INSIDE installDatasetToFileCopyService\n");

  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_INSTALL_MSG, "dataset copy paste");
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);

  HttpService *httpService = makeGeneratedService("datasetToFileCopy", "/datasetToFileCopy/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  httpService->serviceFunction = serveDatasetToFileCopy;
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

