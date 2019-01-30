

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __ZOWE_SERVICE_UTILS__
#define __ZOWE_SERVICE_UTILS__

#include "httpserver.h"

typedef struct TableEmitter_tag{
  /* how do we link from URL parse to getting an exporter */
  void (*columnMetadataWriter)(jsonPrinter *p);
  void (*rowEncoder)(jsonPrinter *p, void *row);
} TableEmitter; 


void startTypeInfo(jsonPrinter *p, char *tableID, char *shortTableName);

void addStringColumnInfo(jsonPrinter *p,
                         char *identifier,
                         char *shortLabel, 
                         char *longLabel, 
                         int   minWidth,
                         int   maxWidth);

void addNumberColumnInfo(jsonPrinter *p,
                         char *identifier,
                         char *shortLabel, 
                         char *longLabel, 
                         int   minWidth,
                         int   maxWidth);

void addBooleanColumnInfo(jsonPrinter *p,
                          char *identifier,
                          char *shortLabel, 
                          char *longLabel, 
                          int   minWidth,
                          int   maxWidth);

void endTypeInfo(jsonPrinter *p);

void printErrorResponseMetadata(jsonPrinter *printer);

void printTableResultResponseMetadata(jsonPrinter *printer);

void printResponseMetadata(jsonPrinter *printer, char *objectType, char *metadataVersion);

void flushAndRecycleJSON(void *arg);

void wsSendError(WSSession *session, char *error);

int minLength(int x, int y);

#endif 


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

