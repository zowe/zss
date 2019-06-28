

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "charsets.h"
#include "bpxnet.h"
#include "socketmgmt.h"
#include "httpserver.h"
#include "logging.h"
#include "json.h"

#include "securityService.h"
#include "zssLogging.h"
#include "serviceUtils.h"

#include "zis/client.h"
#include "zis/services/secmgmt.h"


/* TODO there is a huge amount of boiler plate and copy-paste code in
 * the format and respond functions, consider extracting abstraction
 * once these services have stabilized. */

/************ Common helper functions ************/

#define ERROR_MSG_MAX_SIZE 4096

typedef void (ZISSvcErrorMessageFormatter)(int zisCallRC,
                                           const void *serviceStatus,
                                           char *messageBuffer,
                                           size_t messageBufferSize);

static void formatZISErrorMessage(int zisCallRC,
                                  const void *serviceStatus,
                                  ZISSvcErrorMessageFormatter *formatter,
                                  char *messageBuffer,
                                  size_t messageBufferSize) {

  const ZISServiceStatus *baseStatus = serviceStatus;

  if (zisCallRC == RC_ZIS_SRVC_OK) {
    return;
  }

  if (zisCallRC == RC_ZIS_SRVC_STATUS_NULL) {
    snprintf(messageBuffer, messageBufferSize,
             "ZIS status block not provided");
    return;
  }

  if (zisCallRC == RC_ZIS_SRVC_CMS_FAILED) {
    int cmsRC = baseStatus->cmsRC;
    int cmsRSN = baseStatus->cmsRSN;
    if (0 <= cmsRC && cmsRC <= RC_CMS_MAX_RC && CMS_RC_DESCRIPTION[cmsRC]) {
      snprintf(messageBuffer, messageBufferSize,
               "Cross-Memory server failure (RC = %d, RSN = %d) - %s",
               baseStatus->cmsRC, baseStatus->cmsRSN,
               CMS_RC_DESCRIPTION[cmsRC]);
    } else {
      snprintf(messageBuffer, messageBufferSize,
               "Cross-Memory server failure (RC = %d, RSN = %d)",
               baseStatus->cmsRC, baseStatus->cmsRSN);
    }
    return;
  }

  if (zisCallRC < 0) {
    snprintf(messageBuffer, messageBufferSize,
             "Unknown ZIS error (RC = %d)", zisCallRC);
    return;
  }

  if (zisCallRC == RC_ZIS_SRVC_SERVICE_FAILED && baseStatus->serviceRC < 0) {
    snprintf(messageBuffer, messageBufferSize,
             "Unknown ZIS service error (RC = %d, RSN = %d)",
             baseStatus->serviceRC, baseStatus->serviceRSN);
    return;
  }

  if (formatter == NULL) {
    snprintf(messageBuffer, messageBufferSize,
             "Cross-Memory service failure (RC = %d, RSN = %d)",
             baseStatus->serviceRC, baseStatus->serviceRSN);
  } else {
    formatter(zisCallRC, baseStatus, messageBuffer, messageBufferSize);
  }

}

static bool getDryRunValue(HttpRequest *request) {

  HttpRequestParam *realEntityParam = getCheckedParam(request, "dryRun");
  char *realEntityParmString =
      (realEntityParam ? realEntityParam->stringValue : "");
  if (!strcmp(realEntityParmString, "false")) {
    return false;
  }

  return true;
}

static int getTraceLevel(HttpRequest *request) {
  HttpRequestParam *traceParam = getCheckedParam(request, "traceLevel");
  return (traceParam ? traceParam->intValue : 0);
}

static void respondWithZISError(HttpResponse *response,
                                int zisRC,
                                const void *status,
                                ZISSvcErrorMessageFormatter *formatter) {

  jsonPrinter *printer = respondWithJsonPrinter(response);
  setResponseStatus(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Error");
  setContentType(response, "text/json");
  addStringHeader(response, "Server", "jdmfws");
  addStringHeader(response, "Transfer-Encoding", "chunked");
  writeHeader(response);

  jsonStart(printer);
  {
    printErrorResponseMetadata(printer);
    char errorMessage[ERROR_MSG_MAX_SIZE] = {0};
    formatZISErrorMessage(zisRC, status, formatter,
                          errorMessage, sizeof(errorMessage));
    jsonAddString(printer, "message", errorMessage);
  }
  jsonEnd(printer);

  finishResponse(response);

}

/************ User-profile functions ************/

#define USER_PROFILE_MAX_COUNT  1000

static int getUserProfileCount(HttpRequest *request) {
  HttpRequestParam *countParam = getCheckedParam(request, "count");
  return (countParam ? countParam->intValue : USER_PROFILE_MAX_COUNT);
}

static void printUserProfileToJSON(jsonPrinter *printer,
                                   const ZISUserProfileEntry *entry) {

  char userIDNullTerm[sizeof(entry->userID) + 1] = {0};
  char defaultGroupNullTerm[sizeof(entry->defaultGroup) + 1] = {0};
  char nameNullTerm[sizeof(entry->name) + 1] = {0};
  memcpy(userIDNullTerm, entry->userID, sizeof(entry->userID));
  memcpy(defaultGroupNullTerm, entry->defaultGroup,
         sizeof(entry->defaultGroup));
  memcpy(nameNullTerm, entry->name, sizeof(entry->name));

  nullTerminate(userIDNullTerm, sizeof(userIDNullTerm) - 1);
  nullTerminate(defaultGroupNullTerm, sizeof(defaultGroupNullTerm) - 1);
  nullTerminate(nameNullTerm, sizeof(nameNullTerm) - 1);

  jsonStartObject(printer, NULL);
  {
    jsonAddString(printer, "userID", userIDNullTerm);
    jsonAddString(printer, "defaultGroup", defaultGroupNullTerm);
    jsonAddString(printer, "name", nameNullTerm);
  }
  jsonEndObject(printer);

}

static void formatZISUserProfileServiceError(int zisCallRC,
                                             const void *serviceStatus,
                                             char *messageBuffer,
                                             size_t messageBufferSize) {

  const ZISUserProfileServiceStatus *status = serviceStatus;

  if (zisCallRC == RC_ZIS_SRVC_SERVICE_FAILED) {

    int serviceRC = status->baseStatus.serviceRC;
    int serviceRSN = status->baseStatus.serviceRSN;
    if (serviceRC > RC_ZIS_UPRFSRV_MAX_RC) {
      snprintf(messageBuffer, messageBufferSize,
               "Unknown ZIS service error (RC = %d, RSN = %d)",
               serviceRC, serviceRSN);
      return;
    }

    const char *description = ZIS_UPRFSRV_SERVICE_RC_DESCRIPTION[serviceRC];
    if (serviceRC == RC_ZIS_UPRFSRV_INTERNAL_SERVICE_FAILED) {
      snprintf(messageBuffer, messageBufferSize,
               "R_admin failed (RC = %d, SAF RC = %d, RACF RC = %d, RSN = %d)",
               status->internalServiceRC,
               status->safStatus.safRC,
               status->safStatus.racfRC,
               status->safStatus.racfRSN);
    } else {
      snprintf(messageBuffer, messageBufferSize,
               "ZIS service failed (RC = %d, RSN = %d) - %s",
               serviceRC, serviceRSN, description);
    }
    return;

  }

  if (zisCallRC > RC_ZIS_SRVC_UPRFSRV_MAX_RC) {
    snprintf(messageBuffer, messageBufferSize,
             "Unknown ZIS error (RC = %d)", zisCallRC);
    return;
  }

  const char *description = ZIS_UPRFSRV_WRAPPER_RC_DESCRIPTION[zisCallRC];
  snprintf(messageBuffer, messageBufferSize, "ZIS failed (RC = %d) - %s",
           zisCallRC, description);

}

static void respondWithUserProfiles(HttpResponse *response,
                                    const char *startProfile,
                                    unsigned int profilesRequested,
                                    const ZISUserProfileEntry *profiles,
                                    unsigned int profileCount) {

  jsonPrinter *printer = respondWithJsonPrinter(response);
  setResponseStatus(response, HTTP_STATUS_OK, "OK");
  setContentType(response, "text/json");
  addStringHeader(response, "Server", "jdmfws");
  addStringHeader(response, "Transfer-Encoding", "chunked");
  writeHeader(response);

  jsonStart(printer);

  printTableResultResponseMetadata(printer);
  jsonAddString(printer, "resultType", "table");
  jsonStartObject(printer, "metaData");
  {

    jsonStartObject(printer, "tableMetaData");
    {
      jsonAddString(printer, "tableIdentifier", "user-profiles");
      jsonAddString(printer, "shortTableLabel", "user-profiles");
      jsonAddString(printer, "longTableLabel", "User Profiles");
      jsonAddString(printer, "globalIdentifier",
                    "org.zowe.zss.security-mgmt.user-profiles");
      jsonAddInt(printer, "minRows", 0);
      jsonAddInt(printer, "maxRows", USER_PROFILE_MAX_COUNT);
    }
    jsonEndObject(printer);

    jsonStartArray(printer, "columnMetaData");
    {

      jsonStartObject(printer, NULL);
      {
        jsonAddString(printer, "columnIdentifier", "userID");
        jsonAddString(printer, "shortColumnLabel", "userID");
        jsonAddString(printer, "longColumnLabel", "User ID");
        jsonAddString(printer, "rawDataType", "string");
        jsonAddInt(printer, "rawDataTypeLength", sizeof(profiles[0].userID));
      }
      jsonEndObject(printer);

      jsonStartObject(printer, NULL);
      {
        jsonAddString(printer, "columnIdentifier", "defaultGroup");
        jsonAddString(printer, "shortColumnLabel", "defaultGroup");
        jsonAddString(printer, "longColumnLabel", "Default Group");
        jsonAddString(printer, "rawDataType", "string");
        jsonAddInt(printer, "rawDataTypeLength",
                   sizeof(profiles[0].defaultGroup));
      }
      jsonEndObject(printer);

      jsonStartObject(printer, NULL);
      {
        jsonAddString(printer, "columnIdentifier", "name");
        jsonAddString(printer, "shortColumnLabel", "name");
        jsonAddString(printer, "longColumnLabel", "User Name");
        jsonAddString(printer, "rawDataType", "string");
        jsonAddInt(printer, "rawDataTypeLength", sizeof(profiles[0].name));
      }
      jsonEndObject(printer);

    }
    jsonEndArray(printer);

  }
  jsonEndObject(printer);

  int profilesToReturn = (profileCount > profilesRequested) ?
                          profileCount - 1 : profileCount;

  jsonAddString(printer, "start", startProfile ? (char *)startProfile : "");
  jsonAddInt(printer, "count", profilesToReturn);
  jsonAddInt(printer, "countRequested", profilesRequested);
  if (profilesRequested < profileCount) {
    jsonAddBoolean(printer, "hasMore", TRUE);
  } else {
    jsonAddBoolean(printer, "hasMore", FALSE);
  }

  jsonStartArray(printer, "rows");
  {
    for (int i = 0; i < profilesToReturn; i++) {
      const ZISUserProfileEntry *entry = &profiles[i];
      printUserProfileToJSON(printer, entry);
      zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
              "user profile entry #%d: \'%.*s\', \'%.*s\', \'%.*s\'\n", i,
              sizeof(profiles->userID), profiles->userID,
              sizeof(profiles->defaultGroup), profiles->defaultGroup,
              sizeof(profiles->name), profiles->name);
    }
  }
  jsonEndArray(printer);

  jsonEnd(printer);

  finishResponse(response);

}

/* The endpoint for serving RACF user profiles. Currently, we do not
 * parse the data. This is left to the caller to do once receiving
 * the entirety of the data.
 */
static int serveUserProfile(HttpService *service, HttpResponse *response) {

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  HttpRequest *request = response->request;

  /* A GET request is the only supported method for this
   * route.
   */
  if (strcmp(request->method, methodGET) != 0) {
    respondWithError(response, HTTP_STATUS_NOT_IMPLEMENTED,
                     "Unsupported request");
    return 0;
  }

  /* Gets the CrossMemoryServerName, which is needed
   * to use ZIS functions.
   */
  CrossMemoryServerName *privilegedServerName =
      getConfiguredProperty(service->server,
                            HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY);

  int zisRC = RC_ZIS_SRVC_OK;
  char *startUserID = getQueryParam(response->request, "start");
  int profilesToExtract = getUserProfileCount(request);
  if (profilesToExtract < 0 || USER_PROFILE_MAX_COUNT < profilesToExtract) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Count out of range");
    return 0;
  }

  ZISUserProfileEntry resultBuffer[USER_PROFILE_MAX_COUNT + 1];
  unsigned int profilesExtracted = 0;
  ZISUserProfileServiceStatus status = {0};
  int traceLevel = getTraceLevel(request);

  zisRC = zisExtractUserProfiles(
      privilegedServerName,
      startUserID,
      profilesToExtract + 1,
      resultBuffer,
      &profilesExtracted,
      &status,
      traceLevel
  );

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisExtractUserProfiles: RC=%d, base=(%d,%d,%d,%d), "
         "internal=(%d,%d), SAF=(%d,%d,%d)\n", zisRC,
         status.baseStatus.cmsRC, status.baseStatus.cmsRSN,
         status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
         status.internalServiceRC, status.internalServiceRSN,
         status.safStatus.safRC, status.safStatus.racfRC,
         status.safStatus.racfRSN);

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisExtractUserProfiles: start=%s, profiles to extract = %u, "
         "extracted = %u\n", startUserID ? startUserID : "n/a",
         profilesToExtract, profilesExtracted);

  if (zisRC == RC_ZIS_SRVC_OK) {
    respondWithUserProfiles(response, startUserID, profilesToExtract,
                            resultBuffer, profilesExtracted);
  } else {
    respondWithZISError(response, zisRC, &status,
                        formatZISUserProfileServiceError);
  }

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);

  return 0;
}

/************ Class management functions ************/

