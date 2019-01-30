

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

#define TIMEOUT_TIME 600

/* Stores data for an
 * UploadSession.
 */
typedef struct UploadSession_tag {
  UnixFile *file;
  unsigned int timeOfLastRequest;
  int sessionID;
} UploadSession;

/* Stores all active UploadSessions
 * on the server.
 */
typedef struct UploadSessionTracker_tag {
  hashtable *sessions;
  hashtable *ids;
  pthread_mutex_t fileHashLock;
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
static void respondWithSessionID(HttpResponse *response, int sessionID);
static void assignSessionIDToCaller(UploadSessionTracker *tracker, HttpResponse *response, char *routeFileName, unsigned int currUnixTime);
static void removeSessionTimeOuts(UploadSessionTracker *tracker, void *currUnixTimePtr);
static void doChunking(UploadSessionTracker *tracker, HttpResponse *response, char *routeFileName, char *id);

/* Hashing function for the SessionsByFileID,
 *
 * - Shift 3 bits to the left and ^.
 */
static int fileIDHasher(void *key) {
  FileID *fid = key;

  int a = fid->iNode;
  int b = fid->deviceID;
  int res = 0;

  a = a<<3;
  res = a^b;

  return res;
}

/* Comparator function for SessionsByFileID,
 *
 * - Compares members of the FileID struct.
 */
static int compareFileID(void *key1, void *key2) {
  FileID *fid1 = key1;
  FileID *fid2 = key2;

  if (fid1->iNode == fid2->iNode && fid1->deviceID == fid2->deviceID) {
    return TRUE;
  }

  return FALSE;
}

/* Key Reclaimer for SessionsByFileID
 *
 * - Free's the FileID struct
 *
 * NOTE: This means that fileIDsBySessionID does
 * NOT need a value reclaimer.
 */
static void freeKeySessionsByFileID(void *key) {
  FileID *fid = key;
  safeFree((char*)fid, sizeof(FileID));
}

/* Value Reclaimer for SessionsByFileID
 *
 * - Free's the UploadSession struct
 * - Closes the active file
 */
static void freeValueSessionsByFileID(void *value) {
  int returnCode = 0;
  int reasonCode = 0;

  UploadSession *s = value;
  fileClose(s->file, &returnCode, &reasonCode);
  safeFree((char*)s, sizeof(UploadSession));
}

/* Comparator function for htPrune
 *
 * - Compares the timeOfLastRequest for
 * an UploadSession to the current time
 * in order to detect any sessions that
 * need to be timed out.
 */
static int timeOutMatcher(void *userData, void *key, void *value) {
  UploadSession *s = value;
  unsigned int *currTime = userData;
  if ((*currTime - s->timeOfLastRequest) >= TIMEOUT_TIME) {
    return TRUE;
  }
  return FALSE;
}

/* Destroyer function for htPrune
 *
 * - Free's UploadSession struct
 * - Closes the active file
 */
static void timeOutDestroyer(void *userData, void *value) {
  int status = 0;
  int returnCode = 0;
  int reasonCode = 0;

  UploadSession *s = value;
  status = fileClose(s->file, &returnCode, &reasonCode);
  if (status == -1) {
    printf("ERROR: Failed to close file %s, ReturnCode: %d, ReasonCode: %d", s->file->pathname, returnCode, reasonCode);
  }
  safeFree((char*)s, sizeof(UploadSession));
}

/* Reponds to the client with their
 * generated sessionID.
 */
static void respondWithSessionID(HttpResponse *response, int sessionID) {
  jsonPrinter *out = respondWithJsonPrinter(response);

  setContentType(response, "text/json");
  setResponseStatus(response, 200, "OK");
  addStringHeader(response, "Server", "jdmfws");
  addStringHeader(response, "Transfer-Encoding", "chunked");
  writeHeader(response);

  jsonStart(out);
  jsonAddString(out, "msg", "Please attach the sessionID to subsequent requests");
  jsonAddInt(out, "sessionID", sessionID);
  jsonEnd(out);

  finishResponse(response);
}

/* The sessionID is NOT present, so we try to
 * assign a sessionID to the caller.
 */
static void assignSessionIDToCaller(UploadSessionTracker *tracker, HttpResponse *response, char *routeFileName, unsigned int currUnixTime) {
  int reasonCode = 0;
  int returnCode = 0;
  int status = 0;
  FileInfo info;
  int fileExists = FALSE;
  UploadSession *currSession;
  FileID *fid;
  hashtable *sessionsByFileID = tracker->sessions;
  hashtable *fileIDsBySessionID = tracker->ids;

  /* If the file already exists in USS, then we should
   * only overwrite the file if the caller wants
   * to do so. If the file exist and this query parameter
   * is not present, then a sessionID will NOT be returned
   * and the user will be prompted to pick a different file name
   * or attach the forceOverwrite query parameter.
   *
   * NOTE: This value defaults to FALSE; thus it is not required
   * to be present on every request.
   */
  char *forceVal = getQueryParam(response->request, "forceOverwrite");
  int force = FALSE;
  if (!strcmp(strupcase(forceVal), "TRUE")) {
    force = TRUE;
  }

  /* First we check to see if the file already exists,
   * by calling fileInfo. fileInfo retrieves the metadata
   * for a file and stores it in the FileInfo struct. If it exists,
   * then it will return 0.
   */
  status = fileInfo(routeFileName, &info, &returnCode, &reasonCode);
  if (status == 0) {
    if (force == FALSE) {
      respondWithJsonError(response, "File already exists. Please choose a different file name or attach the forceOverwrite query parameter to your next request.", 200, "OK");
      return;
    }
    fileExists = TRUE;
  }

  /* Because our key is going to be a hash of a file's iNode
   * and deviceID, the file must exist in order to get those
   * values. If is doesn't exist, then we must create it in
   * order to capture those values.
   */
  if (fileExists == FALSE) {
    UnixFile *newFile = fileOpen(routeFileName,
                                 FILE_OPTION_CREATE | FILE_OPTION_READ_ONLY,
                                 0700,
                                 0,
                                 &returnCode,
                                 &reasonCode);

    if (newFile == NULL) {
      zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_WARNING, "Could not create file. Ret: %d, Res: %d\n", returnCode, reasonCode);
      respondWithJsonError(response, "Could not create new file.", 500, "Internal Server Error");
      return;
    }

    int status = fileClose(newFile, &returnCode, &reasonCode);
    if (status != 0) {
      zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_WARNING, "Could not close file. Ret: %d, Res: %d\n", returnCode, reasonCode);
      respondWithJsonError(response, "Could not close file.", 500, "Internal Server Error");
      return;
    }

    /* Call fileInfo for the newly created file in
     * order to get the metadeta.
     */
    status = fileInfo(routeFileName, &info, &returnCode, &reasonCode);
    if (status != 0) {
      zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_WARNING, "Could not get metadata for file. Ret: %d, Res: %d\n", returnCode, reasonCode);
      respondWithJsonError(response, "Could not get metadata for file.", 500, "Internal Server Error");
      return;
    }
  }

  /* Get the iNode from the FileInfo struct,
   * which is used as part of the key in the
   * HashTable.
   */
  int iNode = fileGetINode(&info);
  if (iNode == 0) {
    respondWithJsonError(response, "Could not get iNode of file.", 500, "Internal Server Error");
    return;
  }

  /* Get the iNode from the FileInfo struct,
   * which is used as part of the key in the
   * HashTable.
   */
  int deviceID = fileGetDeviceID(&info);
  if (deviceID == 0) {
    respondWithJsonError(response, "Could not get deviceID of file.", 500, "Internal Server Error");
    return;
  }

  FileID fileID = {0};
  fileID.iNode = iNode;
  fileID.deviceID = deviceID;

  /* Because the server does work on behalf of the caller,
   * the processID accessing files will be the same. As a result,
   * we can't rely on fileOpen to fail to notify the caller that
   * a file is currently busy. The solution to this is to store
   * active files in a HashTable. Here, we are searching the HashTable
   * to see if the requested file is currently active in an UploadSession.
   * If it's not active, then we can go ahead and add it to the HashTable.
   */
  pthread_mutex_lock(&tracker->fileHashLock);
  {
    if (htGet(sessionsByFileID, &fileID) != NULL) {
      pthread_mutex_unlock(&tracker->fileHashLock);

      respondWithJsonError(response, "Duplicate in table. Requested resource is busy. Please try again later.", 403, "Forbidden");
      return;
    }
    else {
      currSession = (UploadSession*)safeMalloc(sizeof(UploadSession), "UploadSession");
      fid = (FileID*)safeMalloc(sizeof(FileID), "FileID");
      fid->iNode = iNode;
      fid->deviceID = deviceID;
      htPut(sessionsByFileID, fid, currSession);
    }
  }
  pthread_mutex_unlock(&tracker->fileHashLock);

  /* Now we must open the file that we will be uploading. We do this now
   * as an indicator of whether or not the file is currently busy. If it
   * is busy on behalf of another instance of ZSS, then this WILL fail. In
   * the case of failure, then we must remove the entry from the HashTable.
   */
  UnixFile *uploadDestination = fileOpen(routeFileName,
                                         FILE_OPTION_TRUNCATE | FILE_OPTION_WRITE_ONLY,
                                         0,
                                         0,
                                         &returnCode,
                                         &reasonCode);

  if (uploadDestination == NULL) {
    pthread_mutex_lock(&tracker->fileHashLock);
    {
      htRemove(sessionsByFileID, fid);
    }
    pthread_mutex_unlock(&tracker->fileHashLock);
    zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_WARNING, "Could not open file. Ret: %d, Res: %d\n", returnCode, reasonCode);
    respondWithJsonError(response, "Could not open file. Requested resource is busy. Please try again later.", 403, "Forbidden");
    return;
  }

  /* We will generate a sessionID using srand(). We will check
   * to see if the sessionID is already active by calling htGet.
   * If after 10 tries, we can't generate a unique ID, then we
   * stop trying.
   */
  int sessionID = -1;
  pthread_mutex_lock(&tracker->fileHashLock);
  {
    int tryCount = 0;
    do {
      if (tryCount > 10) {
        htRemove(sessionsByFileID, fid);
        pthread_mutex_unlock(&tracker->fileHashLock);
        respondWithJsonError(response, "Could not assign a session id. Please try again later.", 500, "Internal Server Error");
        return;
      }
      sessionID = rand();
      tryCount++;
    } while (htIntGet(fileIDsBySessionID, sessionID) != NULL);

    UploadSession *s = htGet(sessionsByFileID, fid);
    s->file = uploadDestination;
    s->timeOfLastRequest = currUnixTime;
    s->sessionID = sessionID;

    htIntPut(fileIDsBySessionID, sessionID, fid);
  }
  pthread_mutex_unlock(&tracker->fileHashLock);

  respondWithSessionID(response, sessionID);
}

