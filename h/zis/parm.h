#ifndef ZIS_PARM_H_
#define ZIS_PARM_H_

#include "crossmemory.h"

#define RC_ZISPARM_OK                       0
#define RC_ZISPARM_MEMBER_NOT_FOUND         2
#define RC_ZISPARM_DDNAME_TOO_LONG          8
#define RC_ZISPARM_MEMBER_NAME_TOO_LONG     9
#define RC_ZISPARM_PARMLIB_ALLOC_FAILED     10
#define RC_ZISPARM_READ_BUFFER_ALLOC_FAILED 11
#define RC_ZISPARM_PARMLIB_READ_FAILED      12
#define RC_ZISPARM_PARMLIB_FREE_FAILED      13
#define RC_ZISPARM_NONZERO_VISITOR_RC       14
#define RC_ZISPARM_SLH_NOT_CREATED          15
#define RC_ZISPARM_SLH_ALLOC_FAILED         16
#define RC_ZISPARM_CMS_CALL_FAILED          17
#define RC_ZISPARM_ALLOC_FAILED             18

#define ZIS_PARM_TEST_PARM_NAME   "TEST_ZIS_PARM"
#define ZIS_PARM_TEST_PARM_VALUE  "THIS VALUE MUST BE AVAILABLE IN ANY SERVICE"

#pragma map(zisMakeParmSet, "ZISMAKPS")
#pragma map(zisRemoveParmSet, "ZISREMPS")
#pragma map(zisReadParmlib, "ZISRDLIB")
#pragma map(zisReadMainParms, "ZISRDMPR")
#pragma map(zisPutParmValue, "ZISPUTPV")
#pragma map(zisGetParmValue, "ZISGETPV")
#pragma map(zisLoadParmsToCMServer, "ZISLOADP")
#pragma map(zisIterateParms, "ZISITERP")

ZOWE_PRAGMA_PACK

typedef struct ZISParmSetEntry_tag {
  char eyecatcher[8];
#define ZIS_PARMSET_ENTRY_EYECATCHER  "RSZISPSE"
  struct ZISParmSetEntry_tag *next;
  char *key;
  char *value;
} ZISParmSetEntry;

typedef struct ZISParmSet_tag {
  char eyecatcher[8];
#define ZIS_PARMSET_EYECATCHER "RSZISPST"
  ShortLivedHeap *slh;
  ZISParmSetEntry *firstEntry;
  ZISParmSetEntry *lastEntry;
} ZISParmSet;

typedef struct ZISParmStatus_tag {
  int internalRC;
  int internalRSN;
} ZISParmStatus;

typedef struct ZISMainFunctionParms_tag {
  unsigned short textLength;
  char text[0];
} ZISMainFunctionParms;

ZOWE_PRAGMA_PACK_RESET

ZISParmSet *zisMakeParmSet(void);

void zisRemoveParmSet(ZISParmSet *parms);

int zisReadParmlib(ZISParmSet *parms, const char *ddname, const char *member,
                   ZISParmStatus *status);

int zisReadMainParms(ZISParmSet *parms, const ZISMainFunctionParms *mainParms);

int zisPutParmValue(ZISParmSet *parms, const char *name, const char *value);

const char *zisGetParmValue(const ZISParmSet *parms, const char *name);

int zisLoadParmsToCMServer(CrossMemoryServer *server, const ZISParmSet *parms,
                           int *reasonCode);

typedef void ZISParmVisitor(const char *name, const char *value, void *userData);

void zisIterateParms(const ZISParmSet *parms, ZISParmVisitor *visitor,
                     void *visitorData);

#endif /* ZIS_PARM_H_ */
