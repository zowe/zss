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
#include "storage.h"
#include "storageApiml.h"

#define CACHING_SERVICE_URI "/cachingservice/api/v1/cache"

#define STORAGE_STATUS_HTTP_ERROR           (STORAGE_STATUS_FIRST_CUSTOM_STATUS + 0)
#define STORAGE_STATUS_LOGIN_ERROR          (STORAGE_STATUS_FIRST_CUSTOM_STATUS + 1)
#define STORAGE_STATUS_INVALID_CREDENTIALS  (STORAGE_STATUS_FIRST_CUSTOM_STATUS + 2)
#define STORAGE_STATUS_RESPONSE_ERROR       (STORAGE_STATUS_FIRST_CUSTOM_STATUS + 3)
#define STORAGE_STATUS_JSON_RESPONSE_ERROR  (STORAGE_STATUS_FIRST_CUSTOM_STATUS + 4)

typedef struct ApimlStorage_tag {
  char *token;
  HttpClientSettings *clientSettings;
  TlsEnvironment *tlsEnv;
} ApimlStorage;

typedef struct JsonMemoryPrinter_tag JsonMemoryPrinter;

struct JsonMemoryPrinter_tag {
  char *buffer;
  int size;
  int capacity;
  jsonPrinter *jsonPrinter;
};

#define INITIAL_CAPACITY 512

static char *duplicateString(const char *str) {
  char *duplicate = safeMalloc(strlen(str) + 1, "duplicate string");
  if (duplicate) {
    strcpy(duplicate, str);
  }
  return duplicate;
}

static void convertToEbcdic(char *str, int len) {
  const char *trTable = getTranslationTable("iso88591_to_ibm1047");
  for (int i = 0; i < len; i++) {
    str[i] = trTable[str[i]];
  }
}

static void convertToAscii(char *str, int len) {
  const char *trTable = getTranslationTable("ibm1047_to_iso88591");
  for (int i = 0; i < len; i++) {
    str[i] = trTable[str[i]];
  }
}

static void printerGrowBuffer(JsonMemoryPrinter *printer) {
  int newCapacity = printer->capacity * 2;
  char *newBuffer = safeMalloc(newCapacity, "JsonMemoryPrinter Buffer");
  memcpy(newBuffer, printer->buffer, printer->size);
  safeFree(printer->buffer, printer->capacity);
  printer->buffer = newBuffer;
  printer->capacity = newCapacity;
}

static void jsonWriteCallback(jsonPrinter *p, char *data, int len) {
  JsonMemoryPrinter *printer = (JsonMemoryPrinter*)p->customObject;
  while (printer->capacity - printer->size < len) {
    printerGrowBuffer(printer);
  }
  memcpy(printer->buffer + printer->size, data, len);
  printer->size += len;
}

static void freeJsonMemoryPrinter(JsonMemoryPrinter *printer) {
  if (!printer) {
    return;
  }
  if (printer->buffer) {
    safeFree(printer->buffer, printer->capacity);
  }
  if (printer->jsonPrinter) {
    freeJsonPrinter(printer->jsonPrinter);
  }
  memset(printer, 0, sizeof(*printer));
  safeFree((char*)printer, sizeof(*printer));
}

static JsonMemoryPrinter *makeJsonMemoryPrinter() {
  JsonMemoryPrinter *printer = (JsonMemoryPrinter*)safeMalloc(sizeof(*printer), "JsonMemoryPrinter");
  if (!printer) {
    return NULL;
  }
  printer->capacity = INITIAL_CAPACITY;
  printer->buffer = safeMalloc(printer->capacity, "JsonMemoryPrinter Buffer");
  printer->size = 0;
  if (!printer->buffer) {
    freeJsonMemoryPrinter(printer);
    return NULL;
  }
  printer->jsonPrinter = makeCustomJsonPrinter(jsonWriteCallback, printer);
  if (!printer->jsonPrinter) {
    freeJsonMemoryPrinter(printer);
    return NULL;
  }
  return printer;
}

