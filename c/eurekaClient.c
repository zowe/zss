/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * eurekaClient.c
 *
 * Implements Eureka (API ML Discovery) client registration for ZSS.
 *
 * Registration flow (runs inside a dedicated RLETask):
 *
 *   1. POST  /eureka/apps/<SERVICEID>  --  initial service registration
 *   2. PUT   /eureka/apps/<SERVICEID>/<instanceId>  --  heartbeat every 30 s
 *
 * On receipt of HTTP 404 from a heartbeat the task automatically re-registers.
 * On any other network error the task sleeps retryIntervalSecs and tries again.
 *
 * All strings sent over HTTP are native EBCDIC; httpClientSessionSetRequestBody
 * is called with bodyIsNativeText=TRUE so that the library translates them to
 * ASCII before writing to the socket.
 */

#ifdef METTLE
#error Metal C is not supported
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "bpxnet.h"
#include "collections.h"
#include "socketmgmt.h"
#include "le.h"
#include "logging.h"
#include "scheduling.h"
#include "json.h"
#include "httpclient.h"
#include "configmgr.h"
#ifdef USE_ZOWE_TLS
#include "tls.h"
#endif
#include "zssLogging.h"
#include "zss.h"
#include "eurekaClient.h"

/* -------------------------------------------------------------------------
 * IPv6 helpers
 * ------------------------------------------------------------------------- */

/* Returns true when addr is an IPv6 address (contains a colon). */
static bool isIPv6Address(const char *addr) {
  return addr && strchr(addr, ':') != NULL;
}

/* Returns a safeMalloc'd copy of addr with every ':' replaced by '_'.
 * Used to produce a legal Eureka instanceId component from an IPv6 address. */
static char *ipv6ColonsToUnderscores(const char *addr) {
  int len = strlen(addr);
  char *dst = (char *)safeMalloc(len + 1, "eureka_ipv6id");
  if (!dst) return NULL;
  for (int i = 0; i <= len; i++) {
    dst[i] = (addr[i] == ':') ? '_' : addr[i];
  }
  return dst;
}

/* -------------------------------------------------------------------------
 * Internal context held by the RLETask
 * ------------------------------------------------------------------------- */
typedef struct EurekaClientContext_tag {
  EurekaClientSettings *settings;
} EurekaClientContext;

/* -------------------------------------------------------------------------
 * Return-code constants used internally
 * ------------------------------------------------------------------------- */
#define EUREKA_RC_OK               0
#define EUREKA_RC_CTX_ERROR        1
#define EUREKA_RC_SESSION_ERROR    2
#define EUREKA_RC_STAGE_ERROR      3
#define EUREKA_RC_SEND_ERROR       4
#define EUREKA_RC_RECV_ERROR       5
#define EUREKA_RC_HTTP_ERROR       6   /* unexpected HTTP status code        */
#define EUREKA_RC_NOT_FOUND        7   /* HTTP 404 -- instance not registered */

/* -------------------------------------------------------------------------
 * Eureka REST API defaults
 * ------------------------------------------------------------------------- */
#define EUREKA_HEARTBEAT_INTERVAL_SECS  30
#define EUREKA_RETRY_INTERVAL_SECS      30
#define EUREKA_RECV_TIMEOUT_SECS        10
#define EUREKA_APPS_PATH_PREFIX         "/eureka/apps/"

/* Size of the static buffer used to build the JSON registration body.
 * Eureka bodies are typically well under 4 KB. */
#define EUREKA_BODY_BUFSIZE  4096

/* -------------------------------------------------------------------------
 * Forward declarations
 * ------------------------------------------------------------------------- */
static int  eurekaClientTaskMain(RLETask *task);
static int  doRegistration(EurekaClientSettings *settings, int *httpStatusOut);
static int  doHeartbeat(EurekaClientSettings *settings, int *httpStatusOut);
static int  sendEurekaRequest(EurekaClientSettings *settings,
                              const char *method,
                              const char *urlPath,
                              const char *body,   /* NULL for heartbeat  */
                              int         bodyLen,
                              int        *httpStatusOut);
