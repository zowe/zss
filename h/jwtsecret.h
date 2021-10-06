/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef JWT_SECRET_H
#define JWT_SECRET_H

#include "tls.h"

typedef struct JwtSecretSettings_tag JwtSecretSettings;

char *obtainJwtSecret(JwtSecretSettings *settings);

struct JwtSecretSettings_tag {
  TlsEnvironment *tlsEnv;
  char *host;
  int port;
  int timeoutSeconds;
  char *path;
};

#define JWT_SECRET_STATUS_OK             0
#define JWT_SECRET_STATUS_HTTP_ERROR     1
#define JWT_SECRET_STATUS_RESPONSE_ERROR 2

#endif // JWT_SECRET_H

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/