static void resetJsonMemoryPrinter(JsonMemoryPrinter *printer) {
  memset(printer->buffer, 0, printer->capacity);
  printer->size = 0;
  freeJsonPrinter(printer->jsonPrinter);
  printer->jsonPrinter = makeCustomJsonPrinter(jsonWriteCallback, printer);
}

char *jsonMemoryPrinterGetOutput(JsonMemoryPrinter *printer) {
  char *output = safeMalloc(printer->size + 1, "JsonMemoryPrinter output");
  if (output) {
    memcpy(output, printer->buffer, printer->size);
  }
  return output;
}

static char *extractTokenFromCookie(const char *cookie) {
  const char *name = "apimlAuthenticationToken=";
  int len = strlen(name);
  char *place = strstr(cookie, name);
  if (!place) {
    return NULL;
  }
  char *end = strchr(place + len, ';');
  if (!end) {
    return NULL;
  }
  char *token = safeMalloc(end - place + 1, "APIML token");
  memcpy(token, place, end - place);
  return token;
}

static char *extractTokenFromHeaders(HttpHeader *headers) {
  char *token = NULL;
  while (headers) {
    if (0 == strcmp(headers->nativeName, "set-cookie")) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "header '%s' -> '%s'\n", headers->nativeName, headers->nativeValue);
      token = extractTokenFromCookie(headers->nativeValue);
      if (token) {
        zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "token found '%s'\n", token);
        break;
      } else {
        zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "oops, token not found\n");
      }
    }
    headers = headers->next;
  }
  return token;
}

static void receiveResponse(HttpClientContext *httpClientContext, HttpClientSession *session, int *statusOut) {
  bool success = false;
  ShortLivedHeap *slh = session->slh;
  while (!success) {
    int status = httpClientSessionReceiveNative(httpClientContext, session, 1024);
    if (status != 0) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error receiving response: %d\n", status);
      break;
    }
    if (session->response) {
      success = true;
      break;
    }
  }
  if (success) {
    int contentLength = session->response->contentLength;
    char *responseEbcdic = SLHAlloc(slh, contentLength + 1);
    memset(responseEbcdic, '\0', contentLength + 1);
    memcpy(responseEbcdic, session->response->body, contentLength);
    convertToEbcdic(responseEbcdic, contentLength);
    zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "successfully received response(EBCDIC): %s\n", responseEbcdic);
    *statusOut = STORAGE_STATUS_OK;
  } else {
    *statusOut = STORAGE_STATUS_RESPONSE_ERROR;
  }
}

static Json *receiveJsonResponse(HttpClientContext *httpClientContext, HttpClientSession *session, int *statusOut) {
  int status = 0;
  receiveResponse(httpClientContext, session, &status);
  if (status != STORAGE_STATUS_OK) {
    *statusOut = status;
    return NULL;
  }
  char errorBuf[1024];
  ShortLivedHeap *slh = session->slh;
  char *response = session->response->body;
  int contentLength = session->response->contentLength;
  Json *json = jsonParseUnterminatedUtf8String(slh, CCSID_IBM1047, response, contentLength, errorBuf, sizeof(errorBuf));
  if (!json) {
    zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error parsing JSON response: %s\n", errorBuf);
    *statusOut = STORAGE_STATUS_JSON_RESPONSE_ERROR;
    return NULL;
  }
  *statusOut = STORAGE_STATUS_OK;
  return json;
}

static char *apimlCreateLoginRequestBody(const char *username, const char *password) {
  JsonMemoryPrinter *printer = makeJsonMemoryPrinter();
  jsonPrinter *p = printer->jsonPrinter;
  jsonStart(p);
  jsonAddString(p, "username", (char *)username);
  jsonAddString(p, "password", (char *)password);
  jsonEnd(p);
  char *body = jsonMemoryPrinterGetOutput(printer);
  freeJsonMemoryPrinter(printer);
  int bodyLen = strlen(body);
  convertToAscii(body, bodyLen);
  return body;
}

