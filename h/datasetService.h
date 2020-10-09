

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __DATASET_SERVICE_H__
#define __DATASET_SERVICE_H__

void installDatasetContentsService(HttpServer *server);
void installDatasetEnqueueService(HttpServer *server);
void installVSAMDatasetContentsService(HttpServer *server);
void installDatasetMetadataService(HttpServer *server);

#endif /* __DATASET_SERVICE_H__ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

