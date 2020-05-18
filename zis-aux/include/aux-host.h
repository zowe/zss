#ifndef SRC_AUX_HOST_H_
#define SRC_AUX_HOST_H_

#include "collections.h"
#include "crossmemory.h"
#include "resmgr.h"
#include "zis/parm.h"
#include "zis/version.h"

#include "aux-manager.h"
#include "aux-utils.h"

#ifndef _LP64
#error LP64 is supported only
#endif

#ifndef METTLE
#error Non-metal C code is not supported
#endif

#define ZISAUX_MAJOR_VERSION        ZIS_MAJOR_VERSION
#define ZISAUX_MINOR_VERSION        ZIS_MINOR_VERSION
#define ZISAUX_REVISION             ZIS_REVISION
#define ZISAUX_VERSION_DATE_STAMP   ZIS_VERSION_DATE_STAMP

typedef struct ZISAUXContext_tag {

  char eyecatcher[8];
#define ZISAUX_CONTEXT_EYE    "ZISAUXC "
  int version;
#define ZISAUX_CONTEXT_VERSION 1
  int key : 8;
  unsigned int subpool : 8;
  unsigned short size;
  unsigned int flags;
#define ZISAUX_CONTEXT_FLAG_TERM_COMMAND_RECEIVED     0x00000001
#define ZISAUX_CONTEXT_FLAG_GUEST_INITIALIZED         0x00000002
  char reserved0[12];

  struct STCBase_tag *base;
  ResourceManagerHandle taskResManager;
  char reservred1[2];
  ZISAUXCommArea *masterCommArea;

  Queue *availablePETs;
  PET *requestPETs;

  CMSWTORouteInfo startRouteInfo;
  CMSWTORouteInfo termRouteInfo;

  ZISParmSet *parms;

  struct ZISAUXModuleDescriptor_tag *userModuleDescriptor;
  void *moduleData;

} ZISAUXContext;

#define ZISAUX_PE_OK                    0
#define ZISAUX_PE_RC_SCHEDULE_ERROR     8
#define ZISAUX_PE_RC_TERM               9
#define ZISAUX_PE_RC_CANCEL             10
#define ZISAUX_PE_RC_WORKER_ABEND       11
#define ZISAUX_PE_RC_ENV_ERROR          12
#define ZISAUX_PE_RC_HOST_ABEND         13

#define ZISAUX_ABEND_CC_PAUSE_FAILED  200

#define ZISAUX_CALLER_KEY 4

typedef struct ZISAUXCommServiceParmList_tag {
  char eyecatcher[8];
#define ZISAUX_COMM_SERVICE_PARM_EYE  "ZISAUXCM"
  uint16_t version;
#define ZISAUX_COMM_SERVICE_PARM_VERSION  1
  uint16_t function;
#define ZISAUX_COMM_SERVICE_FUNC_COMMAND  1
#define ZISAUX_COMM_SERVICE_FUNC_WORK     2
#define ZISAUX_COMM_SERVICE_FUNC_TERM     3
  char reserved[4];
  ZISAUXRequestPayload payload;
  ZISAUXRequestResponse response;
  int auxRC;
  int auxRSN;
  int traceLevel;
} ZISAUXCommServiceParmList;

#endif /* SRC_AUX_HOST_H_ */
