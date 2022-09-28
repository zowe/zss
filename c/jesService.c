

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#include "zowetypes.h"
#include "alloc.h"
#include "bpxnet.h"
#include "utils.h"
#include "socketmgmt.h"

#include "httpserver.h"
#include "dataservice.h"
#include "json.h"
#include "datasetjson.h"
#include "logging.h"
#include "zssLogging.h"

#include "datasetService.h"
#include "jesService.h"

#include "unixfile.h"
#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif 

#include "charsets.h"
#include "pdsutil.h"
#include "jcsi.h"
#include "impersonation.h"
#include "dynalloc.h"
#include "vsam.h"
#include "qsam.h"
#include "dsutils.h"

#pragma linkage(ZWESSI, OS)

#define INDEXED_DSCB 96
#define JCL_RECLEN 80
#define RPL_RET_OFFSET 60
#define JOB_ID_LEN 8
#define DDNAME_LEN 8
#define ERROR_BUF_LEN 256
#define MSG_BUF_LEN 132
#define MAXCC_8 8
#define MAXCC_12 12

#define JOB_ID_EMPTY "????????"
#define CHECK_BIT(var,pos) (((var)>>(pos)) & 1)

#define IS_DAMEMBER_EMPTY($member) \
  (!memcmp(&($member), &(DynallocMemberName){"        "}, sizeof($member)))

#ifndef NATIVE_CODEPAGE
#define NATIVE_CODEPAGE CCSID_EBCDIC_1047
#endif

#ifdef __ZOWE_OS_ZOS

static int serveJesRequests(HttpService *service, HttpResponse *response){
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);
  HttpRequest *request = response->request;

  if (!strcmp(request->method, methodPUT)) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG, "Submit Job/s from jcl content or dataset.\n");
    responseJesServiceWithSubmitJobs(response, TRUE);
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setContentType(response, "text/json");
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
    addStringHeader(response, "Allow", "PUT");
    writeHeader(response);

    jsonStart(out);
    jsonEnd(out);

    finishResponse(response);
  }
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "end %s\n", __FUNCTION__);
  return 0;
}

void installJesService(HttpServer *server) {
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO, ZSS_LOG_INSTALL_MSG, "jes requests");
  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG2, "begin %s\n", __FUNCTION__);

  HttpService *httpService = makeGeneratedService("jes", "/jes/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  httpService->serviceFunction = serveJesRequests;
  httpService->paramSpecList = makeStringParamSpec("force", SERVICE_ARG_OPTIONAL, NULL);
  registerHttpService(server, httpService);
}

static char* getJobID(RPLCommon *rpl){
  
  __asm(
#ifdef _LP64
      " SAM31                 \n"
      " SYSSTATE AMODE64=NO   \n"
#endif
      "  ENDREQ RPL=(%0) \n"
#ifdef _LP64
      " SAM64                 \n"
      " SYSSTATE AMODE64=YES  \n"
#endif
		  :
		  :"r"(rpl)
		  :);

  return (char *) rpl + RPL_RET_OFFSET;
}

static void responseWithJobDetails(HttpResponse* response, char* jobID, int jsonMode) {

  HttpRequest *request = response->request;

  jsonPrinter *jPrinter = respondWithJsonPrinter(response);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);

  writeHeader(response);

  jsonStart(jPrinter);

  jsonAddUnterminatedString(jPrinter, "jobId", jobID, strlen(jobID));

  jsonEnd(jPrinter);
  finishResponse(response);
}

static int validateJCLlengthIsCorrect(char *uJobLines, int numOfLines, int maxRecordLength){
  
  int recordLength;
  int lineCount=0;
  char *tJCL;
  tJCL = uJobLines;

  for (lineCount = 0; lineCount < numOfLines; lineCount++) {
    recordLength = strlen(tJCL);
    if (recordLength > maxRecordLength) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "Error JCL line details - (%d-%s-%d)\n", lineCount, tJCL, strlen(tJCL));
      return -1;
    }
    tJCL = tJCL + recordLength + 1;
  }
return 0;
}

