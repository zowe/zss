

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
#include <metal/stdlib.h>
#include <metal/string.h>
#else
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#endif

#include "zowetypes.h"
#include "cmutils.h"
#include "crossmemory.h"
#include "zis/client.h"
#include "zis/service.h"
#include "zis/server.h"
#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif

CrossMemoryServerName zisGetDefaultServerName() {
  return CMS_DEFAULT_SERVER_NAME;
}

int zisCopyDataFromAddressSpace(const CrossMemoryServerName *serverName,
                                void *dest, void *src, unsigned int size,
                                int srcKey, ASCB *ascb,
                                ZISCopyServiceStatus *status) {

  SnarferServiceParmList parmList = {
      .eyecatcher = ZIS_SNARFER_SERVICE_PARMLIST_EYECATCHER,
      .destinationAddress = (uint64)dest,
      .sourceAddress = (uint64)src,
      .sourceASCB = ascb,
      .sourceKey = srcKey,
      .size = size
  };
  int rc = RC_ZIS_SRVC_OK;

  /* since AMODE31 callers can assume that it's safe to pass addresses
   * with the 32nd bit on, we need to take care of that, because our code
   * operates in AMODE64 */
#ifndef _LP64
  parmList.destinationAddress &= 0x000000007FFFFFFFLLU;
  parmList.sourceAddress &= 0x000000007FFFFFFFLLU;
#endif

  const CrossMemoryServerName *localServerName = serverName ? serverName : &CMS_DEFAULT_SERVER_NAME;

  int serviceRC = 0;
  int cmsRC = cmsCallService(localServerName, ZIS_SERVICE_ID_SNARFER_SRV, &parmList, &serviceRC);

  if (cmsRC != RC_CMS_OK) {
    rc = RC_ZIS_SRVC_CMS_FAILED;
    status->baseStatus.cmsRC = cmsRC;
  } else if (serviceRC != RC_ZIS_SNRFSRV_OK) {
    rc = RC_ZIS_SRVC_SERVICE_FAILED;
    status->baseStatus.serviceRC = serviceRC;
  }

  return rc;
}

static int authRequest(const CrossMemoryServerName *serverName,
                       AuthServiceParmList *parmList,
                       ZISAuthServiceStatus *status) {
  int authServiceRC = 0;
  int rc = RC_ZIS_SRVC_OK;
  const CrossMemoryServerName *localServerName = serverName ? serverName :
      &CMS_DEFAULT_SERVER_NAME;
  int cmsRC = cmsCallService(localServerName, ZIS_SERVICE_ID_AUTH_SRV, parmList,
      &authServiceRC);
  if (cmsRC != RC_CMS_OK) {
    rc = RC_ZIS_SRVC_CMS_FAILED;
    status->baseStatus.cmsRC = cmsRC;
  } else if (authServiceRC != RC_ZIS_AUTHSRV_OK) {
    rc = RC_ZIS_SRVC_SERVICE_FAILED;
    status->baseStatus.serviceRC = authServiceRC;
    switch (authServiceRC) {
    case RC_ZIS_AUTHSRV_SAF_ABENDED:
      status->abendInfo = parmList->abendInfo;
      break;
    case RC_ZIS_AUTHSRV_DELETE_FAILED:
    case RC_ZIS_AUTHSRV_CREATE_FAILED:
    case RC_ZIS_AUTHSRV_SAF_ERROR:
      status->safStatus = parmList->safStatus;
      break;
    }
  }
  return rc;
}

/*
 *   safIdt - a buffer for SAF IDT token. The buffer must be ZIS_AUTH_SERVICE_SAFIDT_LENGTH + 1 bytes long.
 *   appl - application name to be included in the audience claim of SAF IDT. It is an optional parameter and can be null.
 */
int zisGenerateOrValidateSafIdtWithAppl(const CrossMemoryServerName *serverName,
                                const char *userName, const char *password,
                                const char *appl,
                                char *safIdt,
                                ZISAuthServiceStatus *status) {
  AuthServiceParmList parmList = {0};

  memcpy(&parmList.eyecatcher[0], ZIS_AUTH_SERVICE_PARMLIST_EYECATCHER,
      sizeof(parmList.eyecatcher));
  parmList.fc = ZIS_AUTH_SERVICE_PARMLIST_FC_GENERATE_TOKEN;

  if (strlen(userName) >= sizeof (parmList.userIDNullTerm)) {
    status->baseStatus.serviceRC = RC_ZIS_AUTHSRV_INPUT_STRING_TOO_LONG;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  }
  strncpy(parmList.userIDNullTerm, userName, sizeof(parmList.userIDNullTerm));

  if (strlen(password) >= sizeof (parmList.passwordNullTerm)) {
    status->baseStatus.serviceRC = RC_ZIS_AUTHSRV_INPUT_STRING_TOO_LONG;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  }
  strncpy(parmList.passwordNullTerm, password, sizeof(parmList.passwordNullTerm));

  if (appl != NULL && strlen(appl) != 0) {
    if (strlen(appl) >= sizeof (parmList.safIdtService.applNullTerm)) {
      status->baseStatus.serviceRC = RC_ZIS_AUTHSRV_INPUT_STRING_TOO_LONG;
      return RC_ZIS_SRVC_SERVICE_FAILED;
    }
    strcpy(parmList.safIdtService.applNullTerm, appl);

    parmList.safIdtService.options |= ZIS_AUTH_SERVICE_SAFIDT_OPTION_IDT_APPL;
  }

  parmList.safIdtService.safIdtLen = strlen(safIdt);
  parmList.safIdtService.safIdtServiceVersion = 1;
  if (strlen(safIdt) >= sizeof(parmList.safIdtService.safIdt)) {
    status->baseStatus.serviceRC = RC_ZIS_AUTHSRV_INPUT_STRING_TOO_LONG;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  }
  memcpy((void *)parmList.safIdtService.safIdt, safIdt, strlen(safIdt));

  int rc = authRequest(serverName, &parmList, status);

  if (parmList.safIdtService.safIdtLen > 0) {
    memset(safIdt, 0, ZIS_AUTH_SERVICE_SAFIDT_LENGTH + 1);
    memcpy(safIdt, (void *)parmList.safIdtService.safIdt, parmList.safIdtService.safIdtLen);
  }

  return rc;
}

