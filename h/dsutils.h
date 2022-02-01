
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __DSUTILS__
#define __DSUTILS__ 1

#include "httpserver.h"
#include "dynalloc.h"


#define INDEXED_DSCB 96

static char defaultDatasetTypesAllowed[3] = {'A','D','X'};
static char *defaultCSIFields[] ={ "NAME    ", "TYPE    ", "VOLSER  "};
static int defaultCSIFieldCount = 3;

const int DSCB_TRACE = FALSE;


typedef struct DatasetMemberName_tag {
  char value[8]; /* space-padded */
} DatasetMemberName;

typedef struct DDName_tag {
  char value[8]; /* space-padded */
} DDName;

typedef struct DatasetName_tag {
  char value[44]; /* space-padded */
} DatasetName;

typedef struct Volser_tag {
  char value[6]; /* space-padded */
} Volser;

int getVolserForDataset(const DatasetName *dataset, Volser *volser);
char getRecordLengthType(char *dscb);
int getMaxRecordLength(char *dscb);

int obtainDSCB1(const char *dsname, unsigned int dsnameLength,
                       const char *volser, unsigned int volserLength,
                       char *dscb1);
int getLreclOrRespondError(HttpResponse *response,
		const DatasetName *dsn,
		const char *ddPath);
bool isDatasetPathValid(const char *path);

void extractDatasetAndMemberName(const char *datasetPath,
                                        DatasetName *dsn,
                                        DatasetMemberName *memberName);
void respondWithDYNALLOCError(HttpResponse *response,
                              int rc, int sysRC, int sysRSN,
                              const DynallocDatasetName *dsn,
                              const DynallocMemberName *member,
                              const char *site);
#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

