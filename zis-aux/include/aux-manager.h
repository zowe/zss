#ifndef SRC_AUX_MANAGER_H_
#define SRC_AUX_MANAGER_H_

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stdbool.h>
#include <metal/stdint.h>
#else
#include "stdbool.h"
#include "stdint.h"
#endif

#include "zowetypes.h"
#include "collections.h"
#include "pause-element.h"
#include "shrmem64.h"

#include "aux-utils.h"

#ifndef _LP64
#error LP64 is supported only
#endif

#ifndef METTLE
#error Non-metal C code is not supported
#endif

ZOWE_PRAGMA_PACK

#define ZISAUX_MAX_REQUEST_COUNT  16

typedef struct ZISAUXSTCName_tag {
  char name[8];
} ZISAUXSTCName;

typedef struct ZISAUXManager_tag {
  char eyecatcher[8];
#define ZISAUX_MGR_EYECATCHER "ZISAUXM "
  ZISAUXSTCName hostSTC;
  LongHashtable *ascbToComm;
  LongHashtable *nicknameToComm;
  void * __ptr32 managerTCB;
  CPID commPool;
  MemObjToken sharedStorageToken;
} ZISAUXManager;

typedef struct ZISAUXCommArea_tag {
  char eyecatcher[8];
#define ZISAUX_COMM_EYECATCHER    "ZISAUXC "
  int32_t flag;
#define ZISAUX_HOST_FLAG_READY          0x00000001
#define ZISAUX_HOST_FLAG_TERMINATED     0x00000002
  uint32_t pcNumber;
  uint32_t sequenceNumber;
  int32_t termECB;
  uint64_t stoken;
  ASCB * __ptr32 ascb;
  uint16_t parentASID;
  char reserved0[2];
} ZISAUXCommArea;

typedef struct ZISAUXNickname_tag {
  char text[4];
} ZISAUXNickname;

typedef struct ZISAUXModule_tag {
  char text[8];
} ZISAUXModule;

typedef struct ZISAUXParm_tag {
  char value[200];
} ZISAUXParm;

ZOWE_PRAGMA_PACK_RESET

#ifndef __LONGNAME__

#define zisauxMgrInit ZISAMIN
#define zisauxMgrClean ZISAMCL
#define zisauxMgrSetHostSTC ZISAMHS
#define zisauxMgrStartGuest ZISAMST
#define zisauxMgrStopGuest ZISAMSP
#define zisauxMgrInitCommand ZISAICMD
#define zisauxMgrCleanCommand ZISACCMD
#define zisauxMgrInitRequestPayload ZISAIRQP
#define zisauxMgrCleanRequestPayload ZISACRQP
#define zisauxMgrInitRequestResponse ZISAIRSP
#define zisauxMgrCleanRequestResponse ZISACRSP
#define zisauxMgrCopyRequestResponseData ZISADRSP
#define zisauxMgrSendTermSignal ZISAMTM
#define zisauxMgrSendCommand ZISAMCM
#define zisauxMgrSendWork ZISAMWK
#define zisauxMgrWaitForTerm ZISAMWT

#endif

/* Manager init/cleanup functions */

int zisauxMgrInit(ZISAUXManager *mgr, int *reasonCode);
int zisauxMgrClean(ZISAUXManager *mgr, int *reasonCode);

/* Manager configuration functions */

void zisauxMgrSetHostSTC(ZISAUXManager *mgr, ZISAUXSTCName hostSTC);

/* Guest management functions */

int zisauxMgrStartGuest(ZISAUXManager *mgr,
                        ZISAUXNickname guestNickname,
                        ZISAUXModule guestModuleName,
                        const ZISAUXParm *guestParm,
                        int *reasonCode, int traceLevel);

int zisauxMgrStopGuest(ZISAUXManager *mgr,
                       ZISAUXNickname guestNickname,
                       int *reasonCode, int traceLevel);

int zisauxMgrSendTermSignal(ZISAUXManager *mgr,
                            ZISAUXNickname guestNickname,
                            int *reasonCode, int traceLevel);

/* Helper functions for command/request/response init/cleanup */

int zisauxMgrInitCommand(ZISAUXCommand *command,
                         CMSWTORouteInfo routeInfo,
                         const char *commandText);
void zisauxMgrCleanCommand(ZISAUXCommand *command);

