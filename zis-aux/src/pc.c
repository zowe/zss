

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stdbool.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#else
#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"
#include "stdlib.h"
#include "string.h"
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "pc.h"


int pcSetAllAddressSpaceAuthority(int *reasonCode) {

  int axsetRC = 0;
  __asm(
      ASM_PREFIX

      "         SYSSTATE PUSH                                                  \n"
      "         SYSSTATE OSREL=ZOSV1R6                                         \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif

      "         LA    1,1                 allow SSAR authority to all A/S      \n"
      "         AXSET AX=(1)                                                   \n"

#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         SYSSTATE POP                                                   \n"

      "         ST    15,%[rc]                                                 \n"

      : [rc]"=m"(axsetRC)
      :
      : "r0", "r1", "r14", "r15"
  );

  if (axsetRC != 0) {
    *reasonCode = axsetRC;
    return RC_PC_AXSET_FAILED;
  }

  return RC_PC_OK;
}


ZOWE_PRAGMA_PACK

typedef struct PCLinkageIndexList_tag {
  int indexCount;
  PCLinkageIndex entries[32];
} PCLinkageIndexList;

typedef struct PCEntryTableTokenList_tag {
  int tokenCount;
  PCEntryTableToken entries[32];
} PCEntryTableTokenList;

ZOWE_PRAGMA_PACK_RESET


static int reserveLinkageIndex(PCLinkageIndexList * __ptr32 indexList,
                               bool isSystem, bool isReusable,
                               PCLinkageIndexSize lxSize) {

  typedef struct LXRESParmList_tag {
    uint8_t formatByte;
#define LXRES_FORMAT_BYTE 0x00
    uint8_t optionByte;
#define LXRES_OPT_SYSTEM    0x80
#define LXRES_OPT_REUSE     0x40
#define LXRES_OPT_SIZE_16   0x20
#define LXRES_OPT_SIZE_23   0x10
#define LXRES_OPT_SIZE_24   0x08
#define LXRES_OPT_EXLIST    0x04
#define LXRES_OPT_RESERVED1 0x02
#define LXRES_OPT_RESERVED2 0x01
    char reserved[2];
    PCLinkageIndexList * __ptr32 elxList;
  } LXRESParmList;

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      LXRESParmList parmList;
    )
  );

  below2G->parmList.formatByte = LXRES_FORMAT_BYTE;

  if (isSystem) {
    below2G->parmList.optionByte |= LXRES_OPT_SYSTEM;
  }

  if (isReusable) {
    below2G->parmList.optionByte |= LXRES_OPT_REUSE;
  }

  switch (lxSize) {
  case LX_SIZE_12:
    break;
  case LX_SIZE_16:
    below2G->parmList.optionByte |= LXRES_OPT_SIZE_16;
    break;
  case LX_SIZE_23:
    below2G->parmList.optionByte |= LXRES_OPT_SIZE_23;
    break;
  case LX_SIZE_24:
    below2G->parmList.optionByte |= LXRES_OPT_SIZE_24;
    break;
  default:
    break;
  }

  below2G->parmList.optionByte |= LXRES_OPT_EXLIST;
  below2G->parmList.elxList = indexList;

  int lxresRC = 0;
  __asm(
      ASM_PREFIX

      "         SYSSTATE PUSH                                                  \n"
      "         SYSSTATE OSREL=ZOSV1R6                                         \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif

      "         LXRES MF=(E,(%[parm]))                                         \n"

#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         SYSSTATE POP                                                   \n"

      "         ST    15,%[rc]                                                 \n"

      : [rc]"=m"(lxresRC)
      : [parm]"r"(&below2G->parmList)
      : "r0", "r1", "r14", "r15"
  );

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return lxresRC;
}


