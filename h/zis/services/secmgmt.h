

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef ZIS_SERVICES_SECMGMT_H_
#define ZIS_SERVICES_SECMGMT_H_

#include "zowetypes.h"
#include "zos.h"

/*** Common security structs and definitions ***/

#define ZIS_SECURITY_CLASS_MAX_LENGTH       8
#define ZIS_SECURITY_PROFILE_MAX_LENGTH     255 /* 246->255: Top Secret has a max profile of 255 and needs to use this struct. */
#define ZIS_SECURITY_GROUP_MAX_LENGTH       8
#define ZIS_USER_ID_MAX_LENGTH              8

#define ZIS_PARMLIB_PARM_SECMGMT_USER_CLASS   CMS_PROD_ID".SECMGMT.CLASS"
#define ZIS_PARMLIB_PARM_SECMGMT_AUTORESFRESH CMS_PROD_ID".SECMGMT.AUTOREFRESH"

typedef struct SAFStatus_tag {
  int safRC;
  int racfRC;
  int racfRSN;
} SAFStatus;

typedef struct ZISSecurityClass_tag {
  uint8_t length;
  char value[ZIS_SECURITY_CLASS_MAX_LENGTH];
  char padding[7];
} ZISSecurityClass;

typedef struct ZISSecurityProfile_tag {
  uint8_t length;
  char value[ZIS_SECURITY_PROFILE_MAX_LENGTH];
  char padding[1];
} ZISSecurityProfile;

typedef struct ZISSecurityGroup_tag {
  uint8_t length;
  char value[ZIS_SECURITY_GROUP_MAX_LENGTH];
  char padding[7];
} ZISSecurityGroup;

typedef struct ZISUserID_tag {
  uint8_t length;
  char value[ZIS_USER_ID_MAX_LENGTH];
  char padding[7];
} ZISUserID;

/*** User profile service ***/

#define ZIS_SERVICE_ID_USERPROF_SRV               14

ZOWE_PRAGMA_PACK

typedef struct ZISUserProfileEntry_tag {
  char userID[8];
  char defaultGroup[8];
  char name[32]; /* 20->32: Top Secret has a max name of 32 and needs to use this struct. */
} ZISUserProfileEntry;

typedef struct ZISUserProfileServiceParmList_tag {

  /* input-output fields */
  char eyecatcher[8];
#define ZIS_USERPROF_SERVICE_PARMLIST_EYECATCHER "RSUPROFS"
  int version;
  int flags;
  int traceLevel;

  ZISUserID startUserID;
  unsigned int profilesToExtract;
  PAD_LONG(x00, ZISUserProfileEntry *resultBuffer);

  /* output fields */
  unsigned int profilesExtracted;
  int internalServiceRC;
  int internalServiceRSN;
  SAFStatus safStatus;

} ZISUserProfileServiceParmList;

ZOWE_PRAGMA_PACK_RESET

#pragma map(zisUserProfilesServiceFunction, "ZISSXUPR")
int zisUserProfilesServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                                   CrossMemoryService *service, void *parm);
int validateUserProfileParmList(ZISUserProfileServiceParmList *parm);

#define RC_ZIS_UPRFSRV_OK                         0
#define RC_ZIS_UPRFSRV_PARMLIST_NULL              8
#define RC_ZIS_UPRFSRV_BAD_EYECATCHER             9
#define RC_ZIS_UPRFSRV_USER_ID_TOO_LONG           10
#define RC_ZIS_UPRFSRV_RESULT_BUFF_NULL           11
#define RC_ZIS_UPRFSRV_PROFILE_COUNT_NULL         12
#define RC_ZIS_UPRFSRV_IMPERSONATION_MISSING      13
#define RC_ZIS_UPRFSRV_INTERNAL_SERVICE_FAILED    14
#define RC_ZIS_UPRFSRV_ALLOC_FAILED               15
#define RC_ZIS_UPRFSRV_UNSUPPORTED_ESM            16
#define RC_ZIS_UPRFSRV_MAX_RC                     16

/*** Genres profile service ***/

#define ZIS_SERVICE_ID_GRESPROF_SRV               15

ZOWE_PRAGMA_PACK

typedef struct ZISGenresProfileEntry_tag {
  char profile[ZIS_SECURITY_PROFILE_MAX_LENGTH];
  char owner[8];
} ZISGenresProfileEntry;