/*
 *   safIdt - a buffer for SAF IDT token. The buffer must be ZIS_AUTH_SERVICE_SAFIDT_LENGTH + 1 bytes long.
 */
int zisGenerateOrValidateSafIdt(const CrossMemoryServerName *serverName,
                                const char *userName, const char *password,
                                char *safIdt,
                                ZISAuthServiceStatus *status) {

    return zisGenerateOrValidateSafIdtWithAppl(serverName,
                                               userName,
                                               password,
                                               NULL,
                                               safIdt,
                                               status
                                               );
}

int zisCheckUsernameAndPassword(const CrossMemoryServerName *serverName,
                                const char *userName, const char *password,
                                ZISAuthServiceStatus *status) {
  AuthServiceParmList parmList = {0};

  memcpy(&parmList.eyecatcher[0], ZIS_AUTH_SERVICE_PARMLIST_EYECATCHER,
      sizeof(parmList.eyecatcher));
  parmList.fc = ZIS_AUTH_SERVICE_PARMLIST_FC_VERIFY_PASSWORD;
  if (strlen(userName) >= sizeof (parmList.userIDNullTerm)) {
    status->baseStatus.serviceRC = RC_ZIS_AUTHSRV_INPUT_STRING_TOO_LONG;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  }
  strncpy(parmList.userIDNullTerm, userName, sizeof(parmList.userIDNullTerm));
  if (strlen(password) >= sizeof (parmList.passwordNullTerm)) {
    status->baseStatus.serviceRC = RC_ZIS_AUTHSRV_INPUT_STRING_TOO_LONG;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  }
  strncpy(parmList.passwordNullTerm, password, sizeof(parmList.passwordNullTerm));
  return authRequest(serverName, &parmList, status);
}

int zisCheckEntity(const CrossMemoryServerName *serverName,
                   const char *userName, const char *class, const char *entity,
                   int access, ZISAuthServiceStatus *status) {
  AuthServiceParmList parmList = {0};

  memcpy(&parmList.eyecatcher[0], ZIS_AUTH_SERVICE_PARMLIST_EYECATCHER,
      sizeof(parmList.eyecatcher));
  parmList.fc = ZIS_AUTH_SERVICE_PARMLIST_FC_ENTITY_CHECK;
  parmList.access = access;
  if (strlen(userName) >= sizeof (parmList.userIDNullTerm)) {
    status->baseStatus.serviceRC = RC_ZIS_AUTHSRV_INPUT_STRING_TOO_LONG;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  }
  strncpy(parmList.userIDNullTerm, userName, sizeof(parmList.userIDNullTerm));
  if (strlen(class) >= sizeof (parmList.classNullTerm)) {
    status->baseStatus.serviceRC = RC_ZIS_AUTHSRV_INPUT_STRING_TOO_LONG;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  }
  strncpy(parmList.classNullTerm, class, sizeof(parmList.classNullTerm));
  size_t entityLen = strlen(entity);
  if (entityLen == 0) {
    status->baseStatus.serviceRC = RC_ZIS_AUTHSRV_INPUT_STRING_TOO_SHORT;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  }
  if (entityLen >= sizeof (parmList.entityNullTerm)) {
    status->baseStatus.serviceRC = RC_ZIS_AUTHSRV_INPUT_STRING_TOO_LONG;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  }
  strncpy(parmList.entityNullTerm, entity, sizeof(parmList.entityNullTerm));
  return authRequest(serverName, &parmList, status);
}


int zisGetSAFAccessLevel(const CrossMemoryServerName *serverName,
                         const char *id,
                         const char *class,
                         const char *entity,
                         ZISSAFAccessLevel *accessLevel,
                         ZISAuthServiceStatus *status,
                         int traceLevel) {

  AuthServiceParmList parmList = {0};

  memcpy(&parmList.eyecatcher[0], ZIS_AUTH_SERVICE_PARMLIST_EYECATCHER,
         sizeof(parmList.eyecatcher));
  parmList.fc = ZIS_AUTH_SERVICE_PARMLIST_FC_GET_ACCESS;
  parmList.access = ZIS_SAF_ACCESS_NONE;
  parmList.traceLevel = traceLevel;

  if (strlen(id) >= sizeof (parmList.userIDNullTerm)) {
    status->baseStatus.serviceRC = RC_ZIS_AUTHSRV_INPUT_STRING_TOO_LONG;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  }
  strncpy(parmList.userIDNullTerm, id, sizeof(parmList.userIDNullTerm));

  if (class != NULL) {
    if (strlen(class) >= sizeof (parmList.classNullTerm)) {
      status->baseStatus.serviceRC = RC_ZIS_AUTHSRV_INPUT_STRING_TOO_LONG;
      return RC_ZIS_SRVC_SERVICE_FAILED;
    }
    strncpy(parmList.classNullTerm, class, sizeof(parmList.classNullTerm));
  }

  if (strlen(entity) >= sizeof (parmList.entityNullTerm)) {
    status->baseStatus.serviceRC = RC_ZIS_AUTHSRV_INPUT_STRING_TOO_LONG;
    return RC_ZIS_SRVC_SERVICE_FAILED;;
  }

  strncpy(parmList.entityNullTerm, entity, sizeof(parmList.entityNullTerm));

  int authRC = authRequest(serverName, &parmList, status);

  *accessLevel = parmList.access;
  return authRC;
}


