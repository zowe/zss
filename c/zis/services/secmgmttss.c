

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/string.h>

#ifndef METTLE
#error Non-metal C code is not supported
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "crossmemory.h"
#include "radmin.h"
#include "recovery.h"
#include "zos.h"

#include "zis/utils.h"
#include "zis/services/secmgmt.h"

#include "zis/services/tssparsing.h"

typedef struct LogData_t {
  CrossMemoryServerGlobalArea *globalArea;
  int traceLevel;
} LogData;

/* A helper function which finds the line where the value where we are looking for starts
 * and returns a pointer to it. This is useful for when the caller wants to start from
 * a particular value because they either reached the maximum they could return in a 
 * request, or if they want to information on a single user. It also returns pointers
 * to the lastByteOffset and the blockBytesExtracted so when continuing to parse,
 * we still have the right place through the output.
 */
static const RadminVLText *findLineOfStart(const RadminVLText *currentLine, const char *searchValue, 
                                     int searchValueMaxLength, const char *searchKey,
                                     const unsigned int *lastByteOffset, unsigned int *blockBytesExtracted,
                                     LogData *logData) {
  if (searchValueMaxLength > MAX_VALUE_LEN) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
               "The search value maximum length is greater than"
               " the maximum length allowed.\n");
    return NULL;
  }
  
  int startValueLen = strlen(searchValue);
  *blockBytesExtracted = 16;
  
  while (*blockBytesExtracted < *lastByteOffset) {
    KeyData keys[MAX_NUM_OF_KEYS] = {0};
    char tmpBuffer[MAX_VALUE_LEN] = {0}; /* Use the maximum value length of 255 to handle all cases. */
    
    if (isOutputDone(currentLine->text, currentLine->length)) {
      return NULL;
    }
    
    int numberOfKeys = parseLineForKeys(currentLine->text, currentLine->length, keys);
    if (numberOfKeys == -1) {
      /* In this case, print the line in question. */
      CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                 "An invalid number of keys were detected on line.\n"
                 "Current line is: \n%.*s\n", currentLine->length, currentLine->text);
        
      return NULL;
    }
    else if (numberOfKeys == 0) { 
      *blockBytesExtracted += currentLine->length + 2;
      currentLine = (RadminVLText *)(currentLine->text + currentLine->length);
      continue; /* ignore blank lines and carry on */
    }
    else {
      int keyIndex = findDesiredKey(searchKey, keys);
      if (keyIndex != -1) {
        int status = findValueForKey(currentLine->text, currentLine->length, keys,
                                     keyIndex, numberOfKeys, tmpBuffer, searchValueMaxLength);
        if (status == -1) {
          CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                    "Could not find value for the key, %s...\n", searchKey);
          return NULL;
        } 
        
        int valueLen = getActualValueLength(tmpBuffer, sizeof(tmpBuffer));
        if (valueLen == -1) {
          CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                     "Problem getting the actual length of %.*s\n", sizeof(tmpBuffer), tmpBuffer);
          return NULL;
        }

        if (valueLen == startValueLen) {
          if (!memcmp(searchValue, tmpBuffer, valueLen)) {
            return currentLine;
          }
        }
      }   
    }
    
    *blockBytesExtracted += currentLine->length + 2;
    currentLine = (RadminVLText *)(currentLine->text + currentLine->length);
  }
  
  return NULL;
}


/* A parameter list passed into the handler function
 * for the userProfiles service. 
 */
typedef struct TSSUserProfileParms_t {
  ZISUserProfileEntry *tmpResultBuffer;
  unsigned int profilesExtracted;
  unsigned int profilesToExtract;
  const char *startUserID;
  int startIndex;
  LogData *logData;
} TSSUserProfileParms;

/* A handler function that is called after the r_admin command is issued. It parses the user
 * data from the output by going line by line.
 */
