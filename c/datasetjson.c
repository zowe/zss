

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
#include <sys/stat.h>
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "json.h"
#include "bpxnet.h"
#include "logging.h"
#include "unixfile.h"
#include "errno.h"
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
#include "icsf.h"

#define INDEXED_DSCB      96
#define LEN_THREE_BYTES   3
#define LEN_ONE_BYTE      1
#define VOLSER_SIZE       6
#define CLASS_WRITER_SIZE 8
#define TOTAL_TEXT_UNITS  23

#define ERROR_DECODING_DATASET         -2
#define ERROR_CLOSING_DATASET          -3
#define ERROR_ALLOCATING_DATASET       -4
#define ERROR_DEALLOCATING_DATASET     -5
#define ERROR_UNDEFINED_LENGTH_DATASET -6
#define ERROR_BAD_DATASET_NAME         -7
#define ERROR_INCORRECT_DATASET_TYPE   -8
#define ERROR_DATASET_NOT_EXIST        -9
#define ERROR_MEMBER_ALREADY_EXISTS    -10
#define ERROR_INVALID_JSON_BODY        -11
#define ERROR_COPY_NOT_SUPPORTED       -12
#define ERROR_DATASET_ALREADY_EXIST    -13

static char defaultDatasetTypesAllowed[3] = {'A','D','X'};
static char clusterTypesAllowed[3] = {'C','D','I'}; /* TODO: support 'I' type DSNs */
static int clusterTypesCount = 3;
static char *datasetStart = "//'";
static char *defaultCSIFields[] ={ "NAME    ", "TYPE    ", "VOLSER  "};
static int defaultCSIFieldCount = 3;
static char *defaultVSAMCSIFields[] ={"AMDCIREC", "AMDKEY  ", "ASSOC   ", "VSAMTYPE"};
static int defaultVSAMCSIFieldCount = 4;
static char vsamCSITypes[5] = {'R', 'D', 'G', 'I', 'C'};

static char getRecordLengthType(char *dscb);
static int getMaxRecordLength(char *dscb);

const static int DSCB_TRACE = FALSE;

typedef struct DatasetName_tag {
  char value[44]; /* space-padded */
} DatasetName;

typedef struct DatasetMemberName_tag {
  char value[8]; /* space-padded */
} DatasetMemberName;

typedef struct DDName_tag {
  char value[8]; /* space-padded */
} DDName;

typedef struct Volser_tag {
  char value[6]; /* space-padded */
} Volser;

static int getVolserForDataset(const DatasetName *dataset, Volser *volser);
static bool memberExists(char* dsName, DynallocMemberName daMemberName);
static int getDSCB(DatasetName *dsName, char* dscb, int bufferSize);
static int setDatasetAttributesForCreation(JsonObject *object, int *configsCount, TextUnit **inputTextUnit);
int createDataset(HttpResponse* response, char* absolutePath, char* datasetAttributes, int translationLength, int* reasonCode);

static int getLreclOrRespondError(HttpResponse *response, const DatasetName *dsn, const char *ddPath) {
  int lrecl = 0;

  FileInfo info;
  int returnCode;
  int reasonCode;
  FILE *in = fopen(ddPath, "r");
  if (in == NULL) {
    respondWithError(response,HTTP_STATUS_NOT_FOUND,"File could not be opened or does not exist");
    return 0;
  }

  Volser volser;
  memset(&volser.value, ' ', sizeof(volser.value));

  int volserSuccess = getVolserForDataset(dsn, &volser);
  int handledThroughDSCB = FALSE;

  if (!volserSuccess){
    
    char dscb[INDEXED_DSCB] = {0};
    int rc = obtainDSCB1(dsn->value, sizeof(dsn->value),
                         volser.value, sizeof(volser.value),
                         dscb);
    if (rc == 0){
      if (DSCB_TRACE){
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "DSCB for %.*s found\n", sizeof(dsn->value), dsn->value);
        dumpbuffer(dscb,INDEXED_DSCB);
      }

      lrecl = getMaxRecordLength(dscb);
      char recordType = getRecordLengthType(dscb);
      if (recordType == 'U'){
        fclose(in);
        respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Undefined-length dataset");
        return 0;
      }
      handledThroughDSCB = TRUE;
    }
  }
  if (!handledThroughDSCB){
    FileInfo info;
    fldata_t fileinfo = {0};
    char filenameOutput[100];
    int returnCode = fldata(in,filenameOutput,&fileinfo);
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "FLData request rc=0x%x\n",returnCode);
    if (!returnCode) {
      if (fileinfo.__recfmU) {
        fclose(in);
        respondWithError(response,  HTTP_STATUS_BAD_REQUEST,
                         "Undefined-length dataset");
        return 0;
      }
      lrecl = fileinfo.__maxreclen;
    } else {
      fclose(in);
      respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,
                       "Could not read dataset information");
      return 0;
    }
  }
  fclose(in);

  return lrecl;
}

/*
 TODO this duplicates a lot of stremDataset. Thinking of putting conditionals on streamDataset writing to json stream, but then function becomes misleading.
 */
static char *getDatasetETag(char *filename, int recordLength, int *rc, int *eTagReturnLength) {
  
#ifdef __ZOWE_OS_ZOS
  int rcEtag = 0;
  int eTagLength = 0;
    
  // Note: to allow processing of zero-length records set _EDC_ZERO_RECLEN=Y
  int defaultSize = DATA_STREAM_BUFFER_SIZE;
  FILE *in;
  if (recordLength < 1){
    recordLength = defaultSize;
    in = fopen(filename,"rb");
  }
  else {
    in = fopen(filename,"rb, type=record");
  }

  ICSFDigest digest;
  char hash[32];

  int bufferSize = recordLength+1;
  char buffer[bufferSize];
  int contentLength = 0;
  int bytesRead = 0;
  if (in) {
    rcEtag = icsfDigestInit(&digest, ICSF_DIGEST_SHA1);
    if (rcEtag) { //if etag generation has an error, just don't send it.
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_WARNING,  "ICSF error for SHA etag init, %d\n",rcEtag);
    } else {
      while (!feof(in)){
        bytesRead = fread(buffer,1,recordLength,in);
        if (bytesRead > 0 && !ferror(in)) {
          rcEtag = icsfDigestUpdate(&digest, buffer, bytesRead);
          contentLength = contentLength + bytesRead;
        } else if (bytesRead == 0 && !feof(in) && !ferror(in)) {
          // empty record
        } else if (ferror(in)) {
          zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "Error reading DSN=%s, rc=%d\n", filename, bytesRead);
          break;
        }
      }
    }
    fclose(in);
    if (!rcEtag) { rcEtag = icsfDigestFinish(&digest, hash); }
    if (rcEtag) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_WARNING,  "ICSF error for SHA etag, %d\n",rcEtag);
    }
  }
  else {
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "FAILED TO OPEN FILE\n");
  }

  *rc = rcEtag;

  if (!rcEtag) {
    // Convert hash text to hex.
    eTagLength = digest.hashLength*2;
    *eTagReturnLength = eTagLength;
    char *eTag = safeMalloc(eTagLength+1, "datasetetag");
    memset(eTag, '\0', eTagLength+1);
    simpleHexPrint(eTag, hash, digest.hashLength);
    return eTag;
  }

#else /* not __ZOWE_OS_ZOS */

  /* Currently nothing else has "datasets" */
  /* TBD: Is it really necessary to provide this empty array?
     It seems like the safest approach, not knowing anyting about the client..
   */

#endif /* not __ZOWE_OS_ZOS */
  return NULL;
}

int streamDataset(char *filename, int recordLength, jsonPrinter *jPrinter){
#ifdef __ZOWE_OS_ZOS
  // Note: to allow processing of zero-length records set _EDC_ZERO_RECLEN=Y
  int defaultSize = DATA_STREAM_BUFFER_SIZE;
  FILE *in;
  if (recordLength < 1){
    recordLength = defaultSize;
    in = fopen(filename,"rb");
  }
  else {
    in = fopen(filename,"rb, type=record");
  }

  int rcEtag;
  ICSFDigest digest;
  char hash[32];

  int bufferSize = recordLength+1;
  char buffer[bufferSize];
  jsonStartArray(jPrinter,"records");
  int contentLength = 0;
  int bytesRead = 0;
  if (in) {
    rcEtag = icsfDigestInit(&digest, ICSF_DIGEST_SHA1);
    if (rcEtag) { //if etag generation has an error, just don't send it.
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_WARNING,  "ICSF error for SHA etag init, %d\n",rcEtag);
    }
    while (!feof(in)){
      bytesRead = fread(buffer,1,recordLength,in);
      if (bytesRead > 0 && !ferror(in)) {
        if (!rcEtag) { rcEtag = icsfDigestUpdate(&digest, buffer, bytesRead); }
        jsonAddUnterminatedString(jPrinter, NULL, buffer, bytesRead);
        contentLength = contentLength + bytesRead;
      } else if (bytesRead == 0 && !feof(in) && !ferror(in)) {
        // empty record
        jsonAddString(jPrinter, NULL, "");
      } else if (ferror(in)) {
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "Error reading DSN=%s, rc=%d\n", filename, bytesRead);
        break;
      }
    }
    fclose(in);
    if (!rcEtag) { rcEtag = icsfDigestFinish(&digest, hash); }
  }
  else {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "FAILED TO OPEN FILE\n");
  }

  jsonEndArray(jPrinter);

  if (!rcEtag) {
    // Convert hash text to hex.
    int eTagLength = digest.hashLength*2;
    char eTag[eTagLength+1];
    memset(eTag, '\0', eTagLength+1);
    simpleHexPrint(eTag, hash, digest.hashLength);
    jsonAddString(jPrinter, "etag", eTag);
  }

#else /* not __ZOWE_OS_ZOS */

  /* Currently nothing else has "datasets" */
  /* TBD: Is it really necessary to provide this empty array?
     It seems like the safest approach, not knowing anyting about the client..
   */
  jsonStartArray(jPrinter,"records");
  jsonEndArray(jPrinter);
  int contentLength = 0;
#endif /* not __ZOWE_OS_ZOS */
  return contentLength;
}

int streamVSAMDataset(HttpResponse* response, char *acb, int maxRecordLength, int maxRecords, int maxBytes,
                      int keyLoc, int keyLen, jsonPrinter *jPrinter) {
#ifdef __ZOWE_OS_ZOS
  int defaultSize = DATA_STREAM_BUFFER_SIZE;

  int returnCode = 0;
  int reasonCode = 0;
  RPLCommon *rpl = NULL;

  if (acb) {
    rpl = (RPLCommon *)(acb+RPL_COMMON_OFFSET+8);
  } else {
    /* TODO: should never happen, but regardless, close out the JSON before this error so it doesn't look weird? */
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "We were not passed an ACB.\n");
    /* respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,"Missing ACB"); should really return this */
    return 8;
  }
  int bufferSize = rpl->bufLen + 1;
  int keyBufferSize = rpl->keyLen ? rpl->keyLen + 1 : 4;
  char *buffer = safeMalloc(bufferSize, "VSAM buffer");
  char *keyBuffer = safeMalloc(keyBufferSize, "VSAM key buffer");
  memset(buffer,0,bufferSize);
  memset(keyBuffer,0,keyBufferSize);
  jsonStartArray(jPrinter,"records");

  int contentLength = 0;
  int encodedLength = 0;
  int encodedKeyLength = 0;
  char *encodedRecord = NULL;
  char *encodedKey = NULL;
  int bytesRead = 0;
  int rbaFound = 0;
  int status = 0;

  /* TODO: limit the bytes or records returned if those values are set */
  while (!(rpl->status)) {
    status = getRecord(acb, buffer, &bytesRead);
    /* TODO: if the user enters a parm to skip the first record, we can call continue in this loop after getRecord, but before JSON gets sent */
    if (bytesRead > bufferSize) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "CRITICAL ERROR: Catalog was wrong about maximum record size.\n");
      jsonAddString(jPrinter, NULL, "_ERROR: Catalog Error. Record found was too large.");
      jsonEndArray(jPrinter);
    }
    memset(buffer+bytesRead,0,bufferSize-bytesRead);
    if (rpl->keyLen) {
      memcpy(keyBuffer, buffer+keyLoc, keyLen);
      keyBuffer[keyLen] = 0;
    } else {
      rbaFound = *(int *)((rpl->rba)+4);
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "rbaFound = %d\n", rbaFound);
      memcpy(keyBuffer, &rbaFound, 4);
    }
    if (!bytesRead && rpl->status) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Read 0 bytes with error: ACB=%08x, rc=%d, rplRC = %0x%06x\n", acb, status, rpl->status, rpl->feedback);
      break;
    } else if (rpl->status && (rpl->feedback & 0xFF) == 0x04) { /* TODO: extra eof info to pass into json? */
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "eof found after RBA %d\n", rbaFound);
      /* TODO: I suggest we remove our hashtable entry, close the ACB, and free their relative memories here - but confirm this with team first. */
      break;
    } else {
      contentLength = contentLength + bytesRead;
      if (rpl->keyLen) {
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "found (\"%s\",\"%s\") Total bytes = %d.\n", keyBuffer, buffer, contentLength);
      } else{
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "found (0x%08x,\"%s\") Total bytes = %d.\n", *(int *)keyBuffer, buffer, contentLength);
      }
      encodedRecord = encodeBase64(NULL, buffer, bytesRead, &encodedLength, 1);
      encodedKey = encodeBase64(NULL, keyBuffer, keyLen ? keyLen : 4, &encodedKeyLength, 1);
      jsonStartObject(jPrinter, NULL);
      jsonAddString(jPrinter, "key", encodedKey);
      jsonAddString(jPrinter, "record", encodedRecord);
      jsonEndObject(jPrinter);
      safeFree(encodedRecord, encodedLength);
      safeFree(encodedKey, encodedKeyLength);
    }
    if (rpl->status) zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "New Error: ACB=%08x, rc=%d, rplRC = %02x%06x\n", acb, status, rpl->status, rpl->feedback);
  }

  jsonEndArray(jPrinter);
  safeFree(buffer, bufferSize);
  safeFree(keyBuffer, keyBufferSize);

#else /* not __ZOWE_OS_ZOS */

  /* Currently nothing else has VSAM */
  /* TBD: Is it really necessary to provide this empty array?
     It seems like the safest approach, not knowing anything about the client..
   */
  jsonStartArray(jPrinter,"records");
  jsonEndArray(jPrinter);
  int contentLength = 0;
#endif /* not __ZOWE_OS_ZOS */
  return contentLength;
}


static void addDetailsFromDSCB(char *dscb, jsonPrinter *jPrinter, int *isPDS) {
#ifdef __ZOWE_OS_ZOS
    int posOffset = 44;

    int recfm = dscb[84-posOffset];
    
    jsonStartObject(jPrinter,"recfm");
    {
      char recLen;
      if ((recfm & 0xC0) == 0xC0){/*undefined*/
        jsonAddString(jPrinter,"recordLength","U");
        recLen = 'U';
      }
      else if (recfm & 0x80){/*fixed*/
        jsonAddString(jPrinter,"recordLength","F");
        recLen = 'F';
      }
      else if (recfm & 0x40){/*variable*/
        jsonAddString(jPrinter,"recordLength","V");
        recLen = 'V';
      }
      
      if (recfm & 0x10){/*blocked*/
        jsonAddBoolean(jPrinter,"isBlocked",TRUE);
      }
      if (recfm & 0x0f){/*standard (or spanned?!)*/
        if (recLen == 'V'){
          jsonAddBoolean(jPrinter,"isSpanned",TRUE);
        }
        else if (recLen == 'F'){
          jsonAddBoolean(jPrinter,"isStandard",TRUE);
        }
      }
      
      if (recfm & 0x04){
        jsonAddString(jPrinter,"carriageControl","ansi");
      }
      else if (recfm & 0x02){
        jsonAddString(jPrinter,"carriageControl","machine");
      }
      else{
        jsonAddString(jPrinter,"carriageControl","unknown");
      }
    }
    jsonEndObject(jPrinter);


    int dsorgHigh = dscb[82-posOffset];
    int isUnmovable = FALSE;

    jsonStartObject(jPrinter,"dsorg");
    {
      if (dsorgHigh & 0x40){/*Physical Sequential / PS*/
        jsonAddString(jPrinter,"organization","sequential");
      }
      else if (dsorgHigh & 0x20){/*Direct Organization / DA*/
  
      }
      else if (dsorgHigh & 0x10){/*BTAM or QTAM / CX*/
  
      }
      else if (dsorgHigh & 0x02){ /*Partitioned / PO*/
        int pdsCheck = dscb[78-posOffset] & 0x0a;
        if (pdsCheck == 0x0a){/*pdse & pdsex = hfs*/
          jsonAddString(jPrinter,"organization","hfs");
        }
        else{
          *isPDS = TRUE;
          jsonAddString(jPrinter,"organization","partitioned");
          jsonAddBoolean(jPrinter,"isPDSDir",TRUE);
          if (pdsCheck == 0x08){ /*only pdse = pdse*/
            jsonAddBoolean(jPrinter,"isPDSE",TRUE);
          }
          else{
          /*is pds*/
          }
        }
      }
      if (dsorgHigh & 0x01){
        isUnmovable = TRUE;
      }
      
      int dsorgLow = dscb[83-posOffset];
  
      if (dsorgLow & 0x80){/*Graphics / GS*/
  
      }
      else if (dsorgLow & 0x08){/*VSAM*/
        jsonAddString(jPrinter,"organization","vsam");
        int keylen = dscb[90-posOffset] << 12;
        int keyoffset = dscb[91-posOffset] << 8;
        jsonAddInt(jPrinter,"keylen",keylen);
        jsonAddInt(jPrinter,"keyoffset",keyoffset);
      }
      
      int lrecl = (dscb[88-posOffset] << 8) | dscb[89-posOffset];
      jsonAddInt(jPrinter,"maxRecordLen",lrecl);
      
      int blockSize = (dscb[86-posOffset] << 8 | dscb[87-posOffset]);
      jsonAddInt(jPrinter,"totalBlockSize",blockSize);  
    }
    jsonEndObject(jPrinter);
#endif /* __ZOWE_OS_ZOS */
}

