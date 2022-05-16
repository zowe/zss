
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#error Metal C is not supported
#else
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "logging.h"
#ifdef __ZOWE_OS_ZOS
#include "zos.h"
#endif
#include "jcsi.h"
#include "dynalloc.h"
#include "utils.h"
#include "dsutils.h"


#ifdef __ZOWE_OS_ZOS
/*
  Dataset only, no wildcards or pds members accepted - beware that this should be wrapped with authentication
 */
int dsutilsGetVolserForDataset(const DatasetName *dataset, Volser *volser) {
  if (dataset == NULL){
    return -1;
  }
  int length = sizeof(dataset->value);

  char dsnNullTerm[45] = {0};
  memcpy(dsnNullTerm, dataset->value, sizeof(dataset->value));

  int lParenIndex = indexOf(dsnNullTerm, length, '(', 0);
  int asterixIndex = indexOf(dsnNullTerm, length, '*', 0);
  int dollarIndex = indexOf(dsnNullTerm, length, '$', 0);
  if (lParenIndex > -1 || asterixIndex > -1 || dollarIndex > -1){
    return -1;
  }
  csi_parmblock * __ptr32 returnParms = (csi_parmblock* __ptr32)safeMalloc31(sizeof(csi_parmblock),"CSI ParmBlock");
  EntryDataSet *entrySet = returnEntries(dsnNullTerm, defaultDatasetTypesAllowed,3, 0, defaultCSIFields, defaultCSIFieldCount, NULL, NULL, returnParms);

  EntryData *entry = entrySet->entries ? entrySet->entries[0] : NULL;
  int rc = -1;
  if (entry){
    char *fieldData = (char*)entry+sizeof(EntryData);
    unsigned short *fieldLengthArray = ((unsigned short *)((char*)entry+sizeof(EntryData)));
    char *fieldValueStart = (char*)entry+sizeof(EntryData)+defaultCSIFieldCount*sizeof(short);
    for (int j=0; j<defaultCSIFieldCount; j++){
      if (!strcmp(defaultCSIFields[j],"VOLSER  ") && fieldLengthArray[j]){
        memcpy(volser->value,fieldValueStart,6);
        rc = 0;
        break;
      }
      fieldValueStart += fieldLengthArray[j];
    }
  }

  for (int i = 0; i < entrySet->length; i++){
    EntryData *currentEntry = entrySet->entries[i];
    int fieldDataLength = currentEntry->data.fieldInfoHeader.totalLength;
    int entrySize = sizeof(EntryData)+fieldDataLength-4;
    safeFree((char*)(currentEntry),entrySize);
  }
  if (entrySet->entries != NULL) {
    safeFree((char*)(entrySet->entries),sizeof(EntryData*)*entrySet->size);
    entrySet->entries = NULL;
  }
  safeFree((char*)entrySet,sizeof(EntryDataSet));
  safeFree31((char*)returnParms,sizeof(csi_parmblock));
  return rc;
}

char dsutilsGetRecordLengthType(char *dscb){
  int posOffset = 44;
  int recfm = dscb[84 - posOffset];
  if ((recfm & 0xc0) == 0xc0){
    return 'U';
  }
  else if (recfm & 0x80){
    return 'F';
  }
  else if (recfm & 0x40){
    return 'V';
  }
}

int dsutilsGetMaxRecordLength(char *dscb){
  int posOffset = 44;
  int lrecl = (dscb[88-posOffset] << 8) | dscb[89-posOffset];
  return lrecl;
}