static int  buildBodyPath(EurekaClientSettings *settings,
                          char *buf, int bufLen);
static int  buildInstancePath(EurekaClientSettings *settings,
                              char *buf, int bufLen);
static int  buildRegistrationBody(EurekaClientSettings *settings,
                                  char *buf, int bufLen);


/* =========================================================================
 * Public API
 * ========================================================================= */

/*
 * makeEurekaClientSettings
 *
 * Reads from the Zowe YAML:
 *   - zowe.externalDomains[0]         -> discoveryHost / hostName for ZSS
 *   - components.discovery.port       -> discoveryPort
 *   - zssPort (passed by caller)      -> port
 *
 * Constructs the registration URL fragments from these values.
 */
EurekaClientSettings *makeEurekaClientSettings(ShortLivedHeap  *slh,
                                               ConfigManager   *configmgr,
                                               const char      *zssAddress,
                                               int              zssPort,
                                               bool             zssSecure,
                                               const char      *zssVersion
#ifdef USE_ZOWE_TLS
                                               , TlsEnvironment *tlsEnv
#endif
                                               )
{
  /* ---- discovery or apiml enabled? -------------------------------- */
  bool discoveryEnabled = false;
  cfgGetBooleanC(configmgr, ZSS_CFGNAME, &discoveryEnabled,
                 3, "components", "discovery", "enabled");

  bool apimlEnabled = false;
  cfgGetBooleanC(configmgr, ZSS_CFGNAME, &apimlEnabled,
                 3, "components", "apiml", "enabled");

  if (!discoveryEnabled && !apimlEnabled) {
    zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_DEBUG,
            "Eureka registration skipped: neither components.discovery.enabled "
            "nor components.apiml.enabled is true\n");
    return NULL;
  }
  zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_DEBUG,
          "Eureka registration enabled (discovery=%d, apiml=%d)\n",
          discoveryEnabled, apimlEnabled);

  /* ---- discovery port -------------------------------------------- */
  int discoveryPort = 0;
  int portStatus = cfgGetIntC(configmgr, ZSS_CFGNAME, &discoveryPort,
                              3, "components", "discovery", "port");
  if (portStatus != ZCFG_SUCCESS || discoveryPort <= 0) {
    zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_DEBUG,
            "Eureka registration skipped: components.discovery.port not found "
            "(status=%d, port=%d)\n", portStatus, discoveryPort);
    return NULL;
  }

  /* ---- external hostname (used both as discovery host and as this
   *      service's advertised hostname)                              ---- */
  char *externalDomain = NULL;
  int hostStatus = cfgGetStringC(configmgr, ZSS_CFGNAME, &externalDomain,
                                 3, "zowe", "externalDomains", 0);
  if (hostStatus != ZCFG_SUCCESS || externalDomain == NULL) {
    zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_SEVERE,
            ZSS_LOG_EUREKA_NO_DOMAIN_MSG, hostStatus);
    return NULL;
  }

  /* ---- IPv6: prepare bracketed form for URLs and underscore form for instanceId ---- */
  bool ipv6 = isIPv6Address(externalDomain);

  /* hostForUrl: "[addr]" for IPv6, plain addr otherwise */
  char *hostForUrl;
  if (ipv6) {
    int bracketLen = strlen(externalDomain) + 3; /* '[' + addr + ']' + NUL */
    hostForUrl = (char *)safeMalloc(bracketLen, "eurekaHostForUrl");
    snprintf(hostForUrl, bracketLen, "[%s]", externalDomain);
  } else {
    hostForUrl = externalDomain;  /* no transformation needed for IPv4 or hostname */
  }

  /* hostForInstanceId: colons become underscores for IPv6             */
  char *hostForInstanceId = ipv6
      ? ipv6ColonsToUnderscores(externalDomain)
      : externalDomain;

  /* ---- allocate and populate settings ---- */
  EurekaClientSettings *eurekaSettings =
      (EurekaClientSettings *)safeMalloc(sizeof(EurekaClientSettings),
                                         "EurekaClientSettings");
  if (!eurekaSettings) {
    zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_SEVERE,
            ZSS_LOG_EUREKA_ALLOC_SETTINGS_MSG);
    return NULL;
  }
  memset(eurekaSettings, 0, sizeof(EurekaClientSettings));

  /* discovery coordinates */
  eurekaSettings->discoveryHost = externalDomain;
  eurekaSettings->discoveryPort = discoveryPort;

  /* service identity */
  eurekaSettings->serviceId  = "zss";
  eurekaSettings->hostName   = externalDomain;
  eurekaSettings->ipAddr     = zssAddress;
  eurekaSettings->port       = zssPort;
  eurekaSettings->securePortEnabled = zssSecure;

  /* instanceId: <host>:<serviceId>:<port>  (IPv6 colons -> underscores) */
  {
    int idLen = strlen(hostForInstanceId) + 1 /* : */ + 3 /* ZSS */ + 1 + 10 + 1;
    char *iid = (char *)safeMalloc(idLen, "EurekaInstanceId");
    snprintf(iid, idLen, "%s:ZSS:%d", hostForInstanceId, zssPort);
    eurekaSettings->instanceId = iid;
  }

  /* well-known URLs  (IPv6 host already bracketed in hostForUrl) */
  {
    const char *scheme = zssSecure ? "https" : "http";
    int urlBase = strlen(scheme) + 3 /* :// */ + strlen(hostForUrl) + 1 + 10 + 32;

    char *homeUrl    = (char *)safeMalloc(urlBase, "EurekaHomeUrl");
    char *statusUrl  = (char *)safeMalloc(urlBase, "EurekaStatusUrl");
    char *healthUrl  = (char *)safeMalloc(urlBase, "EurekaHealthUrl");

    snprintf(homeUrl,   urlBase, "%s://%s:%d/", scheme, hostForUrl, zssPort);
    snprintf(statusUrl, urlBase, "%s://%s:%d/info", scheme, hostForUrl, zssPort);
    snprintf(healthUrl, urlBase, "%s://%s:%d/health", scheme, hostForUrl, zssPort);

    eurekaSettings->homePageUrl    = homeUrl;
    eurekaSettings->statusPageUrl  = statusUrl;
    eurekaSettings->healthCheckUrl = healthUrl;
  }

  eurekaSettings->version = zssVersion;

  /* timing: read heartbeatIntervalSeconds from YAML, fall back to compile-time default */
  int heartbeatIntervalSeconds = EUREKA_HEARTBEAT_INTERVAL_SECS;
  cfgGetIntC(configmgr, ZSS_CFGNAME, &heartbeatIntervalSeconds,
             5, "components", "zss", "agent", "mediationLayer", "heartbeatIntervalSeconds");
  if (heartbeatIntervalSeconds <= 0) {
    heartbeatIntervalSeconds = EUREKA_HEARTBEAT_INTERVAL_SECS;
  }

  eurekaSettings->heartbeatIntervalSeconds = heartbeatIntervalSeconds;

  /* timing: read retryIntervalSeconds from YAML, fall back to compile-time default */
  int retryIntervalSeconds = EUREKA_RETRY_INTERVAL_SECS;
  cfgGetIntC(configmgr, ZSS_CFGNAME, &retryIntervalSeconds,
             5, "components", "zss", "agent", "mediationLayer", "retryIntervalSeconds");
  if (retryIntervalSeconds <= 0) {
    retryIntervalSeconds = EUREKA_RETRY_INTERVAL_SECS;
  }

  eurekaSettings->retryIntervalSeconds     = retryIntervalSeconds;
  eurekaSettings->maxRetries            = 0;   /* retry indefinitely */