#ifdef __ZOWE_OS_ZOS
/* https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.1.0/com.ibm.zos.v2r1.idas300/s3013.htm */
static int isPartionedDataset(char *dscb) {
  int posOffset = 44;
  int dsorgHigh = dscb[82-posOffset];

  if (dsorgHigh & 0x02) {
    return TRUE;
  }

  return FALSE;
}
#endif

static bool isSupportedWriteDsorg(char *dscb, bool *isPds) {
  int posOffset = 44;
  int dsorgHigh = dscb[82-posOffset];

  if (dsorgHigh & 0x40){/*Physical Sequential / PS*/
    return true;
  }
  else if (dsorgHigh & 0x20){/*Direct Organization / DA*/
    return false;
  }
  else if (dsorgHigh & 0x10){/*BTAM or QTAM / CX*/
    return false;
  }
  else if (dsorgHigh & 0x02){ /*Partitioned / PO*/
    int pdsCheck = dscb[78-posOffset] & 0x0a;
    if (pdsCheck == 0x0a){/*pdse & pdsex = hfs*/
      return false;
    }
    else{
      *isPds = true;
      return true;
    }
  }
  if (dsorgHigh & 0x01){//"unmovable"
    return false;
  }
      
  int dsorgLow = dscb[83-posOffset];
    
  if (dsorgLow & 0x80){/*Graphics / GS*/
    return false;
  }
  else if (dsorgLow & 0x08){/*VSAM*/
    return false;
  }
}

static int obtainDSCB1(const char *dsname, unsigned int dsnameLength,
                       const char *volser, unsigned int volserLength,
                       char *dscb1) {

#define DSCB1_SIZE                    96

#define SVC27_WORKAREA_SIZE           140
#define SVC27_OPCODE_SEARCH           0xC100
#define SVC27_OPCODE_SEEK             0xC080

#define SVC27_OPTION_NO_TIOT_ENQ      0x80
#define SVC27_OPTION_NO_DUMMY_DSCB1   0x40
#define SVC27_OPTION_NO_CAT_ALLOC     0x20
#define SVC27_OPTION_HIDE_NAME        0x10
#define SVC27_OPTION_EADSCB_OK        0x08

  ALLOC_STRUCT31(
    STRUCT31_NAME(mem31),
    STRUCT31_FIELDS(
      char dsnameSpacePadded[44];
      char volserSpacePadded[6];
      char workArea[SVC27_WORKAREA_SIZE];
      __packed struct {
        int operationCode : 16;
        int option        : 8;
        int dscbNumber    : 8;
        char * __ptr32 dsname;
        char * __ptr32 volser;
        char * __ptr32 workArea;
      } parmList;
    )
  );

  memset(mem31->dsnameSpacePadded, ' ', sizeof(mem31->dsnameSpacePadded));
  memcpy(mem31->dsnameSpacePadded, dsname, dsnameLength);
  memset(mem31->volserSpacePadded, ' ', sizeof(mem31->volserSpacePadded));
  memcpy(mem31->volserSpacePadded, volser, volserLength);

  memset(mem31->workArea, 0, sizeof(mem31->workArea));

  mem31->parmList.operationCode = SVC27_OPCODE_SEARCH;
  mem31->parmList.option = SVC27_OPTION_EADSCB_OK;
  mem31->parmList.dscbNumber = 0;
  mem31->parmList.dsname = mem31->dsnameSpacePadded;
  mem31->parmList.volser =  mem31->volserSpacePadded;
  mem31->parmList.workArea = mem31->workArea;

  int rc = 0;

  __asm(
      "         LA    1,0(%1)                                                  \n"
      "         SVC   27                                                       \n"
      "         ST    15,%0                                                    \n"
      : "=m"(rc)
      : "r"(&mem31->parmList)
      : "r0", "r15"
  );

  memcpy(dscb1, mem31->workArea, DSCB1_SIZE);

  FREE_STRUCT31(
    STRUCT31_NAME(mem31)
  );

  return rc;
}

#ifdef __ZOWE_OS_ZOS
void addDetailedDatasetMetadata(char *datasetName, int nameLength,
                                char *volser, int volserLength,
                                jsonPrinter *jPrinter) {

  int isPDS = FALSE;
  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Going to check dataset %s attributes\n",datasetName);
  char dscb[INDEXED_DSCB] = {0};
  int rc = obtainDSCB1(datasetName, nameLength,
                       volser, volserLength,
                       dscb);
  if (rc == 0){
    if (DSCB_TRACE){
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "DSCB for %s found\n",datasetName);
      dumpbuffer(dscb,INDEXED_DSCB);
    }
    addDetailsFromDSCB(dscb,jPrinter,&isPDS);
  }
  else{
    char buffer[100];
    sprintf(buffer,"Type 1 or 8 DSCB for dataset %s not found",datasetName);
    jsonAddString(jPrinter,"error",buffer);
  }
}
#endif /* __ZOWE_OS_ZOS */

#ifdef __ZOWE_OS_ZOS
void addMemberedDatasetMetadata(char *datasetName, int nameLength,
                                char *volser, int volserLength,
                                char *memberQuery, int memberLength,
                                jsonPrinter *jPrinter,
                                int includeUnprintable) {

  int isPDS = FALSE;
  char dscb[INDEXED_DSCB] = {0};
  int rc = obtainDSCB1(datasetName, nameLength,
                       volser, volserLength,
                       dscb);

  if (rc != 0) {
    char buffer[100];
    sprintf(buffer, "Type 1 or 8 DSCB for dataset %s not found", datasetName);
    jsonAddString(jPrinter, "error", buffer);
    return;
  }

  /* Use the DSCB to determine if it's
   * a PDS or not, then free the
   * memory.
   */
  isPDS = isPartionedDataset(dscb);

  /* If it's not a PDS, we exit. */
  if (!isPDS) {
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Cannot print members. Selected dataset is not a PDS.");
    return;
  }

  char *theQuery = (memberLength < 1) ? "*":memberQuery;
  if (memberLength < 1){ memberLength = 1;}
  StringList *memberList = getPDSMembers(datasetName);
  int memberCount = stringListLength(memberList);
  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "--PDS %s has %d members\n",datasetName,memberCount);
  if (memberCount > 0){
    jsonStartArray(jPrinter,"members");
    StringListElt *stringElement = firstStringListElt(memberList);
    for (int i = 0; i < memberCount; i++){
      char *memberName = stringElement->string;
      if (matchWithWildcards(theQuery,memberLength,memberName,8,0)) {
        char percentName[24];
        int namePos = 0;
        int containsUnprintable = FALSE;
        for (int j = 0; j < 8; j++) {
          if (memberName[j] < 0x40){
            containsUnprintable = TRUE;
            if (includeUnprintable == FALSE){
              break;
            }
            zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Member pos=%d has hex of 0x%x, will change.\n",j,memberName[j]);
            zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Member name: %s, percent name:%s\n",memberName,percentName);
            sprintf(&percentName[namePos],"%%%2.2x",memberName[j]);
            zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "After: percentName: %s\n",percentName);
            namePos = namePos+3;
          }
          else {
            percentName[namePos] = memberName[j];
            namePos++;
          }
        }

        if (containsUnprintable == FALSE || includeUnprintable == TRUE){
          percentName[namePos] = '\0';

          jsonStartObject(jPrinter,NULL);
          {
            jsonAddString(jPrinter,"name",percentName);
          }
          jsonEndObject(jPrinter);

        }
      }
      stringElement = stringElement->next;
    }
    jsonEndArray(jPrinter);
  }
  SLHFree(memberList->slh);
}
#endif /* __ZOWE_OS_ZOS */

#ifdef __ZOWE_OS_ZOS

#define DSPATH_PREFIX   "//\'"
#define DSPATH_SUFFIX   "\'"

static bool isDatasetPathValid(const char *path) {

  /* Basic check. The fopen() dataset path format is //'dsn(member)' */

  /* Min dataset file name:
   * //'              - 3
   * min ds name      - 1
   * '                - 1
   */
#define PATH_MIN_LENGTH 5

  /* Max dataset file name:
   * //'              - 3
   * man ds name      - 44
   * (                - 1
   * max member name  - 8
   * )                - 1
   * '                - 1
   */
#define PATH_MAX_LENGTH 58

#define DSN_MIN_LENGTH      1
#define DSN_MAX_LENGTH      44
#define MEMBER_MIN_LENGTH   1
#define MEMBER_MAX_LENGTH   8

  size_t pathLength = strlen(path);

  if (pathLength < PATH_MIN_LENGTH || PATH_MAX_LENGTH < pathLength) {
    return false;
  }

  if (memcmp(path, DSPATH_PREFIX, strlen(DSPATH_PREFIX))) {
    return false;
  }
  if (memcmp(&path[pathLength - 1], DSPATH_SUFFIX, strlen(DSPATH_SUFFIX))) {
    return false;
  }

  const char *dsnStart = path + strlen(DSPATH_PREFIX);
  const char *dsnEnd = path + pathLength - 1 - strlen(DSPATH_SUFFIX);

  const char *leftParen = strchr(dsnStart, '(');
  const char *rightParen = strchr(dsnStart, ')');

  if (!leftParen ^ !rightParen) {
    return false;
  }

  if (leftParen) {

    ptrdiff_t dsnLength = leftParen - dsnStart;
    if (dsnLength < DSN_MIN_LENGTH || DSN_MAX_LENGTH < dsnLength) {
      return false;
    }

    if (rightParen != dsnEnd) {
      return false;
    }

    ptrdiff_t memberNameLength = rightParen - leftParen - 1;
    if (memberNameLength < MEMBER_MIN_LENGTH || MEMBER_MAX_LENGTH < memberNameLength) {
      return false;
    }

  } else {

    ptrdiff_t dsnLength = dsnEnd - dsnStart + 1;
    if (dsnLength < DSN_MIN_LENGTH || DSN_MAX_LENGTH < dsnLength) {
      return false;
    }

  }

#undef PATH_MIN_LENGTH
#undef PATH_MAX_LENGTH

#undef DSN_MIN_LENGTH
#undef DSN_MAX_LENGTH
#undef MEMBER_MIN_LENGTH
#undef MEMBER_MAX_LENGTH

  return true;

}

static void extractDatasetAndMemberName(const char *datasetPath,
                                        DatasetName *dsn,
                                        DatasetMemberName *memberName) {

  memset(&dsn->value, ' ', sizeof(dsn->value));
  memset(&memberName->value, ' ', sizeof(memberName->value));

  size_t pathLength = strlen(datasetPath);

  const char *dsnStart = datasetPath + strlen(DSPATH_PREFIX);
  const char *leftParen = strchr(datasetPath, '(');

  if (leftParen) {
    memcpy(dsn->value, dsnStart, leftParen - dsnStart);
    const char *rightParen = strchr(datasetPath, ')');
    memcpy(memberName->value, leftParen + 1, rightParen - leftParen - 1);
  } else {
    memcpy(dsn->value, dsnStart,
           pathLength - strlen(DSPATH_PREFIX""DSPATH_SUFFIX));
  }

  for (int i = 0; i < sizeof(dsn->value); i++) {
    dsn->value[i] = toupper(dsn->value[i]);
  }

  for (int i = 0; i < sizeof(memberName->value); i++) {
    memberName->value[i] = toupper(memberName->value[i]);
  }
}

#undef DSPATH_PREFIX
#undef DSPATH_SUFFIX

