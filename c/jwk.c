
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
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <gskcms.h>
#include <gskssl.h>
#include <gsktypes.h>
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "xlate.h"
#include "collections.h"
#include "json.h"
#include "tls.h"
#include "httpserver.h"
#include "charsets.h"
#include "httpclient.h"
#include "zssLogging.h"
#include "jwt.h"
#include "jwk.h"

static Json *receiveResponse(ShortLivedHeap *slh, HttpClientContext *httpClientContext, HttpClientSession *session, int *statusOut);
static Json *doRequest(ShortLivedHeap *slh, HttpClientSettings *clientSettings, TlsEnvironment *tlsEnv, char *path, int *statusOut);
static void getPublicKey(Json *jwk, x509_public_key_info *publicKeyOut, int *statusOut);
static int obtainJwk(JwkContext *context);
static void *obtainJwkInBackground(void *data);
static int checkJwtSignature(JwsAlgorithm algorithm, int sigLen, const uint8_t signature[], int msgLen, const uint8_t message[], void *userData);

void configureJwt(HttpServer *server, JwkSettings *jwkSettings) {
  fprintf (stdout, "begin %s jwkSettings 0x%p\n", __FUNCTION__, jwkSettings);
  if (!jwkSettings) {
    return;
  }
  JwkContext *jwkContext = (JwkContext*)safeMalloc(sizeof(*jwkContext), "Jwk Context");
  if (!jwkContext) {
    return;
  }
  jwkContext->settings = jwkSettings;
  jwkContext->slh = makeShortLivedHeap(0x40000, 0x40);
  int rc = 0;
  httpServerInitJwtContextCustom(server, jwkSettings->fallback, checkJwtSignature, jwkContext, &rc);
  pthread_t threadId;
  if (pthread_create(&threadId, NULL, obtainJwkInBackground, jwkContext) != 0) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "failed to create JWK thread - %s\n", strerror(errno));
  };
}

static void *obtainJwkInBackground(void *data) {
  fprintf (stdout, "begin %s\n", __FUNCTION__);
  int maxAttempts = 10000;
  JwkContext *context = data;
    for (int i = 0; i < maxAttempts; i++) {
      fprintf (stdout, "begin attempt %d\n", i);
      int status = obtainJwk(context);
      SLHFree(context->slh);
      if (status == 0) {
        fprintf (stdout, "got jwk %d\n", i);
        zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "jwk received\n");
        context->isPublicKeyInitialized = true;
        break;
      } else {
        context->slh = makeShortLivedHeap(0x40000, 0x40);
        zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "failed to obtain jwk, repeat again\n");
        sleep(2);
    }
  }
}