int zisExtractUserProfiles(const CrossMemoryServerName *serverName,
                           const char *startProfile,
                           unsigned int profilesToExtract,
                           ZISUserProfileEntry *resultBuffer,
                           unsigned int *profilesExtracted,
                           ZISUserProfileServiceStatus *status,
                           int traceLevel) {

  ZISUserProfileServiceParmList parmList = {0};

  memcpy(parmList.eyecatcher, ZIS_USERPROF_SERVICE_PARMLIST_EYECATCHER,
         sizeof(parmList.eyecatcher));

  if (startProfile != NULL) {
    size_t profileLength = strlen(startProfile);
    if (profileLength > ZIS_USER_ID_MAX_LENGTH) {
      return RC_ZIS_SRVC_UPRFSRV_USER_ID_TOO_LONG;
    }
    parmList.startUserID.length = profileLength;
    memcpy(parmList.startUserID.value, startProfile, profileLength);
  }

  parmList.profilesToExtract = profilesToExtract;

  if (resultBuffer == NULL) {
    return RC_ZIS_SRVC_UPRFSRV_RESULT_BUFF_NULL;
  }
  parmList.resultBuffer = resultBuffer;

  parmList.traceLevel = traceLevel;

  if (profilesExtracted == NULL) {
    return RC_ZIS_SRVC_UPRFSRV_PROFILE_COUNT_NULL;
  }

  int serviceRC = RC_ZIS_UPRFSRV_OK;
  int cmsCallRC = cmsCallService(
      serverName,
      ZIS_SERVICE_ID_USERPROF_SRV,
      &parmList,
      &serviceRC
  );


  if (cmsCallRC != RC_CMS_OK) {
    status->baseStatus.cmsRC = cmsCallRC;
    return RC_ZIS_SRVC_CMS_FAILED;
  } else if (serviceRC != RC_ZIS_SNRFSRV_OK) {
    status->baseStatus.serviceRC = serviceRC;
    status->internalServiceRC = parmList.internalServiceRC;
    status->internalServiceRSN = parmList.internalServiceRSN;
    status->safStatus = parmList.safStatus;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  } else {
    *profilesExtracted = parmList.profilesExtracted;
  }

  return RC_ZIS_SRVC_OK;
}

int zisExtractGenresProfiles(const CrossMemoryServerName *serverName,
                             const char *class,
                             const char *startProfile,
                             unsigned int profilesToExtract,
                             ZISGenresProfileEntry *resultBuffer,
                             unsigned int *profilesExtracted,
                             ZISGenresProfileServiceStatus *status,
                             int traceLevel) {

  ZISGenresProfileServiceParmList parmList = {0};

  memcpy(parmList.eyecatcher, ZIS_GRESPROF_SERVICE_PARMLIST_EYECATCHER,
         sizeof(parmList.eyecatcher));

  if (class != NULL) {
    size_t classLength = strlen(class);
    if (classLength > ZIS_SECURITY_CLASS_MAX_LENGTH) {
      return RC_ZIS_SRVC_GRPRFSRV_CLASS_TOO_LONG;
    }
    parmList.class.length = classLength;
    memcpy(parmList.class.value, class, classLength);
  }

  if (startProfile != NULL) {
    size_t profileLength = strlen(startProfile);
    if (profileLength > ZIS_SECURITY_PROFILE_MAX_LENGTH) {
      return RC_ZIS_SRVC_GRPRFSRV_PROFILE_TOO_LONG;
    }
    parmList.startProfile.length = profileLength;
    memcpy(parmList.startProfile.value, startProfile, profileLength);
  }

  parmList.profilesToExtract = profilesToExtract;

  if (resultBuffer == NULL) {
    return RC_ZIS_SRVC_GRPRFSRV_RESULT_BUFF_NULL;
  }
  parmList.resultBuffer = resultBuffer;

  parmList.traceLevel = traceLevel;

  if (profilesExtracted == NULL) {
    return RC_ZIS_SRVC_GRPRFSRV_PROFILE_COUNT_NULL;
  }

  int serviceRC = RC_ZIS_UPRFSRV_OK;
  int cmsCallRC = cmsCallService(
      serverName,
      ZIS_SERVICE_ID_GRESPROF_SRV,
      &parmList,
      &serviceRC
  );

  if (cmsCallRC != RC_CMS_OK) {
    status->baseStatus.cmsRC = cmsCallRC;
    return RC_ZIS_SRVC_CMS_FAILED;
  } else if (serviceRC != RC_ZIS_SNRFSRV_OK) {
    status->baseStatus.serviceRC = serviceRC;
    status->internalServiceRC = parmList.internalServiceRC;
    status->internalServiceRSN = parmList.internalServiceRSN;
    status->safStatus = parmList.safStatus;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  } else {
    *profilesExtracted = parmList.profilesExtracted;
  }

  return RC_ZIS_SRVC_OK;

}