static void respondWithDYNALLOCError(HttpResponse *response,
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

#define IS_DAMEMBER_EMPTY($member) \
  (!memcmp(&($member), &(DynallocMemberName){"        "}, sizeof($member)))

static void updateDatasetWithJSONInternal(HttpResponse* response,
                                          const char *datasetPath, /* backward compatibility */
                                          const DatasetName *dsn,
                                          const DDName *ddName,
                                          JsonObject *json) {

  char ddPath[16];
  snprintf(ddPath, sizeof(ddPath), "DD:%8.8s", ddName->value);

  JsonArray *recordArray = jsonObjectGetArray(json,"records");
  int recordCount = jsonArrayGetCount(recordArray);
  int maxRecordLength = 80;
  int isFixed = FALSE;

  /*Check if valid type of dataset to be written to*/
  Volser volser;
  memset(&volser.value, ' ', sizeof(volser.value));

  int volserSuccess = getVolserForDataset(dsn, &volser);
  if (!volserSuccess){
    
    char dscb[INDEXED_DSCB] = {0};
    int rc = obtainDSCB1(dsn->value, sizeof(dsn->value),
                         volser.value, sizeof(volser.value),
                         dscb);
    if (rc == 0){
      if (DSCB_TRACE){
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "DSCB for %.*s found\n", sizeof(dsn->value), dsn->value);
        dumpbuffer(dscb,INDEXED_DSCB);
      }
      bool isPds = false;
      if (!isSupportedWriteDsorg(dscb, &isPds)) {
        respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Unsupported dataset type");
        return;
      } else if (isPds) {
        bool isMember = false;
        int memberStart=44;
        int memberEnd=memberStart+8;
        for (int i = memberStart; i < memberEnd; i++){
          if (*(dsn->value+i) != 0x40){
            isMember = true;
            break;
          }
        }
        if (!isMember){
          respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Overwrite of PDS not supported");
          return;
        }
      }
      
      maxRecordLength = getMaxRecordLength(dscb);
      char recordType = getRecordLengthType(dscb);
      if (recordType == 'F'){
        isFixed = TRUE;
      } else if (recordType == 'U') {
        respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Undefined-length dataset");
        return;
      }
    }
  }
  else{
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "fallback for record length discovery\n");
    fldata_t fileinfo = {0};
    char filenameOutput[100];
    FILE *datasetRead = fopen(datasetPath, "rb, recfm=*, type=record");
    if (datasetRead == NULL) {
      respondWithError(response,HTTP_STATUS_NOT_FOUND,"File could not be opened or does not exist");
      return;
    }

    int returnCode = fldata(datasetRead,filenameOutput,&fileinfo);
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "FLData request rc=0x%x\n",returnCode);
    fflush(stdout);
    if (!returnCode) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_WARNING, 
             "fldata concat=%d, mem=%d, hiper=%d, temp=%d, vsam=%d, hfs=%d, device=%s\n",
             fileinfo.__dsorgConcat, fileinfo.__dsorgMem, fileinfo.__dsorgHiper,
             fileinfo.__dsorgTemp, fileinfo.__dsorgVSAM, fileinfo.__dsorgHFS, fileinfo.__device);

      if (fileinfo.__dsorgVSAM || fileinfo.__dsorgHFS || fileinfo.__dsorgHiper) {
        respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Dataset type not supported");
        fclose(datasetRead);
        return;
      }
      if (fileinfo.__maxreclen){
        maxRecordLength = fileinfo.__maxreclen;
      }
      else {
        respondWithError(response,HTTP_STATUS_INTERNAL_SERVER_ERROR,"Could not discover record length");
        fclose(datasetRead);
        return;
      }
      if (fileinfo.__recfmF) {
        isFixed = TRUE;
      } else if (fileinfo.__recfmU) {
        respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Undefined-length dataset");
        fclose(datasetRead);
        return;
      }
    }
    else {
      respondWithError(response,HTTP_STATUS_INTERNAL_SERVER_ERROR,"Could not read dataset information");
      fclose(datasetRead);
      return;
    }
    fclose(datasetRead);
  }
  /*end dataset type check*/


  /*record length validation*/
  for ( int i = 0; i < recordCount; i++) {
    Json *item = jsonArrayGetItem(recordArray,i);
    if (jsonIsString(item) == TRUE) {
      char *jsonString = jsonAsString(item);
      int recordLength = strlen(jsonString);
      if (recordLength > maxRecordLength) {
        for (int j = recordLength; j > maxRecordLength-1; j--){
          if (jsonString[j] > 0x40){
            zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Invalid record for dataset, recordLength=%d but max for dataset is %d\n", recordLength, maxRecordLength);
            char errorMessage[1024];
            int errorLength = sprintf(errorMessage,"Record #%d with contents \"%s\" is longer than the max record length of %d",i+1,jsonString,maxRecordLength);
            errorMessage[errorLength] = '\0';
            respondWithError(response, HTTP_STATUS_BAD_REQUEST,errorMessage);
            return;
          } 
        }
        recordLength = maxRecordLength;
      }
      if (isFixed && recordLength < maxRecordLength) {
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "UPDATE DATASET: record given for fixed datset less than maxLength=%d, len=%d, data:%s\n",maxRecordLength,recordLength,jsonString);
      }
    }
    else {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Incorrectly formatted array!\n");
      char errorMessage[1024];
      int errorLength = sprintf(errorMessage,"Array position %d is not a string, but must be for record updating",i);
      errorMessage[errorLength] = '\0';
      respondWithError(response, HTTP_STATUS_BAD_REQUEST,errorMessage);
      return;
    }
  }
  /*passed record length check and type check*/

  FILE *outDataset = fopen(datasetPath, "wb, recfm=*, type=record");
  if (outDataset == NULL) {
    respondWithError(response,HTTP_STATUS_NOT_FOUND,"File could not be opened or does not exist");
    return;
  }

  int bytesWritten = 0;
  int recordsWritten = 0;
  char recordBuffer[maxRecordLength+1];

  ICSFDigest digest;
  char hash[32];

  int rcEtag = icsfDigestInit(&digest, ICSF_DIGEST_SHA1);
  if (rcEtag) { //if etag generation has an error, just don't send it.
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_WARNING,  "ICSF error for SHA etag init for write, %d\n",rcEtag);
  }

  for (int i = 0; i < recordCount; i++) {
    char *record = jsonArrayGetString(recordArray,i);
    int recordLength = strlen(record);
    if (recordLength == 0) { //this is a hack, which will be removed as we move away from fwrite
      record = " ";
      recordLength = 1;
    }
    int len;
    if (isFixed) {
      // pad with spaces/or trim if needed
      len = snprintf (recordBuffer, sizeof(recordBuffer), "%-*s", maxRecordLength, record);
    } else {
      // trim if needed
      len = snprintf (recordBuffer, sizeof(recordBuffer), "%s", record);
    }
    bytesWritten = fwrite(recordBuffer,1,len,outDataset);
    recordsWritten++;
    if (bytesWritten < 0 && ferror(outDataset)){
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Error writing to dataset, rc=%d\n", bytesWritten);
      respondWithError(response,HTTP_STATUS_INTERNAL_SERVER_ERROR,"Error writing to dataset");
      fclose(outDataset);
      break;
    } else if (!rcEtag) {
      rcEtag = icsfDigestUpdate(&digest, recordBuffer, bytesWritten);
    }
  }
  
  if (!rcEtag) { rcEtag = icsfDigestFinish(&digest, hash); }
  if (rcEtag) {
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_WARNING,  "ICSF error for SHA etag, %d\n",rcEtag);
  }

  /*success!*/

  jsonPrinter *p = respondWithJsonPrinter(response);
  setResponseStatus(response, 201, "Created");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(p);

  char msgBuffer[128];
  snprintf(msgBuffer, sizeof(msgBuffer), "Updated dataset %s with %d records", datasetPath, recordsWritten);
  jsonAddString(p, "msg", msgBuffer);

  if (!rcEtag) {
    // Convert hash text to hex.
    int eTagLength = digest.hashLength*2;
    char eTag[eTagLength+1];
    memset(eTag, '\0', eTagLength);
    int len = digest.hashLength;
    simpleHexPrint(eTag, hash, digest.hashLength);
    jsonAddString(p, "etag", eTag);
  }
  jsonEnd(p);

  finishResponse(response);

  fclose(outDataset);
}

static void updateDatasetWithJSON(HttpResponse *response, JsonObject *json, char *datasetPath,
                                  const char *lastEtag, bool force) {

  HttpRequest *request = response->request;

  if (!isDatasetPathValid(datasetPath)) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Invalid dataset name");
    return;
  }

  if (!lastEtag && !force) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "No etag given");
    return;
  }

  DatasetName dsn;
  DatasetMemberName memberName;
  extractDatasetAndMemberName(datasetPath, &dsn, &memberName);
  
  DynallocDatasetName daDsn;
  DynallocMemberName daMember;
  memcpy(daDsn.name, dsn.value, sizeof(daDsn.name));
  memcpy(daMember.name, memberName.value, sizeof(daMember.name));
  DynallocDDName daDDname = {.name = "????????"};

  int daRC = RC_DYNALLOC_OK, daSysRC = 0, daSysRSN = 0;
  daRC = dynallocAllocDataset(
      &daDsn,
      IS_DAMEMBER_EMPTY(daMember) ? NULL : &daMember,
      &daDDname,
      DYNALLOC_DISP_OLD,
      DYNALLOC_ALLOC_FLAG_NO_CONVERSION | DYNALLOC_ALLOC_FLAG_NO_MOUNT,
      &daSysRC, &daSysRSN
  );

  if (daRC != RC_DYNALLOC_OK) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,
            "error: ds alloc dsn=\'%44.44s\', member=\'%8.8s\', dd=\'%8.8s\',"
            " rc=%d sysRC=%d, sysRSN=0x%08X (update)\n",
            daDsn.name, daMember.name, daDDname.name, daRC, daSysRC, daSysRSN, "update");
    respondWithDYNALLOCError(response, daRC, daSysRC, daSysRSN,
                             &daDsn, &daMember, "w");
    return;
  }

  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,
          "debug: updating dsn=\'%44.44s\', member=\'%8.8s\', dd=\'%8.8s\'\n",
          daDsn.name, daMember.name, daDDname.name);

  DDName ddName;
  memcpy(&ddName.value, &daDDname.name, sizeof(ddName.value));

  char ddPath[16];
  snprintf(ddPath, sizeof(ddPath), "DD:%8.8s", ddName.value);

  int eTagRC = 0;
  if (!force) { //do not write dataset if current contents do not match contents client expected, unless forced
    int eTagReturnLength = 0;
    int lrecl = getLreclOrRespondError(response, &dsn, ddPath);
    if (lrecl) {
      char *eTag = getDatasetETag(ddPath, lrecl, &eTagRC, &eTagReturnLength);
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_INFO, "Given etag=%s, current etag=%s\n",lastEtag, eTag);
      if (!eTag) {
        respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Could not generate etag");
      } else if (strcmp(eTag, lastEtag)) {
        respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Provided etag did not match system etag. To write, read the dataset again and resolve the difference, then retry.");
        safeFree(eTag,eTagReturnLength+1);
      } else {
        safeFree(eTag,eTagReturnLength+1);
        updateDatasetWithJSONInternal(response, datasetPath, &dsn, &ddName, json);
      }
    }
  } else {
    updateDatasetWithJSONInternal(response, datasetPath, &dsn, &ddName, json);
  }

  daRC = dynallocUnallocDatasetByDDName(&daDDname, DYNALLOC_UNALLOC_FLAG_NONE,
                                        &daSysRC, &daSysRSN);
  if (daRC != RC_DYNALLOC_OK) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,
    		    "error: ds unalloc dsn=\'%44.44s\', member=\'%8.8s\', dd=\'%8.8s\',"
            " rc=%d sysRC=%d, sysRSN=0x%08X (update)\n",
            daDsn.name, daMember.name, daDDname.name, daRC, daSysRC, daSysRSN, "update");
  }
}

#endif /* __ZOWE_OS_ZOS */

#ifdef __ZOWE_OS_ZOS
/* TODO: this has not yet been tested with real, live JSON */
static void updateVSAMDatasetWithJSON(HttpResponse *response, JsonObject *json, char *dsn, hashtable *acbTable) {
  JsonArray *recordArray = jsonObjectGetArray(json,"records");
  int recordCount = jsonArrayGetCount(recordArray);
  char *username = response->request->username;
  int maxRecordLength = 80;
  /*ACEE *newACEE;
  int impersonationBegan = startZOSImpersonation(username,&newACEE);
  if (!impersonationBegan) {
    respondWithError(response,HTTP_STATUS_INTERNAL_SERVER_ERROR,"Failed privilege check");
    endZOSImpersonation(&newACEE);
    return;
  }*/
  char dsnUidPair[44+8+1];
  memset(dsnUidPair, ' ', 44+8);
  memcpy(dsnUidPair, dsn, 44);
  memcpy(dsnUidPair+44, username, 8);
  StatefulACB *state = (StatefulACB *)htGet(acbTable, dsnUidPair);
  char *outACB = state ? state->acb : NULL;
  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "htGet on \"%s\" returned 0x%0x\n", dsnUidPair, outACB);

  if (outACB == NULL) { /* TODO: openACB for output */
    /* TODO: open CSI to find relevant fields if ACB needs to be opened. */
  }
  RPLCommon *rpl = (RPLCommon *)(outACB+RPL_COMMON_OFFSET+8);
  maxRecordLength = rpl->bufLen;

  /* Time to check the JSON for errors while parsing it into a readable format */
  /* TODO: However you wish to format the JSON to easily be read later is your choice.  Below is an unfinished, theoretical approach. */
  for ( int i = 0; i < recordCount; i++) {
    Json *item = jsonArrayGetItem(recordArray,i);
    if (jsonIsString(item) == TRUE) {
      /* TODO: find and B64 decrypt the (key,record) pairs and write them as a block of chars */
      char *jsonString = jsonAsString(item); /* TODO: should be looking at a "str:str" or "str: str" value here? */
      int recordLength = strlen(jsonString); /* TODO: more correctly calculate the recordLength AND keyLength */
      if (recordLength > maxRecordLength) { /* TODO: check recordLength AND keyLength (rpl->keyLen for keyLength) */
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Invalid record for dataset, recordLength=%d but max for dataset is %d\n",recordLength,maxRecordLength);
        char errorMessage[1024];
        int errorLength = sprintf(errorMessage,"Record #%d with contents \"%s\" is longer than the max record length of %d",i+1,jsonString,maxRecordLength);
        errorMessage[errorLength] = '\0';
        respondWithError(response, HTTP_STATUS_BAD_REQUEST,errorMessage);
        return;
      }
    } else {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Incorrectly formatted array!\n");
      char errorMessage[1024];
      int errorLength = sprintf(errorMessage,"Array position %d is not a string, but must be for record updating",i);
      errorMessage[errorLength] = '\0';
      respondWithError(response, HTTP_STATUS_BAD_REQUEST,errorMessage);
      return;
    }
  }
  /* Passed JSON check */

  /* TODO: parse all of the user request parms here.  Currently, the only planned one is "replace". */
  int update = 0;
  /* TODO: make sure RPL is correctly formatted.  If it is not, add a modRPL call similar to the one in respondWithVSAMDataset() */
  /* TODO: the only user parm we need to add to the RPL's list is RPL_OPTCD_UPD (replace) here */

  int recordsWritten = 0;
  int recordLength = 0;
  char *lastKey;
  char *tempRecord = safeMalloc(maxRecordLength, "Temporary Record Bufer"); /* TODO: does it make sense to SLHAlloc this instead? */
  for (int i = 0; i < recordCount; i++) {
    /* TODO: note that non-KSDS uses a pointer to the ? */
    /* TODO: get Key before storing */
    char *key = jsonArrayGetString(recordArray,i); /* TODO: get key AND record here */
    char *record; /* TODO: correctly parse the record, minding your decided format from the above loop. */
    /* TODO: point to the key, based on which type of dataset it is.  See pointByXXX functions in respondWithVSAMDataset() for example syntax */
    if (update) {
      getRecord(outACB, tempRecord, &recordLength); /* in VSAM, we need to GET for update before we can write over a record with a later PUT for update */
    }
    putRecord(outACB, record, recordLength);
    if (rpl->status) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Error writing to dataset, rc=%02x%06x\n",rpl->status,rpl->feedback);
      respondWithError(response,HTTP_STATUS_INTERNAL_SERVER_ERROR,"Error writing to dataset");
      break;
    }
    recordsWritten++;
    lastKey = key; /* TODO: we are keeping this so the caller knows where we left off */
  }
  /*success!*/
  safeFree(tempRecord, maxRecordLength);

  char successMessage[1024];
  int successMessageLength = sprintf(successMessage,"Updated dataset %s with %d records",dsn,recordsWritten); /* TODO: Note last key */
  successMessage[successMessageLength] = '\0';
  respondWithError(response,HTTP_STATUS_OK,successMessage); /*why do we call it respondWithError if we can use successful messages too*/
  /*endZOSImpersonation(&newACEE);*/
}
#endif /* __ZOWE_OS_ZOS */

#ifndef NATIVE_CODEPAGE
#define NATIVE_CODEPAGE CCSID_EBCDIC_1047
#endif

/*
  Dataset only, no wildcards or pds members accepted - beware that this should be wrapped with authentication
 */
static int getVolserForDataset(const DatasetName *dataset, Volser *volser) {
  if (dataset == NULL){
    return -1;
  }
  int length = sizeof(dataset->value);

  char dsnNullTerm[45] = {0};
  memcpy(dsnNullTerm, dataset->value, sizeof(dataset->value));

  int lParenIndex = indexOf(dsnNullTerm, length, '(', 0);
  int asterixIndex = indexOf(dsnNullTerm, length, '*', 0);
  int dollarIndex = indexOf(dsnNullTerm, length, '$', 0);
  if (lParenIndex > -1 || asterixIndex > -1 || dollarIndex > -1){
    return -1;
  }
  csi_parmblock * __ptr32 returnParms = (csi_parmblock* __ptr32)safeMalloc31(sizeof(csi_parmblock),"CSI ParmBlock");
  EntryDataSet *entrySet = returnEntries(dsnNullTerm, defaultDatasetTypesAllowed,3, 0, defaultCSIFields, defaultCSIFieldCount, NULL, NULL, returnParms);
  
  EntryData *entry = entrySet->entries ? entrySet->entries[0] : NULL;
  int rc = -1;
  if (entry){
    char *fieldData = (char*)entry+sizeof(EntryData);
    unsigned short *fieldLengthArray = ((unsigned short *)((char*)entry+sizeof(EntryData)));
    char *fieldValueStart = (char*)entry+sizeof(EntryData)+defaultCSIFieldCount*sizeof(short);
    for (int j=0; j<defaultCSIFieldCount; j++){
      if (!strcmp(defaultCSIFields[j],"VOLSER  ") && fieldLengthArray[j]){
        memcpy(volser->value,fieldValueStart,6);
        rc = 0;
        break;
      }
      fieldValueStart += fieldLengthArray[j];
    }
  }

  for (int i = 0; i < entrySet->length; i++){
    EntryData *currentEntry = entrySet->entries[i];
    int fieldDataLength = currentEntry->data.fieldInfoHeader.totalLength;
    int entrySize = sizeof(EntryData)+fieldDataLength-4;
    safeFree((char*)(currentEntry),entrySize);
  }
  if (entrySet->entries != NULL) {
    safeFree((char*)(entrySet->entries),sizeof(EntryData*)*entrySet->size);
    entrySet->entries = NULL;
  }
  safeFree((char*)entrySet,sizeof(EntryDataSet));    
  safeFree31((char*)returnParms,sizeof(csi_parmblock));
  return rc;
}

static char getRecordLengthType(char *dscb){
  int posOffset = 44;
  int recfm = dscb[84 - posOffset];
  if ((recfm & 0xc0) == 0xc0){
    return 'U';
  }
  else if (recfm & 0x80){
    return 'F';
  }
  else if (recfm & 0x40){
    return 'V';
  }
}

static int getMaxRecordLength(char *dscb){
  int posOffset = 44;
  int lrecl = (dscb[88-posOffset] << 8) | dscb[89-posOffset];
  return lrecl;
}