static void apimlLogin(ApimlStorage *storage, const char *username, const char *password, int *statusOut) {
  zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "[*] about to login with %s:%s\n", username, password);
  int status = 0;
  int statusCode = 0;
  TlsEnvironment *tlsEnv = storage->tlsEnv;
  HttpClientSettings *clientSettings = storage->clientSettings;
  HttpClientContext *httpClientContext = NULL;
  HttpClientSession *session = NULL;
  LoggingContext *loggingContext = makeLoggingContext();
  char *token = NULL;
  char *path = "/api/v1/apicatalog/auth/login";
  char *body = apimlCreateLoginRequestBody(username, password);
  int bodyLen = strlen(body);

  do {
    status = httpClientContextInitSecure(clientSettings, loggingContext, tlsEnv, &httpClientContext);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error in httpcb ctx init: %d\n", status);
      break;
    }
    status = httpClientSessionInit(httpClientContext, &session);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error initing session: %d\n", status);
      break;
    }
    status = httpClientSessionStageRequest(httpClientContext, session, "POST", path, NULL, NULL, body, bodyLen);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error staging request: %d\n", status);
      break;
    }
    requestStringHeader(session->request, TRUE, "Content-type", "application/json");
    status = httpClientSessionSend(httpClientContext, session);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error sending request: %d\n", status);
      break;
    }
    safeFree(body, bodyLen + 1);
    receiveResponse(httpClientContext, session, &status);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error receiving response: %d\n", status);
      break;
    }
    statusCode = session->response->statusCode;
    if (statusCode != HTTP_STATUS_OK && statusCode != HTTP_STATUS_NO_CONTENT) {
      status = STORAGE_STATUS_INVALID_CREDENTIALS;
      break;
    }
    token = extractTokenFromHeaders(session->response->headers);
    if (token) {
      *statusOut = STORAGE_STATUS_OK;
      storage->token = token;
    } else {
      *statusOut = STORAGE_STATUS_LOGIN_ERROR;
    }
  } while (0);
  zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "login status: %d, http status: %d\n", status, statusCode);
  if (session) {
    httpClientSessionDestroy(session);
  }
  if (httpClientContext) {
    httpClientContextDestroy(httpClientContext);
  }
}

static char *apimlCreateCachingServiceRequestBody(const char *key, const char *value) {
  JsonMemoryPrinter *printer = makeJsonMemoryPrinter();
  jsonPrinter *p = printer->jsonPrinter;
  jsonStart(p);
  jsonAddString(p, "key", (char*)key);
  jsonAddString(p, "value", (char*)value);
  jsonEnd(p);
  char *body = jsonMemoryPrinterGetOutput(printer);
  int bodyLen = strlen(body);
  freeJsonMemoryPrinter(printer);
  convertToAscii(body, bodyLen);
  return body;
} 

#define OP_CREATE 1
#define OP_CHANGE 2
static void createOrChange(ApimlStorage *storage, int op, const char *key, const char *value, int *statusOut) {
  zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "[*] about to createOrChange [%s]:%s\n", key, value);
  int status = 0;
  TlsEnvironment *tlsEnv = storage->tlsEnv;
  HttpClientSettings *clientSettings = storage->clientSettings;
  HttpClientContext *httpClientContext = NULL;
  HttpClientSession *session = NULL;
  LoggingContext *loggingContext = makeLoggingContext();
  char *path = CACHING_SERVICE_URI;
  char *body = apimlCreateCachingServiceRequestBody(key, value);
  int bodyLen = strlen(body);

  do {
    status = httpClientContextInitSecure(clientSettings, loggingContext, tlsEnv, &httpClientContext);

    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error in httpcb ctx init: %d\n", status);
      break;
    }
    status = httpClientSessionInit(httpClientContext, &session);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error initing session: %d\n", status);
      break;
    }
    char *method = op == OP_CHANGE ? "PUT" : "POST";
    status = httpClientSessionStageRequest(httpClientContext, session, method, path, NULL, NULL, body, bodyLen);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error staging request: %d\n", status);
      break;
    }
    requestStringHeader(session->request, TRUE, "Content-type", "application/json");
    requestStringHeader(session->request, TRUE, "Cookie", storage->token);
    status = httpClientSessionSend(httpClientContext, session);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error sending request: %d\n", status);
      break;
    }
    receiveResponse(httpClientContext, session, &status);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error receiving response: %d\n", status);
      break;
    }
    int statusCode = session->response->statusCode;
    if (statusCode >= 200 && statusCode < 300) {
      *statusOut = STORAGE_STATUS_OK;
    } else if (statusCode == 404) {
      *statusOut = STORAGE_STATUS_KEY_NOT_FOUND;
    } else {
      *statusOut = STORAGE_STATUS_HTTP_ERROR;
    }
  } while (0);
  safeFree(body, bodyLen + 1);
  zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "http response status %d\n", session->response->statusCode);

  if (session) {
    httpClientSessionDestroy(session);
  }
  if (httpClientContext) {
    httpClientContextDestroy(httpClientContext);
  }
}

