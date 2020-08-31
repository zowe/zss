

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stdbool.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include "metalio.h"
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "printables_for_dump.h"
#include "zos.h"
#include "utils.h"

#include "aux-utils.h"

#include "zis/message.h"

#ifndef _LP64
#error LP64 is supported only
#endif

#ifndef METTLE
#error Non-metal C code is not supported
#endif

/* TODO:
 *
 * This is a source file for various util functions (often copied from other
 * sources where they are private).
 *
 * All the content must eventually go to either zowe-common-c, zss or other
 * places.
 *
 */

ZOWE_PRAGMA_PACK

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

#define LOG_MSG_PREFIX_SIZE 57

typedef struct LogMessagePrefix_tag {
  char text[LOG_MSG_PREFIX_SIZE];
} LogMessagePrefix;

typedef struct STCKCONVOutput_tag {
  unsigned char hour;
  unsigned char minute;
  unsigned char second;
  unsigned int  x10      : 4;
  unsigned int  x100     : 4;
  unsigned int  x1000    : 4;
  unsigned int  x10000   : 4;
  unsigned int  x100000  : 4;
  unsigned int  x1000000 : 4;
  char filler1[2];
  unsigned char year[2];
  unsigned char month;
  unsigned char day;
  char filler2[4];
} STCKCONVOutput;

ZOWE_PRAGMA_PACK_RESET

static void stckToLogTimestamp(uint64 stck, LogTimestamp *timestamp) {

  memset(timestamp->text, ' ', sizeof(timestamp->text));

  ALLOC_STRUCT31(
    STRUCT31_NAME(parms31),
    STRUCT31_FIELDS(
      uint64 stck;
      char stckconvParmList[256];
      STCKCONVOutput stckconvResult;
      int stckconvRC;
    )
  );
  parms31->stck = stck;
  memset(parms31->stckconvParmList, 0, sizeof(parms31->stckconvParmList));

  __asm(
      ASM_PREFIX

#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif

      "         PUSH USING                                                     \n"
      "         DROP                                                           \n"
      "STCKBGN  LARL  15,STCKBGN                                               \n"
      "         USING STCKBGN,15                                               \n"
      "         LA    1,0                                                      \n"
      "         SAR   1,1                                                      \n"
      "         STCKCONV STCKVAL=(%1)"
      ",CONVVAL=(%2)"
      ",TIMETYPE=DEC"
      ",DATETYPE=YYYYMMDD"
      ",MF=(E,(%3))                                                            \n"
      "         ST 15,%0                                                       \n"
      "         POP USING                                                      \n"

#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      : "=m"(parms31->stckconvRC)
      : "r"(&parms31->stck), "r"(&parms31->stckconvResult), "r"(&parms31->stckconvParmList)
      : "r0", "r1", "r14", "r15"
  );

  if (parms31->stckconvRC == 0) {

    /* packed to zoned */
    timestamp->dateAndTime.year[0]        = (parms31->stckconvResult.year[0]   >> 4) | 0xF0;
    timestamp->dateAndTime.year[1]        = (parms31->stckconvResult.year[0]       ) | 0xF0;
    timestamp->dateAndTime.year[2]        = (parms31->stckconvResult.year[1]   >> 4) | 0xF0;
    timestamp->dateAndTime.year[3]        = (parms31->stckconvResult.year[1]       ) | 0xF0;

    timestamp->dateAndTime.delim00[0]     = '/';

    timestamp->dateAndTime.month[0]       = (parms31->stckconvResult.month     >> 4) | 0xF0;
    timestamp->dateAndTime.month[1]       = (parms31->stckconvResult.month         ) | 0xF0;

    timestamp->dateAndTime.delim01[0]     = '/';

    timestamp->dateAndTime.day[0]         = (parms31->stckconvResult.day       >> 4) | 0xF0;
    timestamp->dateAndTime.day[1]         = (parms31->stckconvResult.day           ) | 0xF0;

    timestamp->dateAndTime.delim02[0]     = '-';

    timestamp->dateAndTime.hour[0]        = (parms31->stckconvResult.hour      >> 4) | 0xF0;
    timestamp->dateAndTime.hour[1]        = (parms31->stckconvResult.hour          ) | 0xF0;

    timestamp->dateAndTime.delim03[0]     = ':';

    timestamp->dateAndTime.minute[0]      = (parms31->stckconvResult.minute    >> 4) | 0xF0;
    timestamp->dateAndTime.minute[1]      = (parms31->stckconvResult.minute        ) | 0xF0;

    timestamp->dateAndTime.delim04[0]     = ':';

    timestamp->dateAndTime.second[0]      = (parms31->stckconvResult.second    >> 4) | 0xF0;
    timestamp->dateAndTime.second[1]      = (parms31->stckconvResult.second        ) | 0xF0;

    timestamp->dateAndTime.delim05[0]     = '.';

    timestamp->dateAndTime.hundredth[0]   = ((char)parms31->stckconvResult.x10     ) | 0xF0;
    timestamp->dateAndTime.hundredth[1]   = ((char)parms31->stckconvResult.x100    ) | 0xF0;

  }

  FREE_STRUCT31(
    STRUCT31_NAME(parms31)
  );

}

