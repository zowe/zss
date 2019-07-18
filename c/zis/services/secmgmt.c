

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/string.h>

#ifndef METTLE
#error Non-metal C code is not supported
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "crossmemory.h"
#include "radmin.h"
#include "recovery.h"
#include "zos.h"

#include "zis/utils.h"
#include "zis/services/secmgmt.h"
#include "zis/services/secmgmtUtils.h"

int validateUserProfileParmList(ZISUserProfileServiceParmList *parm) {

  if (memcmp(parm->eyecatcher, ZIS_USERPROF_SERVICE_PARMLIST_EYECATCHER,
             sizeof(parm->eyecatcher))) {
    return RC_ZIS_UPRFSRV_BAD_EYECATCHER;
  }

  if (parm->startUserID.length > ZIS_USER_ID_MAX_LENGTH) {
    return RC_ZIS_UPRFSRV_USER_ID_TOO_LONG;
  }

  if (parm->resultBuffer == NULL) {
    return RC_ZIS_UPRFSRV_RESULT_BUFF_NULL;
  }

  return RC_ZIS_UPRFSRV_OK;
}

static void copyUserProfiles(ZISUserProfileEntry *dest,
                             const RadminBasicUserPofileInfo *src,
                             unsigned int count) {
  /* RadminBasicUserPofileInfo and ZISUserProfileEntry have different sizes
   * for the name, so it must be taken into an account when copying to the
   * other address space.
   */
  for (unsigned int i = 0; i < count; i++) {
    ZISUserProfileEntry tmpDest = {0};
    memcpy(tmpDest.userID, src[i].userID, sizeof(src[i].userID));
    memcpy(tmpDest.defaultGroup, src[i].defaultGroup, sizeof(src[i].defaultGroup));
    memcpy(tmpDest.name, src[i].name, sizeof(src[i].name));
    cmCopyToSecondaryWithCallerKey(&dest[i], &tmpDest, sizeof(tmpDest));
  }

}


static int zisUserProfilesServiceFunctionRACF(CrossMemoryServerGlobalArea *globalArea,
                                   CrossMemoryService *service, void *parm) {

  int status = RC_ZIS_UPRFSRV_OK;
  int radminExtractRC = RC_RADMIN_OK;

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_UPRFSRV_PARMLIST_NULL;
  }

  ZISUserProfileServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr,
                                   sizeof(localParmList));

  int parmValidateRC = validateUserProfileParmList(&localParmList);
  if (parmValidateRC != RC_ZIS_UPRFSRV_OK) {
    return parmValidateRC;
  }

  int traceLevel = localParmList.traceLevel;
  if (globalArea->pcLogLevel > traceLevel) {
    traceLevel = globalArea->pcLogLevel;
  }

  do {

    RadminUserID caller;
    if (!secmgmtGetCallerUserID(&caller)) {
      status = RC_ZIS_UPRFSRV_IMPERSONATION_MISSING;
      break;
    }

    CMS_DEBUG2(globalArea, traceLevel, "UPRFSRV: userID = \'%.*s\', "
               "profilesToExtract = %u, result buffer = 0x%p\n",
               localParmList.startUserID.length,
               localParmList.startUserID.value,
               localParmList.profilesToExtract,
               localParmList.resultBuffer);
    CMS_DEBUG2(globalArea, traceLevel, "UPRFSRV: caller = \'%.*s\'\n",
               caller.length, caller.value);

    RadminCallerAuthInfo authInfo = {
        .acee = NULL,
        .userID = caller
    };

    char userIDBuffer[ZIS_USER_ID_MAX_LENGTH + 1] = {0};
    memcpy(userIDBuffer, localParmList.startUserID.value,
           localParmList.startUserID.length);
    const char *startUserIDNullTerm = localParmList.startUserID.length > 0 ?
                                      userIDBuffer : NULL;

    size_t tmpResultBufferSize =
        sizeof(RadminBasicUserPofileInfo) * localParmList.profilesToExtract;
    int allocRC = 0, allocSysRC = 0, allocSysRSN = 0;
    RadminBasicUserPofileInfo *tmpResultBuffer =
        (RadminBasicUserPofileInfo *)safeMalloc64v3(tmpResultBufferSize, TRUE,
                                                    "RadminBasicUserPofileInfo",
                                                    &allocRC,
                                                    &allocSysRSN,
                                                    &allocSysRSN);
    CMS_DEBUG2(globalArea, traceLevel,
               "UPRFSRV: tmpBuff allocated @ 0x%p (%zu %d %d %d)\n",
               tmpResultBuffer, tmpResultBufferSize,
               allocRC, allocSysRC, allocSysRSN);

    if (tmpResultBuffer == NULL) {
      status = RC_ZIS_UPRFSRV_ALLOC_FAILED;
      break;
    }

    RadminStatus radminStatus;
    size_t profilesExtracted = 0;

    radminExtractRC = radminExtractBasicUserProfileInfo(
        authInfo,
        startUserIDNullTerm,
        localParmList.profilesToExtract,
        tmpResultBuffer,
        &profilesExtracted,
        &radminStatus
    );

    if (radminExtractRC == RC_RADMIN_OK) {
      copyUserProfiles(localParmList.resultBuffer, tmpResultBuffer,
                       profilesExtracted);
      localParmList.profilesExtracted = profilesExtracted;
    } else  {
      localParmList.internalServiceRC = radminExtractRC;
      localParmList.internalServiceRSN = radminStatus.reasonCode;
      localParmList.safStatus.safRC = radminStatus.apiStatus.safRC;
      localParmList.safStatus.racfRC = radminStatus.apiStatus.racfRC;
      localParmList.safStatus.racfRSN = radminStatus.apiStatus.racfRSN;
      status = RC_ZIS_UPRFSRV_INTERNAL_SERVICE_FAILED;
    }

    int freeRC = 0, freeSysRC = 0, freeSysRSN = 0;
    freeRC = safeFree64v3(tmpResultBuffer, tmpResultBufferSize,
                          &freeSysRC, &freeSysRSN);
    CMS_DEBUG2(globalArea, traceLevel,
               "UPRFSRV: tmpBuff free'd @ 0x%p (%zu %d %d %d)\n",
               tmpResultBuffer, tmpResultBufferSize,
               freeRC, freeSysRC, freeSysRSN);

    tmpResultBuffer = NULL;

  } while (0);

  cmCopyToSecondaryWithCallerKey(clientParmAddr, &localParmList,
                                 sizeof(ZISUserProfileServiceParmList));

  CMS_DEBUG2(globalArea, traceLevel,
             "UPRFSRV: status = %d, extracted = %d, RC = %d, "
             "internal RC = %d, RSN = %d, "
             "SAF RC = %d, RACF RC = %d, RSN = %d\n",
             status,
             localParmList.profilesExtracted, radminExtractRC,
             localParmList.internalServiceRC,
             localParmList.internalServiceRSN,
             localParmList.safStatus.safRC,
             localParmList.safStatus.racfRC,
             localParmList.safStatus.racfRSN);

  return status;
}

int zisUserProfilesServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                                   CrossMemoryService *service, void *parm) {
  ExternalSecurityManager esm = getExternalSecurityManager();
  switch (esm) {
    case ZOS_ESM_RACF: return zisUserProfilesServiceFunctionRACF(globalArea, service, parm);
    case ZOS_ESM_RTSS: return zisUserProfilesServiceFunctionTSS(globalArea, service, parm);
    default: return RC_ZIS_UPRFSRV_UNSUPPORTED_ESM;
  }
}

static int validateGenresProfileParmList(ZISGenresProfileServiceParmList *parm)
{

  if (memcmp(parm->eyecatcher, ZIS_GRESPROF_SERVICE_PARMLIST_EYECATCHER,
             sizeof(parm->eyecatcher))) {
    return RC_ZIS_GRPRFSRV_BAD_EYECATCHER;
  }

  if (parm->class.length > ZIS_SECURITY_CLASS_MAX_LENGTH) {
    return RC_ZIS_GRPRFSRV_CLASS_TOO_LONG;
  }

  if (parm->startProfile.length > ZIS_SECURITY_PROFILE_MAX_LENGTH) {
    return RC_ZIS_GRPRFSRV_PROFILE_TOO_LONG;
  }

  if (parm->resultBuffer == NULL) {
    return RC_ZIS_GRPRFSRV_RESULT_BUFF_NULL;
  }

  return RC_ZIS_GRPRFSRV_OK;
}

static void copyGenresProfiles(ZISGenresProfileEntry *dest,
                               const RadminBasicGenresPofileInfo *src,
                               unsigned int count) {

  for (unsigned int i = 0; i < count; i++) {
    cmCopyToSecondaryWithCallerKey(dest[i].profile, src[i].profile,
                                   sizeof(dest[i].profile));
    cmCopyToSecondaryWithCallerKey(dest[i].owner, src[i].owner,
                                   sizeof(dest[i].owner));
  }

}


int zisGenresProfilesServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                                     CrossMemoryService *service, void *parm) {

  int status = RC_ZIS_GRPRFSRV_OK;
  int radminExtractRC = RC_RADMIN_OK;

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_GRPRFSRV_PARMLIST_NULL;
  }

  ZISGenresProfileServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr,
                                   sizeof(localParmList));

  int parmValidateRC = validateGenresProfileParmList(&localParmList);
  if (parmValidateRC != RC_ZIS_GRPRFSRV_OK) {
    return parmValidateRC;
  }

  int traceLevel = localParmList.traceLevel;
  if (globalArea->pcLogLevel > traceLevel) {
    traceLevel = globalArea->pcLogLevel;
  }

  do {

    RadminUserID caller;
    if (!secmgmtGetCallerUserID(&caller)) {
      status = RC_ZIS_GRPRFSRV_IMPERSONATION_MISSING;
      break;
    }

    CMS_DEBUG2(globalArea, traceLevel,
               "GRPRFSRV: profile = \'%.*s\'\n",
               localParmList.startProfile.length,
               localParmList.startProfile.value);
    CMS_DEBUG2(globalArea, traceLevel,
               "GRPRFSRV: class = \'%.*s\', profilesToExtract = %u,"
               " result buffer = 0x%p\n",
               localParmList.class.length, localParmList.class.value,
               localParmList.profilesToExtract,
               localParmList.resultBuffer);
    CMS_DEBUG2(globalArea, traceLevel, "GRPRFSRV: caller = \'%.*s\'\n",
               caller.length, caller.value);

    RadminCallerAuthInfo authInfo = {
        .acee = NULL,
        .userID = caller
    };

    RadminStatus radminStatus;
    char classNullTerm[ZIS_SECURITY_CLASS_MAX_LENGTH + 1] = {0};
    memcpy(classNullTerm, localParmList.class.value,
           localParmList.class.length);
    char profileNameBuffer[ZIS_SECURITY_PROFILE_MAX_LENGTH + 1] = {0};
    memcpy(profileNameBuffer, localParmList.startProfile.value,
           localParmList.startProfile.length);
    const char *startProfileNullTerm = localParmList.startProfile.length > 0 ?
                                       profileNameBuffer : NULL;

    if (localParmList.class.length == 0) {
      CrossMemoryServerConfigParm userClassParm = {0};
      int getParmRC = cmsGetConfigParm(&globalArea->serverName,
                                       ZIS_PARMLIB_PARM_SECMGMT_USER_CLASS,
                                       &userClassParm);
      if (getParmRC != RC_CMS_OK) {
        localParmList.internalServiceRC = getParmRC;
        status = RC_ZIS_GRPRFSRV_USER_CLASS_NOT_READ;
        break;
      }
      if (userClassParm.valueLength > ZIS_SECURITY_CLASS_MAX_LENGTH) {
        status = RC_ZIS_GRPRFSRV_CLASS_TOO_LONG;
        break;
      }
      memcpy(classNullTerm, userClassParm.charValueNullTerm,
             userClassParm.valueLength);
    }

    size_t tmpResultBufferSize =
        sizeof(RadminBasicGenresPofileInfo) * localParmList.profilesToExtract;
    int allocRC = 0, allocSysRC = 0, allocSysRSN = 0;
    RadminBasicGenresPofileInfo *tmpResultBuffer =
        (RadminBasicGenresPofileInfo *)safeMalloc64v3(
            tmpResultBufferSize, TRUE,
            "RadminBasicGenresPofileInfo",
            &allocRC,
            &allocSysRSN,
            &allocSysRSN
        );
    CMS_DEBUG2(globalArea, traceLevel,
               "GRPRFSRV: tmpBuff allocated @ 0x%p (%zu %d %d %d)\n",
               tmpResultBuffer, tmpResultBufferSize,
               allocRC, allocSysRC, allocSysRSN);

    if (tmpResultBuffer == NULL) {
      status = RC_ZIS_GRPRFSRV_ALLOC_FAILED;
      break;
    }

    size_t profilesExtracted = 0;

    radminExtractRC = radminExtractBasicGenresProfileInfo(
        authInfo,
        classNullTerm,
        startProfileNullTerm,
        localParmList.profilesToExtract,
        tmpResultBuffer,
        &profilesExtracted,
        &radminStatus
    );

    if (radminExtractRC == RC_RADMIN_OK) {
      copyGenresProfiles(localParmList.resultBuffer, tmpResultBuffer,
                         profilesExtracted);
      localParmList.profilesExtracted = profilesExtracted;
    } else  {
      localParmList.internalServiceRC = radminExtractRC;
      localParmList.internalServiceRSN = radminStatus.reasonCode;
      localParmList.safStatus.safRC = radminStatus.apiStatus.safRC;
      localParmList.safStatus.racfRC = radminStatus.apiStatus.racfRC;
      localParmList.safStatus.racfRSN = radminStatus.apiStatus.racfRSN;
      status = RC_ZIS_GRPRFSRV_INTERNAL_SERVICE_FAILED;
    }

    int freeRC = 0, freeSysRC = 0, freeSysRSN = 0;
    freeRC = safeFree64v3(tmpResultBuffer, tmpResultBufferSize,
                          &freeSysRC, &freeSysRSN);
    CMS_DEBUG2(globalArea, traceLevel, "GRPRFSRV: tmpBuff free'd @ 0x%p (%zu %d %d %d)\n",
              tmpResultBuffer, tmpResultBufferSize,
              freeRC, freeSysRC, freeSysRSN);

    tmpResultBuffer = NULL;

  } while (0);

  cmCopyToSecondaryWithCallerKey(clientParmAddr, &localParmList,
                                 sizeof(ZISGenresProfileServiceParmList));

  CMS_DEBUG2(globalArea, traceLevel,
             "GRPRFSRV: status = %d, extracted = %d, RC = %d, "
             "internal RC = %d, RSN = %d, "
              "SAF RC = %d, RACF RC = %d, RSN = %d\n",
             status,
             localParmList.profilesExtracted, radminExtractRC,
             localParmList.internalServiceRC,
             localParmList.internalServiceRSN,
             localParmList.safStatus.safRC,
             localParmList.safStatus.racfRC,
             localParmList.safStatus.racfRSN);

  return status;
}

static int
validateGenresAccessListParmList(ZISGenresAccessListServiceParmList *parm) {

  if (memcmp(parm->eyecatcher, ZIS_ACSLIST_SERVICE_PARMLIST_EYECATCHER,
             sizeof(parm->eyecatcher))) {
    return RC_ZIS_ACSLSRV_BAD_EYECATCHER;
  }

  if (parm->class.length > ZIS_SECURITY_CLASS_MAX_LENGTH) {
    return RC_ZIS_ACSLSRV_CLASS_TOO_LONG;
  }

  if (parm->profile.length > ZIS_SECURITY_PROFILE_MAX_LENGTH) {
    return RC_ZIS_ACSLSRV_PROFILE_TOO_LONG;
  }

  if (parm->resultBuffer == NULL) {
    return RC_ZIS_ACSLSRV_RESULT_BUFF_NULL;
  }

  return RC_ZIS_ACSLSRV_OK;
}

static void copyGenresAccessList(ZISGenresAccessEntry *dest,
                                 const RadminAccessListEntry *src,
                                 unsigned int count) {

  for (unsigned int i = 0; i < count; i++) {
    cmCopyToSecondaryWithCallerKey(dest[i].id, src[i].id,
                                   sizeof(dest[i].id));
    cmCopyToSecondaryWithCallerKey(dest[i].accessType, src[i].accessType,
                                   sizeof(dest[i].accessType));
  }

}

int zisGenresAccessListServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                                       CrossMemoryService *service, void *parm) {

  int status = RC_ZIS_ACSLSRV_OK;
  int radminExtractRC = RC_RADMIN_OK;

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_ACSLSRV_PARMLIST_NULL;
  }

  ZISGenresAccessListServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr,
                                   sizeof(localParmList));

  int parmValidateRC = validateGenresAccessListParmList(&localParmList);
  if (parmValidateRC != RC_ZIS_ACSLSRV_OK) {
    return parmValidateRC;
  }

  int traceLevel = localParmList.traceLevel;
  if (globalArea->pcLogLevel > traceLevel) {
    traceLevel = globalArea->pcLogLevel;
  }

  do {

    RadminUserID caller;
    if (!secmgmtGetCallerUserID(&caller)) {
      status = RC_ZIS_ACSLSRV_IMPERSONATION_MISSING;
      break;
    }

    CMS_DEBUG2(globalArea, traceLevel,
               "ACSLSRV: class = \'%.*s\', result buffer = 0x%p (%u)\n",
               localParmList.class.length, localParmList.class.value,
               localParmList.resultBuffer,
               localParmList.resultBufferCapacity);
    CMS_DEBUG2(globalArea, traceLevel, "ACSLSRV: profile = \'%.*s\'\n",
               localParmList.profile.length, localParmList.profile.value);
    CMS_DEBUG2(globalArea, traceLevel, "ACSLSRV: caller = \'%.*s\'\n",
               caller.length, caller.value);

    RadminCallerAuthInfo authInfo = {
        .acee = NULL,
        .userID = caller
    };

    size_t entriesExtracted = 0;

    RadminStatus radminStatus;
    char classNullTerm[ZIS_SECURITY_CLASS_MAX_LENGTH + 1] = {0};
    memcpy(classNullTerm, localParmList.class.value,
           localParmList.class.length);
    char profileNullTerm[ZIS_SECURITY_PROFILE_MAX_LENGTH + 1] = {0};
    memcpy(profileNullTerm, localParmList.profile.value,
           localParmList.profile.length);

    if (localParmList.class.length == 0) {
      CrossMemoryServerConfigParm userClassParm = {0};
      int getParmRC = cmsGetConfigParm(&globalArea->serverName,
                                       ZIS_PARMLIB_PARM_SECMGMT_USER_CLASS,
                                       &userClassParm);
      if (getParmRC != RC_CMS_OK) {
        localParmList.internalServiceRC = getParmRC;
        status = RC_ZIS_ACSLSRV_USER_CLASS_NOT_READ;
        break;
      }
      if (userClassParm.valueLength > ZIS_SECURITY_CLASS_MAX_LENGTH) {
        status = RC_ZIS_ACSLSRV_CLASS_TOO_LONG;
        break;
      }
      memcpy(classNullTerm, userClassParm.charValueNullTerm,
             userClassParm.valueLength);
    }

    size_t tmpResultBufferSize =
        sizeof(RadminAccessListEntry) * localParmList.resultBufferCapacity;
    int allocRC = 0, allocSysRC = 0, allocSysRSN = 0;
    RadminAccessListEntry *tmpResultBuffer =
        (RadminAccessListEntry *)safeMalloc64v3(tmpResultBufferSize, TRUE,
                                                "RadminAccessListEntry",
                                                 &allocRC,
                                                 &allocSysRC,
                                                 &allocSysRSN);
    CMS_DEBUG2(globalArea, traceLevel,
               "ACSLSRV: tmpBuff allocated @ 0x%p (%zu %d %d %d)\n",
               tmpResultBuffer, tmpResultBufferSize,
               allocRC, allocSysRC, allocSysRSN);

    if (tmpResultBuffer == NULL) {
      status = RC_ZIS_ACSLSRV_ALLOC_FAILED;
      break;
    }

    int radminExtractRC = radminExtractGenresAccessList(
        authInfo,
        classNullTerm,
        profileNullTerm,
        tmpResultBuffer,
        localParmList.resultBufferCapacity,
        &entriesExtracted,
        &radminStatus
    );

    if (radminExtractRC == RC_RADMIN_OK) {
      copyGenresAccessList(localParmList.resultBuffer, tmpResultBuffer,
                     entriesExtracted);
      localParmList.entriesExtracted = entriesExtracted;
      localParmList.entriesFound = entriesExtracted;
    } else if (radminExtractRC == RC_RADMIN_INSUFF_SPACE) {
      localParmList.entriesExtracted = 0;
      localParmList.entriesFound = radminStatus.reasonCode;
      status = RC_ZIS_ACSLSRV_INSUFFICIENT_SPACE;
    } else  {
      localParmList.internalServiceRC = radminExtractRC;
      localParmList.internalServiceRSN = radminStatus.reasonCode;
      localParmList.safStatus.safRC = radminStatus.apiStatus.safRC;
      localParmList.safStatus.racfRC = radminStatus.apiStatus.racfRC;
      localParmList.safStatus.racfRSN = radminStatus.apiStatus.racfRSN;
      status = RC_ZIS_ACSLSRV_INTERNAL_SERVICE_FAILED;
    }

    int freeRC = 0, freeSysRC = 0, freeSysRSN = 0;
    freeRC = safeFree64v3(tmpResultBuffer, tmpResultBufferSize,
                          &freeSysRC, &freeSysRSN);
    CMS_DEBUG2(globalArea, traceLevel,
               "ACSLSRV: tmpBuff free'd @ 0x%p (%zu %d %d %d)\n",
               tmpResultBuffer, tmpResultBufferSize,
               freeRC, freeSysRC, freeSysRSN);

    tmpResultBuffer = NULL;

  } while (0);

  cmCopyToSecondaryWithCallerKey(clientParmAddr, &localParmList,
                                 sizeof(ZISGenresAccessListServiceParmList));

  CMS_DEBUG2(globalArea, traceLevel,
             "ACSLSRV: entriesExtracted = %u, RC = %d, "
             "internal RC = %d, RSN = %d, "
             "SAF RC = %d, RACF RC = %d, RSN = %d\n",
             localParmList.entriesExtracted, radminExtractRC,
             localParmList.internalServiceRC,
             localParmList.internalServiceRSN,
             localParmList.safStatus.safRC,
             localParmList.safStatus.racfRC,
             localParmList.safStatus.racfRSN);

  return status;
}

static int validateGenresParmList(ZISGenresAdminServiceParmList *parmList) {

  if (memcmp(parmList->eyecatcher,
             ZIS_GENRES_ADMIN_SERVICE_PARMLIST_EYECATCHER,
             sizeof(parmList->eyecatcher) != 0)) {
    return RC_ZIS_GSADMNSRV_BAD_EYECATCHER;
  }

  if (parmList->function < ZIS_GENRES_ADMIN_SRV_FC_MIN ||
      ZIS_GENRES_ADMIN_SRV_FC_MAX < parmList->function) {
    return RC_ZIS_GSADMNSRV_BAD_FUNCTION;
  }

  if (parmList->profile.length > ZIS_SECURITY_PROFILE_MAX_LENGTH) {
    return RC_ZIS_GSADMNSRV_PROF_TOO_LONG;
  }

  if (parmList->owner.length > ZIS_USER_ID_MAX_LENGTH) {
    return RC_ZIS_GSADMNSRV_OWNER_TOO_LONG;
  }

  const char *productName = parmList->profile.value;
  bool isProductSupported = false;
  int productCount =
      sizeof(ZIS_GENRES_PRODUCTS) / sizeof(ZIS_GENRES_PRODUCTS[0]);
  for (int i = 0; i < productCount; i++) {

    const char *currProductName = ZIS_GENRES_PRODUCTS[i];
    size_t currProductNameLength = strlen(currProductName);
    if (currProductNameLength > parmList->profile.length) {
      continue;
    }

    if (memcmp(productName, currProductName, currProductNameLength) == 0 &&
        productName[currProductNameLength] == '.') {
      isProductSupported = true;
      break;
    }
  }

  if (isProductSupported == false) {
    return RC_ZIS_GSADMNSRV_UNSUPPORTED_PRODUCT;
  }

  if (parmList->user.length > ZIS_USER_ID_MAX_LENGTH) {
    return RC_ZIS_GSADMNSRV_USER_ID_TOO_LONG;
  }

  if (parmList->function == ZIS_GENRES_ADMIN_SRV_FC_GIVE_ACCESS) {
    if (parmList->accessType < ZIS_GENRES_ADMIN_ACESS_TYPE_MIN ||
        ZIS_GENRES_ADMIN_ACESS_TYPE_MAX < parmList->accessType) {
      return RC_ZIS_GSADMNSRV_BAD_ACCESS_TYPE;
    }
  }

  return RC_ZIS_GSADMNSRV_OK;
}

static RadminResAction mapZISToRadminResAction(ZISGenresAdminFunction function)
{
  switch (function) {
  case ZIS_GENRES_ADMIN_SRV_FC_DEFINE:
    return RADMIN_RES_ACTION_ADD_GENRES;
  case ZIS_GENRES_ADMIN_SRV_FC_DELETE:
    return RADMIN_RES_ACTION_DELETE_GENRES;
  case ZIS_GENRES_ADMIN_SRV_FC_GIVE_ACCESS:
    return RADMIN_RES_ACTION_PERMIT;
  case ZIS_GENRES_ADMIN_SRV_FC_REVOKE_ACCESS:
    return RADMIN_RES_ACTION_PROHIBIT;
  default:
    return RADMIN_RES_ACTION_NA;
  }
}

typedef struct GenresOutputHandlerData_tag {
  char eyecatcher[8];
#define GENRES_OUT_HADNLER_DATA_EYECATCHER  "RSGRONDA"
  CrossMemoryServerGlobalArea *globalArea;
  size_t fullMessageLenght;
  size_t fullCommandLength;
  ZISGenresAdminServiceMessage message;
  ZISGenresAdminServiceMessage command;
} GenresOutputHandlerData;

