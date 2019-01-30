

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdlib.h>
#include <metal/string.h>

#include "zowetypes.h"
#include "alloc.h"
#include "crossmemory.h"
#include "metalio.h"
#include "zis/parm.h"
#include "zis/server.h"

#define IEFPRMLB_RECORD_SIZE 80

typedef __packed struct IEFPRMLBHeader_tag {
  unsigned int totalSize;
  unsigned int recordsRead;
  unsigned int sizeRequired;
  unsigned int recordsTotal;
  char reserved[8];
  char records[0][IEFPRMLB_RECORD_SIZE];
} IEFPRMLBHeader;

#define RC_IEFPRMLB_OK      0x00
#define RC_IEFPRMLB_WARN    0x04
  #define RSN_IEFPRMLB_ALREADY_ALLOCATED    0x01
#define RC_IEFPRMLB_LOCK    0x08
#define RC_IEFPRMLB_FAILED  0x0C
  #define RSN_IEFPRMLB_MEMBER_NOT_FOUND     0x01
  #define RSN_IEFPRMLB_BUFFER_FULL          0x0A

typedef int (RecordVisitor)(const char *record,
                            unsigned int recordLength,
                            void *userData);

static int iterateMember(IEFPRMLBHeader *memberContent,
                         RecordVisitor *visitor,
                         void *userData) {

  int state = RC_ZISPARM_OK;
  for (unsigned int recID = 0; recID < memberContent->recordsTotal; recID++) {
    int visitorRC = visitor(memberContent->records[recID],
                           IEFPRMLB_RECORD_SIZE, userData);
    if (visitorRC != 0) {
      state = RC_ZISPARM_NONZERO_VISITOR_RC;
      break;
    }

  }

  return state;
}

__asm("GLBPRML    IEFPRMLB MF=(L,GLBPRML)" : "DS"(GLBPRML));

static const char *IEFPRMLB_CALLER = CMS_PROD_ID"IS            ";

static int allocLogicalParmLib(const char ddnameSpacePadded[],
                               int *reasonCode) {

  __asm(" IEFPRMLB MF=L" : "DS"(iefprmlbParmList));
  iefprmlbParmList = GLBPRML;

  int rc = 0, rsn = 0;
  __asm(
      ASM_PREFIX

#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         IEFPRMLB REQUEST=ALLOCATE,ALLOCDDNAME=(%2)"
      ",CALLERNAME=(%3)"
      ",RETCODE=%0,RSNCODE=%1,MF=(E,(%4),COMPLETE)                             \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      : "=m"(rc), "=m"(rsn)
      : "r"(ddnameSpacePadded), "r"(IEFPRMLB_CALLER), "r"(&iefprmlbParmList)
      : "r0", "r1", "r14", "r15"
  );

  *reasonCode = rsn;
  return rc;
}

static int readLogicalParmLib(const char ddnameSpacePadded[],
                              const char memberNameSpacePadded[],
                              IEFPRMLBHeader *readBuffer,
                              int *reasonCode) {

  __asm(" IEFPRMLB MF=L" : "DS"(iefprmlbParmList));
  iefprmlbParmList = GLBPRML;

  int rc = 0, rsn = 0;
  __asm(
      ASM_PREFIX
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         IEFPRMLB REQUEST=READMEMBER,DDNAME=(%2),MEMNAME=(%3)"
      ",CALLERNAME=(%4)"
      ",READBUF=(%5)"
      ",RETCODE=%0,RSNCODE=%1,MF=(E,(%6),COMPLETE)                             \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      : "=m"(rc), "=m"(rsn)
      : "r"(ddnameSpacePadded), "r"(memberNameSpacePadded),
        "r"(IEFPRMLB_CALLER), "r"(readBuffer), "r"(&iefprmlbParmList)
      : "r0", "r1", "r14", "r15"
  );

  *reasonCode = rsn;
  return rc;
}

