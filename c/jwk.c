
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#error Metal C not supported
#endif // METTLE

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <gskssl.h>
#include <gsktypes.h>
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "xlate.h"
#include "bpxnet.h"
#include "collections.h"
#include "unixfile.h"
#include "socketmgmt.h"
#include "le.h"
#include "logging.h"
#include "scheduling.h"
#include "json.h"
#include "httpserver.h"
#include "charsets.h"
#include "httpclient.h"
#include "zssLogging.h"
#include "jwt.h"
#include "jwk.h"

static Json *receiveResponse(ShortLivedHeap *slh, HttpClientContext *httpClientContext, HttpClientSession *session, int *statusOut);
static Json *doRequest(ShortLivedHeap *slh, HttpClientSettings *clientSettings, TlsEnvironment *tlsEnv, char *path, int *statusOut);
static void getPublicKey(Json *jwk, x509_public_key_info *publicKeyOut, int *statusOut);
static int getJwk(JwkContext *context);
static int checkJwtSignature(JwsAlgorithm algorithm, int sigLen, const uint8_t *signature, int msgLen, const uint8_t *message, void *userData);
static bool decodeBase64Url(const char *data, char *resultBuf, int *lenOut);
static int jwkTaskMain(RLETask *task);

void configureJwt(HttpServer *server, JwkSettings *settings) {
  int rc = 0;

  if (!settings) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "JWT disabled or APIML is not configured\n");
    return;
  }

  JwkContext *context = (JwkContext*)safeMalloc(sizeof(*context), "Jwk Context");
  if (!context) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "failed to allocate JWK context\n");
    return;
  }

  context->settings = settings;
  if (httpServerInitJwtContextCustom(server, settings->fallback, checkJwtSignature, context, &rc) != 0) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "failed to init JWT context for HTTP server, rc = %d\n", rc);
    safeFree((char*)context, sizeof(*context));
    return;
  };

  RLETask *task = makeRLETask(server->base->rleAnchor, RLE_TASK_TCB_CAPABLE | RLE_TASK_DISPOSABLE, jwkTaskMain);
  if (!task) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "failed to create background task for JWK\n");
    safeFree((char*)context, sizeof(*context));
    return;
  }
  task->userPointer = context;
  startRLETask(task, NULL);

  zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_INFO, ZSS_LOG_JWK_URL_MSG, settings->host, settings->port, settings->path);
}

static int jwkTaskMain(RLETask *task) {
  JwkContext *context = (JwkContext*)task->userPointer;
  JwkSettings *settings = context->settings;
  const int maxAttempts = 1000;
  const int retryIntervalSeconds = 30;
  bool success = false;

  for (int i = 0; i < maxAttempts; i++) {
    int status = getJwk(context);
    if (status == JWK_STATUS_OK) {
      success = true;
      context->isPublicKeyInitialized = true;
      break;
    } else if (status == JWK_STATUS_UNRECOGNIZED_FMT_ERROR) {
      zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_WARNING, ZSS_LOG_JWK_UNRECOGNIZED_MSG);
      break;
    } else if (status == JWK_STATUS_PUBLIC_KEY_ERROR) {
      zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_WARNING, ZSS_LOG_JWK_PUBLIC_KEY_ERROR_MSG);
      break;
    } else if (status == JWK_STATUS_HTTP_CONTEXT_ERROR) {
      zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_WARNING, ZSS_LOG_JWK_HTTP_CTX_ERROR_MSG);
      break;
    } else {
      zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_WARNING, ZSS_LOG_JWK_RETRY_MSG,
              jwkGetStrStatus(status), retryIntervalSeconds);
      sleep(retryIntervalSeconds);
    }
  }
  if (success) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_INFO, ZSS_LOG_JWK_READY_MSG, settings->fallback ? "with" : "without");
  } else {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_WARNING, ZSS_LOG_JWK_FAILED_MSG);
  }
  fflush(stdout);
}

static int getJwk(JwkContext *context) {
  JwkSettings *settings = context->settings;
  int status = 0;
  ShortLivedHeap *slh = makeShortLivedHeap(0x40000, 0x40);

  HttpClientSettings clientSettings = {0};
  clientSettings.host = settings->host;
  clientSettings.port = settings->port;
  clientSettings.recvTimeoutSeconds = (settings->timeoutSeconds > 0) ? settings->timeoutSeconds : 10;

  Json *jwkJson = doRequest(slh, &clientSettings, settings->tlsEnv, settings->path, &status);
  if (status) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "failed to obtain JWK, status = %d\n", status);
  } else {
    x509_public_key_info publicKey;
    getPublicKey(jwkJson, &publicKey, &status);
    if (status == 0) {
      context->publicKey = publicKey;
    }
  }
  SLHFree(slh);
  return status; 
}