void updateDataset(HttpResponse* response, char* absolutePath, int jsonMode) {
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
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "UPDATE DATASET: before JSON parse dataLength=0x%x\n",bodyLength);
    Json *json = jsonParseUnterminatedString(slh, 
                                             convertedBody, translationLength,
                                             errorBuffer, sizeof(errorBuffer));
    if (json) {
      if (jsonIsObject(json)) {
        JsonObject *jsonObject = jsonAsObject(json);
        char *etag = jsonObjectGetString(jsonObject,"etag");
        if (!etag) {
          HttpHeader *etagHeader = getHeader(request, "etag");
          if (etagHeader) {
            etag = etagHeader->nativeValue;
          }
        }
        updateDatasetWithJSON(response, jsonObject, absolutePath, etag, force);
      } else{
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
    respondWithError(response,HTTP_STATUS_INTERNAL_SERVER_ERROR,"Could not translate character set to EBCDIC");    
  }
  safeFree(convertedBody,conversionBufferLength);
#endif /* __ZOWE_OS_ZOS */
}

void deleteDatasetOrMember(HttpResponse* response, char* absolutePath) {
#ifdef __ZOWE_OS_ZOS
  HttpRequest *request = response->request;
  if (!isDatasetPathValid(absolutePath)) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Invalid dataset name");
    return;
  }
  
  DatasetName datasetName;
  DatasetMemberName memberName;
  extractDatasetAndMemberName(absolutePath, &datasetName, &memberName);
  DynallocDatasetName daDatasetName;
  DynallocMemberName daMemberName;
  memcpy(daDatasetName.name, datasetName.value, sizeof(daDatasetName.name));
  memcpy(daMemberName.name, memberName.value, sizeof(daMemberName.name));
  DynallocDDName daDDName = {.name = "????????"};

  char CSIType = getCSIType(absolutePath);
  if (CSIType == '') {
    respondWithMessage(response, HTTP_STATUS_NOT_FOUND,
                      "Dataset or member does not exist \'%44.44s(%8.8s)\' "
                      "(%s)", daDatasetName.name, daMemberName.name, "r");
    return;
  }
  if (isVsam(CSIType)) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "VSAM dataset detected. Please use regular dataset route");
    return;
  }

  int daReturnCode = RC_DYNALLOC_OK, daSysReturnCode = 0, daSysReasonCode = 0;
  daReturnCode = dynallocAllocDataset(
              &daDatasetName,
              NULL,
              &daDDName,
              DYNALLOC_DISP_OLD,
              DYNALLOC_ALLOC_FLAG_NO_CONVERSION | DYNALLOC_ALLOC_FLAG_NO_MOUNT,
              &daSysReturnCode, &daSysReasonCode
              );
  
  if (daReturnCode != RC_DYNALLOC_OK) {
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,
    		    "error: ds alloc dsn=\'%44.44s\', member=\'%8.8s\', dd=\'%8.8s\',"
            " rc=%d sysRC=%d, sysRSN=0x%08X (read)\n",
            daDatasetName.name, daMemberName.name, daDDName.name,
            daReturnCode, daSysReturnCode, daSysReasonCode);
    respondWithDYNALLOCError(response, daReturnCode, daSysReturnCode,
                             daSysReasonCode, &daDatasetName, &daMemberName,
                             "r");
    return;
  }
  
  bool isMemberEmpty = IS_DAMEMBER_EMPTY(daMemberName);
  
  if (isMemberEmpty) {
    daReturnCode = dynallocUnallocDatasetByDDName2(&daDDName, DYNALLOC_UNALLOC_FLAG_NONE,
                                                   &daSysReturnCode, &daSysReasonCode,
                                                   TRUE /* Delete data set on deallocation */
                                                   ); 
    if (daReturnCode != RC_DYNALLOC_OK) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,
    		      "error: ds alloc dsn=\'%44.44s\', member=\'%8.8s\', dd=\'%8.8s\',"
              " rc=%d sysRC=%d, sysRSN=0x%08X (read)\n",
              daDatasetName.name, daMemberName.name, daDDName.name,
              daReturnCode, daSysReturnCode, daSysReasonCode);
      respondWithDYNALLOCError(response, daReturnCode, daSysReturnCode,
                               daSysReasonCode, &daDatasetName, &daMemberName,
                               "r");
      return;
    }  
  }
  else {
    char dsNameNullTerm[DATASET_NAME_LEN + 1] = {0};
    memcpy(dsNameNullTerm, datasetName.value, sizeof(datasetName.value));
    
    char *dcb = openSAM(daDDName.name,      /* The data set must be opened by supplying a dd name */
                        OPEN_CLOSE_OUTPUT,  /* To delete a pds data set member, this option must be set */
                        FALSE,              /* Indicates that this data set is not QSAM */
                        0,                  /* Record format (zero if unknown) */
                        0,                  /* Record length (zero if unknown) */
                        0);                 /* Block size (zero if unknown) */
                      
    if (dcb == NULL) {
      respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Data set could not be opened");
      return;
    }
    
    if (!memberExists(dsNameNullTerm, daMemberName)) {
      respondWithError(response, HTTP_STATUS_NOT_FOUND, "Data set member does not exist");
      closeSAM(dcb, 0);
      daReturnCode = dynallocUnallocDatasetByDDName(&daDDName, DYNALLOC_UNALLOC_FLAG_NONE,
                                                    &daSysReturnCode, &daSysReasonCode); 
      return;
    }

    char *belowMemberName = NULL;
    belowMemberName = malloc24(DATASET_MEMBER_NAME_LEN); /* This must be allocated below the line */
    
    if (belowMemberName == NULL) {
      respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Could not allocate member name");
      closeSAM(dcb, 0);
      return;
    }
    
    memset(belowMemberName, ' ', DATASET_MEMBER_NAME_LEN);
    memcpy(belowMemberName, memberName.value, DATASET_MEMBER_NAME_LEN);
    
    int stowReturnCode = 0, stowReasonCode = 0;  
    stowReturnCode = bpamDeleteMember(dcb, belowMemberName, &stowReasonCode);
    
    /* Free member name and dcb as they are no longer needed */
    free24(belowMemberName, DATASET_MEMBER_NAME_LEN);
    closeSAM(dcb, 0);
    
    daReturnCode = dynallocUnallocDatasetByDDName(&daDDName, DYNALLOC_UNALLOC_FLAG_NONE,
                                                  &daSysReturnCode, &daSysReasonCode); 
    
    if (stowReturnCode != 0) {
      char responseMessage[128];
      snprintf(responseMessage, sizeof(responseMessage), "Member %8.8s could not be deleted\n", daMemberName.name);
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,
              "error: stowReturnCode=%d, stowReasonCode=%d\n",
              stowReturnCode, stowReasonCode);
      respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, responseMessage);
      return;
    }

    if (daReturnCode != RC_DYNALLOC_OK) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,
    		      "error: ds alloc dsn=\'%44.44s\', member=\'%8.8s\', dd=\'%8.8s\',"
              " rc=%d sysRC=%d, sysRSN=0x%08X (read)\n",
              daDatasetName.name, daMemberName.name, daDDName.name,
              daReturnCode, daSysReturnCode, daSysReasonCode);
      respondWithDYNALLOCError(response, daReturnCode, daSysReturnCode,
                               daSysReasonCode, &daDatasetName, &daMemberName,
                               "r");
      return;
    }
  }

  jsonPrinter *p = respondWithJsonPrinter(response);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
 
  writeHeader(response);

  jsonStart(p);
  char responseMessage[128];
  if (isMemberEmpty) {
    char* dsName;
    dsName = absolutePath+3;
    dsName[strlen(dsName) - 1] = '\0';
    snprintf(responseMessage, sizeof(responseMessage), "Data set %s was deleted successfully", dsName);
    jsonAddString(p, "msg", responseMessage);
  }
  else {
    snprintf(responseMessage, sizeof(responseMessage), "Data set member %8.8s was deleted successfully", daMemberName.name);
    jsonAddString(p, "msg", responseMessage);
  }
  jsonEnd(p);
 
  finishResponse(response);
  
#endif /* __ZOWE_OS_ZOS */
}

bool memberExists(char* dsName, DynallocMemberName daMemberName) {
  bool found = false;
  StringList *memberList = getPDSMembers(dsName);
  int memberCount = stringListLength(memberList);
  if (memberCount > 0){
    StringListElt *stringElement = firstStringListElt(memberList);
    for (int i = 0; i < memberCount; i++){
      char *memName = stringElement->string;
      char dest[9];
      strncpy(dest, daMemberName.name, 8);
      dest[8] = '\0';
      if (strcmp(memName, dest) == 0) {
        found = true;
      }
      stringElement = stringElement->next;
    }
  }
  SLHFree(memberList->slh);
  return found;
}

bool isVsam(char CSIType) {
  int index = indexOf(vsamCSITypes, strlen(vsamCSITypes), CSIType, 0);
  if (index == -1) {
    return false;
  } else {
    return true;
  }
}

char getCSIType(char* absolutePath) {
  char *typesArg = defaultDatasetTypesAllowed;
  int datasetTypeCount = (typesArg == NULL) ? 3 : strlen(typesArg);
  int workAreaSizeArg = 0;
  int fieldCount = defaultCSIFieldCount;
  char **csiFields = defaultCSIFields;

  csi_parmblock * __ptr32 returnParms = (csi_parmblock* __ptr32)safeMalloc31(sizeof(csi_parmblock),"CSI ParmBlock");

  DatasetName datasetName;
  DatasetMemberName memberName;
  extractDatasetAndMemberName(absolutePath, &datasetName, &memberName);

  char dsNameNullTerm[DATASET_NAME_LEN + 1] = {0};
  memcpy(dsNameNullTerm, datasetName.value, sizeof(datasetName.value));

  EntryDataSet *entrySet = returnEntries(dsNameNullTerm, typesArg, datasetTypeCount, 
                                         workAreaSizeArg, csiFields, fieldCount, 
                                         NULL, NULL, returnParms);

  EntryData *entry = entrySet->entries[0];

  if (entrySet->length == 1) {
    if (entry) {
        return entry->type;
    }
  } else if (entrySet->length == 0) {
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "No entries for the dataset name found");
  } else {
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "More than one entry found for dataset name");
  }

  return '';
}

void deleteVSAMDataset(HttpResponse* response, char* absolutePath) {
#ifdef __ZOWE_OS_ZOS
  HttpRequest *request = response->request;
  
  if (!isDatasetPathValid(absolutePath)) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Invalid dataset name");
    return;
  }
  char CSIType = getCSIType(absolutePath);

  if (CSIType == '') {
    respondWithError(response, HTTP_STATUS_NOT_FOUND, "Dataset not found");
    return;
  }

  if (!isVsam(CSIType)) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "Non VSAM dataset detected. Please use regular dataset route");
    return;
  }

  char* dsName;
  dsName = absolutePath+3;
  dsName[strlen(dsName) - 1] = '\0';
  for (int i = 0; i < strlen(dsName); i++) {
    if (isalpha(dsName[i])) {
      dsName[i] = toupper(dsName[i]);
    }
  }
    
  int rc = deleteCluster(dsName);
  char responseMessage[128];

  if (rc == 0) {
    snprintf(responseMessage, sizeof(responseMessage), "VSAM dataset %s was successfully deleted", dsName);
    jsonPrinter *p = respondWithJsonPrinter(response);
    setResponseStatus(response, 200, "OK");
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);
    jsonStart(p);
    jsonAddString(p, "msg", responseMessage);
    jsonEnd(p);

    finishResponse(response);  
  } else {
    snprintf(responseMessage, sizeof(responseMessage), "Invalid VSAM delete with IDCAMS with return code: %d", rc);
    jsonPrinter *p = respondWithJsonPrinter(response);
    setResponseStatus(response, 403, "Forbidden");
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);
    jsonStart(p);
    jsonAddString(p, "msg", responseMessage);
    jsonEnd(p);

    finishResponse(response);  
      
  }


#endif /* __ZOWE_OS_ZOS */
}


void updateVSAMDataset(HttpResponse* response, char* absolutePath, hashtable *acbTable, int jsonMode) {
#ifdef __ZOWE_OS_ZOS
  if (jsonMode != TRUE) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Cannot update file without JSON formatted record request");
    return;
  }

  HttpRequest *request = response->request;

  int returnCode;
  int reasonCode;

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

    ShortLivedHeap *slh = makeShortLivedHeap(0x10000,0x10);
    char errorBuffer[2048];
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "UPDATE DATASET: before JSON parse dataLength=0x%x\n",bodyLength);
    Json *json = jsonParseUnterminatedString(slh,
                                             convertedBody, translationLength,
                                             errorBuffer, sizeof(errorBuffer));
    if (json) {
      if (jsonIsObject(json)){
        updateVSAMDatasetWithJSON(response, jsonAsObject(json), absolutePath, acbTable);
      } else{
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "*** INTERNAL ERROR *** message is JSON, but not an object\n");
      }
    }
    else {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "UPDATE DATASET: body was not JSON!\n");
      respondWithError(response, HTTP_STATUS_BAD_REQUEST,"POST body could not be parsed as JSON format");
    }
    SLHFree(slh);
  } else {
    respondWithError(response,HTTP_STATUS_INTERNAL_SERVER_ERROR,"Could not translate character set to EBCDIC");
  }
  safeFree(convertedBody,conversionBufferLength);
#endif /* __ZOWE_OS_ZOS */
}

/*
  write = openSAM(name,OPEN_CLOSE_OUTPUT,TRUE,recfm,lrecl,blksize);
  read = openSAM(name,OPEN_CLOSE_INPUT,TRUE,recfm,lrecl,blksize);

 */

static void respondWithDatasetInternal(HttpResponse* response,
                                       const char *datasetPath,
                                       const DatasetName *dsn,
                                       const DDName *ddName) {
#ifdef __ZOWE_OS_ZOS
  HttpRequest *request = response->request;

  char ddPath[16];
  snprintf(ddPath, sizeof(ddPath), "DD:%8.8s", ddName->value);

  int lrecl = getLreclOrRespondError(response, dsn, ddPath);
  if (!lrecl) {
    return;
  }

  jsonPrinter *jPrinter = respondWithJsonPrinter(response);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);

  writeHeader(response);

  if (lrecl){
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Streaming data for %s\n", datasetPath);

    jsonStart(jPrinter);
    int status = streamDataset(ddPath, lrecl, jPrinter);
    jsonEnd(jPrinter);
  }
  finishResponse(response);
#endif /* __ZOWE_OS_ZOS */
}

void respondWithDataset(HttpResponse* response, char* absolutePath, int jsonMode) {

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
  DynallocDDName daDDname = {.name = "????????"};

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

  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,
          "debug: reading dsn=\'%44.44s\', member=\'%8.8s\', dd=\'%8.8s\'\n",
          daDsn.name, daMember.name, daDDname.name);

  DDName ddName;
  memcpy(&ddName.value, &daDDname.name, sizeof(ddName.value));
  respondWithDatasetInternal(response, absolutePath, &dsn, &ddName);

  daRC = dynallocUnallocDatasetByDDName(&daDDname, DYNALLOC_UNALLOC_FLAG_NONE,
                                        &daSysRC, &daSysRSN);
  if (daRC != RC_DYNALLOC_OK) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,
            "error: ds unalloc dsn=\'%44.44s\', member=\'%8.8s\', dd=\'%8.8s\',"
            " rc=%d sysRC=%d, sysRSN=0x%08X (read)\n",
            daDsn.name, daMember.name, daDDname.name, daRC, daSysRC, daSysRSN, "read");
  }
}

#define CSI_VSAMTYPE_KSDS  0x8000
#define CSI_VSAMTYPE_RRDS  0x0200
#define CSI_VSAMTYPE_LDS   0x0004
#define CSI_VSAMTYPE_VRRDS 0x0001
#define CSI_VSAMTYPE_CLOER 0x0020 /* Error on last close - stats may be inaccurate */