typedef struct ZISGenresProfileServiceParmList_tag {

  /* input-output fields */
  char eyecatcher[8];
#define ZIS_GRESPROF_SERVICE_PARMLIST_EYECATCHER "RSGPROFS"
  int version;
  int flags;
  int traceLevel;

  ZISSecurityClass class;
  ZISSecurityProfile startProfile;
  unsigned int profilesToExtract;
  PAD_LONG(x00, ZISGenresProfileEntry *resultBuffer);

  /* output fields */
  unsigned int profilesExtracted;
  int internalServiceRC;
  int internalServiceRSN;
  SAFStatus safStatus;

} ZISGenresProfileServiceParmList;

ZOWE_PRAGMA_PACK_RESET

#pragma map(zisGenresProfilesServiceFunction, "ZISSXGRP")
int zisGenresProfilesServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                                     CrossMemoryService *service, void *parm);
int validateGenresProfileParmList(ZISGenresProfileServiceParmList *parm);

#define RC_ZIS_GRPRFSRV_OK                        0
#define RC_ZIS_GRPRFSRV_PARMLIST_NULL             8
#define RC_ZIS_GRPRFSRV_BAD_EYECATCHER            9
#define RC_ZIS_GRPRFSRV_CLASS_TOO_LONG            10
#define RC_ZIS_GRPRFSRV_PROFILE_TOO_LONG          11
#define RC_ZIS_GRPRFSRV_RESULT_BUFF_NULL          12
#define RC_ZIS_GRPRFSRV_PROFILE_COUNT_NULL        14
#define RC_ZIS_GRPRFSRV_IMPERSONATION_MISSING     15
#define RC_ZIS_GRPRFSRV_INTERNAL_SERVICE_FAILED   16
#define RC_ZIS_GRPRFSRV_ALLOC_FAILED              17
#define RC_ZIS_GRPRFSRV_USER_CLASS_NOT_READ       18
#define RC_ZIS_GRPRFSRV_UNSUPPORTED_ESM           19
#define RC_ZIS_GRPRFSRV_MAX_RC                    19

/*** General resource access list service ***/

#define ZIS_SERVICE_ID_ACSLIST_SRV                16

ZOWE_PRAGMA_PACK

typedef struct ZISGenresAccessEntry_tag {
  char id[8];
  char accessType[8];
} ZISGenresAccessEntry;

typedef struct ZISAccessListServiceParmList_tag {

  /* input-output fields */
  char eyecatcher[8];
#define ZIS_ACSLIST_SERVICE_PARMLIST_EYECATCHER "RSACSLST"
  int version;
  int flags;
  int traceLevel;

  ZISSecurityClass class;
  ZISSecurityProfile profile;
  unsigned int resultBufferCapacity;
  PAD_LONG(x00, ZISGenresAccessEntry *resultBuffer);

  /* output fields */
  unsigned int entriesExtracted;
  unsigned int entriesFound;
  int internalServiceRC;
  int internalServiceRSN;
  SAFStatus safStatus;

} ZISGenresAccessListServiceParmList;

ZOWE_PRAGMA_PACK_RESET

#pragma map(zisGenresAccessListServiceFunction, "ZISSXGAL")
int zisGenresAccessListServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                                       CrossMemoryService *service, void *parm);
int validateGenresAccessListParmList(ZISGenresAccessListServiceParmList *parm);

#define RC_ZIS_ACSLSRV_OK                         0
#define RC_ZIS_ACSLSRV_PARMLIST_NULL              8
#define RC_ZIS_ACSLSRV_BAD_EYECATCHER             9
#define RC_ZIS_ACSLSRV_CLASS_TOO_LONG             10
#define RC_ZIS_ACSLSRV_PROFILE_TOO_LONG           11
#define RC_ZIS_ACSLSRV_RESULT_BUFF_NULL           12
#define RC_ZIS_ACSLSRV_IMPERSONATION_MISSING      13
#define RC_ZIS_ACSLSRV_ALLOC_FAILED               14
#define RC_ZIS_ACSLSRV_INTERNAL_SERVICE_FAILED    15
#define RC_ZIS_ACSLSRV_INSUFFICIENT_SPACE         16
#define RC_ZIS_ACSLSRV_USER_CLASS_NOT_READ        17
#define RC_ZIS_ACSLSRV_UNSUPPORTED_ESM            18
#define RC_ZIS_ACSLSRV_MAX_RC                     18

/*** General resource profile administration service ***/

#define ZIS_SERVICE_ID_GENRES_ADMIN_SRV           17

#pragma enum(2)