static Json *doRequest(ShortLivedHeap *slh, HttpClientSettings *clientSettings, TlsEnvironment *tlsEnv, char *path, int *statusOut) {
  int status = 0;
  HttpClientContext *httpClientContext = NULL;
  HttpClientSession *session = NULL;
  LoggingContext *loggingContext = makeLoggingContext();
  Json *jsonBody = NULL;

  do {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "JWK request to https://%s:%d%s\n",
            clientSettings->host, clientSettings->port, path);
    status = httpClientContextInitSecure(clientSettings, loggingContext, tlsEnv, &httpClientContext);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "error in httpcb ctx init: %d\n", status);
      *statusOut = JWK_STATUS_HTTP_CONTEXT_ERROR;
      break;
    }
    status = httpClientSessionInit(httpClientContext, &session);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "error initing session: %d\n", status);
      *statusOut = JWK_STATUS_HTTP_REQUEST_ERROR;
      break;
    }
    status = httpClientSessionStageRequest(httpClientContext, session, "GET", path, NULL, NULL, NULL, 0);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "error staging request: %d\n", status);
      *statusOut = JWK_STATUS_HTTP_REQUEST_ERROR;
      break;
    }
    requestStringHeader(session->request, TRUE, "accept", "application/json");
    status = httpClientSessionSend(httpClientContext, session);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "error sending request: %d\n", status);
      *statusOut = JWK_STATUS_HTTP_REQUEST_ERROR;
      break;
    }
    jsonBody = receiveResponse(slh, httpClientContext, session, &status);
    if (status) {
      *statusOut = status;
      break;
    }
    int statusCode = session->response->statusCode;
    if (statusCode != 200) {
      zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "HTTP status %d\n", statusCode);
      *statusOut = JWK_STATUS_RESPONSE_ERROR;
      break;
    }
  } while (0);
  if (session) {
    httpClientSessionDestroy(session);
  }
  if (httpClientContext) {
    httpClientContextDestroy(httpClientContext);
  }  
  return jsonBody;
}

static Json *receiveResponse(ShortLivedHeap *slh, HttpClientContext *httpClientContext, HttpClientSession *session, int *statusOut) {
  bool done = false;
  Json *jsonBody = NULL;
  while (!done) {
    int status = httpClientSessionReceiveNative(httpClientContext, session, 1024);
    if (status != 0) {
      zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "error receiving response: %d\n", status);
      break;
    }
    if (session->response) {
      done = true;
      break;
    }
  }
  if (done) {
    int contentLength = session->response->contentLength;
    char *body = session->response->body;
    char responseEbcdic[contentLength + 1];
    memset(responseEbcdic, '\0', contentLength + 1);
    memcpy(responseEbcdic, body, contentLength);
    __atoe_l(responseEbcdic, contentLength);
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "JWK response: %s\n", responseEbcdic);
    char errorBuf[1024];
    ShortLivedHeap *slh = session->slh;
    jsonBody = jsonParseUnterminatedUtf8String(slh, CCSID_IBM1047, body, contentLength, errorBuf, sizeof(errorBuf));
    if (!jsonBody) {
      zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "error parsing JSON response: %s\n", errorBuf);
      *statusOut = JWK_STATUS_JSON_RESPONSE_ERROR;
      return NULL;
    } else {
      *statusOut = JWK_STATUS_OK;
    }
  } else {
    *statusOut = JWK_STATUS_RESPONSE_ERROR;
  }
  return jsonBody;
}

