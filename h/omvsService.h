

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifndef __OMVS_SERVICE_H__
#define __OMVS_SERVICE_H__

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif
#include "logging.h"
#include "json.h"
#include "bpxnet.h"
#include "socketmgmt.h"
#include "httpserver.h"
#include "dataservice.h"

/* This indicates the start of the
 * OMVS segment
 */
#define OMVS_SEGMENT_MARKER "OMVS INFORMATION"

/* Offsets for values in
 * the OMVS Segment
 */
#define UID_OFFSET  5
#define HOME_OFFSET 5
#define PROGRAM_OFFSET 9
#define CPU_TIME_MAX_OFFSET 12
#define ASSIZEMAX_OFFSET 11
#define FILE_PROC_MAX_OFFSET 13
#define PROC_USER_MAX_OFFSET 13
#define THREADS_MAX_OFFSET 12
#define M_MAP_AREA_MAX_OFFSET 13

typedef struct OMVSSegment_tag
{
  char uid[1024];
  char home[1024];
  char program[1024];
  char cpuTimeMax[64];
  char assizeMax[64];
  char fileProcMax[64];
  char procUserMax[64];
  char threadsMax[64];
  char mMapAreaMax[64];
} OMVSSegment;

int installOMVSService(HttpServer *server);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