typedef enum ZISGenresAdminFunction_tag {
  ZIS_GENRES_ADMIN_SRV_FC_MIN = 0,
  ZIS_GENRES_ADMIN_SRV_FC_DEFINE = 0,
  ZIS_GENRES_ADMIN_SRV_FC_DELETE = 1,
  ZIS_GENRES_ADMIN_SRV_FC_GIVE_ACCESS = 2,
  ZIS_GENRES_ADMIN_SRV_FC_REVOKE_ACCESS = 3,
  ZIS_GENRES_ADMIN_SRV_FC_MAX = 3,
} ZISGenresAdminFunction;

typedef enum ZISGenresAdminServiceAccessType_tag {
  ZIS_GENRES_ADMIN_ACESS_TYPE_UNKNOWN = -1,
  ZIS_GENRES_ADMIN_ACESS_TYPE_MIN = 0,
  ZIS_GENRES_ADMIN_ACESS_TYPE_READ = 0,
  ZIS_GENRES_ADMIN_ACESS_TYPE_UPDATE = 1,
  ZIS_GENRES_ADMIN_ACESS_TYPE_ALTER = 2,
  ZIS_GENRES_ADMIN_ACESS_TYPE_ALL = 3, /* Top Secret doesn't have alter. */
  ZIS_GENRES_ADMIN_ACESS_TYPE_MAX = 3,
} ZISGenresAdminServiceAccessType;

#pragma enum(reset)

static const char *ZIS_GENRES_PRODUCTS[] = {
  "ZOWE"
};

ZOWE_PRAGMA_PACK

typedef struct ZISGenresAdminServiceMessage_tag {
  uint16_t length;
  char text[510];
} ZISGenresAdminServiceMessage;

typedef struct ZISGenresAdminServiceParmList_tag {

  /* input fields */
  char eyecatcher[8];
#define ZIS_GENRES_ADMIN_SERVICE_PARMLIST_EYECATCHER "RSGRADMN"
  int version;
  int flags;
#define ZIS_GENRES_ADMIN_FLAG_DRYRUN  0x00000001
  int traceLevel;

  ZISGenresAdminFunction function;
  ZISSecurityProfile profile;
  ZISUserID owner;
  ZISUserID user;
  ZISGenresAdminServiceAccessType accessType;

  /* output fields */
  int internalServiceRC;
  int internalServiceRSN;
  SAFStatus safStatus;
  ZISGenresAdminServiceMessage serviceMessage;
  ZISGenresAdminServiceMessage operatorCommand;

} ZISGenresAdminServiceParmList;

ZOWE_PRAGMA_PACK_RESET


#pragma map(zisGenresProfileAdminServiceFunction, "ZISSXPAA")
int zisGenresProfileAdminServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                                         CrossMemoryService *service, void *parm);
int validateGenresParmList(ZISGenresAdminServiceParmList *parmList);

#define RC_ZIS_GSADMNSRV_OK                       0
#define RC_ZIS_GSADMNSRV_PARMLIST_NULL            8
#define RC_ZIS_GSADMNSRV_BAD_EYECATCHER           9
#define RC_ZIS_GSADMNSRV_PROF_TOO_LONG            10
#define RC_ZIS_GSADMNSRV_UNSUPPORTED_PRODUCT      11
#define RC_ZIS_GSADMNSRV_USER_ID_NULL             12
#define RC_ZIS_GSADMNSRV_USER_ID_TOO_LONG         13
#define RC_ZIS_GSADMNSRV_IMPERSONATION_MISSING    14
#define RC_ZIS_GSADMNSRV_BAD_FUNCTION             15
#define RC_ZIS_GSADMNSRV_BAD_ACCESS_TYPE          16
#define RC_ZIS_GSADMNSRV_INTERNAL_SERVICE_FAILED  17
#define RC_ZIS_GSADMNSRV_PARM_INTERNAL_ERROR      18
#define RC_ZIS_GSADMNSRV_USER_CLASS_TOO_LONG      19
#define RC_ZIS_GSADMNSRV_USER_CLASS_NOT_READ      20
#define RC_ZIS_GSADMNSRV_AUTOREFRESH_NOT_READ     21
#define RC_ZIS_GSADMNSRV_OWNER_TOO_LONG           22
#define RC_ZIS_GSADMNSRV_UNSUPPORTED_ESM          23
#define RC_ZIS_GSADMNSRV_MAX_RC                   23

/*** Group profile service ***/

