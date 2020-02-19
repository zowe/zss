#ifndef SRC_AUX_UTILS_H_
#define SRC_AUX_UTILS_H_

#include <metal/metal.h>
#include <metal/stdint.h>

#include "zowetypes.h"
#include "crossmemory.h"

#ifndef _LP64
#error LP64 is supported only
#endif

#ifndef METTLE
#error Non-metal C code is not supported
#endif

ZOWE_PRAGMA_PACK

typedef struct ZISAUXCommand_tag {
  CMSWTORouteInfo routeInfo;
  uint8_t length;
  char text[126];
} ZISAUXCommand;

typedef struct ZISAUXCommandReponse_tag {
  union {
    CMSModifyCommandStatus status;
    /* CMSModifyCommandStatus may be any size from 1 to 4 bytes, use a union to
     * ensure we have enough room and padding */
    char padding[4];
  };
  uint8_t length;
  char text[251];
} ZISAUXCommandReponse;

typedef struct ZISAUXRequestPayload_tag {
  uint64_t length;
  char inlineData[256];
  void *data;
} ZISAUXRequestPayload;

typedef struct ZISAUXRequestResponse_tag {
  uint64_t length;
  char inlineData[256];
  void *data;
} ZISAUXRequestResponse;


ZOWE_PRAGMA_PACK_RESET

#define AUXUTILS_FIELD_SIZE(struct_name, field) \
  (sizeof(((struct_name *)0)->field))

/* TODO:
 *
 * Below are various util functions (often copied from other
 * sources where they are private) which must eventually go to either
 * zowe-common-c, zss or other places.
 *
 */

#include "zowetypes.h"
#include "crossmemory.h"
#include "logging.h"
#include "utils.h"

#ifndef __LONGNAME__

#define auxutilPrintWithPrefix AUXUPPFX
#define auxutilDumpWithEmptyMessageID AUXUDMSG

#define auxutilTokenizeString AUXUTKNS

#define auxutilGetTCBKey AUXUGTCB

#define auxutilGetCallersKey AUXUGCK

#define auxutilWait AUXUWT
#define auxutilPost AUXUPST

#define auxutilSleep AUXUSLP

#endif

/* Logging fucntion */

void auxutilPrintWithPrefix(LoggingContext *context, LoggingComponent *component,
                            void *data, char *formatString, va_list argList);
char *auxutilDumpWithEmptyMessageID(char *workBuffer, int workBufferSize,
                                    void *data, int dataSize, int lineNumber);

/* Misc string util functions for... utils.c */
int auxutilTokenizeString(ShortLivedHeap *slh,
                          const char *str, unsigned length, char delim,
                          char ***result, unsigned *tokenCount);

#define GET_MAIN_FUNCION_PARMS() \
({ \
  unsigned int r1; \
  __asm(" ST    1,%0 "  : "=m"(r1) : : "r1"); \
  ZISMainFunctionParms * __ptr32 parms = NULL; \
  if (r1 != 0) { \
    parms = (ZISMainFunctionParms *)((*(unsigned int *)r1) & 0x7FFFFFFF); \
  } \
  parms; \
})


/* Misc zos.c related data and functions */

typedef int ECB;

int auxutilGetTCBKey(void);

int auxutilGetCallersKey(void);

void auxutilWait(int32_t * __ptr32 ecb);
void auxutilPost(int32_t * __ptr32 ecb, int code);

void auxutilSleep(int seconds);

#endif /* SRC_AUX_UTILS_H_ */