/* TODO this is copy-pasted from zss.c, move somewhere before the PR */
#define JSON_ERROR_BUFFER_SIZE 1024
static JsonObject * getJsonBody(char *content, int contentLength,
                                ShortLivedHeap *slh) {

  char errorBuffer[JSON_ERROR_BUFFER_SIZE];
  char *ebcdicBuffer = safeMalloc(contentLength * 2,
                                  "Test Echo Ebcdic Buffer");
  int conversionLength = 0;
  int reasonCode = 0;
  int status = convertCharset(content,
                              contentLength,
                              CCSID_UTF_8,
                              CHARSET_OUTPUT_USE_BUFFER,
                              &ebcdicBuffer,
                              contentLength * 2,
                              CCSID_EBCDIC_1047,
                              NULL,
                              &conversionLength,
                              &reasonCode);
  Json * body = jsonParseUnterminatedString(
      slh,
      ebcdicBuffer,
      conversionLength,
      errorBuffer,
      JSON_ERROR_BUFFER_SIZE
  );

  return  jsonAsObject(body);
}

#define CLASS_MGMT_ALLOWED_CLASS              "default-class"
#define CLASS_MGMT_QUERY_MIN_SEGMENT_COUNT    1
#define CLASS_MGMT_QUERY_MAX_SEGMENT_COUNT    7
#define CLASS_MGMT_CLASS_SEGMENT_ID           1
#define CLASS_MGMT_PROFILE_SEGMENT_ID         3
#define CLASS_MGMT_ACCESS_LIST_SEGMENT_ID     5

static bool isClassMgmtQueryStringValid(StringList *path) {

  char *pathSegments[CLASS_MGMT_QUERY_MAX_SEGMENT_COUNT] = {0};
  int minSegmentCount = CLASS_MGMT_QUERY_MIN_SEGMENT_COUNT;
  int maxSegmentCount = sizeof(pathSegments) / sizeof(pathSegments[0]);

  int segmentCount = path->count;

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "class-mgmt query segment count = %d [%d, %d]\n",
         segmentCount, minSegmentCount, maxSegmentCount);

  if (segmentCount < minSegmentCount || maxSegmentCount < segmentCount) {
    return false;
  }

  StringListElt *pathSegment = firstStringListElt(path);

  for (int i = 0; i < maxSegmentCount; i++) {
    if (pathSegment == NULL) {
      break;
    }

    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
           "class-mgmt path segment #%d =\'%s\'\n", i, pathSegment->string);

    pathSegments[i] = pathSegment->string;
    pathSegment = pathSegment->next;
  }

  char *serviceSegment = pathSegments[0];
  if (serviceSegment == NULL) {
    return false;
  }
  if (strcmp(serviceSegment, "security-mgmt") != 0) {
    return false;
  }

  /* security-mgmt/classes ? No, leave. The URL is incorrect */
  char *classSegment = pathSegments[CLASS_MGMT_CLASS_SEGMENT_ID];
  if (classSegment != NULL && strcmp(classSegment, "classes") != 0) {
    return false;
  }

  /* security-mgmt/classes/class_value/profiles ? No, leave.
   * The URL is incorrect */
  char *profileSegment = pathSegments[CLASS_MGMT_PROFILE_SEGMENT_ID];
  if (profileSegment != NULL && strcmp(profileSegment, "profiles") != 0) {
    return false;
  }

  /* security-mgmt/classes/class_value/profiles/profile_value/access-list ?
   * No, leave. The URL is incorrect */
  char *accessListSegment = pathSegments[CLASS_MGMT_ACCESS_LIST_SEGMENT_ID];
  if (accessListSegment != NULL &&
      strcmp(accessListSegment, "access-list") != 0) {
    return false;
  }

  return true;
}

static void extractClassMgmtQueryParms(StringList *path,
                                       const char **className,
                                       const char **profileName,
                                       const char **accessListEntryID) {

  char *pathSegments[CLASS_MGMT_QUERY_MAX_SEGMENT_COUNT] = {0};
  int maxSegmentCount = sizeof(pathSegments) / sizeof(pathSegments[0]);

  StringListElt *pathSegment = firstStringListElt(path);
  for (int i = 0; i < maxSegmentCount; i++) {
    if (pathSegment == NULL) {
      break;
    }
    pathSegments[i] = pathSegment->string;
    pathSegment = pathSegment->next;
  }

  char *classSegment = pathSegments[CLASS_MGMT_CLASS_SEGMENT_ID];
  char *classSegmentValue =
      pathSegments[CLASS_MGMT_CLASS_SEGMENT_ID + 1];
  if (classSegment != NULL && classSegmentValue == NULL) {
    *className = "";
  } else {
    *className = classSegmentValue;
  }

  if (*className != NULL && !strcmp(*className, CLASS_MGMT_ALLOWED_CLASS)) {
    *className = "";
  }

  char *profileSegment = pathSegments[CLASS_MGMT_PROFILE_SEGMENT_ID];
  char *profileSegmentValue =
      pathSegments[CLASS_MGMT_PROFILE_SEGMENT_ID + 1];
  if (profileSegment != NULL && profileSegmentValue == NULL) {
    *profileName = "";
  } else {
    *profileName = profileSegmentValue;
  }

  char *accessListSegment = pathSegments[CLASS_MGMT_ACCESS_LIST_SEGMENT_ID];
  char *accessListSegmentValue =
      pathSegments[CLASS_MGMT_ACCESS_LIST_SEGMENT_ID + 1];
  if (accessListSegment != NULL && accessListSegmentValue == NULL) {
    *accessListEntryID = "";
  } else {
    *accessListEntryID = accessListSegmentValue;
  }

}

typedef enum ClassMgmtRequestType_tag {
  CLASS_MGMT_UNKNOWN_REQUEST,
  CLASS_MGMT_PROFILE_GET,
  CLASS_MGMT_PROFILE_POST,
  CLASS_MGMT_PROFILE_PUT,
  CLASS_MGMT_PROFILE_DELETE,
  CLASS_MGMT_ACCESS_LIST_GET,
  CLASS_MGMT_ACCESS_LIST_POST,
  CLASS_MGMT_ACCESS_LIST_PUT,
  CLASS_MGMT_ACCESS_LIST_DELETE
} ClassMgmtRequestType;

static ClassMgmtRequestType getClassMgmtRequstType(HttpRequest *request) {

  const char *className = NULL;
  const char *profileName = NULL;
  const char *accessListEntryID = NULL;
  extractClassMgmtQueryParms(request->parsedFile,
                    &className,
                    &profileName,
                    &accessListEntryID);

  bool profileQuery = false;
  bool accessListQuery = false;

  if (className != NULL && profileName != NULL && accessListEntryID != NULL) {
    accessListQuery = true;
  } else if (className != NULL && profileName != NULL) {
    profileQuery = true;
  } else {
    return CLASS_MGMT_UNKNOWN_REQUEST;
  }

  if (!strcmp(request->method, methodGET)) {
    if (profileQuery) {
      return CLASS_MGMT_PROFILE_GET;
    } else if (accessListQuery) {
      return CLASS_MGMT_ACCESS_LIST_GET;
    }
  } else if (!strcmp(request->method, methodPOST)) {
    if (profileQuery) {
      return CLASS_MGMT_PROFILE_POST;
    } else if (accessListQuery) {
      return CLASS_MGMT_ACCESS_LIST_POST;
    }
  } else if (!strcmp(request->method, methodPUT)) {
    if (profileQuery) {
      return CLASS_MGMT_PROFILE_PUT;
    } else if (accessListQuery) {
      return CLASS_MGMT_ACCESS_LIST_PUT;
    }
  } else if (!strcmp(request->method, methodDELETE)) {
    if (profileQuery) {
      return CLASS_MGMT_PROFILE_DELETE;
    } else if (accessListQuery) {
      return CLASS_MGMT_ACCESS_LIST_DELETE;
    }
  } else {
    return CLASS_MGMT_UNKNOWN_REQUEST;
  }


  return CLASS_MGMT_UNKNOWN_REQUEST;
}

typedef struct ClassMgmtCommonParms_tag {
  JsonObject *requestBody;
  CrossMemoryServerName *zisServerName;
  const char *className;
  const char *profileName;
  const char *accessListEntryID;
  bool isDryRun;
  int traceLevel;
} ClassMgmtCommonParms;

static void printGenresProfileToJSON(jsonPrinter *printer,
                                     const ZISGenresProfileEntry *entry) {

  char profileNullTerm[sizeof(entry->profile) + 1] = {0};
  char ownerNullTerm[sizeof(entry->owner) + 1] = {0};
  memcpy(profileNullTerm, entry->profile, sizeof(entry->profile));
  memcpy(ownerNullTerm, entry->owner, sizeof(entry->owner));

  nullTerminate(profileNullTerm, sizeof(profileNullTerm) - 1);
  nullTerminate(ownerNullTerm, sizeof(ownerNullTerm) - 1);

  jsonStartObject(printer, NULL);
  {
    jsonAddString(printer, "profile", profileNullTerm);
    jsonAddString(printer, "owner", ownerNullTerm);
  }
  jsonEndObject(printer);

}

static void formatZISGenresAdminServiceError(int zisCallRC,
                                             const void *serviceStatus,
                                             char *messageBuffer,
                                             size_t messageBufferSize) {

  const ZISGenresAdminServiceStatus *status = serviceStatus;

  if (zisCallRC == RC_ZIS_SRVC_SERVICE_FAILED) {

    int serviceRC = status->baseStatus.serviceRC;
    int serviceRSN = status->baseStatus.serviceRSN;
    if (serviceRC > RC_ZIS_GSADMNSRV_MAX_RC) {
      snprintf(messageBuffer, messageBufferSize,
               "Unknown ZIS service error (RC = %d, RSN = %d)",
               serviceRC, serviceRSN);
      return;
    }

    const char *description = ZIS_GSADMNSRV_SERVICE_RC_DESCRIPTION[serviceRC];
    if (serviceRC == RC_ZIS_GSADMNSRV_INTERNAL_SERVICE_FAILED) {

      const char *radminMessage = NULL;
      size_t radminMessageLength = 0;
      if (status->message.length > 0) {
        radminMessage = status->message.text;
        radminMessageLength = status->message.length;
      } else {
        radminMessage = "no additional info";
        radminMessageLength = strlen(radminMessage);
      }

      snprintf(messageBuffer, messageBufferSize,
               "R_admin failed (RC = %d, SAF RC = %d, RACF RC = %d, RSN = %d)"
               " - \'%.*s\'",
               status->internalServiceRC,
               status->safStatus.safRC,
               status->safStatus.racfRC,
               status->safStatus.racfRSN,
               radminMessageLength, radminMessage);

    } else {
      snprintf(messageBuffer, messageBufferSize,
               "ZIS service failed (RC = %d, RSN = %d) - %s",
               serviceRC, serviceRSN, description);
    }
    return;

  }

  if (zisCallRC > RC_ZIS_SRVC_PADMIN_MAX_RC) {
    snprintf(messageBuffer, messageBufferSize,
             "Unknown ZIS error (RC = %d)", zisCallRC);
    return;
  }

  const char *description = ZIS_GSADMNSRV_WRAPPER_RC_DESCRIPTION[zisCallRC];
  snprintf(messageBuffer, messageBufferSize, "ZIS failed (RC = %d) - %s",
           zisCallRC, description);

}

static void formatZISGenresProfileServiceError(int zisCallRC,
                                               const void *serviceStatus,
                                               char *messageBuffer,
                                               size_t messageBufferSize) {

  const ZISGenresProfileServiceStatus *status = serviceStatus;

  if (zisCallRC == RC_ZIS_SRVC_SERVICE_FAILED) {

    int serviceRC = status->baseStatus.serviceRC;
    int serviceRSN = status->baseStatus.serviceRSN;
    if (serviceRC > RC_ZIS_GRPRFSRV_MAX_RC) {
      snprintf(messageBuffer, messageBufferSize,
               "Unknown ZIS service error (RC = %d, RSN = %d)",
               serviceRC, serviceRSN);
      return;
    }

    const char *description = ZIS_GRPRFSRV_SERVICE_RC_DESCRIPTION[serviceRC];
    if (serviceRC == RC_ZIS_GRPRFSRV_INTERNAL_SERVICE_FAILED) {
      snprintf(messageBuffer, messageBufferSize,
               "R_admin failed (RC = %d, SAF RC = %d, RACF RC = %d, RSN = %d)",
               status->internalServiceRC,
               status->safStatus.safRC,
               status->safStatus.racfRC,
               status->safStatus.racfRSN);
    } else {
      snprintf(messageBuffer, messageBufferSize,
               "ZIS service failed (RC = %d, RSN = %d) - %s",
               serviceRC, serviceRSN, description);
    }
    return;

  }

  if (zisCallRC > RC_ZIS_SRVC_GRPRFSRV_MAX_RC) {
    snprintf(messageBuffer, messageBufferSize,
             "Unknown ZIS error (RC = %d)", zisCallRC);
    return;
  }

  const char *description = ZIS_GRPRFSRV_WRAPPER_RC_DESCRIPTION[zisCallRC];
  snprintf(messageBuffer, messageBufferSize, "ZIS failed (RC = %d) - %s",
           zisCallRC, description);

}

static void printGenresAccessListEntryToJSON(jsonPrinter *printer,
                                       const ZISGenresAccessEntry *entry) {

  char idNullTerm[sizeof(entry->id) + 1] = {0};
  char accessTypeNullTerm[sizeof(entry->accessType) + 1] = {0};
  memcpy(idNullTerm, entry->id, sizeof(entry->id));
  memcpy(accessTypeNullTerm, entry->accessType, sizeof(entry->accessType));

  nullTerminate(idNullTerm, sizeof(idNullTerm) - 1);
  nullTerminate(accessTypeNullTerm, sizeof(accessTypeNullTerm) - 1);

  jsonStartObject(printer, NULL);
  {
    jsonAddString(printer, "id", idNullTerm);
    jsonAddString(printer, "accessType", accessTypeNullTerm);
  }
  jsonEndObject(printer);

}

