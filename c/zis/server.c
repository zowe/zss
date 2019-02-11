

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include <metal/metal.h>
#include <metal/limits.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>

#ifndef METTLE
#error Non-metal C code is not supported
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "crossmemory.h"
#include "logging.h"
#include "metalio.h"
#include "zos.h"
#include "qsam.h"
#include "stcbase.h"
#include "utils.h"
#include "recovery.h"

#include "zis/message.h"
#include "zis/parm.h"
#include "zis/server.h"

#include "zis/services/auth.h"
#include "zis/services/nwm.h"
#include "zis/services/snarfer.h"

/*

BUILD INSTRUCTIONS:

Build using build.sh

DEPLOYMENT:

See details in the ZSS Cross Memory Server installation guide

*/

#define ZIS_PARM_DDNAME "PARMLIB"
#define ZIS_PARM_MEMBER_PREFIX                CMS_PROD_ID"IP"
#define ZIS_PARM_MEMBER_DEFAULT_SUFFIX        "00"
#define ZIS_PARM_MEMBER_SUFFIX_SIZE            2
#define ZIS_PARM_MEMBER_NAME_MAX_SIZE          8

#define ZIS_PARM_MEMBER_SUFFIX                "MEM"
#define ZIS_PARM_SERVER_NAME                  "NAME"
#define ZIS_PARM_COLD_START                   "COLD"
#define ZIS_PARM_DEBUG_MODE                   "DEBUG"

#define ZIS_PARMLIB_PARM_SERVER_NAME          CMS_PROD_ID".NAME"

static int registerCoreServices(CrossMemoryServer *server) {

  /* TODO handle us */
  cmsRegisterService(server, ZIS_SERVICE_ID_AUTH_SRV,
                     zisAuthServiceFunction, NULL,
                     CMS_SERVICE_FLAG_RELOCATE_TO_COMMON);
  cmsRegisterService(server, ZIS_SERVICE_ID_SNARFER_SRV,
                     zisSnarferServiceFunction, NULL,
                     CMS_SERVICE_FLAG_SPACE_SWITCH |
                     CMS_SERVICE_FLAG_RELOCATE_TO_COMMON);
  cmsRegisterService(server, ZIS_SERVICE_ID_NWM_SRV,
                     zisNWMServiceFunction, NULL,
                     CMS_SERVICE_FLAG_RELOCATE_TO_COMMON);

  return RC_ZIS_OK;
}


ZOWE_PRAGMA_PACK
typedef struct MainFunctionParms_tag {
  unsigned short textLength;
  char text[0];
} MainFunctionParms;
ZOWE_PRAGMA_PACK_RESET

#define GET_MAIN_FUNCION_PARMS() \
({ \
  unsigned int r1; \
  __asm(" ST    1,%0 "  : "=m"(r1) : : "r1"); \
  MainFunctionParms * __ptr32 parms = NULL; \
  if (r1 != 0) { \
    parms = (MainFunctionParms *)((*(unsigned int *)r1) & 0x7FFFFFFF); \
  } \
  parms; \
})

static bool isKeywordInPassedParms(const char *keyword, const MainFunctionParms *parms) {

  if (parms == NULL) {
    return false;
  }

  unsigned int keywordLength = strlen(keyword);
  for (int startIdx = 0; startIdx < parms->textLength; startIdx++) {
    int endIdx;
    for (endIdx = startIdx; endIdx < parms->textLength; endIdx++) {
      if (parms->text[endIdx] == ',' || parms->text[endIdx] == ' ') {
        break;
      }
    }
    unsigned int tokenLength = endIdx - startIdx;
    if (tokenLength == keywordLength) {
      if (memcmp(keyword, &parms->text[startIdx], tokenLength) == 0) {
        return true;
      }
    }
    startIdx = endIdx;
  }

  return false;
}

static char *getPassedParmValue(const char *parmName, const MainFunctionParms *parms, char *buffer, unsigned int bufferSize) {

  if (buffer == NULL || bufferSize == 0 || parms == NULL) {
    return NULL;
  }

  buffer[0] = '\0';

  unsigned int parmNameLength = strlen(parmName);
  for (int startIdx = 0; startIdx < parms->textLength; startIdx++) {
    int endIdx;
    for (endIdx = startIdx; endIdx < parms->textLength; endIdx++) {
      if (parms->text[endIdx] == ',' || parms->text[endIdx] == ' ') {
        break;
      }
    }
    unsigned int tokenLength = endIdx - startIdx;
    if (tokenLength > parmNameLength + 1) {
      if (memcmp(parmName, &parms->text[startIdx], parmNameLength) == 0 &&
          parms->text[startIdx + parmNameLength] == '=') {
        snprintf(buffer, bufferSize, "%.*s", tokenLength - parmNameLength - 1, &parms->text[startIdx + parmNameLength + 1]);
        return buffer;
      }
    }
    startIdx = endIdx;
  }

  return NULL;
}