int zisExtractGenresAccessList(const CrossMemoryServerName *serverName,
                               const char *class,
                               const char *profile,
                               ZISGenresAccessEntry *resultBuffer,
                               unsigned int resultBufferCapacity,
                               unsigned int *entriesExtracted,
                               ZISGenresAccessListServiceStatus *status,
                               int traceLevel) {

  ZISGenresAccessListServiceParmList parmList = {0};

  memcpy(parmList.eyecatcher, ZIS_ACSLIST_SERVICE_PARMLIST_EYECATCHER,
         sizeof(parmList.eyecatcher));

  if (class != NULL) {
    size_t classLength = strlen(class);
    if (classLength > ZIS_SECURITY_CLASS_MAX_LENGTH) {
      return RC_ZIS_SRVC_ACSLSRV_CLASS_TOO_LONG;
    }
    parmList.class.length = classLength;
    memcpy(parmList.class.value, class, classLength);
  }

  if (profile == NULL) {
    return RC_ZIS_SRVC_ACSLSRV_PROFILE_NULL;
  }

  size_t profileLength = strlen(profile);
  if (profileLength > ZIS_SECURITY_PROFILE_MAX_LENGTH) {
    return RC_ZIS_SRVC_ACSLSRV_PROFILE_TOO_LONG;
  }
  parmList.profile.length = profileLength;
  memcpy(parmList.profile.value, profile, profileLength);

  if (resultBuffer == NULL) {
    return RC_ZIS_SRVC_ACSLSRV_RESULT_BUFF_NULL;
  }
  parmList.resultBuffer = resultBuffer;
  parmList.resultBufferCapacity = resultBufferCapacity;

  parmList.traceLevel = traceLevel;

  if (entriesExtracted == NULL) {
    return RC_ZIS_SRVC_ACSLSRV_ENTRY_COUNT_NULL;
  }

  int serviceRC = RC_ZIS_ACSLSRV_OK;
  int cmsCallRC = cmsCallService(
      serverName,
      ZIS_SERVICE_ID_ACSLIST_SRV,
      &parmList,
      &serviceRC
  );

  if (cmsCallRC != RC_CMS_OK) {
    status->baseStatus.cmsRC = cmsCallRC;
    return RC_ZIS_SRVC_CMS_FAILED;
  } else if (serviceRC == RC_ZIS_ACSLSRV_INSUFFICIENT_SPACE) {
    *entriesExtracted = parmList.entriesFound;
    return RC_ZIS_SRVC_ACSLSRV_INSUFFICIENT_BUFFER;
  } else if (serviceRC != RC_ZIS_ACSLSRV_OK) {
    status->baseStatus.serviceRC = serviceRC;
    status->internalServiceRC = parmList.internalServiceRC;
    status->internalServiceRSN = parmList.internalServiceRSN;
    status->safStatus = parmList.safStatus;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  } else {
    *entriesExtracted = parmList.entriesExtracted;
  }

  return RC_ZIS_SRVC_OK;
}


static int handleProfileFunction(
    ZISGenresAdminFunction function,
    const CrossMemoryServerName *serverName,
    const char *userID,
    const char *profileName,
    const char *owner,
    ZISGenresAdminServiceAccessType accessType,
    bool isDryRun,
    ZISGenresAdminServiceMessage *operatorCommand,
    ZISGenresAdminServiceStatus *status,
    int traceLevel
) {

  if (status == NULL) {
    return RC_ZIS_SRVC_STATUS_NULL;
  }

  size_t userIDLength = 0;
  size_t profileLength = 0;
  size_t ownerLength = 0;

  if (userID == NULL) {
    return RC_ZIS_SRVC_PADMIN_USERID_NULL;
  }
  userIDLength = strlen(userID);
  if (userIDLength > ZIS_USER_ID_MAX_LENGTH) {
    return RC_ZIS_SRVC_PADMIN_USERID_TOO_LONG;
  }

  if (profileName == NULL) {
    return RC_ZIS_SRVC_PADMIN_PROFILE_NULL;
  }
  profileLength = strlen(profileName);
  if (profileLength > ZIS_SECURITY_PROFILE_MAX_LENGTH) {
    return RC_ZIS_SRVC_PADMIN_PROFILE_TOO_LONG;
  }

  if (owner != NULL) {
    ownerLength = strlen(owner);
    if (ownerLength > ZIS_USER_ID_MAX_LENGTH) {
      return RC_ZIS_SRVC_PADMIN_OWNER_TOO_LONG;
    }
  }

  if (isDryRun) {
    if (operatorCommand == NULL) {
      return RC_ZIS_SRVC_PADMIN_OPER_CMD_NULL;
    }
  }

  ZISGenresAdminServiceParmList parmList = {0};

  memcpy(parmList.eyecatcher, ZIS_GENRES_ADMIN_SERVICE_PARMLIST_EYECATCHER,
         sizeof(parmList.eyecatcher));
  parmList.flags = isDryRun ? ZIS_GENRES_ADMIN_FLAG_DRYRUN : 0x00;
  parmList.function = function;
  parmList.profile.length = profileLength;
  memcpy(parmList.profile.value, profileName, profileLength);
  parmList.owner.length = ownerLength;
  memcpy(parmList.owner.value, owner, ownerLength);
  parmList.user.length = userIDLength;
  memcpy(parmList.user.value, userID, userIDLength);
  parmList.accessType = accessType;
  parmList.traceLevel = traceLevel;

  int serviceRC = RC_ZIS_GSADMNSRV_OK;
  int cmsRC = cmsCallService(serverName, ZIS_SERVICE_ID_GENRES_ADMIN_SRV,
                             &parmList, &serviceRC);

  if (cmsRC != RC_CMS_OK) {
    status->baseStatus.cmsRC = cmsRC;
    return RC_ZIS_SRVC_CMS_FAILED;
  }

  if (serviceRC != RC_ZIS_GSADMNSRV_OK) {
    status->baseStatus.serviceRC = serviceRC;
    status->internalServiceRC = parmList.internalServiceRC;
    status->internalServiceRSN = parmList.internalServiceRSN;
    status->safStatus = parmList.safStatus;
    status->message = parmList.serviceMessage;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  }

  if (isDryRun) {
    *operatorCommand = parmList.operatorCommand;
  }

  return RC_ZIS_SRVC_OK;
}

