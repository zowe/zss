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
#include "httpserver.h"
#include "logging.h"
#include "zssLogging.h"
#include "configmgr.h"
#include "zis/client.h"
#include "zos.h"
#include "zss.h"

#define SAF_CLASS "ZOWE"
#define RBAC_INSTANCE_ID "0"
#define RBAC_PRODUCT_CODE "ZLUX"
#define RBAC_MAX_PROFILE_LEN 246

/* URLs that should be exempt from RBAC checks */
static bool isRBACExemptURL(const char *urlMask) {
  return !strcmp(urlMask, "/login/**") ||
         !strcmp(urlMask, "/logout/**") ||
         !strcmp(urlMask, "/saf-auth/**") ||
         !strcmp(urlMask, "saf/**") ||
         !strcmp(urlMask, "/password/**") ||
         !strcmp(urlMask, "/plugins");
}

/*
 * Build RBAC profile name from request URL and method.
 * Format: ZLUX.<instanceId>.COR.<METHOD>.<URL_SEGMENTS>
 * Example: /omvs/user with GET -> ZLUX.0.COR.GET.OMVS.USER
 */
static int buildProfileName(HttpRequest *request, char *profileBuf, int bufLen) {
  const char *method = request->method;
  char *path = stringListPrint(request->parsedFile, 1, 1000, "/", 0);
  if (path == NULL || strlen(path) == 0) {
    return -1;
  }

  /* Start with base: ZLUX.0.COR.<METHOD> */
  int written = snprintf(profileBuf, bufLen, "%s.%s.COR.%s",
                         RBAC_PRODUCT_CODE, RBAC_INSTANCE_ID, method);
  if (written < 0 || written >= bufLen) {
    return -1;
  }

  /* Append path segments as dots, uppercased */
  char *pathCopy = safeMalloc(strlen(path) + 1, "rbac path copy");
  if (pathCopy == NULL) {
    return -1;
  }
  strcpy(pathCopy, path);

  char *segment = strtok(pathCopy, "/");
  while (segment != NULL) {
    int segLen = strlen(segment);
    /* Check if adding this segment would exceed max profile length */
    if (written + 1 + segLen >= RBAC_MAX_PROFILE_LEN) {
      break;
    }
    if (written + 1 + segLen >= bufLen) {
      break;
    }
    profileBuf[written] = '.';
    written++;
    for (int i = 0; i < segLen; i++) {
      char c = segment[i];
      if (c >= 'a' && c <= 'z') {
        profileBuf[written + i] = c - 32; /* uppercase */
      } else if (c == '.') {
        profileBuf[written + i] = '_'; /* dots become underscores */
      } else {
        profileBuf[written + i] = c;
      }
    }
    written += segLen;
    segment = strtok(NULL, "/");
  }
  profileBuf[written] = '\0';

  safeFree(pathCopy, strlen(path) + 1);
  return 0;
}

typedef struct RBACServiceContext_tag {
  HttpService *service;
  HttpServiceServe *originalServiceFunction;
} RBACServiceContext;

#define MAX_RBAC_SERVICES 64
static RBACServiceContext rbacRegistry[MAX_RBAC_SERVICES];
static int rbacRegistryCount = 0;

static HttpServiceServe *findOriginalFunction(HttpService *service) {
  for (int i = 0; i < rbacRegistryCount; i++) {
    if (rbacRegistry[i].service == service) {
      return rbacRegistry[i].originalServiceFunction;
    }
  }
  return NULL;
}

static int rbacWrappedServiceFunction(HttpService *service, HttpResponse *response) {
  HttpRequest *request = response->request;
  HttpServer *server = service->server;
  ConfigManager *configmgr = httpServerConfigManager(server);

  /* Check if RBAC is enabled */
  Json *dataserviceAuthJson = NULL;
  int cfgGetStatus = cfgGetAnyC(configmgr, ZSS_CFGNAME, &dataserviceAuthJson, 3,
                                "components", "app-server", "dataserviceAuthentication");
  JsonObject *dataserviceAuth = (cfgGetStatus == ZCFG_SUCCESS ? jsonAsObject(dataserviceAuthJson) : NULL);
  int rbacEnabled = dataserviceAuth ? jsonObjectGetBoolean(dataserviceAuth, "rbac") : 0;

  HttpServiceServe *originalFunction = findOriginalFunction(service);
  if (originalFunction == NULL) {
    respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "RBAC configuration error");
    return -1;
  }

  if (!rbacEnabled) {
    return originalFunction(service, response);
  }

  /* If not authenticated, let the original handler deal with it (it may allow optional auth) */
  if (!request->authenticated) {
    return originalFunction(service, response);
  }

  /* Build the RBAC profile name from the request */
  char profileName[RBAC_MAX_PROFILE_LEN + 1];
  if (buildProfileName(request, profileName, sizeof(profileName)) != 0) {
    respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "Failed to build RBAC profile");
    return -1;
  }

  /* Perform SAF authorization check */
  CrossMemoryServerName *privilegedServerName = getConfiguredProperty(server,
      HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY);
  ZISAuthServiceStatus reqStatus = {0};
  int rc = zisCheckEntity(privilegedServerName, request->username, SAF_CLASS,
      profileName, SAF_AUTH_ATTR_READ, &reqStatus);

  if (rc != RC_ZIS_SRVC_OK) {
    zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG,
            "RBAC: user '%s' denied access to '%s'", request->username, profileName);
    respondWithError(response, HTTP_STATUS_FORBIDDEN,
                     "Forbidden - insufficient RBAC authorization");
    return -1;
  }

  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_DEBUG,
          "RBAC: user '%s' authorized for '%s'", request->username, profileName);

  /* Authorized — call original service function */
  return originalFunction(service, response);
}

void installRBACAuthorization(HttpServer *server) {
  HttpService *service = server->config->serviceList;

  while (service != NULL) {
    /* Skip services that don't require authentication */
    if (service->authType == SERVICE_AUTH_NONE) {
      service = service->next;
      continue;
    }

    /* Skip exempt URLs */
    if (service->urlMask != NULL && isRBACExemptURL(service->urlMask)) {
      service = service->next;
      continue;
    }

    /* Skip services that already have no serviceFunction */
    if (service->serviceFunction == NULL) {
      service = service->next;
      continue;
    }

    /* Register in the lookup table and wrap */
    if (rbacRegistryCount >= MAX_RBAC_SERVICES) {
      zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_WARNING,
              "RBAC: registry full, cannot wrap service '%s'", service->name);
      service = service->next;
      continue;
    }

    rbacRegistry[rbacRegistryCount].service = service;
    rbacRegistry[rbacRegistryCount].originalServiceFunction = service->serviceFunction;
    rbacRegistryCount++;
    service->serviceFunction = &rbacWrappedServiceFunction;

    service = service->next;
  }

  zowelog(NULL, LOG_COMP_ID_MVD_SERVER, ZOWE_LOG_INFO,
          "ZSS RBAC authorization wrapper installed on %d services", rbacRegistryCount);
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