static int userProfilesHandler(RadminAPIStatus status, const RadminCommandOutput *result,
                               void *userData) {
  if (status.racfRC != 0) {
    return status.racfRC;
  }

  TSSUserProfileParms *parms = userData;
  ZISUserProfileEntry *buffer = parms->tmpResultBuffer;
  LogData *logData = parms->logData;
  ZISUserProfileEntry entry = {0};

  const RadminCommandOutput *currentBlock = result;  
  
  if (currentBlock == NULL) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "No output was found in output message entry on first block\n");
    return 0;
  }
  
  const RadminVLText *currentLine = &currentBlock->firstMessageEntry;
  
  /* Look for an obvious error before we start parsing.
   * The output will look something like:
   * 
   *  TSS{SOME NUMBERS}  {SOME MESSAGE}                      
   *  TSS0301I  TSS      FUNCTION FAILED, RETURN CODE =  {SOME NUMBER}
   */
  bool errorDetected = isErrorDetected(currentLine->text, currentLine->length);
  if (errorDetected) {
    return -1;
  }
  
  /* If specified, iterate through the output until start is found. If it's not ever found
   * then return an error.
   */
  unsigned int blockBytesExtracted = 0;
  if (parms->startUserID != NULL) {
    do {
      currentLine = findLineOfStart(currentLine, parms->startUserID, ACCESSOR_ID_VALUE_MAX_LEN,
                                    ACCESSOR_ID_KEY, &currentBlock->lastByteOffset, &blockBytesExtracted,
                                    logData);
      if (currentLine != NULL) {
        break;
      }
      
      currentBlock = currentBlock->next;
      if (currentBlock != NULL) {
        currentLine = (RadminVLText *)&(currentBlock->firstMessageEntry);
      }
    } while (currentBlock != NULL);
    
    if (currentLine == NULL) {
      CMS_DEBUG2(logData->globalArea, logData->traceLevel,
                 "Start profile %s could not be found\n", parms->startUserID);
      return 0;
    }
  }

  /* Iterate through the output and store information on the users that
   * are found.
   */
  unsigned int numExtracted = parms->startIndex; /* this is for any continuations with multiple calls using the same array */
  do {
    while (blockBytesExtracted < currentBlock->lastByteOffset) {      
      if (numExtracted == parms->profilesToExtract || isOutputDone(currentLine->text, currentLine->length)) {
        parms->profilesExtracted = numExtracted;
        return 0;
      }

      KeyData keys[MAX_NUM_OF_KEYS] = {0};
      
      int numberOfKeys = parseLineForKeys(currentLine->text, currentLine->length, keys);
      if (numberOfKeys == -1) {
        /* In this case, print the line in question. */
        CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                   "An invalid number of keys were detected on line.\n"
                   "Current line is: \n%.*s\n", currentLine->length, currentLine->text);
        return -1;
      }      
      /* A blank line means the end of a single user's data.
       * Store what was found into the ZISUserProfileEntry
       * struct.
       */
      else if (numberOfKeys == 0) {
        ZISUserProfileEntry *p = &buffer[numExtracted++];
        memset(p, 0, sizeof(*p));
        memcpy(p->userID, entry.userID, sizeof(p->userID));
        memcpy(p->name, entry.name, sizeof(p->name));
        memcpy(p->defaultGroup, entry.defaultGroup, sizeof(p->defaultGroup));
        memset(&entry, 0, sizeof(ZISUserProfileEntry));      
      }
      else { 
        int keyIndex = findDesiredKey(ACCESSOR_ID_KEY, keys);
        if (keyIndex != -1) {
          int status = findValueForKey(currentLine->text, currentLine->length, keys,
                                       keyIndex, numberOfKeys, entry.userID, ACCESSOR_ID_VALUE_MAX_LEN);
          if (status == -1) {
            CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                       "Could not find value of %s...\n", ACCESSOR_ID_KEY);
            return -1;
          }
        }
        
        keyIndex = findDesiredKey(NAME_KEY, keys);
        if (keyIndex != -1) {
          int status = findValueForKey(currentLine->text, currentLine->length, keys,
                                       keyIndex, numberOfKeys, entry.name, NAME_VALUE_MAX_LEN);
          if (status == -1) {
            CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                       "Could not find value of %s...\n", NAME_KEY);
            return -1;
          }
        }
        
        keyIndex = findDesiredKey(DEFAULT_GROUP_KEY, keys);
        if (keyIndex != -1) {
          int status = findValueForKey(currentLine->text, currentLine->length, keys, keyIndex,
                                      numberOfKeys, entry.defaultGroup, DEFAULT_GROUP_VALUE_MAX_LEN);
          if (status == -1) {
            CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                       "Could not find value of %s...\n", DEFAULT_GROUP_KEY);
            return -1;
          }
        }      
      }
    
      blockBytesExtracted += currentLine->length + 2;
      currentLine = (RadminVLText *)(currentLine->text + currentLine->length);
    }
    
    currentBlock = currentBlock->next;
    if (currentBlock != NULL) {
      currentLine = (RadminVLText *)&(currentBlock->firstMessageEntry);
      blockBytesExtracted = 16;
    }
  } while (currentBlock != NULL);
  
  return 0;
}

