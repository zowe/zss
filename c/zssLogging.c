

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

#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <iconv.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#endif

#include "zowetypes.h"
#include "logging.h"
#include "zos.h"
#include "zssLogging.h"

bool isLogLevelValid(int level) {

  if (level < ZOWE_LOG_SEVERE || level > ZOWE_LOG_DEBUG3) {
    return FALSE;
  }

  return TRUE;
}

#define PREFIXED_LINE_MAX_COUNT         1000
#define PREFIXED_LINE_MAX_MSG_LENGTH    4096
#define LOG_MSG_PREFIX_SIZE 57

typedef struct LogMessagePrefix_tag {
  char text[LOG_MSG_PREFIX_SIZE];
} LogMessagePrefix;

typedef struct LogTimestamp_tag {
  union {
    struct {
      char year[4];
      char delim00[1];
      char month[2];
      char delim01[1];
      char day[2];
      char delim02[1];
      char hour[2];
      char delim03[1];
      char minute[2];
      char delim04[1];
      char second[2];
      char delim05[1];
      char hundredth[2];
    } dateAndTime;
    char text[22];
  };
} LogTimestamp;


static void getCurrentLogTimestamp(LogTimestamp *timestamp) {

  uint64 stck = 0;
  getSTCKU(&stck);

  stck += getLocalTimeOffset();

  stckToLogTimestamp(stck, timestamp);

}

static void getLocationData(char *path, int line, char **locationInfo, uint64 compID, LoggingComponent *component) {

  char prefix[128]; 
  char suffix[128]; 

  unsigned int id = compID & 0xFFFFFFFF;
  if(compID >= LOG_PROD_COMMON && compID < LOG_PROD_ZIS) {
    snprintf(prefix,5,"_zcc");
    switch (id) { // most of these don't actally log, but are defined in logging.h
      case LOG_COMP_ALLOC: snprintf(suffix,6,"alloc");
                    break;
      case LOG_COMP_UTILS: snprintf(suffix,6,"utils");
                    break;
      case LOG_COMP_COLLECTIONS: snprintf(suffix,12,"collections");    
                    break;
      case LOG_COMP_SERIALIZATION: snprintf(suffix,14,"serialization");
                    break;
      case LOG_COMP_ZLPARSER: snprintf(suffix,9,"zlparser");
                    break;
      case LOG_COMP_ZLCOMPILER: snprintf(suffix,11,"zlcompiler");
                    break;
      case LOG_COMP_ZLRUNTIME: snprintf(suffix,10,"zlruntime"); 
                    break;
      case LOG_COMP_STCBASE: snprintf(suffix,8,"stcbase");
                    break;
      case LOG_COMP_HTTPSERVER: snprintf(suffix,11,"httpserver");
                    break;
      case LOG_COMP_DISCOVERY: snprintf(suffix,10,"discovery");
                    break;
      case LOG_COMP_DATASERVICE: snprintf(suffix,12,"dataservice");
                    break;
      case LOG_COMP_CMS: snprintf(suffix,4,"cms");
                    break;
      case 0xC0001: snprintf(suffix,4,"cms");
                    break;
      case 0xC0002: snprintf(suffix,6,"cmspc");
                    break;
      case LOG_COMP_LPA: snprintf(suffix,4,"lpa");
                    break;
      case LOG_COMP_RESTDATASET: snprintf(suffix,12,"restdataset");
                    break;
      case LOG_COMP_RESTFILE: snprintf(suffix,9,"restfile");
                    break;
    }
  }
  else if (compID >= LOG_PROD_ZIS && compID < LOG_PROD_ZSS) {
    snprintf(prefix,5,"_zis");
  }
  else if (compID >= LOG_PROD_ZSS && compID < LOG_PROD_PLUGINS ) {
    snprintf(prefix,5,"_zss"); 
    switch (id) {
      case LOG_COMP_ID_ZSS: snprintf(suffix,4,"zss");
                    break;
      case LOG_COMP_ID_CTDS: snprintf(suffix,5,"ctds");
                    break;
      case LOG_COMP_ID_SECURITY: snprintf(suffix,9,"security");
                    break;
      case LOG_COMP_ID_UNIXFILE: snprintf(suffix,9, "unixfile");
                    break;
    }
  }
  else if (compID > LOG_PROD_PLUGINS) {
    snprintf(suffix,strlen(component->name),"%s",(strrchr(component->name, '/')+1)); // given more characters than it writes
    snprintf(prefix,strlen(component->name)-strlen(suffix),"%s",component->name);
  }

  char *filename;
  filename = strrchr(path, '/'); // returns a pointer to the last occurence of '/'
  filename+=1; 
  //               formatting + prefix + suffix + filename + line number
  int locationSize =  7  + strlen(prefix) + strlen(suffix) + strlen(filename) + 5; 
  *locationInfo = (char*) safeMalloc(locationSize,"locationInfo");
  snprintf(*locationInfo,locationSize+1," (%s:%s,%s:%d) ",prefix,suffix,filename,line); 
}

