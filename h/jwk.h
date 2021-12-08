/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef JWK_H
#define JWK_H

#include <gskcms.h>
#include "tls.h"

typedef struct JwkSettings_tag JwkSettings;
typedef struct JwkContext_tag JwkContext;


struct JwkSettings_tag {
  TlsEnvironment *tlsEnv;
  char *host;
  int port;
  int timeoutSeconds;
  char *path;
  bool fallback;
};

struct JwkContext_tag {
  x509_public_key_info publicKey;
  bool isPublicKeyInitialized;
  JwkSettings *settings;
};

#define JWK_STATUS_OK                      0
#define JWK_STATUS_HTTP_ERROR              1
#define JWK_STATUS_RESPONSE_ERROR          2
#define JWK_STATUS_JSON_RESPONSE_ERROR     3
#define JWK_STATUS_UNRECOGNIZED_FMT_ERROR  4
#define JWK_STATUS_PUBLIC_KEY_ERROR        7
#define JWK_STATUS_HTTP_CONTEXT_ERROR      8
#define JWK_STATUS_HTTP_REQUEST_ERROR      9

void configureJwt(HttpServer *server, JwkSettings *jwkSettings);
const char *jwkGetStrStatus(int status);

#endif // JWK_H

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/