#define ZIS_SERVICE_ID_GRPPROF_SRV                18

ZOWE_PRAGMA_PACK

typedef struct ZISGroupProfileEntry_tag {
  char group[ZIS_SECURITY_GROUP_MAX_LENGTH];
  char superiorGroup[ZIS_SECURITY_GROUP_MAX_LENGTH];
  char owner[8];
} ZISGroupProfileEntry;

typedef struct ZISGroupProfileServiceParmList_tag {

  /* input-output fields */
  char eyecatcher[8];
#define ZIS_GRPPROF_SERVICE_PARMLIST_EYECATCHER "RSGRPPRF"
  int version;
  int flags;
  int traceLevel;

  ZISSecurityGroup startGroup;
  unsigned int profilesToExtract;
  PAD_LONG(x00, ZISGroupProfileEntry *resultBuffer);

  /* output fields */
  unsigned int profilesExtracted;
  int internalServiceRC;
  int internalServiceRSN;
  SAFStatus safStatus;

} ZISGroupProfileServiceParmList;

ZOWE_PRAGMA_PACK_RESET

#pragma map(zisGroupProfilesServiceFunction, "ZISSXGPP")
int zisGroupProfilesServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                                    CrossMemoryService *service, void *parm);
int validateGroupProfileParmList(ZISGroupProfileServiceParmList *parm);

#define RC_ZIS_GPPRFSRV_OK                        0
#define RC_ZIS_GPPRFSRV_PARMLIST_NULL             8
#define RC_ZIS_GPPRFSRV_BAD_EYECATCHER            9
#define RC_ZIS_GPPRFSRV_GROUP_TOO_LONG            10
#define RC_ZIS_GPPRFSRV_RESULT_BUFF_NULL          11
#define RC_ZIS_GPPRFSRV_PROFILE_COUNT_NULL        12
#define RC_ZIS_GPPRFSRV_IMPERSONATION_MISSING     13
#define RC_ZIS_GPPRFSRV_INTERNAL_SERVICE_FAILED   14
#define RC_ZIS_GPPRFSRV_ALLOC_FAILED              15
#define RC_ZIS_GPPRFSRV_UNSUPPORTED_ESM           16
#define RC_ZIS_GPPRFSRV_MAX_RC                    16

/*** Group access list service ***/

#define ZIS_SERVICE_ID_GRPALIST_SRV               19

ZOWE_PRAGMA_PACK

typedef struct ZISGroupAccessEntry_tag {
  char id[8];
  char accessType[8];
} ZISGroupAccessEntry;

typedef struct ZISGroupAccessListServiceParmList_tag {

  /* input-output fields */
  char eyecatcher[8];
#define ZIS_GRPALST_SERVICE_PARMLIST_EYECATCHER "RSGRPALST"
  int version;
  int flags;
  int traceLevel;

  ZISSecurityGroup group;
  unsigned int resultBufferCapacity;
  PAD_LONG(x00, ZISGroupAccessEntry *resultBuffer);

  /* output fields */
  unsigned int entriesExtracted;
  unsigned int entriesFound;
  int internalServiceRC;
  int internalServiceRSN;
  SAFStatus safStatus;

} ZISGroupAccessListServiceParmList;

ZOWE_PRAGMA_PACK_RESET


#pragma map(zisGroupAccessListServiceFunction, "ZISSXGPA")
int zisGroupAccessListServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                                      CrossMemoryService *service, void *parm);
int validateGroupAccessListParmList(ZISGroupAccessListServiceParmList *parm);

#define RC_ZIS_GRPALSRV_OK                        0
#define RC_ZIS_GRPALSRV_PARMLIST_NULL             8
#define RC_ZIS_GRPALSRV_BAD_EYECATCHER            9
#define RC_ZIS_GRPALSRV_GROUP_TOO_LONG            10
#define RC_ZIS_GRPALSRV_RESULT_BUFF_NULL          11
#define RC_ZIS_GRPALSRV_IMPERSONATION_MISSING     12
#define RC_ZIS_GRPALSRV_ALLOC_FAILED              13
#define RC_ZIS_GRPALSRV_INTERNAL_SERVICE_FAILED   14
#define RC_ZIS_GRPALSRV_INSUFFICIENT_SPACE        15
#define RC_ZIS_GRPALSRV_UNSUPPORTED_ESM           16
#define RC_ZIS_GRPALSRV_MAX_RC                    16

/*** Group administration service ***/