static void formatZISGenresAccessListServiceError(int zisCallRC,
                                                  const void *serviceStatus,
                                                  char *messageBuffer,
                                                  size_t messageBufferSize) {

  const ZISGenresAccessListServiceStatus *status = serviceStatus;

  if (zisCallRC == RC_ZIS_SRVC_SERVICE_FAILED) {

    int serviceRC = status->baseStatus.serviceRC;
    int serviceRSN = status->baseStatus.serviceRSN;
    if (serviceRC > RC_ZIS_ACSLSRV_MAX_RC) {
      snprintf(messageBuffer, messageBufferSize,
               "Unknown ZIS service error (RC = %d, RSN = %d)",
               serviceRC, serviceRSN);
      return;
    }

    const char *description = ZIS_ACSLSRV_SERVICE_RC_DESCRIPTION[serviceRC];
    if (serviceRC == RC_ZIS_ACSLSRV_INTERNAL_SERVICE_FAILED) {
      snprintf(messageBuffer, messageBufferSize,
               "R_admin failed (RC = %d, SAF RC = %d, RACF RC = %d, RSN = %d)",
               status->internalServiceRC,
               status->safStatus.safRC,
               status->safStatus.racfRC,
               status->safStatus.racfRSN);
    } else {
      snprintf(messageBuffer, messageBufferSize,
               "ZIS service failed (RC = %d, RSN = %d) - %s",
               serviceRC, serviceRSN, description);
    }
    return;

  }

  if (zisCallRC > RC_ZIS_SRVC_ACSLSRV_MAX_RC) {
    snprintf(messageBuffer, messageBufferSize,
             "Unknown ZIS error (RC = %d)", zisCallRC);
    return;
  }

  const char *description = ZIS_ACSLSRV_WRAPPER_RC_DESCRIPTION[zisCallRC];
  snprintf(messageBuffer, messageBufferSize, "ZIS failed (RC = %d) - %s",
           zisCallRC, description);

}

#define GENRES_PROFILE_MAX_COUNT  1000

static void respondWithGenresProfiles(HttpResponse *response,
                                      const char *startProfile,
                                      unsigned int profilesRequested,
                                      const ZISGenresProfileEntry *profiles,
                                      unsigned int profileCount) {

  jsonPrinter *printer = respondWithJsonPrinter(response);
  setResponseStatus(response, HTTP_STATUS_OK, "OK");
  setContentType(response, "text/json");
  addStringHeader(response, "Server", "jdmfws");
  addStringHeader(response, "Transfer-Encoding", "chunked");
  writeHeader(response);

  jsonStart(printer);

  printTableResultResponseMetadata(printer);
  jsonAddString(printer, "resultType", "table");
  jsonStartObject(printer, "metaData");
  {

    jsonStartObject(printer, "tableMetaData");
    {
      jsonAddString(printer, "tableIdentifier", "profiles");
      jsonAddString(printer, "shortTableLabel", "profiles");
      jsonAddString(printer, "longTableLabel", "Profiles");
      jsonAddString(printer, "globalIdentifier",
                    "org.zowe.zss.security-mgmt.profiles");
      jsonAddInt(printer, "minRows", 0);
      jsonAddInt(printer, "maxRows", GENRES_PROFILE_MAX_COUNT);
    }
    jsonEndObject(printer);

    jsonStartArray(printer, "columnMetaData");
    {

      jsonStartObject(printer, NULL);
      {
        jsonAddString(printer, "columnIdentifier", "profile");
        jsonAddString(printer, "shortColumnLabel", "profile");
        jsonAddString(printer, "longColumnLabel", "Profile Name");
        jsonAddString(printer, "rawDataType", "string");
        jsonAddInt(printer, "rawDataTypeLength", sizeof(profiles[0].profile));
      }
      jsonEndObject(printer);

      jsonStartObject(printer, NULL);
      {
        jsonAddString(printer, "columnIdentifier", "owner");
        jsonAddString(printer, "shortColumnLabel", "owner");
        jsonAddString(printer, "longColumnLabel", "Owner");
        jsonAddString(printer, "rawDataType", "string");
        jsonAddInt(printer, "rawDataTypeLength", sizeof(profiles[0].owner));
      }
      jsonEndObject(printer);

    }
    jsonEndArray(printer);

  }
  jsonEndObject(printer);

  int profilesToReturn = (profileCount > profilesRequested) ?
                          profileCount - 1 : profileCount;

  jsonAddString(printer, "start", startProfile ? (char *)startProfile : "");
  jsonAddInt(printer, "count", profilesToReturn);
  jsonAddInt(printer, "countRequested", profilesRequested);
  if (profilesRequested < profileCount) {
    jsonAddBoolean(printer, "hasMore", TRUE);
  } else {
    jsonAddBoolean(printer, "hasMore", FALSE);
  }

  jsonStartArray(printer, "rows");
  {
    for (int i = 0; i < profilesToReturn; i++) {
      const ZISGenresProfileEntry *entry = &profiles[i];
      printGenresProfileToJSON(printer, entry);
      zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
             "genres entry #%d: %.*s, %.*s\n", i,
             sizeof(entry->profile), entry->profile,
             sizeof(entry->owner), entry->owner);
    }
  }
  jsonEndArray(printer);

  jsonEnd(printer);

  finishResponse(response);

}

typedef enum GenresAction_tag {
  GENRES_ACTION_ADD_PROFILE,
  GENRES_ACTION_REMOVE_PROFILE,
  GENRES_ACTION_UPDATE_PROFILE
} GenresAction;

static void respondWithGenresAdminInfo(
    HttpResponse *response,
    GenresAction action,
    bool isDryRun,
    const ZISGenresAdminServiceMessage *operatorCommand)
{

  if (isDryRun) {

    jsonPrinter *printer = respondWithJsonPrinter(response);
    setResponseStatus(response, HTTP_STATUS_OK, "OK");
    setContentType(response, "text/json");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
    writeHeader(response);

    jsonStart(printer);

    jsonAddString(printer, "_objectType",
                  "org.zowe.zss.security-mgmt.operator-command");
    jsonAddString(printer, "resultMetaDataSchemaVersion", "1.0");
    jsonAddString(printer, "serviceVersion", "1.0");


    jsonAddLimitedString(printer, "operatorCommand",
                         (char *)operatorCommand->text,
                         operatorCommand->length);

    jsonEnd(printer);

  } else {

    if (action == GENRES_ACTION_ADD_PROFILE) {
      setResponseStatus(response, HTTP_STATUS_CREATED, "OK");
    } else {
      setResponseStatus(response, HTTP_STATUS_NO_CONTENT, "OK");
    }
    setContentType(response, "text/plain");
    addIntHeader(response, "Content-Length", 0);
    addStringHeader(response,"Server","jdmfws");
    writeHeader(response);

  }

  finishResponse(response);

}

static void respondWithProfileAccessList(HttpResponse *response,
                                         const ZISGenresAccessEntry *accessList,
                                         unsigned int accessListEntryCount) {


  jsonPrinter *printer = respondWithJsonPrinter(response);
  setResponseStatus(response, HTTP_STATUS_OK, "OK");
  setContentType(response, "text/json");
  addStringHeader(response, "Server", "jdmfws");
  addStringHeader(response, "Transfer-Encoding", "chunked");
  writeHeader(response);

  jsonStart(printer);

  printTableResultResponseMetadata(printer);
  jsonAddString(printer, "resultType", "table");
  jsonStartObject(printer, "metaData");
  {

    jsonStartObject(printer, "tableMetaData");
    {
      jsonAddString(printer, "tableIdentifier", "access-list");
      jsonAddString(printer, "shortTableLabel", "access-list");
      jsonAddString(printer, "longTableLabel", "Access List");
      jsonAddString(printer, "globalIdentifier",
                    "org.zowe.zss.security-mgmt.profile-access-list");
    }
    jsonEndObject(printer);

    jsonStartArray(printer, "columnMetaData");
    {

      jsonStartObject(printer, NULL);
      {
        jsonAddString(printer, "columnIdentifier", "id");
        jsonAddString(printer, "shortColumnLabel", "id");
        jsonAddString(printer, "longColumnLabel", "ID");
        jsonAddString(printer, "rawDataType", "string");
        jsonAddInt(printer, "rawDataTypeLength", sizeof(accessList[0].id));
      }
      jsonEndObject(printer);

      jsonStartObject(printer, NULL);
      {
        jsonAddString(printer, "columnIdentifier", "accessType");
        jsonAddString(printer, "shortColumnLabel", "accessType");
        jsonAddString(printer, "longColumnLabel", "Access Type");
        jsonAddString(printer, "rawDataType", "string");
        jsonAddInt(printer, "rawDataTypeLength",
                   sizeof(accessList[0].accessType));
      }
      jsonEndObject(printer);

    }
    jsonEndArray(printer);

  }
  jsonEndObject(printer);

  jsonAddInt(printer, "count", accessListEntryCount);

  jsonStartArray(printer, "rows");
  {
    for (int i = 0; i < accessListEntryCount; i++) {
      const ZISGenresAccessEntry *entry = &accessList[i];
      printGenresAccessListEntryToJSON(printer, entry);
      zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
             "access list entry #%d: %.*s, %.*s\n", i,
             sizeof(entry->id), entry->id,
             sizeof(entry->accessType), entry->accessType);
    }
  }
  jsonEndArray(printer);

  jsonEnd(printer);

  finishResponse(response);

}

static void respondToProfileGET(ClassMgmtCommonParms *commonParms,
                                HttpService *service,
                                HttpResponse *response) {

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  if (strlen(commonParms->profileName) > 0) {
    respondWithError(response, HTTP_STATUS_NOT_IMPLEMENTED,
                     "specific profile info retrieval not implemented");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "profile not provided for profiles GET, leaving...\n");
    return;
  }

  HttpRequest *request = response->request;

  int zisRC = RC_ZIS_SRVC_OK;
  char *startProfile = getQueryParam(response->request, "start");
  int profilesToExtract = getUserProfileCount(request);
  if (profilesToExtract < 0 || GENRES_PROFILE_MAX_COUNT < profilesToExtract) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Count out of range");
    return;
  }

  ZISGenresProfileEntry resultBuffer[GENRES_PROFILE_MAX_COUNT + 1];
  unsigned int profilesExtracted = 0;
  ZISGenresProfileServiceStatus status = {0};

  zisRC = zisExtractGenresProfiles(
      commonParms->zisServerName,
      commonParms->className,
      startProfile,
      profilesToExtract + 1,
      resultBuffer,
      &profilesExtracted,
      &status,
      commonParms->traceLevel
  );

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisExtractGenresProfiles: RC=%d, base=(%d,%d,%d,%d), "
         "internal=(%d,%d), SAF=(%d,%d,%d)\n", zisRC,
         status.baseStatus.cmsRC, status.baseStatus.cmsRSN,
         status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
         status.internalServiceRC, status.internalServiceRSN,
         status.safStatus.safRC, status.safStatus.racfRC,
         status.safStatus.racfRSN);

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisExtractGenresProfiles: profiles to extract = %u, "
         "extracted = %u\n",
         profilesToExtract, profilesExtracted);

  if (zisRC == RC_ZIS_SRVC_OK) {
    respondWithGenresProfiles(response, startProfile, profilesToExtract,
                              resultBuffer, profilesExtracted);
  } else {
    respondWithZISError(response, zisRC, &status,
                        formatZISGenresProfileServiceError);
  }

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);

}

static void respondToProfilePOST(ClassMgmtCommonParms *commonParms,
                                 HttpService *service,
                                 HttpResponse *response) {

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  if (strlen(commonParms->className) > 0) {
    respondWithError(response, HTTP_STATUS_FORBIDDEN,
                     "non standard class not allowed for mutation requests");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "non standard class provided for profiles POST, leaving...\n");
    return;
  }

  if (strlen(commonParms->profileName) == 0) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "profile name required");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "profile name required for profile POST\n");
    return;
  }

  char *owner = NULL;
  if (commonParms->requestBody != NULL) {
    Json *ownerParm = jsonObjectGetPropertyValue(commonParms->requestBody,
                                                 "owner");
    if (ownerParm != NULL) {
      owner = jsonAsString(ownerParm);
    }
  }

  bool isDryRun = commonParms->isDryRun;

  ZISGenresAdminServiceMessage operatorCommand = {0};
  ZISGenresAdminServiceStatus status = {0};
  int zisRC = zisDefineProfile(
      commonParms->zisServerName,
      commonParms->profileName,
      owner,
      isDryRun,
      &operatorCommand,
      &status,
      commonParms->traceLevel
  );

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisDefineProfile: RC=%d, base=(%d,%d,%d,%d), internal=(%d,%d), "
         "SAF=(%d,%d,%d)\n", zisRC,
         status.baseStatus.cmsRC, status.baseStatus.cmsRSN,
         status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
         status.internalServiceRC, status.internalServiceRSN,
         status.safStatus.safRC, status.safStatus.racfRC,
         status.safStatus.racfRSN);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisDefineProfile: serviceMsg=\'%.*s\'\n",
         status.message.length, status.message.text);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisDefineProfile: isDryRun=%d, operCmd=\'%.*s\'\n",
         isDryRun, operatorCommand.length, operatorCommand.text);

  if (zisRC == RC_ZIS_SRVC_OK) {
    respondWithGenresAdminInfo(response, GENRES_ACTION_ADD_PROFILE, isDryRun,
                               &operatorCommand);
  } else {
    respondWithZISError(response, zisRC, &status,
                        formatZISGenresAdminServiceError);
  }

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);

}

static void respondToProfileDELETE(ClassMgmtCommonParms *commonParms,
                                   HttpService *service,
                                   HttpResponse *response) {

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  if (strlen(commonParms->className) > 0) {
    respondWithError(response, HTTP_STATUS_FORBIDDEN,
                     "non standard class not allowed for mutation requests");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "non standard class provided for profiles DELETE, leaving...\n");
    return;
  }

  if (strlen(commonParms->profileName) == 0) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "profile name required");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "profile name required for profile DELETE\n");
    return;
  }

  bool isDryRun = commonParms->isDryRun;

  ZISGenresAdminServiceMessage operatorCommand = {0};
  ZISGenresAdminServiceStatus status = {0};
  int zisRC = zisDeleteProfile(
      commonParms->zisServerName,
      commonParms->profileName,
      isDryRun,
      &operatorCommand,
      &status,
      commonParms->traceLevel
  );

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisDeleteProfile: RC=%d, base=(%d,%d,%d,%d), internal=(%d,%d), "
         "SAF=(%d,%d,%d)\n", zisRC,
         status.baseStatus.cmsRC, status.baseStatus.cmsRSN,
         status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
         status.internalServiceRC, status.internalServiceRSN,
         status.safStatus.safRC, status.safStatus.racfRC,
         status.safStatus.racfRSN);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisDeleteProfile:  serviceMsg=\'%.*s\'\n",
         status.message.length, status.message.text);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisDeleteProfile: isDryRun=%d, operCmd=\'%.*s\'\n",
         isDryRun, operatorCommand.length, operatorCommand.text);

  if (zisRC == RC_ZIS_SRVC_OK) {
    respondWithGenresAdminInfo(response, GENRES_ACTION_REMOVE_PROFILE, isDryRun,
                               &operatorCommand);
  } else {
    respondWithZISError(response, zisRC, &status,
                        formatZISGenresAdminServiceError);
  }

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);

}