static int obtainJwk(JwkContext *context) {
  JwkSettings *settings = context->settings;
  int status = 0;

  HttpClientSettings clientSettings = {0};
  clientSettings.host = settings->host;
  clientSettings.port = settings->port;
  clientSettings.recvTimeoutSeconds = (settings->timeoutSeconds > 0) ? settings->timeoutSeconds : 10;

  Json *jwkJson = doRequest(context->slh, &clientSettings, settings->tlsEnv, settings->path, &status);
  if (status) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "failed to obtain JWK, rc = %d\n", status);
    return status;
  }
  if (jwkJson) {
    x509_public_key_info publicKey;
    getPublicKey(jwkJson, &publicKey, &status);
    if (status == 0) {
      context->publicKey = publicKey;
    }
  }
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
      *statusOut = JWK_STATUS_HTTP_ERROR;
      break;
    }
    status = httpClientSessionInit(httpClientContext, &session);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "error initing session: %d\n", status);
      *statusOut = JWK_STATUS_HTTP_ERROR;
      break;
    }
    status = httpClientSessionStageRequest(httpClientContext, session, "GET", path, NULL, NULL, NULL, 0);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "error staging request: %d\n", status);
      *statusOut = JWK_STATUS_HTTP_ERROR;
      break;
    }
    requestStringHeader(session->request, TRUE, "accept", "application/json");
    status = httpClientSessionSend(httpClientContext, session);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "error sending request: %d\n", status);
      *statusOut = JWK_STATUS_HTTP_ERROR;
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
    char *responseEbcdic = SLHAlloc(slh, contentLength + 1);
    if (responseEbcdic) {
      memset(responseEbcdic, '\0', contentLength + 1);
      memcpy(responseEbcdic, body, contentLength);
      __atoe_l(responseEbcdic, contentLength);
      zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "successfully received response: %s\n", responseEbcdic);
    }
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
  JsonObject *jwkObject = jsonAsObject(jwk);
  if (!jwkObject) {
    *statusOut = JWK_STATUS_INVALID_FORMAT_ERROR;
    return;
  }
  JsonArray *keysArray = jsonObjectGetArray(jwkObject, "keys");
  if (!keysArray) {
    *statusOut = JWK_STATUS_INVALID_FORMAT_ERROR;
    return;
  }
  JsonObject *keyObject = jsonArrayGetObject(keysArray, 0);
  if (!keyObject) {
    *statusOut = JWK_STATUS_INVALID_FORMAT_ERROR;
    return;
  }
  char *alg = jsonObjectGetString(keyObject, "kty");
    if (!alg) {
    *statusOut = JWK_STATUS_INVALID_FORMAT_ERROR;
    return;
  }
  if (0 != strcmp(alg, "RSA")) {
    *statusOut = JWK_STATUS_NOT_RSA_KEY;
    return;
  }
  char *modulusBase64Url = jsonObjectGetString(keyObject, "n");
  char *exponentBase64Url = jsonObjectGetString(keyObject, "e");
  if (!modulusBase64Url || !exponentBase64Url) {
    *statusOut = JWK_STATUS_RSA_FACTORS_NOT_FOUND;
    return;
  }

  size_t modulusBase64Size = strlen(modulusBase64Url) * 2 + 1;
  char modulusBase64[modulusBase64Size];
  strcpy(modulusBase64, modulusBase64Url);
  if (base64urlToBase64(modulusBase64, modulusBase64Size) < 0) {
    *statusOut = JWK_STATUS_INVALID_BASE64_URL;
    return;    
  }

  size_t exponentBase64Size = strlen(exponentBase64Url) * 2 + 1;
  char exponentBase64[exponentBase64Size];
  strcpy(exponentBase64, exponentBase64Url);
  if (base64urlToBase64(exponentBase64, exponentBase64Size) < 0) {
    *statusOut = JWK_STATUS_INVALID_BASE64_URL;
    return;    
  }

  char modulus[modulusBase64Size];
  int modulusLen = decodeBase64(modulusBase64, modulus);
  if (modulusLen <= 0) {
    *statusOut = JWK_STATUS_INVALID_BASE64;
    return;    
  }
  modulus[modulusLen] = '\0';

  char exponent[exponentBase64Size];
  int exponentLen = decodeBase64(exponentBase64, exponent);
  if (exponentLen <= 0) {
    *statusOut = JWK_STATUS_INVALID_BASE64;
    return;    
  }
  exponent[exponentLen] = '\0';

  gsk_buffer modulusBuffer = { .data = (void*)modulus, .length = modulusLen };
  gsk_buffer exponentBuffer = { .data = (void*)exponent, .length = exponentLen };
  int status = gsk_construct_public_key_rsa(&modulusBuffer, &exponentBuffer, publicKeyOut);
  if (status != 0) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "failed to create public key: %s\n", gsk_strerror(status));
    *statusOut = JWK_STATUS_INVALID_PUBLIC_KEY;
  }

  *statusOut = JWK_STATUS_OK;
}

static int checkJwtSignature(JwsAlgorithm algorithm,
                      int sigLen, const uint8_t signature[],
                      int msgLen, const uint8_t message[],
                      void *userData) {
  JwkContext *context = userData;
  if (!context->isPublicKeyInitialized) {
    return RC_JWT_PUBLIC_KEY_NOT_CONFIGURED;
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


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