static void getSTCK(uint64 *stckValue) {
  __asm(" STCK 0(%0)" : : "r"(stckValue));
}

static int64 getLocalTimeOffset() {
  CVT * __ptr32 cvt = *(void * __ptr32 * __ptr32)0x10;
  void * __ptr32 cvtext2 = cvt->cvtext2;
  int64 *cvtldto = (int64 * __ptr32)(cvtext2 + 0x38);
  return *cvtldto;
}

static void getCurrentLogTimestamp(LogTimestamp *timestamp) {

  uint64 stck = 0;
  getSTCK(&stck);

  stck += getLocalTimeOffset();

  stckToLogTimestamp(stck, timestamp);

}

static void initLogMessagePrefix(LogMessagePrefix *prefix) {
  LogTimestamp currentTime;
  getCurrentLogTimestamp(&currentTime);
  ASCB *ascb = getASCB();
  char *jobName = getASCBJobname(ascb);
  TCB *tcb = getTCB();
  snprintf(prefix->text, sizeof(prefix->text), "%22.22s %8.8s %08X(%04X) %08X  ",
           currentTime.text, jobName, ascb, ascb->ascbasid, tcb);
  prefix->text[sizeof(prefix->text) - 1] = ' ';
}

#define PREFIXED_LINE_MAX_COUNT         1000
#define PREFIXED_LINE_MAX_MSG_LENGTH    4096

void auxutilPrintWithPrefix(LoggingContext *context, LoggingComponent *component, char* path, int line, int level, uint64 compID,
                     void *data, char *formatString, va_list argList) {

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
      initLogMessagePrefix(&prefix);
    }
    printf("%.*s%.*s\n", sizeof(prefix.text), prefix.text, nextLineLength, nextLine);
    nextLine += (nextLineLength + 1);
  }

}

#define MESSAGE_ID_SHIFT  10

#define DUMP_MSG_ID ZIS_LOG_DUMP_MSG_ID

char *auxutilDumpWithEmptyMessageID(char *workBuffer, int workBufferSize,
                             void *data, int dataSize, int lineNumber) {

  int formatWidth = 16;
  int index = lineNumber * formatWidth;
  int length = dataSize;
  int lastIndex = length - 1;
  const unsigned char *tranlationTable = printableEBCDIC;

  memset(workBuffer, ' ', MESSAGE_ID_SHIFT);
  memcpy(workBuffer, DUMP_MSG_ID, strlen(DUMP_MSG_ID));
  workBuffer += MESSAGE_ID_SHIFT;
  if (index <= lastIndex){
    int pos;
    int linePos = 0;
    linePos = hexFill(workBuffer, linePos, 0, 8, 2, index);
    for (pos = 0; pos < formatWidth; pos++){
      if (((index + pos) % 4 == 0) && ((index + pos) % formatWidth != 0)){
        if ((index + pos) % 16 == 0){
          workBuffer[linePos++] = ' ';
        }
        workBuffer[linePos++] = ' ';
      }
      if ((index + pos) < length){
        linePos = hexFill(workBuffer, linePos, 0, 2, 0, (0xFF & ((char *) data)[index + pos]));
      } else {
        linePos += sprintf(workBuffer + linePos, "  ");
      }
    }
    linePos += sprintf(workBuffer + linePos, " |");
    for (pos = 0; pos < formatWidth && (index + pos) < length; pos++){
      int ch = tranlationTable[0xFF & ((char *) data)[index + pos]];
      workBuffer[linePos++] = ch;
    }
    workBuffer[linePos++] = '|';
    workBuffer[linePos++] = 0;
    return workBuffer - MESSAGE_ID_SHIFT;
  }

  return NULL;
}

