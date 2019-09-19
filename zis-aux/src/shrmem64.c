

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#else
#include "stddef.h"
#include "stdint.h"
#endif

#include "shrmem64.h"
#include "zos.h"

#ifndef _LP64
#error ILP32 is not supported
#endif

typedef uint64_t MemObj;

#define IARV64_V4PLIST_SIZE 160

#define IS_IARV64_OK($rc) (($rc) < 8)

#define MAKE_RSN($functionRC, $iarv64RC, $iarv64RSN) \
({ \
  (((unsigned)$functionRC << 24) | ((unsigned)$iarv64RC << 16) | (($iarv64RSN >> 8) & 0x0000FFFF)); \
})

static MemObj getSharedMemObject(uint64_t segmentCount,
                                 MemObjToken token,
                                 uint32_t *iarv64RC,
                                 uint32_t *iarv64RSN) {

  MemObj result = 0;
  int localRC = 0;
  int localRSN = 0;
  char parmList[IARV64_V4PLIST_SIZE] = {0};

  __asm(
      ASM_PREFIX
      "         IARV64 REQUEST=GETSHARED"
      ",USERTKN=(%[token])"
      ",COND=YES"
      ",SEGMENTS=(%[size])"
      ",ORIGIN=(%[result])"
      ",RETCODE=%[rc]"
      ",RSNCODE=%[rsn]"
      ",PLISTVER=4"
      ",MF=(E,(%[parm]),COMPLETE)                                              \n"
      : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
      : [token]"r"(&token), [size]"r"(&segmentCount), [result]"r"(&result),
        [parm]"r"(&parmList)
      : "r0", "r1", "r14", "r15"
  );

  if (iarv64RC) {
    *iarv64RC = localRC;
  }
  if (iarv64RSN) {
    *iarv64RSN = localRSN;
  }

  return result;
}

static void shareMemObject(MemObj object,
                           MemObjToken token,
                           uint32_t *iarv64RC,
                           uint32_t *iarv64RSN) {

  int localRC = 0;
  int localRSN = 0;
  char parmList[IARV64_V4PLIST_SIZE] = {0};

  struct {
    MemObj vsa;
    uint64_t reserved;
  } rangeList = {object, 0};

  uint64_t rangeListAddress = (uint64_t)&rangeList;

  __asm(
      ASM_PREFIX
      "         IARV64 REQUEST=SHAREMEMOBJ"
      ",USERTKN=(%[token])"
      ",RANGLIST=(%[range])"
      ",NUMRANGE=1"
      ",COND=YES"
      ",RETCODE=%[rc]"
      ",RSNCODE=%[rsn]"
      ",PLISTVER=4"
      ",MF=(E,(%[parm]),COMPLETE)                                              \n"
      : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
      : [token]"r"(&token), [range]"r"(&rangeListAddress), [parm]"r"(&parmList)
      : "r0", "r1", "r14", "r15"
  );

  if (iarv64RC) {
    *iarv64RC = localRC;
  }
  if (iarv64RSN) {
    *iarv64RSN = localRSN;
  }

}

static void detachSingleSharedMemObject(MemObj object,
                                        MemObjToken token,
                                        uint32_t *iarv64RC,
                                        uint32_t *iarv64RSN) {

  int localRC = 0;
  int localRSN = 0;
  char parmList[IARV64_V4PLIST_SIZE] = {0};

  __asm(
      ASM_PREFIX
      "         IARV64 REQUEST=DETACH"
      ",MATCH=SINGLE"
      ",MEMOBJSTART=(%[mobj])"
      ",MOTKN=(%[token])"
      ",MOTKNCREATOR=USER"
      ",AFFINITY=LOCAL"
      ",OWNER=YES"
      ",COND=YES"
      ",RETCODE=%[rc]"
      ",RSNCODE=%[rsn]"
      ",PLISTVER=4"
      ",MF=(E,(%[parm]),COMPLETE)                                              \n"
      : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
      : [mobj]"r"(&object), [parm]"r"(&parmList), [token]"r"(&token)
      : "r0", "r1", "r14", "r15"
  );

  if (iarv64RC){
    *iarv64RC = localRC;
  }
  if (iarv64RSN){
    *iarv64RSN = localRSN;
  }

}

static void detachSharedMemObjects(MemObjToken token,
                                   uint32_t *iarv64RC,
                                   uint32_t *iarv64RSN) {

  int localRC = 0;
  int localRSN = 0;
  char parmList[IARV64_V4PLIST_SIZE] = {0};

  __asm(
      ASM_PREFIX
      "         IARV64 REQUEST=DETACH"
      ",MATCH=MOTOKEN"
      ",MOTKN=(%[token])"
      ",MOTKNCREATOR=USER"
      ",AFFINITY=LOCAL"
      ",OWNER=YES"
      ",COND=YES"
      ",RETCODE=%[rc]"
      ",RSNCODE=%[rsn]"
      ",PLISTVER=4"
      ",MF=(E,(%[parm]),COMPLETE)                                              \n"
      : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
      : [token]"r"(&token), [parm]"r"(&parmList)
      : "r0", "r1", "r14", "r15"
  );

  if (iarv64RC){
    *iarv64RC = localRC;
  }
  if (iarv64RSN){
    *iarv64RSN = localRSN;
  }

}