static char *apimlStorageGetString(ApimlStorage *storage, const char *key, int *statusOut) {
  zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "[*] about to get [%s]\n", key);
  int status = 0;
  TlsEnvironment *tlsEnv = storage->tlsEnv;
  HttpClientSettings *clientSettings = storage->clientSettings;
  HttpClientContext *httpClientContext = NULL;
  HttpClientSession *session = NULL;
  LoggingContext *loggingContext = makeLoggingContext();
  char *token = NULL;
  char *value = NULL;
  char path[2048] = {0};
  snprintf (path, sizeof(path), "%s/%s", CACHING_SERVICE_URI, key);

  do {
    status = httpClientContextInitSecure(clientSettings, loggingContext, tlsEnv, &httpClientContext);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error in httpcb ctx init: %d\n", status);
      break;
    }
    status = httpClientSessionInit(httpClientContext, &session);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error initing session: %d\n", status);
      break;
    }
    status = httpClientSessionStageRequest(httpClientContext, session, "GET", path, NULL, NULL, 0, 0);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error staging request: %d\n", status);
      break;
    }
    requestStringHeader(session->request, TRUE, "Content-type", "application/json");
    requestStringHeader(session->request, TRUE, "Cookie", storage->token);
    status = httpClientSessionSend(httpClientContext, session);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error sending request: %d\n", status);
      break;
    }
    Json *jsonResponse = receiveJsonResponse(httpClientContext, session, &status);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error receiving response: %d\n", status);
      break;
    }
    int statusCode = session->response->statusCode;
    if (statusCode >= 200 && statusCode < 300) {
      *statusOut = STORAGE_STATUS_OK;
    } else if (statusCode == 404) {
      *statusOut = STORAGE_STATUS_KEY_NOT_FOUND;
      break;
    } else {
      *statusOut = STORAGE_STATUS_HTTP_ERROR;
      break;
    }
    JsonObject *keyValue = jsonAsObject(jsonResponse);
    if (keyValue) {
      char *val = jsonObjectGetString(keyValue, "value");
      if (val) {
        value = duplicateString(val);
      }
    }
  } while (0);
  if (session) {
    httpClientSessionDestroy(session);
  }
  if (httpClientContext) {
    httpClientContextDestroy(httpClientContext);
  }
  return value;
}

static void apimlStorageRemove(ApimlStorage *storage, const char *key, int *statusOut) {
  zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "[*] about to delete [%s]\n", key);
  int status = 0;
  TlsEnvironment *tlsEnv = storage->tlsEnv;
  HttpClientSettings *clientSettings = storage->clientSettings;
  HttpClientContext *httpClientContext = NULL;
  HttpClientSession *session = NULL;
  LoggingContext *loggingContext = makeLoggingContext();
  char *token = NULL;
  char path[2048] = {0};
  snprintf(path, sizeof(path), "%s/%s", CACHING_SERVICE_URI, key);

  do {
    status = httpClientContextInitSecure(clientSettings, loggingContext, tlsEnv, &httpClientContext);

    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error in httpcb ctx init: %d\n", status);
      break;
    }
    status = httpClientSessionInit(httpClientContext, &session);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error initing session: %d\n", status);
      break;
    }
    status = httpClientSessionStageRequest(httpClientContext, session, "DELETE", path, NULL, NULL, NULL, 0);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error staging request: %d\n", status);
      break;
    }
    requestStringHeader(session->request, TRUE, "Content-type", "application/json");
    requestStringHeader(session->request, TRUE, "Cookie", storage->token);
    status = httpClientSessionSend(httpClientContext, session);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error sending request: %d\n", status);
      break;
    }
    receiveResponse(httpClientContext, session, &status);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error receiving response: %d\n", status);
      break;
    }
    int statusCode = session->response->statusCode;
    if (statusCode >= 200 && statusCode < 300) {
      *statusOut = STORAGE_STATUS_OK;
    } else if (statusCode == 404) {
      *statusOut = STORAGE_STATUS_KEY_NOT_FOUND;
    } else {
    *statusOut = STORAGE_STATUS_HTTP_ERROR;
    }
  } while (0);
  if (session) {
    httpClientSessionDestroy(session);
  }
  if (httpClientContext) {
    httpClientContextDestroy(httpClientContext);
  }
}

