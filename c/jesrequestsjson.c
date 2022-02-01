

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
/* HAS NOT BEEN COMPILED WITH METTLE BEFORE */
#else
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
/* #include <sys/stat.h> */
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "json.h"
#include "bpxnet.h"
#include "logging.h"
#include "unixfile.h"
#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif 

#include "charsets.h"

#include "socketmgmt.h"
#include "httpserver.h"
#include "datasetjson.h"
#include "pdsutil.h"
#include "jcsi.h"
#include "impersonation.h"
#include "dynalloc.h"
#include "utils.h"
#include "vsam.h"
#include "qsam.h"
#include "dsutils.h"
#include "jesrequestsjson.h"

#pragma linkage(ZWESSI, OS)

#define INDEXED_DSCB 96
#define JCL_RECLEN 80
#define RPL_RET_OFFSET 60
#define JOB_ID_LEN 8
#define DDNAME_LEN 8
#define ERROR_BUF_LEN 256
#define MSG_BUF_LEN 132

#define JOB_ID_EMPTY "????????"
#define CHECK_BIT(var,pos) (((var)>>(pos)) & 1)

#define IS_DAMEMBER_EMPTY($member) \
  (!memcmp(&($member), &(DynallocMemberName){"        "}, sizeof($member)))

#ifndef NATIVE_CODEPAGE
#define NATIVE_CODEPAGE CCSID_EBCDIC_1047
#endif




void responseWithJobDetails(HttpResponse* response, char* jobID, int jsonMode) {
#ifdef __ZOWE_OS_ZOS

  HttpRequest *request = response->request;

  jsonPrinter *jPrinter = respondWithJsonPrinter(response);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);

  writeHeader(response);

  jsonStart(jPrinter);

  jsonAddUnterminatedString(jPrinter, "jobId", jobID, 8);

  jsonEnd(jPrinter);
  finishResponse(response);

#else /* not __ZOWE_OS_ZOS */

  /* Currently nothing else has "datasets" */
  /* TBD: Is it really necessary to provide this empty array?
     It seems like the safest approach, not knowing anyting about the client..
   */

#endif /* __ZOWE_OS_ZOS */
}

static int getJobID(HttpResponse* response, RPLCommon *rpl){
#ifdef __ZOWE_OS_ZOS
	char *jobID;

  __asm(
		  "  ENDREQ RPL=(%0) \n"
		  :
		  :"r"(rpl)
		  :);
  jobID = (char *) rpl + RPL_RET_OFFSET;

  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "jobID_Length (%d)\n", strlen(jobID));
  if (!strlen(jobID)) {
    respondWithMessage(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,
      "BAD_REQUEST failed with RC = %d, Submit input data does not start with a valid job line", 12);
  }
  else {
	responseWithJobDetails(response, jobID, TRUE);
  }

  return 0;
#endif /* __ZOWE_OS_ZOS */
}


void responseWithSubmitJCLContents(HttpResponse* response, char* jclLines) {
#ifdef __ZOWE_OS_ZOS

  int i=0;
  int j=0;

  while(jclLines[i]!='\0')
    {
      if(jclLines[i]=='\n') {
        jclLines[i]='\0';
        j++;
        }
      i++;
    }

  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "number of lines (%d-%s-%d)\n", j, jclLines, strlen(jclLines));

  int macrfParms = 0;
  int rplParms   = 0;
  int rc = 0;
  char ddnameResult[DDNAME_LEN + 1] = "         ";
  char errorBuffer[ERROR_BUF_LEN + 1];
  unsigned int maxlrecl       = JCL_RECLEN;
  unsigned int maxbuffer      = maxlrecl;

  char *outACB = NULL;
  RPLCommon *rpl = NULL;

  rc = AllocIntReader("A", ddnameResult, errorBuffer);

  outACB = openACB(ddnameResult, ACB_MODE_OUTPUT, macrfParms, 0, rplParms, maxlrecl, maxbuffer);
      
  if (outACB) {
    rpl = (RPLCommon *)(outACB+RPL_COMMON_OFFSET+8);
    if (rpl) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "rpl ok\n");
      }
    else {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "rpl error\n");
      }
    }
  else {
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "ACB error ok\n");
  }

  int maxRecordLength = 80;
  char *jobID;
  char *tJCL;
  int recordLength;
  tJCL = jclLines;
  for ( i = 0; i < j; i++) {
    recordLength = strlen(tJCL);
    if (recordLength > maxRecordLength) {
      respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Invalid jcl line length 80");
      return;
      recordLength = maxRecordLength;
      }
      tJCL = tJCL + recordLength + 1;
    }

  /*passed record length check and type check*/
  int bytesWritten = 0;
  int recordsWritten = 0;
  char recordBuffer[maxRecordLength+1];
  
  char *record = jclLines;

  for (int i = 0; i < j; i++) {
    int recordLength = strlen(record);
    if (recordLength == 0) { //this is a hack, which will be removed as we move away from fwrite
      record = " ";
      recordLength = 1;
    }
    snprintf (recordBuffer, sizeof(recordBuffer), "%-*s", maxRecordLength, record);

    /* output to INTRDR */
    putRecord(outACB, recordBuffer, maxRecordLength);
    record = record + recordLength + 1;
    }

  rc = getJobID(response, rpl);
  closeACB(outACB, ACB_MODE_OUTPUT);