/* In development - this is hardcoded for now */
void respondWithVSAMDataset(HttpResponse* response, char* absolutePath, hashtable *acbTable, int jsonMode) {
#ifdef __ZOWE_OS_ZOS
  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "begin %s\n", __FUNCTION__);
  HttpRequest *request = response->request;

  HttpRequestParam *closeParam = getCheckedParam(request,"closeAfter");
  char *closeArg = (closeParam ? closeParam->stringValue : NULL);
  int closeAfter = (closeArg != NULL && !strcmp(closeArg,"true"));

  int returnCode;
  int reasonCode;
  char dsn[45] = "";
  int rplParms = 0;
  memcpy(dsn, absolutePath, 44);
  if (strlen(absolutePath) <= 44) {
    memset(dsn + strlen(absolutePath), ' ', 44 - strlen(absolutePath));
    dsn[44] = 0;
  } else {
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "DSN of size %d is too large.\n", strlen(absolutePath));
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Dataset Name must be 44 bytes of less");
    return;
  }
  char *username = response->request->username;

  int          maxRecords = 0;
  int          maxBytes = 0;
  /* TODO: parse out the parms from the HttpRequest* request */

  unsigned int vsamType       = 0;
  unsigned int ciSize         = 0;
  unsigned int maxlrecl       = 0;
  unsigned int maxbuffer      = 0;
  unsigned int keyLoc         = 0;
  unsigned int keyLen         = 0;
  char         dsnCluster[45] = "";
  char         dsnData[45]    = "";

  /* TODO: We should not need to do this on every call - only if the ACB needs to be opened */
  /* TODO: How to access the CSI in cases where the entry is archived? Is this possible? */
  csi_parmblock * __ptr32 returnParms = (csi_parmblock* __ptr32)safeMalloc31(sizeof(csi_parmblock),"CSI ParmBlock");
  EntryDataSet *entrySet = returnEntries(dsn, clusterTypesAllowed, clusterTypesCount, 0, defaultVSAMCSIFields, defaultVSAMCSIFieldCount, NULL, NULL, returnParms);
  EntryData *entry = entrySet->entries[0];
  if (entry){
    if (entry->type == 'I') { /* TODO: how do we want to handle INDEX datasets?  Some editors use */
      /* TODO: free the entire entrySet, then call CSI again */
    }
    if (entry->type == 'C') {
      unsigned short *fieldLengthArray = ((unsigned short *)((char*)entry+sizeof(EntryData)));
      char *fieldValueStart = (char*)entry+sizeof(EntryData)+defaultVSAMCSIFieldCount*sizeof(short);
      for (int j=0; j<defaultVSAMCSIFieldCount; j++){
        /* zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "j = %d: '%s' (%d)\n", j, defaultVSAMCSIFields[j], fieldLengthArray[j]); */
        if (!strcmp(defaultVSAMCSIFields[j],"ASSOC   ") && fieldLengthArray[j]){
          zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "DATA     = '%44.44s' [%c]\n", fieldValueStart+1, *fieldValueStart);
          memcpy(dsnData,fieldValueStart+1,44);
          /* Is there a use to storing the index component, located here in the case of KSDS? */
          break;
        }
        fieldValueStart += fieldLengthArray[j];
      }
      for (int i = 0; i < entrySet->length; i++){
        EntryData *currentEntry = entrySet->entries[i];
        int fieldDataLength = currentEntry->data.fieldInfoHeader.totalLength;
        int entrySize = sizeof(EntryData)+fieldDataLength-4;
        memset((char*)(currentEntry),0,entrySize);
        safeFree((char*)(currentEntry),entrySize);
      }
      memset((char*)(entrySet->entries),0,sizeof(EntryData*)*entrySet->length);
      safeFree((char*)(entrySet->entries),sizeof(EntryData*)*entrySet->size);
      memset((char*)entrySet,0,sizeof(EntryDataSet));
      safeFree((char*)entrySet,sizeof(EntryDataSet));

      EntryDataSet *entrySet = returnEntries(dsnData, clusterTypesAllowed, clusterTypesCount, 0, defaultVSAMCSIFields, defaultVSAMCSIFieldCount, NULL, NULL, returnParms);
      entry = entrySet->entries[0];
    } else if (entry->type != 'D') {
      safeFree31((char*)returnParms,sizeof(csi_parmblock));
      for (int i = 0; i < entrySet->length; i++){
        EntryData *currentEntry = entrySet->entries[i];
        if (!(entrySet->entries)) break;
        int fieldDataLength = currentEntry->data.fieldInfoHeader.totalLength;
        int entrySize = sizeof(EntryData)+fieldDataLength-4;
        safeFree((char*)(currentEntry),entrySize);
      }
      safeFree((char*)(entrySet->entries),sizeof(EntryData*)*entrySet->length);
      safeFree((char*)entrySet,sizeof(EntryDataSet));
      respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Not Found in Catalog");
      return;
    }

    if (entry->type == 'D') {
      memcpy(dsnData, entry->name, 44);
      unsigned short *fieldLengthArray = ((unsigned short *)((char*)entry+sizeof(EntryData)));
      char *fieldValueStart = (char*)entry+sizeof(EntryData)+defaultVSAMCSIFieldCount*sizeof(short);
      for (int j=0; j<defaultVSAMCSIFieldCount; j++){
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "j = %d: '%s' (%d)\n", j, defaultVSAMCSIFields[j], fieldLengthArray[j]);
        if (!strcmp(defaultVSAMCSIFields[j],"VSAMTYPE") && fieldLengthArray[j]){
          memcpy(&vsamType,fieldValueStart,2);
          vsamType >>= (sizeof(int) * 8 - 16);  /* in case of weird int sizes */
        }
        if (!strcmp(defaultVSAMCSIFields[j],"ASSOC   ") && fieldLengthArray[j]){
          zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "CLUSTER  = '%44.44s' [%c]\n", fieldValueStart+1, *fieldValueStart);
          memcpy(dsnCluster,fieldValueStart+1,44);
        }
        if (!strcmp(defaultVSAMCSIFields[j],"AMDCIREC") && fieldLengthArray[j]){
          memcpy(&ciSize,fieldValueStart,4);
          ciSize >>= (sizeof(int) * 8 - 32);  /* in case of weird int sizes */
          memcpy(&maxlrecl,fieldValueStart+4,4);
          maxlrecl >>= (sizeof(int) * 8 - 32);  /* in case of weird int sizes */
        }
        if (!strcmp(defaultVSAMCSIFields[j],"AMDKEY  ") && fieldLengthArray[j]){
          memcpy(&keyLoc,fieldValueStart,2);
          keyLoc >>= (sizeof(int) * 8 - 16);  /* in case of weird int sizes */
          memcpy(&keyLen,fieldValueStart+2,2);
          keyLen >>= (sizeof(int) * 8 - 16);  /* in case of weird int sizes */
        }
        fieldValueStart += fieldLengthArray[j];
      }
    }
  } else {
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Catalog Entry not found for \"%s\"\n", dsn);
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Not Found in Catalog");
    return;
  } /* end Catalog Search */
  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "vsamType = 0x%0x, ciSize = %d, maxlrecl = %d, keyLoc = %d, keyLen = %d\n", vsamType, ciSize, maxlrecl, keyLoc, keyLen);

  safeFree31((char*)returnParms,sizeof(csi_parmblock));
  for (int i = 0; i < entrySet->length; i++){
    EntryData *currentEntry = entrySet->entries[i];
    if (!(entrySet->entries)) break;
    if (currentEntry == (EntryData *)0x000a0000) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "... how did this happen:\n");
      dumpbuffer((char *)entrySet, 1000);
    }
    int fieldDataLength = currentEntry->data.fieldInfoHeader.totalLength;
    int entrySize = sizeof(EntryData)+fieldDataLength-4;
    safeFree((char*)(currentEntry),entrySize);
  }
  safeFree((char*)(entrySet->entries),sizeof(EntryData*)*entrySet->size);
  safeFree((char*)entrySet,sizeof(EntryDataSet));

  char *dsnUidPair = safeMalloc(44+8+1, "DSN,UID Pair Entry");  /* TODO: plug this leak for each time it is htPut below. */
  memset(dsnUidPair, ' ', 44+8);                                /* TODO:  we will want to free it when the ACB closes.   */
  memcpy(dsnUidPair, dsnData, 44);
  memcpy(dsnUidPair+44, username, 8);
  StatefulACB *state = (StatefulACB *)htGet(acbTable, dsnUidPair);
  char *inACB = state ? state->acb : NULL;
  int searchArg = 0;
  void *argPtr = NULL;

  if (inACB) { /* an ACB exists */
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "ACB found at 0x%0x\n", inACB);
    /* TODO: if opened for output, close and reopen for input by preserving existing macrf, rpl, maxrecl parms. Reset arg. */
    /* TODO: if the user submits a start key parm, this is where it should be entered */
    switch (state->type) {
      case VSAM_TYPE_KSDS:
        argPtr = state->argPtr.key;
        break;
      case VSAM_TYPE_ESDS:
        argPtr = &(state->argPtr.rba);
        break;
      case VSAM_TYPE_LDS:
        argPtr = &(state->argPtr.ci);
        break;
      case VSAM_TYPE_RRDS:
        argPtr = &(state->argPtr.record);
        break;
    }
    safeFree(dsnUidPair, 44+8+1);
  } else { /* need to open an ACB */
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "about to dynalloc a DD to %s\n", dsn);
    DynallocInputParms inputParms;
    memcpy(inputParms.dsName, dsn, DATASET_NAME_LEN);
    memcpy(inputParms.ddName, "MVD00000", DD_NAME_LEN);
    inputParms.disposition = DISP_SHARE;
    char ddname[9] = "MVD00000";
    returnCode = dynallocDataset(&inputParms, &reasonCode);

    int ddNumber = 1;
    while (reasonCode==0x4100000 && ddNumber < 100000) {
      sprintf(inputParms.ddName, "MVD%05d", ddNumber);
      sprintf(ddname, "MVD%05d", ddNumber);
      returnCode = dynallocDataset(&inputParms, &reasonCode);
      ddNumber++;
    }
    if (returnCode) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Dynalloc RC = %d, reasonCode = %x\n", returnCode, reasonCode);
      respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Unable to allocate a DD for ACB");
      return;
    }

    int macrfParms = 0;
    maxbuffer = maxlrecl;
    state = (StatefulACB *)safeMalloc(sizeof(StatefulACB), "Stateful ACB Entry");
    if (vsamType & CSI_VSAMTYPE_KSDS) {
      macrfParms = ACB_MACRF_KEY | ACB_MACRF_SEQ | ACB_MACRF_IN;
      rplParms = RPL_OPTCD_KEY | RPL_OPTCD_SEQ;
      state->type = VSAM_TYPE_KSDS;
    } else if (vsamType & (CSI_VSAMTYPE_RRDS | CSI_VSAMTYPE_VRRDS)) {
      macrfParms = ACB_MACRF_SEQ | ACB_MACRF_IN;
      rplParms = RPL_OPTCD_KEY | RPL_OPTCD_SEQ;
      state->type = VSAM_TYPE_RRDS;
    } else if (vsamType & CSI_VSAMTYPE_LDS) {
      macrfParms = ACB_MACRF_CNV | ACB_MACRF_SEQ | ACB_MACRF_IN;
      rplParms = RPL_OPTCD_CNV | RPL_OPTCD_SEQ;
      state->type = VSAM_TYPE_LDS;
      maxbuffer = ciSize;
    } else { /* assume ESDS otherwise */
      macrfParms = ACB_MACRF_ADR | ACB_MACRF_SEQ | ACB_MACRF_IN;
      rplParms = RPL_OPTCD_ADR | RPL_OPTCD_SEQ;
      state->type = VSAM_TYPE_ESDS;
    }
    inACB = openACB(ddname, ACB_MODE_INPUT, macrfParms, 0, rplParms, maxlrecl, maxbuffer);
    if (!inACB) {
      /* Failed to open ACB! */
      /* TODO: free the few things that we created above that will leak */
      respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,"ACB Not Opened");
      return;
    }
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "ACB for %s opened at %08x\n", ddname, inACB);
    /* TODO: if the user submits a start key parm, enter it in the point function calls below */
    state->acb = inACB;
    switch (state->type) {
      case VSAM_TYPE_KSDS:
        state->argPtr.key = (char *)safeMalloc(keyLen+1, "Stateful ACB Key Buffer");
        memset(state->argPtr.key, 0, keyLen+1);
        argPtr = state->argPtr.key;
        /* returnCode = pointByKey(inACB, (char *)argPtr, keyLen); TODO: add only when the user provides a known key */
        break;
      case VSAM_TYPE_ESDS:
        state->argPtr.rba = 0;
        argPtr = &(state->argPtr.rba);
        returnCode = pointByRBA(inACB, argPtr);
        break;
      case VSAM_TYPE_LDS:
        state->argPtr.ci = 0;
        argPtr = &(state->argPtr.ci);
        returnCode = pointByCI(inACB, argPtr);
        break;
      case VSAM_TYPE_RRDS:
        state->argPtr.record = 1;
        argPtr = &(state->argPtr.record);
        returnCode = pointByRecord(inACB, argPtr);
        break;
    }

    if (returnCode) {
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "point failed with RC = %08x\n", returnCode);
      /* TODO: free the few things that we created above that will otherwise leak */
      respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Could not POINT to the designated search argument.");
      return;
    }
    htPut(acbTable, dsnUidPair, state);
  } /* end Open */

  RPLCommon *inRPL = (RPLCommon *) (inACB+RPL_COMMON_OFFSET+8);
  /* TODO: if the user submits a backwards parm, this is where it should be added to rplParms RPL_OPTCD_BWD here */
  modRPL(inACB, inRPL->rplType, inRPL->keyLen, inRPL->workArea, inRPL->arg,
         rplParms, inRPL->optcd2, inRPL->nextRPL, inRPL->recLen, inRPL->bufLen);

  jsonPrinter *jPrinter = respondWithJsonPrinter(response);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response); 

  writeHeader(response);

  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Streaming data for %s\n", absolutePath);
  jsonStart(jPrinter);
  /* TODO: if the user submits parms that limit the length of the returned data, modifying the below maxRecords or maxBytes will be needed */
  returnCode = streamVSAMDataset(response, inACB, maxlrecl, maxRecords, maxBytes, keyLoc, keyLen, jPrinter);
  zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Dataset bytesRead = %d\n", returnCode);
  jsonEnd(jPrinter);
  dumpbuffer((char *)state, sizeof(StatefulACB));

  finishResponse(response);
  if (closeAfter == TRUE){
    closeACB(inACB,ACB_MODE_INPUT);
    htRemove(acbTable,dsnUidPair);
    safeFree(dsnUidPair, 44+8+1);
    safeFree((char*)state,sizeof(StatefulACB));
  } 

#endif /* __ZOWE_OS_ZOS */
}

int decodePercentByte(char *inString, int inLength, char *outString, int *outStringLength) {
  int outPos = 0;
  for (int i = 0; i < inLength; i++) {
    if (inString[i] == '%') {
      if (i >= inLength-2) {
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Error: Percent seen without following 2 hex characters for '%s'\n",inString);
        return 2;
      }
      char hex[3];
      hex[2] = '\0';
      strncpy(hex,&inString[i+1],2);
      char hexChar = (char)(0xff & strtol(hex,NULL,16));
      memcpy(&outString[outPos],&hexChar,1);
      //outString[outPos] = hexChar;
      outPos++;
      i = i+2;
    }
    else {
      outString[outPos] = inString[i];
      outPos++;
    }
  }
  outString[outPos] = '\0';
  *outStringLength = outPos;
  return 0;
}

void getDatasetAttributes(JsonBuffer *buffer, char** organization, int* maxRecordLen, int* totalBlockSize, char** recordLength, bool* isBlocked, bool* isPDSE) {
  ShortLivedHeap *slh = makeShortLivedHeap(0x10000,0x10);
  char errorBuffer[2048];
  Json *json = jsonParseUnterminatedString(slh,
                                             buffer->data, buffer->len,
                                             errorBuffer, sizeof(errorBuffer));

  if (json) {
    if (jsonIsObject(json)){
      JsonObject *jsonObject = jsonAsObject(json);
      //Get array of datasets
      JsonArray *datasetArray = jsonObjectGetArray(jsonObject,"datasets");
      int i = 0;
      Json *element = jsonArrayGetItem(datasetArray,i);
      if(element && jsonIsObject(element)) {
        JsonObject *jsonDatasetObject = jsonAsObject(element);
        // Get dsorg object
        JsonObject *dsOrg = jsonObjectGetObject(jsonDatasetObject,"dsorg");
        *organization = jsonObjectGetString(dsOrg,"organization");
        *maxRecordLen = jsonObjectGetNumber(dsOrg,"maxRecordLen");
        *totalBlockSize = jsonObjectGetNumber(dsOrg,"totalBlockSize");
        *isPDSE = jsonObjectGetBoolean(dsOrg,"isPDSE");
        // Get recfm object
        JsonObject *recfm = jsonObjectGetObject(jsonDatasetObject,"recfm");
        *recordLength = jsonObjectGetString(recfm,"recordLength");
        *isBlocked = jsonObjectGetBoolean(recfm,"isBlocked");
      }
    }
  }
  SLHFree(slh);
}