int main() {

  const MainFunctionParms * __ptr32 passedParms = GET_MAIN_FUNCION_PARMS();

  int status = RC_ZIS_OK;

  STCBase *base = (STCBase *)safeMalloc31(sizeof(STCBase), "stcbase");
  stcBaseInit(base);
  {

    cmsInitializeLogging();

    wtoPrintf(ZIS_LOG_STARTUP_MSG);
    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO, ZIS_LOG_STARTUP_MSG);
    /* TODO refactor consolidation of passed and parmlib parms */
    bool isColdStart = isKeywordInPassedParms(ZIS_PARM_COLD_START, passedParms);
    bool isDebug = isKeywordInPassedParms(ZIS_PARM_DEBUG_MODE, passedParms);
    if (passedParms != NULL && passedParms->textLength > 0) {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO,
              ZIS_LOG_INPUT_PARM_MSG, passedParms);
      zowedump(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO,
               (void *)passedParms,
               sizeof(MainFunctionParms) + passedParms->textLength);
    }

    unsigned int serverFlags = CMS_SERVER_FLAG_NONE;
    if (isColdStart) {
      serverFlags |= CMS_SERVER_FLAG_COLD_START;
    }
    if (isDebug) {
      serverFlags |= CMS_SERVER_FLAG_DEBUG;
    }

    ZISParmSet *parmSet = zisMakeParmSet();
    if (parmSet == NULL) {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
              ZIS_LOG_CXMS_RES_ALLOC_FAILED_MSG, "parameter set");
      status = RC_ZIS_ERROR;
    }

    /* find out if MEM has been specified in the JCL and use it to create
     * the parmlib member name */
    char parmlibMember[ZIS_PARM_MEMBER_NAME_MAX_SIZE + 1] = {0};
    if (status == RC_ZIS_OK) {

      /* max suffix length + null-terminator + additional byte to determine
       * whether the passed value is too big */
      char parmlibMemberSuffixBuffer[ZIS_PARM_MEMBER_SUFFIX_SIZE + 1 + 1];
      const char *parmlibMemberSuffix = getPassedParmValue(
          ZIS_PARM_MEMBER_SUFFIX, passedParms,
          parmlibMemberSuffixBuffer,
          sizeof(parmlibMemberSuffixBuffer)
      );
      if (parmlibMemberSuffix == NULL) {
        parmlibMemberSuffix = ZIS_PARM_MEMBER_DEFAULT_SUFFIX;
      }

      if (strlen(parmlibMemberSuffix) == ZIS_PARM_MEMBER_SUFFIX_SIZE) {
        strcat(parmlibMember, ZIS_PARM_MEMBER_PREFIX);
        strcat(parmlibMember, parmlibMemberSuffix);
      } else {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
                ZIS_LOG_CXMS_BAD_PMEM_SUFFIX_MSG, parmlibMemberSuffix);
        status = RC_ZIS_ERROR;
      }
    }

    if (status == RC_ZIS_OK) {
      ZISParmStatus readStatus = {0};
      int readParmRC = zisReadParmlib(parmSet, ZIS_PARM_DDNAME,
                                      parmlibMember, &readStatus);
      if (readParmRC != RC_ZISPARM_OK) {
        if (readParmRC == RC_ZISPARM_MEMBER_NOT_FOUND) {
          zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING,
                  ZIS_LOG_CXMS_CFG_MISSING_MSG, parmlibMember, readParmRC);
          status = RC_ZIS_ERROR;
        } else {
          zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
                  ZIS_LOG_CXMS_CFG_NOT_READ_MSG, parmlibMember, readParmRC,
                  readStatus.internalRC, readStatus.internalRSN);
          status = RC_ZIS_ERROR;
        }
      }
    }

    CrossMemoryServerName serverName;
    char serverNameBuffer[sizeof(serverName.nameSpacePadded) + 1];
    const char *serverNameNullTerm = NULL;
    serverNameNullTerm = getPassedParmValue(ZIS_PARM_SERVER_NAME, passedParms,
                                            serverNameBuffer,
                                            sizeof(serverNameBuffer));
    if (serverNameNullTerm == NULL) {
      serverNameNullTerm =
          zisGetParmValue(parmSet, ZIS_PARMLIB_PARM_SERVER_NAME);
    }

    if (serverNameNullTerm != NULL) {
      serverName = cmsMakeServerName(serverNameNullTerm);
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO,
              ZIS_LOG_CXMS_NAME_MSG, serverName.nameSpacePadded);
    } else {
      serverName = CMS_DEFAULT_SERVER_NAME;
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO, ZIS_LOG_CXMS_NO_NAME_MSG, serverName.nameSpacePadded);
    }

    int makeServerRSN = 0;
    CrossMemoryServer *server = makeCrossMemoryServer(base, &serverName, serverFlags, &makeServerRSN);
    if (server == NULL) {
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE, ZIS_LOG_CXMS_NOT_CREATED_MSG, makeServerRSN);
      status = RC_ZIS_ERROR;
    }

    if (status == RC_ZIS_OK) {
      int loadParmRSN = 0;
      int loadParmRC = zisLoadParmsToServer(server, parmSet, &loadParmRSN);
      if (loadParmRC != RC_ZISPARM_OK) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE,
                ZIS_LOG_CXMS_CFG_NOT_LOADED_MSG, loadParmRC, loadParmRSN);
        status = RC_ZIS_ERROR;
      }
    }

    if (status == RC_ZIS_OK) {
      registerCoreServices(server);
      int startRC = cmsStartMainLoop(server);
      if (startRC != RC_CMS_OK) {
        zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE, ZIS_LOG_CXMS_NOT_STARTED_MSG, startRC);
        status = RC_ZIS_ERROR;
      }
    }

    if (server != NULL) {
      removeCrossMemoryServer(server);
      server = NULL;
    }

    if (parmSet != NULL) {
      zisRemoveParmSet(parmSet);
      parmSet = NULL;
    }

    if (status == RC_ZIS_OK) {
      wtoPrintf(ZIS_LOG_CXMS_TERM_OK_MSG);
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO, ZIS_LOG_CXMS_TERM_OK_MSG);
    } else {
      wtoPrintf(ZIS_LOG_CXMS_TERM_FAILURE_MSG, status);
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_SEVERE, ZIS_LOG_CXMS_TERM_FAILURE_MSG, status);
    }

  }
  stcBaseTerm(base);
  safeFree31((char *)base, sizeof(STCBase));
  base = NULL;

  return status;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