#ifdef USE_ZOWE_TLS
  eurekaSettings->tlsEnv = tlsEnv;
#endif

  zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_INFO,
          ZSS_LOG_EUREKA_SETTINGS_MSG,
          eurekaSettings->discoveryHost, eurekaSettings->discoveryPort, eurekaSettings->instanceId);

  return eurekaSettings;
}


/*
 * startEurekaClient
 *
 * Spawns the background RLETask.  The task pointer (EurekaClientContext) is
 * attached via task->userPointer and is owned by the task.
 */
void startEurekaClient(HttpServer *server, EurekaClientSettings *settings) {
  if (!settings) {
    zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_DEBUG,
            "startEurekaClient: settings is NULL, skipping\n");
    return;
  }

  EurekaClientContext *ctx =
      (EurekaClientContext *)safeMalloc(sizeof(EurekaClientContext),
                                         "EurekaClientContext");
  if (!ctx) {
    zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_SEVERE,
            ZSS_LOG_EUREKA_ALLOC_CTX_MSG);
    return;
  }
  ctx->settings = settings;

  RLETask *task = makeRLETask(server->base->rleAnchor,
                              RLE_TASK_TCB_CAPABLE | RLE_TASK_DISPOSABLE,
                              eurekaClientTaskMain);
  if (!task) {
    zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_SEVERE,
            ZSS_LOG_EUREKA_CREATE_TASK_MSG);
    safeFree((char *)ctx, sizeof(EurekaClientContext));
    return;
  }

  task->userPointer = ctx;
  startRLETask(task, NULL);

  zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_INFO,
          ZSS_LOG_EUREKA_TASK_STARTED_MSG,
          settings->discoveryHost, settings->discoveryPort,
          settings->instanceId);
}


