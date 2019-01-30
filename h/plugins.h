

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __PLUGINS__
#define __PLUGINS__

#include "dataservice.h"

typedef struct WebPluginListElt_tag {
  WebPlugin *plugin;
  struct WebPluginListElt_tag *next;
}WebPluginListElt;

char *getFilePathForPlugin(WebPlugin *plugin, char *filename, ShortLivedHeap *slh);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