static void responseWithSubmitJCLContents(HttpResponse* response, const char* jclLines) {
  
  int jobLineLength = strlen(jclLines) + 1;
  char *jobLines = safeMalloc(jobLineLength, "copyConstjclLines");
  strcpy(jobLines, jclLines);

  int jclLinecnt=0;
  int numLines=0;

  while(jobLines[jclLinecnt]!='\0')
    {
      if(jobLines[jclLinecnt]=='\n') {
        jobLines[jclLinecnt]='\0';
        numLines++;
        }
      jclLinecnt++;
    }

  int returnOutputValidJCL = 0;
  int maxRecordLength = 80;
  returnOutputValidJCL = validateJCLlengthIsCorrect(jobLines, numLines, maxRecordLength);

  if (returnOutputValidJCL != 0){
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Invalid jcl line length 80");
  }
  
  int macrfParms = 0;
  int rplParms   = 0;
  int rc = 0;
  char ddnameResult[DDNAME_LEN + 1] = "         ";
  char errorBuffer[ERROR_BUF_LEN + 1];
  unsigned int maxLrecl       = JCL_RECLEN;
  unsigned int maxBuffer      = maxLrecl;

  char *outACB = NULL;
  RPLCommon *rpl = NULL;

  rc = AllocIntReader("A", ddnameResult, errorBuffer);
    
  if (rc != 0) {
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "SubmitJCLContents - Allocate Internal Reader error\n");
    respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Allocate Internal Reader error");
  }

  outACB = openACB(ddnameResult, ACB_MODE_OUTPUT, macrfParms, 0, rplParms, maxLrecl, maxBuffer);
        
  if (outACB) {
    rpl = (RPLCommon *)(outACB+RPL_COMMON_OFFSET+8);
  }
  else {
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "SubmitJCLContents - ACB error ok\n");
    respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "ACB error");
  }

  /*passed record length check and type check*/
  int bytesWritten = 0;
  int recordsWritten = 0;
  int recordBuffersize = maxRecordLength + 1;
  char *recordBuffer = safeMalloc(recordBuffersize, "recordBufferSubmitContent");
  char *record = safeMalloc(recordBuffersize, "recordSubmitContent");
  char *jobID;
  
  record = jobLines;

  for (int jclLinecnt = 0; jclLinecnt < numLines; jclLinecnt++) {
    int recordLength = strlen(record);
    if (recordLength == 0) { 
      record = " ";
      recordLength = 1;
    }
    snprintf (recordBuffer, recordBuffersize, "%-*s", maxRecordLength, record);

    /* output to INTRDR */
    putRecord(outACB, recordBuffer, maxRecordLength);
    record = record + recordLength + 1;
  }
      
  jobID = getJobID(rpl);

  char uJobID[JOB_ID_LEN + 1];
  sscanf(jobID, "%s", uJobID);

  if (!strlen(uJobID)){
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "SubmitJCLContents - RPL error\n");
    respondWithMessage(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,
    "BAD_REQUEST failed with RC = %d, Submit input data does not start with a valid job line", MAXCC_12);
  }

  responseWithJobDetails(response, uJobID, TRUE);

  safeFree(recordBuffer,recordBuffersize);
  safeFree(record,recordBuffersize);

  closeACB(outACB, ACB_MODE_OUTPUT);

  safeFree(jobLines, jobLineLength);
}

static void respondWithSubmitDatasetInternal(HttpResponse* response,
                                       const char *datasetPath,
                                       const DatasetName *dsn,
                                       const DDName *ddName,
                                       int jsonMode) {

  HttpRequest *request = response->request;

  char ddPath[16];
  snprintf(ddPath, sizeof(ddPath), "DD:%8.8s", ddName->value);

  int lrecl = dsutilsGetLreclOr(dsn, ddPath);
  if (lrecl == -1) {
    respondWithError(response, HTTP_STATUS_NOT_FOUND, "File could not be opened or does not exist");
  }
  else if (lrecl == -2){
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Undefined-length dataset");
  }
  else if (lrecl == -3){
    respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,
                       "Could not read dataset information");
  }

  if (lrecl){
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Streaming data to INTRDR for %s\n", datasetPath);
    
    int macrfParms = 0;
    int rplParms   = 0;
    int rc = 0;
    int recordLength = lrecl;
    char ddnameResult[DDNAME_LEN + 1] = "         ";
    char errorBuffer[ERROR_BUF_LEN + 1];
    unsigned int maxLrecl       = JCL_RECLEN;
    unsigned int maxBuffer      = maxLrecl;

    char *outACB = NULL;
    RPLCommon *rpl = NULL;

    rc = AllocIntReader("A", ddnameResult, errorBuffer);

    if (rc != 0) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "SubmitDatasetInternal - Allocate Internal Reader error\n");
      respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Allocate Internal Reader error");
    }

    outACB = openACB(ddnameResult, ACB_MODE_OUTPUT, macrfParms, 0, rplParms, maxLrecl, maxBuffer);
        
    if (outACB) {
      rpl = (RPLCommon *)(outACB+RPL_COMMON_OFFSET+8);
    }
    else {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "SubmitDatasetInternal - ACB error\n");
      respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "ACB error");
    }

    int defaultSize = DATA_STREAM_BUFFER_SIZE;
    FILE *in;
    if (lrecl < 1){
      recordLength = defaultSize;
      in = fopen(ddPath,"rb");
    }
    else {
      in = fopen(ddPath,"rb, type=record");
    }

    int bufferSize = DATA_STREAM_BUFFER_SIZE + 1;
    char *buffer = safeMalloc(bufferSize, "bufferSubmitDataset");
    char *record = safeMalloc(bufferSize, "recordSubmitDataset");
    char *jobID;
    int bytesRead = 0;
    if (in) {
      while (!feof(in)){
        bytesRead = fread(buffer,1,recordLength,in);
        if (bytesRead > 0 && !ferror(in)) {
          memset(record, ' ', bytesRead);
          memcpy(record, buffer, bytesRead - 1);
          putRecord(outACB, record, bytesRead);
        } 
        else if (ferror(in)) {
          int inFileError = ferror(in);
          zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "Error reading DSN=%s, rc=%d, error no=%d\n", ddPath, bytesRead, inFileError);
          break;
        }
      }

      jobID = getJobID(rpl);

      char uJobID[JOB_ID_LEN + 1];
      sscanf(jobID, "%s", uJobID);

    
      if (!strlen(uJobID)){
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "SubmitDatasetInternal - RPL error\n");
        respondWithMessage(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,
        "BAD_REQUEST failed with RC = %d, Submit input data does not start with a valid job line", MAXCC_12);
      }
      responseWithJobDetails(response, uJobID, TRUE);
      safeFree(buffer,bufferSize);
      safeFree(record,bufferSize);
      closeACB(outACB, ACB_MODE_OUTPUT);
      fclose(in);

    }
    else {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Failed to open dataset\n");
    }
  }
}

