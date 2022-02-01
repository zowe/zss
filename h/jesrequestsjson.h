

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __JESREQUESTSJSON__
#define __JESREQUESTSJSON__ 1


#include "stcbase.h"
#include "json.h"
#include "xml.h"
#include "jcsi.h"

void respondWithSubmitDataset(HttpResponse* response, char* absolutePath, int jsonMode);
int streamDatasetToINTRDR(HttpResponse* response, char *filename, int recordLength, int jsonMode);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/