int setAttrForDSCopyAndRespondIfError(HttpResponse *response, JsonBuffer *buffer, char* datasetAttributes, bool isSourceMember) {
  char *organization = NULL;
  char *recordLength = NULL;
  char *dsnt = NULL;
  int maxRecordLen = 0;
  int totalBlockSize = 0;
  bool isBlocked = NULL;
  bool isPDSE = NULL;
  char recFormat[3];
  int dirBlock = 0;

  getDatasetAttributes(buffer, &organization, &maxRecordLen, &totalBlockSize, &recordLength, &isBlocked, &isPDSE);

  if(recordLength == "U") {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Undefined-length dataset");
    return ERROR_UNDEFINED_LENGTH_DATASET;
  }

  if(organization == NULL) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Source dataset does not exist");
    return ERROR_DATASET_NOT_EXIST;
  }

  if(!strcmp(organization, "sequential") || isSourceMember) {
    // Pasting a PS as PS or a member as PS dataset [PS -> PS OR Member -> PS]
    organization = "PS";
    dsnt = "";
  } else {
    organization = "PO";
    dsnt = (isPDSE) ? "PDSE" : "PDS";
    dirBlock = 10;
    // Todo: We need to implement the copy paste for the PDS(E) and members.
    respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Do not support copy-paste for PDS(E) Dataset");
    return ERROR_COPY_NOT_SUPPORTED;
  }

  // Hardcoding these attributes because datasetMetadata API does not return it.
  // To do: Update this after the datasetMetadata API is updated
  char *space = "TRK";
  int primaryQuantity = 10;
  int secondaryQuantity = 10;

  strcpy(recFormat, recordLength);
  if(isBlocked) {
    strcat(recFormat, "B");
  }

  sprintf(datasetAttributes, "{\"ndisp\": \"CATALOG\",\"status\": \"NEW\",\"dsorg\": \"%s\",\"space\": \"%s\",\"blksz\": %d,\"lrecl\": %d,\"recfm\": \"%s\",\"close\": \"true\",\"dir\": %d,\"prime\": %d,\"secnd\": %d,\"avgr\": \"U\",\"dsnt\": \"%s\"}\0", organization, space, totalBlockSize, maxRecordLen, recFormat, dirBlock, primaryQuantity, secondaryQuantity, dsnt);

  return 0;
}

int getTargetDsnRecordLength(char* targetDataset) {

  DatasetName targetDsnName;
  DatasetMemberName targetMemName;
  extractDatasetAndMemberName(targetDataset, &targetDsnName, &targetMemName);

  // Buffer to save attributes for target dataset
  JsonBuffer *datasetAttrBuffer = makeJsonBuffer();
  jsonPrinter *jPrinter = makeBufferNativeJsonPrinter(CCSID_UTF_8, datasetAttrBuffer);
  jsonStart(jPrinter);

  // To get the attributes for target dataset
  getDatasetMetadata(&targetDsnName, &targetMemName, targetDataset, "true", "true", defaultDatasetTypesAllowed, "true", 0, NULL, NULL, "", NULL, jPrinter);
  jsonEnd(jPrinter);

  int targetRecLen = 0;

  ShortLivedHeap *slh = makeShortLivedHeap(0x10000,0x10);
  char errorBuffer[2048];
  Json *json = jsonParseUnterminatedString(slh,
                                             datasetAttrBuffer->data, datasetAttrBuffer->len,
                                             errorBuffer, sizeof(errorBuffer));

  if (json) {
    if (jsonIsObject(json)){
      JsonObject *jsonObject = jsonAsObject(json);
      //Get array of datasets
      JsonArray *datasetArray = jsonObjectGetArray(jsonObject,"datasets");
      int i = 0;
      Json *element = jsonArrayGetItem(datasetArray,i);
      if(element && jsonIsObject(element)) {
        JsonObject *jsonDatasetObject = jsonAsObject(element);
        // Get dsorg object
        JsonObject *dsOrg = jsonObjectGetObject(jsonDatasetObject,"dsorg");
        targetRecLen = jsonObjectGetNumber(dsOrg,"maxRecordLen");
        printf("-----TARGET REC LENGTH : %d", targetRecLen);
      }
    }
  }
  SLHFree(slh);
  return targetRecLen;
}

void streamDatasetForCopyAndRespond(HttpResponse *response, char *sourceDataset, int sourceRecordLen, char *targetDataset, bool isTargetMember) {

  FILE *inDataset = fopen(sourceDataset,"rb, type=record");

  if (inDataset == NULL) {
    respondWithError(response,HTTP_STATUS_NOT_FOUND,"Source dataset could not be opened or does not exist");
    return;
  }

  FILE *outDataset = fopen(targetDataset, "wb, recfm=*, type=record");

  if (outDataset == NULL) {
    respondWithError(response,HTTP_STATUS_NOT_FOUND,"Target dataset could not be opened or does not exist");
    return;
  }

  ICSFDigest digest;
  char hash[32];

  int rcEtag = icsfDigestInit(&digest, ICSF_DIGEST_SHA1);
  if (rcEtag) { //if etag generation has an error, just don't send it.
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_WARNING,  "ICSF error for SHA etag init for write, %d\n",rcEtag);
  }

  if(isTargetMember) {
    int targetRecordLen = getTargetDsnRecordLength(targetDataset);
    if(targetRecordLen < sourceRecordLen) {
      respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Cannot copy dataset. Record length for source is longer than the target");
      return;
    }
  }

  int bufferSize = sourceRecordLen+1;
  char buffer[bufferSize];
  int bytesRead = 0;
  int bytesWritten = 0;
  int recordsWritten = 0;

  while (!feof(inDataset)){
    bytesRead = fread(buffer,1,sourceRecordLen,inDataset);
    printf("---BYTES READ: %d\n", bytesRead);
    if (bytesRead > 0 && !ferror(inDataset)) {
      bytesWritten = fwrite(buffer,1,bytesRead,outDataset);
      printf("---BYTES WRITTEN: %d\n", bytesWritten);
      if(ferror(outDataset)) {
        printf("---ferror outDataset\n");
      }
      if ((bytesWritten < 0 && ferror(outDataset)) || (bytesWritten != bytesRead)){
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "Error writing to dataset, rc=%d\n", bytesWritten);
        respondWithError(response,HTTP_STATUS_INTERNAL_SERVER_ERROR,"Error writing to dataset");
        fclose(outDataset);
        fclose(inDataset);
        return;
      } else if (!rcEtag) {
        rcEtag = icsfDigestUpdate(&digest, buffer, bytesWritten);
      }
      recordsWritten++;
    } else if (ferror(inDataset)) {
      printf("---ferror inDataset\n");
      respondWithError(response,HTTP_STATUS_INTERNAL_SERVER_ERROR,"Error writing to dataset");
      zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG,  "Error reading DSN=%s, rc=%d\n", sourceDataset, bytesRead);
      return;
    }
  }

  if (!rcEtag) { rcEtag = icsfDigestFinish(&digest, hash); }
  if (rcEtag) {
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_WARNING,  "ICSF error for SHA etag, %d\n",rcEtag);
  }

  jsonPrinter *p = respondWithJsonPrinter(response);
  setResponseStatus(response, 201, "Successfully Copied Dataset");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(p);

  char msgBuffer[128];
  snprintf(msgBuffer, sizeof(msgBuffer), "Pasted dataset %s with %d records", targetDataset, recordsWritten);
  jsonAddString(p, "msg", msgBuffer);

  if (!rcEtag) {
    // Convert hash text to hex.
    int eTagLength = digest.hashLength*2;
    char eTag[eTagLength+1];
    memset(eTag, '\0', eTagLength);
    int len = digest.hashLength;
    simpleHexPrint(eTag, hash, digest.hashLength);
    jsonAddString(p, "etag", eTag);
  }

  jsonEnd(p);

  finishResponse(response);

  fclose(inDataset);
  fclose(outDataset);

  return;
}

void readWriteToDatasetAndRespond(HttpResponse *response, char* sourceDataset, char* targetDataset, bool isTargetMember) {

  DatasetName dsn;
  DatasetMemberName memberName;
  extractDatasetAndMemberName(sourceDataset, &dsn, &memberName);

  DynallocDatasetName daDsn;
  DynallocMemberName daMember;
  memcpy(daDsn.name, dsn.value, sizeof(daDsn.name));
  memcpy(daMember.name, memberName.value, sizeof(daMember.name));
  DynallocDDName daDDname = {.name = "????????"};

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
  zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,
          "debug: reading dsn=\'%44.44s\', member=\'%8.8s\', dd=\'%8.8s\'\n",
          daDsn.name, daMember.name, daDDname.name);

  DDName ddName;
  memcpy(&ddName.value, &daDDname.name, sizeof(ddName.value));

  char ddPath[16];
  snprintf(ddPath, sizeof(ddPath), "DD:%8.8s", ddName.value);

  int lrecl = getLreclOrRespondError(response, &dsn, ddPath);
  if (!lrecl) {
    daRC = dynallocUnallocDatasetByDDName(&daDDname, DYNALLOC_UNALLOC_FLAG_NONE,
                                        &daSysRC, &daSysRSN);
    if (daRC != RC_DYNALLOC_OK) {
      zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,
            "error: ds unalloc dsn=\'%44.44s\', member=\'%8.8s\', dd=\'%8.8s\',"
            " rc=%d sysRC=%d, sysRSN=0x%08X (read)\n",
            daDsn.name, daMember.name, daDDname.name, daRC, daSysRC, daSysRSN, "read");
    }
    return;
  }

  streamDatasetForCopyAndRespond(response, ddPath, lrecl, targetDataset, isTargetMember);

  daRC = dynallocUnallocDatasetByDDName(&daDDname, DYNALLOC_UNALLOC_FLAG_NONE,
                                        &daSysRC, &daSysRSN);
  if (daRC != RC_DYNALLOC_OK) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_DEBUG,
            "error: ds unalloc dsn=\'%44.44s\', member=\'%8.8s\', dd=\'%8.8s\',"
            " rc=%d sysRC=%d, sysRSN=0x%08X (read)\n",
            daDsn.name, daMember.name, daDDname.name, daRC, daSysRC, daSysRSN, "read");
  }
  return;
}

int checkIfDatasetExistsAndRespond(HttpResponse* response, char* dataset, bool isMember) {

  if(isMember) {
    FILE* memberExists = fopen(dataset,"r");
    if(memberExists) {
      if (fclose(memberExists) != 0) {
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_WARNING, "ERROR CLOSING FILE");
        respondWithJsonError(response, "Could not close dataset", 500, "Internal Server Error");
        return ERROR_CLOSING_DATASET;
      }
      else {
        respondWithJsonError(response, "Member already exists", 400, "Bad Request");
        return ERROR_MEMBER_ALREADY_EXISTS;
      }
    }
    return 0;
  }

  DatasetName dsnName;
  DatasetMemberName memName;
  extractDatasetAndMemberName(dataset, &dsnName, &memName);

  int datasetCount = 0;

  // Buffer to save attributes for target dataset
  JsonBuffer *datasetAttrBuffer = makeJsonBuffer();
  jsonPrinter *jPrinter = makeBufferNativeJsonPrinter(CCSID_UTF_8, datasetAttrBuffer);
  jsonStart(jPrinter);

   // To get the attributes for target dataset
  getDatasetMetadata(&dsnName, &memName, dataset, "true", "true", defaultDatasetTypesAllowed, "true", 0, NULL, NULL, "", NULL, jPrinter);
  jsonEnd(jPrinter);

  ShortLivedHeap *slh = makeShortLivedHeap(0x10000,0x10);
  char errorBuffer[2048];
  Json *json = jsonParseUnterminatedString(slh,
                                             datasetAttrBuffer->data, datasetAttrBuffer->len,
                                             errorBuffer, sizeof(errorBuffer));

  if (json) {
    if (jsonIsObject(json)){
      JsonObject *jsonObject = jsonAsObject(json);
      //Get array of datasets
      JsonArray *datasetArray = jsonObjectGetArray(jsonObject,"datasets");
      datasetCount = jsonArrayGetCount(datasetArray);
    }
  }

  safeFree((char*)datasetAttrBuffer, datasetAttrBuffer->size);
  SLHFree(slh);

  if(datasetCount > 0) {
    respondWithJsonError(response, "Target dataset already exists", 400, "Bad Request");
    return ERROR_DATASET_ALREADY_EXIST;;
  }
  return 0;
}

void pasteAsDatasetMember(HttpResponse *response, char* sourceDataset, char* targetDataset) {
  printf("---PASTING AS A MEMBER\n");
  bool isTargetMember = true;

  int reasonCode = 0;
  int rc = createDataset(response, targetDataset, NULL, 0, &reasonCode);
  if(rc != 0) {
    return;
  }

  return readWriteToDatasetAndRespond(response, sourceDataset, targetDataset, isTargetMember);
}

void copyDatasetAndRespond(HttpResponse *response, char* sourceDataset, char* targetDataset) {
  #ifdef __ZOWE_OS_ZOS
  HttpRequest *request = response->request;

  if (sourceDataset == NULL || strlen(sourceDataset) < 1){
    respondWithError(response,HTTP_STATUS_BAD_REQUEST,"No source dataset name given");
    return;
  }
  if (targetDataset == NULL || strlen(targetDataset) < 1){
    respondWithError(response,HTTP_STATUS_BAD_REQUEST,"No target dataset name given");
    return;
  }

  if(!isDatasetPathValid(sourceDataset)){
    respondWithError(response,HTTP_STATUS_BAD_REQUEST,"Invalid dataset path");
    return;
  }

  if(!isDatasetPathValid(targetDataset)){
    respondWithError(response,HTTP_STATUS_BAD_REQUEST,"Invalid dataset path");
    return;
  }

  /* From here on, we know we have a valid data path */
  DatasetName sourceDsnName;
  DatasetMemberName sourceMemName;
  extractDatasetAndMemberName(sourceDataset, &sourceDsnName, &sourceMemName);

  // Checking if source dataset is a member
  DynallocDatasetName daSourceDsnName;
  DynallocMemberName daSourceMemName;
  memcpy(daSourceDsnName.name, sourceDsnName.value, sizeof(daSourceDsnName.name));
  memcpy(daSourceMemName.name, sourceMemName.value, sizeof(daSourceMemName.name));

  bool isSourceMemberEmpty = IS_DAMEMBER_EMPTY(daSourceMemName);

  DatasetName targetDsnName;
  DatasetMemberName targetMemName;
  extractDatasetAndMemberName(targetDataset, &targetDsnName, &targetMemName);

 // Checking if target dataset is a member
  DynallocDatasetName daTargetDsnName;
  DynallocMemberName daTargetMemName;
  memcpy(daTargetDsnName.name, targetDsnName.value, sizeof(daTargetDsnName.name));
  memcpy(daTargetMemName.name, targetMemName.value, sizeof(daTargetMemName.name));

  bool isTargetMemberEmpty = IS_DAMEMBER_EMPTY(daTargetMemName);

  int dsnExists = checkIfDatasetExistsAndRespond(response, targetDataset, !isTargetMemberEmpty);
  if(dsnExists < 0 ) {
    return;
  }

  // Pasting as a dataset member [PS -> Member OR Member -> Member]
  if(!isTargetMemberEmpty){
    return pasteAsDatasetMember(response, sourceDataset, targetDataset);
  }

  bool isTargetMember = false;

  // Pasting as PS dataset [PS -> PS]
  // Buffer to save attributes for source dataset
  JsonBuffer *datasetAttrBuffer = makeJsonBuffer();
  jsonPrinter *jPrinter = makeBufferNativeJsonPrinter(CCSID_UTF_8, datasetAttrBuffer);
  jsonStart(jPrinter);

  // To get the attributes for source dataset
  getDatasetMetadata(&sourceDsnName, &sourceMemName, sourceDataset, "true", "true", defaultDatasetTypesAllowed, "true", 0, NULL, NULL, "", NULL, jPrinter);
  jsonEnd(jPrinter);

  // To set attributes for target dataset
  char datasetAttributes[300];
  int rc = 0;
  rc = setAttrForDSCopyAndRespondIfError(response, datasetAttrBuffer, datasetAttributes, !isSourceMemberEmpty);

  safeFree((char*)datasetAttrBuffer, datasetAttrBuffer->size);

  if(rc != 0) {
    return;
  }

  int reasonCode = 0;
  rc = createDataset(response, targetDataset, datasetAttributes, strlen(datasetAttributes), &reasonCode);

  if(rc != 0) {
    return;
  }

  return readWriteToDatasetAndRespond(response, sourceDataset, targetDataset, isTargetMember);

  #endif /* __ZOWE_OS_ZOS */
}

