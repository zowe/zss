

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
#include "zssLogging.h"
#include "timeutls.h"
#include "zos.h"
#include "openprims.h"
#include "sys/time.h"
#include "time.h"

bool isLogLevelValid(int level) {

  if (level < ZOWE_LOG_SEVERE || level > ZOWE_LOG_DEBUG3) {
    return FALSE;
  }

  return TRUE;
}

LoggingContext *zoweLoggingContext = NULL;

static LoggingComponentTable *makeComponentTable(int componentCount) {
  int tableSize = sizeof(LoggingComponentTable) + componentCount * sizeof(LoggingComponent);
  LoggingComponentTable *table = (LoggingComponentTable *)safeMalloc(tableSize, "LoggingComponentTable");
  memset(table, 0, tableSize);
  memcpy(table->eyecatcher, "RSLOGCTB", sizeof(table->eyecatcher));
  table->componentCount = componentCount;
  return table;
}

static LoggingZoweAnchor *makeZoweAnchor() {
  LoggingZoweAnchor *anchor = (LoggingZoweAnchor *)safeMalloc(sizeof(LoggingZoweAnchor), "LoggingZoweAnchor");
  memset(anchor, 0, sizeof(LoggingZoweAnchor));
  memcpy(anchor->eyecatcher, "RSLOGRSA", sizeof(anchor->eyecatcher));
  memcpy(anchor->topLevelComponentTable.eyecatcher, "RSLOGCTB", sizeof(anchor->topLevelComponentTable.eyecatcher));
  anchor->topLevelComponentTable.componentCount = 1;
  anchor->topLevelComponent.subcomponents = makeComponentTable(LOG_DEFAULT_COMPONENT_COUNT);
  return anchor;
}

static LoggingContext *makeZoweLoggingContext() {
	LoggingContext *zoweContext = (LoggingContext *)safeMalloc(sizeof(LoggingContext),"LoggingContext");
	memcpy(zoweContext->eyecatcher, "ZOWECNTX", sizeof(zoweContext->eyecatcher));
	zoweContext->vendorTable = htCreate(LOG_VENDOR_HT_BACKBONE_SIZE, NULL, NULL, NULL, NULL);
	zoweContext->zoweAnchor = makeZoweAnchor();
	return zoweContext;
}

LoggingContext *getZoweLoggingContext() {
	if (zoweLoggingContext == NULL) {
		zoweLoggingContext = makeZoweLoggingContext();
	}
	return zoweLoggingContext;
}

static void getLocationData(char *path, int line, char **locationInfo, uint64 compID, LoggingComponent *component) {

  char prefix[PREFIX_SUFFIX_SIZE];
  char suffix[PREFIX_SUFFIX_SIZE];

  unsigned int id = compID & 0xFFFFFFFF;
  if(compID >= LOG_PROD_COMMON && compID < LOG_PROD_ZIS) {
    memcpy(prefix, LOG_PREFIX_ZCC, sizeof(LOG_PREFIX_ZCC));
    switch (id) { // most of these don't actally log, but are defined in logging.h
      case LOG_COMP_ALLOC:
        memcpy(suffix, LOG_COMP_TEXT_ALLOC, sizeof(LOG_COMP_TEXT_ALLOC));
        break;
      case LOG_COMP_UTILS:
        memcpy(suffix, LOG_COMP_TEXT_UTILS, sizeof(LOG_COMP_TEXT_UTILS));
        break;
      case LOG_COMP_COLLECTIONS:
        memcpy(suffix, LOG_COMP_TEXT_COLLECTIONS, sizeof(LOG_COMP_TEXT_COLLECTIONS));
        break;
      case LOG_COMP_SERIALIZATION:
        memcpy(suffix, LOG_COMP_TEXT_SERIALIZATION, sizeof(LOG_COMP_TEXT_SERIALIZATION));
        break;
      case LOG_COMP_ZLPARSER:
        memcpy(suffix, LOG_COMP_TEXT_ZLPARSER, sizeof(LOG_COMP_TEXT_ZLPARSER));
        break;
      case LOG_COMP_ZLCOMPILER:
        memcpy(suffix, LOG_COMP_TEXT_ZLCOMPILER, sizeof(LOG_COMP_TEXT_ZLCOMPILER));
        break;
      case LOG_COMP_ZLRUNTIME:
        memcpy(suffix, LOG_COMP_TEXT_ZLRUNTIME, sizeof(LOG_COMP_TEXT_ZLRUNTIME));
        break;
      case LOG_COMP_STCBASE:
        memcpy(suffix, LOG_COMP_TEXT_STCBASE, sizeof(LOG_COMP_TEXT_STCBASE));
        break;
      case LOG_COMP_HTTPSERVER:
        memcpy(suffix, LOG_COMP_TEXT_HTTPSERVER, sizeof(LOG_COMP_TEXT_HTTPSERVER));
        break;
      case LOG_COMP_DISCOVERY:
        memcpy(suffix, LOG_COMP_TEXT_DISCOVERY, sizeof(LOG_COMP_TEXT_DISCOVERY));
        break;
      case LOG_COMP_DATASERVICE:
        memcpy(suffix, LOG_COMP_TEXT_DATASERVICE, sizeof(LOG_COMP_TEXT_DATASERVICE));
        break;
      case LOG_COMP_CMS:
        memcpy(suffix, LOG_COMP_TEXT_CMS, sizeof(LOG_COMP_TEXT_CMS));
        break;
      case LOG_COMP_LPA:
        memcpy(suffix, LOG_COMP_TEXT_LPA, sizeof(LOG_COMP_TEXT_LPA));
        break;
      case LOG_COMP_RESTDATASET:
        memcpy(suffix, LOG_COMP_TEXT_RESTDATASET, sizeof(LOG_COMP_TEXT_RESTDATASET));
        break;
      case LOG_COMP_RESTFILE:
        memcpy(suffix, LOG_COMP_TEXT_RESTFILE, sizeof(LOG_COMP_TEXT_RESTFILE));
        break;
    }
  } else if (compID >= LOG_PROD_ZIS && compID < LOG_PROD_ZSS) {
    memcpy(prefix, LOG_PREFIX_ZIS, sizeof(LOG_PREFIX_ZIS));
  } else if (compID >= LOG_PROD_ZSS && compID < LOG_PROD_PLUGINS ) {
    memcpy(prefix, LOG_PREFIX_ZSS, sizeof(LOG_PREFIX_ZSS));
    switch (id) {
      case LOG_COMP_ID_ZSS:
        memcpy(suffix, LOG_COMP_ID_TEXT_ZSS, sizeof(LOG_COMP_ID_TEXT_ZSS));
        break;
      case LOG_COMP_ID_CTDS:
        memcpy(suffix, LOG_COMP_ID_TEXT_CTDS, sizeof(LOG_COMP_ID_TEXT_CTDS));
        break;
      case LOG_COMP_ID_SECURITY:
        memcpy(suffix, LOG_COMP_ID_TEXT_SECURITY, sizeof(LOG_COMP_ID_TEXT_SECURITY));
        break;
      case LOG_COMP_ID_UNIXFILE:
        memcpy(suffix, LOG_COMP_ID_TEXT_UNIXFILE, sizeof(LOG_COMP_ID_TEXT_UNIXFILE));
        break;
    }
  } else {
    //Do nothing? Leave Prefix/Suffix blank? or fill Suffix with component->name if exists?
  }

  char *filename;
  if (strrchr(path, '/') != NULL) {
    filename = strrchr(path, '/'); // returns a pointer to the last occurence of '/'
    filename+=1;
  } else if (path != NULL) {
    filename = path;
  } else {
    //What should happen if path is null?
  }
  //               formatting + prefix + suffix + filename + line number
  int locationSize =  LOCATION_PREFIX_PADDING  + strlen(prefix) + strlen(suffix) + strlen(filename) + LOCATION_SUFFIX_PADDING;
  *locationInfo = (char*) safeMalloc(locationSize, "locationInfo");
  snprintf(*locationInfo, locationSize, "(%s:%s,%s:%d)", prefix, suffix, filename, line);
}