static ZISGenresAdminServiceAccessType
translateGenresAccessType(const char *typeString) {

  if (!strcmp(typeString, "READ")) {
    return ZIS_GENRES_ADMIN_ACESS_TYPE_READ;
  }

  if (!strcmp(typeString, "UPDATE")) {
    return ZIS_GENRES_ADMIN_ACESS_TYPE_UPDATE;
  }

  if (!strcmp(typeString, "ALTER")) {
    return ZIS_GENRES_ADMIN_ACESS_TYPE_ALTER;
  }

  return ZIS_GENRES_ADMIN_ACESS_TYPE_UNKNOWN;
}

static void respondToProfileAccessListPUT(ClassMgmtCommonParms *commonParms,
                                          HttpService *service,
                                          HttpResponse *response) {

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  if (strlen(commonParms->className) > 0) {
    respondWithError(response, HTTP_STATUS_FORBIDDEN,
                     "non standard class not allowed for mutation requests");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "non standard class provided for user POST/PUT, leaving...\n");
    return;
  }

  if (strlen(commonParms->profileName) == 0) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "profile name required");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "profile name required for user POST/PUT\n");
    return;
  }

  if (strlen(commonParms->accessListEntryID) == 0) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "user ID required");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "user ID required for user POST/PUT\n");
    return;
  }

  if (commonParms->requestBody == NULL) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Body missing");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "body not provided for user POST/PUT, leaving...\n");
    return;
  }

  Json *accessTypeParm = jsonObjectGetPropertyValue(commonParms->requestBody,
                                                    "accessType");
  if (accessTypeParm == NULL) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "accessType missing");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "access type not provided for user POST/PUT, leaving...\n");
    return;
  }

  char *accessTypeString = jsonAsString(accessTypeParm);
  if (accessTypeString == NULL) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "accessType has bad type");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "bad access type provided for user POST/PUT, leaving...\n");
    return;
  }

  ZISGenresAdminServiceAccessType accessType =
      translateGenresAccessType(accessTypeString);
  if (accessType == ZIS_GENRES_ADMIN_ACESS_TYPE_UNKNOWN) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "unknown access type, use [READ, UPDATE, ALTER]");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "unknown access type (%d( provided for user POST/PUT, leaving...\n",
           accessType);
    return;
  }

  bool isDryRun = commonParms->isDryRun;

  ZISGenresAdminServiceMessage operatorCommand = {0};
  ZISGenresAdminServiceStatus status = {0};
  int zisRC = zisGiveAccessToProfile(
      commonParms->zisServerName,
      commonParms->accessListEntryID,
      commonParms->profileName,
      accessType,
      isDryRun,
      &operatorCommand,
      &status,
      commonParms->traceLevel
  );

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisGiveAccessToProfile: RC=%d, base=(%d,%d,%d,%d), internal=(%d,%d), "
         "SAF=(%d,%d,%d)\n", zisRC,
         status.baseStatus.cmsRC, status.baseStatus.cmsRSN,
         status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
         status.internalServiceRC, status.internalServiceRSN,
         status.safStatus.safRC, status.safStatus.racfRC,
         status.safStatus.racfRSN);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisGiveAccessToProfile:  serviceMsg=\'%.*s\'\n",
         status.message.length, status.message.text);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisGiveAccessToProfile: isDryRun=%d, operCmd=\'%.*s\'\n",
         isDryRun, operatorCommand.length, operatorCommand.text);

  if (zisRC == RC_ZIS_SRVC_OK) {
    respondWithGenresAdminInfo(response, GENRES_ACTION_UPDATE_PROFILE, isDryRun,
                               &operatorCommand);
  } else {
    respondWithZISError(response, zisRC, &status,
                        formatZISGenresAdminServiceError);
  }

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);

}

#define CLASS_MGMT_ACCESS_LIST_DEFAULT_SIZE     128
#define CLASS_MGMT_ACCESS_LIST_MAX_SIZE         8192

static void respondToProfileAccessListGET(ClassMgmtCommonParms *commonParms,
                                          HttpService *service,
                                          HttpResponse *response) {

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  if (strlen(commonParms->accessListEntryID) > 0) {
    respondWithError(response, HTTP_STATUS_NOT_IMPLEMENTED,
                     "specific user access status retrieval not implemented");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "access list can only be retrieved in bulk, leaving...\n");
    return;
  }

  unsigned int resultBufferCapacity = CLASS_MGMT_ACCESS_LIST_DEFAULT_SIZE;
  unsigned int resultBufferSize = 0;
  ZISGenresAccessEntry *resultBuffer = NULL;
  int zisRC = RC_ZIS_SRVC_OK;
  unsigned int entriesExtracted = 0;
  ZISGenresAccessListServiceStatus status = {0};

  for (int i = 0; i < 2; i++) {

    resultBufferSize = sizeof(ZISGenresAccessEntry) * resultBufferCapacity;
    resultBuffer =
        (ZISGenresAccessEntry *)safeMalloc(resultBufferSize, "access list");
    if (resultBuffer == NULL) {
      respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,
                       "out of memory");
      zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
             "access list buffer with size %u not allocated, leaving...\n",
             resultBufferSize);
      return;
    }

    zisRC = zisExtractGenresAccessList(
        commonParms->zisServerName,
        commonParms->className,
        commonParms->profileName,
        resultBuffer,
        resultBufferCapacity,
        &entriesExtracted,
        &status,
        commonParms->traceLevel
    );

    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
           "zisExtractAccessList: RC=%d, base=(%d,%d,%d,%d), internal=(%d,%d), "
           "SAF=(%d,%d,%d)\n", zisRC,
           status.baseStatus.cmsRC, status.baseStatus.cmsRSN,
           status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
           status.internalServiceRC, status.internalServiceRSN,
           status.safStatus.safRC, status.safStatus.racfRC,
           status.safStatus.racfRSN);

    if (zisRC == RC_ZIS_SRVC_ACSLSRV_INSUFFICIENT_BUFFER) {

      safeFree((char *)resultBuffer, resultBufferSize);
      resultBuffer = NULL;

      if (entriesExtracted <= 0 ||
          CLASS_MGMT_ACCESS_LIST_MAX_SIZE < entriesExtracted) {
        respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,
                         "access list size our of range");
        zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
               "access list size our of range (%u), leaving...\n",
               entriesExtracted);
        return;
      }

      resultBufferCapacity = entriesExtracted;
      zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
             "access list will be re-allocated with capacity %u\n",
             resultBufferCapacity);

    } else {
      break;
    }

  }

  if (zisRC == RC_ZIS_SRVC_OK) {
    respondWithProfileAccessList(response, resultBuffer, entriesExtracted);
  } else {
    respondWithZISError(response, zisRC, &status,
                        formatZISGenresAccessListServiceError);
  }

  safeFree((char *)resultBuffer, resultBufferSize);

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "access list buffer with size %u freed @ 0x%p\n",
         resultBufferSize, resultBuffer);

  resultBuffer = NULL;

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);

}

static void respondToProfileAccessListDELETE(ClassMgmtCommonParms *commonParms,
                                             HttpService *service,
                                             HttpResponse *response) {

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  if (strlen(commonParms->className) > 0) {
    respondWithError(response, HTTP_STATUS_FORBIDDEN,
                     "non standard class not allowed for mutation requests");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "non standard class provided for access list DELETE, leaving...\n");
    return;
  }

  if (strlen(commonParms->profileName) == 0) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "profile name required");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "profile name required for access list  DELETE\n");
    return;
  }

  if (strlen(commonParms->accessListEntryID) == 0) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "access list entry name required");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "access list entry name required for access list DELETE\n");
    return;
  }

  HttpRequest *request = response->request;
  bool isDryRun = commonParms->isDryRun;

  ZISGenresAdminServiceMessage operatorCommand;
  ZISGenresAdminServiceStatus status = {0};
  int zisRC = zisRevokeAccessToProfile(
      commonParms->zisServerName,
      commonParms->accessListEntryID,
      commonParms->profileName,
      isDryRun,
      &operatorCommand,
      &status,
      commonParms->traceLevel
  );

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisRevokeAccessToProfile: RC=%d, base=(%d,%d,%d,%d), "
         "internal=(%d,%d), SAF=(%d,%d,%d)\n", zisRC,
         status.baseStatus.cmsRC, status.baseStatus.cmsRSN,
         status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
         status.internalServiceRC, status.internalServiceRSN,
         status.safStatus.safRC, status.safStatus.racfRC,
         status.safStatus.racfRSN);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisRevokeAccessToProfile:  serviceMsg=\'%.*s\'\n",
         status.message.length, status.message.text);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisRevokeAccessToProfile: isDryRun=%d, operCmd=\'%.*s\'\n",
         isDryRun, operatorCommand.length, operatorCommand.text);

  if (zisRC == RC_ZIS_SRVC_OK) {
    respondWithGenresAdminInfo(response, GENRES_ACTION_UPDATE_PROFILE, isDryRun,
                               &operatorCommand);
  } else {
    respondWithZISError(response, zisRC, &status,
                        formatZISGenresAdminServiceError);
  }

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);

}

static void extractClassMgmtCommonParms(HttpService *service,
                                        HttpRequest *request,
                                        ClassMgmtCommonParms *parms) {

  parms->zisServerName =
      getConfiguredProperty(service->server,
                            HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY);
  extractClassMgmtQueryParms(request->parsedFile,
                    &parms->className,
                    &parms->profileName,
                    &parms->accessListEntryID);
  parms->requestBody = getJsonBody(request->contentBody,
                                   request->contentLength,
                                   request->slh);

  parms->isDryRun = getDryRunValue(request);
  parms->traceLevel = getTraceLevel(request);

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "class-mgmt parms: zisServerName=\'%.16s\', "
         "class=\'%s\', profile=\'%s\', accessListEntryID=\'%s\', "
         "traceLevel=%d\n",
         parms->zisServerName->nameSpacePadded,
         parms->className ? parms->className : "null",
         parms->profileName ? parms->profileName : "null",
         parms->accessListEntryID ? parms->accessListEntryID : "null",
         parms->traceLevel);

}

static int serveClassManagement(HttpService *service,
                                HttpResponse *response) {

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  HttpRequest *request = response->request;

  if (isClassMgmtQueryStringValid(request->parsedFile) == false) {
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "class-mgmt query string is invalid, leaving...\n");
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Incorrect query");
    return 0;
  }

  ClassMgmtCommonParms commonParms;
  extractClassMgmtCommonParms(service, response->request, &commonParms);

  ClassMgmtRequestType requestType = getClassMgmtRequstType(request);

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "class-mgmt requestType = %d\n", requestType);

  switch (requestType) {
  case CLASS_MGMT_PROFILE_GET:
    respondToProfileGET(&commonParms, service, response);
    break;
  case CLASS_MGMT_PROFILE_POST:
    respondToProfilePOST(&commonParms, service, response);
    break;
  case CLASS_MGMT_PROFILE_DELETE:
    respondToProfileDELETE(&commonParms, service, response);
    break;
  case CLASS_MGMT_ACCESS_LIST_GET:
    respondToProfileAccessListGET(&commonParms, service, response);
    break;
  case CLASS_MGMT_ACCESS_LIST_PUT:
    respondToProfileAccessListPUT(&commonParms, service, response);
    break;
  case CLASS_MGMT_ACCESS_LIST_DELETE:
    respondToProfileAccessListDELETE(&commonParms, service, response);
    break;
  default:
  {
    respondWithError(response, HTTP_STATUS_NOT_IMPLEMENTED,
                     "Unsupported request");
  }
  }

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);
  return 0;
}

/************ Group management functions ************/

#define GROUP_MGMT_QUERY_MIN_SEGMENT_COUNT    1
#define GROUP_MGMT_QUERY_MAX_SEGMENT_COUNT    5
#define GROUP_MGMT_GROUP_SEGMENT_ID           1
#define GROUP_MGMT_ACCESS_LIST_SEGMENT_ID     3

static bool isGroupMgmtQueryStringValid(StringList *path) {

  char *pathSegments[GROUP_MGMT_QUERY_MAX_SEGMENT_COUNT] = {0};
  int minSegmentCount = GROUP_MGMT_QUERY_MIN_SEGMENT_COUNT;
  int maxSegmentCount = sizeof(pathSegments) / sizeof(pathSegments[0]);

  int segmentCount = path->count;

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "group-mgmt query segment count = %d [%d, %d]\n",
         segmentCount, minSegmentCount, maxSegmentCount);

  if (segmentCount < minSegmentCount || maxSegmentCount < segmentCount) {
    return false;
  }

  StringListElt *pathSegment = firstStringListElt(path);

  for (int i = 0; i < maxSegmentCount; i++) {
    if (pathSegment == NULL) {
      break;
    }

    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
           "group-mgmt path segment #%d =\'%s\'\n", i, pathSegment->string);

    pathSegments[i] = pathSegment->string;
    pathSegment = pathSegment->next;
  }

  char *serviceSegment = pathSegments[0];
  if (serviceSegment == NULL) {
    return false;
  }
  if (strcmp(serviceSegment, "security-mgmt") != 0) {
    return false;
  }

  /* security-mgmt/groups ? No, leave. The URL is incorrect */
  char *groupSegment = pathSegments[GROUP_MGMT_GROUP_SEGMENT_ID];
  if (groupSegment != NULL && strcmp(groupSegment, "groups") != 0) {
    return false;
  }

  /* security-mgmt/groups/group_value/access-list ?
   * No, leave. The URL is incorrect */
  char *accessListSegment = pathSegments[GROUP_MGMT_ACCESS_LIST_SEGMENT_ID];
  if (accessListSegment != NULL &&
      strcmp(accessListSegment, "access-list") != 0) {
    return false;
  }

  return true;
}