#define ZIS_SERVICE_ID_GROUP_ADMIN_SRV            20

#pragma enum(2)

typedef enum ZISGroupAdminFunction_tag {
  ZIS_GROUP_ADMIN_SRV_FC_MIN = 0,
  ZIS_GROUP_ADMIN_SRV_FC_ADD = 0,
  ZIS_GROUP_ADMIN_SRV_FC_DELETE = 1,
  ZIS_GROUP_ADMIN_SRV_FC_CONNECT_USER = 2,
  ZIS_GROUP_ADMIN_SRV_FC_REMOVE_USER = 3,
  ZIS_GROUP_ADMIN_SRV_FC_MAX = 3,
} ZISGroupAdminFunction;

typedef enum ZISGroupAdminServiceAccessType_tag {
  ZIS_GROUP_ADMIN_ACESS_TYPE_UNKNOWN = -1,
  ZIS_GROUP_ADMIN_ACESS_TYPE_MIN = 0,
  ZIS_GROUP_ADMIN_ACESS_TYPE_USE = 0,
  ZIS_GROUP_ADMIN_ACESS_TYPE_CREATE = 1,
  ZIS_GROUP_ADMIN_ACESS_TYPE_CONNECT = 2,
  ZIS_GROUP_ADMIN_ACESS_TYPE_JOIN = 3,
  ZIS_GROUP_ADMIN_ACESS_TYPE_MAX = 3,
} ZISGroupAdminServiceAccessType;

#pragma enum(reset)

static const char *ZIS_GROUP_PRODUCTS[] = {
  "ZWE",
  "ZOWE",
  "MVD",
  "ZIS"
};

ZOWE_PRAGMA_PACK

typedef struct ZISGroupAdminServiceMessage_tag {
  uint16_t length;
  char text[510];
} ZISGroupAdminServiceMessage;

typedef struct ZISGroupAdminServiceParmList_tag {

  /* input fields */
  char eyecatcher[8];
#define ZIS_GROUP_ADMIN_SERVICE_PARMLIST_EYECATCHER "RSGPADMN"
  int version;
  int flags;
#define ZIS_GROUP_ADMIN_FLAG_DRYRUN  0x00000001
  int traceLevel;

  ZISGroupAdminFunction function;
  ZISSecurityGroup group;
  ZISSecurityGroup superiorGroup;
  ZISUserID user;
  ZISGroupAdminServiceAccessType accessType;

  /* output fields */
  int internalServiceRC;
  int internalServiceRSN;
  SAFStatus safStatus;
  ZISGroupAdminServiceMessage serviceMessage;
  ZISGroupAdminServiceMessage operatorCommand;

} ZISGroupAdminServiceParmList;

ZOWE_PRAGMA_PACK_RESET

#pragma map(zisGroupAdminServiceFunction, "ZISSXPGA")
int zisGroupAdminServiceFunction(CrossMemoryServerGlobalArea *globalArea,
                                 CrossMemoryService *service, void *parm);
int validateGroupParmList(ZISGroupAdminServiceParmList *parmList);

#define RC_ZIS_GRPASRV_OK                         0
#define RC_ZIS_GRPASRV_PARMLIST_NULL              8
#define RC_ZIS_GRPASRV_BAD_EYECATCHER             9
#define RC_ZIS_GRPASRV_GROUP_TOO_LONG             10
#define RC_ZIS_GRPASRV_SUPGROUP_TOO_LONG          11
#define RC_ZIS_GRPASRV_UNSUPPORTED_PRODUCT        12
#define RC_ZIS_GRPASRV_USER_ID_TOO_LONG           13
#define RC_ZIS_GRPASRV_IMPERSONATION_MISSING      14
#define RC_ZIS_GRPASRV_BAD_FUNCTION               15
#define RC_ZIS_GRPASRV_BAD_ACCESS_TYPE            16
#define RC_ZIS_GRPASRV_INTERNAL_SERVICE_FAILED    17
#define RC_ZIS_GRPASRV_PARM_INTERNAL_ERROR        18
#define RC_ZIS_GRPASRV_USER_CLASS_TOO_LONG        19
#define RC_ZIS_GRPASRV_USER_CLASS_NOT_READ        20
#define RC_ZIS_GRPASRV_AUTOREFRESH_NOT_READ       21
#define RC_ZIS_GRPASRV_UNSUPPORTED_ESM            22
#define RC_ZIS_GRPASRV_MAX_RC                     22

#endif /* ZIS_SERVICES_SECMGMT_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
