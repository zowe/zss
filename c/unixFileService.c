

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "pthread.h"

#include "zowetypes.h"
#include "utils.h"
#include "json.h"
#include "bpxnet.h"
#include "socketmgmt.h"
#include "charsets.h"
#include "unixfile.h"
#include "httpfileservice.h"
#include "collections.h"
#include "unixFileService.h"
#include "zssLogging.h"
#include "httpserver.h"

/* Time it takes in seconds for a session to be
 * removed from the hashtable due to
 * inactivity.
 */
#define TIMEOUT_TIME 600

/* An enumeration of the possible
 * file transfer types supported
 * in the service.
 */
enum TransferType {NONE, BINARY, TEXT};

/* Stores data for an
 * UploadSession.
 */
typedef struct UploadSession_tag {
  UnixFile *file;
  unsigned int timeOfLastRequest;
  unsigned int sessionID;
  int targetCCSID;
  int sourceCCSID;
  enum TransferType tType;
  char userName[9];
} UploadSession;

/* Stores all active UploadSessions
 * on the server.
 */
typedef struct UploadSessionTracker_tag {
  hashtable *sessionsByFileID;
  hashtable *fileIDsBySessionID;
  pthread_mutex_t fileHashLock;
  unsigned int sessionCounter;
} UploadSessionTracker;

/* A unique file identifier created
 * using an iNode and deviceID.
 */
typedef struct FileID_tag {
  int iNode;
  int deviceID;
} FileID;

static int fileIDHasher(void *key);
static int compareFileID(void *key1, void *key2);
static int timeOutMatcher(void *userData, void *key, void *value);
static void timeOutDestroyer(void *userData, void *value);
static void removeSessionTimeOuts(UploadSessionTracker *tracker, void *currUnixTimePtr);
static int cleanupSession(UploadSessionTracker *tracker, FileID *fileID, int sessionID);

static void addToTable(hashtable *table, void *key, void *value, pthread_mutex_t *lock);
static void addToTableUInt(hashtable *table, unsigned int key, void *value, pthread_mutex_t *lock);
static void *getFromTable(hashtable *table, void *key, pthread_mutex_t *lock);
static void *getFromTableUInt(hashtable *table, unsigned int key, pthread_mutex_t *lock);
static void removeFromTable(hashtable *table, void *key, pthread_mutex_t *lock);
static void removeFromTableUInt(hashtable *table, unsigned int key, pthread_mutex_t *lock);
static int checkForDuplicatesInTable(HttpResponse *response, hashtable *table, void *key, pthread_mutex_t *lock);

static int parseForceOverwriteParameter(HttpRequest *request);
static int parseEncodingParameter(HttpResponse *response, int *outSourceCCSID, int *outTargetCCSID,
                                 enum TransferType *outTransferType);
static void getEncodingInfoFromQueryParameters(char *inSourceEncoding, char *inTargetEncoding, int *outSourceCCSID,
                                               int *outTargetCCSID, enum TransferType *outTransferType);
static int parseLastChunkQueryParameter(HttpResponse *response, int *lastChunkValue);

static void respondWithSessionID(HttpResponse *response, int sessionID);
static void assignSessionIDToCaller(UploadSessionTracker *tracker, HttpResponse *response, char *encodedRouteFileName,
                                    unsigned int currUnixTime);
static void addSessionIDToTableAndIncrementCounter(UploadSessionTracker *tracker, FileID *fileID,
                                                   UploadSession *session);
static int validateSessionWithUserName(char *requestUserName, char *tableUserName);

static void doChunking(UploadSessionTracker *tracker, HttpResponse *response, char *encodedRouteFileName, char *id,
                       unsigned int currUnixTime);

static int checkForOverwritePermission(HttpResponse *response, int fileExists, int forceValue);
static int handleNewFileCase(HttpResponse *response, char *encodedFileName, int *iNode, int *deviceID);
static int checkIfFileIsBusy(HttpResponse *response, char *encodedFileName, UnixFile **file);
static int tagFileForUploading(HttpResponse *response, char *encodedFileName, int targetCCSID);

static int fileIDHasher(void *key) {
  FileID *fid = key;

  int a = fid->iNode;
  int b = fid->deviceID;
  int res = 0;

  a = a<<3;
  res = a^b;

  return res;
}

static int compareFileID(void *key1, void *key2) {
  FileID *fid1 = key1;
  FileID *fid2 = key2;

  if (fid1->iNode == fid2->iNode && fid1->deviceID == fid2->deviceID) {
    return TRUE;
  }

  return FALSE;
}

static void freeKeySessionsByFileID(void *key) {
  FileID *fid = key;
  safeFree((char*)fid, sizeof(FileID));
}

static void freeValueSessionsByFileID(void *value) {
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;

  UploadSession *s = value;
  status = fileClose(s->file, &returnCode, &reasonCode);
  if (status == -1) {
    zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_WARNING, ZSS_LOG_UNABLE_MSG,
          "close", returnCode, reasonCode);
  }
  safeFree((char*)s, sizeof(UploadSession));
}