static void apimlStorageSetString(ApimlStorage *storage, const char *key, const char *value, int *statusOut) {
  int status = 0;
  createOrChange(storage, OP_CHANGE, key, value, &status);
  if (status == STORAGE_STATUS_KEY_NOT_FOUND) {
    createOrChange(storage, OP_CREATE, key, value, &status);
  }
  *statusOut = status;
}

static const char *MESSAGES[] = {
  [STORAGE_STATUS_HTTP_ERROR] = "HTTP error",
  [STORAGE_STATUS_LOGIN_ERROR] = "Login failed",
  [STORAGE_STATUS_INVALID_CREDENTIALS] = "Invalid credentials",
  [STORAGE_STATUS_RESPONSE_ERROR] = "Error receiving response",
  [STORAGE_STATUS_JSON_RESPONSE_ERROR] = "Error parsing JSON response",
};

#define MESSAGE_COUNT sizeof(MESSAGES)/sizeof(MESSAGES[0])

static const char *apimlStorageGetStrStatus(ApimlStorage *storage, int status) {
  int adjustedStatus = status - STORAGE_STATUS_FIRST_CUSTOM_STATUS;
  if (adjustedStatus >= MESSAGE_COUNT || adjustedStatus < 0) {
    return "Unknown status code";
  }
  const char *message = MESSAGES[adjustedStatus];
  if (!message) {
    return "Unknown status code";
  }
  return message;
}

Storage *makeApimlStorage(ApimlStorageSettings *settings) {
  Storage *storage = (Storage*)safeMalloc(sizeof(*storage), "Storage");
  if (!storage) {
    return NULL;
  }
  ApimlStorage *apimlStorage = (ApimlStorage*)safeMalloc(sizeof(*apimlStorage), "APIML Storage");
  if (!apimlStorage) {
    safeFree((char*)storage, sizeof(*storage));
    return NULL;
  }
  HttpClientSettings *clientSettings = (HttpClientSettings*)safeMalloc(sizeof(*clientSettings), "HTTP Client Settings");
  if (!clientSettings) {
    safeFree((char*)storage, sizeof(*storage));
    safeFree((char*)apimlStorage, sizeof(*apimlStorage));
    return NULL;
  }
  clientSettings->host = settings->host;
  clientSettings->port = settings->port;
  clientSettings->recvTimeoutSeconds = 10;

  TlsEnvironment *tlsEnv = NULL;
  int status = tlsInit(&tlsEnv, settings->tlsSettings);
  if (status) {
    zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "failed to init tls environment, rc=%d (%s)\n", status, tlsStrError(status));
    safeFree((char*)storage, sizeof(*storage));
    safeFree((char*)apimlStorage, sizeof(*apimlStorage));
    safeFree((char*)clientSettings, sizeof(*clientSettings));
    return NULL;
  }

  apimlStorage->clientSettings = clientSettings;
  apimlStorage->tlsEnv = tlsEnv;
  apimlStorage->token = settings->token;

  storage->userData = apimlStorage;
  storage->set = (StorageSet) apimlStorageSetString;
  storage->get = (StorageGet) apimlStorageGetString;
  storage->remove = (StorageRemove) apimlStorageRemove;
  storage->strStatus = (StorageGetStrStatus) apimlStorageGetStrStatus;
  return storage;
}

#ifdef _TEST_APIML_STORAGE

