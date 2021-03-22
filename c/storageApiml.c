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
#define STORAGE_STATUS_RESPONSE_ERROR       (STORAGE_STATUS_FIRST_CUSTOM_STATUS + 1)
#define STORAGE_STATUS_JSON_RESPONSE_ERROR  (STORAGE_STATUS_FIRST_CUSTOM_STATUS + 2)
#define STORAGE_STATUS_ALLOC_ERROR          (STORAGE_STATUS_FIRST_CUSTOM_STATUS + 3)
#define STORAGE_STATUS_INVALID_KV_RESPONSE  (STORAGE_STATUS_FIRST_CUSTOM_STATUS + 4)
#define STORAGE_STATUS_INVALID_CLIENT_CERT  (STORAGE_STATUS_FIRST_CUSTOM_STATUS + 5)

typedef struct {
  HttpClientSettings *clientSettings;
  TlsEnvironment *tlsEnv;
} ApimlStorage;

typedef struct {
  char *buffer;
  int size;
  int capacity;
  bool error;
  jsonPrinter *jsonPrinter;
} JsonMemoryPrinter;

typedef struct {
  char *textResponse;
  Json *jsonResponse;
  HttpHeader *headers;
  int statusCode;
  HttpClientContext *_httpClientContext;
  HttpClientSession *_session;
} ApimlResponse;

typedef struct  {
  char *method;
  char *path;
  char *body;
  int bodyLen;
  bool parseJson;
} ApimlRequest;

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

static bool printerGrowBuffer(JsonMemoryPrinter *printer) {
  int newCapacity = printer->capacity * 2;
  char *newBuffer = safeMalloc(newCapacity, "JsonMemoryPrinter Buffer");
  if (!newBuffer) {
    return false;
  }
  memcpy(newBuffer, printer->buffer, printer->size);
  safeFree(printer->buffer, printer->capacity);
  printer->buffer = newBuffer;
  printer->capacity = newCapacity;
}

static void jsonWriteCallback(jsonPrinter *p, char *data, int len) {
  JsonMemoryPrinter *printer = (JsonMemoryPrinter*)p->customObject;
  if (printer->error) {
    return;
  }
  while (printer->capacity - printer->size < len) {
    if (!printerGrowBuffer(printer)) {
      printer->error = true;
      return;
    }
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
  printer->error = false;
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

static char *jsonMemoryPrinterGetOutput(JsonMemoryPrinter *printer) {
  if (printer->error) {
    return NULL;
  }
  char *output = safeMalloc(printer->size + 1, "JsonMemoryPrinter output");
  if (output) {
    memcpy(output, printer->buffer, printer->size);
  }
  return output;
}

static void receiveResponse(HttpClientContext *httpClientContext, HttpClientSession *session, int *statusOut) {
  bool done = false;
  ShortLivedHeap *slh = session->slh;
  while (!done) {
    int status = httpClientSessionReceiveNative(httpClientContext, session, 1024);
    if (status != 0) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error receiving response: %d\n", status);
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
    if (!responseEbcdic) {
      *statusOut = STORAGE_STATUS_ALLOC_ERROR;
      return;
    }
    memset(responseEbcdic, '\0', contentLength + 1);
    memcpy(responseEbcdic, session->response->body, contentLength);
    convertToEbcdic(responseEbcdic, contentLength);
    zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "successfully received response: %s\n", responseEbcdic);
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

static void freeApimlResponse(ApimlResponse *response) {
  if (!response) {
    return;
  }
  if (response->_session) {
    httpClientSessionDestroy(response->_session);
  }
  if (response->_httpClientContext) {
    httpClientContextDestroy(response->_httpClientContext);
  }
  safeFree((char*)response, sizeof(*response));
}

static int transformHttpStatus(int httpStatus) {
  switch (httpStatus) {
    case HTTP_STATUS_NOT_FOUND:
      return STORAGE_STATUS_KEY_NOT_FOUND;
    case HTTP_STATUS_FORBIDDEN:
      return STORAGE_STATUS_INVALID_CLIENT_CERT;
    case HTTP_STATUS_OK:
      return STORAGE_STATUS_OK;
    case HTTP_STATUS_NO_CONTENT:
      return STORAGE_STATUS_OK;
    case HTTP_STATUS_CREATED:
      return STORAGE_STATUS_OK;
    default:
      return STORAGE_STATUS_HTTP_ERROR;
  }
}

static ApimlResponse *apimlDoRequest(ApimlStorage *storage, ApimlRequest *request, int *statusOut) {
  int status = 0;
  int statusCode = 0;
  TlsEnvironment *tlsEnv = storage->tlsEnv;
  HttpClientSettings *clientSettings = storage->clientSettings;
  HttpClientContext *httpClientContext = NULL;
  HttpClientSession *session = NULL;
  LoggingContext *loggingContext = makeLoggingContext();
  char *path = request->path;
  char *body = request->body;
  int bodyLen = request->bodyLen;
  Json *jsonResponse = NULL;
  zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "apiml request %s %s\n", request->method, request->path);

  do {
    status = httpClientContextInitSecure(clientSettings, loggingContext, tlsEnv, &httpClientContext);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error in httpcb ctx init: %d\n", status);
      *statusOut = STORAGE_STATUS_HTTP_ERROR;
      break;
    }
    status = httpClientSessionInit(httpClientContext, &session);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error initing session: %d\n", status);
      *statusOut = STORAGE_STATUS_HTTP_ERROR;
      break;
    }
    status = httpClientSessionStageRequest(httpClientContext, session, request->method, path, NULL, NULL, body, bodyLen);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error staging request: %d\n", status);
      *statusOut = STORAGE_STATUS_HTTP_ERROR;
      break;
    }
    requestStringHeader(session->request, TRUE, "Content-type", "application/json");
    status = httpClientSessionSend(httpClientContext, session);
    if (status) {
      zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "error sending request: %d\n", status);
      *statusOut = STORAGE_STATUS_HTTP_ERROR;
      break;
    }
    if (request->parseJson) {
      jsonResponse = receiveJsonResponse(httpClientContext, session, &status);
    } else {
      receiveResponse(httpClientContext, session, &status);
    }
    if (status) {
      *statusOut = status;
      break;
    }
  } while (0);
  if (status) {
    if (session) {
      httpClientSessionDestroy(session);
    }
    if (httpClientContext) {
      httpClientContextDestroy(httpClientContext);
    }    
    return NULL;
  }
  ApimlResponse *response = (ApimlResponse*)safeMalloc(sizeof(*response), "APIML Response");
  if (response) {
    response->_httpClientContext = httpClientContext;
    response->_session = session;
    response->jsonResponse = jsonResponse;
    response->textResponse = session->response->body;
    response->headers = session->response->headers;
    response->statusCode = session->response->statusCode;
  }
  return response;
}