/* Search for any timeouts before processing the
 * next request. This is in case the file that this
 * current request has specified is stuck in the
 * HashTable.
 */
static void removeSessionTimeOuts(UploadSessionTracker *tracker, void *currUnixTimePtr) {
  hashtable *sessionsByFileID = tracker->sessions;

  pthread_mutex_lock(&tracker->fileHashLock);
  {
    htPrune(sessionsByFileID, timeOutMatcher, timeOutDestroyer, currUnixTimePtr);
  }
  pthread_mutex_unlock(&tracker->fileHashLock);
}

/* The sessionID is present, so we check it's validity
 * and begin the chunking logic for file writing.
 */
static void doChunking(UploadSessionTracker *tracker, HttpResponse *response, char *routeFileName, char *id) {
  FileID *fid;
  UploadSession *currSession;
  hashtable *sessionsByFileID = tracker->sessions;
  hashtable *fileIDsBySessionID = tracker->ids;

  int sessionID = atoi(id);

  /* We retrieve the FileID and the UploadSession
   * from their respective HashTables.
   */
  pthread_mutex_lock(&tracker->fileHashLock);
  {
    fid = htIntGet(fileIDsBySessionID, sessionID);
    currSession = htGet(sessionsByFileID, fid);
  }
  pthread_mutex_unlock(&tracker->fileHashLock);

  /* If the sessionID is valid, then the caller can begin
   * to upload their file. First, we check that the correct
   * encoding information is present in the form of query parameters.
   * After, we will check if this is the last chunk, which is also
   * in the form of a query parameter. Finally, we will write the chunk
   * to the file.
   */
  if (currSession != NULL) {
    char *sourceEncoding = getQueryParam(response->request, "sourceEncoding");
    char *targetEncoding = getQueryParam(response->request, "targetEncoding");
    if (sourceEncoding == NULL || targetEncoding == NULL) {
      respondWithJsonError(response, "Source encoding and target encoding are missing.", 400, "Bad Request");
      return;
    }

    char *lastChunkStr = getQueryParam(response->request, "lastChunk");
    int lastChunk = FALSE;
    if (!strcmp(strupcase(lastChunkStr), "TRUE")) {
      lastChunk = TRUE;
    }

    int returnCode = 0;
    int reasonCode = 0;
    int status = 0;
    if (!strcmp(strupcase(sourceEncoding), "BINARY") && !strcmp(strupcase(targetEncoding), "BINARY")) {
      status = writeBinaryDataFromBase64(currSession->file, response->request->contentBody, response->request->contentLength);
    }
    else if (strcmp(strupcase(sourceEncoding), "BINARY") && strcmp(strupcase(targetEncoding), "BINARY")) {
      status = writeAsciiDataFromBase64(currSession->file, response->request->contentBody, response->request->contentLength, sourceEncoding, targetEncoding);
    }
    else {
      respondWithJsonError(response, "Invalid encoding arguments. Cannot convert between ASCII and BINARY.", 400, "Bad Request");
      return;
    }
    if (status == 0 && lastChunk == FALSE) {
      response200WithMessage(response, "Successfully wrote chunk to file.");
    }
    else if (status == 0 && lastChunk == TRUE) {
     status = tagFile(routeFileName, NULL, TRUE);
     if (status == 0) {
       response200WithMessage(response, "Successfully wrote file.");
     }
     else {
       response200WithMessage(response, "Successfully wrote file, but could not tag.");
     }
     pthread_mutex_lock(&tracker->fileHashLock);
     {
       /* Must be removed from the tables in
        * this order.
        */
       htRemove(sessionsByFileID, fid);
       htRemove(fileIDsBySessionID, (void *)sessionID);
     }
     pthread_mutex_unlock(&tracker->fileHashLock);
    }
    else {
      respondWithJsonError(response, "Failed to write chunk to file. Aborting upload.");
      pthread_mutex_lock(&tracker->fileHashLock);
      {
        htRemove(sessionsByFileID, fid);
        htRemove(fileIDsBySessionID, (void *)sessionID);
      }
      pthread_mutex_unlock(&tracker->fileHashLock);
      return;
    }
  }
  else {
    respondWithJsonError(response, "Session identifier is invalid.", 400, "Bad Request");
    return;
  }
}