/* =========================================================================
 * RLETask entry point
 * ========================================================================= */

static int eurekaClientTaskMain(RLETask *task) {
  EurekaClientContext  *ctx      = (EurekaClientContext *)task->userPointer;
  EurekaClientSettings *settings = ctx->settings;

  const int heartbeatSecs = (settings->heartbeatIntervalSeconds > 0)
                            ? settings->heartbeatIntervalSeconds
                            : EUREKA_HEARTBEAT_INTERVAL_SECS;
  const int retrySecs     = (settings->retryIntervalSeconds > 0)
                            ? settings->retryIntervalSeconds
                            : EUREKA_RETRY_INTERVAL_SECS;
  const int warnEvery     = 10;   /* log a warning every N successive failures */

  bool registered = false;
  int  httpStatus = 0;
  int  rc         = EUREKA_RC_OK;
  int  failCount  = 0;

  zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_INFO,
          ZSS_LOG_EUREKA_REG_LOOP_MSG);

  /* -----------------------------------------------------------------------
   * Outer loop: keep trying to (re)register and heartbeat forever.
   * --------------------------------------------------------------------- */
  while (true) {

    /* ---- registration phase ---- */
    if (!registered) {
      rc = doRegistration(settings, &httpStatus);
      if (rc == EUREKA_RC_OK && (httpStatus == 204 || httpStatus == 200)) {
        zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_INFO,
                ZSS_LOG_EUREKA_REGISTERED_MSG, httpStatus);
        registered = true;
        failCount  = 0;
      } else {
        failCount++;
        if (failCount == 1 || failCount % warnEvery == 0) {
          zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_WARNING,
                  ZSS_LOG_EUREKA_REG_FAILED_MSG,
                  rc, httpStatus, retrySecs, failCount);
        }
        sleep(retrySecs);
        continue;
      }
    }

    /* ---- heartbeat phase ---- */
    sleep(heartbeatSecs);

    rc = doHeartbeat(settings, &httpStatus);

    if (rc == EUREKA_RC_OK && httpStatus == 200) {
      /* heartbeat accepted -- all is well */
      failCount = 0;
    } else if (rc == EUREKA_RC_NOT_FOUND || httpStatus == 404) {
      /* Instance was evicted; force re-registration on next iteration */
      zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_WARNING,
              ZSS_LOG_EUREKA_HB_404_MSG);
      registered = false;
    } else {
      failCount++;
      if (failCount == 1 || failCount % warnEvery == 0) {
        zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_WARNING,
                ZSS_LOG_EUREKA_HB_FAILED_MSG,
                rc, httpStatus, failCount);
      }
    }
  }

  /* unreachable -- task runs for the lifetime of the process */
  return 0;
}