#define RADMIN_OUT_MAX_NODE_COUNT 16

#define RC_GET_MSG_HADNLER_OK     0
#define RC_GET_MSG_HADNLER_LOOP   8

static int getMsgAndOperatorCommand(RadminAPIStatus status,
                                    const RadminCommandOutput *out,
                                    void *userData) {

  GenresOutputHandlerData *handlerData = userData;
  ZISGenresAdminServiceMessage *message = &handlerData->message;
  ZISGenresAdminServiceMessage *command = &handlerData->command;

  bool messageCopied = false, commandCopied = false;

  const RadminCommandOutput *nextOutput = out;
  int nodeCount = 0;
  while (nextOutput != NULL) {

    if (commandCopied && messageCopied) {
      break;
    }
    if (nodeCount > RADMIN_OUT_MAX_NODE_COUNT) {
      return RC_GET_MSG_HADNLER_LOOP;
    }

    if (memcmp(nextOutput->eyecatcher, RADMIN_COMMAND_OUTPUT_EYECATCHER_CMD,
               sizeof(nextOutput->eyecatcher)) == 0) {
      unsigned int copySize =
          min(nextOutput->firstMessageEntry.length, sizeof(command->text));
      memcpy(command->text, nextOutput->firstMessageEntry.text, copySize);
      command->length = copySize;
      handlerData->fullCommandLength = nextOutput->firstMessageEntry.length;
      commandCopied = true;
    }

    if (memcmp(nextOutput->eyecatcher, RADMIN_COMMAND_OUTPUT_EYECATCHER_MSG,
               sizeof(nextOutput->eyecatcher)) == 0) {
      unsigned int copySize =
          min(nextOutput->firstMessageEntry.length, sizeof(message->text));
      memcpy(message->text, nextOutput->firstMessageEntry.text, copySize);
      message->length = copySize;
      handlerData->fullMessageLenght = nextOutput->firstMessageEntry.length;
      messageCopied = true;
    }

    nodeCount++;
    nextOutput = nextOutput->next;
  }

  return RC_GET_MSG_HADNLER_OK;
}

static const char *getAccessString(ZISGenresAdminServiceAccessType type) {
  switch (type) {
  case ZIS_GENRES_ADMIN_ACESS_TYPE_READ:
    return "READ";
  case ZIS_GENRES_ADMIN_ACESS_TYPE_UPDATE:
    return "UPDATE";
  case ZIS_GENRES_ADMIN_ACESS_TYPE_ALTER:
    return "ALTER";
  default:
    return NULL;
  }
}

#define RES_PARMLIST_MIN_SIZE 1024

static RadminResParmList *constructResAdminParmList(
    char parmListBuffer[],
    size_t bufferSize,
    RadminResAction action,
    bool isDryRun,
    const char class[],
    size_t classLength,
    const char profile[],
    size_t profileLength,
    const char owner[],
    size_t ownerLength,
    const char userID[],
    size_t userIDLength,
    ZISGenresAdminServiceAccessType accessType
) {

  if (bufferSize < RES_PARMLIST_MIN_SIZE) {
    return NULL;
  }

  memset(parmListBuffer, ' ', bufferSize);

  RadminResParmList *parmList = (RadminResParmList *)parmListBuffer;

  parmList->classNameLength = classLength;
  memcpy(parmList->className, class, sizeof(parmList->className));
  if (isDryRun) {
    parmList->flags |= RADMIN_RES_FLAG_NORUN;
    parmList->flags |= RADMIN_RES_FLAG_RETCMD;
  }
  parmList->segmentCount = 1;
  RadminSegment *segment = parmList->segmentData;
  memcpy(segment->name, "BASE    ", sizeof(segment->name));
  segment->flag |= RADMIN_SEGMENT_FLAG_Y;
  segment->fieldCount = 1;
  RadminField *field1 = segment->firstField;
  memcpy(field1->name, "PROFILE ", sizeof(field1->name));
  field1->flag |= RADMIN_FIELD_FLAG_Y;
  field1->dataLength = profileLength;
  memcpy(field1->data, profile, field1->dataLength);

  if (action == RADMIN_RES_ACTION_ADD_GENRES) {

    segment->fieldCount++;
    RadminField *field2 =
        (RadminField *)((char *)(field1 + 1) + field1->dataLength);
    memcpy(field2->name, "UACC    ", sizeof(field2->name));
    field2->flag |= RADMIN_FIELD_FLAG_Y;
    field2->dataLength = 4;
    memcpy(field2->data, "NONE", field2->dataLength);

    segment->fieldCount++;
    RadminField *field3 =
        (RadminField *)((char *)(field2 + 1) + field2->dataLength);
    memcpy(field3->name, "OWNER   ", sizeof(field3->name));
    field3->flag |= RADMIN_FIELD_FLAG_Y;
    field3->dataLength = ownerLength;
    memcpy(field3->data, owner, field3->dataLength);

  }

  if (action == RADMIN_RES_ACTION_PERMIT ||
      action == RADMIN_RES_ACTION_PROHIBIT) {

    segment->fieldCount++;
    RadminField *field2 =
        (RadminField *)((char *)(field1 + 1) + field1->dataLength);
    memcpy(field2->name, "ID      ", sizeof(field2->name));
    field2->flag |= RADMIN_FIELD_FLAG_Y;
    field2->dataLength = userIDLength;
    memcpy(field2->data, userID, field2->dataLength);

    segment->fieldCount++;
    RadminField *field3 =
        (RadminField *)((char *)(field2 + 1) + field2->dataLength);

    if (action == RADMIN_RES_ACTION_PERMIT) {
      const char *accessString = getAccessString(accessType);
      size_t accessStringLength = strlen(accessString);
      memcpy(field3->name, "ACCESS  ", sizeof(field3->name));
      field3->flag |= RADMIN_FIELD_FLAG_Y;
      field3->dataLength = accessStringLength;
      memcpy(field3->data, accessString, field3->dataLength);
    } else {
      memcpy(field3->name, "DELETE  ", sizeof(field3->name));
      field3->flag |= RADMIN_FIELD_FLAG_Y;
      field3->dataLength = 0;
    }

  }

  return parmList;
}

static int performRefreshIfNeeded(CrossMemoryServerGlobalArea *globalArea,
                                  RadminCallerAuthInfo authInfo,
                                  const char *classNullTerm,
                                  ZISGenresAdminServiceParmList *parmList,
                                  int traceLevel
) {

  CMS_DEBUG2(globalArea, traceLevel,
             "GSADMNSRV: RACLIST REFRESH for class \'%s\'\n", classNullTerm);

  CrossMemoryServerConfigParm refreshParm = {0};
  int getParmRC = cmsGetConfigParm(&globalArea->serverName,
                                   ZIS_PARMLIB_PARM_SECMGMT_AUTORESFRESH,
                                   &refreshParm);
  if (getParmRC != RC_CMS_OK) {
    if (getParmRC != RC_CMS_CONFIG_PARM_NOT_FOUND) {
      CMS_DEBUG2(globalArea, traceLevel,
                 "GSADMNSRV: AUTORESFRESH read error, RC = %d\n", getParmRC);
      parmList->internalServiceRC = getParmRC;
      return RC_ZIS_GSADMNSRV_AUTOREFRESH_NOT_READ;
    }
    CMS_DEBUG2(globalArea, traceLevel, "GSADMNSRV: AUTORESFRESH not found\n");
    return RC_ZIS_GSADMNSRV_OK;
  }

  CMS_DEBUG2(globalArea, traceLevel,
             "GSADMNSRV: AUTORESFRESH=\'%s\'\n", refreshParm.charValueNullTerm);

  if (strcmp(refreshParm.charValueNullTerm, "YES") != 0) {
    return RC_ZIS_GSADMNSRV_OK;
  }

  GenresOutputHandlerData handlerData = {
      .eyecatcher = GENRES_OUT_HADNLER_DATA_EYECATCHER,
      .globalArea = globalArea,
      .fullMessageLenght = 0,
      .fullCommandLength = 0
  };

  RadminStatus radminStatus = {0};

  char refreshCommand[RADMIN_CMD_MAX_COMMAND_LENGTH + 1];
  snprintf(refreshCommand, sizeof(refreshCommand),
           "SETROPTS REFRESH RACLIST(%s)", classNullTerm);

  CMS_DEBUG2(globalArea, traceLevel,
             "GSADMNSRV: executing \'%s\'\n", refreshCommand);

  int refreshRC = radminRunRACFCommand(
      authInfo,
      refreshCommand,
      getMsgAndOperatorCommand,
      &handlerData,
      &radminStatus
  );

  parmList->serviceMessage = handlerData.message;

  CMS_DEBUG2(globalArea, traceLevel,
             "GSADMNSRV: refresh RC = %d, RSN = %d, "
             "msgLen = %zu, cmdLen = %zu, "
             "SAF RC = %d, RACF RC = %d, RSN = %d\n",
             refreshRC, radminStatus.reasonCode,
             handlerData.message.length, handlerData.command.length,
             radminStatus.apiStatus.safRC,
             radminStatus.apiStatus.racfRC,
             radminStatus.apiStatus.racfRSN);

  if (refreshRC != RC_RADMIN_OK) {
    parmList->internalServiceRC = refreshRC;
    parmList->internalServiceRSN = radminStatus.reasonCode;
    parmList->safStatus.safRC = radminStatus.apiStatus.safRC;
    parmList->safStatus.racfRC = radminStatus.apiStatus.racfRC;
    parmList->safStatus.racfRSN = radminStatus.apiStatus.racfRSN;
    return RC_ZIS_GSADMNSRV_INTERNAL_SERVICE_FAILED;
  }

  return RC_ZIS_GSADMNSRV_OK;
}

int zisGenresProfileAdminServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                                   CrossMemoryService *service, void *parm) {

  int status = RC_ZIS_GSADMNSRV_OK;
  int radminActionRC = RC_RADMIN_OK;

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_GSADMNSRV_PARMLIST_NULL;
  }

  ZISGenresAdminServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr,
                                   sizeof(localParmList));

  int parmValidateRC = validateGenresParmList(&localParmList);
  if (parmValidateRC != RC_ZIS_GSADMNSRV_OK) {
    return parmValidateRC;
  }

  int traceLevel = localParmList.traceLevel;
  if (globalArea->pcLogLevel > traceLevel) {
    traceLevel = globalArea->pcLogLevel;
  }

  do {

    RadminUserID caller;
    if (!secmgmtGetCallerUserID(&caller)) {
      status = RC_ZIS_GSADMNSRV_IMPERSONATION_MISSING;
      break;
    }

    if (localParmList.owner.length == 0) {
      memcpy(localParmList.owner.value, caller.value, sizeof(caller.value));
      localParmList.owner.length = caller.length;
    }

    CMS_DEBUG2(globalArea, traceLevel,
               "GSADMNSRV: flags = 0x%X, function = %d, "
               "owner = \'%.*s\', user = \'%.*s\', access = %d'\n",
               localParmList.flags,
               localParmList.function,
               localParmList.owner.length, localParmList.owner.value,
               localParmList.user.length, localParmList.user.value,
               localParmList.accessType);
    CMS_DEBUG2(globalArea, traceLevel, "GSADMNSRV: profile = \'%.*s\'\n",
               localParmList.profile.length, localParmList.profile.value);
    CMS_DEBUG2(globalArea, traceLevel, "GSADMNSRV: caller = \'%.*s\'\n",
               caller.length, caller.value);

    RadminCallerAuthInfo authInfo = {
        .acee = NULL,
        .userID = caller
    };

    CrossMemoryServerConfigParm userClassParm = {0};
    int getParmRC = cmsGetConfigParm(&globalArea->serverName,
                                     ZIS_PARMLIB_PARM_SECMGMT_USER_CLASS,
                                     &userClassParm);
    if (getParmRC != RC_CMS_OK) {
      localParmList.internalServiceRC = getParmRC;
      status = RC_ZIS_GSADMNSRV_USER_CLASS_NOT_READ;
      break;
    }
    if (userClassParm.valueLength > ZIS_SECURITY_CLASS_MAX_LENGTH) {
      status = RC_ZIS_GSADMNSRV_USER_CLASS_TOO_LONG;
      break;
    }

    CMS_DEBUG2(globalArea, traceLevel, "GSADMNSRV: class = \'%.*s\'\n",
               userClassParm.valueLength, userClassParm.charValueNullTerm);

    RadminResAction action = mapZISToRadminResAction(localParmList.function);
    if (action == RADMIN_RES_ACTION_NA) {
      status = RC_ZIS_GSADMNSRV_BAD_FUNCTION;
      break;
    }

    bool isDryRun = (localParmList.flags & ZIS_GENRES_ADMIN_FLAG_DRYRUN) ?
                    true : false;
    char parmListBuffer[2048];
    RadminResParmList *parmList = constructResAdminParmList(
        parmListBuffer,
        sizeof(parmListBuffer),
        action, isDryRun,
        userClassParm.charValueNullTerm,
        userClassParm.valueLength,
        localParmList.profile.value,
        localParmList.profile.length,
        localParmList.owner.value,
        localParmList.owner.length,
        localParmList.user.value,
        localParmList.user.length,
        localParmList.accessType
    );

    if (parmList == NULL) {
      status = RC_ZIS_GSADMNSRV_PARM_INTERNAL_ERROR;
      break;
    }

    GenresOutputHandlerData handlerData = {
        .eyecatcher = GENRES_OUT_HADNLER_DATA_EYECATCHER,
        .globalArea = globalArea,
        .fullMessageLenght = 0,
        .fullCommandLength = 0
    };

    RadminStatus radminStatus;

    radminActionRC = radminPerformResAction(
        authInfo,
        action,
        parmList,
        getMsgAndOperatorCommand,
        &handlerData,
        &radminStatus
    );

    localParmList.serviceMessage = handlerData.message;
    localParmList.operatorCommand = handlerData.command;

    if (radminActionRC != RC_RADMIN_OK) {
      localParmList.internalServiceRC = radminActionRC;
      localParmList.internalServiceRSN = radminStatus.reasonCode;
      localParmList.safStatus.safRC = radminStatus.apiStatus.safRC;
      localParmList.safStatus.racfRC = radminStatus.apiStatus.racfRC;
      localParmList.safStatus.racfRSN = radminStatus.apiStatus.racfRSN;
      status = RC_ZIS_GSADMNSRV_INTERNAL_SERVICE_FAILED;
      break;
    }

    if (isDryRun == false) {
      status = performRefreshIfNeeded(globalArea,
                                      authInfo,
                                      userClassParm.charValueNullTerm,
                                      &localParmList,
                                      traceLevel);
    }

  } while (0);

  cmCopyToSecondaryWithCallerKey(clientParmAddr, &localParmList,
                                 sizeof(ZISGenresAdminServiceParmList));

  CMS_DEBUG2(globalArea, traceLevel,
             "GSADMNSRV: status = %d, msgLen = %zu, cmdLen = %zu, "
             "RC = %d, internal RC = %d, RSN = %d, "
             "SAF RC = %d, RACF RC = %d, RSN = %d\n",
             status,
             localParmList.serviceMessage.length,
             localParmList.operatorCommand.length,
             radminActionRC,
             localParmList.internalServiceRC,
             localParmList.internalServiceRSN,
             localParmList.safStatus.safRC,
             localParmList.safStatus.racfRC,
             localParmList.safStatus.racfRSN);

  return status;
}

static int validateGroupProfileParmList(ZISGroupProfileServiceParmList *parm)
{

  if (memcmp(parm->eyecatcher, ZIS_GRPPROF_SERVICE_PARMLIST_EYECATCHER,
             sizeof(parm->eyecatcher))) {
    return RC_ZIS_GPPRFSRV_BAD_EYECATCHER;
  }

  if (parm->startGroup.length > ZIS_SECURITY_GROUP_MAX_LENGTH) {
    return RC_ZIS_GPPRFSRV_GROUP_TOO_LONG;
  }

  if (parm->resultBuffer == NULL) {
    return RC_ZIS_GPPRFSRV_RESULT_BUFF_NULL;
  }

  return RC_ZIS_GPPRFSRV_OK;
}

static void copyGroupProfiles(ZISGroupProfileEntry *dest,
                              const RadminBasicGroupPofileInfo *src,
                              unsigned int count) {

  for (unsigned int i = 0; i < count; i++) {
    cmCopyToSecondaryWithCallerKey(dest[i].group, src[i].group,
                                   sizeof(dest[i].group));
    cmCopyToSecondaryWithCallerKey(dest[i].owner, src[i].owner,
                                   sizeof(dest[i].owner));
    cmCopyToSecondaryWithCallerKey(dest[i].superiorGroup, src[i].superiorGroup,
                                   sizeof(dest[i].superiorGroup));
  }

}

int zisGroupProfilesServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                                    CrossMemoryService *service, void *parm) {

  int status = RC_ZIS_GPPRFSRV_OK;
  int radminExtractRC = RC_RADMIN_OK;

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_GPPRFSRV_PARMLIST_NULL;
  }

  ZISGroupProfileServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr,
                                   sizeof(localParmList));

  int parmValidateRC = validateGroupProfileParmList(&localParmList);
  if (parmValidateRC != RC_ZIS_GPPRFSRV_OK) {
    return parmValidateRC;
  }

  int traceLevel = localParmList.traceLevel;
  if (globalArea->pcLogLevel > traceLevel) {
    traceLevel = globalArea->pcLogLevel;
  }

  do {

    RadminUserID caller;
    if (!secmgmtGetCallerUserID(&caller)) {
      status = RC_ZIS_GPPRFSRV_IMPERSONATION_MISSING;
      break;
    }

    CMS_DEBUG2(globalArea, traceLevel,
               "GPPRFSRV: group = \'%.*s\', profilesToExtract = %u,"
               " result buffer = 0x%p\n",
               localParmList.startGroup.length,
               localParmList.startGroup.value,
               localParmList.profilesToExtract,
               localParmList.resultBuffer);
    CMS_DEBUG2(globalArea, traceLevel, "GPPRFSRV: caller = \'%.*s\'\n",
               caller.length, caller.value);

    RadminCallerAuthInfo authInfo = {
        .acee = NULL,
        .userID = caller
    };

    RadminStatus radminStatus;
    char groupNameBuffer[ZIS_SECURITY_GROUP_MAX_LENGTH + 1] = {0};
    memcpy(groupNameBuffer, localParmList.startGroup.value,
           localParmList.startGroup.length);
    const char *startProfileNullTerm = localParmList.startGroup.length > 0 ?
                                       groupNameBuffer : NULL;

    size_t tmpResultBufferSize =
        sizeof(RadminBasicGroupPofileInfo) * localParmList.profilesToExtract;
    int allocRC = 0, allocSysRC = 0, allocSysRSN = 0;
    RadminBasicGroupPofileInfo *tmpResultBuffer =
        (RadminBasicGroupPofileInfo *)safeMalloc64v3(
            tmpResultBufferSize, TRUE,
            "RadminBasicGroupPofileInfo",
            &allocRC,
            &allocSysRSN,
            &allocSysRSN
        );
    CMS_DEBUG2(globalArea, traceLevel,
               "GPPRFSRV: tmpBuff allocated @ 0x%p (%zu %d %d %d)\n",
               tmpResultBuffer, tmpResultBufferSize,
               allocRC, allocSysRC, allocSysRSN);

    if (tmpResultBuffer == NULL) {
      status = RC_ZIS_GPPRFSRV_ALLOC_FAILED;
      break;
    }

    size_t profilesExtracted = 0;

    radminExtractRC = radminExtractBasicGroupProfileInfo(
        authInfo,
        startProfileNullTerm,
        localParmList.profilesToExtract,
        tmpResultBuffer,
        &profilesExtracted,
        &radminStatus
    );

    if (radminExtractRC == RC_RADMIN_OK) {
      copyGroupProfiles(localParmList.resultBuffer, tmpResultBuffer,
                        profilesExtracted);
      localParmList.profilesExtracted = profilesExtracted;
    } else  {
      localParmList.internalServiceRC = radminExtractRC;
      localParmList.internalServiceRSN = radminStatus.reasonCode;
      localParmList.safStatus.safRC = radminStatus.apiStatus.safRC;
      localParmList.safStatus.racfRC = radminStatus.apiStatus.racfRC;
      localParmList.safStatus.racfRSN = radminStatus.apiStatus.racfRSN;
      status = RC_ZIS_GPPRFSRV_INTERNAL_SERVICE_FAILED;
    }

    int freeRC = 0, freeSysRC = 0, freeSysRSN = 0;
    freeRC = safeFree64v3(tmpResultBuffer, tmpResultBufferSize,
                          &freeSysRC, &freeSysRSN);
    CMS_DEBUG2(globalArea, traceLevel,
               "GPPRFSRV: tmpBuff free'd @ 0x%p (%zu %d %d %d)\n",
               tmpResultBuffer, tmpResultBufferSize,
               freeRC, freeSysRC, freeSysRSN);

    tmpResultBuffer = NULL;

  } while (0);

  cmCopyToSecondaryWithCallerKey(clientParmAddr, &localParmList,
                                 sizeof(ZISGroupProfileServiceParmList));

  CMS_DEBUG2(globalArea, traceLevel,
             "GPPRFSRV: status = %d, extracted = %d, RC = %d, "
             "internal RC = %d, RSN = %d, "
              "SAF RC = %d, RACF RC = %d, RSN = %d\n",
             status,
             localParmList.profilesExtracted, radminExtractRC,
             localParmList.internalServiceRC,
             localParmList.internalServiceRSN,
             localParmList.safStatus.safRC,
             localParmList.safStatus.racfRC,
             localParmList.safStatus.racfRSN);

  return status;
}

static int
validateGroupAccessListParmList(ZISGroupAccessListServiceParmList *parm) {

  if (memcmp(parm->eyecatcher, ZIS_GRPALST_SERVICE_PARMLIST_EYECATCHER,
             sizeof(parm->eyecatcher))) {
    return RC_ZIS_GRPALSRV_BAD_EYECATCHER;
  }

  if (parm->group.length > ZIS_SECURITY_GROUP_MAX_LENGTH) {
    return RC_ZIS_GRPALSRV_GROUP_TOO_LONG;
  }

  if (parm->resultBuffer == NULL) {
    return RC_ZIS_GRPALSRV_RESULT_BUFF_NULL;
  }

  return RC_ZIS_GRPALSRV_OK;
}

static void copyGroupAccessList(ZISGroupAccessEntry *dest,
                                const RadminAccessListEntry *src,
                                unsigned int count) {

  for (unsigned int i = 0; i < count; i++) {
    cmCopyToSecondaryWithCallerKey(dest[i].id, src[i].id,
                                   sizeof(dest[i].id));
    cmCopyToSecondaryWithCallerKey(dest[i].accessType, src[i].accessType,
                                   sizeof(dest[i].accessType));
  }

}

int zisGroupAccessListServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                                      CrossMemoryService *service, void *parm) {

  int status = RC_ZIS_GRPALSRV_OK;
  int radminExtractRC = RC_RADMIN_OK;

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_GRPALSRV_PARMLIST_NULL;
  }

  ZISGroupAccessListServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr,
                                   sizeof(localParmList));

  int parmValidateRC = validateGroupAccessListParmList(&localParmList);
  if (parmValidateRC != RC_ZIS_GRPALSRV_OK) {
    return parmValidateRC;
  }

  int traceLevel = localParmList.traceLevel;
  if (globalArea->pcLogLevel > traceLevel) {
    traceLevel = globalArea->pcLogLevel;
  }

  do {

    RadminUserID caller;
    if (!secmgmtGetCallerUserID(&caller)) {
      status = RC_ZIS_GRPALSRV_IMPERSONATION_MISSING;
      break;
    }

    CMS_DEBUG2(globalArea, traceLevel,
               "GRPALSRV: group = \'%.*s\', result buffer = 0x%p (%u)\n",
               localParmList.group.length, localParmList.group.value,
               localParmList.resultBuffer,
               localParmList.resultBufferCapacity);
    CMS_DEBUG2(globalArea, traceLevel, "GRPALSRV: caller = \'%.*s\'\n",
               caller.length, caller.value);

    RadminCallerAuthInfo authInfo = {
        .acee = NULL,
        .userID = caller
    };

    size_t entriesExtracted = 0;

    RadminStatus radminStatus;
    char groupNameNullTerm[ZIS_SECURITY_GROUP_MAX_LENGTH + 1] = {0};
    memcpy(groupNameNullTerm, localParmList.group.value,
           localParmList.group.length);

    size_t tmpResultBufferSize =
        sizeof(RadminAccessListEntry) * localParmList.resultBufferCapacity;
    int allocRC = 0, allocSysRC = 0, allocSysRSN = 0;
    RadminAccessListEntry *tmpResultBuffer =
        (RadminAccessListEntry *)safeMalloc64v3(tmpResultBufferSize, TRUE,
                                                "RadminGroupAccessListEntry",
                                                 &allocRC,
                                                 &allocSysRC,
                                                 &allocSysRSN);
    CMS_DEBUG2(globalArea, traceLevel,
               "GRPALSRV: tmpBuff allocated @ 0x%p (%zu %d %d %d)\n",
               tmpResultBuffer, tmpResultBufferSize,
               allocRC, allocSysRC, allocSysRSN);

    if (tmpResultBuffer == NULL) {
      status = RC_ZIS_GRPALSRV_ALLOC_FAILED;
      break;
    }

    int radminExtractRC = radminExtractGroupAccessList(
        authInfo,
        groupNameNullTerm,
        tmpResultBuffer,
        localParmList.resultBufferCapacity,
        &entriesExtracted,
        &radminStatus
    );

    if (radminExtractRC == RC_RADMIN_OK) {
      copyGroupAccessList(localParmList.resultBuffer, tmpResultBuffer,
                          entriesExtracted);
      localParmList.entriesExtracted = entriesExtracted;
      localParmList.entriesFound = entriesExtracted;
    } else if (radminExtractRC == RC_RADMIN_INSUFF_SPACE) {
      localParmList.entriesExtracted = 0;
      localParmList.entriesFound = radminStatus.reasonCode;
      status = RC_ZIS_GRPALSRV_INSUFFICIENT_SPACE;
    } else  {
      localParmList.internalServiceRC = radminExtractRC;
      localParmList.internalServiceRSN = radminStatus.reasonCode;
      localParmList.safStatus.safRC = radminStatus.apiStatus.safRC;
      localParmList.safStatus.racfRC = radminStatus.apiStatus.racfRC;
      localParmList.safStatus.racfRSN = radminStatus.apiStatus.racfRSN;
      status = RC_ZIS_GRPALSRV_INTERNAL_SERVICE_FAILED;
    }

    int freeRC = 0, freeSysRC = 0, freeSysRSN = 0;
    freeRC = safeFree64v3(tmpResultBuffer, tmpResultBufferSize,
                          &freeSysRC, &freeSysRSN);
    CMS_DEBUG2(globalArea, traceLevel,
               "GRPALSRV: tmpBuff free'd @ 0x%p (%zu %d %d %d)\n",
               tmpResultBuffer, tmpResultBufferSize,
               freeRC, freeSysRC, freeSysRSN);

    tmpResultBuffer = NULL;

  } while (0);

  cmCopyToSecondaryWithCallerKey(clientParmAddr, &localParmList,
                                 sizeof(ZISGroupAccessListServiceParmList));

  CMS_DEBUG2(globalArea, traceLevel,
             "GRPALSRV: entriesExtracted = %u, RC = %d, "
             "internal RC = %d, RSN = %d, "
             "SAF RC = %d, RACF RC = %d, RSN = %d\n",
             localParmList.entriesExtracted, radminExtractRC,
             localParmList.internalServiceRC,
             localParmList.internalServiceRSN,
             localParmList.safStatus.safRC,
             localParmList.safStatus.racfRC,
             localParmList.safStatus.racfRSN);

  return status;
}

