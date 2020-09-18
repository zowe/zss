

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __CERT_SERVICE_H__
#define __CERT_SERVICE_H__

void installCertificateService(HttpServer *server);
void setValidResponseCode(HttpResponse *response, int rc, int return_code, int RACF_return_code, int RACF_reason_code);
void handleInvalidMethod(HttpResponse *response);
void installCertificateService(HttpServer *server)

#endif /* __CERT_SERVICE_H__ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