int zisDefineProfile(const CrossMemoryServerName *serverName,
                     const char *profileName,
                     const char *owner,
                     bool isDryRun,
                     ZISGenresAdminServiceMessage *operatorCommand,
                     ZISGenresAdminServiceStatus *status,
                     int traceLevel) {

  const char *dummyUserID = "";
  ZISGenresAdminServiceAccessType dummyAccessType =
      ZIS_GENRES_ADMIN_ACESS_TYPE_READ;

  int handleRC = handleProfileFunction(
      ZIS_GENRES_ADMIN_SRV_FC_DEFINE,
      serverName,
      dummyUserID,
      profileName,
      owner,
      dummyAccessType,
      isDryRun,
      operatorCommand,
      status,
      traceLevel
  );

  return handleRC;
}

int zisDeleteProfile(const CrossMemoryServerName *serverName,
                     const char *profileName,
                     bool isDryRun,
                     ZISGenresAdminServiceMessage *operatorCommand,
                     ZISGenresAdminServiceStatus *status,
                     int traceLevel) {

  const char *dummyUserID = "";
  const char *dummyOwner = "";
  ZISGenresAdminServiceAccessType dummyAccessType =
      ZIS_GENRES_ADMIN_ACESS_TYPE_READ;

  int handleRC = handleProfileFunction(
      ZIS_GENRES_ADMIN_SRV_FC_DELETE,
      serverName,
      dummyUserID,
      profileName,
      dummyOwner,
      dummyAccessType,
      isDryRun,
      operatorCommand,
      status,
      traceLevel
  );

  return handleRC;
}

int zisGiveAccessToProfile(const CrossMemoryServerName *serverName,
                           const char *userID,
                           const char *profileName,
                           ZISGenresAdminServiceAccessType accessType,
                           bool isDryRun,
                           ZISGenresAdminServiceMessage *operatorCommand,
                           ZISGenresAdminServiceStatus *status,
                           int traceLevel) {

  const char *dummyOwner = "";

  int handleRC = handleProfileFunction(
      ZIS_GENRES_ADMIN_SRV_FC_GIVE_ACCESS,
      serverName,
      userID,
      profileName,
      dummyOwner,
      accessType,
      isDryRun,
      operatorCommand,
      status,
      traceLevel
  );

  return handleRC;
}

int zisRevokeAccessToProfile(const CrossMemoryServerName *serverName,
                            const char *userID,
                            const char *profileName,
                            bool isDryRun,
                            ZISGenresAdminServiceMessage *operatorCommand,
                            ZISGenresAdminServiceStatus *status,
                            int traceLevel) {

  const char *dummyOwner = "";
  ZISGenresAdminServiceAccessType dummyAccessType =
      ZIS_GENRES_ADMIN_ACESS_TYPE_READ;

  int handleRC = handleProfileFunction(
      ZIS_GENRES_ADMIN_SRV_FC_REVOKE_ACCESS,
      serverName,
      userID,
      profileName,
      dummyOwner,
      dummyAccessType,
      isDryRun,
      operatorCommand,
      status,
      traceLevel
  );

  return handleRC;
}

int zisExtractGroupProfiles(const CrossMemoryServerName *serverName,
                            const char *startGroup,
                            unsigned int profilesToExtract,
                            ZISGroupProfileEntry *resultBuffer,
                            unsigned int *profilesExtracted,
                            ZISGroupProfileServiceStatus *status,
                            int traceLevel) {

  ZISGroupProfileServiceParmList parmList = {0};

  memcpy(parmList.eyecatcher, ZIS_GRPPROF_SERVICE_PARMLIST_EYECATCHER,
         sizeof(parmList.eyecatcher));

  if (startGroup != NULL) {
    size_t profileLength = strlen(startGroup);
    if (profileLength > ZIS_SECURITY_GROUP_MAX_LENGTH) {
      return RC_ZIS_SRVC_GPPRFSRV_GROUP_TOO_LONG;
    }
    parmList.startGroup.length = profileLength;
    memcpy(parmList.startGroup.value, startGroup, profileLength);
  }

  parmList.profilesToExtract = profilesToExtract;

  if (resultBuffer == NULL) {
    return RC_ZIS_SRVC_GPPRFSRV_RESULT_BUFF_NULL;
  }
  parmList.resultBuffer = resultBuffer;

  parmList.traceLevel = traceLevel;

  if (profilesExtracted == NULL) {
    return RC_ZIS_SRVC_GPPRFSRV_PROFILE_COUNT_NULL;
  }

  int serviceRC = RC_ZIS_UPRFSRV_OK;
  int cmsCallRC = cmsCallService(
      serverName,
      ZIS_SERVICE_ID_GRPPROF_SRV,
      &parmList,
      &serviceRC
  );

  if (cmsCallRC != RC_CMS_OK) {
    status->baseStatus.cmsRC = cmsCallRC;
    return RC_ZIS_SRVC_CMS_FAILED;
  } else if (serviceRC != RC_ZIS_SNRFSRV_OK) {
    status->baseStatus.serviceRC = serviceRC;
    status->internalServiceRC = parmList.internalServiceRC;
    status->internalServiceRSN = parmList.internalServiceRSN;
    status->safStatus = parmList.safStatus;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  } else {
    *profilesExtracted = parmList.profilesExtracted;
  }

  return RC_ZIS_SRVC_OK;

}

