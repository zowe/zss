/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef STORAGE_APIML_H
#define STORAGE_APIML_H

#include "tls.h"
#include "storage.h"

typedef struct ApimlStorageSettings_tag ApimlStorageSettings;

Storage *makeApimlStorage(ApimlStorageSettings *settings);

struct ApimlStorageSettings_tag {
  TlsSettings *tlsSettings;
  char *host;
  int port;
};

#endif // STORAGE_APIML_H

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/