static int timeOutMatcher(void *userData, void *key, void *value) {
  UploadSession *s = value;
  unsigned int currTime = *(unsigned int *)userData;
  if ((currTime - s->timeOfLastRequest) >= TIMEOUT_TIME) {
    return TRUE;
  }
  return FALSE;
}

static void timeOutDestroyer(void *userData, void *value) {
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;

  UploadSession *s = value;
  status = fileClose(s->file, &returnCode, &reasonCode);
  if (status == -1) {
    zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_WARNING, ZSS_LOG_UNABLE_MSG,
          "close", returnCode, reasonCode);
  }
  safeFree((char*)s, sizeof(UploadSession));
}

static void respondWithSessionID(HttpResponse *response, int sessionID) {
  jsonPrinter *out = respondWithJsonPrinter(response);

  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);

  jsonStart(out);
  jsonAddString(out, "msg", "Please attach the sessionID to subsequent requests");
  jsonAddUInt(out, "sessionID", sessionID);
  jsonEnd(out);

  finishResponse(response);
}

static int parseForceOverwriteParameter(HttpRequest *request) {
  char *forceVal = getQueryParam(request, "forceOverwrite");
  if (!strcmp(strupcase(forceVal), "TRUE")) {
    return TRUE;
  }

  return FALSE;
}

static int checkForOverwritePermission(HttpResponse *response, int fileExists, int forceValue) {
  if (forceValue == FALSE && fileExists == TRUE) {
    respondWithJsonError(response, "File already exists. Please choose a different file name or attach "
        "the forceOverwrite query parameter to your next request.", 200, "OK");
    return -1;
  }

  return 0;
}

static void getEncodingInfoFromQueryParameters(char *inSourceEncoding, char *inTargetEncoding, int *outSourceCCSID,
                                       int *outTargetCCSID, enum TransferType *outTransferType) {

  if (!strcmp(strupcase(inSourceEncoding), "BINARY") && !strcmp(strupcase(inTargetEncoding), "BINARY")) {
    *outSourceCCSID = CCSID_BINARY;
    *outTargetCCSID = CCSID_BINARY;
    *outTransferType = BINARY;
  }
  else {
    *outSourceCCSID = getCharsetCode(inSourceEncoding);
    *outTargetCCSID = getCharsetCode(inTargetEncoding);
    *outTransferType = TEXT;
  }
}

static int parseEncodingParameter(HttpResponse *response, int *outSourceCCSID, int *outTargetCCSID,
                                  enum TransferType *outTransferType) {
  char *sourceEncoding = getQueryParam(response->request, "sourceEncoding");
  char *targetEncoding = getQueryParam(response->request, "targetEncoding");
  if (sourceEncoding == NULL || targetEncoding == NULL) {
    respondWithJsonError(response, "Source encoding and/or target encoding are missing.", 400, "Bad Request");
    return -1;
  }

  getEncodingInfoFromQueryParameters(sourceEncoding, targetEncoding, outSourceCCSID, outTargetCCSID, outTransferType);
  if ((*outSourceCCSID == -1 || *outTargetCCSID == -1) && (*outTransferType == TEXT)) {
    *outTransferType = NONE;
    respondWithJsonError(response, "Unsupported encodings requested.", 400, "Bad Request");
    return -1;
  }

  return 0;
}

static int handleNewFileCase(HttpResponse *response, char *encodedFileName, int *iNode, int *deviceID) {
  int returnCode = 0;
  int reasonCode = 0;
  char *fileName = cleanURLParamValue(response->slh, encodedFileName);
  UnixFile *newFile = fileOpen(fileName,
                               FILE_OPTION_CREATE | FILE_OPTION_READ_ONLY,
                               0700,
                               0,
                               &returnCode,
                               &reasonCode);

  if (newFile == NULL) {
    zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_WARNING, ZSS_LOG_UNABLE_MSG,
           "create", returnCode, reasonCode);
    respondWithJsonError(response, "Could not create new file.", 500, "Internal Server Error");
    return -1;
  }

  int status = fileClose(newFile, &returnCode, &reasonCode);
  if (status != 0) {
    zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_WARNING, ZSS_LOG_UNABLE_MSG,
           "close", returnCode, reasonCode);
    respondWithJsonError(response, "Could not close file.", 500, "Internal Server Error");
    return -1;
  }

  FileInfo info;
  status = fileInfo(fileName, &info, &returnCode, &reasonCode);
  if (status != 0) {
    zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_WARNING, ZSS_LOG_UNABLE_METADATA_MSG,
           returnCode, reasonCode);
    respondWithJsonError(response, "Could not get metadata for file.", 500, "Internal Server Error");
    return -1;
  }

  *iNode = info.inode;
  *deviceID = info.deviceID;

  return 0;
}