static char *apimlCreateCachingServiceRequestBody(const char *key, const char *value) {
  JsonMemoryPrinter *printer = makeJsonMemoryPrinter();
  if (!printer) {
    return NULL;
  }
  jsonPrinter *p = printer->jsonPrinter;
  jsonStart(p);
  jsonAddString(p, "key", (char*)key);
  jsonAddString(p, "value", (char*)value);
  jsonEnd(p);
  char *body = jsonMemoryPrinterGetOutput(printer);
  freeJsonMemoryPrinter(printer);
  if (body) {
    int bodyLen = strlen(body);
    convertToAscii(body, bodyLen);
  }
  return body;
} 

#define OP_CREATE 1
#define OP_CHANGE 2
static void createOrChange(ApimlStorage *storage, int op, const char *key, const char *value, int *statusOut) {
  zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "[+] about to createOrChange [%s]:%s\n", key, value);
  int status = 0;
  char *path = CACHING_SERVICE_URI;
  char *method = (op == OP_CHANGE ? "PUT" : "POST");
  char *body = apimlCreateCachingServiceRequestBody(key, value);
  if (!body) {
    *statusOut = STORAGE_STATUS_ALLOC_ERROR;
    return;
  }
  int bodyLen = strlen(body);
  ApimlRequest request = {
    .method = method,
    .path = CACHING_SERVICE_URI,
    .body = body,
    .bodyLen = bodyLen,
    .parseJson = false
  };
  ApimlResponse *response = apimlDoRequest(storage, &request, &status);
  safeFree(body, bodyLen + 1);
  if (status) {
    *statusOut = status;
    return;
  }
  int statusCode = response->statusCode;
  *statusOut = transformHttpStatus(statusCode);
  freeApimlResponse(response);
  zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "http response status %d\n", statusCode);
}

static char *getValue(Json *jsonResponse, int *statusOut) {
  JsonObject *keyValue = jsonAsObject(jsonResponse);
  if (!keyValue) {
    *statusOut = STORAGE_STATUS_INVALID_KV_RESPONSE;
    return NULL;
  }
  char *val = jsonObjectGetString(keyValue, "value");
  if (!val) {
    *statusOut = STORAGE_STATUS_INVALID_KV_RESPONSE;
    return NULL;
  }
  char *value = duplicateString(val);
  if (!value) {
    *statusOut = STORAGE_STATUS_ALLOC_ERROR;
    return NULL;
  }
  *statusOut = STORAGE_STATUS_OK;
  return value;
}