/* Copies the ZISUserProfileEntry struct to the other address space wherex
 * it can be returned to the caller.
 */
static void copyUserProfiles(ZISUserProfileEntry *dest, ZISUserProfileEntry *src,
                             unsigned int count) {
  for (unsigned int i = 0; i < count; i++) {
    cmCopyToSecondaryWithCallerKey(dest[i].userID, src[i].userID,
                                   sizeof(dest[i].userID));
    cmCopyToSecondaryWithCallerKey(dest[i].defaultGroup, src[i].defaultGroup,
                                   sizeof(dest[i].defaultGroup));
    cmCopyToSecondaryWithCallerKey(dest[i].name, src[i].name,
                                   sizeof(dest[i].name));
  }
}

int zisUserProfilesServiceFunctionTSS(CrossMemoryServerGlobalArea *globalArea,
                                      CrossMemoryService *service, void *parm) {
  int status = RC_ZIS_UPRFSRV_OK;
  int radminExtractRC = RC_RADMIN_OK;

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_UPRFSRV_PARMLIST_NULL;
  }

  ZISUserProfileServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr, sizeof(localParmList));

  int parmValidateRC = validateUserProfileParmList(&localParmList);
  if (parmValidateRC != RC_ZIS_UPRFSRV_OK) {
    return parmValidateRC;
  }

  int traceLevel = localParmList.traceLevel;
  if (globalArea->pcLogLevel > traceLevel) {
    traceLevel = globalArea->pcLogLevel;
  }

  RadminUserID caller;
  if (!getCallerUserID(&caller)) {
    status = RC_ZIS_UPRFSRV_IMPERSONATION_MISSING;
    return status;
  }

  CMS_DEBUG2(globalArea, traceLevel, "UPRFSRV: userID = \'%.*s\', "
             "profilesToExtract = %u, result buffer = 0x%p\n",
             localParmList.startUserID.length,
             localParmList.startUserID.value,
             localParmList.profilesToExtract,
             localParmList.resultBuffer);
  CMS_DEBUG2(globalArea, traceLevel, "UPRFSRV: caller = \'%.*s\'\n",
             caller.length, caller.value);

  RadminCallerAuthInfo authInfo = {
      .acee = NULL,
      .userID = caller
  };

  char userIDBuffer[ZIS_USER_ID_MAX_LENGTH + 1] = {0};
  memcpy(userIDBuffer, localParmList.startUserID.value,
         localParmList.startUserID.length);
  const char *startUserIDNullTerm = localParmList.startUserID.length > 0 ?
                                    userIDBuffer : NULL;

  size_t tmpResultBufferSize =
      sizeof(ZISUserProfileEntry) * localParmList.profilesToExtract;
  int allocRC = 0, allocSysRC = 0, allocSysRSN = 0;
  ZISUserProfileEntry *tmpResultBuffer =
      (ZISUserProfileEntry *)safeMalloc64v3(tmpResultBufferSize, TRUE,
                                            "ZISUserProfileEntry",
                                            &allocRC,
                                            &allocSysRSN,
                                            &allocSysRSN);

  CMS_DEBUG2(globalArea, traceLevel,
             "UPRFSRV: tmpBuff allocated @ 0x%p (%zu %d %d %d)\n",
             tmpResultBuffer, tmpResultBufferSize,
             allocRC, allocSysRC, allocSysRSN);

  if (tmpResultBuffer == NULL) {
    status = RC_ZIS_UPRFSRV_ALLOC_FAILED;
    return status;
  }

  RadminStatus radminStatus = {0};
  
  LogData logData = {
    .globalArea = globalArea,
    .traceLevel = traceLevel
  };
  
  TSSUserProfileParms parms = {
    .profilesToExtract = localParmList.profilesToExtract,
    .tmpResultBuffer = tmpResultBuffer,
    .startUserID = startUserIDNullTerm,
    .logData = &logData,
    .startIndex = 0
  };

  /* We need to extract administrators as well, which requires multiple calls.
   * Logic is put in a one-time do while for readability if any errors occur.
   */
  bool doExtraction = FALSE;
  do {
    CMS_DEBUG2(globalArea, traceLevel,
              "About to extract USER profiles...\n");
              
    radminExtractRC = radminRunRACFCommand(
        authInfo,
        "TSS LIST(ACIDS) TYPE(USER)",
        userProfilesHandler,
        &parms,
        &radminStatus
    );
    
    if (radminExtractRC != RC_RADMIN_OK) {
      CMS_DEBUG2(globalArea, traceLevel,
                 "Could not extract USER profiles...\n");
      break;
    }
    
    parms.startIndex = parms.profilesExtracted;
    if (parms.startIndex >= tmpResultBufferSize) {
      CMS_DEBUG2(globalArea, traceLevel,
                 "Stopping extraction of DCA profiles because the buffer would overflow...\n");
      break;
    }

    CMS_DEBUG2(globalArea, traceLevel,
              "About to extract DCA profiles...\n");
              
    radminExtractRC = radminRunRACFCommand(
        authInfo,
        "TSS LIST(ACIDS) TYPE(DCA)",
        userProfilesHandler,
        &parms,
        &radminStatus
    );
    
    if (radminExtractRC != RC_RADMIN_OK) {
      CMS_DEBUG2(globalArea, traceLevel,
                 "Could not extract DCA profiles...\n");
      break;
    }
    
    parms.startIndex = parms.profilesExtracted;
    if (parms.startIndex >= tmpResultBufferSize) {
      CMS_DEBUG2(globalArea, traceLevel,
                 "Stopping extraction of VCA profiles because the buffer would overflow...\n");
      break;
    }

    CMS_DEBUG2(globalArea, traceLevel,
              "About to extract VCA profiles...\n");
              
    radminExtractRC = radminRunRACFCommand(
        authInfo,
        "TSS LIST(ACIDS) TYPE(VCA)",
        userProfilesHandler,
        &parms,
        &radminStatus
    );

    if (radminExtractRC != RC_RADMIN_OK) {
      CMS_DEBUG2(globalArea, traceLevel,
                 "Could not extract VCA profiles...\n");
      break;
    }
    
    parms.startIndex = parms.profilesExtracted;
    if (parms.startIndex >= tmpResultBufferSize) {
      CMS_DEBUG2(globalArea, traceLevel,
                 "Stopping extraction of ZCA profiles because the buffer would overflow...\n");
      break;
    }

    CMS_DEBUG2(globalArea, traceLevel,
              "About to extract ZCA profiles...\n");
              
    radminExtractRC = radminRunRACFCommand(
        authInfo,
        "TSS LIST(ACIDS) TYPE(ZCA)",
        userProfilesHandler,
        &parms,
        &radminStatus
    );

    if (radminExtractRC != RC_RADMIN_OK) {
      CMS_DEBUG2(globalArea, traceLevel,
                 "Could not extract ZCA profiles...\n");
      break;
    }
    
    parms.startIndex = parms.profilesExtracted;
    if (parms.startIndex >= tmpResultBufferSize) {
      CMS_DEBUG2(globalArea, traceLevel,
                 "Stopping extraction of LSCA profiles because the buffer would overflow...\n");
      break;
    }

    CMS_DEBUG2(globalArea, traceLevel,
              "About to extract LSCA profiles...\n");
              
    radminExtractRC = radminRunRACFCommand(
        authInfo,
        "TSS LIST(ACIDS) TYPE(LSCA)",
        userProfilesHandler,
        &parms,
        &radminStatus
    );

    if (radminExtractRC != RC_RADMIN_OK) {
      CMS_DEBUG2(globalArea, traceLevel,
                 "Could not extract LSCA profiles...\n");
      break;
    }
    
    parms.startIndex = parms.profilesExtracted;
    if (parms.startIndex >= tmpResultBufferSize) {
      CMS_DEBUG2(globalArea, traceLevel,
                 "Stopping extraction of SCA profiles because the buffer would overflow...\n");
      break;
    }

    CMS_DEBUG2(globalArea, traceLevel,
              "About to extract SCA profiles...\n");
              
    radminExtractRC = radminRunRACFCommand(
        authInfo,
        "TSS LIST(ACIDS) TYPE(SCA)",
        userProfilesHandler,
        &parms,
        &radminStatus
    );

    if (radminExtractRC != RC_RADMIN_OK) {
      CMS_DEBUG2(globalArea, traceLevel,
                 "Could not extract SCA profiles...\n");
      break;
    }
  } while (doExtraction == TRUE);
  
  /* If the system has no users, there must have been a serious
   * parsing error somewhere. Return some sort of error to prevent
   * having an empty array.
   */
  if (parms.profilesExtracted == 0) {
    radminExtractRC = RC_RADMIN_SYSTEM_ERROR;
  }
  
  if (radminExtractRC == RC_RADMIN_OK) {
    copyUserProfiles(localParmList.resultBuffer, tmpResultBuffer,
                      parms.profilesExtracted);
    localParmList.profilesExtracted = parms.profilesExtracted;
  } else {
    localParmList.internalServiceRC = radminExtractRC;
    localParmList.internalServiceRSN = radminStatus.reasonCode;
    localParmList.safStatus.safRC = radminStatus.apiStatus.safRC;
    localParmList.safStatus.racfRC = radminStatus.apiStatus.racfRC;
    localParmList.safStatus.racfRSN = radminStatus.apiStatus.racfRSN;
    status = RC_ZIS_UPRFSRV_INTERNAL_SERVICE_FAILED;
  }

  cmCopyToSecondaryWithCallerKey(clientParmAddr, &localParmList,
                                 sizeof(ZISUserProfileServiceParmList));

  int freeRC = 0, freeSysRC = 0, freeSysRSN = 0;
  freeRC = safeFree64v3(tmpResultBuffer, tmpResultBufferSize,
                        &freeSysRC, &freeSysRSN);

  CMS_DEBUG2(globalArea, traceLevel,
             "UPRFSRV: tmpBuff free'd @ 0x%p (%zu %d %d %d)\n",
             tmpResultBuffer, tmpResultBufferSize,
             freeRC, freeSysRC, freeSysRSN);

  tmpResultBuffer = NULL;

  return status;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