static int freeLinkageIndex(PCLinkageIndexList * __ptr32 indexList,
                            bool forced) {

  typedef struct LXFREParmList_tag {
    uint8_t formatByte;
#define LXFRE_FORMAT_BYTE 0x00
    uint8_t optionByte;
#define LXFRE_OPT_FORCE   0x80
#define LXFRE_OPT_EXLIST  0x40
    char reserved[2];
    PCLinkageIndexList * __ptr32 elxList;
  } LXFREParmList;

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      LXFREParmList parmList;
    )
  );

  below2G->parmList.formatByte = LXFRE_FORMAT_BYTE;

  if (forced) {
    below2G->parmList.optionByte |= LXFRE_OPT_FORCE;
  }

  below2G->parmList.optionByte |= LXFRE_OPT_EXLIST;

  below2G->parmList.elxList = indexList;

  int lxfreRC = 0;
  __asm(
      ASM_PREFIX

      "         SYSSTATE PUSH                                                  \n"
      "         SYSSTATE OSREL=ZOSV1R6                                         \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif
      "         LXFRE MF=(E,(%[parm]))                                         \n"
#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         SYSSTATE POP                                                   \n"

      "         ST    15,%[rc]                                                 \n"

      : [rc]"=m"(lxfreRC)
      : [parm]"r"(&below2G->parmList)
      : "r0", "r1", "r14", "r15"
  );

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return lxfreRC;
}


int pcReserveLinkageIndex(bool isSystem, bool isReusable,
                          PCLinkageIndexSize indexSize,
                          PCLinkageIndex *result,
                          int *reasonCode) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      PCLinkageIndexList indexList;
    )
  );

  below2G->indexList.indexCount = 1;

  int rc = reserveLinkageIndex(&below2G->indexList, isSystem, isReusable, indexSize);
  if (rc == 0) {
    *result = below2G->indexList.entries[0];
  }

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  if (rc != 0) {
    *reasonCode = rc;
    return RC_PC_LXRES_FAILED;
  }

  return RC_PC_OK;
}


int pcFreeLinkageIndex(PCLinkageIndex index, bool forced, int *reasonCode) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      PCLinkageIndexList indexList;
    )
  );

  below2G->indexList.indexCount = 1;
  below2G->indexList.entries[0] = index;

  int rc = freeLinkageIndex(&below2G->indexList, forced);

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  if (rc != 0) {
    *reasonCode = rc;
    return RC_PC_LXFRE_FAILED;
  }

  return RC_PC_OK;
}

ZOWE_PRAGMA_PACK

#define ETD_MAX_ENTRY_COUNT 256

typedef struct ETDELE_tag {
  uint8_t index;
  uint8_t flag;
#define ETDELE_FLAG_SUP       0x80
#define ETDELE_FLAG_SSWITCH   0x40
  char reserved[2];
  union {
    char pro[8];
    struct {
      uint32_t zeros;
      uint32_t address;
    } routine;
  };
  uint16_t akm;
  char ekm[2];
  uint32_t par1;
  uint8_t optb1;
#define ETDELE_OPTB1_STACKING_PC    0x80
#define ETDELE_OPTB1_EXTENDED_AMODE 0x40
#define ETDELE_OPTB1_REPLACE_PSW    0x10
#define ETDELE_OPTB1_REPLACE_PKM    0x08
#define ETDELE_OPTB1_REPLACE_EAX    0x04
#define ETDELE_OPTB1_AR_MODE        0x02
#define ETDELE_OPTB1_SASN_NEW       0x01
  uint8_t ek;
  char eax[2];
  char arr[8];
  uint32_t par2;
  char lpafl[4];
} ETDELE;

typedef struct ETD_tag {
  char format;
#define ETD_FORMAT 0x01
  char hflag;
  short entryNumber;
  ETDELE entries[ETD_MAX_ENTRY_COUNT];
} ETD;


ZOWE_PRAGMA_PACK_RESET

typedef struct ETD_tag EntryTableDescriptor;


EntryTableDescriptor *pcMakeEntryTableDescriptor(void) {

  EntryTableDescriptor *descriptor =
      (EntryTableDescriptor *)safeMalloc(sizeof(EntryTableDescriptor), "ETD");
  if (descriptor == NULL) {
    return NULL;
  }

  memset(descriptor, 0, sizeof(EntryTableDescriptor));

  descriptor->format = ETD_FORMAT;

  return descriptor;
}


