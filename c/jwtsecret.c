#ifdef METTLE
#error Metal C not supported
#endif // METTLE

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
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
#include "jwtsecret.h"

static void receiveResponse(HttpClientContext *httpClientContext, HttpClientSession *session, int *statusOut);
static char *doRequest(HttpClientSettings *clientSettings, TlsEnvironment *tlsEnv, char *path, int *statusOut);

char *obtainJwtSecret(JwtSecretSettings *settings) {
  int status = 0;
  if (!settings) {
    zowelog(NULL, LOG_COMP_ID_JWT_SECRET, ZOWE_LOG_DEBUG, "jwt settings are NULL\n");
    return NULL;
  }
  HttpClientSettings clientSettings = {0};
  clientSettings.host = settings->host;
  clientSettings.port = settings->port;
  clientSettings.recvTimeoutSeconds = (settings->timeoutSeconds > 0) ? settings->timeoutSeconds : 10;

  char *secret = doRequest(&clientSettings, settings->tlsEnv, settings->path, &status);
  if (status) {
    zowelog(NULL, LOG_COMP_ID_JWT_SECRET, ZOWE_LOG_DEBUG, "failed to obtain jwt secret, rc = %d\n", status);
    return NULL;
  }
  return secret; 
}

static char* doRequest(HttpClientSettings *clientSettings, TlsEnvironment *tlsEnv, char *path, int *statusOut) {
  int status = 0;
  HttpClientContext *httpClientContext = NULL;
  HttpClientSession *session = NULL;
  LoggingContext *loggingContext = makeLoggingContext();
  char *textResponse = NULL;

  do {
    zowelog(NULL, LOG_COMP_ID_JWT_SECRET, ZOWE_LOG_DEBUG, "jwt secret request to https://%s:%d%s\n",
            clientSettings->host, clientSettings->port, path);
    status = httpClientContextInitSecure(clientSettings, loggingContext, tlsEnv, &httpClientContext);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_JWT_SECRET, ZOWE_LOG_DEBUG, "error in httpcb ctx init: %d\n", status);
      *statusOut = JWT_SECRET_STATUS_HTTP_ERROR;
      break;
    }
    status = httpClientSessionInit(httpClientContext, &session);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_JWT_SECRET, ZOWE_LOG_DEBUG, "error initing session: %d\n", status);
      *statusOut = JWT_SECRET_STATUS_HTTP_ERROR;
      break;
    }
    status = httpClientSessionStageRequest(httpClientContext, session, "GET", path, NULL, NULL, NULL, 0);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_JWT_SECRET, ZOWE_LOG_DEBUG, "error staging request: %d\n", status);
      *statusOut = JWT_SECRET_STATUS_HTTP_ERROR;
      break;
    }
    requestStringHeader(session->request, TRUE, "Content-type", "text/plain");
    status = httpClientSessionSend(httpClientContext, session);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_JWT_SECRET, ZOWE_LOG_DEBUG, "error sending request: %d\n", status);
      *statusOut = JWT_SECRET_STATUS_HTTP_ERROR;
      break;
    }
    receiveResponse(httpClientContext, session, &status);
    if (status) {
      *statusOut = status;
      break;
    }
    int statusCode = session->response->statusCode;
    if (statusCode != 200) {
      zowelog(NULL, LOG_COMP_ID_JWT_SECRET, ZOWE_LOG_DEBUG, "HTTP status %d\n", statusCode);
      *statusOut = JWT_SECRET_STATUS_RESPONSE_ERROR;
      break;
    }
    textResponse = session->response->body;
  } while (0);
  if (session) {
    httpClientSessionDestroy(session);
  }
  if (httpClientContext) {
    httpClientContextDestroy(httpClientContext);
  }  
  return textResponse;
}

static void receiveResponse(HttpClientContext *httpClientContext, HttpClientSession *session, int *statusOut) {
  bool done = false;
  ShortLivedHeap *slh = session->slh;
  while (!done) {
    int status = httpClientSessionReceiveNative(httpClientContext, session, 1024);
    if (status != 0) {
      zowelog(NULL, LOG_COMP_ID_JWT_SECRET, ZOWE_LOG_DEBUG, "error receiving response: %d\n", status);
      break;
    }
    if (session->response) {
      done = true;
      break;
    }
  }
  if (done) {
    int contentLength = session->response->contentLength;
    char *responseEbcdic = SLHAlloc(slh, contentLength + 1);
    if (responseEbcdic) {
      memset(responseEbcdic, '\0', contentLength + 1);
      memcpy(responseEbcdic, session->response->body, contentLength);
      __atoe_l(responseEbcdic, contentLength);
      zowelog(NULL, LOG_COMP_ID_JWT_SECRET, ZOWE_LOG_DEBUG, "successfully received response: %s\n", responseEbcdic);
    }
    *statusOut = JWT_SECRET_STATUS_OK;
  } else {
    *statusOut = JWT_SECRET_STATUS_RESPONSE_ERROR;
  }
}