static void removeSystemInterestForAllObjects(MemObjToken token,
                                              uint32_t *iarv64RC,
                                              uint32_t *iarv64RSN) {

  int localRC = 0;
  int localRSN = 0;
  char parmList[IARV64_V4PLIST_SIZE] = {0};

  __asm(
      ASM_PREFIX
      "         IARV64 REQUEST=DETACH"
      ",MATCH=MOTOKEN"
      ",MOTKN=(%[token])"
      ",MOTKNCREATOR=USER"
      ",AFFINITY=SYSTEM"
      ",COND=YES"
      ",RETCODE=%[rc]"
      ",RSNCODE=%[rsn]"
      ",PLISTVER=4"
      ",MF=(E,(%[parm]),COMPLETE)                                              \n"
      : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
      : [token]"r"(&token), [parm]"r"(&parmList)
      : "r0", "r1", "r14", "r15"
  );

  if (iarv64RC){
    *iarv64RC = localRC;
  }
  if (iarv64RSN){
    *iarv64RSN = localRSN;
  }

}

static void removeSystemInterestForSingleObject(MemObj object,
                                                MemObjToken token,
                                                uint32_t *iarv64RC,
                                                uint32_t *iarv64RSN) {

  int localRC = 0;
  int localRSN = 0;
  char parmList[IARV64_V4PLIST_SIZE] = {0};

  __asm(
      ASM_PREFIX
      "         IARV64 REQUEST=DETACH"
      ",MATCH=SINGLE"
      ",MEMOBJSTART=(%[mobj])"
      ",MOTKN=(%[token])"
      ",MOTKNCREATOR=USER"
      ",AFFINITY=SYSTEM"
      ",COND=YES"
      ",RETCODE=%[rc]"
      ",RSNCODE=%[rsn]"
      ",PLISTVER=4"
      ",MF=(E,(%[parm]),COMPLETE)                                              \n"
      : [rc]"=m"(localRC), [rsn]"=m"(localRSN)
      : [mobj]"r"(&object), [token]"r"(&token), [parm]"r"(&parmList)
      : "r0", "r1", "r14", "r15"
  );

  if (iarv64RC){
    *iarv64RC = localRC;
  }
  if (iarv64RSN){
    *iarv64RSN = localRSN;
  }

}

MemObjToken shrmem64GetAddressSpaceToken(void) {

  union {
    MemObj token;
    struct {
      ASCB * __ptr32 ascb;
      uint32_t asid;
    };
  } result = { .ascb = getASCB(), .asid = result.ascb->ascbasid };

  return result.token;
}

int shrmem64Alloc(MemObjToken userToken, size_t size, void **result, int *rsn) {

  uint32_t iarv64RC = 0, iarv64RSN = 0;

  uint64_t segmentCount = 0;
  if ((size & 0xFFFFF) == 0){
    segmentCount = size >> 20;
  } else{
    segmentCount = (size >> 20) + 1;
  }

  MemObj mobj = getSharedMemObject(segmentCount, userToken,
                                   &iarv64RC, &iarv64RSN);
  if (!IS_IARV64_OK(iarv64RC)) {
    *rsn = MAKE_RSN(RC_SHRMEM64_GETSHARED_FAILED, iarv64RC, iarv64RSN);
    return RC_SHRMEM64_GETSHARED_FAILED;
  }

  *result = (void *)mobj;

  return RC_SHRMEM64_OK;
}

int shrmem64Release(MemObjToken userToken, void *target, int *rsn) {

  uint32_t iarv64RC = 0, iarv64RSN = 0;

  MemObj mobj = (MemObj)target;

  removeSystemInterestForSingleObject(mobj, userToken, &iarv64RC, &iarv64RSN);
  if (!IS_IARV64_OK(iarv64RC)) {
    *rsn = MAKE_RSN(RC_SHRMEM64_SINGLE_SYS_DETACH_FAILED, iarv64RC, iarv64RSN);
    return RC_SHRMEM64_SINGLE_SYS_DETACH_FAILED;
  }

  return RC_SHRMEM64_OK;
}

int shrmem64ReleaseAll(MemObjToken userToken, int *rsn) {

  uint32_t iarv64RC = 0, iarv64RSN = 0;

  removeSystemInterestForAllObjects(userToken, &iarv64RC, &iarv64RSN);
  if (!IS_IARV64_OK(iarv64RC)) {
    *rsn = MAKE_RSN(RC_SHRMEM64_ALL_SYS_DETACH_FAILED, iarv64RC, iarv64RSN);
    return RC_SHRMEM64_ALL_SYS_DETACH_FAILED;
  }

  return RC_SHRMEM64_OK;
}

int shrmem64GetAccess(MemObjToken userToken, void *target, int *rsn) {

  uint32_t iarv64RC = 0, iarv64RSN = 0;

  MemObj mobj = (MemObj)target;

  shareMemObject(mobj, userToken, &iarv64RC, &iarv64RSN);
  if (!IS_IARV64_OK(iarv64RC)) {
    *rsn = MAKE_RSN(RC_SHRMEM64_SHAREMEMOBJ_FAILED, iarv64RC, iarv64RSN);
    return RC_SHRMEM64_SHAREMEMOBJ_FAILED;
  }

  return RC_SHRMEM64_OK;
}

int shrmem64RemoveAccess(MemObjToken userToken, void *target, int *rsn) {

  uint32_t iarv64RC = 0, iarv64RSN = 0;

  MemObj mobj = (MemObj)target;

  detachSingleSharedMemObject(mobj, userToken, &iarv64RC, &iarv64RSN);
  if (!IS_IARV64_OK(iarv64RC)) {
    *rsn = MAKE_RSN(RC_SHRMEM64_DETACH_FAILED, iarv64RC, iarv64RSN);
    return RC_SHRMEM64_DETACH_FAILED;
  }

  return RC_SHRMEM64_OK;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
