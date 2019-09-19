

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef SRC_AS_H_
#define SRC_AS_H_

#ifdef METTLE
#include <metal/stdint.h>
#else
#include <stdint.h>
#endif

#include "zowetypes.h"
#include "zos.h"

#ifndef __LONGNAME__

#define addressSpaceCreate          ASCREATE
#define addressSpaceCreateWithTerm  ASCREATT
#define addressSpaceTerminate       ASTERMIN
#define addressSpaceExtractParm     ASEXTRCT

#endif

ZOWE_PRAGMA_PACK

typedef struct ASOutputData_tag {
  uint64_t stoken;
  ASCB * __ptr32 ascb;
  void * __ptr32 commECBs;
  char reserved[8];
} ASOutputData;

typedef struct ASUserToken {
  char data[8];
} ASUserToken;

typedef struct ASParmString_tag {
  uint16_t length;
  char text[124];
} ASParmString;

typedef struct ASParm_tag {
  uint16_t length;
  char data[254];
} ASParm;

ZOWE_PRAGMA_PACK_RESET

#define AS_ATTR_XMPT      0x00800000
#define AS_ATTR_PXMT      0x00400000
#define AS_ATTR_NOMT      0x00200000
#define AS_ATTR_NOMD      0x00100000
#define AS_ATTR_1LPU      0x00080000
#define AS_ATTR_2LPU      0x00040000
#define AS_ATTR_N2LP      0x00020000
#define AS_ATTR_PRIV      0x00010000
#define AS_ATTR_NOSWAP    0x00008000
#define AS_ATTR_PERM      0x00004000
#define AS_ATTR_CANCEL    0x00002000
#define AS_ATTR_RESERVED  0x00001000
#define AS_ATTR_HIPRI     0x00000800
#define AS_ATTR_NONURG    0x00000400
#define AS_ATTR_KEEPRGN   0x00000200
#define AS_ATTR_REUSASID  0x00000100
#define AS_ATTR_JOBSPACE  0x00000080
#define AS_ATTR_ASCBV31   0x00000040
#define AS_ATTR_MAXRGN    0x00000020

int addressSpaceCreate(const ASParmString *startParmString,
                       const ASParm *parm,
                       uint32_t attributes,
                       ASOutputData *result,
                       int *reasonCode);

#ifdef METTLE

typedef void ASCRETermCallback(void *userParm);

int addressSpaceCreateWithTerm(const ASParmString *startParmString,
                               const ASParm *parm,
                               uint32_t attributes,
                               ASOutputData *result,
                               ASCRETermCallback *termCallback,
                               void * __ptr32 termCallbackParm,
                               int *reasonCode);

#endif /* METTLE */

int addressSpaceTerminate(uint64_t stoken, int *reasonCode);

int addressSpaceExtractParm(ASParm **parm, int *reasonCode);

#endif /* SRC_AS_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