int zisExtractGroupAccessList(const CrossMemoryServerName *serverName,
                              const char *group,
                              ZISGroupAccessEntry *resultBuffer,
                              unsigned int resultBufferCapacity,
                              unsigned int *entriesExtracted,
                              ZISGroupAccessListServiceStatus *status,
                              int traceLevel) {

  ZISGroupAccessListServiceParmList parmList = {0};

  memcpy(parmList.eyecatcher, ZIS_GRPALST_SERVICE_PARMLIST_EYECATCHER,
         sizeof(parmList.eyecatcher));

  if (group == NULL) {
    return RC_ZIS_SRVC_GRPALSRV_GROUP_NULL;
  }

  size_t profileLength = strlen(group);
  if (profileLength > ZIS_SECURITY_GROUP_MAX_LENGTH) {
    return RC_ZIS_SRVC_GRPALSRV_GROUP_TOO_LONG;
  }
  parmList.group.length = profileLength;
  memcpy(parmList.group.value, group, profileLength);

  if (resultBuffer == NULL) {
    return RC_ZIS_SRVC_GRPALSRV_RESULT_BUFF_NULL;
  }
  parmList.resultBuffer = resultBuffer;
  parmList.resultBufferCapacity = resultBufferCapacity;

  parmList.traceLevel = traceLevel;

  if (entriesExtracted == NULL) {
    return RC_ZIS_SRVC_GRPALSRV_ENTRY_COUNT_NULL;
  }

  int serviceRC = RC_ZIS_GRPALSRV_OK;
  int cmsCallRC = cmsCallService(
      serverName,
      ZIS_SERVICE_ID_GRPALIST_SRV,
      &parmList,
      &serviceRC
  );

  if (cmsCallRC != RC_CMS_OK) {
    status->baseStatus.cmsRC = cmsCallRC;
    return RC_ZIS_SRVC_CMS_FAILED;
  } else if (serviceRC == RC_ZIS_GRPALSRV_INSUFFICIENT_SPACE) {
    *entriesExtracted = parmList.entriesFound;
    return RC_ZIS_SRVC_GRPALSRV_INSUFFICIENT_BUFFER;
  } else if (serviceRC != RC_ZIS_GRPALSRV_OK) {
    status->baseStatus.serviceRC = serviceRC;
    status->internalServiceRC = parmList.internalServiceRC;
    status->internalServiceRSN = parmList.internalServiceRSN;
    status->safStatus = parmList.safStatus;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  } else {
    *entriesExtracted = parmList.entriesExtracted;
  }

  return RC_ZIS_SRVC_OK;
}

static int handleGroupAdminFunction(
    ZISGroupAdminFunction function,
    const CrossMemoryServerName *serverName,
    const char *group,
    const char *superiorGroup,
    const char *userID,
    ZISGroupAdminServiceAccessType accessType,
    bool isDryRun,
    ZISGroupAdminServiceMessage *operatorCommand,
    ZISGroupAdminServiceStatus *status,
    int traceLevel
) {

  if (status == NULL) {
    return RC_ZIS_SRVC_STATUS_NULL;
  }

  size_t groupLength = 0;
  size_t superiorGroupLength = 0;
  size_t userIDLength = 0;

  if (group == NULL) {
    return RC_ZIS_SRVC_GADMIN_GROUP_NULL;
  }
  groupLength = strlen(group);
  if (groupLength > ZIS_SECURITY_GROUP_MAX_LENGTH) {
    return RC_ZIS_SRVC_GADMIN_GROUP_TOO_LONG;
  }

  if (superiorGroup == NULL) {
    return RC_ZIS_SRVC_GADMIN_GROUP_NULL;
  }
  superiorGroupLength = strlen(superiorGroup);
  if (superiorGroupLength > ZIS_SECURITY_GROUP_MAX_LENGTH) {
    return RC_ZIS_SRVC_GADMIN_SUPGROUP_TOO_LONG;
  }

  if (userID == NULL) {
    return RC_ZIS_SRVC_GADMIN_USERID_NULL;
  }
  userIDLength = strlen(userID);
  if (userIDLength > ZIS_USER_ID_MAX_LENGTH) {
    return RC_ZIS_SRVC_GADMIN_USERID_TOO_LONG;
  }

  if (isDryRun) {
    if (operatorCommand == NULL) {
      return RC_ZIS_SRVC_GADMIN_OPER_CMD_NULL;
    }
  }

  ZISGroupAdminServiceParmList parmList = {0};

  memcpy(parmList.eyecatcher, ZIS_GROUP_ADMIN_SERVICE_PARMLIST_EYECATCHER,
         sizeof(parmList.eyecatcher));
  parmList.flags = isDryRun ? ZIS_GROUP_ADMIN_FLAG_DRYRUN : 0x00;
  parmList.function = function;
  parmList.group.length = groupLength;
  memcpy(parmList.group.value, group, groupLength);
  parmList.superiorGroup.length = superiorGroupLength;
  memcpy(parmList.superiorGroup.value, superiorGroup, superiorGroupLength);
  parmList.user.length = userIDLength;
  memcpy(parmList.user.value, userID, userIDLength);
  parmList.accessType = accessType;
  parmList.traceLevel = traceLevel;

  int serviceRC = RC_ZIS_GRPASRV_OK;
  int cmsRC = cmsCallService(serverName, ZIS_SERVICE_ID_GROUP_ADMIN_SRV,
                             &parmList, &serviceRC);

  if (cmsRC != RC_CMS_OK) {
    status->baseStatus.cmsRC = cmsRC;
    return RC_ZIS_SRVC_CMS_FAILED;
  }

  if (serviceRC != RC_ZIS_GRPASRV_OK) {
    status->baseStatus.serviceRC = serviceRC;
    status->internalServiceRC = parmList.internalServiceRC;
    status->internalServiceRSN = parmList.internalServiceRSN;
    status->safStatus = parmList.safStatus;
    status->message = parmList.serviceMessage;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  }

  if (isDryRun) {
    *operatorCommand = parmList.operatorCommand;
  }

  return RC_ZIS_SRVC_OK;
}