getDatasetMetadata(const DatasetName *dsnName, DatasetMemberName *memName, char* datasetOrMember, char* addQualifiersArg, char* detailArg, char* typesArg, char* listMembersArg, int workAreaSizeArg, char* migratedArg, char *resumeNameArg, char *unprintableArg, char *resumeCatalogNameArg, jsonPrinter *jPrinter) {
#ifdef __ZOWE_OS_ZOS
  int dsnLen = strlen(datasetOrMember);
  int lParenIndex = indexOf(datasetOrMember, dsnLen, '(', 0);
  int rParenIndex = indexOf(datasetOrMember, dsnLen, ')', 0);
  int memberNameLength = (unsigned int)rParenIndex  - (unsigned int)lParenIndex -1;
  int datasetTypeCount = (typesArg == NULL) ? 3 : strlen(typesArg);
  int includeUnprintable = !strcmp(unprintableArg, "true") ? TRUE : FALSE;

  if(addQualifiersArg != NULL) {
    int addQualifiers = !strcmp(addQualifiersArg, "true");
    #define DSN_MAX_LEN 44
    char dsnNameNullTerm[DSN_MAX_LEN + 1] = {0}; //+1 for null term
    memcpy(dsnNameNullTerm, dsnName->value, sizeof(dsnName->value));
    nullTerminate(dsnNameNullTerm, sizeof(dsnNameNullTerm) - 1);
    if (addQualifiers && dsnLen <= DSN_MAX_LEN) {
      int dblAsteriskPos = indexOfString(dsnNameNullTerm, dsnLen, "**", 0);
      int periodPos = lastIndexOf(dsnNameNullTerm, dsnLen, '.');
      if (!(dblAsteriskPos == dsnLen - 2 && periodPos == dblAsteriskPos - 1)) {
        if (dsnLen <= DSN_MAX_LEN - 3) {
          snprintf(dsnNameNullTerm, DSN_MAX_LEN + 1, "%s.**", dsnNameNullTerm);
        }
      }
    }
    memcpy(&dsnName->value, dsnNameNullTerm, strlen(dsnNameNullTerm));
    #undef DSN_MAX_LEN
  }

  int fieldCount = defaultCSIFieldCount;
  char **csiFields = defaultCSIFields;
  char dsnNameNullTerm[45] = {0};
  memcpy(dsnNameNullTerm, dsnName->value, sizeof(dsnName->value));
  nullTerminate(dsnNameNullTerm, sizeof(dsnNameNullTerm) - 1);
  csi_parmblock * __ptr32 returnParms = (csi_parmblock* __ptr32)safeMalloc31(sizeof(csi_parmblock),"CSI ParmBlock");
  EntryDataSet *entrySet = returnEntries(dsnNameNullTerm, typesArg, datasetTypeCount, workAreaSizeArg, csiFields, fieldCount, resumeNameArg, resumeCatalogNameArg, returnParms);
  char *resumeName = returnParms->resume_name;
  char *catalogName = returnParms->catalog_name;
  int isResume = (returnParms->is_resume == 'Y');

  char volser[7];
  memset(volser,0,7);

  {
    jsonAddInt(jPrinter,"hasMore",isResume);
    if (isResume) {
      jsonAddUnterminatedString(jPrinter,"resumeName",resumeName,44);
      jsonAddUnterminatedString(jPrinter,"resumeCatalogName",catalogName,44);
    }
    jsonStartArray(jPrinter,"datasets");
    for (int i = 0; i < entrySet->length; i++){
      EntryData *entry = entrySet->entries[i];

      if (entry) {
        int fieldDataLength = entry->data.fieldInfoHeader.totalLength;
        int entrySize = sizeof(EntryData)+fieldDataLength-4; /* -4 for the fact that the length is 4 from end of EntryData */
        int isMigrated = FALSE;
        jsonStartObject(jPrinter, NULL);
        int datasetNameLength = sizeof(entry->name);
        char *datasetName = entry->name;
        jsonAddUnterminatedString(jPrinter, "name", datasetName, datasetNameLength);
        jsonAddUnterminatedString(jPrinter, "csiEntryType", &entry->type, 1);
        int volserLength = 0;
        memset(volser, 0, sizeof(volser));
        char type = entry->type;
        if (type == 'A' || type == 'B' || type == 'D' || type == 'H'){
          char *fieldData = (char*)entry+sizeof(EntryData);
          unsigned short *fieldLengthArray = ((unsigned short *)((char*)entry+sizeof(EntryData)));
          char *fieldValueStart = (char*)entry+sizeof(EntryData)+fieldCount*sizeof(short);
          for (int j=0; j<fieldCount; j++){
            if (!strcmp(csiFields[j],"VOLSER  ") && fieldLengthArray[j]){
              volserLength = 6; /* may contain spaces */
              memcpy(volser,fieldValueStart,volserLength);
              jsonAddString(jPrinter,"volser",volser);
              if (!strcmp(volser,"MIGRAT") || !strcmp(volser,"ARCIVE")){
                isMigrated = TRUE;
              }
              break;
            }
            fieldValueStart += fieldLengthArray[j];
          }
        }

        int shouldListMembers = !strcmp(listMembersArg,"true") || (lParenIndex > 0);
        int detail = !strcmp(detailArg, "true");

        if (detail){
          if (!isMigrated || !strcmp(migratedArg, "true")){
            addDetailedDatasetMetadata(datasetName, datasetNameLength,
                                       volser, volserLength,
                                       jPrinter);
          }
        }
        if (shouldListMembers) {
          if (!isMigrated || !strcmp(migratedArg, "true")){
            addMemberedDatasetMetadata(datasetName, datasetNameLength,
                                       volser, volserLength,
                                       memName->value, memberNameLength,
                                       jPrinter, includeUnprintable);
          }
        }
        jsonEndObject(jPrinter);
        safeFree((char*)(entry),entrySize);
      }
    }
    jsonEndArray(jPrinter);
  }
  safeFree31((char*)returnParms,sizeof(csi_parmblock));
  safeFree((char*)(entrySet->entries),sizeof(EntryData*)*entrySet->size);
  safeFree((char*)entrySet,sizeof(EntryDataSet));

#endif /* __ZOWE_OS_ZOS */
}

void respondWithDatasetMetadata(HttpResponse *response) {
#ifdef __ZOWE_OS_ZOS
  HttpRequest *request = response->request;
  char *datasetOrMember = stringListPrint(request->parsedFile, 2, 2, "?", 0); /*get search term*/

  if (datasetOrMember == NULL || strlen(datasetOrMember) < 1){
    respondWithError(response,HTTP_STATUS_BAD_REQUEST,"No dataset name given");
    return;
  }

  char *username = response->request->username;
  char *percentDecoded = cleanURLParamValue(response->slh, datasetOrMember);
  char *absDsPathTemp = stringConcatenate(response->slh, "//'", percentDecoded);
  char *absDsPath = stringConcatenate(response->slh, absDsPathTemp, "'");

  if(!isDatasetPathValid(absDsPath)){
    respondWithError(response,HTTP_STATUS_BAD_REQUEST,"Invalid dataset path");
    return;
  }

  /* From here on, we know we have a valid data path */
  DatasetName dsnName;
  DatasetMemberName memName;

  extractDatasetAndMemberName(absDsPath, &dsnName, &memName);

  HttpRequestParam *addQualifiersParam = getCheckedParam(request,"addQualifiers");
  char *addQualifiersArg = (addQualifiersParam ? addQualifiersParam->stringValue : NULL);

  HttpRequestParam *detailParam = getCheckedParam(request,"detail");
  char *detailArg = (detailParam ? detailParam->stringValue : NULL);

  HttpRequestParam *typesParam = getCheckedParam(request,"types");
  char *typesArg = (typesParam ? typesParam->stringValue : defaultDatasetTypesAllowed);

  HttpRequestParam *listMembersParam = getCheckedParam(request,"listMembers");
  char *listMembersArg = (listMembersParam ? listMembersParam->stringValue : NULL);

  HttpRequestParam *workAreaSizeParam = getCheckedParam(request,"workAreaSize");
  int workAreaSizeArg = (workAreaSizeParam ? workAreaSizeParam->intValue : 0);

  HttpRequestParam *migratedParam = getCheckedParam(request,"includeMigrated");
  char *migratedArg = (migratedParam ? migratedParam->stringValue : NULL);

  HttpRequestParam *unprintableParam = getCheckedParam(request,"includeUnprintable");
  char *unprintableArg = (unprintableParam ? unprintableParam->stringValue : "");

  HttpRequestParam *resumeNameParam = getCheckedParam(request,"resumeName");
  char *resumeNameArg = (resumeNameParam ? resumeNameParam->stringValue : NULL);

  HttpRequestParam *resumeCatalogNameParam = getCheckedParam(request,"resumeCatalogName");
  char *resumeCatalogNameArg = (resumeCatalogNameParam ? resumeCatalogNameParam->stringValue : NULL);

  if (resumeNameArg != NULL) {
    if (strlen(resumeNameArg) > 44) {
      respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Malformed resume dataset name");
    }
    if (resumeCatalogNameArg == NULL) {
      respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Missing resume catalog name");
    }
    else if (strlen(resumeCatalogNameArg) > 44) {
      respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Malformed resume catalog name");
    }
  }
  else if (resumeCatalogNameArg != NULL) {
    if (strlen(resumeCatalogNameArg) > 44) {
      respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Malformed resume catalog name");
    }
    if (resumeNameArg == NULL) {
      respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Missing resume dataset name");
    }
    else if (strlen(resumeNameArg) > 44) {
      respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Malformed resume dataset name");
    }
  }

  jsonPrinter *jPrinter = respondWithJsonPrinter(response);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);

  jsonStart(jPrinter);
  jsonAddString(jPrinter,"_objectType","com.rs.mvd.base.dataset.metadata");
  jsonAddString(jPrinter,"_metadataVersion","1.1");

  getDatasetMetadata(&dsnName, &memName, datasetOrMember, addQualifiersArg, detailArg, typesArg, listMembersArg, workAreaSizeArg, migratedArg, resumeNameArg, unprintableArg, resumeCatalogNameArg, jPrinter);

  jsonEnd(jPrinter);
  finishResponse(response);

#endif /* __ZOWE_OS_ZOS */
}

static const char hlqFirstChar[29] = {'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','$','@','#'};

void respondWithHLQNames(HttpResponse *response, MetadataQueryCache *metadataQueryCache) {
#ifdef __ZOWE_OS_ZOS
  HttpRequest *request = response->request;
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  jsonPrinter *jPrinter = respondWithJsonPrinter(response);
  writeHeader(response);
  EntryDataSet *hlqSet;
  csi_parmblock * __ptr32 * __ptr32 returnParmsArray;
  char **csiFields = defaultCSIFields;
  int fieldCount = defaultCSIFieldCount;

  HttpRequestParam *cacheParam = getCheckedParam(request,"updateCache");
  char *cacheArg = (cacheParam ? cacheParam->stringValue : NULL);

  int updateCache = (cacheArg != NULL && !strcmp(cacheArg,"true"));

  if (metadataQueryCache->cachedHLQSet && updateCache==FALSE){
    hlqSet = metadataQueryCache->cachedHLQSet;
    returnParmsArray = metadataQueryCache->cachedCSIParmblocks;
  }
  else{
    if (metadataQueryCache->cachedHLQSet != NULL){
      EntryData **entries = metadataQueryCache->cachedHLQSet->entries;
      for (int i = 0; i < metadataQueryCache->cachedHLQSet->length; i++){
        EntryData *entry = entries[i];
        int fieldDataLength = entry->data.fieldInfoHeader.totalLength;
        int entrySize = sizeof(EntryData)+fieldDataLength-4; /* -4 for the fact that the length is 4 from end of EntryData */
        safeFree((char*)entries[i],entrySize);
        safeFree((char*)metadataQueryCache->cachedCSIParmblocks[i],sizeof(csi_parmblock));
      }
      safeFree((char*)metadataQueryCache->cachedHLQSet->entries,sizeof(EntryData*)*(metadataQueryCache->cachedHLQSet->length));
      safeFree((char*)metadataQueryCache->cachedHLQSet,sizeof(EntryDataSet));
    }
    HttpRequestParam *typesParam = getCheckedParam(request,"types");
    char *typesArg = (typesParam ? typesParam->stringValue : defaultDatasetTypesAllowed);
    int datasetTypeCount = (typesArg == NULL) ? 3 : strlen(typesArg);

    HttpRequestParam *workAreaSizeParam = getCheckedParam(request,"workAreaSize");
    int workAreaSizeArg = (workAreaSizeParam ? workAreaSizeParam->intValue : 0);

    returnParmsArray = (csi_parmblock* __ptr32 * __ptr32)safeMalloc31(29*sizeof(csi_parmblock* __ptr32),"CSI Parm Results");
    hlqSet = getHLQs(typesArg, datasetTypeCount, workAreaSizeArg, csiFields, fieldCount, returnParmsArray);
  }
  jsonStart(jPrinter);
  {
    char letterOrSymbol[2];
    letterOrSymbol[1] = '\0';
    jsonStartArray(jPrinter,"csiResults");
    for (int i = 0; i < 29; i++){
      strncpy(letterOrSymbol,hlqFirstChar+i,1);
      jsonStartObject(jPrinter,NULL);
      jsonAddString(jPrinter, "name", letterOrSymbol);
      csi_parmblock *returnParms = returnParmsArray[i];
      int isResume = (returnParms->is_resume == 'Y');
      jsonAddInt(jPrinter,"hasMore",isResume);
      if (isResume){
        jsonAddUnterminatedString(jPrinter,"resumeName",returnParms->resume_name,sizeof(returnParms->resume_name));
        jsonAddUnterminatedString(jPrinter,"resumeCatalogName",returnParms->catalog_name,sizeof(returnParms->catalog_name));
      }
      jsonEndObject(jPrinter);
    }
    jsonEndArray(jPrinter);

    jsonStartArray(jPrinter,"datasets");
    fflush(stdout);
    for (int i = 0; i < hlqSet->length;i++){
      EntryData *entry = hlqSet->entries[i];
      if (entry) {
        int fieldDataLength = entry->data.fieldInfoHeader.totalLength;
        int entrySize = sizeof(EntryData)+fieldDataLength-4; /* -4 for the fact that the length is 4 from end of EntryData */
        if (isBlanks(entry->name, 0, sizeof(entry->name))){
          continue;
        }
        jsonStartObject(jPrinter, NULL);
        jsonAddUnterminatedString(jPrinter, "name", entry->name, sizeof(entry->name));
        jsonAddUnterminatedString(jPrinter, "type", &entry->type, 1);
        char type = entry->type;
        if (type == 'A' || type == 'B' || type == 'D' || type == 'H'){
          int fieldDataLength = entry->data.fieldInfoHeader.totalLength;
          char *fieldData = (char*)entry+sizeof(EntryData);
          unsigned short *fieldLengthArray = ((unsigned short *)((char*)entry+sizeof(EntryData)));
          char *fieldValueStart = (char*)entry+sizeof(EntryData)+fieldCount*sizeof(short);
          for (int j=0; j<fieldCount; j++){
            if (!strcmp(csiFields[j],"VOLSER  ") && fieldLengthArray[j]){
              jsonAddUnterminatedString(jPrinter,"VOLSER",fieldValueStart,fieldLengthArray[j]);
              break;
            }
            fieldValueStart += fieldLengthArray[j];
          }
        }

        jsonEndObject(jPrinter);
      }
    }
    jsonEndArray(jPrinter);
  }
  jsonEnd(jPrinter);
  metadataQueryCache->cachedHLQSet = hlqSet;
  metadataQueryCache->cachedCSIParmblocks = returnParmsArray;
  finishResponse(response);     
#endif /* __ZOWE_OS_ZOS */
}

