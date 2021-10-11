#ifdef METTLE
#error Metal C not supported
#endif // METTLE

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
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
#include "jwk.h"

static Json *receiveResponse(HttpClientContext *httpClientContext, HttpClientSession *session, int *statusOut);
static Json *doRequest(HttpClientSettings *clientSettings, TlsEnvironment *tlsEnv, char *path, int *statusOut);

Json *obtainJwk(JwkSettings *settings) {
  int status = 0;
  if (!settings) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "jwt settings are NULL\n");
    return NULL;
  }
  HttpClientSettings clientSettings = {0};
  clientSettings.host = settings->host;
  clientSettings.port = settings->port;
  clientSettings.recvTimeoutSeconds = (settings->timeoutSeconds > 0) ? settings->timeoutSeconds : 10;

  Json *jwk = doRequest(&clientSettings, settings->tlsEnv, settings->path, &status);
  if (status) {
    zowelog(NULL, LOG_COMP_ID_JWK, ZOWE_LOG_DEBUG, "failed to obtain JWK, rc = %d\n", status);
    return NULL;
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