int zisAddGroup(const CrossMemoryServerName *serverName,
                const char *groupName,
                const char *superiorGroupName,
                bool isDryRun,
                ZISGroupAdminServiceMessage *operatorCommand,
                ZISGroupAdminServiceStatus *status,
                int traceLevel) {

  const char *dummyUserID = "";
  ZISGroupAdminServiceAccessType dummyAccessType =
      ZIS_GROUP_ADMIN_ACESS_TYPE_UNKNOWN;

  int handleRC = handleGroupAdminFunction(
      ZIS_GROUP_ADMIN_SRV_FC_ADD,
      serverName,
      groupName,
      superiorGroupName,
      dummyUserID,
      dummyAccessType,
      isDryRun,
      operatorCommand,
      status,
      traceLevel
  );

  return handleRC;
}

int zisDeleteGroup(const CrossMemoryServerName *serverName,
                   const char *groupName,
                   bool isDryRun,
                   ZISGroupAdminServiceMessage *operatorCommand,
                   ZISGroupAdminServiceStatus *status,
                   int traceLevel) {

  const char *dummySuperiorGroup = "";
  const char *dummyUserID = "";
  ZISGroupAdminServiceAccessType dummyAccessType =
      ZIS_GROUP_ADMIN_ACESS_TYPE_UNKNOWN;

  int handleRC = handleGroupAdminFunction(
      ZIS_GROUP_ADMIN_SRV_FC_DELETE,
      serverName,
      groupName,
      dummySuperiorGroup,
      dummyUserID,
      dummyAccessType,
      isDryRun,
      operatorCommand,
      status,
      traceLevel
  );

  return handleRC;
}

int zisConnectToGroup(const CrossMemoryServerName *serverName,
                      const char *userID,
                      const char *groupName,
                      ZISGroupAdminServiceAccessType accessType,
                      bool isDryRun,
                      ZISGroupAdminServiceMessage *operatorCommand,
                      ZISGroupAdminServiceStatus *status,
                      int traceLevel) {

  const char *dummySuperiorGroup = "";

  int handleRC = handleGroupAdminFunction(
      ZIS_GROUP_ADMIN_SRV_FC_CONNECT_USER,
      serverName,
      groupName,
      dummySuperiorGroup,
      userID,
      accessType,
      isDryRun,
      operatorCommand,
      status,
      traceLevel
  );

  return handleRC;
}

int zisRemoveFromGroup(const CrossMemoryServerName *serverName,
                       const char *userID,
                       const char *groupName,
                       bool isDryRun,
                       ZISGroupAdminServiceMessage *operatorCommand,
                       ZISGroupAdminServiceStatus *status,
                       int traceLevel) {

  const char *dummySuperiorGroup = "";
  ZISGroupAdminServiceAccessType dummyAccessType =
      ZIS_GROUP_ADMIN_ACESS_TYPE_UNKNOWN;

  int handleRC = handleGroupAdminFunction(
      ZIS_GROUP_ADMIN_SRV_FC_REMOVE_USER,
      serverName,
      groupName,
      dummySuperiorGroup,
      userID,
      dummyAccessType,
      isDryRun,
      operatorCommand,
      status,
      traceLevel
  );

  return handleRC;
}