/* =========================================================================
 * Registration / heartbeat helpers
 * ========================================================================= */

/*
 * doRegistration  --  POST /eureka/apps/<SERVICEID>
 */
static int doRegistration(EurekaClientSettings *settings, int *httpStatusOut) {
  char urlPath[256];
  if (buildBodyPath(settings, urlPath, sizeof(urlPath)) != 0) {
    return EUREKA_RC_STAGE_ERROR;
  }

  char body[EUREKA_BODY_BUFSIZE];
  int bodyLen = buildRegistrationBody(settings, body, sizeof(body));
  if (bodyLen <= 0) {
    zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_WARNING,
            ZSS_LOG_EUREKA_BODY_OVERFLOW_MSG);
    return EUREKA_RC_STAGE_ERROR;
  }

  zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_DEBUG,
          "Eureka registration body: %s\n", body);

  return sendEurekaRequest(settings, "POST", urlPath, body, bodyLen,
                           httpStatusOut);
}

/*
 * doHeartbeat  --  PUT /eureka/apps/<SERVICEID>/<instanceId>
 */
static int doHeartbeat(EurekaClientSettings *settings, int *httpStatusOut) {
  char urlPath[512];
  if (buildInstancePath(settings, urlPath, sizeof(urlPath)) != 0) {
    return EUREKA_RC_STAGE_ERROR;
  }

  return sendEurekaRequest(settings, "PUT", urlPath, NULL, 0, httpStatusOut);
}


/* =========================================================================
 * Low-level HTTP helper
 * ========================================================================= */

/*
 * sendEurekaRequest
 *
 * Opens a new HTTP(S) client session, stages and sends a single request,
 * reads the response, and returns.  Each call creates and destroys its own
 * HttpClientContext + HttpClientSession so that the task never holds an
 * open socket between heartbeats.
 *
 * body == NULL means no request body (used for heartbeats).
 */
