/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * eurekaClient.h
 *
 * Registers ZSS as a service with the Zowe API ML Discovery server (Eureka)
 * and maintains the registration via periodic heartbeats.  All network I/O
 * runs inside a dedicated RLETask so the main HTTP server loop is not blocked.
 *
 * Typical call sequence from zss.c:
 *
 *   EurekaClientSettings *eurekaSettings =
 *       makeEurekaClientSettings(slh, configmgr, address, port, isHttps, tlsEnv);
 *   startEurekaClient(server, eurekaSettings);
 */

#ifndef ZSS_EUREKACLIENT_H_
#define ZSS_EUREKACLIENT_H_

#ifdef METTLE
#error Metal C is not supported by eurekaClient
#endif

#include <stdbool.h>
#include "zowetypes.h"
#include "alloc.h"
#include "httpserver.h"
#include "configmgr.h"
#ifdef USE_ZOWE_TLS
#include "tls.h"
#endif

/* ---------------------------------------------------------------------------
 * EurekaClientSettings
 *
 * All char* fields are expected to remain valid for the lifetime of the
 * background task; the recommended approach is to allocate them from the
 * server's ShortLivedHeap (slh) or from safeMalloc.
 * --------------------------------------------------------------------------- */
typedef struct EurekaClientSettings_tag {

  /* ---- Discovery server (API ML Discovery) coordinates ---- */
  char           *discoveryHost;         /* hostname / IP of the discovery service     */
  int             discoveryPort;         /* TCP port of the discovery service          */

  /* ---- This service's Eureka identity ---- */
  char           *serviceId;            /* Eureka appId  (upper-case), e.g. "ZSS"     */
  char           *instanceId;           /* <host>:<serviceId>:<port>                  */
  char           *hostName;             /* hostname this service is reachable at       */
  char           *ipAddr;               /* IP address this service is reachable at    */
  int             port;                 /* TCP port this service listens on           */
  bool            securePortEnabled;    /* true when TLS is in use for this service   */

  /* ---- Well-known URL suffixes for Eureka metadata ---- */
  char           *homePageUrl;          /* e.g. "https://<host>:<port>/"             */
  char           *statusPageUrl;        /* e.g. "https://<host>:<port>/info"         */
  char           *healthCheckUrl;       /* e.g. "https://<host>:<port>/health"       */

  /* ---- Service version reported in Eureka metadata ---- */
  char           *version;             /* e.g. "2.17.0+20260101"                    */

  /* ---- Timing ---- */
  int             heartbeatIntervalSeconds; /* PUT heartbeat interval   (default: 30 s)  */
  int             retryIntervalSeconds;     /* retry after failed req   (default: 30 s)  */
  int             maxRetries;            /* 0 means retry indefinitely                */

#ifdef USE_ZOWE_TLS
  /* TLS environment used to connect to the discovery server */
  TlsEnvironment *tlsEnv;
#endif

} EurekaClientSettings;

/* ---------------------------------------------------------------------------
 * makeEurekaClientSettings
 *
 * Reads the Zowe YAML via configmgr to determine the discovery server
 * host/port, then constructs an EurekaClientSettings that describes this
 * ZSS instance.
 *
 * Parameters:
 *   slh         - short-lived heap used for string allocations
 *   configmgr   - the live ConfigManager instance
 *   zssAddress  - the IP/hostname ZSS is bound to (from agent settings)
 *   zssPort     - the TCP port ZSS is listening on
 *   zssSecure   - true when ZSS is serving HTTPS
 *   zssVersion  - product version string (e.g. productVersion from zss.c)
 *   tlsEnv      - TLS environment for outbound connections (may be NULL)
 *
 * Returns NULL when the discovery component is not enabled or required
 * configuration values are absent.
 * --------------------------------------------------------------------------- */
EurekaClientSettings *makeEurekaClientSettings(ShortLivedHeap  *slh,
                                               ConfigManager   *configmgr,
                                               const char      *zssAddress,
                                               int              zssPort,
                                               bool             zssSecure,
                                               const char      *zssVersion
#ifdef USE_ZOWE_TLS
                                               , TlsEnvironment *tlsEnv
#endif
                                               );

/* ---------------------------------------------------------------------------
 * startEurekaClient
 *
 * Creates a disposable RLETask that:
 *   1. Posts a registration request to /eureka/apps/<serviceId>.
 *   2. Sends periodic PUT heartbeats to keep the registration alive.
 *
 * The task retries on transient network errors according to
 * settings->retryIntervalSecs.  It runs until the process exits.
 *
 * Safe to call with settings == NULL (no-op).
 * --------------------------------------------------------------------------- */
void startEurekaClient(HttpServer *server, EurekaClientSettings *settings);

#endif /* ZSS_EUREKACLIENT_H_ */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
