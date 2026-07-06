/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __RBAC_SERVICE_H__
#define __RBAC_SERVICE_H__

#include "httpserver.h"

/*
 * Install centralized RBAC authorization on all registered HTTP services.
 * This wraps each service's handler with a SAF check that constructs a 
 * RACF profile from the request URL and verifies the user has READ access.
 *
 * Profile format: ZLUX.0.COR.<METHOD>.<URL_SEGMENTS_UPPERCASED>
 *
 * Exempt services (not wrapped):
 *   - /login, /logout, /saf-auth, /password, /plugins, saf/*
 *   - Any service with authType = SERVICE_AUTH_NONE
 *
 * Call this AFTER all services are registered but BEFORE mainHttpLoop.
 */
void installRBACAuthorization(HttpServer *server);

#endif /* __RBAC_SERVICE_H__ */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
