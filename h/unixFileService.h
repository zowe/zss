

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __UNIXSERVICE__
#define __UNIXSERVICE__

void installUnixFileContentsService(HttpServer *server);
void installUnixFileRenameService(HttpServer *server);
void installUnixFileCopyService(HttpServer *server);
void installUnixFileMakeDirectoryService(HttpServer *server);
void installUnixFileTouchService(HttpServer *server);
void installUnixFileMetadataService(HttpServer *server);
void installUnixFileChangeOwnerService(HttpServer *server);
void installUnixFileChangeTagService(HttpServer *server);
void installUnixFileTableOfContentsService(HttpServer *server);
void installUnixFileChangeModeService(HttpServer *server);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