static void getPublicKey(Json *jwk, x509_public_key_info *publicKeyOut, int *statusOut) {
  int status = 0;
  JsonObject *jwkObject = jsonAsObject(jwk);

  if (!jwkObject) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "JWK is not object\n");
    *statusOut = JWK_STATUS_UNRECOGNIZED_FMT_ERROR;
    return;
  }

  JsonArray *keysArray = jsonObjectGetArray(jwkObject, "keys");
  if (!keysArray) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "JWK doesn't have 'keys' array\n");
    *statusOut = JWK_STATUS_UNRECOGNIZED_FMT_ERROR;
    return;
  }

  JsonObject *keyObject = jsonArrayGetObject(keysArray, 0);
  if (!keyObject) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "JWK doesn't contain key\n");
    *statusOut = JWK_STATUS_UNRECOGNIZED_FMT_ERROR;
    return;
  }

  char *alg = jsonObjectGetString(keyObject, "kty");
    if (!alg) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "JWK key doesn't have 'kty' property\n");
    *statusOut = JWK_STATUS_UNRECOGNIZED_FMT_ERROR;
    return;
  }

  if (0 != strcasecmp(alg, "RSA")) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "JWK key uses unsupported algorithm - '%s'\n", alg);
    *statusOut = JWK_STATUS_UNRECOGNIZED_FMT_ERROR;
    return;
  }

  char *modulusBase64Url = jsonObjectGetString(keyObject, "n");
  char *exponentBase64Url = jsonObjectGetString(keyObject, "e");
  if (!modulusBase64Url || !exponentBase64Url) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "modulus or exponent not found\n");
    *statusOut = JWK_STATUS_UNRECOGNIZED_FMT_ERROR;
    return;
  }

  char modulus[strlen(modulusBase64Url)+1];
  int modulusLen = 0;
  if (!decodeBase64Url(modulusBase64Url, modulus, &modulusLen)) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "failed to decode modulus '%s'\n", modulusBase64Url);
    *statusOut = JWK_STATUS_UNRECOGNIZED_FMT_ERROR;
    return;
  }

  char exponent[strlen(exponentBase64Url)+1];
  int exponentLen = 0;
  if (!decodeBase64Url(exponentBase64Url, exponent, &exponentLen)) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "failed to decode exponent '%s' - %s\n", exponentBase64Url, jwkGetStrStatus(status));
    *statusOut = JWK_STATUS_UNRECOGNIZED_FMT_ERROR;
    return;
  }

  gsk_buffer modulusBuf = { .data = (void*)modulus, .length = modulusLen };
  gsk_buffer exponentBuf = { .data = (void*)exponent, .length = exponentLen };
  status = gsk_construct_public_key_rsa(&modulusBuf, &exponentBuf, publicKeyOut);
  if (status != 0) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "failed to construct public key: %s\n", gsk_strerror(status));
    *statusOut = JWK_STATUS_PUBLIC_KEY_ERROR;
    return;
  }

  *statusOut = JWK_STATUS_OK;
}

/* Returns true in case of success */
static bool decodeBase64Url(const char *data, char *resultBuf, int *lenOut) {
  size_t dataLen = strlen(data);
  size_t base64Size = dataLen + 16;
  char base64[base64Size];

  strcpy(base64, data);
  if (base64urlToBase64(base64, base64Size) < 0) {
    return false;
  }

  int decodedLen = decodeBase64(base64, resultBuf);
  if (decodedLen <= 0) {
    return false;    
  }
  *lenOut = decodedLen;
  return true;
}

static int checkJwtSignature(JwsAlgorithm algorithm,
                      int sigLen, const uint8_t *signature,
                      int msgLen, const uint8_t *message,
                      void *userData) {
  JwkContext *context = userData;
  if (!context->isPublicKeyInitialized) {
    return RC_JWT_NOT_CONFIGURED;
  }

  if (algorithm == JWS_ALGORITHM_none) {
    if (sigLen == 0) {
      return RC_JWT_INSECURE;
    } else {
      return RC_JWT_INVALID_SIGLEN;
    }
  }

  x509_algorithm_type alg = x509_alg_unknown;
  gsk_buffer msgBuffer = { .length = msgLen, .data = (void*)message };
  gsk_buffer sigBuffer = { .length = sigLen, .data = (void*)signature };

  switch (algorithm) {
    case JWS_ALGORITHM_RS256:
      alg = x509_alg_sha256WithRsaEncryption;
      break;
  }
  int gskStatus = gsk_verify_data_signature(alg, &context->publicKey, 0, &msgBuffer, &sigBuffer);
  if (gskStatus != 0) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "failed to verify signature with status %d - %s\n",
            gskStatus, gsk_strerror(gskStatus));
    return RC_JWT_SIG_MISMATCH;
  }
  return RC_JWT_OK;
}

static const char *MESSAGES[] = {
  [JWK_STATUS_OK] = "OK",
  [JWK_STATUS_HTTP_ERROR] = "HTTP error",
  [JWK_STATUS_RESPONSE_ERROR] = "HTTP response error",
  [JWK_STATUS_JSON_RESPONSE_ERROR] = "HTTP response error - invalid JSON",
  [JWK_STATUS_UNRECOGNIZED_FMT_ERROR] = "JWK is in unrecognized format",
  [JWK_STATUS_PUBLIC_KEY_ERROR] = "failed to create public key",
  [JWK_STATUS_HTTP_CONTEXT_ERROR] = "failed to init HTTP context",
  [JWK_STATUS_HTTP_REQUEST_ERROR] = "failed to send HTTP request"
};

#define MESSAGE_COUNT sizeof(MESSAGES)/sizeof(MESSAGES[0])

const char *jwkGetStrStatus(int status) {
  if (status >= MESSAGE_COUNT || status < 0) {
    return "Unknown status code";
  }
  const char *message = MESSAGES[status];
  if (!message) {
    return "Unknown status code";
  }
  return message;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