static void respondDYNALLOCerror(HttpResponse *response,
                                     int rc, int sysRC, int sysRSN,
                                     const DynallocDatasetName *dsn,
                                     const DynallocMemberName *member,
                                     const char *site) {

  if (rc ==  RC_DYNALLOC_SVC99_FAILED && sysRC == 4) {

    if (sysRSN == 0x020C0000 || sysRSN == 0x02100000) {
      respondWithMessage(response, HTTP_STATUS_FORBIDDEN,
                        "Dataset \'%44.44s(%8.8s)\' busy (%s)",
                        dsn->name, member->name, site);
      return;
    }

    if (sysRSN == 0x02180000) {
      respondWithMessage(response, HTTP_STATUS_NOT_FOUND,
                        "Device not available for dataset \'%44.44s(%8.8s)\' "
                        "(%s)", dsn->name, member->name, site);
      return;
    }

    if (sysRSN == 0x023C0000) {
      respondWithMessage(response, HTTP_STATUS_NOT_FOUND,
                        "Catalog not available for dataset \'%44.44s(%8.8s)\' "
                        "(%s)", dsn->name, member->name, site);
      return;
    }

  }
  
  respondWithMessage(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,
                    "DYNALLOC failed with RC = %d, DYN RC = %d, RSN = 0x%08X, "
                    "dsn=\'%44.44s(%8.8s)\', (%s)", rc, sysRC, sysRSN,
                    dsn->name, member->name, site);
}

static void respondWithSubmitDataset(HttpResponse* response, char* absolutePath, int jsonMode) {

  HttpRequest *request = response->request;

  if (!dsutilsIsDatasetPathValid(absolutePath)) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Invalid dataset name");
    return;
  }

  DatasetName dsn;
  DatasetMemberName memberName;
  dsutilsExtractDatasetAndMemberName(absolutePath, &dsn, &memberName);

  DynallocDatasetName daDsn;
  DynallocMemberName daMember;
  memcpy(daDsn.name, dsn.value, sizeof(daDsn.name));
  memcpy(daMember.name, memberName.value, sizeof(daMember.name));
  DynallocDDName daDDname = {.name = JOB_ID_EMPTY};

  int daRC = RC_DYNALLOC_OK, daSysRC = 0, daSysRSN = 0;
  daRC = dynallocAllocDataset(
      &daDsn,
      IS_DAMEMBER_EMPTY(daMember) ? NULL : &daMember,
      &daDDname,
      DYNALLOC_DISP_SHR,
      DYNALLOC_ALLOC_FLAG_NO_CONVERSION | DYNALLOC_ALLOC_FLAG_NO_MOUNT,
      &daSysRC, &daSysRSN
  );

  if (daRC != RC_DYNALLOC_OK) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,
    		    "error: ds alloc dsn=\'%44.44s\', member=\'%8.8s\', dd=\'%8.8s\',"
            " rc=%d sysRC=%d, sysRSN=0x%08X (read)\n",
            daDsn.name, daMember.name, daDDname.name, daRC, daSysRC, daSysRSN);
    respondDYNALLOCerror(response, daRC, daSysRC, daSysRSN,
                             &daDsn, &daMember, "r");
  }
  
  DDName ddName;
  memcpy(&ddName.value, &daDDname.name, sizeof(ddName.value));

  respondWithSubmitDatasetInternal(response, absolutePath, &dsn, &ddName, jsonMode);

  daRC = dynallocUnallocDatasetByDDName(&daDDname, DYNALLOC_UNALLOC_FLAG_NONE,
                                        &daSysRC, &daSysRSN);
  if (daRC != RC_DYNALLOC_OK) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,
            "error: ds unalloc dsn=\'%44.44s\', member=\'%8.8s\', dd=\'%8.8s\',"
            " rc=%d sysRC=%d, sysRSN=0x%08X (read)\n",
            daDsn.name, daMember.name, daDDname.name, daRC, daSysRC, daSysRSN, "read");
  }
}