#endif /* __ZOWE_OS_ZOS */

}

void responseWithSubmitJobs(HttpResponse* response, int jsonMode) {
#ifdef __ZOWE_OS_ZOS

  if (jsonMode != TRUE) { /*TODO add support for updating files with raw bytes instead of JSON*/
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Cannot update file without JSON formatted record request");
    return;
  }

  HttpRequest *request = response->request;
  HttpRequestParam *forceParam = getCheckedParam(request,"force");
  char *forceArg = (forceParam ? forceParam->stringValue : NULL);
  bool force = (forceArg != NULL && !strcmp(forceArg,"true"));
  
  FileInfo info;
  int returnCode;
  int reasonCode;
  char fileName[49];

  char *contentBody = response->request->contentBody;
  int bodyLength = strlen(contentBody);

  char *convertedBody = safeMalloc(bodyLength*4,"writeDatasetConvert");
  int conversionBufferLength = bodyLength*4;
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
    ShortLivedHeap *slh = makeShortLivedHeap(blockSize,maxBlockCount);
    char errorBuffer[2048];
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Submit Job: before JSON parse dataLength=0x%x\n",bodyLength);
    Json *json = jsonParseUnterminatedString(slh, 
                                             convertedBody, translationLength,
                                             errorBuffer, sizeof(errorBuffer));
    if (json) {
      if (jsonIsObject(json)) {
        JsonObject *jsonObject = jsonAsObject(json);
        char *fileValue = jsonObjectGetString(jsonObject,"file");
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "fileValue...(%s)\n", fileValue);
        if (fileValue) {
          snprintf(fileName, sizeof(fileName), "//'%s'", fileValue);
          zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "filename...(%s)\n", fileName);
          respondWithSubmitDataset(response, fileName, TRUE);
        }
        else {
          char *jclValue = jsonObjectGetString(jsonObject,"jcl");
          if (jclValue) {
            if (jclValue[0] != '/' | jclValue[1] != '/') {
              respondWithMessage(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,
                  "BAD_REQUEST failed with RC = %d, Submit input data does not start with a slash", 8);
            }
            else {
              responseWithSubmitJCLContents(response, jclValue);
            }
          }
          else {
            zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "jclValue...(%s)\n", jclValue);
            zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "No jcl Value...(%s)\n", jclValue);
            respondWithError(response, HTTP_STATUS_BAD_REQUEST,"POST body No jcl value");   
          }   

        }
        /* updateDatasetWithJSON(response, jsonObject, absolutePath, etag, force); */
      } else {
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "*** INTERNAL ERROR *** message is JSON, but not an object\n");
      }
    }
    else {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "UPDATE DATASET: body was not JSON!\n");
      respondWithError(response, HTTP_STATUS_BAD_REQUEST,"POST body could not be parsed as JSON format");      
    }
    SLHFree(slh);
  }
  else {
    respondWithError(response,HTTP_STATUS_INTERNAL_SERVER_ERROR,convertedBody);    
  }
  safeFree(convertedBody,conversionBufferLength);

#endif /* __ZOWE_OS_ZOS */
}


