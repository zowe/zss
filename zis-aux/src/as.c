

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
#include <metal/stdio.h>
#include <metal/string.h>
#else
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#endif

#include "zowetypes.h"
#include "alloc.h"

#include "as.h"

#ifndef __ZOWE_OS_ZOS
#error z/OS targets are supported only
#endif

ZOWE_PRAGMA_PACK

typedef struct ASCrossMemoryInfo_tag {
  void * __ptr32 axlist;
  void * __ptr32 tklist;
  void * __ptr32 elxlist;
} ASCrossMemoryParms;

ZOWE_PRAGMA_PACK_RESET

typedef struct ASMacroParmList_tag {

  uint8_t version;
#define ASCRE_VERSION 1
  uint32_t attributes : 24;

  EightCharString * __ptr32 initRoutineName;
  uint32_t initRoutineNameALET;

  ASParmString * __ptr32 startParmString;
  uint32_t startParmStringALET;

  ASUserToken * __ptr32 utoken;
  uint32_t utokenALET;

  ASParm * __ptr32 parm;
  uint32_t parmALET;

  void * __ptr32 termRoutine;

  void * __ptr32 axlist;
  uint32_t axlistALET;
  void * __ptr32 tklist;
  uint32_t tklistALET;
  void * __ptr32 elxlist;
  uint32_t elxlistALET;

  ASOutputData * __ptr32 oda;
  uint32_t odaALET;

  char reserved[8];

} ASMacroParmList;

#ifdef METTLE

static Addr31 getTermExitWrapper(void) {

  Addr31 wrapper = NULL;

  __asm(
      ASM_PREFIX
      "         LARL  1,L$TEW                                                  \n"
      "         ST    1,%[wrapper]                                             \n"
      "         J     L$TEWEX                                                  \n"
      "L$TEW    DS    0H                                                       \n"

      /* establish addressability */
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         BAKR  14,0                                                     \n"
      "         LARL  10,L$TEW                                                 \n"
      "         USING L$TEW,10                                                 \n"
      /* handle input parms */
      "         LTR   2,1                 UTOKEN provided?                     \n"
      "         BZ    L$TEWRT             no, leave                            \n"
      "         USING ASCREUTK,2                                               \n"
      "         LLGTR 2,2                 make sure higher bits are clean      \n"
      "         LLGT  15,TEWUFUNC         load function provided               \n"
      "         LTR   15,15               is it zero?                          \n"
      "         BZ    L$TEWRT             yes, leave                           \n"
      /* handle stack */
      "         STORAGE OBTAIN"
      ",COND=YES,LENGTH=65536,CALLRKY=YES,SP=132,LOC=31,BNDRY=PAGE             \n"
      "         LTR   15,15               check the RC                         \n"
      "         BNZ   L$TEWRT             bad RC, leave                        \n"
      "         LLGTR 13,1                load stack to R13                    \n"
      "         USING TEWSA,13                                                 \n"
#ifdef _LP64
      "         MVC   TEWSAEYE,=C'F1SA'   stack eyecatcher                     \n"
#endif
      "         LA    5,TEWSANSA          address of next save area            \n"
      "         ST    5,TEWSANXT          save next save area address          \n"
      "         MVC   TEWSAUP,TEWUPRM     user data                            \n"
      /* call user routine */
      "         LLGT  15,TEWUFUNC         user routine                         \n"
      "         LA    1,TEWSAPRM          load pamlist address to R1           \n"
#ifdef _LP64
      "         SAM64                     go AMODE64 if needed                 \n"
#endif
      "         BASR  14,15               call user function                   \n"
#ifdef _LP64
      "         SAM31                     go back to AMODE31                   \n"
#endif
      /* return to caller */
      "         LR    1,13                load save are to R1 for release      \n"
      "         STORAGE RELEASE"
      ",COND=YES,LENGTH=65536,CALLRKY=YES,SP=132,ADDR=(1)                      \n"
      "L$TEWRT  DS    0H                                                       \n"
      "         PR    ,                   return                               \n"
      /* non executable code */
      "         LTORG                                                          \n"
      "         POP   USING                                                    \n"

      "L$TEWEX  DS    0H                                                       \n"
      : [wrapper]"=m"(wrapper)
      :
      : "r1"
  );

  return wrapper;
}