void initLogMessagePrefix(LogMessagePrefix *prefix, LoggingComponent *component, char* path, int line, int level, uint64 compID) {
  LogTimestamp currentTime;
  getCurrentLogTimestamp(&currentTime);
  ASCB *ascb = getASCB();
  char *jobName = getASCBJobname(ascb);
//  TCB *tcb = getTCB();

  ACEE *acee;
  acee = getAddressSpaceAcee();
  char user[7] = { 0 }; // wil this always be 7?
  snprintf(user,7,acee->aceeuser+1);

  pthread_t threadID = pthread_self();
  char thread[10];
  snprintf(thread,10,"%d",threadID);
  
  char *logLevel;
  
  switch(level) {
    case 0: snprintf(logLevel,7,"SEVERE");
            break;
    case 1: snprintf(logLevel,8,"WARN");
            break;
    case 2: snprintf(logLevel,5,"INFO"); 
            break;
    case 3: snprintf(logLevel,5,"DEBUG");
            break;
    case 4: snprintf(logLevel,6,"DEBUG");
            break;
    case 5: snprintf(logLevel,7,"TRACE");
            break;
  }
  
  char *locationInfo; // largest possible variation in size
  getLocationData(path,line,&locationInfo,compID,component); // location info is allocated in getLocationData

  
  snprintf(prefix->text, sizeof(prefix->text), "%22.22s <%8.8s:%s> %s %s (%s) ", currentTime.text, jobName, thread, user, logLevel, locationInfo);
  prefix->text[sizeof(prefix->text) - 1] = ' ';
}

void zssFormatter(LoggingContext *context, LoggingComponent *component, char* path, int line, int level, uint64 compID, void *data, char *formatString, va_list argList) {
  char messageBuffer[PREFIXED_LINE_MAX_MSG_LENGTH];
  size_t messageLength = vsnprintf(messageBuffer, sizeof(messageBuffer), formatString, argList);

  if (messageLength > sizeof(messageBuffer) - 1) {
    messageLength = sizeof(messageBuffer) - 1;
  }

  LogMessagePrefix prefix;

  char *nextLine = messageBuffer;
  char *lastLine = messageBuffer + messageLength;
  for (int lineIdx = 0; lineIdx < PREFIXED_LINE_MAX_COUNT; lineIdx++) {
    if (nextLine >= lastLine) {
      break;
    }
    char *endOfLine = strchr(nextLine, '\n');
    size_t nextLineLength = endOfLine ? (endOfLine - nextLine) : (lastLine - nextLine);
    memset(prefix.text, ' ', sizeof(prefix.text));
    if (lineIdx == 0) {
      initLogMessagePrefix(&prefix, component, path, line, level, compID);
    }
    printf("%.*s%.*s\n", sizeof(prefix.text), prefix.text, nextLineLength, nextLine);
    nextLine += (nextLineLength + 1);
  }

}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

