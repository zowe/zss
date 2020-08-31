

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

  char prefix[PREFIX_SUFFIX_SIZE]; 
  char suffix[PREFIX_SUFFIX_SIZE]; 

  unsigned int id = compID & 0xFFFFFFFF;
  if(compID >= LOG_PROD_COMMON && compID < LOG_PROD_ZIS) {
    snprintf(prefix,sizeof(LOG_PREFIX_ZCC),LOG_PREFIX_ZCC);
    switch (id) { // most of these don't actally log, but are defined in logging.h
      case LOG_COMP_ALLOC: snprintf(suffix,sizeof(LOG_COMP_TEXT_ALLOC),LOG_COMP_TEXT_ALLOC);
                    break;
      case LOG_COMP_UTILS: snprintf(suffix,sizeof(LOG_COMP_TEXT_UTILS),LOG_COMP_TEXT_UTILS);
                    break;
      case LOG_COMP_COLLECTIONS: snprintf(suffix,sizeof(LOG_COMP_TEXT_COLLECTIONS),LOG_COMP_TEXT_COLLECTIONS);    
                    break;
      case LOG_COMP_SERIALIZATION: snprintf(suffix,sizeof(LOG_COMP_TEXT_SERIALIZATION),LOG_COMP_TEXT_SERIALIZATION);
                    break;
      case LOG_COMP_ZLPARSER: snprintf(suffix,sizeof(LOG_COMP_TEXT_ZLPARSER),LOG_COMP_TEXT_ZLPARSER);
                    break;
      case LOG_COMP_ZLCOMPILER: snprintf(suffix,sizeof(LOG_COMP_TEXT_ZLCOMPILER),LOG_COMP_TEXT_ZLCOMPILER);
                    break;
      case LOG_COMP_ZLRUNTIME: snprintf(suffix,sizeof(LOG_COMP_TEXT_ZLRUNTIME),LOG_COMP_TEXT_ZLRUNTIME); 
                    break;
      case LOG_COMP_STCBASE: snprintf(suffix,sizeof(LOG_COMP_TEXT_STCBASE),LOG_COMP_TEXT_STCBASE);
                    break;
      case LOG_COMP_HTTPSERVER: snprintf(suffix,sizeof(LOG_COMP_TEXT_HTTPSERVER),LOG_COMP_TEXT_HTTPSERVER);
                    break;
      case LOG_COMP_DISCOVERY: snprintf(suffix,sizeof(LOG_COMP_TEXT_DISCOVERY),LOG_COMP_TEXT_DISCOVERY);
                    break;
      case LOG_COMP_DATASERVICE: snprintf(suffix,sizeof(LOG_COMP_TEXT_DATASERVICE),LOG_COMP_TEXT_DATASERVICE);
                    break;
      case LOG_COMP_CMS: snprintf(suffix,sizeof(LOG_COMP_TEXT_CMS),LOG_COMP_TEXT_CMS);
                    break;
      case 0xC0001: snprintf(suffix,sizeof(LOG_COMP_TEXT_CMS),LOG_COMP_TEXT_CMS);
                    break;
      case 0xC0002: snprintf(suffix,6,"cmspc");
                    break;
      case LOG_COMP_LPA: snprintf(suffix,sizeof(LOG_COMP_TEXT_LPA),LOG_COMP_TEXT_LPA);
                    break;
      case LOG_COMP_RESTDATASET: snprintf(suffix,sizeof(LOG_COMP_TEXT_RESTDATASET),LOG_COMP_TEXT_RESTDATASET);
                    break;
      case LOG_COMP_RESTFILE: snprintf(suffix,sizeof(LOG_COMP_TEXT_RESTFILE),LOG_COMP_TEXT_RESTFILE);
                    break;
    }
  }
  else if (compID >= LOG_PROD_ZIS && compID < LOG_PROD_ZSS) {
    snprintf(prefix,sizeof(LOG_PREFIX_ZIS),LOG_PREFIX_ZIS);
  }
  else if (compID >= LOG_PROD_ZSS && compID < LOG_PROD_PLUGINS ) {
    snprintf(prefix,sizeof(LOG_PREFIX_ZSS),LOG_PREFIX_ZSS); 
    switch (id) {
      case LOG_COMP_ID_ZSS: snprintf(suffix,sizeof(LOG_COMP_ID_TEXT_ZSS),LOG_COMP_ID_TEXT_ZSS);
                    break;
      case LOG_COMP_ID_CTDS: snprintf(suffix,sizeof(LOG_COMP_ID_TEXT_CTDS),LOG_COMP_ID_TEXT_CTDS);
                    break;
      case LOG_COMP_ID_SECURITY: snprintf(suffix,sizeof(LOG_COMP_ID_TEXT_SECURITY),LOG_COMP_ID_TEXT_SECURITY);
                    break;
      case LOG_COMP_ID_UNIXFILE: snprintf(suffix,sizeof(LOG_COMP_ID_TEXT_UNIXFILE), LOG_COMP_ID_TEXT_UNIXFILE);
                    break;
    }
  }
  else if (compID > LOG_PROD_PLUGINS) {
    snprintf(suffix,sizeof(component->name),"%s",(strrchr(component->name, '/')+1)); // given more characters than it writes
    snprintf(prefix,sizeof(component->name)-sizeof(suffix),"%s",component->name);
  }

  char *filename;
  filename = strrchr(path, '/'); // returns a pointer to the last occurence of '/'
  filename+=1; 
  //               formatting + prefix + suffix + filename + line number
  int locationSize =  LOCATION_PREFIX_PADDING  + strlen(prefix) + strlen(suffix) + strlen(filename) + LOCATION_SUFFIX_PADDING; 
  *locationInfo = (char*) safeMalloc(locationSize,"locationInfo");
  snprintf(*locationInfo,locationSize,"(%s:%s,%s:%d)",prefix,suffix,filename,line); 
}