static void extractGroupMgmtQueryParms(StringList *path,
                                       const char **groupName,
                                       const char **accessListEntryID) {

  char *pathSegments[GROUP_MGMT_QUERY_MAX_SEGMENT_COUNT] = {0};
  int maxSegmentCount = sizeof(pathSegments) / sizeof(pathSegments[0]);

  StringListElt *pathSegment = firstStringListElt(path);
  for (int i = 0; i < maxSegmentCount; i++) {
    if (pathSegment == NULL) {
      break;
    }
    pathSegments[i] = pathSegment->string;
    pathSegment = pathSegment->next;
  }

  char *groupSegment = pathSegments[GROUP_MGMT_GROUP_SEGMENT_ID];
  char *groupSegmentValue =
      pathSegments[GROUP_MGMT_GROUP_SEGMENT_ID + 1];
  if (groupSegment != NULL && groupSegmentValue == NULL) {
    *groupName = "";
  } else {
    *groupName = groupSegmentValue;
  }

  char *accessListSegment = pathSegments[GROUP_MGMT_ACCESS_LIST_SEGMENT_ID];
  char *accessListSegmentValue =
      pathSegments[GROUP_MGMT_ACCESS_LIST_SEGMENT_ID + 1];
  if (accessListSegment != NULL && accessListSegmentValue == NULL) {
    *accessListEntryID = "";
  } else {
    *accessListEntryID = accessListSegmentValue;
  }

}

typedef enum GroupMgmtRequestType_tag {
  GROUP_MGMT_UNKNOWN_REQUEST,
  GROUP_MGMT_GROUP_GET,
  GROUP_MGMT_GROUP_POST,
  GROUP_MGMT_GROUP_PUT,
  GROUP_MGMT_GROUP_DELETE,
  GROUP_MGMT_ACCESS_LIST_GET,
  GROUP_MGMT_ACCESS_LIST_POST,
  GROUP_MGMT_ACCESS_LIST_PUT,
  GROUP_MGMT_ACCESS_LIST_DELETE
} GroupMgmtRequestType;

static GroupMgmtRequestType getGroupMgmtRequstType(HttpRequest *request) {

  const char *groupName = NULL;
  const char *accessListEntryID = NULL;
  extractGroupMgmtQueryParms(request->parsedFile,
                             &groupName,
                             &accessListEntryID);

  bool groupQuery = false;
  bool accessListQuery = false;

  if (groupName != NULL && accessListEntryID != NULL) {
    accessListQuery = true;
  } else if (groupName != NULL) {
    groupQuery = true;
  } else {
    return GROUP_MGMT_UNKNOWN_REQUEST;
  }

  if (!strcmp(request->method, methodGET)) {
    if (groupQuery) {
      return GROUP_MGMT_GROUP_GET;
    } else if (accessListQuery) {
      return GROUP_MGMT_ACCESS_LIST_GET;
    }
  } else if (!strcmp(request->method, methodPOST)) {
    if (groupQuery) {
      return GROUP_MGMT_GROUP_POST;
    } else if (accessListQuery) {
      return GROUP_MGMT_ACCESS_LIST_POST;
    }
  } else if (!strcmp(request->method, methodPUT)) {
    if (groupQuery) {
      return GROUP_MGMT_GROUP_PUT;
    } else if (accessListQuery) {
      return GROUP_MGMT_ACCESS_LIST_PUT;
    }
  } else if (!strcmp(request->method, methodDELETE)) {
    if (groupQuery) {
      return GROUP_MGMT_GROUP_DELETE;
    } else if (accessListQuery) {
      return GROUP_MGMT_ACCESS_LIST_DELETE;
    }
  } else {
    return GROUP_MGMT_UNKNOWN_REQUEST;
  }


  return GROUP_MGMT_UNKNOWN_REQUEST;
}

typedef struct GroupMgmtCommonParms_tag {
  JsonObject *requestBody;
  CrossMemoryServerName *zisServerName;
  const char *groupName;
  const char *accessListEntryID;
  bool isDryRun;
  int traceLevel;
} GroupMgmtCommonParms;

#define GROUP_PROFILE_MAX_COUNT  1000

static int getGroupCount(HttpRequest *request) {
  HttpRequestParam *countParam = getCheckedParam(request, "count");
  return (countParam ? countParam->intValue : GROUP_PROFILE_MAX_COUNT);
}

static void printGroupProfileToJSON(jsonPrinter *printer,
                                    const ZISGroupProfileEntry *entry) {

  char profileNullTerm[sizeof(entry->group) + 1] = {0};
  char ownerNullTerm[sizeof(entry->owner) + 1] = {0};
  char superiorGroupNullTerm[sizeof(entry->superiorGroup) + 1] = {0};
  memcpy(profileNullTerm, entry->group, sizeof(entry->group));
  memcpy(ownerNullTerm, entry->owner, sizeof(entry->owner));
  memcpy(superiorGroupNullTerm, entry->superiorGroup,
         sizeof(entry->superiorGroup));

  nullTerminate(profileNullTerm, sizeof(profileNullTerm) - 1);
  nullTerminate(ownerNullTerm, sizeof(ownerNullTerm) - 1);
  nullTerminate(superiorGroupNullTerm, sizeof(ownerNullTerm) - 1);

  jsonStartObject(printer, NULL);
  {
    jsonAddString(printer, "profile", profileNullTerm);
    jsonAddString(printer, "owner", ownerNullTerm);
    jsonAddString(printer, "superiorGroup", superiorGroupNullTerm);
  }
  jsonEndObject(printer);

}

static void respondWithGroupProfiles(HttpResponse *response,
                                     const char *startGroup,
                                     unsigned int groupsRequested,
                                     const ZISGroupProfileEntry *groups,
                                     unsigned int groupCount) {

  jsonPrinter *printer = respondWithJsonPrinter(response);
  setResponseStatus(response, HTTP_STATUS_OK, "OK");
  setContentType(response, "text/json");
  addStringHeader(response, "Server", "jdmfws");
  addStringHeader(response, "Transfer-Encoding", "chunked");
  writeHeader(response);

  jsonStart(printer);

  printTableResultResponseMetadata(printer);
  jsonAddString(printer, "resultType", "table");
  jsonStartObject(printer, "metaData");
  {

    jsonStartObject(printer, "tableMetaData");
    {
      jsonAddString(printer, "tableIdentifier", "profiles");
      jsonAddString(printer, "shortTableLabel", "profiles");
      jsonAddString(printer, "longTableLabel", "Profiles");
      jsonAddString(printer, "globalIdentifier",
                    "org.zowe.zss.security-mgmt.groups");
      jsonAddInt(printer, "minRows", 0);
      jsonAddInt(printer, "maxRows", GROUP_PROFILE_MAX_COUNT);
    }
    jsonEndObject(printer);

    jsonStartArray(printer, "columnMetaData");
    {

      jsonStartObject(printer, NULL);
      {
        jsonAddString(printer, "columnIdentifier", "group");
        jsonAddString(printer, "shortColumnLabel", "group");
        jsonAddString(printer, "longColumnLabel", "Group Name");
        jsonAddString(printer, "rawDataType", "string");
        jsonAddInt(printer, "rawDataTypeLength", sizeof(groups[0].group));
      }
      jsonEndObject(printer);

      jsonStartObject(printer, NULL);
      {
        jsonAddString(printer, "columnIdentifier", "owner");
        jsonAddString(printer, "shortColumnLabel", "owner");
        jsonAddString(printer, "longColumnLabel", "Owner");
        jsonAddString(printer, "rawDataType", "string");
        jsonAddInt(printer, "rawDataTypeLength", sizeof(groups[0].owner));
      }
      jsonEndObject(printer);

      jsonStartObject(printer, NULL);
      {
        jsonAddString(printer, "columnIdentifier", "superiorGroup");
        jsonAddString(printer, "shortColumnLabel", "superiorGroup");
        jsonAddString(printer, "longColumnLabel", "Superior Group");
        jsonAddString(printer, "rawDataType", "string");
        jsonAddInt(printer, "rawDataTypeLength",
                   sizeof(groups[0].superiorGroup));
      }
      jsonEndObject(printer);

    }
    jsonEndArray(printer);

  }
  jsonEndObject(printer);

  int groupsToReturn = (groupCount > groupsRequested) ?
                        groupCount - 1 : groupCount;

  jsonAddString(printer, "start", startGroup ? (char *)startGroup : "");
  jsonAddInt(printer, "count", groupsToReturn);
  jsonAddInt(printer, "countRequested", groupsRequested);
  if (groupsRequested < groupCount) {
    jsonAddBoolean(printer, "hasMore", TRUE);
  } else {
    jsonAddBoolean(printer, "hasMore", FALSE);
  }

  jsonStartArray(printer, "rows");
  {
    for (int i = 0; i < groupsToReturn; i++) {
      const ZISGroupProfileEntry *entry = &groups[i];
      printGroupProfileToJSON(printer, entry);
      zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
             "group entry #%d: %.*s, %.*s\n", i,
             sizeof(entry->group), entry->group,
             sizeof(entry->owner), entry->owner);
    }
  }
  jsonEndArray(printer);

  jsonEnd(printer);

  finishResponse(response);

}

static void formatZISGroupProfileServiceError(int zisCallRC,
                                              const void *serviceStatus,
                                              char *messageBuffer,
                                              size_t messageBufferSize) {

  const ZISGroupProfileServiceStatus *status = serviceStatus;

  if (zisCallRC == RC_ZIS_SRVC_SERVICE_FAILED) {

    int serviceRC = status->baseStatus.serviceRC;
    int serviceRSN = status->baseStatus.serviceRSN;
    if (serviceRC > RC_ZIS_GRPRFSRV_MAX_RC) {
      snprintf(messageBuffer, messageBufferSize,
               "Unknown ZIS service error (RC = %d, RSN = %d)",
               serviceRC, serviceRSN);
      return;
    }

    const char *description = ZIS_GPPRFSRV_SERVICE_RC_DESCRIPTION[serviceRC];
    if (serviceRC == RC_ZIS_GPPRFSRV_INTERNAL_SERVICE_FAILED) {
      snprintf(messageBuffer, messageBufferSize,
               "R_admin failed (RC = %d, SAF RC = %d, RACF RC = %d, RSN = %d)",
               status->internalServiceRC,
               status->safStatus.safRC,
               status->safStatus.racfRC,
               status->safStatus.racfRSN);
    } else {
      snprintf(messageBuffer, messageBufferSize,
               "ZIS service failed (RC = %d, RSN = %d) - %s",
               serviceRC, serviceRSN, description);
    }
    return;

  }

  if (zisCallRC > RC_ZIS_SRVC_GRPRFSRV_MAX_RC) {
    snprintf(messageBuffer, messageBufferSize,
             "Unknown ZIS error (RC = %d)", zisCallRC);
    return;
  }

  const char *description = ZIS_GPPRFSRV_WRAPPER_RC_DESCRIPTION[zisCallRC];
  snprintf(messageBuffer, messageBufferSize, "ZIS failed (RC = %d) - %s",
           zisCallRC, description);

}

static void respondToGroupGET(GroupMgmtCommonParms *commonParms,
                              HttpService *service,
                              HttpResponse *response) {

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  HttpRequest *request = response->request;

  int zisRC = RC_ZIS_SRVC_OK;
  char *startGroup = getQueryParam(response->request, "start");
  int groupsToExtract = getGroupCount(request);
  if (groupsToExtract < 0 || GROUP_PROFILE_MAX_COUNT < groupsToExtract) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Count out of range");
    return;
  }

  ZISGroupProfileEntry resultBuffer[GROUP_PROFILE_MAX_COUNT + 1];
  unsigned int groupsExtracted = 0;
  ZISGroupProfileServiceStatus status = {0};

  zisRC = zisExtractGroupProfiles(
      commonParms->zisServerName,
      startGroup,
      groupsToExtract + 1,
      resultBuffer,
      &groupsExtracted,
      &status,
      commonParms->traceLevel
  );

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisExtractGroupProfiles: RC=%d, base=(%d,%d,%d,%d), "
         "internal=(%d,%d), SAF=(%d,%d,%d)\n", zisRC,
         status.baseStatus.cmsRC, status.baseStatus.cmsRSN,
         status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
         status.internalServiceRC, status.internalServiceRSN,
         status.safStatus.safRC, status.safStatus.racfRC,
         status.safStatus.racfRSN);

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisExtractGroupProfiles: profiles to extract = %u, "
         "extracted = %u\n",
         groupsToExtract, groupsExtracted);

  if (zisRC == RC_ZIS_SRVC_OK) {
    respondWithGroupProfiles(response, startGroup, groupsToExtract,
                             resultBuffer, groupsExtracted);
  } else {
    respondWithZISError(response, zisRC, &status,
                        formatZISGroupProfileServiceError);
  }

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);

}

typedef enum GroupAction_tag {
  GROUP_ACTION_ADD_GROUP,
  GROUP_ACTION_REMOVE_GROUP,
  GROUP_ACTION_UPDATE_GROUP
} GroupAction;

static void respondWithGroupAdminInfo(
    HttpResponse *response,
    GroupAction action,
    bool isDryRun,
    const ZISGroupAdminServiceMessage *operatorCommand)
{

  if (isDryRun) {

    jsonPrinter *printer = respondWithJsonPrinter(response);
    setResponseStatus(response, HTTP_STATUS_OK, "OK");
    setContentType(response, "text/json");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
    writeHeader(response);

    jsonStart(printer);

    jsonAddString(printer, "_objectType",
                  "org.zowe.zss.security-mgmt.operator-command");
    jsonAddString(printer, "resultMetaDataSchemaVersion", "1.0");
    jsonAddString(printer, "serviceVersion", "1.0");


    jsonAddLimitedString(printer, "operatorCommand",
                         (char *)operatorCommand->text,
                         operatorCommand->length);

    jsonEnd(printer);

  } else {

    if (action == GROUP_ACTION_ADD_GROUP) {
      setResponseStatus(response, HTTP_STATUS_CREATED, "OK");
    } else {
      setResponseStatus(response, HTTP_STATUS_NO_CONTENT, "OK");
    }
    setContentType(response, "text/plain");
    addIntHeader(response, "Content-Length", 0);
    addStringHeader(response,"Server","jdmfws");
    writeHeader(response);

  }

  finishResponse(response);

}