static int setDatasetAttributesForCreation(JsonObject *object, int *configsCount, TextUnit **inputTextUnit) {
  JsonProperty *currentProp = jsonObjectGetFirstProperty(object);
  Json *value = NULL;
  int parmDefn = DALDSORG_NULL;
  int type = TEXT_UNIT_NULL;
  int rc = 0;

  parmDefn = DISP_NEW; //ONLY one for create
  rc = setTextUnit(TEXT_UNIT_CHAR, 0, NULL, parmDefn, DALSTATS, configsCount, inputTextUnit);


  // most parameters below explained here https://www.ibm.com/docs/en/zos/2.1.0?topic=dfsms-zos-using-data-sets
  // or here https://www.ibm.com/docs/en/zos/2.1.0?topic=function-non-jcl-dynamic-allocation-functions
  // or here https://www.ibm.com/docs/en/zos/2.1.0?topic=function-dsname-allocation-text-units
  while(currentProp != NULL){
    value = jsonPropertyGetValue(currentProp);
    char *propString = jsonPropertyGetKey(currentProp);

    if(propString != NULL){
      errno = 0;
      if (!strcmp(propString, "dsorg")) {
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          parmDefn = (!strcmp(valueString, "PS")) ? DALDSORG_PS : DALDSORG_PO;
          rc = setTextUnit(TEXT_UNIT_INT16, 0, NULL, parmDefn, DALDSORG, configsCount, inputTextUnit);
        }
      } else if(!strcmp(propString, "blksz")) {
        // https://www.ibm.com/docs/en/zos/2.1.0?topic=statement-blksize-parameter
        int64_t valueInt = jsonAsInt64(value);
        if(valueInt != 0){
          if (valueInt <= 0x7FF8 && valueInt >= 0) { //<-- If DASD, if tape, it can be 80000000
            type = TEXT_UNIT_INT16;
          } else if (valueInt <= 0x80000000){
            type = TEXT_UNIT_LONGINT;
          }
          if(type != TEXT_UNIT_NULL) {
            rc = setTextUnit(type, 0, NULL, valueInt, DALBLKSZ, configsCount, inputTextUnit);
          }
        }
      } else if(!strcmp(propString, "lrecl")) {
        int valueInt = jsonAsNumber(value);
        if (valueInt == 0x8000 || (valueInt <= 0x7FF8 && valueInt >= 0)) {
          rc = setTextUnit(TEXT_UNIT_INT16, 0, NULL, valueInt, DALLRECL, configsCount, inputTextUnit);
        }
      } else if(!strcmp(propString, "volser")) {
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          int valueStrLen = strlen(valueString);
          if (valueStrLen <= VOLSER_SIZE){
            rc = setTextUnit(TEXT_UNIT_STRING, VOLSER_SIZE, &(valueString)[0], 0, DALVLSER, configsCount, inputTextUnit);
          }
        }
      } else if(!strcmp(propString, "recfm")) {
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          int valueStrLen = strlen(valueString);
          int setRECFM = 0;
          if (indexOf(valueString, valueStrLen, 'A', 0) != -1){
            setRECFM = setRECFM | DALRECFM_A;
          }
          if (indexOf(valueString, valueStrLen, 'B', 0) != -1){
            setRECFM = setRECFM | DALRECFM_B;
          }
          if (indexOf(valueString, valueStrLen, 'V', 0) != -1){
            setRECFM = setRECFM | DALRECFM_V;
          }
          if (indexOf(valueString, valueStrLen, 'F', 0) != -1){
            setRECFM = setRECFM | DALRECFM_F;
          }
          if (indexOf(valueString, valueStrLen, 'U', 0) != -1){
            setRECFM = setRECFM | DALRECFM_U;
          }
          rc = setTextUnit(TEXT_UNIT_CHAR, 0, NULL, setRECFM, DALRECFM, configsCount, inputTextUnit);
        }
      } else if(!strcmp(propString, "blkln")) {
        // https://www.ibm.com/docs/en/zos/2.1.0?topic=units-block-length-specification-key-0009
        int valueInt = jsonAsNumber(value);
        if (valueInt <= 0xFFFF && valueInt >= 0){
          rc = setTextUnit(TEXT_UNIT_INT24, 0, NULL, valueInt, DALBLKLN, configsCount, inputTextUnit);
        }
      } else if (!strcmp(propString, "ndisp")) {
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          if (!strcmp(valueString, "KEEP")){
            parmDefn = DISP_KEEP;
          } else {
            parmDefn = DISP_CATLG;
          }
          rc = setTextUnit(TEXT_UNIT_CHAR, 0, NULL, parmDefn, DALNDISP, configsCount, inputTextUnit);
        }
      } else if(!strcmp(propString, "strcls")) {
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          int valueStrLen = strlen(valueString);
          if (valueStrLen <= CLASS_WRITER_SIZE){
            rc = setTextUnit(TEXT_UNIT_STRING, CLASS_WRITER_SIZE, &(valueString)[0], 0, DALSTCL, configsCount, inputTextUnit);
          }
        }
      } else if(!strcmp(propString, "mngcls")) {
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          int valueStrLen = strlen(valueString);
          if (valueStrLen <= CLASS_WRITER_SIZE){
            rc = setTextUnit(TEXT_UNIT_STRING, CLASS_WRITER_SIZE, &(valueString)[0], 0, DALMGCL, configsCount, inputTextUnit);
          }
        }
      } else if(!strcmp(propString, "datacls")) {
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          int valueStrLen = strlen(valueString);
          if (valueStrLen <= CLASS_WRITER_SIZE){
            rc = setTextUnit(TEXT_UNIT_STRING, CLASS_WRITER_SIZE, &(valueString)[0], 0, DALDACL, configsCount, inputTextUnit);
          }
        }
      } else if(!strcmp(propString, "space")) {
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          if (!strcmp(valueString, "CYL")) {
            parmDefn = DALCYL;
          } else if (!strcmp(valueString, "TRK")) {
            // https://www.ibm.com/docs/en/zos/2.1.0?topic=units-track-space-type-trk-specification-key-0007
            parmDefn = DALTRK;
          }
          if(parmDefn != DALDSORG_NULL) {
            rc = setTextUnit(TEXT_UNIT_BOOLEAN, 0, NULL, 0, parmDefn, configsCount, inputTextUnit);
          }
        }
      } else if(!strcmp(propString, "dir")) {
        int valueInt = jsonAsNumber(value);
        if (valueInt <= 0xFFFFFF && valueInt >= 0) {
          rc = setTextUnit(TEXT_UNIT_INT24, 0, NULL, valueInt, DALDIR, configsCount, inputTextUnit);
        }
      } else if(!strcmp(propString, "prime")) {
        int valueInt = jsonAsNumber(value);
        if (valueInt <= 0xFFFFFF && valueInt >= 0) {
          rc = setTextUnit(TEXT_UNIT_INT24, 0, NULL, valueInt, DALPRIME, configsCount, inputTextUnit);
        }
      } else if(!strcmp(propString, "secnd")) {
        int valueInt = jsonAsNumber(value);
        if (valueInt <= 0xFFFFFF && valueInt >= 0) {
          rc = setTextUnit(TEXT_UNIT_INT24, 0, NULL, valueInt, DALSECND, configsCount, inputTextUnit);
        }
      } else if(!strcmp(propString, "avgr")) {
        // https://www.ibm.com/docs/en/zos/2.1.0?topic=statement-avgrec-parameter
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          if (!strcmp(valueString, "M")) {
            parmDefn = DALDSORG_MREC;
          } else if (!strcmp(valueString, "K")) {
            parmDefn = DALDSORG_KREC;
          } else if (!strcmp(valueString, "U")) {
            parmDefn = DALDSORG_UREC;
          }
          if(parmDefn != DALDSORG_NULL) {
            rc = setTextUnit(TEXT_UNIT_CHAR, 0, NULL, parmDefn, DALAVGR, configsCount, inputTextUnit);
          }
        }
      } else if(!strcmp(propString, "dsnt")) {
        // https://www.ibm.com/docs/en/zos/2.3.0?topic=jcl-allocating-system-managed-data-sets
        char *valueString = jsonAsString(value);
        if (valueString != NULL) {
          if (!strcmp(valueString, "PDSE")) {
            parmDefn = DALDSORG_PDSE;
          } else if (!strcmp(valueString, "PDS")) {
            parmDefn = DALDSORG_PDS;
          } else if (!strcmp(valueString, "HFS")) {
            parmDefn = DALDSORG_HFS;
          } else if (!strcmp(valueString, "EXTREQ")) {
            parmDefn = DALDSORG_EXTREQ;
          } else if (!strcmp(valueString, "EXTPREF")) {
            parmDefn = DALDSORG_EXTPREF;
          } else if (!strcmp(valueString, "BASIC")) {
            parmDefn = DALDSORG_BASIC;
          } else if (!strcmp(valueString, "LARGE")) {
            parmDefn = DALDSORG_LARGE;
          }
          if(parmDefn != DALDSORG_NULL) {
            rc = setTextUnit(TEXT_UNIT_CHAR, 0, NULL, parmDefn, DALDSNT, configsCount, inputTextUnit);
          }
        }
      }
      
    }
    if(rc == -1) {
      break;
    }
    currentProp = jsonObjectGetNextProperty(currentProp);
    parmDefn = DALDSORG_NULL;
    type = TEXT_UNIT_NULL;
  }
  return rc;
}

static int getDSCB(const DatasetName* datasetName, char* dscb, int bufferSize){
  if (bufferSize < INDEXED_DSCB){
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_WARNING, 
            "DSCB of size %d is too small, must be at least %d", bufferSize, INDEXED_DSCB);
    return 1;
  }
  Volser volser = {0};

  int volserSuccess = getVolserForDataset(datasetName, &volser);
  if(!volserSuccess){
    int rc = obtainDSCB1(datasetName->value, sizeof(datasetName->value),
                           volser.value, sizeof(volser.value),
                           dscb);
    if (rc == 0){
      if (DSCB_TRACE){
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_WARNING, "DSCB for %.*s found\n", sizeof(&datasetName->value), datasetName->value);
        dumpbuffer(dscb,INDEXED_DSCB);
      }
    }
    return 0;
  }
  else {
    return 1;
  }
}

int createDatasetMember(HttpResponse* response, DatasetName* datasetName, char* absolutePath) {
  char dscb[INDEXED_DSCB] = {0};
  int bufferSize = sizeof(dscb);
  if (getDSCB(datasetName, dscb, bufferSize) != 0) {
    respondWithJsonError(response, "Error decoding dataset", 400, "Bad Request");
    return ERROR_DECODING_DATASET;
  }
  else {
    if (!isPartionedDataset(dscb)) {
      respondWithJsonError(response, "Dataset must be PDS/E", 400, "Bad Request");
      return ERROR_INCORRECT_DATASET_TYPE;
    }
    else {
      char *overwriteParam = getQueryParam(response->request,"overwrite");
      int overwrite = !strcmp(overwriteParam, "true") ? TRUE : FALSE;
      FILE* memberExists = fopen(absolutePath,"r");
      if (memberExists && overwrite != TRUE) {
        if (fclose(memberExists) != 0) {
            zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_WARNING, "ERROR CLOSING FILE");
            respondWithJsonError(response, "Could not close dataset", 500, "Internal Server Error");
            return ERROR_CLOSING_DATASET;
        }
        else {
          respondWithJsonError(response, "Member already exists and overwrite not specified", 400, "Bad Request");
          return ERROR_MEMBER_ALREADY_EXISTS;
        }
      }
      else { 
        if (memberExists) {
          if (fclose(memberExists) != 0) {
            zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_WARNING, "ERROR CLOSING FILE");
            respondWithJsonError(response, "Could not close dataset", 500, "Internal Server Error");
            return ERROR_CLOSING_DATASET;
          }
        }
        FILE* newMember = fopen(absolutePath, "w");
        if (!newMember){
          respondWithJsonError(response, "Bad dataset name", 400, "Bad Request");
          return ERROR_BAD_DATASET_NAME;
        }
        if (fclose(newMember) == 0){
          // response200WithMessage(response, "Successfully created member");
          return 0;
        }
        else {
          zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_WARNING, "ERROR CLOSING FILE");
          respondWithJsonError(response, "Could not close dataset", 500, "Internal Server Error");
          return ERROR_CLOSING_DATASET;
        }
      }
    }
  }
  return 0;
}

int createDataset(HttpResponse* response, char* absolutePath, char* datasetAttributes, int translationLength, int* reasonCode) {
  #ifdef __ZOWE_OS_ZOS

  DatasetName datasetName;
  DatasetMemberName memberName;
  extractDatasetAndMemberName(absolutePath, &datasetName, &memberName);

  DynallocDatasetName daDatasetName;
  DynallocMemberName daMemberName;
  memcpy(daDatasetName.name, datasetName.value, sizeof(daDatasetName.name));
  memcpy(daMemberName.name, memberName.value, sizeof(daMemberName.name));

  DynallocDDName daDDName = {.name = "????????"};

  int daRC = RC_DYNALLOC_OK, daSysReturnCode = 0, daSysReasonCode = 0;

  bool isMemberEmpty = IS_DAMEMBER_EMPTY(daMemberName);

  if(!isMemberEmpty){
    return createDatasetMember(response, &datasetName, absolutePath);
  }

  int configsCount = 0;
  char ddNameBuffer[DD_NAME_LEN+1] = "MVD00000";
  TextUnit *inputTextUnit[TOTAL_TEXT_UNITS] = {NULL};

  ShortLivedHeap *slh = makeShortLivedHeap(0x10000,0x10);
  char errorBuffer[2048];
  Json *json = jsonParseUnterminatedString(slh,
                                             datasetAttributes, translationLength,
                                             errorBuffer, sizeof(errorBuffer));

  int returnCode = 0;
  if (json) {
    if (jsonIsObject(json)){
      JsonObject * jsonObject = jsonAsObject(json);
      returnCode = setTextUnit(TEXT_UNIT_STRING, DATASET_NAME_LEN, &datasetName.value[0], 0, DALDSNAM, &configsCount, inputTextUnit);
      if(returnCode == 0) {
        returnCode = setTextUnit(TEXT_UNIT_STRING, DD_NAME_LEN, ddNameBuffer, 0, DALDDNAM, &configsCount, inputTextUnit);
      }
      if(returnCode == 0) {
        returnCode = setDatasetAttributesForCreation(jsonObject, &configsCount, inputTextUnit);
      }
    }
  } else {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Invalid JSON request body");
    return ERROR_INVALID_JSON_BODY;
  }

  if (returnCode == 0) {
    returnCode = dynallocNewDataset(inputTextUnit, configsCount, reasonCode);
    int ddNumber = 1;
    while (*reasonCode==0x4100000 && ddNumber < 100000) {
      sprintf(ddNameBuffer, "MVD%05d", ddNumber);
      int ddconfig = 1;
      setTextUnit(TEXT_UNIT_STRING, DD_NAME_LEN, ddNameBuffer, 0, DALDDNAM, &ddconfig, inputTextUnit);
      returnCode = dynallocNewDataset(inputTextUnit, configsCount, reasonCode);
      ddNumber++;
    }
  }

  if (returnCode) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_WARNING,
            "error: ds alloc dsn=\'%44.44s\' dd=\'%8.8s\', sysRC=%d, sysRSN=0x%08X\n",
            daDatasetName.name, ddNameBuffer, returnCode, *reasonCode);
    respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Unable to allocate a DD for ACB");
    return ERROR_ALLOCATING_DATASET;
  }

  memcpy(daDDName.name, ddNameBuffer, DD_NAME_LEN);
  daRC = dynallocUnallocDatasetByDDName(&daDDName, DYNALLOC_UNALLOC_FLAG_NONE, &returnCode, reasonCode);
  if (daRC != RC_DYNALLOC_OK) {
    zowelog(NULL, LOG_COMP_DATASERVICE, ZOWE_LOG_WARNING,
            "error: ds unalloc dsn=\'%44.44s\' dd=\'%8.8s\', rc=%d sysRC=%d, sysRSN=0x%08X\n",
            daDatasetName.name, daDDName.name, daRC, returnCode, *reasonCode);
    respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Unable to deallocate DDNAME");
    return ERROR_DEALLOCATING_DATASET;
  }
  return 0;
  #endif
}

void createDatasetAndRespond(HttpResponse* response, char* absolutePath, int jsonMode) {
  #ifdef __ZOWE_OS_ZOS
  HttpRequest *request = response->request;

  char* errorMessage = NULL;
  int errorCode = 0;

  if (!isDatasetPathValid(absolutePath)) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Invalid dataset name");
    return;
  }

  if (jsonMode != TRUE) { /*TODO add support for updating files with raw bytes instead of JSON*/
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Cannot update file without JSON formatted record request");
    return;
  }

  char *contentBody = request->contentBody;
  int bodyLength = strlen(contentBody);

  char *convertedBody = safeMalloc(bodyLength*4,"writeDatasetConvert");
  int conversionBufferLength = bodyLength*4;
  int translationLength;
  int outCCSID = NATIVE_CODEPAGE;
  int reasonCode;

  int returnCode = convertCharset(contentBody,
                              bodyLength,
                              CCSID_UTF_8,
                              CHARSET_OUTPUT_USE_BUFFER,
                              &convertedBody,
                              conversionBufferLength,
                              outCCSID,
                              NULL,
                              &translationLength,
                              &reasonCode);

  if (returnCode == 0) {
    returnCode = createDataset(response, absolutePath, convertedBody, translationLength, &reasonCode);
  }

  if(returnCode == 0) {
    response200WithMessage(response, "Successfully created dataset");
  }

  #endif
}

#endif /* not METTLE - the whole module */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