static int sendEurekaRequest(EurekaClientSettings *settings,
                             const char *method,
                             const char *urlPath,
                             const char *body,
                             int         bodyLen,
                             int        *httpStatusOut) {
  int rc = EUREKA_RC_OK;
  *httpStatusOut = 0;

  HttpClientSettings clientSettings;
  memset(&clientSettings, 0, sizeof(clientSettings));
  clientSettings.host               = settings->discoveryHost;
  clientSettings.port               = settings->discoveryPort;
  clientSettings.recvTimeoutSeconds = EUREKA_RECV_TIMEOUT_SECS;

  LoggingContext    *logCtx  = makeLoggingContext();
  HttpClientContext *httpCtx = NULL;
  HttpClientSession *session = NULL;

  do {
    /* initialise HTTP context (with or without TLS) */
    int initRc = 0;
#ifdef USE_ZOWE_TLS
    if (settings->tlsEnv) {
      initRc = httpClientContextInitSecure(&clientSettings, logCtx,
                                           settings->tlsEnv, &httpCtx);
    } else {
      initRc = httpClientContextInit(&clientSettings, logCtx, &httpCtx);
    }
#else
    initRc = httpClientContextInit(&clientSettings, logCtx, &httpCtx);
#endif
    if (initRc != 0) {
      zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_DEBUG,
              "Eureka %s %s: httpClientContextInit rc=%d\n",
              method, urlPath, initRc);
      rc = EUREKA_RC_CTX_ERROR;
      break;
    }

    /* open TCP (or TLS) connection */
    int sessionRc = httpClientSessionInit(httpCtx, &session);
    if (sessionRc != 0) {
      zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_DEBUG,
              "Eureka %s %s: httpClientSessionInit rc=%d\n",
              method, urlPath, sessionRc);
      rc = EUREKA_RC_SESSION_ERROR;
      break;
    }

    /* stage the request line and Host header */
    int stageRc = httpClientSessionStageRequest(httpCtx, session,
                                                (char *)method,
                                                (char *)urlPath,
                                                NULL, NULL,  /* no basic auth */
                                                NULL, 0);    /* no inline body */
    if (stageRc != 0) {
      zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_DEBUG,
              "Eureka %s %s: stageRequest rc=%d\n", method, urlPath, stageRc);
      rc = EUREKA_RC_STAGE_ERROR;
      break;
    }

    /* add required HTTP headers */
    requestStringHeader(session->request, TRUE, "Content-Type",
                        "application/json");
    requestStringHeader(session->request, TRUE, "Accept",
                        "application/json");

    /* attach request body when provided (POST only) */
    if (body != NULL && bodyLen > 0) {
      int bodyRc = httpClientSessionSetRequestBody(httpCtx, session,
                                                    (char *)body, bodyLen,
                                                    TRUE /* bodyIsNativeText */);
      if (bodyRc != 0) {
        zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_DEBUG,
                "Eureka %s %s: setRequestBody rc=%d\n",
                method, urlPath, bodyRc);
        rc = EUREKA_RC_STAGE_ERROR;
        break;
      }
    }

    /* send the request */
    int sendRc = httpClientSessionSend(httpCtx, session);
    if (sendRc != 0) {
      zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_DEBUG,
              "Eureka %s %s: send rc=%d\n", method, urlPath, sendRc);
      rc = EUREKA_RC_SEND_ERROR;
      break;
    }

    /* receive and parse the response */
    int recvRc = httpClientSessionReceiveNativeLoop(httpCtx, session);
    if (recvRc != 0) {
      zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_DEBUG,
              "Eureka %s %s: receive rc=%d\n", method, urlPath, recvRc);
      rc = EUREKA_RC_RECV_ERROR;
      break;
    }

    if (session->response == NULL) {
      rc = EUREKA_RC_RECV_ERROR;
      break;
    }

    *httpStatusOut = session->response->statusCode;

    /* 404 is a special signal to the caller to re-register */
    if (*httpStatusOut == 404) {
      rc = EUREKA_RC_NOT_FOUND;
    }

    zowelog(NULL, LOG_COMP_ID_EUREKA, ZOWE_LOG_DEBUG,
            "Eureka %s %s: HTTP %d\n", method, urlPath, *httpStatusOut);

  } while (0);

  /* clean up */
  if (session) {
    httpClientSessionDestroy(session);
  }
  if (httpCtx) {
    httpClientContextDestroy(httpCtx);
  }

  return rc;
}


/* =========================================================================
 * URL path builders
 * ========================================================================= */

/*
 * buildBodyPath  --  /eureka/apps/<SERVICEID>
 * Used for the initial POST registration.
 */
static int buildBodyPath(EurekaClientSettings *settings,
                         char *buf, int bufLen) {
  int n = snprintf(buf, bufLen, "%s%s",
                   EUREKA_APPS_PATH_PREFIX, settings->serviceId);
  return (n > 0 && n < bufLen) ? 0 : -1;
}

/*
 * buildInstancePath  --  /eureka/apps/<SERVICEID>/<instanceId>
 * Used for PUT heartbeats.
 */
static int buildInstancePath(EurekaClientSettings *settings,
                              char *buf, int bufLen) {
  int n = snprintf(buf, bufLen, "%s%s/%s",
                   EUREKA_APPS_PATH_PREFIX,
                   settings->serviceId,
                   settings->instanceId);
  return (n > 0 && n < bufLen) ? 0 : -1;
}