static void formatZISGroupAdminServiceError(int zisCallRC,
                                             const void *serviceStatus,
                                             char *messageBuffer,
                                             size_t messageBufferSize) {

  const ZISGroupAdminServiceStatus *status = serviceStatus;

  if (zisCallRC == RC_ZIS_SRVC_SERVICE_FAILED) {

    int serviceRC = status->baseStatus.serviceRC;
    int serviceRSN = status->baseStatus.serviceRSN;
    if (serviceRC > RC_ZIS_GSADMNSRV_MAX_RC) {
      snprintf(messageBuffer, messageBufferSize,
               "Unknown ZIS service error (RC = %d, RSN = %d)",
               serviceRC, serviceRSN);
      return;
    }

    const char *description = ZIS_GRPASRV_SERVICE_RC_DESCRIPTION[serviceRC];
    if (serviceRC == RC_ZIS_GRPASRV_INTERNAL_SERVICE_FAILED) {

      const char *radminMessage = NULL;
      size_t radminMessageLength = 0;
      if (status->message.length > 0) {
        radminMessage = status->message.text;
        radminMessageLength = status->message.length;
      } else {
        radminMessage = "no additional info";
        radminMessageLength = strlen(radminMessage);
      }

      snprintf(messageBuffer, messageBufferSize,
               "R_admin failed (RC = %d, SAF RC = %d, RACF RC = %d, RSN = %d)"
               " - \'%.*s\'",
               status->internalServiceRC,
               status->safStatus.safRC,
               status->safStatus.racfRC,
               status->safStatus.racfRSN,
               radminMessageLength, radminMessage);

    } else {
      snprintf(messageBuffer, messageBufferSize,
               "ZIS service failed (RC = %d, RSN = %d) - %s",
               serviceRC, serviceRSN, description);
    }
    return;

  }

  if (zisCallRC > RC_ZIS_SRVC_PADMIN_MAX_RC) {
    snprintf(messageBuffer, messageBufferSize,
             "Unknown ZIS error (RC = %d)", zisCallRC);
    return;
  }

  const char *description = ZIS_GRPASRV_WRAPPER_RC_DESCRIPTION[zisCallRC];
  snprintf(messageBuffer, messageBufferSize, "ZIS failed (RC = %d) - %s",
           zisCallRC, description);

}

static void respondToGroupPOST(GroupMgmtCommonParms *commonParms,
                               HttpService *service,
                               HttpResponse *response) {

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  if (strlen(commonParms->groupName) == 0) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "profile name required");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "group name required for profile POST\n");
    return;
  }

  if (commonParms->requestBody == NULL) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Body missing");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "body not provided for group POST, leaving...\n");
    return;
  }

  Json *superiorGroupParm =
      jsonObjectGetPropertyValue(commonParms->requestBody, "superiorGroup");
  if (superiorGroupParm == NULL) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "superiorGroup missing");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "superior not provided for group POST, leaving...\n");
    return;
  }

  char *superiorGroupString = jsonAsString(superiorGroupParm);
  if (superiorGroupString == NULL) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "superior group has bad type");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "bad superior group provided for group POST, leaving...\n");
    return;
  }


  bool isDryRun = commonParms->isDryRun;

  ZISGroupAdminServiceMessage operatorCommand = {0};
  ZISGroupAdminServiceStatus status = {0};
  int zisRC = zisAddGroup(
      commonParms->zisServerName,
      commonParms->groupName,
      superiorGroupString,
      isDryRun,
      &operatorCommand,
      &status,
      commonParms->traceLevel
  );

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisAddGroup: RC=%d, base=(%d,%d,%d,%d), internal=(%d,%d), "
         "SAF=(%d,%d,%d)\n", zisRC,
         status.baseStatus.cmsRC, status.baseStatus.cmsRSN,
         status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
         status.internalServiceRC, status.internalServiceRSN,
         status.safStatus.safRC, status.safStatus.racfRC,
         status.safStatus.racfRSN);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisAddGroup: serviceMsg=\'%.*s\'\n",
         status.message.length, status.message.text);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisAddGroup: isDryRun=%d, operCmd=\'%.*s\'\n",
         isDryRun, operatorCommand.length, operatorCommand.text);

  if (zisRC == RC_ZIS_SRVC_OK) {
    respondWithGroupAdminInfo(response, GROUP_ACTION_ADD_GROUP, isDryRun,
                              &operatorCommand);
  } else {
    respondWithZISError(response, zisRC, &status,
                        formatZISGroupAdminServiceError);
  }

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);

}

static void respondToGroupDELETE(GroupMgmtCommonParms *commonParms,
                                 HttpService *service,
                                 HttpResponse *response) {

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  if (strlen(commonParms->groupName) == 0) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "profile name required");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "profile name required for profile DELETE\n");
    return;
  }

  bool isDryRun = commonParms->isDryRun;

  ZISGroupAdminServiceMessage operatorCommand = {0};
  ZISGroupAdminServiceStatus status = {0};
  int zisRC = zisDeleteGroup(
      commonParms->zisServerName,
      commonParms->groupName,
      isDryRun,
      &operatorCommand,
      &status,
      commonParms->traceLevel
  );

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisDeleteGroup: RC=%d, base=(%d,%d,%d,%d), internal=(%d,%d), "
         "SAF=(%d,%d,%d)\n", zisRC,
         status.baseStatus.cmsRC, status.baseStatus.cmsRSN,
         status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
         status.internalServiceRC, status.internalServiceRSN,
         status.safStatus.safRC, status.safStatus.racfRC,
         status.safStatus.racfRSN);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisDeleteGroup:  serviceMsg=\'%.*s\'\n",
         status.message.length, status.message.text);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisDeleteGroup: isDryRun=%d, operCmd=\'%.*s\'\n",
         isDryRun, operatorCommand.length, operatorCommand.text);

  if (zisRC == RC_ZIS_SRVC_OK) {
    respondWithGroupAdminInfo(response, GROUP_ACTION_REMOVE_GROUP, isDryRun,
                               &operatorCommand);
  } else {
    respondWithZISError(response, zisRC, &status,
                        formatZISGroupAdminServiceError);
  }

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);

}

static ZISGroupAdminServiceAccessType
translateGroupAccessType(const char *typeString) {

  if (!strcmp(typeString, "USE")) {
    return ZIS_GROUP_ADMIN_ACESS_TYPE_USE;
  }

  if (!strcmp(typeString, "CREATE")) {
    return ZIS_GROUP_ADMIN_ACESS_TYPE_CREATE;
  }

  if (!strcmp(typeString, "CONNECT")) {
    return ZIS_GROUP_ADMIN_ACESS_TYPE_CONNECT;
  }

  if (!strcmp(typeString, "JOIN")) {
    return ZIS_GROUP_ADMIN_ACESS_TYPE_JOIN;
  }

  return ZIS_GROUP_ADMIN_ACESS_TYPE_UNKNOWN;
}

static void respondToGroupAccessListPUT(GroupMgmtCommonParms *commonParms,
                                        HttpService *service,
                                        HttpResponse *response) {

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  if (strlen(commonParms->groupName) == 0) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "group name required");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "group name required for user POST/PUT\n");
    return;
  }

  if (strlen(commonParms->accessListEntryID) == 0) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "user ID required");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "user ID required for user POST/PUT\n");
    return;
  }

  if (commonParms->requestBody == NULL) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Body missing");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "body not provided for user POST/PUT, leaving...\n");
    return;
  }

  Json *accessTypeParm = jsonObjectGetPropertyValue(commonParms->requestBody,
                                                    "accessType");
  if (accessTypeParm == NULL) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "accessType missing");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "access type not provided for user POST/PUT, leaving...\n");
    return;
  }

  char *accessTypeString = jsonAsString(accessTypeParm);
  if (accessTypeString == NULL) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "accessType has bad type");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "bad access type provided for user POST/PUT, leaving...\n");
    return;
  }

  ZISGroupAdminServiceAccessType accessType =
      translateGroupAccessType(accessTypeString);
  if (accessType == ZIS_GROUP_ADMIN_ACESS_TYPE_UNKNOWN) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "unknown access type, use [USE, CREATE, CONNECT, JOIN]");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "unknown access type (%d( provided for user POST/PUT, leaving...\n",
           accessType);
    return;
  }

  bool isDryRun = commonParms->isDryRun;

  ZISGroupAdminServiceMessage operatorCommand = {0};
  ZISGroupAdminServiceStatus status = {0};
  int zisRC = zisConnectToGroup(
      commonParms->zisServerName,
      commonParms->accessListEntryID,
      commonParms->groupName,
      accessType,
      isDryRun,
      &operatorCommand,
      &status,
      commonParms->traceLevel
  );

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisConnectToGroup: RC=%d, base=(%d,%d,%d,%d), internal=(%d,%d), "
         "SAF=(%d,%d,%d)\n", zisRC,
         status.baseStatus.cmsRC, status.baseStatus.cmsRSN,
         status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
         status.internalServiceRC, status.internalServiceRSN,
         status.safStatus.safRC, status.safStatus.racfRC,
         status.safStatus.racfRSN);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisConnectToGroup:  serviceMsg=\'%.*s\'\n",
         status.message.length, status.message.text);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisConnectToGroup: isDryRun=%d, operCmd=\'%.*s\'\n",
         isDryRun, operatorCommand.length, operatorCommand.text);

  if (zisRC == RC_ZIS_SRVC_OK) {
    respondWithGroupAdminInfo(response, GROUP_ACTION_UPDATE_GROUP, isDryRun,
                              &operatorCommand);
  } else {
    respondWithZISError(response, zisRC, &status,
                        formatZISGroupAdminServiceError);
  }

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);

}

static void printGroupAccessListEntryToJSON(jsonPrinter *printer,
                                           const ZISGroupAccessEntry *entry) {

  char idNullTerm[sizeof(entry->id) + 1] = {0};
  char accessTypeNullTerm[sizeof(entry->accessType) + 1] = {0};
  memcpy(idNullTerm, entry->id, sizeof(entry->id));
  memcpy(accessTypeNullTerm, entry->accessType, sizeof(entry->accessType));

  nullTerminate(idNullTerm, sizeof(idNullTerm) - 1);
  nullTerminate(accessTypeNullTerm, sizeof(accessTypeNullTerm) - 1);

  jsonStartObject(printer, NULL);
  {
    jsonAddString(printer, "id", idNullTerm);
    jsonAddString(printer, "accessType", accessTypeNullTerm);
  }
  jsonEndObject(printer);

}

static void respondWithGroupAccessList(HttpResponse *response,
                                       const ZISGroupAccessEntry *accessList,
                                       unsigned int accessListEntryCount) {


  jsonPrinter *printer = respondWithJsonPrinter(response);
  setResponseStatus(response, HTTP_STATUS_OK, "OK");
  setContentType(response, "text/json");
  addStringHeader(response, "Server", "jdmfws");
  addStringHeader(response, "Transfer-Encoding", "chunked");
  writeHeader(response);

  jsonStart(printer);

  printTableResultResponseMetadata(printer);
  jsonAddString(printer, "resultType", "table");
  jsonStartObject(printer, "metaData");
  {

    jsonStartObject(printer, "tableMetaData");
    {
      jsonAddString(printer, "tableIdentifier", "access-list");
      jsonAddString(printer, "shortTableLabel", "access-list");
      jsonAddString(printer, "longTableLabel", "Access List");
      jsonAddString(printer, "globalIdentifier",
                    "org.zowe.zss.security-mgmt.group-access-list");
    }
    jsonEndObject(printer);

    jsonStartArray(printer, "columnMetaData");
    {

      jsonStartObject(printer, NULL);
      {
        jsonAddString(printer, "columnIdentifier", "id");
        jsonAddString(printer, "shortColumnLabel", "id");
        jsonAddString(printer, "longColumnLabel", "ID");
        jsonAddString(printer, "rawDataType", "string");
        jsonAddInt(printer, "rawDataTypeLength", sizeof(accessList[0].id));
      }
      jsonEndObject(printer);

      jsonStartObject(printer, NULL);
      {
        jsonAddString(printer, "columnIdentifier", "accessType");
        jsonAddString(printer, "shortColumnLabel", "accessType");
        jsonAddString(printer, "longColumnLabel", "Access Type");
        jsonAddString(printer, "rawDataType", "string");
        jsonAddInt(printer, "rawDataTypeLength",
                   sizeof(accessList[0].accessType));
      }
      jsonEndObject(printer);

    }
    jsonEndArray(printer);

  }
  jsonEndObject(printer);

  jsonAddInt(printer, "count", accessListEntryCount);

  jsonStartArray(printer, "rows");
  {
    for (int i = 0; i < accessListEntryCount; i++) {
      const ZISGroupAccessEntry *entry = &accessList[i];
      printGroupAccessListEntryToJSON(printer, entry);
      zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
             "access list entry #%d: %.*s, %.*s\n", i,
             sizeof(entry->id), entry->id,
             sizeof(entry->accessType), entry->accessType);
    }
  }
  jsonEndArray(printer);

  jsonEnd(printer);

  finishResponse(response);

}

static void formatZISGroupAccessListServiceError(int zisCallRC,
                                                 const void *serviceStatus,
                                                 char *messageBuffer,
                                                 size_t messageBufferSize) {

  const ZISGroupAccessListServiceStatus *status = serviceStatus;

  if (zisCallRC == RC_ZIS_SRVC_SERVICE_FAILED) {

    int serviceRC = status->baseStatus.serviceRC;
    int serviceRSN = status->baseStatus.serviceRSN;
    if (serviceRC > RC_ZIS_ACSLSRV_MAX_RC) {
      snprintf(messageBuffer, messageBufferSize,
               "Unknown ZIS service error (RC = %d, RSN = %d)",
               serviceRC, serviceRSN);
      return;
    }

    const char *description = ZIS_GRPALSRV_SERVICE_RC_DESCRIPTION[serviceRC];
    if (serviceRC == RC_ZIS_GRPALSRV_INTERNAL_SERVICE_FAILED) {
      snprintf(messageBuffer, messageBufferSize,
               "R_admin failed (RC = %d, SAF RC = %d, RACF RC = %d, RSN = %d)",
               status->internalServiceRC,
               status->safStatus.safRC,
               status->safStatus.racfRC,
               status->safStatus.racfRSN);
    } else {
      snprintf(messageBuffer, messageBufferSize,
               "ZIS service failed (RC = %d, RSN = %d) - %s",
               serviceRC, serviceRSN, description);
    }
    return;

  }

  if (zisCallRC > RC_ZIS_SRVC_ACSLSRV_MAX_RC) {
    snprintf(messageBuffer, messageBufferSize,
             "Unknown ZIS error (RC = %d)", zisCallRC);
    return;
  }

  const char *description = ZIS_GRPALSRV_WRAPPER_RC_DESCRIPTION[zisCallRC];
  snprintf(messageBuffer, messageBufferSize, "ZIS failed (RC = %d) - %s",
           zisCallRC, description);

}