static int freeLogicalParmLib(const char ddnameSpacePadded[],
                              int *reasonCode) {

  __asm(" IEFPRMLB MF=L" : "DS"(iefprmlbParmList));
  iefprmlbParmList = GLBPRML;

  int rc = 0, rsn = 0;
  __asm(
      ASM_PREFIX
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         IEFPRMLB REQUEST=FREE,DDNAME=(%2)"
      ",CALLERNAME=(%3)"
      ",RETCODE=%0,RSNCODE=%1,MF=(E,(%4),COMPLETE)                             \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      : "=m"(rc), "=m"(rsn)
      : "r"(ddnameSpacePadded), "r"(IEFPRMLB_CALLER), "r"(&iefprmlbParmList)
      : "r0", "r1", "r14", "r15"
  );

  *reasonCode = rsn;
  return rc;
}

static int printRecords(const char *record,
                        unsigned int recordLength,
                        void *userData) {
 if (record) {
   printf("%.*s\n", recordLength, record);
 }

 return 0;
}

static const char *getTrimmedSlice(const char *string, unsigned int length,
                                   unsigned int *sliceLength) {

  const char *result = string;
  unsigned int resultLength = length;

  if (length == 0) {
    return result;
  }

  for (unsigned int i = 0; i < resultLength; i++) {
    if (result[i] != ' ' || i == resultLength - 1) {
      result = &result[i];
      resultLength = resultLength - i;
      break;
    }
  }


  if (resultLength > 0) {
    for (unsigned int i = resultLength - 1; i >= 0; i--) {
      if (result[i] != ' ' || i == 0) {
        resultLength = resultLength - ((resultLength - 1) - i);
        break;
      }
    }
  }

  *sliceLength = resultLength;
  return result;
}

typedef int (ZISParmHandler)(const char *keyNullTerm,
                             const char *valueNullTerm,
                             void *userData);

typedef __packed struct ParmRecordVisitorData_tag {
  char eyecatcher[8];
#define PARM_REC_VISITOR_DATA_EYECATCHER "RSZISPVD"
  ShortLivedHeap *slh;
  ZISParmSet *parmSet;
} ParmRecordVisitorData;

static int handleParmRecord(const char *record,
                            unsigned int recordLength,
                            void *userData) {

  ParmRecordVisitorData *visitorContext = userData;
  ShortLivedHeap *slh = visitorContext->slh;
  ZISParmSet *parmSet = visitorContext->parmSet;

  unsigned int sliceLength = 0;
  const char *slice = getTrimmedSlice(record, recordLength, &sliceLength);
  if (sliceLength > 0) {

    if (slice[0] == '*') {
      return RC_ZISPARM_OK;
    }

    char *sliceNullTerm = SLHAlloc(slh, sliceLength + 1);
    if (sliceNullTerm == NULL) {
      return RC_ZISPARM_SLH_ALLOC_FAILED;
    }

    memcpy(sliceNullTerm, slice, sliceLength);
    sliceNullTerm[sliceLength] = '\0';

    const char *key = sliceNullTerm;
    const char *value = "";

    char *eqSign = strchr(sliceNullTerm, '=');

    if (eqSign != NULL) {
      *eqSign = '\0';
      value = eqSign + 1;
    }

    int putRC = zisPutParmValue(parmSet, key, value);
    if (putRC != RC_ZISPARM_OK) {
      return putRC;
    }

  }

  return RC_ZISPARM_OK;
}

#define PARM_SLH_BLOCK_SIZE         0x10000
#define PARM_SLH_MAX_BLOCK_COUNT    10