#endif /* METTLE */

static int addressSpaceCreateInternal(const ASParmString *startParmString,
                                      const ASParm *parm,
                                      const EightCharString *initProgramName,
                                      void * __ptr32 termRoutine,
                                      ASUserToken *termRoutineUserToken,
                                      uint16_t attributes,
                                      ASCrossMemoryParms *xmemParms,
                                      ASOutputData *result,
                                      int *reasonCode) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      ASParmString startParmString;
      ASParm parm;
      EightCharString initProgramName;
      ASUserToken utoken;
      ASOutputData oda;
      ASMacroParmList parmList;
    )
  );

  /* Init 31-bit data */
  if (startParmString) {
    below2G->startParmString = *startParmString;
  }

  if (parm) {
    below2G->parm = *parm;
  }

  if (initProgramName) {
    below2G->initProgramName = *initProgramName;
  } else {
    EightCharString dummyInit = {"IEFBR14 "};
    below2G->initProgramName = dummyInit;
  }

  if (termRoutine && termRoutineUserToken) {
    below2G->utoken = *termRoutineUserToken;
  }

  /* Init ASCRE parameter list */
  ASMacroParmList * __ptr32 parmList = &below2G->parmList;
  parmList->version = ASCRE_VERSION;
  parmList->attributes = attributes;
  parmList->initRoutineName = &below2G->initProgramName;
  parmList->startParmString = &below2G->startParmString;
  parmList->utoken = termRoutine ? &below2G->utoken : NULL;
  parmList->parm = &below2G->parm;
  parmList->termRoutine = termRoutine;
  if (xmemParms) {
    parmList->axlist = xmemParms->axlist;
    parmList->tklist = xmemParms->tklist;
    parmList->elxlist = xmemParms->elxlist;
  }
  parmList->oda = &below2G->oda;

  int rc = 0, rsn = 0;
  __asm(
      ASM_PREFIX
      "L@ASCRE  DS    0H                                                       \n"
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         LARL  10,L@ASCRE                                               \n"
      "         USING L@ASCRE,10                                               \n"

      "         ASCRE MF=(E,%[parmList])                                       \n"
      "         ST    15,%[rc]                                                 \n"
      "         ST    0,%[rsn]                                                 \n"

      "         POP   USING                                                    \n"
      : [rc]"=m"(rc), [rsn]"=m"(rsn)
      : [parmList]"m"(below2G->parmList)
      : "r0", "r1", "r10", "r14", "r15"
  );

  if (result) {
    *result = below2G->oda;
  }

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );
  below2G = NULL;

  if (reasonCode) {
    *reasonCode = rsn;
  }
  return rc;
}

int addressSpaceCreate(const ASParmString *startParmString,
                       const ASParm *parm,
                       uint32_t attributes,
                       ASOutputData *result,
                       int *reasonCode) {

  return addressSpaceCreateInternal(startParmString, parm, NULL, NULL, NULL,
                                    attributes, NULL, result, reasonCode);

}

#ifdef METTLE

int addressSpaceCreateWithTerm(const ASParmString *startParmString,
                               const ASParm *parm,
                               uint32_t attributes,
                               ASOutputData *result,
                               ASCRETermCallback *termCallback,
                               void * __ptr32 termCallbackParm,
                               int *reasonCode) {

  __packed union {
    ASUserToken tokenValue;
    __packed struct {
      ASCRETermCallback * __ptr32 termCallback;
      void * __ptr32 termCallbackParm;
    };
  } utoken = {
      .termCallback = termCallback,
      .termCallbackParm = termCallbackParm,
  };

  return addressSpaceCreateInternal(startParmString, parm, NULL,
                                    getTermExitWrapper(), &utoken.tokenValue,
                                    attributes, NULL, result, reasonCode);

}

#endif /* METTLE */