int pcAddToEntryTableDescriptor(EntryTableDescriptor *descriptor,
                                int (* __ptr32 routine)(void),
                                uint32_t routineParameter1,
                                uint32_t routineParameter2,
                                bool isSASNOld,
                                bool isAMODE64,
                                bool isSUP,
                                bool isSpaceSwitch,
                                int key) {

  if (descriptor->entryNumber == ETD_MAX_ENTRY_COUNT) {
    return RC_PC_ETD_FULL;
  }

  ETDELE *entry = &descriptor->entries[descriptor->entryNumber];
  memset(entry, 0, sizeof(ETDELE));

  entry->index = descriptor->entryNumber;

  if (isSUP) {
    entry->flag |= ETDELE_FLAG_SUP;
  }
  if (isSpaceSwitch) {
    entry->flag |= ETDELE_FLAG_SSWITCH;
  }

  entry->routine.address = (uint32_t)routine;
  entry->akm = 0xFFFF; /* AKM 0:15 */
  entry->par1 = routineParameter1;
  entry->par2 = routineParameter2;

  entry->optb1 |= ETDELE_OPTB1_STACKING_PC;
  entry->optb1 |= ETDELE_OPTB1_REPLACE_PSW;
  if (!isSASNOld) {
    entry->optb1 |= ETDELE_OPTB1_SASN_NEW;
  }
  if (isAMODE64) {
    entry->optb1 |= ETDELE_OPTB1_EXTENDED_AMODE;
  }

  entry->ek = (key << 4);

  descriptor->entryNumber++;

  return RC_PC_OK;
}


void pcRemoveEntryTableDescriptor(EntryTableDescriptor *descriptor) {
  safeFree((char *)descriptor, sizeof(EntryTableDescriptor));
}


int pcCreateEntryTable(const EntryTableDescriptor *descriptor,
                       PCEntryTableToken *token,
                       int *reasonCode) {

  int etcreRC = 0;
  __asm(
      ASM_PREFIX

      "         SYSSTATE PUSH                                                  \n"
      "         SYSSTATE OSREL=ZOSV1R6                                         \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif

      "         ETCRE ENTRIES=(%[descriptor])                                  \n"

#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         SYSSTATE POP                                                   \n"

      "         ST    15,%[rc]                                                 \n"
      "         ST    0,0(%[token])                                            \n"

      : [rc]"=m"(etcreRC)
      : [descriptor]"r"(descriptor), [token]"r"(token)
      : "r0", "r1", "r14", "r15"
  );

  if (etcreRC != 0) {
    *reasonCode = etcreRC;
    return RC_PC_ETCRE_FAILED;
  }

  return RC_PC_OK;
}


int pcDestroyEntryTable(PCEntryTableToken token, bool purge, int *reasonCode) {

  typedef struct ETDESParmList_tag {
    uint8_t formatByte;
#define ETDES_FORMAT_BYTE 0x00
    uint8_t optionByte;
  #define ETDES_OPT_PURGE   0x80
    char reserved[2];
    PCEntryTableToken * __ptr32 token;
  } ETDESParmList;

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      ETDESParmList parmList;
      PCEntryTableToken token;
    )
  );

  below2G->parmList.formatByte = ETDES_FORMAT_BYTE;

  if (purge) {
    below2G->parmList.optionByte |= ETDES_OPT_PURGE;
  }

  below2G->token = token;
  below2G->parmList.token = &below2G->token;

  int etdesRC = 0;
  __asm(
      ASM_PREFIX

      "         SYSSTATE PUSH                                                  \n"
      "         SYSSTATE OSREL=ZOSV1R6                                         \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif

      "         ETDES MF=(E,(%[parm]))                                         \n"

#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         SYSSTATE POP                                                   \n"

      "         ST    15,%[rc]                                                 \n"

      : [rc]"=m"(etdesRC)
      : [parm]"r"(&below2G->parmList)
      : "r0", "r1", "r14", "r15"
  );

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  if (etdesRC != 0) {
    *reasonCode = etdesRC;
    return RC_PC_ETDES_FAILED;
  }

  return RC_PC_OK;
}