ZISParmSet *zisMakeParmSet() {

  ShortLivedHeap *slh = makeShortLivedHeap(PARM_SLH_BLOCK_SIZE,
                                           PARM_SLH_MAX_BLOCK_COUNT);
  if (slh == NULL) {
    return NULL;
  }

  ZISParmSet *parmSet = (ZISParmSet *)SLHAlloc(slh, sizeof(ZISParmSet));
  if (parmSet == NULL) {
    SLHFree(slh);
    slh = NULL;
    return NULL;
  }

  memset(parmSet, 0, sizeof(ZISParmSet));
  memcpy(parmSet->eyecatcher, ZIS_PARMSET_EYECATCHER,
         sizeof(parmSet->eyecatcher));
  parmSet->slh = slh;

  int putRC = zisPutParmValue(parmSet, ZIS_PARM_TEST_PARM_NAME,
                              ZIS_PARM_TEST_PARM_VALUE);
  if (putRC != RC_ZISPARM_OK) {
    SLHFree(slh);
    slh = NULL;
    return NULL;
  }

  return parmSet;
}

#define READ_BUFFER_DEFAULT_SIZE  (1024 * 1024)

static IEFPRMLBHeader *allocReadBuffer(unsigned int size) {

  if (size <= sizeof(IEFPRMLBHeader)) {
    return NULL;
  }

  IEFPRMLBHeader *buffer = (IEFPRMLBHeader *)safeMalloc31(size, "IEFPRMLB");
  if (buffer == NULL) {
    return NULL;
  }
  memset(buffer, 0, size);
  buffer->totalSize = size;

  return buffer;
}

static void freeReadBuffer(IEFPRMLBHeader *readBuffer) {
  safeFree31((char *)readBuffer, readBuffer->totalSize);
  readBuffer = NULL;
}

int zisReadParmlib(ZISParmSet *parms, const char *ddname, const char *member,
                   ZISParmStatus *readStatus) {

  /* handle DD and member names */
  char ddnameSpacePadded[8];
  memset(ddnameSpacePadded, ' ', sizeof(ddnameSpacePadded));
  unsigned int ddnameLength = strlen(ddname);
  if (ddnameLength > sizeof(ddnameSpacePadded)) {
    return RC_ZISPARM_DDNAME_TOO_LONG;
  }
  memcpy(ddnameSpacePadded, ddname, ddnameLength);

  char memberNameSpacePadded[8];
  memset(memberNameSpacePadded, ' ', sizeof(memberNameSpacePadded));
  unsigned int memberNameLength = strlen(member);
  if (memberNameLength > sizeof(memberNameSpacePadded)) {
    return RC_ZISPARM_MEMBER_NAME_TOO_LONG;
  }
  memcpy(memberNameSpacePadded, member, memberNameLength);

  int status = RC_ZISPARM_OK;
  bool ddnameAllocated = false;

  int allocRSN = 0;
  int allocRC = allocLogicalParmLib(ddnameSpacePadded, &allocRSN);
  if (allocRC == RC_IEFPRMLB_OK) {
    ddnameAllocated = true;
  } else if (allocRC == RC_IEFPRMLB_WARN &&
             allocRSN == RSN_IEFPRMLB_ALREADY_ALLOCATED) {
    ddnameAllocated = false;
  } else {
    ddnameAllocated = false;
    readStatus->internalRC = allocRC;
    readStatus->internalRSN = allocRSN;
    status = RC_ZISPARM_PARMLIB_ALLOC_FAILED;
  }

  IEFPRMLBHeader *readBuffer = NULL;
  unsigned int readBufferSize = READ_BUFFER_DEFAULT_SIZE;
  for (int i = 0; i < 2; i++) {

    if (status != RC_ZISPARM_OK) {
      break;
    }

    readBuffer = allocReadBuffer(readBufferSize);
    if (readBuffer == NULL) {
      status = RC_ZISPARM_READ_BUFFER_ALLOC_FAILED;
      break;
    }

    unsigned int requiredBufferSize = 0;
    int readRSN = 0;
    int readRC = readLogicalParmLib(ddnameSpacePadded, memberNameSpacePadded,
                                    readBuffer, &readRSN);
    if (readRC == RC_IEFPRMLB_OK) {
      break;
    } else if (readRC == RC_IEFPRMLB_FAILED &&
               readRSN == RSN_IEFPRMLB_MEMBER_NOT_FOUND) {
      status = RC_ZISPARM_MEMBER_NOT_FOUND;
      break;
    } else if (readRC == RC_IEFPRMLB_FAILED &&
               readRSN == RSN_IEFPRMLB_BUFFER_FULL) {
      readBufferSize = readBuffer->sizeRequired;
      freeReadBuffer(readBuffer);
      readBuffer = NULL;
    } else {
      readStatus->internalRC = allocRC;
      readStatus->internalRSN = allocRSN;
      status = RC_ZISPARM_PARMLIB_READ_FAILED;
    }

  }

  if (status == RC_ZISPARM_OK) {
    ParmRecordVisitorData visitorData = {
        .eyecatcher = PARM_REC_VISITOR_DATA_EYECATCHER,
        .slh = parms->slh,
        .parmSet = parms
    };
    int iterateRC = iterateMember(readBuffer, handleParmRecord, &visitorData);
    if (iterateRC != RC_ZISPARM_OK) {
      status = iterateRC;
    }
  }

  if (readBuffer != NULL) {
    freeReadBuffer(readBuffer);
    readBuffer = NULL;
    readBufferSize = 0;
  }

  if (ddnameAllocated) {
    int freeRSN = 0;
    int freeRC = freeLogicalParmLib(ddnameSpacePadded, &freeRSN);
    if (freeRC == RC_IEFPRMLB_OK) {
      ddnameAllocated = false;
    } else {
      readStatus->internalRC = allocRC;
      readStatus->internalRSN = allocRSN;
      status = RC_ZISPARM_PARMLIB_FREE_FAILED;
    }
  }

  return status;
}

