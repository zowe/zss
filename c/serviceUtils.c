

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
/* #include <ctest.h> */
#include <stdlib.h>
#include <strings.h>
#include <stdarg.h>  
#include <pthread.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "collections.h"
#include "bpxnet.h"
#include "socketmgmt.h"
#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif
#include "le.h"
#include "json.h"
#include "serviceUtils.h"
#include "httpserver.h"

/* snippet of column metadata standard 
     "context": context-name, // optional (see preceding comment)
        "columnIdentifier": "SGRP",
        "shortColumnLabel": "SGRP",
        "longColumnLabel": "Storage Group",
        "columnDescription": "",
        "rawDataType": "string",
        "defaultSortDirection": "A",
        "sortType": "lexical", // lexical/numeric. default is based on the rawDataType
        "comparatorFunction" : "myFunction", // optional: if hte sortType is not flexible enough
        "unit": null,
        "displayHints": {
          "tableAlignment": "L",
          "minWidth": "5",
          "maxWidth": "8",
        }
        */

void addStringColumnInfo(jsonPrinter *p,
                         char *identifier,
                         char *shortLabel, 
                         char *longLabel, 
                         int   minWidth,
                         int   maxWidth){
  jsonStartObject(p,NULL);
  jsonAddString(p,"columnIdentifier", identifier);
  jsonAddString(p,"shortColumnLabel", shortLabel);
  jsonAddString(p,"longColumnLabel",  longLabel);
  jsonAddString(p,"rawDataType",      "number");
  jsonStartObject(p,"displayHints");
  if (minWidth != -1){
    jsonAddInt(p,"minWidth",minWidth);
  }
  if (maxWidth != -1){
    jsonAddInt(p,"maxWidth",maxWidth);
  }
  jsonEndObject(p);
  jsonEndObject(p);
}

void addNumberColumnInfo(jsonPrinter *p,
                         char *identifier,
                         char *shortLabel, 
                         char *longLabel, 
                         int   minWidth,
                         int   maxWidth){
  jsonStartObject(p,NULL);
  jsonAddString(p,"columnIdentifier", identifier);
  jsonAddString(p,"shortColumnLabel", shortLabel);
  jsonAddString(p,"longColumnLabel",  longLabel);
  jsonAddString(p,"rawDataType",      "number");
  jsonStartObject(p,"displayHints");
  if (minWidth != -1){
    jsonAddInt(p,"minWidth",minWidth);
  }
  if (maxWidth != -1){
    jsonAddInt(p,"maxWidth",maxWidth);
  }
  jsonEndObject(p);
  jsonEndObject(p);
}

void addBooleanColumnInfo(jsonPrinter *p,
                          char *identifier,
                          char *shortLabel, 
                          char *longLabel, 
                          int   minWidth,
                          int   maxWidth){
  jsonStartObject(p,NULL);
  jsonAddString(p,"columnIdentifier", identifier);
  jsonAddString(p,"shortColumnLabel", shortLabel);
  jsonAddString(p,"longColumnLabel",  longLabel);
  jsonAddString(p,"rawDataType",      "number");
  jsonStartObject(p,"displayHints");
  if (minWidth != -1){
    jsonAddInt(p,"minWidth",minWidth);
  }
  if (maxWidth != -1){
    jsonAddInt(p,"maxWidth",maxWidth);
  }
  jsonEndObject(p);
  jsonEndObject(p);
}

#define JSON_STRING_SIZE 260

static int getSignificantLength(char *s, int len){
  int start = len;
  int i;
  if (len > JSON_STRING_SIZE-1){
    start = JSON_STRING_SIZE-1;
  }
  /* printf("value below, len=%d, start=%d\n",len,start);
     dumpbuffer(value,16); */
  for (i=start; i>0; i--){
    if (s[i-1] > 0x41){
      break;
    }
  }
  return i;
}


static void jsonStringOut(jsonPrinter *p, char *key, char *value, int len){
  char buffer[JSON_STRING_SIZE];
  int significantLength = getSignificantLength(value,len);
  memcpy(buffer,value,significantLength);
  buffer[significantLength] = 0;
  jsonAddString(p,key,buffer);
}

static void jsonObjectOut(jsonPrinter *p, char *value, int len){
  char buffer[JSON_STRING_SIZE];
  int significantLength = getSignificantLength(value,len);
  memcpy(buffer,value,significantLength);
  buffer[significantLength] = 0;
  jsonStartObject(p,buffer);
}

void addMetaData(jsonPrinter *p,
                 char *name,
                 char *identifier,
                 int length) {
  jsonObjectOut(p,name,length);
  jsonStringOut(p,"globalIdentifier",name,8);
  jsonEndObject(p);
}

void startTypeInfo(jsonPrinter *p, char *tableID, char *shortTableName){
  jsonAddString(p, "resultMetaDataSchemaVersion", "1.0");
  jsonAddString(p,"resultType", "table");
  jsonStartObject(p,"metaData");
  jsonStartObject(p,"tableMetaData");
  jsonAddString(p,"tableIdentifier",tableID);
  jsonAddString(p,"shortTableLabel",shortTableName);
  jsonEndObject(p); /* the tableMetaData */
  jsonStartArray(p,"columnMetaData");

  /* call addColumnInfo 1 or more times */
}

void endTypeInfo(jsonPrinter *p){
  jsonEndArray(p); /* column metadata */
  jsonEndObject(p); /* metadata */
  /* rows should start after this */
}

void makeTableResponseMetadata(jsonPrinter *p, 
                               TableEmitter *emitter){
  jsonAddString(p,"resultMetaDataSchemaVersion","1.0");
  jsonAddString(p,"serviceVersion", "1.0");
  jsonAddString(p,"resultType", "table");
  jsonStartObject(p,"metaData");
  
  jsonStartObject(p,"tableMetaData");

  jsonEndObject(p);

  jsonStartArray(p,"groupMetaData");

  jsonEndArray(p);

  jsonStartArray(p,"columnMetaData");

  emitter->columnMetadataWriter(p);

  jsonEndArray(p);

  jsonStartArray(p,"actionMetaData");

  jsonEndArray(p);
  
  jsonEndObject(p); /* Metadata */
}

void printErrorResponseMetadata(jsonPrinter *printer) {
  /* TODO this format may change */
  printResponseMetadata(printer, "org.zowe.zss.error", "0.0.1");
}

void printTableResultResponseMetadata(jsonPrinter *printer) {
  jsonAddString(printer, "_objectType", "org.zowe.zss.doctype.tableResult");
  jsonAddString(printer, "resultMetaDataSchemaVersion", "1.0");
  jsonAddString(printer, "serviceVersion", "1.0");
}

void printResponseMetadata(jsonPrinter *printer, char *objectType, char *metadataVersion) {
  jsonAddString(printer, "_objectType", objectType);
  jsonAddString(printer, "_metaDataVersion", metadataVersion);
}

void flushAndRecycleJSON(void *arg){
  WSSession *session = (WSSession*)arg;
  flushWSJsonPrinting(session);
  session->jp->isStart = TRUE;
  session->jp->isFirstLine = TRUE;
}

void wsSendError(WSSession *session, char *error) {
  jsonStartObject(session->jp,NULL);
  jsonAddString(session->jp,"error", error);
  jsonEndObject(session->jp);
  flushAndRecycleJSON(session);
}

int minLength(int x, int y){
  if (x < y){
    return x;
  } else{
    return y;
  }
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