static void initZssLogMessagePrefix(LogMessagePrefix *prefix, LoggingComponent *component, char* path, int line, int level, uint64 compID) {
  ASCB *ascb = getASCB();
  char *jobName = getASCBJobname(ascb);

  ACEE *acee;
  acee = getAddressSpaceAcee();
  char user[USER_SIZE] = { 0 };
  snprintf(user,sizeof(user), acee->aceeuser+1);
  pthread_t threadID = pthread_self();
  char thread[THREAD_SIZE];
  snprintf(thread,sizeof(thread), "%d", threadID);
  char logLevel[LOG_LEVEL_MAX_SIZE];

  switch(level) {
    case LOG_LEVEL_ID_SEVERE:
      memcpy(logLevel, LOG_LEVEL_SEVERE, sizeof(LOG_LEVEL_SEVERE));
      break;
    case LOG_LEVEL_ID_WARN:
      memcpy(logLevel, LOG_LEVEL_WARN, sizeof(LOG_LEVEL_WARN));
      break;
    case LOG_LEVEL_ID_INFO:
      memcpy(logLevel, LOG_LEVEL_INFO, sizeof(LOG_LEVEL_INFO));
      break;
    case LOG_LEVEL_ID_DEBUG:
      memcpy(logLevel, LOG_LEVEL_DEBUG, sizeof(LOG_LEVEL_DEBUG));
      break;
    case LOG_LEVEL_ID_DEBUG2:
      memcpy(logLevel, LOG_LEVEL_DEBUG, sizeof(LOG_LEVEL_DEBUG));
      break;
    case LOG_LEVEL_ID_TRACE:
      memcpy(logLevel, LOG_LEVEL_TRACE, sizeof(LOG_LEVEL_TRACE));
      break;
  }

  char *locationInfo; // largest possible variation in size
  getLocationData(path, line, &locationInfo, compID, component); // location info is allocated in getLocationData
  struct timeval tv;
  struct tm tm;
  gettimeofday(&tv, NULL);
  localtime_r(&tv.tv_sec, &tm);
  static const size_t ISO8601_LEN = 30;
  char datetime[ISO8601_LEN];
  char timestamp[ISO8601_LEN]; // ISO-8601 timestamp
  strftime(datetime, sizeof datetime, "%Y-%m-%d %H:%M:%S", &tm);
  snprintf(timestamp, sizeof timestamp, "%s.%d", datetime, tv.tv_usec);
  snprintf(prefix->text, sizeof(prefix->text), "%22.22s <%8.8s:%s> %s %s %s ", timestamp, jobName, thread, user, logLevel, locationInfo);
  free(locationInfo);
}


void zssFormatter(LoggingContext *context, LoggingComponent *component, void *data, char *formatString, va_list argList) {
  char messageBuffer[PREFIXED_LINE_MAX_MSG_LENGTH];
  size_t messageLength = vsnprintf(messageBuffer, sizeof(messageBuffer), formatString, argList);

  if (messageLength > sizeof(messageBuffer) - 1) {
    messageLength = sizeof(messageBuffer) - 1;
  }

  LogMessagePrefix prefix = {0};
  LoggingInfo *info = (struct LoggingInfo_tag*)data;
  char *path = info->path;
  int level = info->level;
  int line = info->line;
  uint64 compID = info->compID;
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