#define GROUP_MGMT_ACCESS_LIST_DEFAULT_SIZE     128
#define GROUP_MGMT_ACCESS_LIST_MAX_SIZE         8192

static void respondToGroupAccessListGET(GroupMgmtCommonParms *commonParms,
                                        HttpService *service,
                                        HttpResponse *response) {

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  if (strlen(commonParms->accessListEntryID) > 0) {
    respondWithError(response, HTTP_STATUS_NOT_IMPLEMENTED,
                     "specific user access status retrieval not implemented");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "access list can only be retrieved in bulk, leaving...\n");
    return;
  }

  unsigned int resultBufferCapacity = GROUP_MGMT_ACCESS_LIST_DEFAULT_SIZE;
  unsigned int resultBufferSize = 0;
  ZISGroupAccessEntry *resultBuffer = NULL;
  int zisRC = RC_ZIS_SRVC_OK;
  unsigned int entriesExtracted = 0;
  ZISGroupAccessListServiceStatus status = {0};

  for (int i = 0; i < 2; i++) {

    resultBufferSize = sizeof(ZISGroupAccessEntry) * resultBufferCapacity;
    resultBuffer =
        (ZISGroupAccessEntry *)safeMalloc(resultBufferSize, "access list");
    if (resultBuffer == NULL) {
      respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,
                       "out of memory");
      zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
             "access list buffer with size %u not allocated, leaving...\n",
             resultBufferSize);
      return;
    }

    zisRC = zisExtractGroupAccessList(
        commonParms->zisServerName,
        commonParms->groupName,
        resultBuffer,
        resultBufferCapacity,
        &entriesExtracted,
        &status,
        commonParms->traceLevel
    );

    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
           "zisExtractGroupAccessList: RC=%d, base=(%d,%d,%d,%d), "
           "internal=(%d,%d), SAF=(%d,%d,%d)\n", zisRC,
           status.baseStatus.cmsRC, status.baseStatus.cmsRSN,
           status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
           status.internalServiceRC, status.internalServiceRSN,
           status.safStatus.safRC, status.safStatus.racfRC,
           status.safStatus.racfRSN);

    if (zisRC == RC_ZIS_SRVC_GRPALSRV_INSUFFICIENT_BUFFER) {

      safeFree((char *)resultBuffer, resultBufferSize);
      resultBuffer = NULL;

      if (entriesExtracted <= 0 ||
          GROUP_MGMT_ACCESS_LIST_MAX_SIZE < entriesExtracted) {
        respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,
                         "access list size our of range");
        zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
               "access list size our of range (%u), leaving...\n",
               entriesExtracted);
        return;
      }

      resultBufferCapacity = entriesExtracted;
      zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
             "access list will be re-allocated with capacity %u\n",
             resultBufferCapacity);

    } else {
      break;
    }

  }

  if (zisRC == RC_ZIS_SRVC_OK) {
    respondWithGroupAccessList(response, resultBuffer, entriesExtracted);
  } else {
    respondWithZISError(response, zisRC, &status,
                        formatZISGroupAccessListServiceError);
  }

  safeFree((char *)resultBuffer, resultBufferSize);

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "access list buffer with size %u freed @ 0x%p\n",
         resultBufferSize, resultBuffer);

  resultBuffer = NULL;

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);

}

static void respondToGroupAccessListDELETE(GroupMgmtCommonParms *commonParms,
                                           HttpService *service,
                                           HttpResponse *response) {

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  if (strlen(commonParms->groupName) == 0) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "group name required");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "group name required for access list  DELETE\n");
    return;
  }

  if (strlen(commonParms->accessListEntryID) == 0) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "access list entry name required");
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "access list entry name required for access list DELETE\n");
    return;
  }

  HttpRequest *request = response->request;
  bool isDryRun = commonParms->isDryRun;

  ZISGroupAdminServiceMessage operatorCommand;
  ZISGroupAdminServiceStatus status = {0};
  int zisRC = zisRemoveFromGroup(
      commonParms->zisServerName,
      commonParms->accessListEntryID,
      commonParms->groupName,
      isDryRun,
      &operatorCommand,
      &status,
      commonParms->traceLevel
  );

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisRemoveFromGroup: RC=%d, base=(%d,%d,%d,%d), "
         "internal=(%d,%d), SAF=(%d,%d,%d)\n", zisRC,
         status.baseStatus.cmsRC, status.baseStatus.cmsRSN,
         status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
         status.internalServiceRC, status.internalServiceRSN,
         status.safStatus.safRC, status.safStatus.racfRC,
         status.safStatus.racfRSN);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisRemoveFromGroup:  serviceMsg=\'%.*s\'\n",
         status.message.length, status.message.text);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "zisRemoveFromGroup: isDryRun=%d, operCmd=\'%.*s\'\n",
         isDryRun, operatorCommand.length, operatorCommand.text);

  if (zisRC == RC_ZIS_SRVC_OK) {
    respondWithGroupAdminInfo(response, GROUP_ACTION_UPDATE_GROUP, isDryRun,
                              &operatorCommand);
  } else {
    respondWithZISError(response, zisRC, &status,
                        formatZISGroupAdminServiceError);
  }

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);

}

static void extractGroupMgmtCommonParms(HttpService *service,
                                        HttpRequest *request,
                                        GroupMgmtCommonParms *parms) {

  parms->zisServerName =
      getConfiguredProperty(service->server,
                            HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY);
  extractGroupMgmtQueryParms(request->parsedFile,
                             &parms->groupName,
                             &parms->accessListEntryID);
  parms->requestBody = getJsonBody(request->contentBody,
                                   request->contentLength,
                                   request->slh);

  parms->isDryRun = getDryRunValue(request);
  parms->traceLevel = getTraceLevel(request);

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "group-mgmt parms: zisServerName=\'%.16s\', "
         "gorup=\'%s\', accessListEntryID=\'%s\', "
         "traceLevel=%d\n",
         parms->zisServerName->nameSpacePadded,
         parms->groupName ? parms->groupName : "null",
         parms->accessListEntryID ? parms->accessListEntryID : "null",
         parms->traceLevel);

}

static int serveGroupManagement(HttpService *service,
                                HttpResponse *response) {

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  HttpRequest *request = response->request;

  if (isGroupMgmtQueryStringValid(request->parsedFile) == false) {
    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
           "group-mgmt query string is invalid, leaving...\n");
    respondWithError(response, HTTP_STATUS_BAD_REQUEST, "Incorrect query");
    return 0;
  }

  GroupMgmtCommonParms commonParms;
  extractGroupMgmtCommonParms(service, response->request, &commonParms);

  GroupMgmtRequestType requestType = getGroupMgmtRequstType(request);

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
         "group-mgmt requestType = %d\n", requestType);

  switch (requestType) {
  case GROUP_MGMT_GROUP_GET:
    respondToGroupGET(&commonParms, service, response);
    break;
  case GROUP_MGMT_GROUP_POST:
    respondToGroupPOST(&commonParms, service, response);
    break;
  case GROUP_MGMT_GROUP_DELETE:
    respondToGroupDELETE(&commonParms, service, response);
    break;
  case GROUP_MGMT_ACCESS_LIST_GET:
    respondToGroupAccessListGET(&commonParms, service, response);
    break;
  case GROUP_MGMT_ACCESS_LIST_PUT:
    respondToGroupAccessListPUT(&commonParms, service, response);
    break;
  case GROUP_MGMT_ACCESS_LIST_DELETE:
    respondToGroupAccessListDELETE(&commonParms, service, response);
    break;
  default:
  {
    respondWithError(response, HTTP_STATUS_NOT_IMPLEMENTED,
                     "Unsupported request");
  }
  }

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);
  return 0;
}

#define DEFAULT_TEAM_PROFILE_COUNT  128

static ZISGenresProfileEntry *getProfilesByPrefixOrFail(
    HttpResponse *response,
    CrossMemoryServerName *zisServerName,
    const char *profilePrefix,
    int *profileCount,
    int traceLevel
 ) {

  ShortLivedHeap *slh = response->slh;

  unsigned int prefixSize = strlen(profilePrefix);
  if (prefixSize > ZIS_SECURITY_PROFILE_MAX_LENGTH) {
    /* TODO handle */
    return NULL;
  }

  char startProfile[ZIS_SECURITY_PROFILE_MAX_LENGTH + 1] = {0};
  strcpy(startProfile, profilePrefix);

  unsigned int resultBufferCapacity = DEFAULT_TEAM_PROFILE_COUNT;
  unsigned int resultBufferSize =
      resultBufferCapacity * sizeof(ZISGenresProfileEntry);
  ZISGenresProfileEntry *resultBuffer =
      (ZISGenresProfileEntry *)SLHAlloc(slh, resultBufferSize);
  unsigned int profilesFound = 0;

  ZISGenresProfileEntry workBuffer[GENRES_PROFILE_MAX_COUNT];

  while (TRUE) {

    unsigned int profilesToExtract = sizeof(workBuffer) / sizeof(workBuffer[0]);
    unsigned int profilesExtracted = 0;
    ZISGenresProfileServiceStatus status = {0};

    int zisRC = zisExtractGenresProfiles(
        zisServerName,
        NULL,
        startProfile,
        profilesToExtract,
        workBuffer,
        &profilesExtracted,
        &status,
        traceLevel
    );

    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
           "getProfilesByClassAndPrefix: RC=%d, base=(%d,%d,%d,%d), "
           "internal=(%d,%d), SAF=(%d,%d,%d)\n", zisRC,
           status.baseStatus.cmsRC, status.baseStatus.cmsRSN,
           status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
           status.internalServiceRC, status.internalServiceRSN,
           status.safStatus.safRC, status.safStatus.racfRC,
           status.safStatus.racfRSN);

    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
           "getProfilesByClassAndPrefix: profiles to extract = %u, "
           "extracted = %u\n",
           profilesToExtract, profilesExtracted);

    if (zisRC != RC_ZIS_SRVC_OK) {
      respondWithZISError(response, zisRC, &status,
                          formatZISGenresProfileServiceError);
      *profileCount = 0;
      return NULL;
    }

    int profilesToProcess = profilesExtracted < profilesToExtract ?
                            profilesExtracted : profilesToExtract - 1;
    for (int i = 0; i < profilesToProcess; i++) {

      if (memcmp(&workBuffer[i].profile, profilePrefix, prefixSize) != 0) {
        continue;
      }

      if (resultBufferCapacity < profilesFound + 1) {
        resultBufferCapacity *= 2;
        unsigned int newBufferSize =
            resultBufferCapacity * sizeof(ZISGenresProfileEntry);
        ZISGenresProfileEntry *newBuffer =
            (ZISGenresProfileEntry *)SLHAlloc(slh, newBufferSize);
        memcpy(newBuffer, resultBuffer,
               sizeof(resultBuffer[0]) * profilesFound);
        resultBuffer = newBuffer;
      }

      resultBuffer[profilesFound++] = workBuffer[i];

    }

    if (profilesExtracted < profilesToExtract) {
      break;
    }

    memcpy(startProfile, workBuffer[profilesToExtract - 1].profile,
           sizeof(startProfile) - 1);
    nullTerminate(startProfile, sizeof(startProfile) - 1);

  }

  *profileCount = profilesFound;
  return resultBuffer;
}

static ZISGroupAccessEntry *getAccessListByGroupOrFail(
    HttpResponse *response,
    CrossMemoryServerName *zisServerName,
    const char *group,
    int *entryCount,
    int traceLevel
) {

  ShortLivedHeap *slh = response->slh;
  unsigned int resultBufferCapacity = GROUP_MGMT_ACCESS_LIST_DEFAULT_SIZE;
  unsigned int resultBufferSize = 0;
  ZISGroupAccessEntry *resultBuffer = NULL;
  int zisRC = RC_ZIS_SRVC_OK;
  unsigned int entriesExtracted = 0;
  ZISGroupAccessListServiceStatus status = {0};

  for (int i = 0; i < 2; i++) {

    resultBufferSize = sizeof(ZISGroupAccessEntry) * resultBufferCapacity;
    resultBuffer =
        (ZISGroupAccessEntry *)SLHAlloc(slh, resultBufferSize);

    zisRC = zisExtractGroupAccessList(
        zisServerName,
        group,
        resultBuffer,
        resultBufferCapacity,
        &entriesExtracted,
        &status,
        traceLevel
    );

    zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
           "getAccessListByGroup: RC=%d, base=(%d,%d,%d,%d), "
           "internal=(%d,%d), SAF=(%d,%d,%d)\n", zisRC,
           status.baseStatus.cmsRC, status.baseStatus.cmsRSN,
           status.baseStatus.serviceRC, status.baseStatus.serviceRSN,
           status.internalServiceRC, status.internalServiceRSN,
           status.safStatus.safRC, status.safStatus.racfRC,
           status.safStatus.racfRSN);

    if (zisRC == RC_ZIS_SRVC_GRPALSRV_INSUFFICIENT_BUFFER) {

      if (entriesExtracted <= 0 ||
          GROUP_MGMT_ACCESS_LIST_MAX_SIZE < entriesExtracted) {
        respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,
                         "access list size our of range");
        zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_WARNING,
               "access list size our of range (%u), leaving...\n",
               entriesExtracted);
        *entryCount = 0;
        return NULL;
      }

      resultBufferCapacity = entriesExtracted;
      zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG,
             "access list will be re-allocated with capacity %u\n",
             resultBufferCapacity);

    } else if (zisRC != RC_ZIS_SRVC_OK) {
      respondWithZISError(response, zisRC, &status,
                          formatZISGroupAccessListServiceError);
      *entryCount = 0;
      return NULL;
    }
  }

  *entryCount = entriesExtracted;
  return resultBuffer;
}