static int checkForDuplicatesInTable(HttpResponse *response, hashtable *table, void *key, pthread_mutex_t *lock) {
  pthread_mutex_lock(lock);
  {
    if (htGet(table, key) != NULL) {
    pthread_mutex_unlock(lock);
    respondWithJsonError(response, "Duplicate in table. Requested resource is busy. Please try again later.",
                        403, "Forbidden");
    return -1;
    }
  }
  pthread_mutex_unlock(lock);

  return 0;
}

static void addToTable(hashtable *table, void *key, void *value, pthread_mutex_t *lock) {
  pthread_mutex_lock(lock);
  {
    htPut(table, key, value);
  }
  pthread_mutex_unlock(lock);
}

static void addToTableUInt(hashtable *table, unsigned int key, void *value, pthread_mutex_t *lock) {
  pthread_mutex_lock(lock);
  {
    htUIntPut(table, key, value);
  }
  pthread_mutex_unlock(lock);
}

static void *getFromTable(hashtable *table, void *key, pthread_mutex_t *lock) {
  void *returnValue;

  pthread_mutex_lock(lock);
  {
    returnValue = htGet(table, key);
  }
  pthread_mutex_unlock(lock);

  return returnValue;
}

static void *getFromTableUInt(hashtable *table, unsigned int key, pthread_mutex_t *lock) {
  void *returnValue;

  pthread_mutex_lock(lock);
  {
    returnValue = htUIntGet(table, key);
  }
  pthread_mutex_unlock(lock);

  return returnValue;
}

static void removeFromTable(hashtable *table, void *key, pthread_mutex_t *lock) {
  pthread_mutex_lock(lock);
  {
    htRemove(table, key);
  }
  pthread_mutex_unlock(lock);
}

static void removeFromTableUInt(hashtable *table, unsigned int key, pthread_mutex_t *lock) {
  pthread_mutex_lock(lock);
  {
    htRemove(table, (void *)key);
  }
  pthread_mutex_unlock(lock);
}

static int checkIfFileIsBusy(HttpResponse *response, char *encodedFileName, UnixFile **file) {
  int returnCode = 0;
  int reasonCode = 0;
  char *fileName = cleanURLParamValue(response->slh, encodedFileName);
  *file = fileOpen(fileName,
                  FILE_OPTION_TRUNCATE | FILE_OPTION_WRITE_ONLY,
                  0,
                  0,
                  &returnCode,
                  &reasonCode);

  if (*file == NULL) {
    zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_WARNING, ZSS_LOG_UNABLE_MSG,
          "open", returnCode, reasonCode);
    respondWithJsonError(response, "Could not open file. Requested resource is busy. Please try again later.",
          403, "Forbidden");
    return -1;
  }

  return 0;
}

static int tagFileForUploading(HttpResponse *response, char *encodedFileName, int targetCCSID) {
  int returnCode = 0;
  int reasonCode = 0;
  char *fileName = cleanURLParamValue(response->slh, encodedFileName);
  int status = fileChangeTag(fileName, &returnCode, &reasonCode, targetCCSID);
  if (status == -1) {
    zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_WARNING, "Could not tag file. Ret: %d, Res: %d\n", returnCode, reasonCode);
    respondWithJsonError(response, "Could not tag file.", 500, "Internal Server Error");
    return -1;
  }

  return 0;
}

static void addSessionIDToTableAndIncrementCounter(UploadSessionTracker *tracker, FileID *fileID,
                                                   UploadSession *session) {
  pthread_mutex_lock(&tracker->fileHashLock);
  {
    htUIntPut(tracker->fileIDsBySessionID, tracker->sessionCounter, fileID);
    session->sessionID = tracker->sessionCounter;
    tracker->sessionCounter++;
  }
  pthread_mutex_unlock(&tracker->fileHashLock);
}

static void removeSessionTimeOuts(UploadSessionTracker *tracker, void *currUnixTimePtr) {
  hashtable *sessionsByFileID = tracker->sessionsByFileID;

  pthread_mutex_lock(&tracker->fileHashLock);
  {
    htPrune(sessionsByFileID, timeOutMatcher, timeOutDestroyer, currUnixTimePtr);
  }
  pthread_mutex_unlock(&tracker->fileHashLock);
}