int dsutilsObtainDSCB1(const char *dsname, unsigned int dsnameLength,
                       const char *volser, unsigned int volserLength,
                       char *dscb1) {

#define DSCB1_SIZE                    96

#define SVC27_WORKAREA_SIZE           140
#define SVC27_OPCODE_SEARCH           0xC100
#define SVC27_OPCODE_SEEK             0xC080

#define SVC27_OPTION_NO_TIOT_ENQ      0x80
#define SVC27_OPTION_NO_DUMMY_DSCB1   0x40
#define SVC27_OPTION_NO_CAT_ALLOC     0x20
#define SVC27_OPTION_HIDE_NAME        0x10
#define SVC27_OPTION_EADSCB_OK        0x08

  ALLOC_STRUCT31(
    STRUCT31_NAME(mem31),
    STRUCT31_FIELDS(
      char dsnameSpacePadded[44];
      char volserSpacePadded[6];
      char workArea[SVC27_WORKAREA_SIZE];
      __packed struct {
        int operationCode : 16;
        int option        : 8;
        int dscbNumber    : 8;
        char * __ptr32 dsname;
        char * __ptr32 volser;
        char * __ptr32 workArea;
      } parmList;
    )
  );

  memset(mem31->dsnameSpacePadded, ' ', sizeof(mem31->dsnameSpacePadded));
  memcpy(mem31->dsnameSpacePadded, dsname, dsnameLength);
  memset(mem31->volserSpacePadded, ' ', sizeof(mem31->volserSpacePadded));
  memcpy(mem31->volserSpacePadded, volser, volserLength);

  memset(mem31->workArea, 0, sizeof(mem31->workArea));

  mem31->parmList.operationCode = SVC27_OPCODE_SEARCH;
  mem31->parmList.option = SVC27_OPTION_EADSCB_OK;
  mem31->parmList.dscbNumber = 0;
  mem31->parmList.dsname = mem31->dsnameSpacePadded;
  mem31->parmList.volser =  mem31->volserSpacePadded;
  mem31->parmList.workArea = mem31->workArea;

  int rc = 0;

  __asm(
      "         LA    1,0(%1)                                                  \n"
      "         SVC   27                                                       \n"
      "         ST    15,%0                                                    \n"
      : "=m"(rc)
      : "r"(&mem31->parmList)
      : "r0", "r15"
  );

  memcpy(dscb1, mem31->workArea, DSCB1_SIZE);

  FREE_STRUCT31(
    STRUCT31_NAME(mem31)
  );

  return rc;
}


int dsutilsGetLreclOrRespondError(HttpResponse *response,
		const DatasetName *dsn,
		const char *ddPath){

  int lrecl = 0;

  FileInfo info;
  int returnCode;
  int reasonCode;
  FILE *in = fopen(ddPath, "r");
  if (in == NULL) {
    respondWithError(response, HTTP_STATUS_NOT_FOUND, "File could not be opened or does not exist");
    return 0;
  }

  Volser volser;
  memset(&volser.value, ' ', sizeof(volser.value));

  int volserSuccess = dsutilsGetVolserForDataset(dsn, &volser);
  int handledThroughDSCB = FALSE;

  if (!volserSuccess){

    char dscb[INDEXED_DSCB] = {0};
    int rc = dsutilsObtainDSCB1(dsn->value, sizeof(dsn->value),
                         volser.value, sizeof(volser.value),
                         dscb);
    if (rc == 0){
      if (DSCB_TRACE){
        zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "DSCB for %.*s found\n", sizeof(dsn->value), dsn->value);
        dumpbuffer(dscb,INDEXED_DSCB);
      }

      lrecl = dsutilsGetMaxRecordLength(dscb);
      char recordType = dsutilsGetRecordLengthType(dscb);
      if (recordType == 'U'){
        fclose(in);
        respondWithError(response, HTTP_STATUS_BAD_REQUEST,"Undefined-length dataset");
        return 0;
      }
      handledThroughDSCB = TRUE;
    }
  }
  if (!handledThroughDSCB){
    FileInfo info;
    fldata_t fileinfo = {0};
    char filenameOutput[100];
    int returnCode = fldata(in,filenameOutput,&fileinfo);
    zowelog(NULL, LOG_COMP_RESTDATASET, ZOWE_LOG_DEBUG, "FLData request rc=0x%x\n",returnCode);
    if (!returnCode) {
      if (fileinfo.__recfmU) {
        fclose(in);
        respondWithError(response,  HTTP_STATUS_BAD_REQUEST,
                         "Undefined-length dataset");
        return 0;
      }
      lrecl = fileinfo.__maxreclen;
    } else {
      fclose(in);
      respondWithError(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,
                       "Could not read dataset information");
      return 0;
    }
  }
  fclose(in);

  return lrecl;
}

#define DSPATH_PREFIX   "//\'"
#define DSPATH_SUFFIX   "\'"

