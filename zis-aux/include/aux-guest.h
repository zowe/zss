#ifndef SRC_AUX_GUEST_H_
#define SRC_AUX_GUEST_H_

#ifndef _LP64
#error LP64 is supported only
#endif

#include "zowetypes.h"
#include "crossmemory.h"

#include "aux-utils.h"

#define RC_ZISAUX_GUEST_OK  0

struct ZISAUXContext_tag;

typedef int ZISAUXInitFunction(struct ZISAUXContext_tag *context,
                               void *moduleData, int traceLevel);

typedef int ZISAUXTermFunction(struct ZISAUXContext_tag *context,
                               void *moduleData, int traceLevel);

typedef int ZISAUXCancelHandlerFunction(struct ZISAUXContext_tag *context,
                                        void *moduleData, int traceLevel);

typedef int ZISAUXCommandHandlerFunction(struct ZISAUXContext_tag *context,
                                         const ZISAUXCommand *rawCommand,
                                         const CMSModifyCommand *parsedCommand,
                                         ZISAUXCommandReponse *response,
                                         int traceLevel);

typedef int ZISAUXWorkHandlerFunction(struct ZISAUXContext_tag *context,
                                      const ZISAUXRequestPayload *requestPayload,
                                      ZISAUXRequestResponse *response,
                                      int traceLevel);

ZOWE_PRAGMA_PACK

typedef struct ZISAUXModuleDescriptor_tag {

  char eyecatcher[8];
#define ZISAUX_HOST_DESCRIPTOR_EYE  "ZISAUXD "
  int version;
#define ZISAUX_HOST_DESCRIPTOR_VERSION 1
  int key : 8;
  unsigned int subpool : 8;
  unsigned short size;
  char reserved[12];

  int moduleVersion;
  ZISAUXInitFunction *init;
  ZISAUXTermFunction *term;
  ZISAUXWorkHandlerFunction *handleWork;
  ZISAUXCancelHandlerFunction *handleCancel;
  ZISAUXCommandHandlerFunction *handleCommand;
  void *moduleData;

} ZISAUXModuleDescriptor;

ZOWE_PRAGMA_PACK_RESET

typedef ZISAUXModuleDescriptor *(ZISAUXGetModuleDescriptor)(void);


#ifndef __LONGNAME__

#define zisAUXMakeModuleDescriptor ZISAMMDR
#define zisAUXDestroyModuleDescriptor ZISADMDR

#endif

ZISAUXModuleDescriptor *
zisAUXMakeModuleDescriptor(ZISAUXInitFunction *initFunction,
                           ZISAUXTermFunction *termFunction,
                           ZISAUXCancelHandlerFunction *handleCancelFunciton,
                           ZISAUXCommandHandlerFunction *handleCommandFunction,
                           ZISAUXWorkHandlerFunction *handleWorkFunction,
                           void *moduleData, int moduleVersion);

void zisAUXDestroyModuleDescriptor(ZISAUXModuleDescriptor *descriptor);

#endif /* SRC_AUX_GUEST_H_ */