static int validateGroupParmList(ZISGroupAdminServiceParmList *parmList) {

  if (memcmp(parmList->eyecatcher,
             ZIS_GROUP_ADMIN_SERVICE_PARMLIST_EYECATCHER,
             sizeof(parmList->eyecatcher) != 0)) {
    return RC_ZIS_GRPASRV_BAD_EYECATCHER;
  }

  if (parmList->function < ZIS_GROUP_ADMIN_SRV_FC_MIN ||
      ZIS_GROUP_ADMIN_SRV_FC_MAX < parmList->function) {
    return RC_ZIS_GRPASRV_BAD_FUNCTION;
  }

  if (parmList->group.length > ZIS_SECURITY_GROUP_MAX_LENGTH) {
    return RC_ZIS_GRPASRV_GROUP_TOO_LONG;
  }

  if (parmList->superiorGroup.length > ZIS_SECURITY_GROUP_MAX_LENGTH) {
    return RC_ZIS_GRPASRV_SUPGROUP_TOO_LONG;
  }

  const char *productName = parmList->group.value;
  bool isProductSupported = false;
  int productCount =
      sizeof(ZIS_GROUP_PRODUCTS) / sizeof(ZIS_GROUP_PRODUCTS[0]);
  for (int i = 0; i < productCount; i++) {

    const char *currProductName = ZIS_GROUP_PRODUCTS[i];
    size_t currProductNameLength = strlen(currProductName);
    if (currProductNameLength > parmList->group.length) {
      continue;
    }

    if (memcmp(productName, currProductName, currProductNameLength) == 0) {
      isProductSupported = true;
      break;
    }
  }

  if (isProductSupported == false) {
    return RC_ZIS_GRPASRV_UNSUPPORTED_PRODUCT;
  }

  if (parmList->user.length > ZIS_USER_ID_MAX_LENGTH) {
    return RC_ZIS_GRPASRV_USER_ID_TOO_LONG;
  }

  if (parmList->function == ZIS_GROUP_ADMIN_SRV_FC_CONNECT_USER) {
    if (parmList->accessType < ZIS_GROUP_ADMIN_ACESS_TYPE_MIN ||
        ZIS_GROUP_ADMIN_ACESS_TYPE_MAX < parmList->accessType) {
      return RC_ZIS_GRPASRV_BAD_ACCESS_TYPE;
    }
  }

  return RC_ZIS_GRPASRV_OK;
}

static RadminGroupAction
mapZISToRadminGroupAction(ZISGroupAdminFunction function)
{
  switch (function) {
  case ZIS_GROUP_ADMIN_SRV_FC_ADD:
    return RADMIN_GROUP_ACTION_ADD_GROUP;
  case ZIS_GROUP_ADMIN_SRV_FC_DELETE:
    return RADMIN_GROUP_ACTION_DELETE_GROUP;
  default:
    return RADMIN_GROUP_ACTION_NA;
  }
}

static RadminConnectionAction
mapZISToRadminConnectionAction(ZISGroupAdminFunction function)
{
  switch (function) {
  case ZIS_GROUP_ADMIN_SRV_FC_CONNECT_USER:
    return RADMIN_CONNECTION_ACTION_ADD_CONNECTION;
  case ZIS_GROUP_ADMIN_SRV_FC_REMOVE_USER:
    return RADMIN_CONNECTION_ACTION_REMOVE_CONNECTION;
  default:
    return RADMIN_CONNECTION_ACTION_NA;
  }
}

typedef struct GroupOutputHandlerData_tag {
  char eyecatcher[8];
#define GROUP_OUT_HADNLER_DATA_EYECATCHER  "RSGPONDA"
  CrossMemoryServerGlobalArea *globalArea;
  size_t fullMessageLenght;
  size_t fullCommandLength;
  ZISGroupAdminServiceMessage message;
  ZISGroupAdminServiceMessage command;
} GroupOutputHandlerData;

static int getGroupMsgAndOperatorCommand(RadminAPIStatus status,
                                         const RadminCommandOutput *out,
                                         void *userData) {

  GroupOutputHandlerData *handlerData = userData;
  ZISGroupAdminServiceMessage *message = &handlerData->message;
  ZISGroupAdminServiceMessage *command = &handlerData->command;

  bool messageCopied = false, commandCopied = false;

  const RadminCommandOutput *nextOutput = out;
  int nodeCount = 0;
  while (nextOutput != NULL) {

    if (commandCopied && messageCopied) {
      break;
    }
    if (nodeCount > RADMIN_OUT_MAX_NODE_COUNT) {
      return RC_GET_MSG_HADNLER_LOOP;
    }

    if (memcmp(nextOutput->eyecatcher, RADMIN_COMMAND_OUTPUT_EYECATCHER_CMD,
               sizeof(nextOutput->eyecatcher)) == 0) {
      unsigned int copySize =
          min(nextOutput->firstMessageEntry.length, sizeof(command->text));
      memcpy(command->text, nextOutput->firstMessageEntry.text, copySize);
      command->length = copySize;
      handlerData->fullCommandLength = nextOutput->firstMessageEntry.length;
      commandCopied = true;
    }

    if (memcmp(nextOutput->eyecatcher, RADMIN_COMMAND_OUTPUT_EYECATCHER_MSG,
               sizeof(nextOutput->eyecatcher)) == 0) {
      unsigned int copySize =
          min(nextOutput->firstMessageEntry.length, sizeof(message->text));
      memcpy(message->text, nextOutput->firstMessageEntry.text, copySize);
      message->length = copySize;
      handlerData->fullMessageLenght = nextOutput->firstMessageEntry.length;
      messageCopied = true;
    }

    nodeCount++;
    nextOutput = nextOutput->next;
  }

  return RC_GET_MSG_HADNLER_OK;
}


static const char *getGroupAccessString(ZISGroupAdminServiceAccessType type) {
  switch (type) {
  case ZIS_GROUP_ADMIN_ACESS_TYPE_USE:
    return "USE";
  case ZIS_GROUP_ADMIN_ACESS_TYPE_CREATE:
    return "CREATE";
  case ZIS_GROUP_ADMIN_ACESS_TYPE_CONNECT:
    return "CONNECT";
  case ZIS_GROUP_ADMIN_ACESS_TYPE_JOIN:
    return "JOIN";
  default:
    return NULL;
  }
}

#define GROUP_PARMLIST_MIN_SIZE 1024