static const char *translateSAFAccessLevelToString(int accessLevel,
                                                   bool isTeamFormat) {

  if (isTeamFormat) {
    switch (accessLevel) {
    case ZIS_SAF_ACCESS_NONE:
      return "n/a";
    case ZIS_SAF_ACCESS_READ:
      return "user";
    case ZIS_SAF_ACCESS_UPDATE:
      return "admin";
    case ZIS_SAF_ACCESS_CONTROL:
    case ZIS_SAF_ACCESS_ALETER:
      return "superadmin";
    default:
      return "n/a";
    }
  } else {
    switch (accessLevel) {
    case ZIS_SAF_ACCESS_NONE:
      return "NONE";
    case ZIS_SAF_ACCESS_READ:
      return "READ";
    case ZIS_SAF_ACCESS_UPDATE:
      return "UPDATE";
    case ZIS_SAF_ACCESS_CONTROL:
      return "CONTROL";
    case ZIS_SAF_ACCESS_ALETER:
      return "ALTER";
    default:
      return "UNKNOWN";
    }
  }

}

static void formatZISProfileAccessError(int zisCallRC,
                                        const void *serviceStatus,
                                        char *messageBuffer,
                                        size_t messageBufferSize) {

  const ZISAuthServiceStatus *status = serviceStatus;

  if (zisCallRC == RC_ZIS_SRVC_SERVICE_FAILED) {

    int serviceRC = status->baseStatus.serviceRC;
    int serviceRSN = status->baseStatus.serviceRSN;
    if (serviceRC > RC_ZIS_AUTHSRV_MAX_RC) {
      snprintf(messageBuffer, messageBufferSize,
               "Unknown ZIS service error (RC = %d, RSN = %d)",
               serviceRC, serviceRSN);
      return;
    }

    if (serviceRC == RC_ZIS_AUTHSRV_SAF_ABENDED) {
      snprintf(messageBuffer, messageBufferSize,
               "ZIS service failed (RC = %d, RSN = %d) - "
               "SAF ABENDed with S%03X-%02X",
               serviceRC, serviceRSN,
               status->abendInfo.completionCode,
               status->abendInfo.reasonCode);
    } else if (serviceRC == RC_ZIS_AUTHSRV_SAF_ERROR ||
               serviceRC == RC_ZIS_AUTHSRV_CREATE_FAILED ||
               serviceRC == RC_ZIS_AUTHSRV_DELETE_FAILED) {
      const char *description = ZIS_AUTH_RC_DESCRIPTION[serviceRC];
      snprintf(messageBuffer, messageBufferSize,
               "ZIS service failed (RC = %d, RSN = %d) - %s "
               "(SAF RC = %d, RACF RC = %d, RSN = %d)",
               serviceRC, serviceRSN, description,
               status->safStatus.safRC,
               status->safStatus.racfRC,
               status->safStatus.racfRSN);
    } else {
      const char *description = ZIS_AUTH_RC_DESCRIPTION[serviceRC];
      snprintf(messageBuffer, messageBufferSize,
               "ZIS service failed (RC = %d, RSN = %d) - %s",
               serviceRC, serviceRSN, description);
    }

  } else {
    snprintf(messageBuffer, messageBufferSize,
             "Unknown ZIS error (RC = %d)", zisCallRC);
  }

}

static bool isFatalAccessInfoError(int zisRC,
                                   const ZISAuthServiceStatus *status) {

  if (zisRC == RC_ZIS_SRVC_OK) {
    return false;
  }

  if (zisRC == RC_ZIS_SRVC_SERVICE_FAILED) {

    int serviceRC = status->baseStatus.serviceRC;

    if (serviceRC == RC_ZIS_AUTHSRV_SAF_ERROR ||
        serviceRC == RC_ZIS_AUTHSRV_SAF_ABENDED ||
        serviceRC == RC_ZIS_AUTHSRV_SAF_NO_DECISION) {
      return false;
    }

  }

  /* Anything other than those ZIS and service RCs is considered bad. */

  return true;
}

static void printAccessInfoError(jsonPrinter *printer, int zisRC,
                                 const ZISAuthServiceStatus *serviceStatus) {

  char errorMessage[ERROR_MSG_MAX_SIZE] = {0};
  jsonStartObject(printer, "error");
  {
    printErrorResponseMetadata(printer);
    formatZISErrorMessage(zisRC, serviceStatus,
                          formatZISProfileAccessError,
                          errorMessage, sizeof(errorMessage));
    jsonAddString(printer, "message", errorMessage);
  }
  jsonEndObject(printer);

}

static void respondWithAccessInfo(HttpResponse *response,
                                  const CrossMemoryServerName *zisServerName,
                                  const ZISGroupAccessEntry *groupAccessList,
                                  unsigned int groupAccessEntryCount,
                                  const ZISGenresProfileEntry *profiles,
                                  unsigned int profileCount,
                                  bool isTeamFormat,
                                  int traceLevel) {

  const char *userKey = isTeamFormat ? "member" : "userID";
  const char *profilesKey = isTeamFormat ? "teams" : "profiles";
  const char *accessTypeKey = isTeamFormat ? "role" : "accessType";

  jsonPrinter *printer = respondWithJsonPrinter(response);
  setResponseStatus(response, HTTP_STATUS_OK, "OK");
  setContentType(response, "text/json");
  addStringHeader(response, "Server", "jdmfws");
  addStringHeader(response, "Transfer-Encoding", "chunked");
  writeHeader(response);

  jsonStart(printer);

  jsonAddString(printer, "_objectType",
                "org.zowe.zss.security-mgmt.access-info");
  jsonAddString(printer, "resultMetaDataSchemaVersion", "1.0");
  jsonAddString(printer, "serviceVersion", "1.0");

  jsonStartArray(printer, "users");
  {
    for (unsigned int accessEntryID = 0; accessEntryID < groupAccessEntryCount;
         accessEntryID++) {

      char id[sizeof(groupAccessList[accessEntryID].id) + 1];
      memset(id, 0, sizeof(id));
      memcpy(id, groupAccessList[accessEntryID].id,
             sizeof(groupAccessList[accessEntryID].id));
      nullTerminate(id, sizeof(id) - 1);

      jsonStartObject(printer, NULL);
      {

        bool accessInfoSvcErrorDetected = false;
        int accessInfoSvcErrorRC = RC_ZIS_SRVC_OK;
        ZISAuthServiceStatus accessInfoSvcErrorStatus = {0};

        jsonAddString(printer, (char *)userKey, id);
        jsonStartArray(printer, (char *)profilesKey);
        {
          for (unsigned int profileID = 0; profileID < profileCount;
               profileID++) {

            char profile[sizeof(profiles[0].profile) + 1];
            memset(profile, 0, sizeof(profile));
            memcpy(profile, profiles[profileID].profile,
                   sizeof(profiles[0].profile));
            nullTerminate(profile, sizeof(profile) - 1);

            ZISSAFAccessLevel accessLevel = ZIS_SAF_ACCESS_NONE;

            ZISAuthServiceStatus status = {0};
            int zisRC = zisGetSAFAccessLevel(zisServerName, id, NULL, profile,
                                             &accessLevel, &status, traceLevel);

            if (zisRC == RC_ZIS_SRVC_OK) {
              if (accessLevel != ZIS_SAF_ACCESS_NONE) {
                jsonStartObject(printer, NULL);
                {
                  const char *accessTypeString =
                      translateSAFAccessLevelToString(accessLevel, isTeamFormat);
                  jsonAddString(printer, "name", profile);
                  jsonAddString(printer, (char *)accessTypeKey,
                                (char *)accessTypeString);
                }
                jsonEndObject(printer);
              }
            } else {

              if (isFatalAccessInfoError(zisRC, &status)) {
                accessInfoSvcErrorDetected = true;
                accessInfoSvcErrorRC = zisRC;
                accessInfoSvcErrorStatus = status;
                break;
              }

              jsonStartObject(printer, NULL);
              {
                jsonAddString(printer, "name", profile);
                printAccessInfoError(printer, zisRC, &status);
              }
              jsonEndObject(printer);
            }

          }
        }
        jsonEndArray(printer);

        if (accessInfoSvcErrorDetected) {
          printAccessInfoError(printer, accessInfoSvcErrorRC,
                               &accessInfoSvcErrorStatus);
        }

      }
      jsonEndObject(printer);

    }
  }
  jsonEndArray(printer);

  jsonEnd(printer);

  finishResponse(response);

}

static const ZISGroupAccessEntry *findAccessListEntryByUserID(
    const ZISGroupAccessEntry *entries,
    int count,
    const char *userID
) {

  char userIDSpacePadded[sizeof(entries->id)];

  size_t userIDLength = strlen(userID);
  if (userIDLength > sizeof(userIDSpacePadded)) {
    return NULL;
  }

  memset(userIDSpacePadded, ' ', sizeof(userIDSpacePadded));
  memcpy(userIDSpacePadded, userID, userIDLength);

  /* TODO consider binary search if the entries are sorted */
  for (int i = 0; i < count; i++) {
    if (!memcmp(entries[i].id, userIDSpacePadded, sizeof(userIDSpacePadded))) {
      return &entries[i];
    }
  }

  return NULL;
}

static int serveAccessInfo(HttpService *service,
                           HttpResponse *response) {

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  HttpRequest *request = response->request;

  CrossMemoryServerName *zisServerName =
      getConfiguredProperty(service->server,
                            HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY);

  char *group = getQueryParam(request, "group");
  if (group == NULL) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "group must be provided");
    return 0;
  }

  char *profile = getQueryParam(request, "profile");
  if (profile == NULL) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,
                     "profile must be provided");
    return 0;
  }

  bool isTeamFormat = false;
  {
    char *format = getQueryParam(request, "format");
    if (format && !strcmp(format, "team")) {
      isTeamFormat = TRUE;
    }
  }

  int traceLevel = getTraceLevel(request);

  int accessEntryCount = 0;
  const ZISGroupAccessEntry *accessEntries =
          getAccessListByGroupOrFail(response,
                                     zisServerName,
                                     group,
                                     &accessEntryCount,
                                     traceLevel);
  if (accessEntries == NULL) {
    return 0;
  }

  char *userID = getQueryParam(request, "userID");
  if (userID != NULL) {
    accessEntries = findAccessListEntryByUserID(accessEntries,
                                                accessEntryCount,
                                                userID);
    accessEntryCount = accessEntries ? 1 : 0;
  }

  int profileCount = 0;
  const ZISGenresProfileEntry *profiles = NULL;
  if (accessEntryCount > 0) {
    profiles = getProfilesByPrefixOrFail(response,
                                         zisServerName,
                                         profile,
                                         &profileCount,
                                         traceLevel);
    if (profiles == NULL) {
      return 0;
    }
  }

  respondWithAccessInfo(response, zisServerName,
                        accessEntries, accessEntryCount,
                        profiles, profileCount,
                        isTeamFormat,
                        traceLevel);

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);
  return 0;
}

void installSecurityManagementServices(HttpServer *server) {

  logConfigureComponent(NULL, LOG_COMP_ID_SECURITY, "security-mgmt",
                        LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s begin\n", __FUNCTION__);

  HttpService *classMgmtService =
      makeGeneratedService("class management service",
                           "/security-mgmt/classes/**");
  classMgmtService->paramSpecList =
      makeIntParamSpec("count", SERVICE_ARG_OPTIONAL, 0, 0, 0, 0,
      makeIntParamSpec("traceLevel", SERVICE_ARG_OPTIONAL, 0, 0, 0, 0,
      makeStringParamSpec("dryRun", SERVICE_ARG_OPTIONAL, NULL
      )));
  classMgmtService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  classMgmtService->serviceFunction = &serveClassManagement;
  classMgmtService->runInSubtask = TRUE;
  classMgmtService->doImpersonation = TRUE;
  registerHttpService(server, classMgmtService);

  HttpService *userProfileService =
      makeGeneratedService("user profile service",
                           "/security-mgmt/user-profiles/**");
  userProfileService->paramSpecList =
      makeIntParamSpec("count", SERVICE_ARG_OPTIONAL, 0, 0, 0, 0,
      makeIntParamSpec("traceLevel", SERVICE_ARG_OPTIONAL, 0, 0, 0, 0,
      makeStringParamSpec("dryRun", SERVICE_ARG_OPTIONAL, NULL
      )));
  userProfileService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  userProfileService->serviceFunction = &serveUserProfile;
  userProfileService->runInSubtask = TRUE;
  userProfileService->doImpersonation = TRUE;
  registerHttpService(server, userProfileService);

  HttpService *groupMgmtService =
      makeGeneratedService("group management service",
                           "/security-mgmt/groups/**");
  groupMgmtService->paramSpecList =
      makeIntParamSpec("count", SERVICE_ARG_OPTIONAL, 0, 0, 0, 0,
      makeIntParamSpec("traceLevel", SERVICE_ARG_OPTIONAL, 0, 0, 0, 0,
      makeStringParamSpec("dryRun", SERVICE_ARG_OPTIONAL, NULL
      )));
  groupMgmtService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  groupMgmtService->serviceFunction = &serveGroupManagement;
  groupMgmtService->runInSubtask = TRUE;
  groupMgmtService->doImpersonation = TRUE;
  registerHttpService(server, groupMgmtService);

  HttpService *accessService =
      makeGeneratedService("access info service",
                           "/security-mgmt/access/**");
  accessService->paramSpecList =
      makeIntParamSpec("count", SERVICE_ARG_OPTIONAL, 0, 0, 0, 0,
      makeIntParamSpec("traceLevel", SERVICE_ARG_OPTIONAL, 0, 0, 0, 0, NULL
      ));
  accessService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  accessService->serviceFunction = &serveAccessInfo;
  accessService->runInSubtask = TRUE;
  accessService->doImpersonation = TRUE;
  registerHttpService(server, accessService);

  zowelog(NULL, LOG_COMP_ID_SECURITY, ZOWE_LOG_DEBUG2, "%s end\n", __FUNCTION__);
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

