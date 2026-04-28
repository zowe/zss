
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "json.h"
#include "bpxnet.h"
#include "socketmgmt.h"
#include "unixfile.h"
#include "httpserver.h"
#include "logging.h"
#include "zssLogging.h"
#include "icsf.h"
#include "checksumService.h"

#ifdef __ZOWE_OS_ZOS

#define CHECKSUM_READ_BUFFER_SIZE 65536

typedef struct AlgorithmEntry_tag {
  char *name;
  int icsfType;
  int hashLength;
} AlgorithmEntry;

static AlgorithmEntry algorithmTable[] = {
  {"md5",      ICSF_DIGEST_MD5,      16},
  {"sha1",     ICSF_DIGEST_SHA1,     20},
  {"sha224",   ICSF_DIGEST_SHA224,   28},
  {"sha256",   ICSF_DIGEST_SHA256,   32},
  {"sha384",   ICSF_DIGEST_SHA384,   48},
  {"sha512",   ICSF_DIGEST_SHA512,   64},
  {"sha3-224", ICSF_DIGEST_SHA3_224, 28},
  {"sha3-256", ICSF_DIGEST_SHA3_256, 32},
  {"sha3-384", ICSF_DIGEST_SHA3_384, 48},
  {"sha3-512", ICSF_DIGEST_SHA3_512, 64}
};

#define ALGORITHM_COUNT (sizeof(algorithmTable) / sizeof(algorithmTable[0]))

static AlgorithmEntry *findAlgorithm(const char *name) {
  for (int i = 0; i < ALGORITHM_COUNT; i++) {
    if (!strcasecmp(name, algorithmTable[i].name)) {
      return &algorithmTable[i];
    }
  }
  return NULL;
}

static int probeAlgorithm(int icsfType) {
  ICSFDigest digest;
  char hash[64];
  char *probeString = "ZOWE ALGORITHM CHECK";
  int probeLen = strlen(probeString);
  int rc;

  rc = icsfDigestInit(&digest, icsfType);
  if (rc != 0) {
    return rc;
  }
  rc = icsfDigestUpdate(&digest, probeString, probeLen);
  if (rc != 0) {
    return rc;
  }
  rc = icsfDigestFinish(&digest, hash);
  return rc;
}

static void bytesToHex(const char *bytes, int len, char *hexOut) {
  static const char hexChars[] = "0123456789abcdef";
  for (int i = 0; i < len; i++) {
    unsigned char b = (unsigned char)bytes[i];
    hexOut[i * 2]     = hexChars[(b >> 4) & 0x0F];
    hexOut[i * 2 + 1] = hexChars[b & 0x0F];
  }
  hexOut[len * 2] = '\0';
}

static void respondWithError(HttpResponse *response, int statusCode,
                             const char *statusMessage, const char *errorMsg) {
  jsonPrinter *out = respondWithJsonPrinter(response);
  setResponseStatus(response, statusCode, (char *)statusMessage);
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(out);
  jsonAddString(out, "error", (char *)errorMsg);
  jsonEnd(out);
  finishResponse(response);
}

static int serveChecksumInfo(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;

  if (strcmp(request->method, methodGET)) {
    respondWithError(response, 405, "Method Not Allowed", "Only GET is supported");
    return 0;
  }

  jsonPrinter *out = respondWithJsonPrinter(response);
  setResponseStatus(response, 200, "OK");
  setDefaultJSONRESTHeaders(response);
  writeHeader(response);
  jsonStart(out);
  jsonStartArray(out, "algorithms");

  for (int i = 0; i < ALGORITHM_COUNT; i++) {
    int rc = probeAlgorithm(algorithmTable[i].icsfType);
    jsonStartObject(out, NULL);
    jsonAddString(out, "name", algorithmTable[i].name);
    jsonAddInt(out, "hashBytes", algorithmTable[i].hashLength);
    if (rc == 0) {
      jsonAddString(out, "status", "available");
    } else {
      jsonAddString(out, "status", "unavailable");
      jsonAddInt(out, "icsfReturnCode", rc);
    }
    jsonEndObject(out);
  }

  jsonEndArray(out);
  jsonEnd(out);
  finishResponse(response);
  return 0;
}

