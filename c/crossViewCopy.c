

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
/* HAS NOT BEEN COMPILED WITH METTLE BEFORE */
#else
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "json.h"
#include "bpxnet.h"
#include "logging.h"
#include "unixfile.h"
#include "errno.h"
#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif 

#include "charsets.h"

#include "socketmgmt.h"
#include "httpserver.h"
#include "datasetjson.h"
#include "crossViewCopy.h"
#include "pdsutil.h"
#include "jcsi.h"
#include "impersonation.h"
#include "dynalloc.h"
#include "utils.h"
#include "vsam.h"
#include "qsam.h"
#include "icsf.h"

#define INDEXED_DSCB      96
#define LEN_THREE_BYTES   3
#define LEN_ONE_BYTE      1
#define VOLSER_SIZE       6
#define CLASS_WRITER_SIZE 8
#define TOTAL_TEXT_UNITS  23

#define DATASET_ALLOC_TYPE_BYTE  0
#define DATASET_ALLOC_TYPE_CYL   1
#define DATASET_ALLOC_TYPE_TRK   2
#define DATASET_ALLOC_TYPE_BLOCK 3
#define DATASET_ALLOC_TYPE_KB    4
#define DATASET_ALLOC_TYPE_MB    5

#define ERROR_DECODING_DATASET            -2
#define ERROR_CLOSING_DATASET             -3
#define ERROR_OPENING_DATASET             -4
#define ERROR_ALLOCATING_DATASET          -5
#define ERROR_DEALLOCATING_DATASET        -6
#define ERROR_UNDEFINED_LENGTH_DATASET    -7
#define ERROR_BAD_DATASET_NAME            -8
#define ERROR_INVALID_DATASET_NAME        -9
#define ERROR_INCORRECT_DATASET_TYPE      -10
#define ERROR_DATASET_NOT_EXIST           -11
#define ERROR_MEMBER_ALREADY_EXISTS       -12
#define ERROR_DATASET_ALREADY_EXIST       -13
#define ERROR_DATASET_OR_MEMBER_NOT_EXIST -14
#define ERROR_VSAM_DATASET_DETECTED       -15
#define ERROR_DELETING_DATASET_OR_MEMBER  -16
#define ERROR_INVALID_JSON_BODY           -17
#define ERROR_COPY_NOT_SUPPORTED          -18
#define ERROR_COPYING_DATASET             -19


static char defaultDatasetTypesAllowed[3] = {'A','D','X'};
static char clusterTypesAllowed[3] = {'C','D','I'}; /* TODO: support 'I' type DSNs */
static int clusterTypesCount = 3;
static char *datasetStart = "//'";
static char *defaultCSIFields[] ={ "NAME    ", "TYPE    ", "VOLSER  "};
static int defaultCSIFieldCount = 3;
static char *defaultVSAMCSIFields[] ={"AMDCIREC", "AMDKEY  ", "ASSOC   ", "VSAMTYPE"};
static int defaultVSAMCSIFieldCount = 4;
static char vsamCSITypes[5] = {'R', 'D', 'G', 'I', 'C'};


typedef struct DatasetName_tag {
  char value[44]; /* space-padded */
} DatasetName;

typedef struct DatasetMemberName_tag {
  char value[8]; /* space-padded */
} DatasetMemberName;

typedef struct DDName_tag {
  char value[8]; /* space-padded */
} DDName;

typedef struct Volser_tag {
  char value[6]; /* space-padded */
} Volser;


#define IS_DAMEMBER_EMPTY($member) \
  (!memcmp(&($member), &(DynallocMemberName){"        "}, sizeof($member)))

void copyDatasetToUnixAndRespond(HttpResponse *response, char* sourceDataset) {
  printf("--------INSIDE copyDatasetToUnixAndRespond/n");
  #ifdef __ZOWE_OS_ZOS
  HttpRequest *request = response->request;

  if (sourceDataset == NULL || strlen(sourceDataset) < 1){
    respondWithError(response,HTTP_STATUS_BAD_REQUEST,"No source dataset name given");
    return;
  }

  if(!isDatasetPathValid(sourceDataset)){
    respondWithError(response,HTTP_STATUS_BAD_REQUEST,"Invalid dataset path");
    return;
  }

  /* From here on, we know we have a valid data path */
  DatasetName sourceDsnName;
  DatasetMemberName sourceMemName;
  extractDatasetAndMemberName(sourceDataset, &sourceDsnName, &sourceMemName);

  printf("---SRC DATASET ABS PATH: %s \n", sourceDataset);

  char CSIType = getCSIType(sourceDataset);
  if (isVsam(CSIType)) {
    respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Copy not supported for VSAM DATASET");
    return;
  }

  // Checking if source dataset is a member
  DynallocDatasetName daSourceDsnName;
  DynallocMemberName daSourceMemName;
  memcpy(daSourceDsnName.name, sourceDsnName.value, sizeof(daSourceDsnName.name));
  memcpy(daSourceMemName.name, sourceMemName.value, sizeof(daSourceMemName.name));

  bool isSourceMemberEmpty = IS_DAMEMBER_EMPTY(daSourceMemName);

  int srcDsnExists = checkIfDatasetExistsAndRespond(response, sourceDataset, !isSourceMemberEmpty);
  if(srcDsnExists < 0 ) {
    printf("------------Source DSN Exists\n");
    return;
  } else if(srcDsnExists == 0) {
    printf("------------Source DSN Does not Exists\n");
    respondWithJsonError(response, "Source dataset does not exist", 400, "Bad Request");
    return;
  }

  HttpRequestParam *isFolderParam = getCheckedParam(request,"isFolder");
  char *isFolderArg = (isFolderParam ? isFolderParam->stringValue : NULL);

  HttpRequestParam *nameParam = getCheckedParam(request,"name");
  char *nameArg = (nameParam ? nameParam->stringValue : NULL);

  printf("---isFolder: %s \n", isFolderArg);
  printf("---name %s \n", nameArg);

  #endif /* __ZOWE_OS_ZOS */
}

#endif /* not METTLE - the whole module */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

