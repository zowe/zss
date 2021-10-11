/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef JWK_H
#define JWK_H

#include "tls.h"

typedef struct JwkSettings_tag JwkSettings;

Json *obtainJwk(JwkSettings *settings);

struct JwkSettings_tag {
  TlsEnvironment *tlsEnv;
  char *host;
  int port;
  int timeoutSeconds;
  char *path;
};

#define JWK_STATUS_OK                    0
#define JWK_STATUS_HTTP_ERROR            1
#define JWK_STATUS_RESPONSE_ERROR        2
#define JWK_STATUS_JSON_RESPONSE_ERROR   3
#define JWK_STATUS_INVALID_FORMAT_ERROR  4
#define JWK_STATUS_NOT_RSA_KEY           5
#define JWK_STATUS_RSA_FACTORS_NOT_FOUND 6
#define JWK_STATUS_INVALID_BASE64_URL    7
#define JWK_STATUS_INVALID_BASE64        8
#define JWK_STATUS_INVALID_PUBLIC_KEY    9

#endif // JWK_H

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/