static void initZssLogMessagePrefix(LogMessagePrefix *prefix, LoggingComponent *component, char* path, int line, int level, uint64 compID) {
  LogTimestamp currentTime;
  getCurrentLogTimestamp(&currentTime);
  ASCB *ascb = getASCB();
  char *jobName = getASCBJobname(ascb);

  ACEE *acee;
  acee = getAddressSpaceAcee();
  char user[USER_SIZE] = { 0 };
  snprintf(user,sizeof(user),acee->aceeuser+1);
  pthread_t threadID = pthread_self();
  char thread[THREAD_SIZE];
  snprintf(thread,sizeof(thread),"%d",threadID);
  char logLevel[LOG_LEVEL_MAX_SIZE];
  
  switch(level) {
    case LOG_LEVEL_ID_SEVERE: snprintf(logLevel,sizeof(LOG_LEVEL_SEVERE),LOG_LEVEL_SEVERE);
            break;
    case LOG_LEVEL_ID_WARN: snprintf(logLevel,sizeof(LOG_LEVEL_WARN),LOG_LEVEL_WARN);
            break;
    case LOG_LEVEL_ID_INFO: snprintf(logLevel,sizeof(LOG_LEVEL_INFO),LOG_LEVEL_INFO); 
            break;
    case LOG_LEVEL_ID_DEBUG: snprintf(logLevel,sizeof(LOG_LEVEL_DEBUG),LOG_LEVEL_DEBUG);
            break;
    case LOG_LEVEL_ID_DEBUG2: snprintf(logLevel,sizeof(LOG_LEVEL_DEBUG),LOG_LEVEL_DEBUG);
            break;
    case LOG_LEVEL_ID_TRACE: snprintf(logLevel,sizeof(LOG_LEVEL_TRACE),LOG_LEVEL_TRACE);
            break;
  }
  
  char *locationInfo; // largest possible variation in size
  getLocationData(path,line,&locationInfo,compID,component); // location info is allocated in getLocationData
  
  snprintf(prefix->text, sizeof(prefix->text), "%22.22s <%8.8s:%s> %s %s %s ", currentTime.text, jobName, thread, user, logLevel, locationInfo);
  prefix->text[sizeof(prefix->text) - 1] = ' ';
}

void zssFormatter(LoggingContext *context, LoggingComponent *component, char* path, int line, int level, uint64 compID, void *data, char *formatString, va_list argList) {
  char messageBuffer[PREFIXED_LINE_MAX_MSG_LENGTH];
  size_t messageLength = vsnprintf(messageBuffer, sizeof(messageBuffer), formatString, argList);

  if (messageLength > sizeof(messageBuffer) - 1) {
    messageLength = sizeof(messageBuffer) - 1;
  }

  LogMessagePrefix prefix = {0};

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
      initZssLogMessagePrefix(&prefix, component, path, line, level, compID);
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