static void assignSessionIDToCaller(UploadSessionTracker *tracker, HttpResponse *response, char *encodedRouteFileName,
                                    unsigned int currUnixTime) {
  int reasonCode = 0;
  int returnCode = 0;
  int status = 0;
  char *routeFileName = cleanURLParamValue(response->slh, encodedRouteFileName);
  hashtable *sessionsByFileID = tracker->sessionsByFileID;
  hashtable *fileIDsBySessionID = tracker->fileIDsBySessionID;

  /* If the file already exists in USS, then we should
   * only overwrite the file if the caller wants
   * to do so. If the file exist and this query parameter
   * is not present, then a sessionID will NOT be returned
   * and the user will be prompted to pick a different file name
   * or attach the forceOverwrite query parameter.
   */
  int forceValue = parseForceOverwriteParameter(response->request);

  /* The transferType and the encodings should be decided
   * before a sessionID has been assigned.
   */
  int sourceCCSID = 0;
  int targetCCSID = 0;
  enum TransferType transferType = NONE;
  status = parseEncodingParameter(response, &sourceCCSID, &targetCCSID, &transferType);
  if (status == -1) {
    return;
  }

  FileInfo info;
  int fileExists = FALSE;
  status = fileInfo(routeFileName, &info, &returnCode, &reasonCode);
  if (status == 0) {
    fileExists = TRUE;
  }

  status = checkForOverwritePermission(response, fileExists, forceValue);
  if (status == -1) {
    return;
  }

  /* Because our key is going to be a hash of a file's iNode
   * and deviceID, the file must exist in order to get those
   * values. If is doesn't exist, then we must create it in
   * order to capture those values.
   */
  FileID fileID = {0};

  if (fileExists == FALSE) {
    int iNode, deviceID;
    status = handleNewFileCase(response, routeFileName, &iNode, &deviceID);
    if (status == -1) {
      return;
    }
    else {
      fileID.iNode = iNode;
      fileID.deviceID = deviceID;
    }
  }
  else {
    fileID.iNode = info.inode;
    fileID.deviceID = info.deviceID;
  }

  /* Because the server does work on behalf of the caller,
   * the processID accessing files will be the same. As a result,
   * we can't rely on fileOpen to fail to notify the caller that
   * a file is currently busy. The solution to this is to store
   * active files in a HashTable. Here, we are searching the HashTable
   * to see if the requested file is currently active in an UploadSession.
   * If it's not active, then we can go ahead and add it to the HashTable.
   */
  status = checkForDuplicatesInTable(response, sessionsByFileID, &fileID, &tracker->fileHashLock);
  if (status == -1) {
    return;
  }

  UploadSession *currentSession = (UploadSession*)safeMalloc(sizeof(UploadSession), "UploadSession");
  currentSession->sourceCCSID = sourceCCSID;
  currentSession->targetCCSID = targetCCSID;
  currentSession->tType = transferType;
  currentSession->timeOfLastRequest = currUnixTime;

  FileID *currentFileID = (FileID*)safeMalloc(sizeof(FileID), "FileID");
  currentFileID->iNode = fileID.iNode;
  currentFileID->deviceID = fileID.deviceID;

  addToTable(sessionsByFileID, currentFileID, currentSession, &tracker->fileHashLock);

  /* Now we must open the file that we will be uploading. We do this now
   * as an indicator of whether or not the file is currently busy. If it
   * is busy on behalf of another instance of ZSS, then this WILL fail. In
   * the case of failure, then we must remove the entry from the HashTable.
   */
  UnixFile *file = NULL;
  status = checkIfFileIsBusy(response, routeFileName, &file);
  if (status == -1) {
    removeFromTable(sessionsByFileID, currentFileID, &tracker->fileHashLock);
    return;
  }

  status = tagFileForUploading(response, routeFileName, targetCCSID);
  if (status == -1) {
    removeFromTable(sessionsByFileID, currentFileID, &tracker->fileHashLock);
    return;
  }

  currentSession->file = file;

  /* The following scheme is used to derive session identifier:
   *
   * 1. A counter is incremented each time a new session is added;
   * the value of this counter is used in the hashtable as the
   * session id.
   *
   * 2. A username is associated with each session and is validated
   * before every file operation. This username is sent as a cookie
   * and is handled in httpserver.c itself.
   *
   * With this scheme, there is no need for random number generation
   * and security is guarenteed since only those who have went through
   * the login process can access this API, as it is required.
   *
   * See: NATIVE_AUTH_WITH_SESSION_TOKEN
   *
   * AddSessionIDToTableAndIncrementCounter handles all of the above
   * using mutex in order for synchronous code.
   */
  addSessionIDToTableAndIncrementCounter(tracker, currentFileID, currentSession);

  memset(currentSession->userName, 0, sizeof(currentSession->userName));
  strncpy(&currentSession->userName[0], response->request->username, sizeof (currentSession->userName));

  respondWithSessionID(response, currentSession->sessionID);
}

static int parseLastChunkQueryParameter(HttpResponse *response, int *lastChunkValue) {
  char *lastChunk = getQueryParam(response->request, "lastChunk");
  if (lastChunk == NULL) {
    respondWithJsonError(response, "Last chunk is missing.", 400, "Bad Request");
    return -1;
  }

  if (!strcmp(strupcase(lastChunk), "TRUE")) {
    *lastChunkValue = TRUE;
  }
  else {
    *lastChunkValue = FALSE;
  }

  return 0;
}

static int cleanupSession(UploadSessionTracker *tracker, FileID *fileID, int sessionID) {
  removeFromTable(tracker->sessionsByFileID, fileID, &tracker->fileHashLock);
  removeFromTableUInt(tracker->fileIDsBySessionID, sessionID, &tracker->fileHashLock);
}