static char *apimlStorageGetString(ApimlStorage *storage, const char *key, int *statusOut) {
  zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "[+] about to get [%s]\n", key);
  int status = 0;
  int statusCode = 0;
  char *value = NULL;
  char path[2048] = {0};
  snprintf (path, sizeof(path), "%s/%s", CACHING_SERVICE_URI, key);
  
  ApimlRequest request = {
    .method = "GET",
    .path = path,
    .body = NULL,
    .bodyLen = 0,
    .parseJson = true
  };
  ApimlResponse *response = apimlDoRequest(storage, &request, &status);
  if (status) {
    *statusOut = status;
    return NULL;
  }
  do {
    statusCode = response->statusCode;
    status = transformHttpStatus(statusCode);
    if (status) {
      *statusOut = status;
      break;
    }
    value = getValue(response->jsonResponse, statusOut);
  } while (0);
  freeApimlResponse(response);
  zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "http response status %d\n", statusCode);
  return value;
}

static void apimlStorageRemove(ApimlStorage *storage, const char *key, int *statusOut) {
  zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "[+] about to remove [%s]\n", key);
  int status = 0;
  char path[2048] = {0};
  snprintf(path, sizeof(path), "%s/%s", CACHING_SERVICE_URI, key);
  ApimlRequest request = {
    .method = "DELETE",
    .path = path,
    .body = NULL,
    .bodyLen = 0,
    .parseJson = false
  };
  ApimlResponse *response = apimlDoRequest(storage, &request, &status);
  if (status) {
    *statusOut = status;
    return;
  }
  int statusCode = response->statusCode;
  *statusOut = transformHttpStatus(statusCode);
  freeApimlResponse(response);
  zowelog(NULL, LOG_COMP_ID_APIML_STORAGE, ZOWE_LOG_DEBUG, "http response status %d\n", statusCode);
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
  [STORAGE_STATUS_HTTP_ERROR] = "Failed to send HTTP request",
  [STORAGE_STATUS_RESPONSE_ERROR] = "Error receiving response",
  [STORAGE_STATUS_JSON_RESPONSE_ERROR] = "Error parsing JSON response",
  [STORAGE_STATUS_ALLOC_ERROR] = "Failed to allocate memory",
  [STORAGE_STATUS_INVALID_KV_RESPONSE] = "Invalid key/value response",
  [STORAGE_STATUS_INVALID_CLIENT_CERT] = "Invalid client certificate",
};

#define MESSAGE_COUNT sizeof(MESSAGES)/sizeof(MESSAGES[0])

static const char *apimlStorageGetStrStatus(ApimlStorage *storage, int status) {
  if (status >= MESSAGE_COUNT || status < STORAGE_STATUS_FIRST_CUSTOM_STATUS) {
    return "Unknown status code";
  }
  const char *message = MESSAGES[status];
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
  ./test_apiml_storage <gateway-host> 10010 ./client-certs.p12 password user */

int main(int argc, char *argv[]) {
  int status = 0;
  if (argc != 6) {
    printf("Usage: %s <apiml-gateway-host> <apiml-gateway-host> <keyring> <password> <label>\n", argv[0]);
    printf("For example: %s localhost 10010 ./client-certs.p12 password user\n");
    return 1;
  }
  char *host = argv[1];
  int port = atoi(argv[2]);
  char *keyring = argv[3];
  char *password = argv[4];
  char *label = argv[5];

  LoggingContext *context = makeLoggingContext();
  logConfigureStandardDestinations(context);
  logConfigureComponent(context, LOG_COMP_ID_APIML_STORAGE, "APIML Storage",  LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_DEBUG);
  logConfigureComponent(context, LOG_COMP_HTTPCLIENT, "HTTP Client", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_DEBUG2);
  TlsSettings tlsSettings = {
    .keyring = keyring,
    .password = password,
    .label = label
  };
  ApimlStorageSettings settings = {
    .host = host,
    .port = port,
    .tlsSettings = &tlsSettings,
  };
  Storage *storage = makeApimlStorage(&settings);
  if (!storage) {
    printf("[-] unable to make APIML storage\n");
    return 1;
  }
  ApimlStorage *apimlStorage = storage->userData;
  printf("apimlStorage host %s port %d, keyring %s, password %s, label %s\n",
          host, port, keyring, password, label);
  testStorage(storage);
  printf("[!] test complete successfully\n");
}

#endif // _TEST_APIML_STORAGE

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/