static void respondWithSubmitDatasetInternal(HttpResponse* response,
                                       const char *datasetPath,
                                       const DatasetName *dsn,
                                       const DDName *ddName,
                                       int jsonMode) {
#ifdef __ZOWE_OS_ZOS
  HttpRequest *request = response->request;

  char ddPath[16];
  snprintf(ddPath, sizeof(ddPath), "DD:%8.8s", ddName->value);

  int lrecl = getLreclOrRespondError(response, dsn, ddPath);
  if (!lrecl) {
    return;
  }

  if (lrecl){
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Streaming data to INTRDR for %s\n", datasetPath);
    
    /* jsonStart(jPrinter); */
    int status = streamDatasetToINTRDR(response, ddPath, lrecl, TRUE);
    /* jsonEnd(jPrinter); */
  }

#endif /* __ZOWE_OS_ZOS */
}

void respondWithSubmitDataset(HttpResponse* response, char* absolutePath, int jsonMode) {
#ifdef __ZOWE_OS_ZOS

  HttpRequest *request = response->request;

  if (!isDatasetPathValid(absolutePath)) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Invalid dataset name");
    return;
  }

  DatasetName dsn;
  DatasetMemberName memberName;
  extractDatasetAndMemberName(absolutePath, &dsn, &memberName);

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
    respondWithDYNALLOCError(response, daRC, daSysRC, daSysRSN,
                             &daDsn, &daMember, "r");
    return;
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
#endif /* __ZOWE_OS_ZOS */
}

int streamDatasetToINTRDR(HttpResponse* response, char *filename, int recordLength, int jsonMode){
#ifdef __ZOWE_OS_ZOS
  // Note: to allow processing of zero-length records set _EDC_ZERO_RECLEN=Y

  int macrfParms = 0;
  int rplParms   = 0;
  int rc = 0;
  char ddnameResult[DDNAME_LEN + 1] = "         ";
  char errorBuffer[ERROR_BUF_LEN + 1];
  unsigned int maxlrecl       = JCL_RECLEN;
  unsigned int maxbuffer      = maxlrecl;

  char *outACB = NULL;
  RPLCommon *rpl = NULL;

  rc = AllocIntReader("A", ddnameResult, errorBuffer);

  outACB = openACB(ddnameResult, ACB_MODE_OUTPUT, macrfParms, 0, rplParms, maxlrecl, maxbuffer);
      
  if (outACB) {
    rpl = (RPLCommon *)(outACB+RPL_COMMON_OFFSET+8);
    if (rpl) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "rpl ok\n");
      }
    else {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "rpl error\n");
      }
    }
  else {
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "ACB error ok\n");
  }

  int defaultSize = DATA_STREAM_BUFFER_SIZE;
  FILE *in;
  if (recordLength < 1){
    recordLength = defaultSize;
    in = fopen(filename,"rb");
  }
  else {
    in = fopen(filename,"rb, type=record");
  }

  int bufferSize = recordLength+1;
  char buffer[bufferSize];
  char record[bufferSize];
  char jobID[9];
  int contentLength = 0;
  int bytesRead = 0;
  if (in) {
    while (!feof(in)){
      bytesRead = fread(buffer,1,recordLength,in);
      if (bytesRead > 0 && !ferror(in)) {
        memset(record, ' ', bytesRead);
        memcpy(record, buffer, bytesRead - 1);
        putRecord(outACB, record, bytesRead);
        contentLength = contentLength + bytesRead;
      } else if (bytesRead == 0 && !feof(in) && !ferror(in)) {
        // empty record
        /* jsonAddString(jPrinter, NULL, ""); */
      } else if (ferror(in)) {
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "Error reading DSN=%s, rc=%d\n", filename, bytesRead);
        break;
      }
    }

    rc = getJobID(response, rpl);
    closeACB(outACB, ACB_MODE_OUTPUT);
    fclose(in);

  }
  else {
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "FAILED TO OPEN FILE\n");
  }

#else /* not __ZOWE_OS_ZOS */

  /* Currently nothing else has "datasets" */
  /* TBD: Is it really necessary to provide this empty array?
     It seems like the safest approach, not knowing anyting about the client..
   */

  int contentLength = 0;
#endif /* not __ZOWE_OS_ZOS */
  return contentLength;
}

#endif /* not METTLE - the whole module */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