static int validateSessionWithUserName(char *requestUserName, char *tableUserName) {
  if (!strcmp(requestUserName, tableUserName)) {
    return TRUE;
  }

  return FALSE;
}

static void doChunking(UploadSessionTracker *tracker, HttpResponse *response, char *encodedRouteFileName, char *id,
                       unsigned int currUnixTime) {
  hashtable *sessionsByFileID = tracker->sessionsByFileID;
  hashtable *fileIDsBySessionID = tracker->fileIDsBySessionID;
  char *routefileName = cleanURLParamValue(response->slh, encodedRouteFileName);
  unsigned int currentSessionID = strtoul(id, NULL, 10);

  FileID *currentFileID = getFromTableUInt(fileIDsBySessionID, currentSessionID, &tracker->fileHashLock);
  UploadSession *currentSession = getFromTable(sessionsByFileID, currentFileID, &tracker->fileHashLock);

  /* If the sessionID is valid, then we then check
   * to make sure their userName is correctly
   * associated with the session id.
   */
  if (currentSession == NULL) {
    respondWithJsonError(response, "Session identifier is invalid.", 403, "Forbidden");
    return;
  }

  /* Update time of last request */
  currentSession->timeOfLastRequest = currUnixTime;

  /* If the userName is validated, then the caller can begin
   * to upload their file.
   */
  int validated = validateSessionWithUserName(response->request->username, currentSession->userName);
  if (!validated) {
    respondWithJsonError(response, "User is not associated with the provided session identifier.", 400, "Bad Request");
    return;
  }

  /* The lastChunk query parameter has to be present
   * as it indicates whether the file has been uploaded
   * in full.
   */
  else {
    int lastChunk = FALSE;
    int status = parseLastChunkQueryParameter(response, &lastChunk);
    if (status == -1) {
      return;
    }

    /* Write to the file in TEXT mode. The
     * text is converted to the correct
     * CCSID before being written to the
     * file in USS.
     */
    if (currentSession->tType == TEXT) {
      status = 0;
      //Only write to file if string isn't empty. If string is empty, case is handled by creation of new file.
      if (response->request->contentLength > 0) {
        status = writeAsciiDataFromBase64(currentSession->file, response->request->contentBody,
                                        response->request->contentLength, currentSession->sourceCCSID,
                                        currentSession->targetCCSID);
      }
    }

    /* Write to the file in BINARY mode. The bytes
     * are written to the file in USS as is.
     */
    else if (currentSession->tType == BINARY) {
      status = 0;
      //Only write to file if string isn't empty. If string is empty, case is handled by creation of new file.
      if (response->request->contentLength > 0) {
        status = writeBinaryDataFromBase64(currentSession->file, response->request->contentBody,
                                         response->request->contentLength);
      }
    }

    /* This shouldn't happen, unless tType was somehow
     * set to NONE, or never changed to begin with. It's
     * in the realm of possibilies but not very likely,
     * unless this code is modified in the future and a
     * mistake was made.
     */
    else {
      zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_WARNING, ZSS_LOG_TTYPE_NOT_SET_MSG);
      status = -1;
    }

    if (status == 0 && lastChunk == FALSE) {
      response200WithMessage(response, "Successfully wrote chunk to file.");
    }
    else if (status == 0 && lastChunk == TRUE) {
     response200WithMessage(response, "Successfully wrote file.");
     cleanupSession(tracker, currentFileID, currentSessionID);
    }
    else {
      respondWithJsonError(response, "Failed to write chunk to file.", 500, "Internal Server Error");
      cleanupSession(tracker, currentFileID, currentSessionID);
    }
  }
}

static int serveUnixFileContents(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  char *routeFileFrag = stringListPrint(request->parsedFile, 2, 1000, "/", 0);
  char *encodedRouteFileName = stringConcatenate(response->slh, "/", routeFileFrag);
  char *routeFileName = cleanURLParamValue(response->slh, encodedRouteFileName);
  unsigned int currUnixTime = (unsigned)time(NULL);

  if (!strcmp(request->method, methodPUT)) {
    UploadSessionTracker *tracker = service->userPointer;

    removeSessionTimeOuts(tracker, &currUnixTime);

    /* Attempt to find the sessionID query parameter
     * attached to the current request. If it is present,
     * we check it's validity before allowing the caller
     * to start writing to a file. If it's not present,
     * then we attempt to assign one to the caller. If successful,
     * then we return it back in the response as a JSON object.
     */
    char *id = getQueryParam(response->request, "sessionID");

    if (id == NULL) {
      assignSessionIDToCaller(tracker, response, routeFileName, currUnixTime);
    }
    else {
      doChunking(tracker, response, routeFileName, id, currUnixTime);
    }
  }
  else if (!strcmp(request->method, methodGET)) {
    respondWithUnixFileContentsWithAutocvtMode(NULL, response, routeFileName, TRUE, 0);
  }
  else if (!strcmp(request->method, methodDELETE)) {
    if (doesFileExist(routeFileName) == true) {
      if (isDir(routeFileName) == true) {
        deleteUnixDirectoryAndRespond(response, routeFileName);
      }
      else {
        deleteUnixFileAndRespond(response, routeFileName);
      }
    }
    else {
      respondWithUnixFileNotFound(response, 1);
    }
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setResponseStatus(response, 405, "Method Not Allowed");
    setDefaultJSONRESTHeaders(response);
    addStringHeader(response, "Allow", "GET, PUT, DELETE");
    writeHeader(response);

    jsonStart(out);
    jsonEnd(out);

    finishResponse(response);

    return 0;
  }

  return 0;
}

