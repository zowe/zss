
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
#include "charsets.h"
#include "httpclient.h"
#include "zssLogging.h"
#include "jwt.h"
#include "jwk.h"

static Json *receiveResponse(HttpClientContext *httpClientContext, HttpClientSession *session, int *statusOut);
static Json *doRequest(HttpClientSettings *clientSettings, TlsEnvironment *tlsEnv, char *path, int *statusOut);
static void getPublicKey(Json *jwk, x509_public_key_info *publicKeyOut, int *statusOut);

Jwk *obtainJwk(JwkSettings *settings) {
  Jwk *jwk = NULL;
  int status = 0;
  if (!settings) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "jwt settings are NULL\n");
    return NULL;
  }
  HttpClientSettings clientSettings = {0};
  clientSettings.host = settings->host;
  clientSettings.port = settings->port;
  clientSettings.recvTimeoutSeconds = (settings->timeoutSeconds > 0) ? settings->timeoutSeconds : 10;

  Json *jwkJson = doRequest(&clientSettings, settings->tlsEnv, settings->path, &status);
  if (status) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "failed to obtain JWK, rc = %d\n", status);
    return NULL;
  }
  if (jwkJson) {
    x509_public_key_info publicKey;
    getPublicKey(jwkJson, &publicKey, &status);
    if (status == 0) {
      jwk = (Jwk*)safeMalloc(sizeof(Jwk), "JWK");
      jwk->publicKey = publicKey;
    }
  }
  return jwk; 
}

static Json *doRequest(HttpClientSettings *clientSettings, TlsEnvironment *tlsEnv, char *path, int *statusOut) {
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
    jsonBody = receiveResponse(httpClientContext, session, &status);
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

static Json *receiveResponse(HttpClientContext *httpClientContext, HttpClientSession *session, int *statusOut) {
  bool done = false;
  Json *jsonBody = NULL;
  ShortLivedHeap *slh = session->slh;
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
  int decodedLen = decodeBase64(modulusBase64, modulus);
  if (decodedLen <= 0) {
    *statusOut = JWK_STATUS_INVALID_BASE64;
    return;    
  }
  modulus[decodedLen] = '\0';

  char exponent[exponentBase64Size];
  decodedLen = decodeBase64(exponentBase64, exponent);
  if (decodedLen <= 0) {
    *statusOut = JWK_STATUS_INVALID_BASE64;
    return;    
  }
  exponent[decodedLen] = '\0';

  gsk_buffer modulusBuffer = { .data = (void*)modulus, .length = strlen(modulus) };
  gsk_buffer exponentBuffer = { .data = (void*)exponent, .length = strlen(exponent) };
  int status = gsk_construct_public_key_rsa(&modulusBuffer, &exponentBuffer, publicKeyOut);
  if (status != 0) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "failed to create public key: %s\n", gsk_strerror(status));
    *statusOut = JWK_STATUS_INVALID_PUBLIC_KEY;
  }

  *statusOut = JWK_STATUS_OK;
}

int checkJwtSignature(JwsAlgorithm algorithm,
                      int sigLen, const uint8_t signature[],
                      int msgLen, const uint8_t message[],
                      void *userData) {
  Jwk *jwk = userData;

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
  int gskStatus = gsk_verify_data_signature(alg, &jwk->publicKey, 0, &msgBuffer, &sigBuffer);
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