int zisCallNWMService(const CrossMemoryServerName *serverName,
                      ZISNWMJobName jobName,
                      char *nmiBuffer,
                      unsigned int nmiBufferSize,
                      ZISNWMServiceStatus *status,
                      int traceLevel) {

  if (status == NULL) {
    return RC_ZIS_SRVC_STATUS_NULL;
  }

  if (nmiBuffer == NULL) {
    return RC_ZIS_SRVC_NWM_BUFFER_NULL;
  }

  ZISNWMServiceParmList parmList = {0};

  memcpy(&parmList.eyecatcher[0], ZIS_NWM_SERVICE_PARMLIST_EYECATCHER,
         sizeof(parmList.eyecatcher));
  memcpy(parmList.nmiJobName, jobName.value, sizeof(parmList.nmiJobName));
  parmList.nmiBuffer = nmiBuffer;
  parmList.nmiBufferSize = nmiBufferSize;
  parmList.traceLevel = traceLevel;

  int serviceRC = RC_ZIS_NWMSRV_OK;
  int cmsRC = cmsCallService(serverName, ZIS_SERVICE_ID_NWM_SRV,
                             &parmList, &serviceRC);

  if (cmsRC != RC_CMS_OK) {
    status->baseStatus.cmsRC = cmsRC;
    return RC_ZIS_SRVC_CMS_FAILED;
  }

  status->nmiReturnValue = parmList.nmiReturnValue;
  status->nmiReturnCode = parmList.nmiReturnCode;
  status->nmiReasonCode = parmList.nmiReasonCode;

  if (serviceRC != RC_ZIS_NWMSRV_OK) {
    status->baseStatus.serviceRC = serviceRC;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  }

  return RC_ZIS_SRVC_OK;
}

static int zisCallServiceInternal(const CrossMemoryServerName *serverName,
                                  const ZISServicePath *path, void *parm,
                                  unsigned int version,
                                  bool noSAFCheck,
                                  ZISServiceStatus *status) {

  if (status == NULL) {
    return RC_ZIS_SRVC_STATUS_NULL;
  }

  CrossMemoryServerGlobalArea *cmsGA = NULL;
  int getGlobalAreaRC = cmsGetGlobalArea(serverName, &cmsGA);
  if (getGlobalAreaRC != RC_CMS_OK) {
    return RC_ZIS_SRVC_GLOBAL_AREA_NULL;
  }

  if (!(cmsGA->serverFlags & CROSS_MEMORY_SERVER_FLAG_READY)) {
    status->cmsRC = RC_CMS_SERVER_NOT_READY;
    return RC_ZIS_SRVC_CMS_FAILED;
  }

 ZISServerAnchor *serverAnchor = cmsGA->userServerAnchor;
 if (serverAnchor == NULL) {
   return RC_ZIS_SRVC_SEVER_ANCHOR_NULL;
 }

 if (serverAnchor->serviceTable == NULL) {
   return RC_ZIS_SRVC_SERVICE_TABLE_NULL;
 }

 ZISServiceAnchor *serviceAnchor =
     crossMemoryMapGet(serverAnchor->serviceTable, path);
  if (serviceAnchor == NULL) {
    return RC_ZIS_SRVC_SERVICE_NOT_FOUND;
  }

  int routerServiceID = -1;
  if (serviceAnchor->flags & ZIS_SERVICE_ANCHOR_FLAG_SPACE_SWITCH) {
    routerServiceID = ZIS_SERVICE_ID_SRVC_ROUTER_SS;
  } else {
    routerServiceID = ZIS_SERVICE_ID_SRVC_ROUTER_CP;
  }

  ZISServiceRouterParm routerParmList = {0};
  memcpy(routerParmList.eyecatcher, ZIS_SERVICE_ROUTER_EYECATCHER,
         sizeof(routerParmList.eyecatcher));
  routerParmList.version = ZIS_SERVICE_ROUTER_VERSION;
  routerParmList.size = sizeof(ZISServiceRouterParm);
  routerParmList.targetServicePath = *path;
  routerParmList.targetServiceParm = parm;
  routerParmList.serviceVersion = version;

  int cmsFlags = CMS_CALL_FLAG_NONE;
  if (noSAFCheck) {
    cmsFlags = CMS_CALL_FLAG_NO_SAF_CHECK;
  }

  int routerRC = 0;
  int cmsRC = cmsCallService3(cmsGA, routerServiceID, &routerParmList,
                              cmsFlags, &routerRC);

  if (cmsRC != RC_CMS_OK) {
    status->cmsRC = cmsRC;
    return RC_ZIS_SRVC_CMS_FAILED;
  }

  if (routerRC == RC_ZIS_SRVC_SPECIFIC_AUTH_FAILED){
    status->serviceRC = routerRC;
    return RC_ZIS_SRVC_SPECIFIC_AUTH_FAILED;
  } else if (routerRC != RC_ZIS_SRVC_OK) {
    status->serviceRC = routerRC;
    return RC_ZIS_SRVC_SERVICE_FAILED;
  }

  return RC_ZIS_SRVC_OK;
}

int zisCallService(const CrossMemoryServerName *serverName,
                   const ZISServicePath *path, void *parm,
                   ZISServiceStatus *status) {

  return zisCallServiceInternal(serverName, path, parm,
                                ZIS_SERVICE_ANY_VERSION,
                                false,
                                status);

}

int zisCallServiceUnchecked(const CrossMemoryServerName *serverName,
                            const ZISServicePath *path, void *parm,
                            ZISServiceStatus *status) {

  return zisCallServiceInternal(serverName, path, parm,
                                ZIS_SERVICE_ANY_VERSION,
                                true,
                                status);
}

int zisCallVersionedService(const CrossMemoryServerName *serverName,
                            const ZISServicePath *path, void *parm,
                            unsigned int version,
                            ZISServiceStatus *status) {

  return zisCallServiceInternal(serverName, path, parm, version, false, status);

}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
