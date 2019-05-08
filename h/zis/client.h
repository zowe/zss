

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef ZIS_CLIENT_H_
#define ZIS_CLIENT_H_

#include "zowetypes.h"
#include "crossmemory.h"
#include "zos.h"

#include "zis/server.h"
#include "zis/service.h"

#include "zis/services/auth.h"
#include "zis/services/nwm.h"
#include "zis/services/snarfer.h"

#ifndef __LONGNAME__

#define zisCallService              ZISCSRVC
#define zisCallVersionedService     ZISCVSVC

#define zisGetDefaultServerName     ZISGDSNM
#define zisCopyDataFromAddressSpace ZISCOPYD
#define zisCheckUsernameAndPassword ZISUNPWD
#define zisCheckEntity              ZISSAUTH
#define zisGetSAFAccessLevel        ZISGETAC
#define zisCallNWMService           ZISCNWMS

#endif

#define NOOP(...)

extern const char
    *ZIS_COPY_RC_DESCRIPTION[RC_ZIS_SNRFSRV_SHARED_OBJ_DETACH_FAILED + 1];

#define FORMAT_RC($rc, $arr) (\
  ($rc >= 0 && $rc < sizeof($arr) / sizeof($arr[0]))? \
      $arr[$rc] : "Unknown return code" \
)

typedef struct ZISServiceStatus_tag {
  int serviceRC;
  int serviceRSN;
  int cmsRC;
  int cmsRSN;
} ZISServiceStatus;

int zisCallService(const CrossMemoryServerName *serverName,
                   const ZISServicePath *path, void *parm,
                   ZISServiceStatus *status);

int zisCallVersionedService(const CrossMemoryServerName *serverName,
                            const ZISServicePath *path, void *parm,
                            unsigned int version,
                            ZISServiceStatus *status);

#define _ZIS_FORMAT_CALL_STATUS_TMPL($rc, $s, $printf, $serviceCases,\
                                     $descriptions) do {\
  switch (($rc)) {\
  case RC_ZIS_SRVC_OK:\
    $printf("Call successful");\
    break;\
  case RC_ZIS_SRVC_CMS_FAILED:\
    $printf("Problem with the cross-memory call, RC %d", ($s)->baseStatus.cmsRC);\
    break;\
  case RC_ZIS_SRVC_SERVICE_FAILED:\
    switch (($s)->baseStatus.serviceRC) {\
    $serviceCases($rc, $s, $printf, $descriptions)\
    default:\
      $printf("Service error: %s", FORMAT_RC(($s)->baseStatus.serviceRC, \
              $descriptions));\
      break;\
    }\
    break;\
  default:\
    $printf("Unknown error code %d", ($rc));\
    break;\
  }\
} while (0)

CrossMemoryServerName zisGetDefaultServerName(void);

typedef struct ZISCopyServiceStatus_tag {
  ZISServiceStatus baseStatus;
} ZISCopyServiceStatus;

#define ZIS_FORMAT_COPY_CALL_STATUS($rc, $s, $printf) \
    _ZIS_FORMAT_CALL_STATUS_TMPL($rc, $s, $printf, NOOP, \
                                 ZIS_COPY_RC_DESCRIPTION)

int zisCopyDataFromAddressSpace(const CrossMemoryServerName *serverName,
                                void *dest, void *src, unsigned int size,
                                int srcKey, ASCB *ascb,
                                ZISCopyServiceStatus *result);

extern const char *ZIS_AUTH_RC_DESCRIPTION[RC_ZIS_AUTHSRV_MAX_RC + 1];

typedef struct ZISAuthServiceStatus_tag {
  ZISServiceStatus baseStatus;
  union {
    SAFAuthStatus safStatus;
    AbendInfo abendInfo;
  };
} ZISAuthServiceStatus;

#define _ZIS_AUTH_SERVICE_ERROR_CASES($rc, $status, $printf, $descriptions) \
  case RC_ZIS_AUTHSRV_SAF_ERROR:\
    FORMAT_SAF_STATUS((&($status)->safStatus), $printf);\
    break;\
  case RC_ZIS_AUTHSRV_SAF_ABENDED:\
    FORMAT_ABEND_INFO((&($status)->abendInfo), $printf);\
    break;\


#define ZIS_FORMAT_AUTH_CALL_STATUS($rc, $status, $printf) \
    _ZIS_FORMAT_CALL_STATUS_TMPL($rc, $status, $printf, \
        _ZIS_AUTH_SERVICE_ERROR_CASES, ZIS_AUTH_RC_DESCRIPTION)

int zisCheckUsernameAndPassword(const CrossMemoryServerName *serverName,
                                const char *userName, const char *password,
                                ZISAuthServiceStatus *status);

int zisCheckEntity(const CrossMemoryServerName *serverName,
                   const char *userName, const char *class, const char *entity,
                   int access, ZISAuthServiceStatus *status);

/* The following enum values must be in sync with RACF RSN in RACROUTE
 * REQUEST=AUTH with SAF RC = 0 and RACF RC = 14 */
typedef enum ZISSAFAccessLevel_tag {
  ZIS_SAF_ACCESS_NONE = 0x00,
  ZIS_SAF_ACCESS_READ = 0x04,
  ZIS_SAF_ACCESS_UPDATE  = 0x08,
  ZIS_SAF_ACCESS_CONTROL = 0x0C,
  ZIS_SAF_ACCESS_ALETER = 0x10,
} ZISSAFAccessLevel;

int zisGetSAFAccessLevel(const CrossMemoryServerName *serverName,
                         const char *id,
                         const char *class,
                         const char *entity,
                         ZISSAFAccessLevel *access,
                         ZISAuthServiceStatus *status,
                         int traceLevel);

typedef struct ZISNWMJobName_tag {
  char value[8];
} ZISNWMJobName;

typedef struct ZISNWMService_tag {
  ZISServiceStatus baseStatus;
  int nmiReturnValue;
  int nmiReturnCode;
  int nmiReasonCode;
} ZISNWMServiceStatus;

int zisCallNWMService(const CrossMemoryServerName *serverName,
                      ZISNWMJobName jobName,
                      char *nmiBuffer,
                      unsigned int nmiBufferSize,
                      ZISNWMServiceStatus *status,
                      int traceLevel);

#define RC_ZIS_SRVC_NWM_BUFFER_NULL               (ZIS_MAX_GEN_SRVC_RC + 1)

#endif /* ZIS_CLIENT_H_ */



/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