static int serveUnixFileCopy(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  char *routeFileFrag = stringListPrint(request->parsedFile, 2, 1000, "/", 0);
  char *encodedRouteFileName = stringConcatenate(response->slh, "/", routeFileFrag);
  char *routeFileName = cleanURLParamValue(response->slh, encodedRouteFileName);
  char *newName = getQueryParam(response->request, "newName");
  if (!newName) {
    respondWithJsonError(response, "newName query parameter is missing", 400, "Bad Request");
    return 0;
  }

  char *forceVal = getQueryParam(response->request, "forceOverwrite");
  int force = FALSE;

  if (!strcmp(strupcase(forceVal), "TRUE")) {
    force = TRUE;
  }

  if (!strcmp(request->method, methodPOST)) {
    if (doesFileExist(routeFileName) == true) {
      if (isDir(routeFileName) == true) {
        copyUnixDirectoryAndRespond(response, routeFileName, newName, force);
      }
      else {
        copyUnixFileAndRespond(response, routeFileName, newName, force);
      }
    }
    else {
      respondWithUnixFileNotFound(response, 1);
    }
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setResponseStatus(response, 405, "Method Not Allowed");
    setDefaultJSONRESTHeaders(response);
    addStringHeader(response, "Allow", "POST");
    writeHeader(response);

    jsonStart(out);
    jsonEnd(out);

    finishResponse(response);
  }

  return 0;
}

static int serveUnixFileRename(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  char *routeFileFrag = stringListPrint(request->parsedFile, 2, 1000, "/", 0);
  char *encodedRouteFileName = stringConcatenate(response->slh, "/", routeFileFrag);
  char *routeFileName = cleanURLParamValue(response->slh, encodedRouteFileName);
  char *newName = getQueryParam(response->request, "newName");
  if (!newName) {
    respondWithJsonError(response, "newName query parameter is missing", 400, "Bad Request");
    return 0;
  }

  char *forceVal = getQueryParam(response->request, "forceOverwrite");
  int force = FALSE;

  if (!strcmp(strupcase(forceVal), "TRUE")) {
    force = TRUE;
  }

  if (!strcmp(request->method, methodPOST)) {
    if (doesFileExist(routeFileName) == true) {
      if (isDir(routeFileName) == true) {
        renameUnixDirectoryAndRespond(response, routeFileName, newName, force);
      }
      else {
        renameUnixFileAndRespond(response, routeFileName, newName, force);
      }
    }
    else {
      respondWithUnixFileNotFound(response, 1);
    }
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setResponseStatus(response, 405, "Method Not Allowed");
    setDefaultJSONRESTHeaders(response);
    addStringHeader(response, "Allow", "POST");
    writeHeader(response);

    jsonStart(out);
    jsonEnd(out);

    finishResponse(response);
  }

  return 0;
}

static int serveUnixFileMakeDirectory(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  char *routeFileFrag = stringListPrint(request->parsedFile, 2, 1000, "/", 0);
  char *encodedRouteFileName = stringConcatenate(response->slh, "/", routeFileFrag);
  char *routeFileName = cleanURLParamValue(response->slh, encodedRouteFileName);

  char *forceVal  = getQueryParam(response->request,  "forceOverwrite");
  char *recursive = getQueryParam(response->request, "recursive");
  int force = FALSE, recurse = FALSE;

  if (!strcmp(strupcase(forceVal), "TRUE")) {
    force = TRUE;
  }
  if (!strcmp(strupcase(recursive), "TRUE")) {
    recurse = TRUE;
  }

  if (!strcmp(request->method, methodPOST)) {
    createUnixDirectoryAndRespond(response, routeFileName, recurse, force);
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setResponseStatus(response, 405, "Method Not Allowed");
    setDefaultJSONRESTHeaders(response);
    addStringHeader(response, "Allow", "POST");
    writeHeader(response);

    jsonStart(out);
    jsonEnd(out);

    finishResponse(response);
  }

  return 0;
}