void zisRemoveParmSet(ZISParmSet *parms) {
  SLHFree(parms->slh);
  parms = NULL;
}

static ZISParmSetEntry *findEntry(ZISParmSet *parms, const char *name) {

  ZISParmSetEntry *parm = parms->firstEntry;
    while (parm != NULL) {
      if (strcmp(parm->key, name) == 0) {
        return parm;
      }
      parm = parm->next;
    }

  return NULL;
}

int zisPutParmValue(ZISParmSet *parms, const char *name, const char *value) {

  ZISParmSetEntry *existingEntry = findEntry(parms, name);
  if (existingEntry != NULL) {
    existingEntry->value = value;
    return RC_ZISPARM_OK;
  }

  ZISParmSetEntry *newParmEntry =
      (ZISParmSetEntry *)SLHAlloc(parms->slh, sizeof(ZISParmSetEntry));
  if (newParmEntry == NULL) {
    return RC_ZISPARM_SLH_ALLOC_FAILED;
  }

  memset(newParmEntry, 0, sizeof(ZISParmSetEntry));
  memcpy(newParmEntry->eyecatcher, ZIS_PARMSET_ENTRY_EYECATCHER,
         sizeof(newParmEntry->eyecatcher));
  newParmEntry->key = name;
  newParmEntry->value = value;

  newParmEntry->next = parms->firstEntry;
  parms->firstEntry = newParmEntry;

  return RC_ZISPARM_OK;
}

const char *zisGetParmValue(const ZISParmSet *parms, const char *name) {

  ZISParmSetEntry *entry = findEntry((ZISParmSet *)parms, name);
  if (entry != NULL) {
    return entry->value;
  }

  return NULL;
}

int zisLoadParmsToServer(CrossMemoryServer *server, const ZISParmSet *parms,
                         int *reasonCode) {

  ZISParmSetEntry *parm = parms->firstEntry;
  while (parm != NULL) {

    /* the only type we support for now is char */
    int cmsRC = cmsAddConfigParm(server, parm->key, parm->value,
                                 CMS_CONFIG_PARM_TYPE_CHAR);
    if (cmsRC != RC_CMS_OK) {
      *reasonCode = cmsRC;
      return RC_ZISPARM_CMS_CALL_FAILED;
    }

    parm = parm->next;
  }

  return RC_ZISPARM_OK;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