static int serveChecksumFile(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;

  if (strcmp(request->method, methodGET)) {
    respondWithError(response, 405, "Method Not Allowed", "Only GET is supported");
    return 0;
  }

  /* URL: /checksum/file/<algorithm>/<path...>
   * parsedFile segments: 0=checksum 1=file 2=algorithm 3+=path
   */
  StringList *parsedFile = request->parsedFile;

  /* Extract algorithm name from segment 2 */
  char *algorithmFrag = stringListPrint(parsedFile, 2, 3, "/", 0);
  if (algorithmFrag == NULL || strlen(algorithmFrag) == 0) {
    respondWithError(response, 400, "Bad Request", "Missing algorithm in URL path");
    return 0;
  }

  AlgorithmEntry *alg = findAlgorithm(algorithmFrag);
  if (alg == NULL) {
    respondWithError(response, 400, "Bad Request", "Unsupported algorithm");
    return 0;
  }

  /* Extract file path from segments 3+ */
  char *routeFileFrag = stringListPrint(parsedFile, 3, 1000, "/", 0);
  if (routeFileFrag == NULL || strlen(routeFileFrag) == 0) {
    respondWithError(response, 400, "Bad Request", "Missing file path in URL");
    return 0;
  }

  char *encodedPath = stringConcatenate(response->slh, "/", routeFileFrag);
  char *filePath = cleanURLParamValue(response->slh, encodedPath);

  zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_DEBUG,
          "checksum request: algorithm=%s path=%s\n", alg->name, filePath);

  /* Stat the file to verify it exists and is a regular file */
  FileInfo info;
  int returnCode = 0;
  int reasonCode = 0;
  int status = fileInfo(filePath, &info, &returnCode, &reasonCode);
  if (status != 0) {
    respondWithError(response, 404, "Not Found", "File not found or not accessible");
    return 0;
  }

  if (info.fileType != BPXSTA_FILETYPE_REGULAR) {
    respondWithError(response, 400, "Bad Request", "Path is not a regular file");
    return 0;
  }

  /* Open the file for reading */
  UnixFile *file = fileOpen(filePath, FILE_OPTION_READ_ONLY, 0, 0,
                            &returnCode, &reasonCode);
  if (file == NULL) {
    zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_WARNING,
            "checksum: failed to open %s, rc=%d rsn=0x%08x\n",
            filePath, returnCode, reasonCode);
    respondWithError(response, 500, "Internal Server Error", "Failed to open file");
    return 0;
  }

  /* Initialize the digest */
  ICSFDigest digest;
  int rc = icsfDigestInit(&digest, alg->icsfType);
  if (rc != 0) {
    fileClose(file, &returnCode, &reasonCode);
    zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_WARNING,
            "checksum: icsfDigestInit failed, rc=%d\n", rc);
    respondWithError(response, 500, "Internal Server Error",
                     "Hash algorithm initialization failed");
    return 0;
  }

  /* Stream the file through the digest */
  char *readBuffer = safeMalloc(CHECKSUM_READ_BUFFER_SIZE, "checksumReadBuf");
  int hashFailed = 0;

  while (1) {
    int bytesRead = fileRead(file, readBuffer, CHECKSUM_READ_BUFFER_SIZE,
                             &returnCode, &reasonCode);
    if (bytesRead <= 0) {
      break;
    }
    rc = icsfDigestUpdate(&digest, readBuffer, bytesRead);
    if (rc != 0) {
      zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_WARNING,
              "checksum: icsfDigestUpdate failed, rc=%d\n", rc);
      hashFailed = 1;
      break;
    }
  }

  fileClose(file, &returnCode, &reasonCode);

  if (!hashFailed) {
    char hashResult[64];
    rc = icsfDigestFinish(&digest, hashResult);
    if (rc != 0) {
      zowelog(NULL, LOG_COMP_ID_UNIXFILE, ZOWE_LOG_WARNING,
              "checksum: icsfDigestFinish failed, rc=%d\n", rc);
      hashFailed = 1;
    }

    if (!hashFailed) {
      /* Convert hash to hex string */
      char hexString[129];
      bytesToHex(hashResult, alg->hashLength, hexString);

      jsonPrinter *out = respondWithJsonPrinter(response);
      setResponseStatus(response, 200, "OK");
      setDefaultJSONRESTHeaders(response);
      writeHeader(response);
      jsonStart(out);
      jsonAddString(out, "algorithm", alg->name);
      jsonAddString(out, "checksum", hexString);
      jsonAddString(out, "path", filePath);
      jsonEnd(out);
      finishResponse(response);

      safeFree(readBuffer, CHECKSUM_READ_BUFFER_SIZE);
      return 0;
    }
  }

  safeFree(readBuffer, CHECKSUM_READ_BUFFER_SIZE);
  respondWithError(response, 500, "Internal Server Error", "Hash computation failed");
  return 0;
}

void installChecksumService(HttpServer *server) {
  HttpService *httpService;

  httpService = makeGeneratedService("ChecksumInfo", "/checksum/info");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = serveChecksumInfo;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  registerHttpService(server, httpService);

  httpService = makeGeneratedService("ChecksumFile", "/checksum/file/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = serveChecksumFile;
  httpService->runInSubtask = TRUE;
  httpService->doImpersonation = TRUE;
  registerHttpService(server, httpService);
}

#endif /* __ZOWE_OS_ZOS */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