static int serveUnixFileTouch(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  char *routeFileFrag = stringListPrint(request->parsedFile, 2, 1000, "/", 0);
  char *encodedRouteFileName = stringConcatenate(response->slh, "/", routeFileFrag);
  char *routeFileName = cleanURLParamValue(response->slh, encodedRouteFileName);
  char *forceVal = getQueryParam(response->request, "forceOverwrite");
  int force = FALSE;

  if (!strcmp(strupcase(forceVal), "TRUE")) {
    force = TRUE;
  }

  if (!strcmp(request->method, methodPOST)) {
    writeEmptyUnixFileAndRespond(response, routeFileName, force);
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setResponseStatus(response, 405, "Method Not Allowed");
    setDefaultJSONRESTHeaders(response);
    addStringHeader(response, "Allow", "POST");
    writeHeader(response);

    jsonStart(out);
    jsonEnd(out);

    finishResponse(response);
  }
}

static int serveUnixFileChangeMode(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  char *routeFileFrag = stringListPrint(request->parsedFile, 2, 1000, "/", 0);
  char *encodedRouteFileName = stringConcatenate(response->slh, "/", routeFileFrag);
  char *routeFileName = cleanURLParamValue(response->slh, encodedRouteFileName);
 
  char *recursive = getQueryParam(response->request, "recursive");
  char *mode = getQueryParam(response->request, "mode");
  char *pattern = getQueryParam(response->request, "pattern");

  if (!strcmp(request->method, methodPOST)) {
    directoryChangeModeAndRespond (response, routeFileName, 
          recursive, mode, pattern );
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setResponseStatus(response, 405, "Method Not Allowed");
    setDefaultJSONRESTHeaders(response);
    addStringHeader(response, "Allow", "POST");
    writeHeader(response);

    jsonStart(out);
    jsonEnd(out);

    finishResponse(response);
  }

  return 0;
}

static int serveUnixFileMetadata(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  char *fileFrag = stringListPrint(request->parsedFile, 2, 1000, "/", 0);
  char *fileName = stringConcatenate(response->slh, "/", fileFrag);

  if (!strcmp(request->method, methodGET)) {
    respondWithUnixFileMetadata(response, fileName);
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setResponseStatus(response, 405, "Method Not Allowed");
    setDefaultJSONRESTHeaders(response);
    addStringHeader(response, "Allow", "GET");
    writeHeader(response);

    jsonStart(out);
    jsonEnd(out);

    finishResponse(response);
  }

  return 0;
}

static int serveUnixFileChangeOwner (HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  char *routeFileFrag = stringListPrint(request->parsedFile, 2, 1000, "/", 0);
  char *encodedRouteFileName = stringConcatenate(response->slh, "/", routeFileFrag);
  char *routeFileName = cleanURLParamValue(response->slh, encodedRouteFileName);

  char *userId    = getQueryParam(response->request, "user");
  char *groupId   = getQueryParam(response->request, "group");
  char *pattern   = getQueryParam(response->request, "pattern");
  char *recursive = getQueryParam(response->request, "recursive");

  if (!strcmp(request->method, methodPOST)) {
    directoryChangeOwnerAndRespond (response, routeFileName,
                          userId, groupId, recursive, pattern);
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setResponseStatus(response, 405, "Method Not Allowed");
    setDefaultJSONRESTHeaders(response);
    addStringHeader(response, "Allow", "POST");
    writeHeader(response);
    finishResponse(response);
  }
  return 0;
}

static int serveUnixFileChangeTag(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  char *routeFileFrag = stringListPrint(request->parsedFile, 2, 1000, "/", 0);

  if (routeFileFrag == NULL) {
   respondWithJsonError(response, "Allocation error", HTTP_STATUS_INTERNAL_SERVER_ERROR, "Allocation error");
   return 0;
  }

  if (strlen(routeFileFrag) == 0) {
   respondWithJsonError(response, "Required absolute path of the resource is not provided",
        HTTP_STATUS_BAD_REQUEST, "Bad Request");
   return 0;
  }

  char *encodedRouteFileName = stringConcatenate(response->slh, "/", routeFileFrag);
  if (encodedRouteFileName == NULL) {
   respondWithJsonError(response, "Allocation error", HTTP_STATUS_INTERNAL_SERVER_ERROR, "Allocation error");
   return 0;
  }
  char *routeFileName = cleanURLParamValue(response->slh, encodedRouteFileName);
  if (routeFileName == NULL) {
   respondWithJsonError(response, "Allocation error", HTTP_STATUS_INTERNAL_SERVER_ERROR, "Allocation error");
   return 0;
  }

  char *codepage  = getQueryParam(response->request, "codeset");
  char *recursive = getQueryParam(response->request, "recursive");
  char *pattern   = getQueryParam(response->request, "pattern");
  char *type      = getQueryParam(response->request, "type");

  if (recursive == NULL || type == NULL) {
   respondWithJsonError(response, "Required parameter not provided", HTTP_STATUS_BAD_REQUEST, "Bad Request");
   return 0;
  }

  if (!strcmp(request->method, methodPOST)) {
    directoryChangeTagAndRespond(response, routeFileName, type, codepage, recursive, pattern);
  }
  else if (!strcmp(request->method, methodDELETE)) {
      char type[8] = {"delete"};
      directoryChangeDeleteTagAndRespond (response, routeFileName,
                                  type, codepage, recursive, pattern);
  }
  else {
    respondWithJsonError(response, "Method Not Allowed", HTTP_STATUS_METHOD_NOT_FOUND, "Bad Request");
    return 0;
  }
  return 0;
}