void responseJesServiceWithSubmitJobs(HttpResponse* response, int jsonMode) {

  if (jsonMode != TRUE) { /*TODO add support for updating files with raw bytes instead of JSON*/
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Cannot update file without JSON formatted record request");
    return;
  }

  HttpRequest *request = response->request;
  HttpRequestParam *forceParam = getCheckedParam(request,"force");
  char *forceArg = (forceParam ? forceParam->stringValue : NULL);
  bool force = (forceArg != NULL && !strcmp(forceArg,"true"));
  
  /*FileInfo info;*/
  int returnCode;
  int reasonCode;
  char fileName[49];

  char *contentBody = response->request->contentBody;
  int bodyLength = strlen(contentBody);

  if (bodyLength){
    
    int totalBodyLength = bodyLength*4;
    char *convertedBody = safeMalloc(totalBodyLength, "writeDatasetConvert");
    int conversionBufferLength = totalBodyLength;
    int translationLength;
    int outCCSID = NATIVE_CODEPAGE;

    returnCode = convertCharset(contentBody,
                                bodyLength,
                                CCSID_UTF_8,
                                CHARSET_OUTPUT_USE_BUFFER,
                                &convertedBody,
                                conversionBufferLength,
                                outCCSID,
                                NULL,
                                &translationLength,
                                &reasonCode);

    if(returnCode == 0) {  
      int blockSize = 0x10000;
      int maxBlockCount = (translationLength*2)/blockSize;
      if (!maxBlockCount){
        maxBlockCount = 0x10;
      }
      ShortLivedHeap *slh = makeShortLivedHeap(blockSize, maxBlockCount);
      char errorBuffer[2048];
      zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_DEBUG, "Submit Job: before JSON parse dataLength=%d\n", bodyLength);
      Json *json = jsonParseUnterminatedString(slh, 
                                               convertedBody, translationLength,
                                               errorBuffer, sizeof(errorBuffer));
      if (json) {
        if (jsonIsObject(json)) {
          JsonObject *jsonObject = jsonAsObject(json);
          char *fileValue = jsonObjectGetString(jsonObject, "file");
    
          if (fileValue) {
            snprintf(fileName, sizeof(fileName), "//'%s'", fileValue);
            zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "fileValue...(%s)\n", fileValue);
            zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "fileName...(%s)\n", fileName);
            respondWithSubmitDataset(response, fileName, TRUE);
          }
          else {
            char *jclValue = jsonObjectGetString(jsonObject, "jcl");
            if (jclValue) {
              int cmpResult = strncmp(jclValue, "//", 2);
              if (cmpResult != 0) {
                respondWithMessage(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, 
                "BAD_REQUEST failed with RC = %d, Submit input data does not start with a slash", MAXCC_8);
              }
              else {
                responseWithSubmitJCLContents(response, jclValue);
              }
            }
            else {
              zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "No jcl Value\n");
              respondWithError(response, HTTP_STATUS_BAD_REQUEST, "POST body No jcl value");   
            }   
          }
        } else {
          zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_DEBUG, "*** INTERNAL ERROR *** message is JSON, but not an object\n");
        }
      }
      else {
        zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_DEBUG, "UPDATE the content, body was not JSON!\n");
        respondWithError(response, HTTP_STATUS_BAD_REQUEST, "POST body could not be parsed as JSON format");      
      }
      SLHFree(slh);
    }
    else {
      respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Could not translate character set");    
    }
    safeFree(convertedBody,conversionBufferLength);
  }
  else {
    zowelog(NULL, LOG_COMP_RESTFILE, ZOWE_LOG_DEBUG, "Check the content body\n");
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Content body is empty"); 
  }
}

#endif /* not __ZOWE_OS_ZOS */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