static RadminGroupParmList *constructGroupAdminParmList(
    char parmListBuffer[],
    size_t bufferSize,
    RadminGroupAction action,
    bool isDryRun,
    const char group[],
    size_t groupLength,
    const char superiorGroup[],
    size_t superiorGroupLength
) {

  if (bufferSize < GROUP_PARMLIST_MIN_SIZE) {
    return NULL;
  }

  memset(parmListBuffer, ' ', bufferSize);

  RadminGroupParmList *parmList = (RadminGroupParmList *)parmListBuffer;

  parmList->groupNameLength = groupLength;
  memcpy(parmList->groupName, group, groupLength);

  if (isDryRun) {
    parmList->flags |= RADMIN_GROUP_FLAG_NORUN;
    parmList->flags |= RADMIN_GROUP_FLAG_RETCMD;
  }

  if (action == RADMIN_GROUP_ACTION_ADD_GROUP) {

    parmList->segmentCount = 1;
    RadminSegment *segment = parmList->segmentData;
    memcpy(segment->name, "BASE    ", sizeof(segment->name));
    segment->flag |= RADMIN_SEGMENT_FLAG_Y;
    segment->fieldCount = 0;

    segment->fieldCount++;
    RadminField *field1 = segment->firstField;
    memcpy(field1->name, "SUPGROUP", sizeof(field1->name));
    field1->flag |= RADMIN_FIELD_FLAG_Y;
    field1->dataLength = superiorGroupLength;
    memcpy(field1->data, superiorGroup, field1->dataLength);

    segment->fieldCount++;
    RadminField *field2 =
        (RadminField *)((char *)(field1 + 1) + field1->dataLength);
    memcpy(field2->name, "UNIVERSL", sizeof(field2->name));
    field2->flag |= RADMIN_FIELD_FLAG_Y;
    field2->dataLength = 0;

  }

  return parmList;
}

static RadminConnectionParmList *constructConnectionAdminParmList(
    char parmListBuffer[],
    size_t bufferSize,
    RadminGroupAction action,
    bool isDryRun,
    const char userID[],
    size_t userIDLength,
    const char group[],
    size_t groupLength,
    ZISGroupAdminServiceAccessType accessType
) {

  if (bufferSize < GROUP_PARMLIST_MIN_SIZE) {
    return NULL;
  }

  memset(parmListBuffer, ' ', bufferSize);

  RadminConnectionParmList *parmList =
      (RadminConnectionParmList *)parmListBuffer;

  parmList->userIDLength = userIDLength;
  memcpy(parmList->userID, userID, userIDLength);

  if (isDryRun) {
    parmList->flags |= RADMIN_CONNECTION_FLAG_NORUN;
    parmList->flags |= RADMIN_CONNECTION_FLAG_RETCMD;
  }
  parmList->segmentCount = 1;
  RadminSegment *segment = parmList->segmentData;
  memcpy(segment->name, "BASE    ", sizeof(segment->name));
  segment->flag |= RADMIN_SEGMENT_FLAG_Y;
  segment->fieldCount = 1;
  RadminField *field1 = segment->firstField;
  memcpy(field1->name, "GROUP   ", sizeof(field1->name));
  field1->flag |= RADMIN_FIELD_FLAG_Y;
  field1->dataLength = groupLength;
  memcpy(field1->data, group, field1->dataLength);

  if (action == RADMIN_CONNECTION_ACTION_ADD_CONNECTION) {

    const char *accessString = getGroupAccessString(accessType);
    size_t accessStringLength = strlen(accessString);

    segment->fieldCount++;
    RadminField *field2 =
        (RadminField *)((char *)(field1 + 1) + field1->dataLength);
    memcpy(field2->name, "AUTH    ", sizeof(field2->name));
    field2->flag |= RADMIN_FIELD_FLAG_Y;
    field2->dataLength = accessStringLength;
    memcpy(field2->data, accessString, field2->dataLength);

  }

  return parmList;
}

int zisGroupAdminServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                                 CrossMemoryService *service, void *parm) {

  int status = RC_ZIS_GRPASRV_OK;
  int radminActionRC = RC_RADMIN_OK;

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_GRPASRV_PARMLIST_NULL;
  }

  ZISGroupAdminServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr,
                                   sizeof(localParmList));

  int parmValidateRC = validateGroupParmList(&localParmList);
  if (parmValidateRC != RC_ZIS_GRPASRV_OK) {
    return parmValidateRC;
  }

  int traceLevel = localParmList.traceLevel;
  if (globalArea->pcLogLevel > traceLevel) {
    traceLevel = globalArea->pcLogLevel;
  }

  do {

    RadminUserID caller;
    if (!secmgmtGetCallerUserID(&caller)) {
      status = RC_ZIS_GRPASRV_IMPERSONATION_MISSING;
      break;
    }

    CMS_DEBUG2(globalArea, traceLevel,
               "GRPASRV: flags = 0x%X, function = %d, group = \'%.*s\', "
               "superior group = \'%.*s\', user = \'%.*s\', access = %d'\n",
               localParmList.flags,
               localParmList.function,
               localParmList.group.length, localParmList.group.value,
               localParmList.superiorGroup.length,
               localParmList.superiorGroup.value,
               localParmList.user.length, localParmList.user.value,
               localParmList.accessType);
    CMS_DEBUG2(globalArea, traceLevel, "GRPASRV: caller = \'%.*s\'\n",
               caller.length, caller.value);

    RadminCallerAuthInfo authInfo = {
        .acee = NULL,
        .userID = caller
    };

    GroupOutputHandlerData handlerData = {
        .eyecatcher = GROUP_OUT_HADNLER_DATA_EYECATCHER,
        .globalArea = globalArea,
        .fullMessageLenght = 0,
        .fullCommandLength = 0
    };

    RadminStatus radminStatus;

    bool isDryRun = (localParmList.flags & ZIS_GROUP_ADMIN_FLAG_DRYRUN) ?
                    true : false;
    char parmListBuffer[2048];

    if (localParmList.function == ZIS_GROUP_ADMIN_SRV_FC_ADD ||
        localParmList.function == ZIS_GROUP_ADMIN_SRV_FC_DELETE) {

      RadminGroupAction action =
          mapZISToRadminGroupAction(localParmList.function);
      if (action == RADMIN_GROUP_ACTION_NA) {
        status = RC_ZIS_GRPASRV_BAD_FUNCTION;
        break;
      }

      RadminGroupParmList *parmList = constructGroupAdminParmList(
          parmListBuffer,
          sizeof(parmListBuffer),
          action, isDryRun,
          localParmList.group.value,
          localParmList.group.length,
          localParmList.superiorGroup.value,
          localParmList.superiorGroup.length
      );

      if (parmList == NULL) {
        status = RC_ZIS_GRPASRV_PARM_INTERNAL_ERROR;
        break;
      }

      radminActionRC = radminPerformGroupAction(
          authInfo,
          action,
          parmList,
          getGroupMsgAndOperatorCommand,
          &handlerData,
          &radminStatus
      );

    } else {

      RadminConnectionAction action =
          mapZISToRadminConnectionAction(localParmList.function);
      if (action == RADMIN_CONNECTION_ACTION_NA) {
        status = RC_ZIS_GRPASRV_BAD_FUNCTION;
        break;
      }

      RadminConnectionParmList *parmList = constructConnectionAdminParmList(
          parmListBuffer,
          sizeof(parmListBuffer),
          action, isDryRun,
          localParmList.user.value,
          localParmList.user.length,
          localParmList.group.value,
          localParmList.group.length,
          localParmList.accessType
      );

      if (parmList == NULL) {
        status = RC_ZIS_GRPASRV_PARM_INTERNAL_ERROR;
        break;
      }

      radminActionRC = radminPerformConnectionAction(
          authInfo,
          action,
          parmList,
          getMsgAndOperatorCommand,
          &handlerData,
          &radminStatus
      );

    }

    localParmList.serviceMessage = handlerData.message;
    localParmList.operatorCommand = handlerData.command;

    if (radminActionRC != RC_RADMIN_OK) {
      localParmList.internalServiceRC = radminActionRC;
      localParmList.internalServiceRSN = radminStatus.reasonCode;
      localParmList.safStatus.safRC = radminStatus.apiStatus.safRC;
      localParmList.safStatus.racfRC = radminStatus.apiStatus.racfRC;
      localParmList.safStatus.racfRSN = radminStatus.apiStatus.racfRSN;
      status = RC_ZIS_GRPASRV_INTERNAL_SERVICE_FAILED;
      break;
    }

  } while (0);

  cmCopyToSecondaryWithCallerKey(clientParmAddr, &localParmList,
                                 sizeof(ZISGroupAdminServiceParmList));

  CMS_DEBUG2(globalArea, traceLevel,
             "GRPASRV: status = %d, msgLen = %zu, cmdLen = %zu, "
             "RC = %d, internal RC = %d, RSN = %d, "
             "SAF RC = %d, RACF RC = %d, RSN = %d\n",
             status,
             localParmList.serviceMessage.length,
             localParmList.operatorCommand.length,
             radminActionRC,
             localParmList.internalServiceRC,
             localParmList.internalServiceRSN,
             localParmList.safStatus.safRC,
             localParmList.safStatus.racfRC,
             localParmList.safStatus.racfRSN);

  return status;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