int addressSpaceTerminate(uint64_t stoken, int *reasonCode) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      uint64_t stoken;
    )
  );

  below2G->stoken = stoken;

  int rc = 0, rsn = 0;
  __asm(
      ASM_PREFIX
      "L@ASDES  DS    0H                                                       \n"
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         LARL  10,L@ASDES                                               \n"
      "         USING L@ASDES,10                                               \n"

      "         ASDES STOKEN=(%[stoken])                                       \n"
      "         ST    15,%[rc]                                                 \n"
      "         ST    0,%[rsn]                                                 \n"

      "         POP   USING                                                    \n"
      : [rc]"=m"(rc), [rsn]"=m"(rsn)
      : [stoken]"r"(&below2G->stoken)
      : "r0", "r1", "r10", "r14", "r15"
  );

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );
  below2G = NULL;

  if (reasonCode) {
    *reasonCode = rsn;
  }
  return rc;
}

int addressSpaceExtractParm(ASParm **parm, int *reasonCode) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      uint64_t stoken;
    )
  );

  ASParm *__ptr32 result = NULL;
  int rc = 0, rsn = 0;
  __asm(
      ASM_PREFIX
      "L@ASEXT  DS    0H                                                       \n"
      "         PUSH  USING                                                    \n"
      "         DROP                                                           \n"
      "         LARL  10,L@ASEXT                                               \n"
      "         USING L@ASEXT,10                                               \n"

      "         ASEXT ASPARM                                                   \n"
      "         ST    1,%[parm]                                                \n"
      "         ST    15,%[rc]                                                 \n"
      "         ST    0,%[rsn]                                                 \n"

      "         POP   USING                                                    \n"
      : [parm]"=m"(result), [rc]"=m"(rc), [rsn]"=m"(rsn)
      :
      : "r0", "r1", "r10", "r14", "r15"
  );

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );
  below2G = NULL;

  if (parm) {
    *parm = result;
  }

  if (reasonCode) {
    *reasonCode = rsn;
  }
  return rc;
}

#ifdef METTLE

#define addressSpaceDESCTs ASCDSECT
void addressSpaceDESCTs(void) {

  __asm(
#ifndef _LP64
      "TEWSA    DSECT ,                                                        \n"
      /* 31-bit save area */
      "TEWSARSV DS    CL4                                                      \n"
      "TEWSAPRV DS    A                    previous save area                  \n"
      "TEWSANXT DS    A                    next save area                      \n"
      "TEWSAGPR DS    15F                  GPRs                                \n"
      /* parameters on stack */
      "TEWSAPRM DS    0F                   parameter block for Metal C         \n"
      "TEWSAUP  DS    F                    user parms                          \n"
      "TEWSALEN EQU   *-TEWSA                                                  \n"
      "TEWSANSA DS    0F                   top of next save area               \n"
      "         EJECT ,                                                        \n"
#else
      "TEWSA    DSECT ,                                                        \n"
      /* 64-bit save area */
      "TEWSARSV DS    CL4                                                      \n"
      "TEWSAEYE DS    CL4                  eyecatcher F1SA                     \n"
      "TEWSAGPR DS    15D                  GPRs                                \n"
      "TEWSAPD1 DS    F                    padding                             \n"
      "TEWSAPRV DS    A                    previous save area                  \n"
      "TEWSAPD2 DS    F                    padding                             \n"
      "TEWSANXT DS    A                    next save area                      \n"
      /* parameters on stack */
      "TEWSAPRM DS    0F                   parameter block for Metal C         \n"
      "TEWSAPD3 DS    F                    padding                             \n"
      "TEWSAUP  DS    F                    user parms                          \n"
      "TEWSALEN EQU   *-TEWSA                                                  \n"
      "TEWSANSA DS    0D                   top of next save area               \n"
      "         EJECT ,                                                        \n"
#endif

      "ASCREUTK DSECT ,                                                        \n"
      "TEWUFUNC DS    A                    user function                       \n"
      "TEWUPRM  DS    A                    user parm                           \n"
      "RMPLLEN  EQU   *-ASCREUTK                                               \n"
      "         EJECT ,                                                        \n"

#ifdef METTLE
      "         CSECT ,                                                        \n"
      "         RMODE ANY                                                      \n"
#endif
  );

}

#endif /* METTLE */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