static int connectEntryTable(PCEntryTableTokenList * __ptr32 tokenList,
                             PCLinkageIndexList * __ptr32 elxList) {

  typedef struct ETCONParmList_tag {
    uint8_t formatByte;
#define ETCON_FORMAT_BYTE 0x00
    uint8_t optionByte;
#define ETCON_OPT_EXLIST  0x80
    char reserved[2];
    PCEntryTableTokenList* __ptr32 tokenList;
    PCLinkageIndexList * __ptr32 elxList;
  } ETCONParmList;

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      ETCONParmList parmList;
    )
  );

  below2G->parmList.formatByte = ETCON_FORMAT_BYTE;
  below2G->parmList.optionByte |= ETCON_OPT_EXLIST;
  below2G->parmList.tokenList = tokenList;
  below2G->parmList.elxList = elxList;

  int etconRC = 0;
  __asm(
      ASM_PREFIX

      "         SYSSTATE PUSH                                                  \n"
      "         SYSSTATE OSREL=ZOSV1R6                                         \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif

      "         ETCON MF=(E,(%[parm]))                                         \n"

#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         SYSSTATE POP                                                   \n"

      "         ST    15,%[rc]                                                 \n"

      : [rc]"=m"(etconRC)
      : [parm]"r"(&below2G->parmList)
      : "r0", "r1", "r14", "r15"
  );

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  return etconRC;
}


static int disconnectEntryTable(PCEntryTableTokenList * __ptr32 tokenList) {

  int etdisRC = 0;
  __asm(
      ASM_PREFIX

      "         SYSSTATE PUSH                                                  \n"
      "         SYSSTATE OSREL=ZOSV1R6                                         \n"
#ifdef _LP64
      "         SAM31                                                          \n"
      "         SYSSTATE AMODE64=NO                                            \n"
#endif

      "         ETDIS TKLIST=(%[tokens])                                       \n"

#ifdef _LP64
      "         SAM64                                                          \n"
      "         SYSSTATE AMODE64=YES                                           \n"
#endif
      "         SYSSTATE POP                                                   \n"

      "         ST    15,%[rc]                                                 \n"

      : [rc]"=m"(etdisRC)
      : [tokens]"r"(tokenList)
      : "r0", "r1", "r14", "r15"
  );

  return etdisRC;
}


int pcConnectEntryTable(PCEntryTableToken token, PCLinkageIndex index,
                        int *reasonCode) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      PCEntryTableTokenList tokenList;
      PCLinkageIndexList indexList;
    )
  );

  below2G->tokenList.tokenCount = 1;
  below2G->tokenList.entries[0] = token;

  below2G->indexList.indexCount = 1;
  below2G->indexList.entries[0] = index;

  int rc = connectEntryTable(&below2G->tokenList, &below2G->indexList);

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  if (rc != 0) {
    *reasonCode = rc;
    return RC_PC_ETCON_FAILED;
  }

  return RC_PC_OK;
}

int pcDisconnectEntryTable(PCEntryTableToken token, int *reasonCode) {

  ALLOC_STRUCT31(
    STRUCT31_NAME(below2G),
    STRUCT31_FIELDS(
      PCEntryTableTokenList tokenList;
    )
  );

  below2G->tokenList.tokenCount = 1;
  below2G->tokenList.entries[0] = token;

  int rc = disconnectEntryTable(&below2G->tokenList);

  FREE_STRUCT31(
    STRUCT31_NAME(below2G)
  );

  if (rc != 0) {
    *reasonCode = rc;
    return RC_PC_ETDIS_FAILED;
  }

  return RC_PC_OK;
}

int pcCallRoutine(uint32_t pcNumber, uint32_t sequenceNumber, void *parmBlock) {

  int returnCode = 0;
  __asm(
      ASM_PREFIX
      "         LMH   15,15,%[sn]                                              \n"
      "         L     14,%[pcn]                                                \n"
      "         XGR   1,1                                                      \n"
      "         LA    1,0(%[parm])                                             \n"
      "         PC    0(14)                                                    \n"
      "         ST    15,%[rc]                                                 \n"
      : [rc]"=m"(returnCode)
      : [pcn]"m"(pcNumber), [sn]"m"(sequenceNumber), [parm]"r"(parmBlock)
      : "r1", "r14", "r15"
  );

  return returnCode;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