bool dsutilsIsDatasetPathValid(const char *path) {

  /* Basic check. The fopen() dataset path format is //'dsn(member)' */

  /* Min dataset file name:
   * //'              - 3
   * min ds name      - 1
   * '                - 1
   */
#define PATH_MIN_LENGTH 5

  /* Max dataset file name:
   * //'              - 3
   * man ds name      - 44
   * (                - 1
   * max member name  - 8
   * )                - 1
   * '                - 1
   */
#define PATH_MAX_LENGTH 58

#define DSN_MIN_LENGTH      1
#define DSN_MAX_LENGTH      44
#define MEMBER_MIN_LENGTH   1
#define MEMBER_MAX_LENGTH   8

  size_t pathLength = strlen(path);

  if (pathLength < PATH_MIN_LENGTH || PATH_MAX_LENGTH < pathLength) {
    return false;
  }

  if (memcmp(path, DSPATH_PREFIX, strlen(DSPATH_PREFIX))) {
    return false;
  }
  if (memcmp(&path[pathLength - 1], DSPATH_SUFFIX, strlen(DSPATH_SUFFIX))) {
    return false;
  }

  const char *dsnStart = path + strlen(DSPATH_PREFIX);
  const char *dsnEnd = path + pathLength - 1 - strlen(DSPATH_SUFFIX);

  const char *leftParen = strchr(dsnStart, '(');
  const char *rightParen = strchr(dsnStart, ')');

  if (!leftParen ^ !rightParen) {
    return false;
  }

  if (leftParen) {

    ptrdiff_t dsnLength = leftParen - dsnStart;
    if (dsnLength < DSN_MIN_LENGTH || DSN_MAX_LENGTH < dsnLength) {
      return false;
    }

    if (rightParen != dsnEnd) {
      return false;
    }

    ptrdiff_t memberNameLength = rightParen - leftParen - 1;
    if (memberNameLength < MEMBER_MIN_LENGTH || MEMBER_MAX_LENGTH < memberNameLength) {
      return false;
    }

  } else {

    ptrdiff_t dsnLength = dsnEnd - dsnStart + 1;
    if (dsnLength < DSN_MIN_LENGTH || DSN_MAX_LENGTH < dsnLength) {
      return false;
    }

  }

#undef PATH_MIN_LENGTH
#undef PATH_MAX_LENGTH

#undef DSN_MIN_LENGTH
#undef DSN_MAX_LENGTH
#undef MEMBER_MIN_LENGTH
#undef MEMBER_MAX_LENGTH

  return true;

}

#define DSPATH_PREFIX   "//\'"
#define DSPATH_SUFFIX   "\'"

void dsutilsExtractDatasetAndMemberName(const char *datasetPath,
                                        DatasetName *dsn,
                                        DatasetMemberName *memberName) {

  memset(&dsn->value, ' ', sizeof(dsn->value));
  memset(&memberName->value, ' ', sizeof(memberName->value));

  size_t pathLength = strlen(datasetPath);

  const char *dsnStart = datasetPath + strlen(DSPATH_PREFIX);
  const char *leftParen = strchr(datasetPath, '(');

  if (leftParen) {
    memcpy(dsn->value, dsnStart, leftParen - dsnStart);
    const char *rightParen = strchr(datasetPath, ')');
    memcpy(memberName->value, leftParen + 1, rightParen - leftParen - 1);
  } else {
    memcpy(dsn->value, dsnStart,
           pathLength - strlen(DSPATH_PREFIX""DSPATH_SUFFIX));
  }

  for (int i = 0; i < sizeof(dsn->value); i++) {
    dsn->value[i] = toupper(dsn->value[i]);
  }

  for (int i = 0; i < sizeof(memberName->value); i++) {
    memberName->value[i] = toupper(memberName->value[i]);
  }

}

#undef DSPATH_PREFIX
#undef DSPATH_SUFFIX

void dsutilsRespondWithDYNALLOCError(HttpResponse *response,
                              int rc, int sysRC, int sysRSN,
                              const DynallocDatasetName *dsn,
                              const DynallocMemberName *member,
                              const char *site) {

  if (rc ==  RC_DYNALLOC_SVC99_FAILED && sysRC == 4) {

    if (sysRSN == 0x020C0000 || sysRSN == 0x02100000) {
      respondWithMessage(response, HTTP_STATUS_FORBIDDEN,
                        "Dataset \'%44.44s(%8.8s)\' busy (%s)",
                        dsn->name, member->name, site);
      return;
    }

    if (sysRSN == 0x02180000) {
      respondWithMessage(response, HTTP_STATUS_NOT_FOUND,
                        "Device not available for dataset \'%44.44s(%8.8s)\' "
                        "(%s)", dsn->name, member->name, site);
      return;
    }

    if (sysRSN == 0x023C0000) {
      respondWithMessage(response, HTTP_STATUS_NOT_FOUND,
                        "Catalog not available for dataset \'%44.44s(%8.8s)\' "
                        "(%s)", dsn->name, member->name, site);
      return;
    }

  }

  respondWithMessage(response, HTTP_STATUS_INTERNAL_SERVER_ERROR,
                    "DYNALLOC failed with RC = %d, DYN RC = %d, RSN = 0x%08X, "
                    "dsn=\'%44.44s(%8.8s)\', (%s)", rc, sysRC, sysRSN,
                    dsn->name, member->name, site);

}

#endif /* __ZOWE_OS_ZOS */

#endif /* not METTLE - the whole module */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/