int zisauxMgrInitRequestPayload(ZISAUXManager *mgr,
                                ZISAUXRequestPayload *payload,
                                void *requestData, size_t requestDataSize,
                                bool isRequestDataLocal, int *reasonCode);
int zisauxMgrCleanRequestPayload(ZISAUXManager *mgr,
                                 ZISAUXRequestPayload *payload,
                                 int *reasonCode);

int zisauxMgrInitRequestResponse(ZISAUXManager *mgr,
                                 ZISAUXRequestResponse *response,
                                 size_t bufferSize,
                                 int *reasonCode);
int zisauxMgrCleanRequestResponse(ZISAUXManager *mgr,
                                  ZISAUXRequestResponse *response,
                                  int *reasonCode);

int zisauxMgrCopyRequestResponseData(void *dest, size_t destSize, bool isDestLocal,
                                     const ZISAUXRequestResponse *response,
                                     int *reasonCode);

/* Communication functions */

int zisauxMgrSendCommand(ZISAUXManager *mgr,
                         ZISAUXNickname guestNickname,
                         const ZISAUXCommand *command,
                         ZISAUXCommandReponse *response,
                         int *reasonCode, int traceLevel);

int zisauxMgrSendWork(ZISAUXManager *mgr,
                      ZISAUXNickname guestNickname,
                      const ZISAUXRequestPayload *requestPayload,
                      ZISAUXRequestResponse *response,
                      int *reasonCode, int traceLevel);

int zisauxMgrWaitForTerm(ZISAUXManager *mgr,
                         ZISAUXNickname guestNickname,
                         int *reasonCode, int traceLevel);

#define RC_ZISAUX_OK                      0
#define RC_ZISAUX_CMD_REJECTED            4
#define RC_ZISAUX_ERROR                   8
#define RC_ZISAUX_CPOOL_ALLOC_FAILED      9
#define RC_ZISAUX_COMM_NOT_ALLOCATED      10
#define RC_ZISAUX_PRINT_ERROR             11
#define RC_ZISAUX_AS_NOT_STARTED          12
#define RC_ZISAUX_COMM_NOT_FOUND          13
#define RC_ZISAUX_AS_NOT_STOPPED          14
#define RC_ZISAUX_PE_RELEASE_FAILED       15
#define RC_ZISAUX_BAD_EYECATCHER          16
#define RC_ZISAUX_PC_ENV_NOT_ESTABLISHED  17
#define RC_ZISAUX_PC_ABEND_DETECTED       18
#define RC_ZISAUX_PC_RECOVERY_ENV_FAILED  19
#define RC_ZISAUX_PC_ENV_NOT_TERMINATED   20
#define RC_ZISAUX_PC_PARM_INVALID         21
#define RC_ZISAUX_COMM_AREA_INVALID       22
#define RC_ZISAUX_ALLOC_FAILED            23
#define RC_ZISAUX_COMM_SRVC_PARM_INVALID  24
#define RC_ZISAUX_UNSUPPORTED_VERSION     25
#define RC_ZISAUX_REQUEST_LIMIT_REACHED   26
#define RC_ZISAUX_AUX_TERMINATION         27
#define RC_ZISAUX_REQUEST_TCB_ABEND       28
#define RC_ZISAUX_BAD_PET                 29
#define RC_ZISAUX_PAUSE_FAILED            30
#define RC_ZISAUX_AUX_NOT_READY           31
#define RC_ZISAUX_COMMAND_TOO_LONG        32
#define RC_ZISAUX_HANDLER_ERROR           33
#define RC_ZISAUX_USER_HANDLER_ERROR      34
#define RC_ZISAUX_BAD_FUNCTION_CODE       35
#define RC_ZISAUX_HANDLER_NOT_SCHEDULED   36
#define RC_ZISAUX_AUX_STOPPED             37
#define RC_ZISAUX_HOST_ABENDED            38
#define RC_ZISAUX_BAD_CALLER_KEY          39
#define RC_ZISAUX_SHR64_ERROR             40
#define RC_ZISAUX_BUFFER_TOO_SMALL        41
#define RC_ZISAUX_NOT_MANAGER_TCB         42
#define RC_ZISAUX_CALLER_NOT_RECOGNIZED   43

#endif /* SRC_AUX_MANAGER_H_ */