static int serveUnixFileContents(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  char *routeFileFrag = stringListPrint(request->parsedFile, 2, 1000, "/", 0);
  char *routeFileName = stringConcatenate(response->slh, "/", routeFileFrag);

  /* Seed based on time to help with
   * randomness when generating a
   * sessionID.
   */
  srand(time(NULL));
  unsigned int currUnixTime = (unsigned)time(NULL);
  void *currUnixTimePtr = &currUnixTime;

  if (!strcmp(request->method, methodPUT)) {
    UploadSessionTracker *tracker = service->userPointer;

    removeSessionTimeOuts(tracker, currUnixTimePtr);

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
      doChunking(tracker, response, routeFileName, id);
    }
  }
  else if (!strcmp(request->method, methodGET)) {
    respondWithUnixFileContentsWithAutocvtMode(NULL, response, routeFileName, TRUE, 0);
  }
  else if (!strcmp(request->method, methodDELETE)) {
    if (isDir(routeFileName) == 1) {
      deleteUnixDirectoryAndRespond(response, routeFileName);
    }
    else if (isDir(routeFileName) == 0) {
      deleteUnixFileAndRespond(response, routeFileName);
    }
    else {
      respondWithUnixFileNotFound(response, 1);
    }
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setContentType(response, "text/json");
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
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
  char *routeFileName = stringConcatenate(response->slh, "/", routeFileFrag);

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
    if (isDir(routeFileName) == 1) {
     copyUnixDirectoryAndRespond(response, routeFileName, newName, force);
    }
    else if (isDir(routeFileName) == 0) {
     copyUnixFileAndRespond(response, routeFileName, newName, force);
    }
    else {
      respondWithUnixFileNotFound(response, 1);
    }
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setContentType(response, "text/json");
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
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
  char *routeFileName = stringConcatenate(response->slh, "/", routeFileFrag);

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
    if (isDir(routeFileName) == 1) {
     renameUnixDirectoryAndRespond(response, routeFileName, newName, force);
    }
    else if (isDir(routeFileName) == 0) {
      renameUnixFileAndRespond(response, routeFileName, newName, force);
    }
    else {
      respondWithUnixFileNotFound(response, 1);
    }
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setContentType(response, "text/json");
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
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
  char *routeFileName = stringConcatenate(response->slh, "/", routeFileFrag);

  char *forceVal = getQueryParam(response->request, "forceOverwrite");
  int force = FALSE;

  if (!strcmp(strupcase(forceVal), "TRUE")) {
    force = TRUE;
  }

  if (!strcmp(request->method, methodPOST)) {
    createUnixDirectoryAndRespond(response, routeFileName, force);
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setContentType(response, "text/json");
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
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
  char *routeFileName = stringConcatenate(response->slh, "/", routeFileFrag);

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

    setContentType(response, "text/json");
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
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

    setContentType(response, "text/json");
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
    addStringHeader(response, "Allow", "GET");
    writeHeader(response);

    jsonStart(out);
    jsonEnd(out);

    finishResponse(response);
  }

  return 0;
}

static int serveTableOfContents(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;

  if (!strcmp(request->method, methodGET)) {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setContentType(response, "text/json");
    setResponseStatus(response, 404, "Not Found");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
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

    jsonEndArray(out);
    jsonEnd(out);

    finishResponse(response);
  }
  else {
    jsonPrinter *out = respondWithJsonPrinter(response);

    setContentType(response, "text/json");
    setResponseStatus(response, 405, "Method Not Allowed");
    addStringHeader(response, "Server", "jdmfws");
    addStringHeader(response, "Transfer-Encoding", "chunked");
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

  UploadSessionTracker *tracker = (UploadSessionTracker*)safeMalloc(sizeof(UploadSessionTracker), "UploadSessionTracker");
  tracker->sessions = htCreate(17, fileIDHasher, compareFileID, freeKeySessionsByFileID, freeValueSessionsByFileID);
  tracker->ids = htCreate(17, NULL, NULL, NULL, NULL);
  pthread_mutex_init(&tracker->fileHashLock, NULL);
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