/*
c89 \
  -D_TEST_STORAGE=1 \
  -D_TEST_APIML_STORAGE=1 \
  -D_XOPEN_SOURCE=600 \
  -D_OPEN_THREADS=1 \
  -DUSE_ZOWE_TLS=1 \
  -DHTTPSERVER_BPX_IMPERSONATION=1 \
  -DAPF_AUTHORIZED=0 \
  -Wc,dll,langlvl\(extc99\),gonum,goff,hgpr,roconst,ASM,asmlib\('CEE.SCEEMAC','SYS1.MACLIB','SYS1.MODGEN'\) \
  -Wc,agg,exp,list\(\),so\(\),off,xref \
  -Wl,ac=1,dll \
  -I../h \
  -I../deps/zowe-common-c/h \
  -I/usr/lpp/gskssl/include \
  -o test_apiml_storage \
  storageApiml.c \
  ../deps/zowe-common-c/c/alloc.c \
  ../deps/zowe-common-c/c/bpxskt.c \
  ../deps/zowe-common-c/c/charsets.c \
  ../deps/zowe-common-c/c/collections.c \
  ../deps/zowe-common-c/c/crossmemory.c \
  ../deps/zowe-common-c/c/fdpoll.c \
  ../deps/zowe-common-c/c/http.c \
  ../deps/zowe-common-c/c/httpclient.c \
  ../deps/zowe-common-c/c/json.c \
  ../deps/zowe-common-c/c/le.c \
  ../deps/zowe-common-c/c/logging.c \
  ../deps/zowe-common-c/c/recovery.c \
  ../deps/zowe-common-c/c/scheduling.c \
  ../deps/zowe-common-c/c/socketmgmt.c \
  ../deps/zowe-common-c/c/storage.c \
  ../deps/zowe-common-c/c/timeutls.c \
  ../deps/zowe-common-c/c/tls.c \
  ../deps/zowe-common-c/c/utils.c \
  ../deps/zowe-common-c/c/xlate.c \
  ../deps/zowe-common-c/c/zos.c \
  ../deps/zowe-common-c/c/zosfile.c \
  ../deps/zowe-common-c/c/zvt.c \
  /usr/lpp/gskssl/lib/GSKSSL.x

Run:
  ./test_apiml_storage localhost 10010 ./key.kdb ./key.sth client USER validPassword
*/

int main(int argc, char *argv[]) {
  int status = 0;
  if (argc != 8) {
    printf("Usage: %s <apiml-gateway-host> <apiml-gateway-host> <kdb-file> <stash-file> <key-label> <user> <password>\n", argv[0]);
    printf("For example: %s localhost 10010 ./key.kdb ./key.sth client USER validPassword\n");
    return 1;
  }
  char *host = argv[1];
  int port = atoi(argv[2]);
  char *keyring = argv[3];
  char *stash = argv[4];
  char *label = argv[5];
  char *user = argv[6];
  char *password = argv[7];

  LoggingContext *context = makeLoggingContext();
  logConfigureStandardDestinations(context);
  logConfigureComponent(context, LOG_COMP_ID_APIML_STORAGE, "APIML Storage",  LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_DEBUG);
  TlsSettings tlsSettings = {
    .keyring = keyring,
    .stash = stash,
    .label = label
  };
  ApimlStorageSettings settings = {
    .host = host,
    .port = port,
    .tlsSettings = &tlsSettings,
    .token = NULL // obtain the token later using apimlLogin()
  };
  Storage *storage = makeApimlStorage(&settings);
  if (!storage) {
    printf("unable to make APIML storage\n");
    return 1;
  }
  ApimlStorage *apimlStorage = storage->userData;
  printf("apimlStorage host %s port %d, keyring %s, stash %s, label %s, user %s\n",
          host, port, keyring, stash, label, user);
  apimlLogin(apimlStorage, user, password, &status);
  if (status == STORAGE_STATUS_OK) {
    printf("login ok, token '%s'\n", apimlStorage->token);
  } else {
    printf("oops, login failed, status %d - %s\n", status, apimlStorageGetStrStatus(apimlStorage, status));
    return 1;
  }
  testStorage(storage);
  printf("test complete\n");
}

#endif // _TEST_APIML_STORAGE

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/