/* =========================================================================
 * JSON body builder
 * ========================================================================= */

/*
 * buildRegistrationBody
 *
 * Produces the Eureka instance registration JSON as a native (EBCDIC on
 * z/OS) null-terminated string.  Returns the number of bytes written
 * (excluding the NUL), or -1 if the buffer was too small.
 *
 * Example output (pretty-printed for readability):
 *
 *   {
 *     "instance": {
 *       "instanceId":      "myhost:ZSS:7557",
 *       "app":             "ZSS",
 *       "hostName":        "myhost",
 *       "ipAddr":          "1.2.3.4",
 *       "vipAddress":      "zss",
 *       "secureVipAddress":"zss",
 *       "status":          "UP",
 *       "port":            { "$": 7557, "@enabled": "false" },
 *       "securePort":      { "$": 7557, "@enabled": "true"  },
 *       "homePageUrl":     "https://myhost:7557/",
 *       "statusPageUrl":   "https://myhost:7557/info",
 *       "healthCheckUrl":  "https://myhost:7557/health",
 *       "dataCenterInfo":  {
 *         "@class": "com.netflix.appinfo.InstanceInfo$DefaultDataCenterInfo",
 *         "name":   "MyOwn"
 *       },
 *       "leaseInfo": {
 *         "renewalIntervalInSecs": 30,
 *         "durationInSecs":        90
 *       },
 *       "metadata": {
 *         "apiml.service.title":       "Zowe ZSS",
 *         "apiml.service.description": "Zowe ZSS Service (z/OS Systems Services)"
 *       }
 *     }
 *   }
 */
static int buildRegistrationBody(EurekaClientSettings *settings,
                                 char *buf, int bufLen) {
  const char *portEnabled       = settings->securePortEnabled ? "false" : "true";
  const char *securePortEnabled = settings->securePortEnabled ? "true"  : "false";
  int renewalSecs = (settings->heartbeatIntervalSeconds > 0)
                    ? settings->heartbeatIntervalSeconds
                    : EUREKA_HEARTBEAT_INTERVAL_SECS;
  int durationSecs = renewalSecs * 3;   /* Eureka convention: 3x renewal */

  int n = snprintf(buf, bufLen,
    "{"
      "\"instance\":{"
        "\"instanceId\":\"%s\","
        "\"app\":\"%s\","
        "\"hostName\":\"%s\","
        "\"ipAddr\":\"%s\","
        "\"vipAddress\":\"zss\","
        "\"secureVipAddress\":\"zss\","
        "\"status\":\"UP\","
        "\"port\":{\"$\":%d,\"@enabled\":\"%s\"},"
        "\"securePort\":{\"$\":%d,\"@enabled\":\"%s\"},"
        "\"homePageUrl\":\"%s\","
        "\"statusPageUrl\":\"%s\","
        "\"healthCheckUrl\":\"%s\","
        "\"dataCenterInfo\":{"
          "\"@class\":\"com.netflix.appinfo.InstanceInfo$DefaultDataCenterInfo\","
          "\"name\":\"MyOwn\""
        "},"
        "\"leaseInfo\":{"
          "\"renewalIntervalInSecs\":%d,"
          "\"durationInSecs\":%d"
        "},"
        "\"metadata\":{"
          "\"apiml.service.title\":\"Zowe ZSS\","
          "\"apiml.service.description\":\"Zowe ZSS Service (z/OS Systems Services)\","
          "\"apiml.service.version\":\"%s\""
        "}"
      "}"
    "}",
    settings->instanceId,
    settings->serviceId,
    settings->hostName,
    settings->ipAddr,
    settings->port,       portEnabled,
    settings->port,       securePortEnabled,
    settings->homePageUrl,
    settings->statusPageUrl,
    settings->healthCheckUrl,
    renewalSecs,
    durationSecs,
    settings->version ? settings->version : ""
  );

  if (n <= 0 || n >= bufLen) {
    return -1;
  }
  return n;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