int auxutilTokenizeString(ShortLivedHeap *slh,
                   const char *str, unsigned length, char delim,
                   char ***result, unsigned *tokenCount) {

  unsigned resultCapacity = 4;
  char **tokens = (char **)SLHAlloc(slh, sizeof(char *) * resultCapacity);
  if (tokens == NULL) {
    return -1;
  }

  unsigned currTokenCount = 0;
  for (unsigned charIdx = 0; charIdx < length; charIdx++) {

    unsigned tokenStartIdx = 0;
    for (; charIdx < length; charIdx++) {
      if (str[charIdx] != delim) {
        tokenStartIdx = charIdx;
        break;
      }
    }

    unsigned tokenLength = 0;

    for (; charIdx < length; charIdx++) {
      if (str[charIdx] == delim) {
        break;
      }
      tokenLength++;
    }

    if (tokenLength > 0) {

      if (currTokenCount >= resultCapacity) {
        unsigned newCapacity = resultCapacity * 2;
        char **newTokens = (char **)SLHAlloc(slh, sizeof(char *) * newCapacity);
        if (newTokens == NULL) {
          return -1;
        }
        memcpy(newTokens, tokens, sizeof(char *) * resultCapacity);
        resultCapacity = newCapacity;
        tokens = newTokens;
      }

      tokens[currTokenCount] = SLHAlloc(slh, tokenLength + 1);
      if (tokens[currTokenCount] == NULL) {
        return -1;
      }

      memcpy(tokens[currTokenCount], &str[tokenStartIdx], tokenLength);
      tokens[currTokenCount][tokenLength] = '\0';
      currTokenCount++;

    }

  }

  *result = tokens;
  *tokenCount = currTokenCount;
  return 0;
}

int auxutilGetTCBKey(void) {
  TCB *tcb = getTCB();
  int key = ((int)tcb->tcbpkf >> 4) & 0x0000000F;
  return key;
}

int auxutilGetCallersKey(void) {

  int key = 0;
  __asm(
      ASM_PREFIX
      "         LA    7,1                                                      \n"
      "         ESTA  6,7                                                      \n"
      "         SRL   6,20                                                     \n"
      "         N     6,=X'0000000F'                                           \n"
      "         ST    6,%0                                                     \n"
      : "=m"(key)
      :
      : "r6", "r7"
  );

  return key;
}

void auxutilWait(ECB * __ptr32 ecb) {
  __asm(
      ASM_PREFIX
      "       WAIT  1,ECB=(%0)                                                 \n"
      :
      : "r"(ecb)
      : "r0", "r1", "r14", "r15"
  );
}


void auxutilPost(int32_t * __ptr32 ecb, int code) {
  __asm(
      ASM_PREFIX
      "         POST (%0),(%1),LINKAGE=SYSTEM                                  \n"
      :
      : "r"(ecb), "r"(code)
      : "r0", "r1", "r14", "r15"
  );
}

void auxutilSleep(int seconds) {
  int waitValue = seconds * 100;
  __asm(
      ASM_PREFIX
      "       STIMER WAIT,BINTVL=%[interval]                                   \n"
      :
      : [interval]"m"(waitValue)
      : "r0", "r1", "r14", "r15"
  );
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