static int serveTableOfContents(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;

  if (!strcmp(request->method, methodGET)) {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setResponseStatus(response, 404, "Not Found");
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);

    jsonStart(out);
    jsonStartArray(out, "routes");

    jsonStartObject(out, NULL);
    jsonAddString(out, "contents", "/unixfile/contents/{absPath}");
    jsonEndObject(out);

    jsonStartObject(out, NULL);
    jsonAddString(out, "copy", "/unixfile/copy/{absPath}");
    jsonEndObject(out);

    jsonStartObject(out, NULL);
    jsonAddString(out, "rename", "/unixfile/rename/{absPath}");
    jsonEndObject(out);

    jsonStartObject(out, NULL);
    jsonAddString(out, "touch", "/unixfile/touch/{absPath}");
    jsonEndObject(out);

    jsonStartObject(out, NULL);
    jsonAddString(out, "mkdir", "/unixfile/mkdir/{absPath}");
    jsonEndObject(out);

    jsonStartObject(out, NULL);
    jsonAddString(out, "stat", "/unixfile/metadata/{absPath}");
    jsonEndObject(out);

    jsonStartObject(out, NULL);
    jsonAddString(out, "chmod", "/unixfile/chmod/{absPath}");
    jsonEndObject(out);

    jsonEndArray(out);
    jsonEnd(out);

    finishResponse(response);
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setResponseStatus(response, 405, "Method Not Allowed");
    setDefaultJSONRESTHeaders(response);
    addStringHeader(response, "Allow", "GET");
    writeHeader(response);

    jsonStart(out);
    jsonEnd(out);

    finishResponse(response);
  }

  return 0;
}

void installUnixFileContentsService(HttpServer *server) {
  HttpService *httpService = makeGeneratedService("UnixFileContents",
      "/unixfile/contents/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = serveUnixFileContents;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  registerHttpService(server, httpService);

  UploadSessionTracker *tracker = (UploadSessionTracker*)safeMalloc(sizeof(UploadSessionTracker),
                                  "UploadSessionTracker");
  tracker->sessionsByFileID = htCreate(17, fileIDHasher, compareFileID, freeKeySessionsByFileID,
                                       freeValueSessionsByFileID);
  tracker->fileIDsBySessionID = htCreate(17, NULL, NULL, NULL, NULL);
  pthread_mutex_init(&tracker->fileHashLock, NULL);
  tracker->sessionCounter = 1;
  httpService->userPointer = tracker;
}

void installUnixFileRenameService(HttpServer *server) {
  HttpService *httpService = makeGeneratedService("UnixFileRename",
      "/unixfile/rename/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = serveUnixFileRename;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  registerHttpService(server, httpService);
}

void installUnixFileCopyService(HttpServer *server) {
  HttpService *httpService = makeGeneratedService("UnixFileCopy",
      "/unixfile/copy/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = serveUnixFileCopy;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  registerHttpService(server, httpService);
}

void installUnixFileMakeDirectoryService(HttpServer *server) {
  HttpService *httpService = makeGeneratedService("UnixFileMkdir",
      "/unixfile/mkdir/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = serveUnixFileMakeDirectory;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  registerHttpService(server, httpService);
}

void installUnixFileTouchService(HttpServer *server) {
  HttpService *httpService = makeGeneratedService("UnixFileTouch",
      "/unixfile/touch/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = serveUnixFileTouch;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  registerHttpService(server, httpService);
}

void installUnixFileMetadataService(HttpServer *server) {
  HttpService *httpService = makeGeneratedService("unixFileMetadata",
      "/unixfile/metadata/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  httpService->serviceFunction = serveUnixFileMetadata;
  registerHttpService(server, httpService);
}

void installUnixFileChangeOwnerService(HttpServer *server) {
  HttpService *httpService = makeGeneratedService("unixFileMetadata",
      "/unixfile/chown/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  httpService->serviceFunction = serveUnixFileChangeOwner;
  registerHttpService(server, httpService);
}

void installUnixFileChangeTagService(HttpServer *server) {
  HttpService *httpService = makeGeneratedService("UnixFileChtag",
      "/unixfile/chtag/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = serveUnixFileChangeTag;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  registerHttpService(server, httpService);
}

void installUnixFileChangeModeService(HttpServer *server) {
  HttpService *httpService = makeGeneratedService("UnixFileChmod",
      "/unixfile/chmod/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = serveUnixFileChangeMode;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  registerHttpService(server, httpService);
}

void installUnixFileTableOfContentsService(HttpServer *server) {
  HttpService *httpService = makeGeneratedService("unixFileMetadata",
      "/unixfile/");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  httpService->serviceFunction = serveTableOfContents;
  registerHttpService(server, httpService);
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
