

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
#include <metal/ctype.h>

#ifndef METTLE
#error Non-metal C code is not supported
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "crossmemory.h"
#include "radmin.h"
#include "zis/utils.h"
#include "zis/services/secmgmt.h"
#include "zis/services/tssparsing.h"
#include "zis/services/secmgmtUtils.h"

/* A struct to more easily carry the required arguments
 * for logging in ZIS into handler and helper functions.
 */
typedef struct TSSLogData_t {
  CrossMemoryServerGlobalArea *globalArea;
  int traceLevel;
} TSSLogData;

/* A helper function which finds the line where the value where we are looking for starts
 * and returns a pointer to it. This is useful for when the caller wants to start from
 * a particular value because they either reached the maximum they could return in a 
 * request, or if they want to information on a single user. It also returns pointers
 * to the maxOffsetInBlockand the offsetInBlock so when continuing to parse,
 * we still have the right place through the output.
 */
static const RadminVLText *findLineOfStart(const RadminVLText *currentLine,
                                           const char *searchKey,
                                           const char *searchValue,
                                           unsigned int searchValueMaxLength,
                                           const unsigned int maxOffsetInBlock,
                                           unsigned int *offsetInBlock,
                                           TSSLogData *logData) {
  if (searchValueMaxLength > TSS_MAX_VALUE_LEN) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "Maximum value length supplied is %d, but the maximum allowed is %d\n",
               searchValueMaxLength, TSS_MAX_VALUE_LEN);
    return NULL;
  }
  
  int startValueLen = strlen(searchValue);
  CMS_DEBUG2(logData->globalArea, logData->traceLevel, "Length of Search Value: %d\n", startValueLen);
  
  while (*offsetInBlock < maxOffsetInBlock) {
    TSSKeyData keys[TSS_MAX_NUM_OF_KEYS] = {0};
    char tmpBuffer[TSS_MAX_VALUE_LEN] = {0}; /* Use the maximum value length of 255 to handle all cases. */
    
    if (tssIsOutputDone(currentLine->text, currentLine->length)) {
      return NULL;
    }
    
    int numberOfKeys = tssParseLineForKeys(currentLine->text, currentLine->length, keys);
    if (numberOfKeys == -1) {
      /* In this case, print the line in question. */
      CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                 "An invalid number of keys were detected on line: '"
                 "%.*s'\n", currentLine->length, currentLine->text);
        
      return NULL;
    }
    else if (numberOfKeys == 0) { 
      *offsetInBlock += currentLine->length + 2;
      currentLine = (RadminVLText *)(currentLine->text + currentLine->length);
      continue; /* ignore blank lines and carry on */
    }
    else {
      CMS_DEBUG2(logData->globalArea, logData->traceLevel, "Number of keys on line: %d\n", numberOfKeys);
      
      int keyIndex = tssFindDesiredKey(searchKey, keys);
      if (keyIndex != -1) {                  
        int status = tssFindValueForKey(currentLine->text, currentLine->length, keys,
                                        keyIndex, numberOfKeys, tmpBuffer, searchValueMaxLength);
        if (status == -1) {
          CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                    "Could not find value for the key, %s...\n", searchKey);
          return NULL;
        }
        
        int valueLen = tssGetActualValueLength(tmpBuffer, sizeof(tmpBuffer));
        if (valueLen == -1) {
          CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                     "Problem getting the actual length of %.*s\n", sizeof(tmpBuffer), tmpBuffer);
          return NULL;
        }

        CMS_DEBUG2(logData->globalArea, logData->traceLevel, "Current Value Found: %.*s\n", valueLen, tmpBuffer);
        CMS_DEBUG2(logData->globalArea, logData->traceLevel, "Length of Value Found: %d\n", valueLen);
        
        if (valueLen == startValueLen) {
          if (!memcmp(searchValue, tmpBuffer, startValueLen)) {
            return currentLine;
          }
        }
      }
    }
    
    *offsetInBlock += currentLine->length + 2;
    currentLine = (RadminVLText *)(currentLine->text + currentLine->length);
  }
  
  return NULL;
}

/* Iterates through all the output to find how many access list entries there actually are. This is
 * needed by the security endpoint to determine buffer size in case the first size allocated isn'tan
 * enough.
 */
static int findNumberOfValidGenreEntries(const RadminVLText *currentLine, unsigned int lastByteOffset,
                                         const char *searchProfile, unsigned int *numberOfEntries,
                                         TSSLogData *logData) {
  int offsetInBlock = 16;
              
  while (offsetInBlock < lastByteOffset) {
    if (tssIsOutputDone(currentLine->text, currentLine->length)) {
      return 0;
    }     

    TSSKeyData keys[TSS_MAX_NUM_OF_KEYS] = {0};
              
    int numberOfKeys = tssParseLineForKeys(currentLine->text, currentLine->length, keys);
    if (numberOfKeys == -1) {
       CMS_DEBUG2(logData->globalArea, logData->traceLevel,
              "Found invalid number of keys on line...\n");
      return -1;
    }
              
    int keyIndex = tssFindDesiredKey(TSS_XAUTH_KEY, keys);
    if (keyIndex != -1) {
      char tmpBuffer[ZIS_SECURITY_PROFILE_MAX_LENGTH] = {0};
      int acidIndex = lastIndexOfString((char*)currentLine->text, currentLine->length, TSS_ACID_KEY);
      if (acidIndex == -1) {
        CMS_DEBUG2(logData->globalArea, logData->traceLevel,
                  "Expecting %s, but it was not found on line...\n",
                  TSS_ACID_KEY);
				return -1;
      }

      int status = tssFindValueForKey(currentLine->text, acidIndex-1, keys,
                                      keyIndex, numberOfKeys, tmpBuffer,
                                      sizeof(tmpBuffer));
      if (status == -1) {
        CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                   "Could not find value of %s...\n", TSS_NAME_KEY);
        return -1;
      }
                
      int valueLen = tssGetActualValueLength(tmpBuffer, sizeof(tmpBuffer));
                
      int profileLength = strlen(searchProfile);
                
      if (valueLen == profileLength) {
        if (!memcmp(searchProfile, tmpBuffer, valueLen)) {
          (*numberOfEntries)++;
        }
      }
    }
    
    offsetInBlock += currentLine->length + 2;
    currentLine = (RadminVLText *)(currentLine->text + currentLine->length);
  }

  return 0;
}

/* Iterates through all the output to find the total number of valid lines of data. */
static const RadminVLText *getNumberOfLinesOfData(const RadminVLText *currentLine,
                                                  unsigned int lastByteOffset, 
                                                  unsigned int *numberOfLines) {
  int offsetInBlock = 16;
  
  while (offsetInBlock < lastByteOffset) { 
    if (tssIsOutputDone(currentLine->text, currentLine->length)) {
      return NULL;
    }     

    if (!tssIsLineBlank(currentLine->text, currentLine->length)) {
      (*numberOfLines)++;
    }
    
    offsetInBlock += currentLine->length + 2;
    currentLine = (RadminVLText *)(currentLine->text + currentLine->length);
  }

  return currentLine;
}

/* A struct to hold the information
 * returned from getAcidsInLine()
 */
typedef struct TSSAcidData_t {
  char acid[ZIS_USER_ID_MAX_LENGTH];
  bool isAdmin;
} TSSAcidData;

/* Parses a passed in line and gets the acids. There should be a maximum of four on each line. It copies them into
 * an array which is accessed after the call.
 */
static unsigned int getAcidsInLine(const RadminVLText *currentLine, TSSAcidData *data, TSSLogData *logData, int startPos) {
  unsigned int acidCounter = 0;
  for (int i = 0; i < 4; i++) {
    char tmpBuffer[11] = {0}; /* Acid - 8, Admin Indicator - 3 */
    
    /* Line can start with spaces. Make sure we get to the actual output. */
    for (int j = startPos; j < currentLine->length; j++) {
      if (currentLine->text[j] != ' ') {
        startPos = j;
        break;
      }
    }
    
    /* Check that we aren't going out of bounds. If we reach end of line,
     * copy up to that index.
     */
    int copySize = sizeof(tmpBuffer);
    if (startPos + copySize > currentLine->length - 1) {
      copySize = (currentLine->length - 1) - startPos;
    }
    
    if (copySize <= 0) {
      return acidCounter; /* Probably less than four acids on this line... */
    }
                 
    memcpy(tmpBuffer, currentLine->text+startPos, copySize);
    
    memcpy(data[i].acid, tmpBuffer, sizeof(data[i].acid));
    padWithSpaces(data[i].acid, sizeof(data[i].acid), 1, 1); /* All other values are padded with spaces. */
      
    int isAdmin = lastIndexOfString(tmpBuffer, sizeof(tmpBuffer), TSS_ADMIN_INDICATOR);
    if (isAdmin == -1) {
      data[i].isAdmin = FALSE;
    }
    else {
      data[i].isAdmin = TRUE;
    }
   
#define ACID_OFFSET 12
    startPos+=ACID_OFFSET; /* This is safe enough. */
    
    acidCounter++;
  }
  
   CMS_DEBUG2(logData->globalArea, logData->traceLevel,
              "Returning %d acids...\n", acidCounter);
               
  return acidCounter;
}


/* A parameter list passed into the handler function
 * for the user profiles service. 
 */
typedef struct TSSUserProfileParms_t {
  ZISUserProfileEntry *tmpResultBuffer;
  unsigned int profilesExtracted;
  unsigned int profilesToExtract;
  const char *startUserID;
  int startIndex;
  TSSLogData *logData;
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
  TSSLogData *logData = parms->logData;
  ZISUserProfileEntry entry = {0};

  const RadminCommandOutput *currentBlock = result;  
  
  if (currentBlock == NULL) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "No output was found in output message entry on first block...\n");
    return 0;
  }
  
  const RadminVLText *currentLine = &currentBlock->firstMessageEntry;
  
  /* Look for an obvious error before we start parsing.
   * The output will look something like:
   * 
   *  TSS{SOME NUMBERS}  {SOME MESSAGE}                      
   *  TSS0301I  TSS      FUNCTION FAILED, RETURN CODE =  {SOME NUMBER}
   */
  bool errorDetected = tssIsErrorDetected(currentLine->text, currentLine->length);
  if (errorDetected) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "Error detected!\n");
    return -1;
  }
  
  /* If specified, iterate through the output until start is found. If it's not ever found
   * then return an error.
   */
  unsigned int offsetInBlock = 16;
  if (parms->startUserID != NULL) {
    do {
      currentLine = findLineOfStart(currentLine, TSS_ACCESSOR_ID_KEY, parms->startUserID,
                                    ZIS_USER_ID_MAX_LENGTH, currentBlock->lastByteOffset,
                                    &offsetInBlock, logData);
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
                 "Start profile %s could not be found...\n", parms->startUserID);
      return 0;
    }
  }

  /* Iterate through the output and store information on the users that
   * are found.
   */
  unsigned int numExtracted = parms->startIndex; /* this is for any continuations with multiple calls using the same array */
  do {
    while (offsetInBlock < currentBlock->lastByteOffset) {      
      if (numExtracted == parms->profilesToExtract || tssIsOutputDone(currentLine->text, currentLine->length)) {
        parms->profilesExtracted = numExtracted;
        return 0;
      }

      TSSKeyData keys[TSS_MAX_NUM_OF_KEYS] = {0};
      
      int numberOfKeys = tssParseLineForKeys(currentLine->text, currentLine->length, keys);
      if (numberOfKeys == -1) {
        /* In this case, print the line in question. */
        CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                   "An invalid number of keys were detected on line: '"
                   "%.*s'\n", currentLine->length, currentLine->text);
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
        int keyIndex = tssFindDesiredKey(TSS_ACCESSOR_ID_KEY, keys);
        if (keyIndex != -1) {
          int status = tssFindValueForKey(currentLine->text, currentLine->length, keys,
                                       keyIndex, numberOfKeys, entry.userID, sizeof(entry.userID));
          if (status == -1) {
            CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                       "Could not find value of %s...\n", TSS_ACCESSOR_ID_KEY);
            return -1;
          }
        }
        
        keyIndex = tssFindDesiredKey(TSS_NAME_KEY, keys);
        if (keyIndex != -1) {
          int status = tssFindValueForKey(currentLine->text, currentLine->length, keys,
                                          keyIndex, numberOfKeys, entry.name, sizeof(entry.name));
          if (status == -1) {
            CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                       "Could not find value of %s...\n", TSS_NAME_KEY);
            return -1;
          }
        }
        
        keyIndex = tssFindDesiredKey(TSS_DEFAULT_GROUP_KEY, keys);
        if (keyIndex != -1) {
          int status = tssFindValueForKey(currentLine->text, currentLine->length, keys, keyIndex,
                                      numberOfKeys, entry.defaultGroup, sizeof(entry.defaultGroup));
          if (status == -1) {
            CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                       "Could not find value of %s...\n", TSS_DEFAULT_GROUP_KEY);
            return -1;
          }
        }      
      }
    
      offsetInBlock += currentLine->length + 2;
      currentLine = (RadminVLText *)(currentLine->text + currentLine->length);
    }
    
    currentBlock = currentBlock->next;
    if (currentBlock != NULL) {
      currentLine = (RadminVLText *)&(currentBlock->firstMessageEntry);
      offsetInBlock = 16;
    }
  } while (currentBlock != NULL);
  
  return 0;
}

/* Prepares to copy the ZISUserProfileEntry struct to the other address space where
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

/* Goes through several r_admin commands to retrieve information on all the users
 * in the system, including administrators.
 */
static int getAllUsers(RadminStatus *radminStatus, RadminCallerAuthInfo authInfo,
                      TSSUserProfileParms *parms) {                         
  CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
            "About to extract USER profiles...\n");
            
  int radminExtractRC = radminRunRACFCommand(
      authInfo,
      "TSS LIST(ACIDS) TYPE(USER)",
      userProfilesHandler,
      parms,
      radminStatus
  );
  
  if (radminExtractRC != RC_RADMIN_OK) {
    CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
               "Could not extract USER profiles...\n");
    return radminExtractRC;
  }
  
  parms->startIndex = parms->profilesExtracted;
  if (parms->startIndex >= parms->profilesToExtract) {
    CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
               "Stopping extraction of DCA profiles because the buffer would overflow...\n");
    return RC_RADMIN_OK;
  }

  CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
            "About to extract DCA profiles...\n");
            
  radminExtractRC = radminRunRACFCommand(
      authInfo,
      "TSS LIST(ACIDS) TYPE(DCA)",
      userProfilesHandler,
      parms,
      radminStatus
  );
  
  if (radminExtractRC != RC_RADMIN_OK) {
    CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
               "Could not extract DCA profiles...\n");
    return radminExtractRC;
  }
  
  parms->startIndex = parms->profilesExtracted;
  if (parms->startIndex >= parms->profilesToExtract) {
    CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
               "Stopping extraction of VCA profiles because the buffer would overflow...\n");
    return RC_RADMIN_OK;
  }

  CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
            "About to extract VCA profiles...\n");
            
  radminExtractRC = radminRunRACFCommand(
      authInfo,
      "TSS LIST(ACIDS) TYPE(VCA)",
      userProfilesHandler,
      parms,
      radminStatus
  );

  if (radminExtractRC != RC_RADMIN_OK) {
    CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
               "Could not extract VCA profiles...\n");
    return radminExtractRC;
  }
  
  parms->startIndex = parms->profilesExtracted;
  if (parms->startIndex >= parms->profilesToExtract) {
    CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
               "Stopping extraction of ZCA profiles because the buffer would overflow...\n");
    return RC_RADMIN_OK;
  }

  CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
            "About to extract ZCA profiles...\n");
            
  radminExtractRC = radminRunRACFCommand(
      authInfo,
      "TSS LIST(ACIDS) TYPE(ZCA)",
      userProfilesHandler,
      parms,
      radminStatus
  );

  if (radminExtractRC != RC_RADMIN_OK) {
    CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
               "Could not extract ZCA profiles...\n");
    return radminExtractRC;
  }
  
  parms->startIndex = parms->profilesExtracted;
  if (parms->startIndex >= parms->profilesToExtract) {
    CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
               "Stopping extraction of LSCA profiles because the buffer would overflow...\n");
    return RC_RADMIN_OK;
  }

  CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
            "About to extract LSCA profiles...\n");
            
  radminExtractRC = radminRunRACFCommand(
      authInfo,
      "TSS LIST(ACIDS) TYPE(LSCA)",
      userProfilesHandler,
      parms,
      radminStatus
  );

  if (radminExtractRC != RC_RADMIN_OK) {
    CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
               "Could not extract LSCA profiles...\n");
    return radminExtractRC;
  }
  
  parms->startIndex = parms->profilesExtracted;
  if (parms->startIndex >= parms->profilesToExtract) {
    CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
               "Stopping extraction of SCA profiles because the buffer would overflow...\n");
    return RC_RADMIN_OK;
  }

  CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
            "About to extract SCA profiles...\n");
            
  radminExtractRC = radminRunRACFCommand(
      authInfo,
      "TSS LIST(ACIDS) TYPE(SCA)",
      userProfilesHandler,
      parms,
      radminStatus
  );

  if (radminExtractRC != RC_RADMIN_OK) {
    CMS_DEBUG2(parms->logData->globalArea, parms->logData->traceLevel,
               "Could not extract SCA profiles...\n");
    return radminExtractRC;
  }
  
  return RC_RADMIN_OK;
}

/* The entry point into the user profiles endpoint logic. */
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
  if (!secmgmtGetCallerUserID(&caller)) {
    return RC_ZIS_UPRFSRV_IMPERSONATION_MISSING;
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
    return RC_ZIS_UPRFSRV_ALLOC_FAILED;
  }

  RadminStatus radminStatus = {0};
  
  TSSLogData logData = {
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

  radminExtractRC = getAllUsers(&radminStatus, authInfo, &parms);
  
  CMS_DEBUG2(globalArea, traceLevel,
             "Extracted user %d profiles...\n", parms.profilesExtracted);
             
  /* If the system has no users, there must have been a serious
   * parsing error somewhere. Return some sort of error to prevent
   * having an empty array. Note, even if having no users was correct,
   * returning an empty array is not.
   */ 
  if (parms.profilesExtracted == 0 && radminExtractRC == RC_RADMIN_OK) {
    CMS_DEBUG2(globalArea, traceLevel, "No user profiles could be found...\n");
    radminExtractRC = RC_RADMIN_NONZERO_USER_RC;
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

/* A parameter list passed into the handler function
 * for the group profiles service.
 */
typedef struct TSSGroupProfileParms_t {
  ZISGroupProfileEntry *tmpResultBuffer;
  unsigned int profilesExtracted;
  unsigned int profilesToExtract;
  const char *startGroupProfile;
  TSSLogData *logData;
} TSSGroupProfileParms;

/* A handler function that is called after the r_admin command is issued. It parses the group
 * data from the output by going line by line.
 */
static int groupProfilesHandler(RadminAPIStatus status, const RadminCommandOutput *result,
                                void *userData) {
  if (status.racfRC != 0) {
    return status.racfRC;
  }

  TSSGroupProfileParms *parms = userData;
  ZISGroupProfileEntry *buffer = parms->tmpResultBuffer;
  TSSLogData *logData = parms->logData;
  ZISGroupProfileEntry entry = {0};

  const RadminCommandOutput *currentBlock = result;  
  
  if (currentBlock == NULL) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "No output was found in output message entry on first block...\n");
    return 0;
  }
  
  const RadminVLText *currentLine = &currentBlock->firstMessageEntry;
  
  /* Look for an obvious error before we start parsing.
   * The output will look something like:
   * 
   *  TSS{SOME NUMBERS}  {SOME MESSAGE}                      
   *  TSS0301I  TSS      FUNCTION FAILED, RETURN CODE =  {SOME NUMBER}
   */
  bool errorDetected = tssIsErrorDetected(currentLine->text, currentLine->length);
  if (errorDetected) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "Error detected!\n");
    return -1;
  }
  
  /* If specified, iterate through the output until start is found. If it's not ever found
   * then return an error.
   */
  unsigned int offsetInBlock = 16;
  if (parms->startGroupProfile != NULL) {
    do {
      currentLine = findLineOfStart(currentLine, TSS_ACCESSOR_ID_KEY, parms->startGroupProfile,
                                    ZIS_SECURITY_GROUP_MAX_LENGTH, currentBlock->lastByteOffset,
                                    &offsetInBlock, logData);
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
                 "Start profile %s could not be found...\n", parms->startGroupProfile);
      return 0;
    }
  }

  /* Iterate through the output and store information on the users that
   * are found.
   */
  unsigned int numExtracted = 0;
  do {
    while (offsetInBlock < currentBlock->lastByteOffset) {      
      if (numExtracted == parms->profilesToExtract || tssIsOutputDone(currentLine->text, currentLine->length)) {
        parms->profilesExtracted = numExtracted;
        return 0;
      }

      TSSKeyData keys[TSS_MAX_NUM_OF_KEYS] = {0};
      
      int numberOfKeys = tssParseLineForKeys(currentLine->text, currentLine->length, keys);
      if (numberOfKeys == -1) {
        /* In this case, print the line in question. */
        CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                   "An invalid number of keys were detected on line: '"
                   "%.*s'\n", currentLine->length, currentLine->text);
        return -1;
      }      
      /* A blank line means the end of a single groups's data.
       * Store what was found into the ZISGroupProfileEntry
       * struct.
       */
      else if (numberOfKeys == 0) {
        ZISGroupProfileEntry *p = &buffer[numExtracted++];
        memset(p, 0, sizeof(*p));
        memcpy(p->group, entry.group, sizeof(p->group));

        memcpy(p->owner, entry.owner, sizeof(p->owner));
        memcpy(p->superiorGroup, entry.superiorGroup, sizeof(p->superiorGroup));

        memset(&entry, 0, sizeof(ZISGroupProfileEntry));      
      }
      else { 
        int keyIndex = tssFindDesiredKey(TSS_ACCESSOR_ID_KEY, keys);
        if (keyIndex != -1) {
          int status = tssFindValueForKey(currentLine->text, currentLine->length, keys,
                                       keyIndex, numberOfKeys, entry.group, sizeof(entry.group));
          if (status == -1) {
            CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                       "Could not find value of %s...\n", TSS_ACCESSOR_ID_KEY);
            return -1;
          }
        }
        
        keyIndex = tssFindDesiredKey(TSS_DEPARTMENT_KEY, keys);
        if (keyIndex != -1) {
          int status = tssFindValueForKey(currentLine->text, currentLine->length, keys,
                                       keyIndex, numberOfKeys, entry.owner, sizeof(entry.owner));
          if (status == -1) {
            CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                       "Could not find value of %s...\n", TSS_DEPARTMENT_KEY);
            return -1;
          }
          memcpy(entry.superiorGroup, entry.owner, sizeof(entry.superiorGroup)); /* No superior group in Top Secret. */
        }               
        
      }
    
      offsetInBlock += currentLine->length + 2;
      currentLine = (RadminVLText *)(currentLine->text + currentLine->length);
    }
    
    currentBlock = currentBlock->next;
    if (currentBlock != NULL) {
      currentLine = (RadminVLText *)&(currentBlock->firstMessageEntry);
      offsetInBlock = 16;
    }
  } while (currentBlock != NULL);
  
  return 0;                                 
}

/* Prepares to copy the ZISGroupProfileEntry struct to the other address space where
 * it can be returned to the caller.
 */
static void copyGroupProfiles(ZISGroupProfileEntry *dest,
                             ZISGroupProfileEntry *src,
                             unsigned int count) {
  for (unsigned int i = 0; i < count; i++) {
    cmCopyToSecondaryWithCallerKey(dest[i].group, src[i].group,
                                   sizeof(dest[i].group));
    cmCopyToSecondaryWithCallerKey(dest[i].owner, src[i].owner,
                                   sizeof(dest[i].owner));
    cmCopyToSecondaryWithCallerKey(dest[i].superiorGroup, src[i].superiorGroup,
                                   sizeof(dest[i].superiorGroup));
  }                        
}

/* The entry point into the group profiles endpoint logic. Note, that profile is actually the
 * equivalent to a groups in RACF (not to be confused with general resource profile). */
int zisGroupProfilesServiceFunctionTSS(CrossMemoryServerGlobalArea *globalArea,
                                       CrossMemoryService *service, void *parm) {
  int status = RC_ZIS_GPPRFSRV_OK;
  int radminExtractRC = RC_RADMIN_OK;

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_GPPRFSRV_PARMLIST_NULL;
  }

  ZISGroupProfileServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr,
                                   sizeof(localParmList));

  int parmValidateRC = validateGroupProfileParmList(&localParmList);
  if (parmValidateRC != RC_ZIS_GPPRFSRV_OK) {
    return parmValidateRC;
  }

  int traceLevel = localParmList.traceLevel;
  if (globalArea->pcLogLevel > traceLevel) {
    traceLevel = globalArea->pcLogLevel;
  }

  RadminUserID caller;
  if (!secmgmtGetCallerUserID(&caller)) {
    return RC_ZIS_GPPRFSRV_IMPERSONATION_MISSING;
  }

  CMS_DEBUG2(globalArea, traceLevel,
             "GPPRFSRV: group = \'%.*s\', profilesToExtract = %u,"
             " result buffer = 0x%p\n",
             localParmList.startGroup.length,
             localParmList.startGroup.value,
             localParmList.profilesToExtract,
             localParmList.resultBuffer);
  CMS_DEBUG2(globalArea, traceLevel, "GPPRFSRV: caller = \'%.*s\'\n",
             caller.length, caller.value);

  RadminCallerAuthInfo authInfo = {
      .acee = NULL,
      .userID = caller
  };

  char groupBuffer[ZIS_SECURITY_GROUP_MAX_LENGTH + 1] = {0};
  memcpy(groupBuffer, localParmList.startGroup.value,
         localParmList.startGroup.length);
  const char *startGroupNullTerm = localParmList.startGroup.length > 0 ?
                                    groupBuffer : NULL;
                                    
  size_t tmpResultBufferSize =
      sizeof(ZISGroupProfileEntry) * localParmList.profilesToExtract;
  int allocRC = 0, allocSysRC = 0, allocSysRSN = 0;
  ZISGroupProfileEntry *tmpResultBuffer =
      (ZISGroupProfileEntry *)safeMalloc64v3(
          tmpResultBufferSize, TRUE,
          "ZISGroupProfileEntry",
          &allocRC,
          &allocSysRSN,
          &allocSysRSN
      );

  CMS_DEBUG2(globalArea, traceLevel,
             "GPPRFSRV: tmpBuff allocated @ 0x%p (%zu %d %d %d)\n",
             tmpResultBuffer, tmpResultBufferSize,
             allocRC, allocSysRC, allocSysRSN);

  if (tmpResultBuffer == NULL) {
    return RC_ZIS_GPPRFSRV_ALLOC_FAILED;
  }

  RadminStatus radminStatus = {0};
  
  TSSLogData logData = {
    .globalArea = globalArea,
    .traceLevel = traceLevel
  };
  
  TSSGroupProfileParms parms = {
    .profilesToExtract = localParmList.profilesToExtract,
    .tmpResultBuffer = tmpResultBuffer,
    .startGroupProfile = startGroupNullTerm,
    .logData = &logData,
  };
  
  radminExtractRC = radminRunRACFCommand(
      authInfo,
      "TSS LIST(ACIDS) TYPE(PROFILE)",
      groupProfilesHandler,
      &parms,
      &radminStatus
  );
  
  CMS_DEBUG2(globalArea, traceLevel,
             "Extracted %d group profiles...\n", parms.profilesExtracted);
  
  /* If the system has no groups, there must have been a serious
   * parsing error somewhere. Return some sort of error to prevent
   * having an empty array. Note, even if having no groups was correct,
   * returning an empty array is not.
   */ 
  if (parms.profilesExtracted == 0 && radminExtractRC == RC_RADMIN_OK) {
    CMS_DEBUG2(globalArea, traceLevel, "No group profiles could be found...\n");
    radminExtractRC = RC_RADMIN_NONZERO_USER_RC;
  }

  if (radminExtractRC == RC_RADMIN_OK) {
    copyGroupProfiles(localParmList.resultBuffer, tmpResultBuffer,
                       parms.profilesExtracted);
    localParmList.profilesExtracted = parms.profilesExtracted;
  }
  else {
    localParmList.internalServiceRC = radminExtractRC;
    localParmList.internalServiceRSN = radminStatus.reasonCode;
    localParmList.safStatus.safRC = radminStatus.apiStatus.safRC;
    localParmList.safStatus.racfRC = radminStatus.apiStatus.racfRC;
    localParmList.safStatus.racfRSN = radminStatus.apiStatus.racfRSN;
    status = RC_ZIS_GPPRFSRV_INTERNAL_SERVICE_FAILED;
  }

  int freeRC = 0, freeSysRC = 0, freeSysRSN = 0;
  freeRC = safeFree64v3(tmpResultBuffer, tmpResultBufferSize,
                          &freeSysRC, &freeSysRSN);

  CMS_DEBUG2(globalArea, traceLevel,
             "GPPRFSRV: tmpBuff free'd @ 0x%p (%zu %d %d %d)\n",
             tmpResultBuffer, tmpResultBufferSize,
             freeRC, freeSysRC, freeSysRSN);

  tmpResultBuffer = NULL;

  cmCopyToSecondaryWithCallerKey(clientParmAddr, &localParmList,
                                 sizeof(ZISGroupProfileServiceParmList));

  return status;                                    
}

/* A parameter list passed into the handler function
 * for the genres profile access list extraction service.
 */ 
typedef struct TSSGenresProfileAccessListParms_t {
  ZISGenresAccessEntry *tmpResultBuffer;
  unsigned int entriesExtracted;
  unsigned int entriesFound;
  unsigned int entriesToExtract;
  char *searchProfile;
  TSSLogData *logData;
} TSSGenresProfileAccessListParms;

/* A handler function that is called after the r_admin command is issued. It parses the access list
 * data from the output by going line by line.
 */
static int genresProfileAccessListHandler(RadminAPIStatus status, const RadminCommandOutput *result,
                                          void *userData) {
  if (status.racfRC != 0) {
    return status.racfRC;
  }

  TSSGenresProfileAccessListParms *parms = userData;
  ZISGenresAccessEntry *buffer = parms->tmpResultBuffer;
  TSSLogData *logData = parms->logData;
  ZISGenresAccessEntry entry = {0};

  const RadminCommandOutput *currentBlock = result;  
  
  if (currentBlock == NULL) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "No output was found in output message entry on first block...\n");
    return 0;
  }
  
  const RadminVLText *currentLine = &currentBlock->firstMessageEntry;
  
  /* Look for an obvious error before we start parsing.
   * The output will look something like:
   * 
   *  TSS{SOME NUMBERS}  {SOME MESSAGE}                      
   *  TSS0301I  TSS      FUNCTION FAILED, RETURN CODE =  {SOME NUMBER}
   */
  bool errorDetected = tssIsErrorDetected(currentLine->text, currentLine->length);
  if (errorDetected) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "Error detected!\n");
    return -1;
  }
  
  CMS_DEBUG2(logData->globalArea, logData->traceLevel,
             "Finding number of valid entries...\n");
               
  unsigned int numberOfEntries = 0;
  do {                     
    int status = findNumberOfValidGenreEntries(currentLine, currentBlock->lastByteOffset,
                                               parms->searchProfile, &numberOfEntries, logData);
  
    if (status == -1) {
      CMS_DEBUG2(logData->globalArea, logData->traceLevel,
                "There was an error finding the valid genre entries...\n",
                 numberOfEntries);
      return -1;
    }
           
    currentBlock = currentBlock->next;
    if (currentBlock != NULL) {
      currentLine = (RadminVLText *)&(currentBlock->firstMessageEntry);
    }
  } while (currentBlock != NULL);
  
  if (numberOfEntries == 0) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "No valid entries were found...\n");
    return -1;
  }
  
  CMS_DEBUG2(logData->globalArea, logData->traceLevel,
            "Found %d valid entries...\n", numberOfEntries);
            
  parms->entriesFound = numberOfEntries;
  
  currentBlock = result; /* Bring us back to the beginning. */
  currentLine = &currentBlock->firstMessageEntry;
  
  unsigned int offsetInBlock = 16;

  /* Iterate through the output and store information on the access list that
   * are found.
   */
  unsigned int numExtracted = 0;
  bool foundProfile = FALSE; /* Keeps track of which access level goes to which user */
  do {      
    while (offsetInBlock < currentBlock->lastByteOffset) {      
      if (numExtracted == parms->entriesToExtract || tssIsOutputDone(currentLine->text, currentLine->length)) {
        parms->entriesExtracted = numExtracted;
        return 0;
      }

      TSSKeyData keys[TSS_MAX_NUM_OF_KEYS] = {0};
      
      int numberOfKeys = tssParseLineForKeys(currentLine->text, currentLine->length, keys);
      if (numberOfKeys == -1 || numberOfKeys == 0) {
        /* In this case, print the line in question. */
        CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                   "An invalid number of keys were detected on line: '"
                   "%.*s'\n", currentLine->length, currentLine->text);
        return -1;
      }
      else {
        int keyIndex = tssFindDesiredKey(TSS_XAUTH_KEY, keys);
        if (keyIndex != -1) {           
          char tmpBuffer[ZIS_SECURITY_PROFILE_MAX_LENGTH] = {0};
          int acidIndex = lastIndexOfString((char*)currentLine->text, currentLine->length, TSS_ACID_KEY);
          if (acidIndex == -1) {
            return -1;
          }
          
          int status = tssFindValueForKey(currentLine->text, acidIndex-1, keys,
                                          keyIndex, numberOfKeys, tmpBuffer,
                                          sizeof(tmpBuffer));
          if (status == -1) {
            CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                       "Could not find value of %s...\n", TSS_XAUTH_KEY);
            return -1;
          }
                     
          int valueLen = tssGetActualValueLength(tmpBuffer, sizeof(tmpBuffer));
                 
          int profileLength = strlen(parms->searchProfile);
                  
          if (valueLen == profileLength) {
            if (!memcmp(tmpBuffer, parms->searchProfile, profileLength)) {           
              memcpy(entry.id, currentLine->text+acidIndex+5, sizeof(entry.id));
              
              foundProfile = TRUE; /* Wait for next access type. */
            }
          } 
        }
        else {
          if (foundProfile) {
            keyIndex = tssFindDesiredKey(TSS_ACCESS_KEY, keys);
            if (keyIndex != -1) {           
              int status = tssFindValueForKey(currentLine->text, currentLine->length, keys,
                                              keyIndex, numberOfKeys, entry.accessType,
                                              sizeof(entry.accessType));
              if (status == -1) {
                CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                           "Could not find value of %s...\n", TSS_ACCESS_KEY);
                return -1;
              }
              
              ZISGenresAccessEntry *p = &buffer[numExtracted++];
              memset(p, 0, sizeof(*p));
              
              memcpy(p->id, entry.id, sizeof(p->id));


              if (!memcmp(entry.accessType, "ALL", 3)) { /* Top Secret doesn't have ALL. */
                memset(entry.accessType, 0, sizeof(entry.accessType));
                memcpy(entry.accessType, "ALTER", 5);
                padWithSpaces(entry.accessType, sizeof(entry.accessType), 1, 1);
              }
              
              
              memcpy(p->accessType, entry.accessType, sizeof(p->accessType));
              
              memset(&entry, 0, sizeof(ZISGenresAccessEntry));
              
              foundProfile = FALSE; /* Wait for next profile entry. */
            }
          }
        }             
      }
    
      offsetInBlock += currentLine->length + 2;
      currentLine = (RadminVLText *)(currentLine->text + currentLine->length);
    }
    
    currentBlock = currentBlock->next;
    if (currentBlock != NULL) {
      currentLine = (RadminVLText *)&(currentBlock->firstMessageEntry);
      offsetInBlock = 16;
    }
  } while (currentBlock != NULL);
  
  return 0;                                 
}

/* Prepares to copy the ZISGenresAccessEntry struct to the other address space where
 * it can be returned to the caller.
 */
static void copyGenresAccessList(ZISGenresAccessEntry *dest,
                                 const ZISGenresAccessEntry *src,
                                 unsigned int count) {
  for (unsigned int i = 0; i < count; i++) {
    cmCopyToSecondaryWithCallerKey(dest[i].id, src[i].id,
                                   sizeof(dest[i].id));
    cmCopyToSecondaryWithCallerKey(dest[i].accessType, src[i].accessType,
                                   sizeof(dest[i].accessType));
  }
}

/* The entry point into the genres profiles endpoint logic. Note, access lists aren't
 * stored in the profile like in RACF, instead the user has it. It makes this process
 * much more annoying to find the access list entries because of it.
 */ 
int zisGenresAccessListServiceFunctionTSS(CrossMemoryServerGlobalArea *globalArea,
                                          CrossMemoryService *service, void *parm) {                                           
  int status = RC_ZIS_ACSLSRV_OK;
  int radminExtractRC = RC_RADMIN_OK;
  
  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_ACSLSRV_PARMLIST_NULL;
  }
  
  ZISGenresAccessListServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr,
                                   sizeof(localParmList));
  
  int parmValidateRC = validateGenresAccessListParmList(&localParmList);
  if (parmValidateRC != RC_ZIS_ACSLSRV_OK) {
    return parmValidateRC;
  }

  int traceLevel = localParmList.traceLevel;
  if (globalArea->pcLogLevel > traceLevel) {
    traceLevel = globalArea->pcLogLevel;
  }
  
  RadminUserID caller;
  if (!secmgmtGetCallerUserID(&caller)) {
    return RC_ZIS_ACSLSRV_IMPERSONATION_MISSING;
  }
  
  CMS_DEBUG2(globalArea, traceLevel,
             "ACSLSRV: class = \'%.*s\', result buffer = 0x%p (%u)\n",
             localParmList.class.length, localParmList.class.value,
             localParmList.resultBuffer,
             localParmList.resultBufferCapacity);
  CMS_DEBUG2(globalArea, traceLevel, "ACSLSRV: profile = \'%.*s\'\n",
             localParmList.profile.length, localParmList.profile.value);
  CMS_DEBUG2(globalArea, traceLevel, "ACSLSRV: caller = \'%.*s\'\n",
             caller.length, caller.value);
             
  RadminCallerAuthInfo authInfo = {
      .acee = NULL,
      .userID = caller
  };
 
  char classNullTerm[ZIS_SECURITY_CLASS_MAX_LENGTH + 1] = {0};
  memcpy(classNullTerm, localParmList.class.value,
         localParmList.class.length);
  char profileNullTerm[ZIS_SECURITY_PROFILE_MAX_LENGTH + 1] = {0};
  memcpy(profileNullTerm, localParmList.profile.value,
         localParmList.profile.length);

  if (localParmList.class.length == 0) {
    CrossMemoryServerConfigParm userClassParm = {0};
    int getParmRC = cmsGetConfigParm(&globalArea->serverName,
                                     ZIS_PARMLIB_PARM_SECMGMT_USER_CLASS,
                                     &userClassParm);
    if (getParmRC != RC_CMS_OK) {
      localParmList.internalServiceRC = getParmRC;
      return RC_ZIS_ACSLSRV_USER_CLASS_NOT_READ;
    }
    if (userClassParm.valueLength > ZIS_SECURITY_CLASS_MAX_LENGTH) {
      return RC_ZIS_ACSLSRV_CLASS_TOO_LONG;
    }
    memcpy(classNullTerm, userClassParm.charValueNullTerm,
           userClassParm.valueLength);
  }

  size_t tmpResultBufferSize =
      sizeof(ZISGenresAccessEntry) * localParmList.resultBufferCapacity;
  int allocRC = 0, allocSysRC = 0, allocSysRSN = 0;
  ZISGenresAccessEntry *tmpResultBuffer =
      (ZISGenresAccessEntry *)safeMalloc64v3(tmpResultBufferSize, TRUE,
                                              "ZISGenresAccessEntry",
                                               &allocRC,
                                               &allocSysRC,
                                               &allocSysRSN);
  CMS_DEBUG2(globalArea, traceLevel,
             "ACSLSRV: tmpBuff allocated @ 0x%p (%zu %d %d %d)\n",
             tmpResultBuffer, tmpResultBufferSize,
             allocRC, allocSysRC, allocSysRSN);

  if (tmpResultBuffer == NULL) {
    return RC_ZIS_ACSLSRV_ALLOC_FAILED;
  }

  RadminStatus radminStatus = {0};
  
  TSSLogData logData = {
    .globalArea = globalArea,
    .traceLevel = traceLevel
  };
  
  TSSGenresProfileAccessListParms parms = {
    .tmpResultBuffer = tmpResultBuffer,
    .logData = &logData,
    .entriesToExtract = localParmList.resultBufferCapacity,
    .searchProfile = profileNullTerm,
  };
  
  CMS_DEBUG2(globalArea, traceLevel, "ResultBufferCapacity: %d\n",
             localParmList.resultBufferCapacity);

#define TSS_PRF_ACCESS_LIST_EXTR_MAX_LEN 274 /* Class - 8, Profile - 255, Rest of command - 11 */              
  char extractGenresProfileAccessListCommand[TSS_PRF_ACCESS_LIST_EXTR_MAX_LEN + 1] = {0};
  snprintf(extractGenresProfileAccessListCommand, sizeof(extractGenresProfileAccessListCommand),
          "TSS WHOH %s(%s)", classNullTerm, profileNullTerm);
             
  radminExtractRC = radminRunRACFCommand(
      authInfo,
      extractGenresProfileAccessListCommand,
      genresProfileAccessListHandler,
      &parms,
      &radminStatus
  );

  if (radminExtractRC == RC_RADMIN_OK) {
    copyGenresAccessList(localParmList.resultBuffer, tmpResultBuffer,
                         parms.entriesExtracted);
    localParmList.entriesExtracted = parms.entriesExtracted;
    localParmList.entriesFound = parms.entriesExtracted;
  } else if (radminExtractRC == RC_RADMIN_OK && parms.entriesExtracted < parms.entriesFound) {
    localParmList.entriesExtracted = 0;
    localParmList.entriesFound = parms.entriesFound;
    status = RC_ZIS_ACSLSRV_INSUFFICIENT_SPACE;
  } else  {
    localParmList.internalServiceRC = radminExtractRC;
    localParmList.internalServiceRSN = radminStatus.reasonCode;
    localParmList.safStatus.safRC = radminStatus.apiStatus.safRC;
    localParmList.safStatus.racfRC = radminStatus.apiStatus.racfRC;
    localParmList.safStatus.racfRSN = radminStatus.apiStatus.racfRSN;
    status = RC_ZIS_ACSLSRV_INTERNAL_SERVICE_FAILED;
  }

  int freeRC = 0, freeSysRC = 0, freeSysRSN = 0;
  freeRC = safeFree64v3(tmpResultBuffer, tmpResultBufferSize,
                        &freeSysRC, &freeSysRSN);
  CMS_DEBUG2(globalArea, traceLevel,
             "ACSLSRV: tmpBuff free'd @ 0x%p (%zu %d %d %d)\n",
             tmpResultBuffer, tmpResultBufferSize,
             freeRC, freeSysRC, freeSysRSN);

  tmpResultBuffer = NULL;

  
  cmCopyToSecondaryWithCallerKey(clientParmAddr, &localParmList,
                                 sizeof(ZISGenresAccessListServiceParmList));

  CMS_DEBUG2(globalArea, traceLevel,
             "ACSLSRV: entriesExtracted = %u, RC = %d, "
             "internal RC = %d, RSN = %d, "
             "SAF RC = %d, RACF RC = %d, RSN = %d\n",
             localParmList.entriesExtracted, radminExtractRC,
             localParmList.internalServiceRC,
             localParmList.internalServiceRSN,
             localParmList.safStatus.safRC,
             localParmList.safStatus.racfRC,
             localParmList.safStatus.racfRSN);

  return status;
}

/* A parameter list passed into the handler function
 * for the group access list extraction service.
 */ 
typedef struct TSSGroupAccessListParms_t {
  ZISGroupAccessEntry *tmpResultBuffer;
  int entriesExtracted;
  int entriesFound;
  int entriesToExtract;
  TSSLogData *logData;
} TSSGroupAccessListParms;

/* A handler function that is called after the r_admin command is issued. It parses the group access list
 * data from the output by going line by line.
 */
static int groupAccessListHandler(RadminAPIStatus status, const RadminCommandOutput *result,
                                  void *userData) {
  if (status.racfRC != 0) {
    return status.racfRC;
  }

  TSSGroupAccessListParms *parms = userData;
  ZISGroupAccessEntry *buffer = parms->tmpResultBuffer;
  TSSLogData *logData = parms->logData;
  ZISGroupAccessEntry entry = {0};

  const RadminCommandOutput *currentBlock = result;  
  
  if (currentBlock == NULL) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "No output was found in output message entry on first block...\n");
    return 0;
  }
  
  const RadminVLText *currentLine = &currentBlock->firstMessageEntry;
  
  /* Look for an obvious error before we start parsing.
   * The output will look something like:
   * 
   *  TSS{SOME NUMBERS}  {SOME MESSAGE}                      
   *  TSS0301I  TSS      FUNCTION FAILED, RETURN CODE =  {SOME NUMBER}
   */
  bool errorDetected = tssIsErrorDetected(currentLine->text, currentLine->length);
  if (errorDetected) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "Error detected!\n");
    return -1;
  }
  
  currentLine = (RadminVLText *)(currentLine->text + currentLine->length); /* go to next line */
  
  unsigned int numberOfLines = 0;
  do {
    currentLine = getNumberOfLinesOfData(currentLine, currentBlock->lastByteOffset,
                                         &numberOfLines);
                                         
    if (currentLine == NULL) {
      break;
    }
    
    currentBlock = currentBlock->next;
    if (currentBlock != NULL) {
      currentLine = &currentBlock->firstMessageEntry;
    }
  } while (currentBlock != NULL);
  
  CMS_DEBUG2(logData->globalArea, logData->traceLevel,
             "Found %d total lines of data...\n", numberOfLines);
  
  parms->entriesFound = numberOfLines * 4 ; /* four entries max on each line */
  
  CMS_DEBUG2(logData->globalArea, logData->traceLevel,
             "Total number of users in group estimated at %d...\n",
             parms->entriesFound);
             
  currentBlock = result; /* point back to first block */
  currentLine = &currentBlock->firstMessageEntry; /* point to start of output */
  currentLine = (RadminVLText *)(currentLine->text + currentLine->length); /* go to next line */
  
  int acidsIndex = indexOfString((char *)currentLine->text, currentLine->length, TSS_ACIDS_KEY, 0);
  if (acidsIndex == -1) {
    return -1; /* not expected */
  }
  
  int delimIndex = indexOfString((char *)currentLine->text, currentLine->length, TSS_KEY_DELIMITER, 0);
  if (delimIndex == -1) {
    return -1;
  }
    
  unsigned int offsetInBlock = 16;
  
  /* Iterate through the output and store information on the users that
   * are found.
   */
  unsigned int numExtracted = 0; 
  
  bool firstLoop = TRUE;
  do {
    while (offsetInBlock < currentBlock->lastByteOffset) {      
      if (numExtracted == parms->entriesToExtract || tssIsOutputDone(currentLine->text, currentLine->length)) {
        parms->entriesExtracted = numExtracted;
        return 0;
      }
      
      if (!tssIsLineBlank(currentLine->text, currentLine->length)) {
        TSSAcidData acids[4] = {0};
        unsigned int acidsFound = 0;
        if (firstLoop) {
          acidsFound = getAcidsInLine(currentLine, acids, logData, delimIndex+TSS_KEY_DELIMITER_LEN);
          firstLoop = FALSE;
        }
        else {
          acidsFound = getAcidsInLine(currentLine, acids, logData, 0);
        }
        if (acidsFound == 0 || acidsFound > 4) {
          CMS_DEBUG2(logData->globalArea, logData->traceLevel,
                     "Invalid number of acids found...\n");
          return -1;
        }
        
        CMS_DEBUG2(logData->globalArea, logData->traceLevel,
                   "%d acids were found on line: %s\n", acidsFound);
                   
        
        for (int i = 0; i < acidsFound; i++) {
           CMS_DEBUG2(logData->globalArea, logData->traceLevel,
                   "ACID %d: %.*s\n", acidsFound, sizeof(acids[i].acid), acids[i].acid);
                   
          ZISGroupAccessEntry *p = &buffer[numExtracted++];
          memset(p, 0, sizeof(*p));
            
          memcpy(p->id, acids[i].acid, sizeof(p->id));
          
          if (acids[i].isAdmin) {
            memcpy(p->accessType, "JOIN", sizeof(p->accessType));
          }
          else {
            memcpy(p->accessType, "USE", sizeof(p->accessType));
          }     
        }
      }
      
      offsetInBlock += currentLine->length + 2;
      currentLine = (RadminVLText *)(currentLine->text + currentLine->length);
    }
    
    currentBlock = currentBlock->next;
    if (currentBlock != NULL) {
      currentLine = (RadminVLText *)&(currentBlock->firstMessageEntry);
      offsetInBlock = 16;
    }
  } while (currentBlock != NULL);
  
  return 0;
}

/* Prepares to copy the ZISGroupAccessEntry struct to the other address space where
 * it can be returned to the caller.
 */
static void copyGroupAccessList(ZISGroupAccessEntry *dest,
                                const ZISGroupAccessEntry *src,
                                unsigned int count) {

  for (unsigned int i = 0; i < count; i++) {
    cmCopyToSecondaryWithCallerKey(dest[i].id, src[i].id,
                                   sizeof(dest[i].id));
    cmCopyToSecondaryWithCallerKey(dest[i].accessType, src[i].accessType,
                                   sizeof(dest[i].accessType));
  }
}

/* The entry point into the group access list endpoint logic. */
int zisGroupAccessListServiceFunctionTSS(CrossMemoryServerGlobalArea *globalArea,
                                         CrossMemoryService *service, void *parm) {                                       
  int status = RC_ZIS_GRPALSRV_OK;
  int radminExtractRC = RC_RADMIN_OK;

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_GRPALSRV_PARMLIST_NULL;
  }

  ZISGroupAccessListServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr,
                                   sizeof(localParmList));

  int parmValidateRC = validateGroupAccessListParmList(&localParmList);
  if (parmValidateRC != RC_ZIS_GRPALSRV_OK) {
    return parmValidateRC;
  }

  int traceLevel = localParmList.traceLevel;
  if (globalArea->pcLogLevel > traceLevel) {
    traceLevel = globalArea->pcLogLevel;
  }
  
  RadminUserID caller;
  if (!secmgmtGetCallerUserID(&caller)) {
    return RC_ZIS_GRPALSRV_IMPERSONATION_MISSING;
  }

  CMS_DEBUG2(globalArea, traceLevel,
             "GRPALSRV: group = \'%.*s\', result buffer = 0x%p (%u)\n",
             localParmList.group.length, localParmList.group.value,
             localParmList.resultBuffer,
             localParmList.resultBufferCapacity);
  CMS_DEBUG2(globalArea, traceLevel, "GRPALSRV: caller = \'%.*s\'\n",
             caller.length, caller.value);

  RadminCallerAuthInfo authInfo = {
      .acee = NULL,
      .userID = caller
  };

  char groupNameNullTerm[ZIS_SECURITY_GROUP_MAX_LENGTH + 1] = {0};
  memcpy(groupNameNullTerm, localParmList.group.value,
         localParmList.group.length);

  size_t tmpResultBufferSize =
      sizeof(ZISGroupAccessEntry) * localParmList.resultBufferCapacity;
  int allocRC = 0, allocSysRC = 0, allocSysRSN = 0;
  ZISGroupAccessEntry *tmpResultBuffer =
      (ZISGroupAccessEntry *)safeMalloc64v3(tmpResultBufferSize, TRUE,
                                              "RadminGroupAccessListEntry",
                                               &allocRC,
                                               &allocSysRC,
                                               &allocSysRSN);
  CMS_DEBUG2(globalArea, traceLevel,
             "GRPALSRV: tmpBuff allocated @ 0x%p (%zu %d %d %d)\n",
             tmpResultBuffer, tmpResultBufferSize,
             allocRC, allocSysRC, allocSysRSN);

  if (tmpResultBuffer == NULL) {
    return RC_ZIS_GRPALSRV_ALLOC_FAILED;
  }
  
  RadminStatus radminStatus = {0};
  
  TSSLogData logData = {
    .globalArea = globalArea,
    .traceLevel = traceLevel
  };
  
  TSSGroupAccessListParms parms = {
    .tmpResultBuffer = tmpResultBuffer,
    .logData = &logData,
    .entriesToExtract = localParmList.resultBufferCapacity,
  };
  
  CMS_DEBUG2(globalArea, traceLevel, "ResultBufferCapacity: %d\n",
             localParmList.resultBufferCapacity);
             
#define TSS_GRP_ACCESS_LIST_EXTR_MAX_LEN 30 /* ACID - 8, Rest of command - 22 */             
  char extractGroupAccessList[TSS_GRP_ACCESS_LIST_EXTR_MAX_LEN + 1] = {0};
  snprintf(extractGroupAccessList, sizeof(extractGroupAccessList), "TSS LIST(%s) DATA(ACIDS)", groupNameNullTerm);
  
  radminExtractRC = radminRunRACFCommand(
      authInfo,
      extractGroupAccessList,
      groupAccessListHandler,
      &parms,
      &radminStatus
  );

  CMS_DEBUG2(globalArea, traceLevel, "Entries Extracted: %d\n",
             parms.entriesExtracted);
             
  CMS_DEBUG2(globalArea, traceLevel, "Entries Found: %d\n",
             parms.entriesFound);
  
  int entryDifference = parms.entriesFound - parms.entriesExtracted;
  
  if (radminExtractRC == RC_RADMIN_OK && entryDifference > 4) {
    localParmList.entriesExtracted = 0;
    localParmList.entriesFound = parms.entriesFound;
    status = RC_ZIS_GRPALSRV_INSUFFICIENT_SPACE;
  } 
  else if (radminExtractRC == RC_RADMIN_OK && entryDifference <= 4) {
    copyGroupAccessList(localParmList.resultBuffer, tmpResultBuffer,
                        parms.entriesExtracted);
    localParmList.entriesExtracted = parms.entriesExtracted;
    localParmList.entriesFound = parms.entriesExtracted;
  }
  else {
    localParmList.internalServiceRC = radminExtractRC;
    localParmList.internalServiceRSN = radminStatus.reasonCode;
    localParmList.safStatus.safRC = radminStatus.apiStatus.safRC;
    localParmList.safStatus.racfRC = radminStatus.apiStatus.racfRC;
    localParmList.safStatus.racfRSN = radminStatus.apiStatus.racfRSN;
    status = RC_ZIS_GRPALSRV_INTERNAL_SERVICE_FAILED;
  }

  int freeRC = 0, freeSysRC = 0, freeSysRSN = 0;
  freeRC = safeFree64v3(tmpResultBuffer, tmpResultBufferSize,
                          &freeSysRC, &freeSysRSN);

  CMS_DEBUG2(globalArea, traceLevel,
             "GPPRFSRV: tmpBuff free'd @ 0x%p (%zu %d %d %d)\n",
             tmpResultBuffer, tmpResultBufferSize,
             freeRC, freeSysRC, freeSysRSN);

  tmpResultBuffer = NULL;

  cmCopyToSecondaryWithCallerKey(clientParmAddr, &localParmList,
                                 sizeof(ZISGroupProfileServiceParmList));

  return status;
}

/* After issuing a command, TSS will spit out
 * a reason code if there is an error, or a 
 * messageid.
 */
typedef struct TSSCommandOutput_t {
  int returnCode;
  char reasonMessageID[TSS_MESSAGE_ID_MAX_LEN];
} TSSCommandOutput;

/* This function returns TRUE on a command failure or FALSE
 * on a command success.
 */
static bool didCommandFail(RadminAPIStatus status, const RadminCommandOutput *result,
                           void *userData) {
  
  if (status.racfRC != 0) {
    return TRUE;
  }
     
  TSSCommandOutput *output = userData;
  
  const RadminCommandOutput *currentBlock = result;  
  const RadminVLText *currentLine = &currentBlock->firstMessageEntry;
   
  int offsetInBlock = 16;
  bool commandFailure = FALSE;
  do {
    while (offsetInBlock < currentBlock->lastByteOffset) {
      if (currentLine->length >= TSS_MESSAGE_ID_MAX_LEN) {
        if (!memcmp(currentLine->text, TSS_COMMAND_SUCCESS, strlen(TSS_COMMAND_SUCCESS))) {
          output->returnCode = 0;
          return commandFailure;
        }
        if (!memcmp(currentLine->text, TSS_COMMAND_FAILURE, strlen(TSS_COMMAND_FAILURE))) {
          int retCodeIndex = lastIndexOfString((char*)currentLine->text, currentLine->length, "RETURN CODE");
          int equalSignIndex = indexOfString((char*)currentLine->text, currentLine->length, "=", retCodeIndex);
           
          int valueIndex = equalSignIndex;
          while (valueIndex <= currentLine->length - 1 && valueIndex >= 0) {
            if (isdigit(currentLine->text[valueIndex])) {
              char c1 = currentLine->text[valueIndex];
              char c2 = '\0';
               
              if (valueIndex + 1 <= currentLine->length - 1) {
                if (isdigit(currentLine->text[valueIndex + 1])) {
                  c2 = currentLine->text[valueIndex + 1];
                }
              }
               
              char returnCodeString[3] = { c1, c2, '\0'};
              
              int returnCode = atoi(returnCodeString);
              output->returnCode = returnCode;
               
              break;
            }
             
            valueIndex++;
          }
          
          commandFailure = TRUE;
        }
      }
       
      offsetInBlock += currentLine->length + 2;
      currentLine = (RadminVLText *)(currentLine->text + currentLine->length);
    }
       
    currentBlock = currentBlock->next;
    if (currentBlock != NULL) {
      currentLine = &(currentBlock->firstMessageEntry);
      offsetInBlock = 16;
    }
  } while (currentBlock != NULL);
   
   currentBlock = result;  
   currentLine = (RadminVLText *)&currentBlock->firstMessageEntry;
   
   if (currentLine->length >= TSS_MESSAGE_ID_MAX_LEN) {
     if (!memcmp(currentLine->text, TSS_FUNCTION_END, strlen(TSS_FUNCTION_END))) {
       memcpy(output->reasonMessageID, currentLine->text, sizeof(output->reasonMessageID));
     }
   }
  
  return commandFailure;
}

/* Executes an admin command */
static int executeAdminCommand(RadminStatus *radminStatus,
                               char *operatorCommand, char *serviceMessage,
                               int serviceMessageLength,
                               RadminCallerAuthInfo authInfo) {
  int radminActionRC = RC_RADMIN_OK;

  TSSCommandOutput output = {0};
                     
  radminActionRC = radminRunRACFCommand(
       authInfo,
       operatorCommand,
       didCommandFail,
       &output,
       radminStatus
  );
  
  if (radminActionRC != RC_RADMIN_OK) {
    if (serviceMessageLength >= TSS_MESSAGE_ID_MAX_LEN) {
      snprintf(serviceMessage, serviceMessageLength, "%.*s, RC = %d", sizeof(output.reasonMessageID), 
               output.reasonMessageID, output.returnCode);
    }
  }
  
  return radminActionRC;
}

/* The entry point into the group admin endpoint logic. */
int zisGroupAdminServiceFunctionTSS(CrossMemoryServerGlobalArea *globalArea,
                                    CrossMemoryService *service, void *parm) {
  int status = RC_ZIS_GRPASRV_OK;
  int radminActionRC = RC_RADMIN_OK;

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_GRPASRV_PARMLIST_NULL;
  }

  ZISGroupAdminServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr,
                                   sizeof(localParmList));

  int parmValidateRC = validateGroupParmList(&localParmList);
  if (parmValidateRC != RC_ZIS_GRPASRV_OK) {
    return parmValidateRC;
  }

  int traceLevel = localParmList.traceLevel;
  if (globalArea->pcLogLevel > traceLevel) {
    traceLevel = globalArea->pcLogLevel;
  }

  RadminUserID caller;
  if (!secmgmtGetCallerUserID(&caller)) {
    return RC_ZIS_GRPASRV_IMPERSONATION_MISSING;
  }

  CMS_DEBUG2(globalArea, traceLevel,
             "GRPASRV: flags = 0x%X, function = %d, group = \'%.*s\', "
             "superior group = \'%.*s\', user = \'%.*s\', access = %d'\n",
             localParmList.flags,
             localParmList.function,
             localParmList.group.length, localParmList.group.value,
             localParmList.superiorGroup.length,
             localParmList.superiorGroup.value,
             localParmList.user.length, localParmList.user.value,
             localParmList.accessType);
  CMS_DEBUG2(globalArea, traceLevel, "GRPASRV: caller = \'%.*s\'\n",
             caller.length, caller.value);

  RadminCallerAuthInfo authInfo = {
      .acee = NULL,
      .userID = caller
  };
    
  bool isDryRun = (localParmList.flags & ZIS_GROUP_ADMIN_FLAG_DRYRUN) ?
                   true : false;
  
  RadminStatus radminStatus = {0};
    
  char serviceMessage[TSS_SERVICE_MSG_MAX_LEN + 1] = {0};
  
  if (localParmList.function == ZIS_GROUP_ADMIN_SRV_FC_ADD) {
#define TSS_GRP_ADMIN_ADD_MAX_LEN 94 /* ACID - 8, Name - 32, Department - 8, Rest of command - 46 */
    char operatorCommand[TSS_GRP_ADMIN_ADD_MAX_LEN + 1] = {0};
                   
    snprintf(operatorCommand, sizeof(operatorCommand), 
             "TSS CREATE(%.*s) NAME('%.*s') TYPE(PROFILE) DEPARTMENT(%.*s)",
             localParmList.group.length, localParmList.group.value,
             localParmList.group.length, localParmList.group.value,
             localParmList.superiorGroup.length, localParmList.superiorGroup.value);
               
    if (isDryRun) {
      localParmList.operatorCommand.length = sizeof(operatorCommand);
      memcpy(localParmList.operatorCommand.text, operatorCommand,
             sizeof(operatorCommand));
    }
    else {
      CMS_DEBUG2(globalArea, traceLevel,
                 "Attemping to add group (%.*s).\n",
                 localParmList.group.length, localParmList.group.value);
       
      radminActionRC = executeAdminCommand(&radminStatus, operatorCommand,
                                                serviceMessage, sizeof(serviceMessage),
                                                authInfo);

      if (radminActionRC != RC_RADMIN_OK) {
        CMS_DEBUG2(globalArea, traceLevel,
                   "Failed to delete group (%.*s), rsn = %s\n",
                   localParmList.group.length, localParmList.group.value,
                   serviceMessage);
      }
    }
  }
  else if (localParmList.function == ZIS_GROUP_ADMIN_SRV_FC_DELETE) {
#define TSS_GRP_ADMIN_DEL_MAX_LEN 20 /* ACID - 8, Rest of command - 12 */
    char operatorCommand[TSS_GRP_ADMIN_DEL_MAX_LEN + 1] = {0};

    snprintf(operatorCommand, sizeof(operatorCommand), "TSS DELETE(%.*s)", 
             localParmList.group.length, localParmList.group.value);
             
    if (isDryRun) {
      localParmList.operatorCommand.length = sizeof(operatorCommand);
      memcpy(localParmList.operatorCommand.text, operatorCommand,
             sizeof(operatorCommand));
    }
    else {
      CMS_DEBUG2(globalArea, traceLevel,
                 "Attemping to delete group (%.*s).\n",
                 localParmList.group.length, localParmList.group.value);
                 
      radminActionRC = executeAdminCommand(&radminStatus, operatorCommand,
                                                serviceMessage, sizeof(serviceMessage),
                                                authInfo);
                                                
      if (radminActionRC != RC_RADMIN_OK) {
        CMS_DEBUG2(globalArea, traceLevel,
                   "Failed to delete group (%.*s), rsn = %s\n",
                   localParmList.group.length, localParmList.group.value,
                   serviceMessage);
      }
    }
  }
  else if (localParmList.function == ZIS_GROUP_ADMIN_SRV_FC_CONNECT_USER) {
#define TSS_GRP_ADMIN_ADD_CON_MAX_LEN                   37 /* ACID - 8, Group - 8, Rest of command - 21 */
    char operatorCommand[TSS_GRP_ADMIN_ADD_CON_MAX_LEN + 1] = {0};
                   
    snprintf(operatorCommand, sizeof(operatorCommand), 
             "TSS ADDTO(%.*s) GROUP(%.*s)",
             localParmList.user.length, localParmList.user.value,
             localParmList.group.length, localParmList.group.value);
             
    
    if (isDryRun) {
      localParmList.operatorCommand.length = sizeof(operatorCommand);
      memcpy(localParmList.operatorCommand.text, operatorCommand,
             sizeof(operatorCommand));
    }
    else {
      CMS_DEBUG2(globalArea, traceLevel,
                 "Attemping to add connection to (%.*s).\n",
                 localParmList.group.length, localParmList.group.value);
       
      radminActionRC = executeAdminCommand(&radminStatus, operatorCommand,
                                                serviceMessage, sizeof(serviceMessage),
                                                authInfo);

      if (radminActionRC != RC_RADMIN_OK) {
        CMS_DEBUG2(globalArea, traceLevel,
                   "Failed to add connection to (%.*s), rsn = %s\n",
                   localParmList.group.length, localParmList.group.value,
                   serviceMessage);
      }
    }
  }
  else if (localParmList.function == ZIS_GROUP_ADMIN_SRV_FC_REMOVE_USER) {
#define TSS_GRP_ADMIN_REM_CON_MAX_LEN                   38 /* ACID - 8, Group - 8, Rest of command - 22 */
    char operatorCommand[TSS_GRP_ADMIN_REM_CON_MAX_LEN + 1] = {0};
                   
    snprintf(operatorCommand, sizeof(operatorCommand), 
             "TSS REMOVE(%.*s) GROUP(%.*s)",
             localParmList.user.length, localParmList.user.value,
             localParmList.group.length, localParmList.group.value);
             
    
    if (isDryRun) {
      localParmList.operatorCommand.length = sizeof(operatorCommand);
      memcpy(localParmList.operatorCommand.text, operatorCommand,
             sizeof(operatorCommand));
    }
    else {
      CMS_DEBUG2(globalArea, traceLevel,
                 "Attemping to remove connection to (%.*s).\n",
                 localParmList.group.length, localParmList.group.value);
       
      radminActionRC = executeAdminCommand(&radminStatus, operatorCommand,
                                           serviceMessage, sizeof(serviceMessage),
                                           authInfo);

      if (radminActionRC != RC_RADMIN_OK) {
        CMS_DEBUG2(globalArea, traceLevel,
                   "Failed to remove connection to (%.*s), rsn = %s\n",
                   localParmList.group.length, localParmList.group.value,
                   serviceMessage);
      }
    }
  }
  else {
    return RC_ZIS_GRPASRV_BAD_FUNCTION;
  }
  
  if (radminActionRC != RC_RADMIN_OK) {       
    localParmList.internalServiceRC = radminActionRC;
    localParmList.internalServiceRSN = radminStatus.reasonCode;
    localParmList.safStatus.safRC = radminStatus.apiStatus.safRC;
    localParmList.safStatus.racfRC = radminStatus.apiStatus.racfRC;
    localParmList.safStatus.racfRSN = radminStatus.apiStatus.racfRSN;
    
    unsigned int copySize = strlen(serviceMessage);
    if (copySize > sizeof(localParmList.serviceMessage.text)) {
      copySize = sizeof(localParmList.serviceMessage.text);
    }
    
    localParmList.serviceMessage.length = copySize;
    memcpy(localParmList.serviceMessage.text, serviceMessage, copySize);
    status = RC_ZIS_GRPASRV_INTERNAL_SERVICE_FAILED;
  }
         
  cmCopyToSecondaryWithCallerKey(clientParmAddr, &localParmList,
                                 sizeof(ZISGroupAdminServiceParmList));

  return status;
}

/* A small struct to hold the information
 * returned from getGenresProfileOwnerHandler()
 */
typedef struct TSSOwnerData_t {
  char owner[ZIS_USER_ID_MAX_LENGTH]; /* owner can also be a user id, so it has the same max length of 8 */
  TSSLogData *logData;
} TSSOwnerData;

/* A handler function that returns the owner of a particular profile (resource). */
static int getGenresProfileOwnerHandler(RadminAPIStatus status, const RadminCommandOutput *result,
                                        void *userData) {
 if (status.racfRC != 0) {
    return status.racfRC;
 }

  TSSOwnerData *parms = userData;
  TSSLogData *logData = parms->logData;

  const RadminCommandOutput *currentBlock = result;  
  
  if (currentBlock == NULL) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "No output was found in output message entry on first block...\n");
    return 0;
  }
  
  const RadminVLText *currentLine = &currentBlock->firstMessageEntry;
  
  /* Look for an obvious error before we start parsing.
   * The output will look something like:
   * 
   *  TSS{SOME NUMBERS}  {SOME MESSAGE}                      
   *  TSS0301I  TSS      FUNCTION FAILED, RETURN CODE =  {SOME NUMBER}
   */
  bool errorDetected = tssIsErrorDetected(currentLine->text, currentLine->length);
  if (errorDetected) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "Error detected!\n");
    return -1;
  }
  
  /* The output looks like:
   *
   * {OWNER} OWNS {CLASS} {PROFILE}
   *
   * I can easily get it by just reading in the first 8 characters
   * of the line.
   */
  if (currentLine->length >= sizeof(parms->owner)) { /* this should always be the case */
    memcpy(parms->owner, currentLine->text, sizeof(parms->owner));
    padWithSpaces(parms->owner, sizeof(parms->owner), 1, 1);
  }
  
  return 0;                      
}

/* A small struct to hold the information
 * returned from isUserAlreadyPermitted()
 */
typedef struct PermittedData_t {
  bool isPermitted;
  const char *className;
  const char *profileName;
  TSSLogData *logData;
} PermittedData;

/* A handler function that returns the whether or not a user is already permitted to a 
 * particular profile (resource)
 */
static int isUserAlreadyPermitted(RadminAPIStatus status, const RadminCommandOutput *result,
                                  void *userData) {
  if (status.racfRC != 0) {
    return status.racfRC;
  }
  
  PermittedData *parms = userData;
  TSSLogData *logData = parms->logData;

  const RadminCommandOutput *currentBlock = result;  
  
  if (currentBlock == NULL) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "No output was found in output message entry on first block...\n");
    return 0;
  }
  
  const RadminVLText *currentLine = &currentBlock->firstMessageEntry;
  
  /* Look for an obvious error before we start parsing.
   * The output will look something like:
   * 
   *  TSS{SOME NUMBERS}  {SOME MESSAGE}                      
   *  TSS0301I  TSS      FUNCTION FAILED, RETURN CODE =  {SOME NUMBER}
   */
  bool errorDetected = tssIsErrorDetected(currentLine->text, currentLine->length);
  if (errorDetected) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "Error detected!\n");
    return -1;
  }
  
  unsigned int offsetInBlock = 16;
  
  char searchKey[11 + 1] = {0};
  snprintf(searchKey, sizeof(searchKey), "XA %s", parms->className);
  nullTerminate(searchKey, sizeof(searchKey) - 1);

  do {
    while (offsetInBlock < currentBlock->lastByteOffset) {      
      if (tssIsOutputDone(currentLine->text, currentLine->length)) {
        return 0;
      }

      TSSKeyData keys[TSS_MAX_NUM_OF_KEYS] = {0};
      
      int numberOfKeys = tssParseLineForKeys(currentLine->text, currentLine->length, keys);
      if (numberOfKeys == -1) {
        /* In this case, print the line in question. */
        CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                   "An invalid number of keys were detected on line: '"
                   "%.*s'\n", currentLine->length, currentLine->text);
        return -1;
      }      
      else {
        char tmpBuffer[ZIS_SECURITY_PROFILE_MAX_LENGTH] = {0};
        int keyIndex = tssFindDesiredKey(searchKey, keys);
        if (keyIndex != -1) {
          int ownerIndex = lastIndexOfString((char*)currentLine->text, currentLine->length, TSS_OWNER_KEY);
          if (ownerIndex == -1) {
            return -1;
          }
          
          int status = tssFindValueForKey(currentLine->text, ownerIndex-1, keys,
                                          keyIndex, numberOfKeys, tmpBuffer, sizeof(tmpBuffer));
          if (status == -1) {
            CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                       "Could not find value of %s...\n", searchKey);
            return -1;
          }
        }      
            
        int foundProfileLength = tssGetActualValueLength(tmpBuffer, sizeof(tmpBuffer));
        int searchProfileLength = strlen(parms->profileName);
                  
        if (foundProfileLength == searchProfileLength) {
          if (!memcmp(tmpBuffer, parms->profileName, searchProfileLength)) {
            parms->isPermitted = TRUE;
            return 0;
          }
        }
      }
    
      offsetInBlock += currentLine->length + 2;
      currentLine = (RadminVLText *)(currentLine->text + currentLine->length);
    }
    
    currentBlock = currentBlock->next;
    if (currentBlock != NULL) {
      currentLine = (RadminVLText *)&(currentBlock->firstMessageEntry);
      offsetInBlock = 16;
    }
  } while (currentBlock != NULL);
 
  return 0;              
}

/* Returns a string representation of an ENUM value for access type. */
static const char *getAccessString(ZISGenresAdminServiceAccessType type) {
  switch (type) {
  case ZIS_GENRES_ADMIN_ACESS_TYPE_READ:
    return "READ";
  case ZIS_GENRES_ADMIN_ACESS_TYPE_UPDATE:
    return "UPDATE";
  case ZIS_GENRES_ADMIN_ACESS_TYPE_ALTER:
    return "ALTER";
  case ZIS_GENRES_ADMIN_ACESS_TYPE_ALL:
    return "ALL";
  default:
    return NULL;
  }
}

/* A helper function to issue the command to revoke access to a resource. */
static int handleRevokeAccess(ZISGenresAdminServiceParmList *localParmList, char *className, char *serviceMessage, int serviceMessageSize,
                              bool isDryRun, RadminStatus *radminStatus, RadminCallerAuthInfo authInfo) {                  
#define TSS_PRF_ACCESS_LIST_ADMIN_REVOKE_MAX_LEN 302 /* Class - 8, Profile - 255, Access - 7, Rest of command - 32 */
  char operatorCommand[TSS_PRF_ACCESS_LIST_ADMIN_REVOKE_MAX_LEN] = {0};
  
  snprintf(operatorCommand, sizeof(operatorCommand), "TSS REVOKE(%.*s) %s(%.*s)",
           localParmList->user.length, localParmList->user.value,
           className, localParmList->profile.length,
           localParmList->profile.value);
  
  int radminRC = RC_RADMIN_OK;
  
  if (isDryRun) {
    localParmList->operatorCommand.length = sizeof(operatorCommand);
    memcpy(localParmList->operatorCommand.text, operatorCommand,
           sizeof(operatorCommand));
  }    
  else {     
    radminRC = executeAdminCommand(radminStatus, operatorCommand,
                                   serviceMessage, serviceMessageSize,
                                   authInfo);
  }
  
  return radminRC;
}

/* A helper function to issue a command to permit access to a resource. */
static int handlePermitAccess(ZISGenresAdminServiceParmList *localParmList, char *className, char *serviceMessage, int serviceMessageSize,
                              bool isDryRun, RadminStatus *radminStatus, RadminCallerAuthInfo authInfo) {
#define TSS_PRF_ACCESS_LIST_ADMIN_PERMIT_MAX_LEN 302 /* Class - 8, Profile - 255, Access - 7, Rest of command - 32 */
  char operatorCommand[TSS_PRF_ACCESS_LIST_ADMIN_PERMIT_MAX_LEN] = {0};
  
  if (localParmList->accessType == ZIS_GENRES_ADMIN_ACESS_TYPE_ALTER) { /* Top Secret doesn't have ALTER */
    localParmList->accessType = ZIS_GENRES_ADMIN_ACESS_TYPE_ALL;
  }
  
  const char *accessString = getAccessString(localParmList->accessType);
  
  snprintf(operatorCommand, sizeof(operatorCommand), "TSS PERMIT(%.*s) %s(%.*s) ACCESS(%s)",
           localParmList->user.length, localParmList->user.value,
           className, localParmList->profile.length,
           localParmList->profile.value, accessString);
  
  int radminRC = RC_RADMIN_OK;
  
  if (isDryRun) {
    localParmList->operatorCommand.length = sizeof(operatorCommand);
    memcpy(localParmList->operatorCommand.text, operatorCommand,
           sizeof(operatorCommand));
  }    
  else {
    radminRC = executeAdminCommand(radminStatus, operatorCommand,
                                   serviceMessage, serviceMessageSize,
                                   authInfo);
  }
  
  return radminRC;
}

/* A helper function to issue a command to remove a resource */
static int handleDeleteProfile(ZISGenresAdminServiceParmList *localParmList, char *className, char *ownerName,
                              char *serviceMessage, int serviceMessageSize,
                              bool isDryRun, RadminStatus *radminStatus, RadminCallerAuthInfo authInfo) {
#define TSS_PRF_ADMIN_DEL_MAX_LEN 286 /* Acid - 8, Class - 8, Profile - 255, Rest of command - 15 */
  char operatorCommand[TSS_PRF_ADMIN_DEL_MAX_LEN] = {0};    
  
  snprintf(operatorCommand, sizeof(operatorCommand), "TSS REMOVE(%s) %s(%.*s)",
           ownerName, className, localParmList->profile.length,
           localParmList->profile.value);
  
  int radminRC = RC_RADMIN_OK;
  
  if (isDryRun) {
    localParmList->operatorCommand.length = sizeof(operatorCommand);
    memcpy(localParmList->operatorCommand.text, operatorCommand,
           sizeof(operatorCommand));
  }    
  else { 
    radminRC = executeAdminCommand(radminStatus, operatorCommand,
                                   serviceMessage, serviceMessageSize,
                                   authInfo);
  }
  
  return radminRC;
}

/* A helper function to issue a command to define a resource. */
static int handleDefineProfile(ZISGenresAdminServiceParmList *localParmList, char *className, char *serviceMessage, int serviceMessageSize,
                              bool isDryRun, RadminStatus *radminStatus, RadminCallerAuthInfo authInfo) {
#define TSS_PRF_ADMIN_ADD_MAX_LEN 285 /* Acid - 8, Class - 8, Profile - 255, Rest of command - 14 */   
  char operatorCommand[TSS_PRF_ADMIN_ADD_MAX_LEN] = {0};
  
  snprintf(operatorCommand, sizeof(operatorCommand), "TSS ADDTO(%.*s) %s(%.*s)",
           localParmList->owner.length, localParmList->owner.value,
           className, localParmList->profile.length,
           localParmList->profile.value); 
  
  int radminRC = RC_RADMIN_OK;
    
  if (isDryRun) {
    localParmList->operatorCommand.length = sizeof(operatorCommand);
    memcpy(localParmList->operatorCommand.text, operatorCommand,
           sizeof(operatorCommand));
  }    
  else {
    radminRC = executeAdminCommand(radminStatus, operatorCommand,
                                   serviceMessage, serviceMessageSize,
                                   authInfo);
  }
  
  return radminRC;
}

/* Entry point into the genres profile admin endpoint logic */             
int zisGenresProfileAdminServiceFunctionTSS(CrossMemoryServerGlobalArea *globalArea,
                                            CrossMemoryService *service, void *parm) {
  int status = RC_ZIS_GSADMNSRV_OK;
  int radminActionRC = RC_RADMIN_OK;

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_GSADMNSRV_PARMLIST_NULL;
  }

  ZISGenresAdminServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr,
                                   sizeof(localParmList));

  int parmValidateRC = validateGenresParmList(&localParmList);
  if (parmValidateRC != RC_ZIS_GSADMNSRV_OK) {
    return parmValidateRC;
  }

  int traceLevel = localParmList.traceLevel;
  if (globalArea->pcLogLevel > traceLevel) {
    traceLevel = globalArea->pcLogLevel;
  }

  RadminUserID caller;
  if (!secmgmtGetCallerUserID(&caller)) {
    return RC_ZIS_GSADMNSRV_IMPERSONATION_MISSING;
  }

  if (localParmList.owner.length == 0) {
    memcpy(localParmList.owner.value, caller.value, sizeof(caller.value));
    localParmList.owner.length = caller.length;
  }

  CMS_DEBUG2(globalArea, traceLevel,
             "GSADMNSRV: flags = 0x%X, function = %d, "
             "owner = \'%.*s\', user = \'%.*s\', access = %d'\n",
             localParmList.flags,
             localParmList.function,
             localParmList.owner.length, localParmList.owner.value,
             localParmList.user.length, localParmList.user.value,
             localParmList.accessType);
  CMS_DEBUG2(globalArea, traceLevel, "GSADMNSRV: profile = \'%.*s\'\n",
             localParmList.profile.length, localParmList.profile.value);
  CMS_DEBUG2(globalArea, traceLevel, "GSADMNSRV: caller = \'%.*s\'\n",
             caller.length, caller.value);

  RadminCallerAuthInfo authInfo = {
      .acee = NULL,
      .userID = caller
  };

  CrossMemoryServerConfigParm userClassParm = {0};
  int getParmRC = cmsGetConfigParm(&globalArea->serverName,
                                   ZIS_PARMLIB_PARM_SECMGMT_USER_CLASS,
                                   &userClassParm);
  if (getParmRC != RC_CMS_OK) {
    localParmList.internalServiceRC = getParmRC;
    return RC_ZIS_GSADMNSRV_USER_CLASS_NOT_READ;
  }
  if (userClassParm.valueLength > ZIS_SECURITY_CLASS_MAX_LENGTH) {
    return RC_ZIS_GSADMNSRV_USER_CLASS_TOO_LONG;
  }

  TSSLogData logData = {
    .globalArea = globalArea,
    .traceLevel = traceLevel
  };
  
  CMS_DEBUG2(globalArea, traceLevel, "GSADMNSRV: class = \'%.*s\'\n",
             userClassParm.valueLength, userClassParm.charValueNullTerm);
  
  bool isDryRun = (localParmList.flags & ZIS_GROUP_ADMIN_FLAG_DRYRUN) ?
                   true : false;
                   
  RadminStatus radminStatus = {0};
    
  char serviceMessage[TSS_SERVICE_MSG_MAX_LEN + 1];
  
  if (localParmList.function == ZIS_GENRES_ADMIN_SRV_FC_GIVE_ACCESS) {
#define TSS_PRF_PERMIT_EXTR_MAX_LEN 30 /* Acid - 8, Data - 11, Rest of command - 11 */
    char extractGenresProfilePermit[TSS_PRF_PERMIT_EXTR_MAX_LEN + 1] = {0};
    
    snprintf(extractGenresProfilePermit, sizeof(extractGenresProfilePermit),
        "TSS LIST(%.*s) DATA(XAUTH)", localParmList.user.length,
        localParmList.user.value);
    
    char profileNameNullTerm[ZIS_SECURITY_PROFILE_MAX_LENGTH + 1] = {0};
    memcpy(profileNameNullTerm, localParmList.profile.value,
           localParmList.profile.length);
           
    PermittedData parms = {
      .isPermitted = FALSE,
      .className = userClassParm.charValueNullTerm,
      .profileName = profileNameNullTerm,
      .logData = &logData
    };
    
    radminActionRC = radminRunRACFCommand(
        authInfo,
        extractGenresProfilePermit,
        isUserAlreadyPermitted,
        &parms,
        &radminStatus
    );
    
    if (radminActionRC == RC_RADMIN_OK) {
      CMS_DEBUG2(logData.globalArea, logData.traceLevel,
               "Attemping to permit access to %.*s for %.*s...\n",
               localParmList.profile.length, localParmList.profile.value,
               localParmList.user.length, localParmList.user.value);
               
      if (!parms.isPermitted || isDryRun) {
        radminActionRC = handlePermitAccess(&localParmList, userClassParm.charValueNullTerm, serviceMessage,
                                            sizeof(serviceMessage), isDryRun,
                                            &radminStatus, authInfo);
      }
      else {
        radminActionRC = handleRevokeAccess(&localParmList, userClassParm.charValueNullTerm, serviceMessage,
                                            sizeof(serviceMessage), isDryRun,
                                            &radminStatus, authInfo);
         
        if (radminActionRC == RC_RADMIN_OK) {
          radminActionRC = handlePermitAccess(&localParmList, userClassParm.charValueNullTerm, serviceMessage,
                                              sizeof(serviceMessage), isDryRun,
                                              &radminStatus, authInfo);
        }
      }
      
      if (radminActionRC != RC_RADMIN_OK) {
        CMS_DEBUG2(logData.globalArea, logData.traceLevel,
                   "Failed to permit access to (%.*s), rsn = %s\n",
                   localParmList.profile.length, localParmList.profile.value,
                   serviceMessage);
      }
    }
  }
  else if (localParmList.function == ZIS_GENRES_ADMIN_SRV_FC_REVOKE_ACCESS) {
    CMS_DEBUG2(logData.globalArea, logData.traceLevel,
           "Attemping to revoke access to %.*s for %.*s...\n",
           localParmList.profile.length, localParmList.profile.value,
           localParmList.user.length, localParmList.user.value);
           
    radminActionRC = handleRevokeAccess(&localParmList, userClassParm.charValueNullTerm, serviceMessage,
                                        sizeof(serviceMessage), isDryRun,
                                        &radminStatus, authInfo);
    
    if (radminActionRC != RC_RADMIN_OK) {
      CMS_DEBUG2(logData.globalArea, logData.traceLevel,
                 "Failed to revoke access to (%.*s), rsn = %s\n",
                 localParmList.profile.length, localParmList.profile.value,
                 serviceMessage);
    } 
  }
  else if (localParmList.function == ZIS_GENRES_ADMIN_SRV_FC_DEFINE) {
    CMS_DEBUG2(logData.globalArea, logData.traceLevel,
               "Attemping to define profile %.*s to %s...\n",
               localParmList.profile.length, localParmList.profile.value,
               userClassParm.charValueNullTerm);
     
    radminActionRC = handleDefineProfile(&localParmList, userClassParm.charValueNullTerm, serviceMessage,
                                         sizeof(serviceMessage), isDryRun,
                                         &radminStatus, authInfo);
                                        
    if (radminActionRC != RC_RADMIN_OK) {
      CMS_DEBUG2(logData.globalArea, logData.traceLevel,
                 "Failed to define profile %.*s to %s, rsn = %s\n",
                 localParmList.profile.length, localParmList.profile.value,
                 userClassParm.charValueNullTerm, serviceMessage);
    }  
  }
  else if (localParmList.function == ZIS_GENRES_ADMIN_SRV_FC_DELETE) {
#define TSS_PRF_OWNER_EXTR_MAX_LEN 274 /* Class - 8, Profile - 255, Rest of command - 11 */
    char extractGenresProfileOwner[TSS_PRF_OWNER_EXTR_MAX_LEN + 1] = {0};
         
    snprintf(extractGenresProfileOwner, sizeof(extractGenresProfileOwner),
            "TSS WHOO %s(%.*s)", userClassParm.charValueNullTerm,
            localParmList.profile.length, localParmList.profile.value);
            
    TSSOwnerData parms = {0};
    
    radminActionRC = radminRunRACFCommand(
        authInfo,
        extractGenresProfileOwner,
        getGenresProfileOwnerHandler,
        &parms,
        &radminStatus
    );
    
    if (radminActionRC == RC_RADMIN_OK) {    
      char ownerNameNullTerm[ZIS_USER_ID_MAX_LENGTH + 1] = {0};
      memcpy(ownerNameNullTerm, parms.owner, sizeof(parms.owner));
      nullTerminate(ownerNameNullTerm, sizeof(ownerNameNullTerm) - 1);
      
      CMS_DEBUG2(logData.globalArea, logData.traceLevel,
                "Attemping to remove profile %.*s from %s...\n",
                localParmList.profile.length, localParmList.profile.value,
                userClassParm.charValueNullTerm);
                
      radminActionRC = handleDeleteProfile(&localParmList, userClassParm.charValueNullTerm,
                                           ownerNameNullTerm,
                                           serviceMessage,
                                           sizeof(serviceMessage), isDryRun,
                                           &radminStatus, authInfo);
                                         
      if (radminActionRC != RC_RADMIN_OK) {
        CMS_DEBUG2(logData.globalArea, logData.traceLevel,
                   "Failed to remove profile %.*s from %s, rsn = %s\n",
                   localParmList.profile.length, localParmList.profile.value,
                   userClassParm.charValueNullTerm, serviceMessage);
      } 
    }
  }
  else {
    return RC_ZIS_GSADMNSRV_BAD_FUNCTION;
  }
  
  if (radminActionRC != RC_RADMIN_OK) {
    localParmList.internalServiceRC = radminActionRC;
    localParmList.internalServiceRSN = radminStatus.reasonCode;
    localParmList.safStatus.safRC = radminStatus.apiStatus.safRC;
    localParmList.safStatus.racfRC = radminStatus.apiStatus.racfRC;
    localParmList.safStatus.racfRSN = radminStatus.apiStatus.racfRSN;
    
    unsigned copySize = strlen(serviceMessage);
    if (copySize > sizeof(localParmList.serviceMessage.text)) {
      copySize = sizeof(localParmList.serviceMessage.text);
    }
    
    localParmList.serviceMessage.length = copySize; /* snprintf includes a '\0' */
    memcpy(localParmList.serviceMessage.text, serviceMessage, copySize);
    status = RC_ZIS_GSADMNSRV_INTERNAL_SERVICE_FAILED;
  }

  cmCopyToSecondaryWithCallerKey(clientParmAddr, &localParmList,
                                 sizeof(ZISGenresAdminServiceParmList));

  CMS_DEBUG2(globalArea, traceLevel,
             "GSADMNSRV: status = %d, msgLen = %zu, cmdLen = %zu, "
             "RC = %d, internal RC = %d, RSN = %d, "
             "SAF RC = %d, RACF RC = %d, RSN = %d\n",
             status,
             localParmList.serviceMessage.length,
             localParmList.operatorCommand.length,
             radminActionRC,
             localParmList.internalServiceRC,
             localParmList.internalServiceRSN,
             localParmList.safStatus.safRC,
             localParmList.safStatus.racfRC,
             localParmList.safStatus.racfRSN);

  return status;
}

/* A struct to hold information about where
 * in the output we left off.
 */
typedef struct TSSLineData_t {
  const RadminVLText *linePtr;
  const RadminCommandOutput *blockPtr;
  unsigned int offsetInBlock;
} TSSLineData;

/* A parameter list passed into the handler function
 * for the genres profile extraction service.
 */ 
typedef struct TSSGenresProfileParms_t {
  ZISGenresProfileEntry *tmpResultBuffer;
  unsigned int profilesExtracted;
  unsigned int profilesToExtract;
  const char *className;
  TSSLineData *lineData;
  int startIndex;
  bool reachedEndOfOutput;
  TSSLogData *logData;
} TSSGenresProfileParms;

/* Iterates through the tmpResultBuffer (ZISGenresProfileEntry array) to check for a duplicate
 * profile. This is a slow solution, but based on the structure of the output, there isn't really
 * a more reliable way to ensure that the same profile isn't added twice.
 */
static int isDuplicateProfile(ZISGenresProfileEntry *buffer, char *searchString, int searchStringBufferSize,
                              int stopSearch) {
  for (int i = 0; i < stopSearch; i++) {
    int searchStringLength = tssGetActualValueLength(searchString, searchStringBufferSize);
    int bufferStringLength = tssGetActualValueLength(buffer[i].profile, sizeof(buffer[i].profile));
    
    if (searchStringLength == bufferStringLength) {
      if (!memcmp(searchString, buffer[i].profile, searchStringLength)) {
        return TRUE;
      }
    }
  }
  
  return FALSE;
}

/* A helper function that calls an r_admin command to query the database
 * for a profile owner.
 */                               
static int assignCorrectProfileOwners(ZISGenresProfileEntry *buffer, unsigned int numberOfProfiles,
                                      const char *className, RadminStatus *radminStatus,
                                      RadminCallerAuthInfo authInfo) {
  for (int i = 0; i < numberOfProfiles; i++) {
    if (buffer[i].owner[0] == 0) {
#define TSS_PRF_OWNER_EXTR_MAX_LEN 274 /* Class - 8, Profile - 255, Rest of command - 11 */
      char extractGenresProfileOwner[TSS_PRF_OWNER_EXTR_MAX_LEN + 1] = {0};
           
      snprintf(extractGenresProfileOwner, sizeof(extractGenresProfileOwner),
              "TSS WHOO %s(%.*s)", className,
              sizeof(buffer[i].profile), buffer[i].profile);
              
      TSSOwnerData parms = {0};
      
      int radminRC = radminRunRACFCommand(
            authInfo,
            extractGenresProfileOwner,
            getGenresProfileOwnerHandler,
            &parms,
            radminStatus
      );
      
      if (radminRC != RC_RADMIN_OK) {
        return radminRC;
      }
      
      ZISGenresProfileEntry *p = &buffer[i];           
      memcpy(p->owner, parms.owner, sizeof(p->owner));
    }
  }
  
  return 0;
}

/* Shifts the data extracted to the left (index of choice now starts at index 0) */
static int shiftProfileBufferLeft(ZISGenresProfileEntry *buffer, unsigned int numberOfProfiles,
                                  int foundAt) {
                                    
  unsigned int copyIndex = foundAt;
  for (int i = 0; i < numberOfProfiles; i++) {
    memset(buffer[i].profile, 0, sizeof(buffer[i].profile));
    memset(buffer[i].owner, 0, sizeof(buffer[i].owner));
    
    memcpy(buffer[i].profile, buffer[copyIndex].profile, sizeof(buffer[copyIndex].profile));
    memcpy(buffer[i].owner, buffer[copyIndex].owner, sizeof(buffer[copyIndex].owner));
    
    memset(buffer[copyIndex].profile, 0, sizeof(buffer[copyIndex].profile));
    memset(buffer[copyIndex].owner, 0, sizeof(buffer[copyIndex].owner));
    
    copyIndex++;
  }
}

/* Searches the extracted profiles to check for a match. The index where the match occurs
 * is returned via foundAt.
 */
static bool findStartProfile(ZISGenresProfileEntry *buffer, unsigned int numberOfProfiles,
                             const char *searchProfile, int *foundAt) {
  int searchProfileLength = strlen(searchProfile);
  
  for (int i = 0; i < numberOfProfiles; i++) {
    int extractedProfileLength = tssGetActualValueLength(buffer[i].profile,
                                                         sizeof(buffer[i].profile));                                                   
      
    if (searchProfileLength == extractedProfileLength) {
      if (!memcmp(searchProfile, buffer[i].profile, searchProfileLength)) {
        *foundAt = i;
        return TRUE;
      }
    }
  }
  
  return FALSE;
}

/* A handler function that is called after the r_admin command is issued. It parses the profile
 * data from the output by going line by line.
 */
static int genresProfileHandler(RadminAPIStatus status, const RadminCommandOutput *result,
                                 void *userData) {
  if (status.racfRC != 0) {
    return status.racfRC;
  }

  TSSGenresProfileParms *parms = userData;
  ZISGenresProfileEntry *buffer = parms->tmpResultBuffer;
  TSSLogData *logData = parms->logData;
  ZISGenresProfileEntry entry = {0};

  const RadminCommandOutput *currentBlock = result;  
  
  if (currentBlock == NULL) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "No output was found in output message entry on first block...\n");
    return 0;
  }
  
  const RadminVLText *currentLine = &currentBlock->firstMessageEntry;
  
  /* Look for an obvious error before we start parsing.
   * The output will look something like:
   * 
   *  TSS{SOME NUMBERS}  {SOME MESSAGE}                      
   *  TSS0301I  TSS      FUNCTION FAILED, RETURN CODE =  {SOME NUMBER}
   */
  bool errorDetected = tssIsErrorDetected(currentLine->text, currentLine->length);
  if (errorDetected) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel,
               "Error detected!\n");
    return -1;
  }
  
  unsigned int offsetInBlock = 16;

  if (parms->lineData->blockPtr != NULL && parms->lineData->linePtr != NULL && parms->lineData->offsetInBlock != 0) {
    CMS_DEBUG2(logData->globalArea, logData->traceLevel, "Continuing...\n");
    currentBlock = parms->lineData->blockPtr;
    currentLine = parms->lineData->linePtr;
    offsetInBlock = parms->lineData->offsetInBlock;
  }
  
  /* Iterate through the output and store information on the profiles that
   * are found.
   */
  unsigned int numExtracted = parms->startIndex;
  do {
    char currOwner[ZIS_USER_ID_MAX_LENGTH] = {0};   
    while (offsetInBlock < currentBlock->lastByteOffset) {   
      if (tssIsOutputDone(currentLine->text, currentLine->length)) {
        parms->profilesExtracted = numExtracted;
        parms->reachedEndOfOutput = TRUE;
        
        return 0;
      }
      
      if (numExtracted == parms->profilesToExtract) {
        parms->profilesExtracted = numExtracted;
        parms->lineData->blockPtr = currentBlock;
        parms->lineData->linePtr = currentLine;
        parms->lineData->offsetInBlock = offsetInBlock;
             
        return 0;
      }

      TSSKeyData keys[TSS_MAX_NUM_OF_KEYS] = {0};
      
      int numberOfKeys = tssParseLineForKeys(currentLine->text, currentLine->length, keys);
      if (numberOfKeys == -1 || numberOfKeys == 0) {
        /* In this case, print the line in question. */
        CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                   "An invalid number of keys were detected on line: '"
                   "%.*s'\n", currentLine->length, currentLine->text);
        return -1;
      }
      else {
        int keyIndex = tssFindDesiredKey(parms->className, keys);
        if (keyIndex != -1) {          
          int ownerIndex = lastIndexOfString((char*)currentLine->text, currentLine->length, TSS_OWNER_KEY);
          if (ownerIndex == -1) {
            return -1;
          }         
          memcpy(currOwner, currentLine->text+ownerIndex+6, sizeof(currOwner));
          padWithSpaces(currOwner, sizeof(currOwner), 1, 1);
        }
        else {        
          int keyIndex = tssFindDesiredKey(TSS_XAUTH_KEY, keys);
          if (keyIndex != -1) {
            int acidIndex = lastIndexOfString((char*)currentLine->text, currentLine->length, TSS_ACID_KEY);
            if (acidIndex == -1) {
              return -1;
            }
            
            int status = tssFindValueForKey(currentLine->text, acidIndex-1, keys,
                                            keyIndex, numberOfKeys, entry.profile,
                                            sizeof(entry.profile));
            if (status == -1) {
              CMS_DEBUG2(logData->globalArea, logData->traceLevel, 
                         "Could not find value of %s...\n", TSS_XAUTH_KEY);
              return -1;
            }
            
            if (!isDuplicateProfile(buffer, entry.profile, sizeof(entry.profile), numExtracted)) {
              ZISGenresProfileEntry *p = &buffer[numExtracted++];
              memset(p, 0, sizeof(*p));
              
              memcpy(p->owner, currOwner, sizeof(currOwner)); /* They will have the same owner as the profile prefix. */
              memcpy(p->profile, entry.profile, sizeof(p->profile));
              
              memset(&entry, 0, sizeof(ZISGenresProfileEntry));                                     
            }
          }
        }         
      }
    
      offsetInBlock += currentLine->length + 2;
      currentLine = (RadminVLText *)(currentLine->text + currentLine->length);
    }
    
    currentBlock = currentBlock->next;
    if (currentBlock != NULL) {
      currentLine = (RadminVLText *)&(currentBlock->firstMessageEntry);
      offsetInBlock = 16;
    }
  } while (currentBlock != NULL);
  
  return 0;                 
}

/* Prepares to copy the ZISGenresProfileEntry struct to the other address space where
 * it can be returned to the caller.
 */
static void copyGenresProfiles(ZISGenresProfileEntry *dest,
                               const ZISGenresProfileEntry *src,
                               unsigned int count,
                               int startIndex) {
  for (unsigned int i = 0; i < count; i++) {
    cmCopyToSecondaryWithCallerKey(dest[i].profile, src[startIndex].profile,
                                   sizeof(dest[i].profile));
    cmCopyToSecondaryWithCallerKey(dest[i].owner, src[startIndex].owner,
                                   sizeof(dest[i].owner));
    startIndex++;
  }
}

/* The entry point into the genres profiles endpoint logic. Note, a general resource
 * followed by a permit to that resource in Top Secret is the equivalent of a profile
 * in RACF.
 */ 
int zisGenresProfilesServiceFunctionTSS(CrossMemoryServerGlobalArea *globalArea,
                                        CrossMemoryService *service, void *parm) {
  int status = RC_ZIS_GRPRFSRV_OK;
  int radminExtractRC = RC_RADMIN_OK;

  void *clientParmAddr = parm;
  if (clientParmAddr == NULL) {
    return RC_ZIS_GRPRFSRV_PARMLIST_NULL;
  }

  ZISGenresProfileServiceParmList localParmList;
  cmCopyFromSecondaryWithCallerKey(&localParmList, clientParmAddr,
                                   sizeof(localParmList));

  int parmValidateRC = validateGenresProfileParmList(&localParmList);
  if (parmValidateRC != RC_ZIS_GRPRFSRV_OK) {
    return parmValidateRC;
  }

  int traceLevel = localParmList.traceLevel;
  if (globalArea->pcLogLevel > traceLevel) {
    traceLevel = globalArea->pcLogLevel;
  }

  RadminUserID caller;
  if (!secmgmtGetCallerUserID(&caller)) {
    return RC_ZIS_GRPRFSRV_IMPERSONATION_MISSING;
  }

  CMS_DEBUG2(globalArea, traceLevel,
             "GRPRFSRV: profile = \'%.*s\'\n",
             localParmList.startProfile.length,
             localParmList.startProfile.value);
  CMS_DEBUG2(globalArea, traceLevel,
             "GRPRFSRV: class = \'%.*s\', profilesToExtract = %u,"
             " result buffer = 0x%p\n",
             localParmList.class.length, localParmList.class.value,
             localParmList.profilesToExtract,
             localParmList.resultBuffer);
  CMS_DEBUG2(globalArea, traceLevel, "GRPRFSRV: caller = \'%.*s\'\n",
             caller.length, caller.value);

  RadminCallerAuthInfo authInfo = {
      .acee = NULL,
      .userID = caller
  };

  char classNullTerm[ZIS_SECURITY_CLASS_MAX_LENGTH + 1] = {0};
  memcpy(classNullTerm, localParmList.class.value,
         localParmList.class.length);
  char profileNameBuffer[ZIS_SECURITY_PROFILE_MAX_LENGTH + 1] = {0};
  memcpy(profileNameBuffer, localParmList.startProfile.value,
         localParmList.startProfile.length);
  const char *startProfileNullTerm = localParmList.startProfile.length > 0 ?
                                     profileNameBuffer : NULL;                                                

  if (localParmList.class.length == 0) {
    CrossMemoryServerConfigParm userClassParm = {0};
    int getParmRC = cmsGetConfigParm(&globalArea->serverName,
                                     ZIS_PARMLIB_PARM_SECMGMT_USER_CLASS,
                                     &userClassParm);
    if (getParmRC != RC_CMS_OK) {
      localParmList.internalServiceRC = getParmRC;
      return RC_ZIS_GRPRFSRV_USER_CLASS_NOT_READ;
    }
    if (userClassParm.valueLength > ZIS_SECURITY_CLASS_MAX_LENGTH) {
      return RC_ZIS_GRPRFSRV_CLASS_TOO_LONG;
    }
    memcpy(classNullTerm, userClassParm.charValueNullTerm,
           userClassParm.valueLength);
  }

  size_t tmpResultBufferSize =
      sizeof(ZISGenresProfileEntry) * localParmList.profilesToExtract;
      
  int allocRC = 0, allocSysRC = 0, allocSysRSN = 0;
  
  ZISGenresProfileEntry *tmpResultBuffer =
      (ZISGenresProfileEntry *)safeMalloc64v3(
          tmpResultBufferSize, TRUE,
          "ZISGenresProfileEntry",
          &allocRC,
          &allocSysRSN,
          &allocSysRSN
      );
   
  CMS_DEBUG2(globalArea, traceLevel,
             "GRPRFSRV: tmpBuff allocated @ 0x%p (%zu %d %d %d)\n",
             tmpResultBuffer, tmpResultBufferSize,
             allocRC, allocSysRC, allocSysRSN);
             
  if (tmpResultBuffer == NULL) {
    return RC_ZIS_GRPRFSRV_ALLOC_FAILED;
  }
  
  RadminStatus radminStatus = {0};
    
  TSSLogData logData = {
    .globalArea = globalArea,
    .traceLevel = traceLevel
  };
  
  TSSLineData lineData = {0};
  
  TSSGenresProfileParms parms = {
    .tmpResultBuffer = tmpResultBuffer,
    .profilesToExtract = localParmList.profilesToExtract,
    .className = classNullTerm,
    .lineData = &lineData,
    .logData = &logData
  };
  
  
  bool isProfileFound = FALSE;
  int foundAt = -1;
  do {
#define TSS_EXTRACT_PROFILE_CMD_MAX_LEN 33  /* Class - 8, Data - 10, Rest of command - 15 */
    char extractGenresProfileCommand[TSS_EXTRACT_PROFILE_CMD_MAX_LEN + 1] = {0};
    snprintf(extractGenresProfileCommand, sizeof(extractGenresProfileCommand), "TSS WHOH %s(*) DATA(MASK)", classNullTerm);
         
    radminExtractRC = radminRunRACFCommand(
        authInfo,
        extractGenresProfileCommand,
        genresProfileHandler,
        &parms,
        &radminStatus
    );
    
    if (startProfileNullTerm == NULL) {
      CMS_DEBUG2(globalArea, traceLevel, "No start profile was specified...\n");
      break; /* We aren't starting from a specific profile, so just return everything. */
    }
       
    if (!isProfileFound) {
      isProfileFound = findStartProfile(tmpResultBuffer, parms.profilesExtracted, startProfileNullTerm, &foundAt);
      if (!isProfileFound) {
        CMS_DEBUG2(globalArea, traceLevel, "Profile was not found on current interation, clearing buffer...\n");
        memset(tmpResultBuffer, 0, tmpResultBufferSize); /* We found nothing of value, just clear the array. */
      }
      
      if (foundAt == 0) {
        CMS_DEBUG2(globalArea, traceLevel, "Profile was found at index #0, nothing needs to be done...\n");
        break; /* Nothing needs to be done. Just return the output. */
      }
    }
    
    if (isProfileFound) {
      CMS_DEBUG2(globalArea, traceLevel, "Profile was found at index #%d, shifting everything left...\n", foundAt);
      
      unsigned int validExtractedProfiles = parms.profilesExtracted - foundAt;
      CMS_DEBUG2(globalArea, traceLevel, "The valid number of extracted profiles is %d...\n", validExtractedProfiles);
              
      if (foundAt != 0) {        
        shiftProfileBufferLeft(tmpResultBuffer, validExtractedProfiles, foundAt);        
        foundAt = 0; /* it will now always be index 0 */
      }
      
      if (parms.reachedEndOfOutput) {
        CMS_DEBUG2(globalArea, traceLevel, "The end of the output has been reached...\n");
        parms.profilesExtracted = validExtractedProfiles;
        break; /* Just return the output. Everything has already been shifted. */
      }
      
      if (validExtractedProfiles >= localParmList.profilesToExtract) {
        CMS_DEBUG2(globalArea, traceLevel, "There are enough profiles left...\n");
        parms.profilesExtracted = validExtractedProfiles;
        break; /* Just return the output. Everything up to the requested count will be returned */
      }
      
      if (validExtractedProfiles < localParmList.profilesToExtract) {
        CMS_DEBUG2(globalArea, traceLevel, "Not enough profiles, looping again...\n");
        parms.startIndex = validExtractedProfiles; /* Start at the index after the last valid profile. */
      }
    }
    
    if (parms.reachedEndOfOutput) {
      CMS_DEBUG2(globalArea, traceLevel, "The start profile was not found...\n");
      parms.profilesExtracted = 0; /* We didn't find the profile, so we should return an error. */
      break;
    }
    
  } while (1);     
  
  CMS_DEBUG2(globalArea, traceLevel,
             "Extracted %d genres profiles\n",
             parms.profilesExtracted);
  
  /* Return some sort of error to prevent
   * having an empty array. Note, even if having no profiles was correct,
   * returning an empty array is not.
   */ 
  if (parms.profilesExtracted == 0 && radminExtractRC == RC_RADMIN_OK) {
    CMS_DEBUG2(globalArea, traceLevel, "No genres profiles could be found...\n");
    radminExtractRC = RC_RADMIN_NONZERO_USER_RC;
  }
  
  if (radminExtractRC == RC_RADMIN_OK) {
    if (startProfileNullTerm) {
      CMS_DEBUG2(globalArea, traceLevel, "Assigning correct profile owners...\n");   
      radminExtractRC = assignCorrectProfileOwners(tmpResultBuffer, parms.profilesExtracted, classNullTerm,
                                                   &radminStatus, authInfo);
    }
  }
  
  if (radminExtractRC == RC_RADMIN_OK) {
    copyGenresProfiles(localParmList.resultBuffer, tmpResultBuffer, 
                       parms.profilesExtracted, 0);
    localParmList.profilesExtracted = parms.profilesExtracted;
  } else  {
    localParmList.internalServiceRC = radminExtractRC;
    localParmList.internalServiceRSN = radminStatus.reasonCode;
    localParmList.safStatus.safRC = radminStatus.apiStatus.safRC;
    localParmList.safStatus.racfRC = radminStatus.apiStatus.racfRC;
    localParmList.safStatus.racfRSN = radminStatus.apiStatus.racfRSN;
    status = RC_ZIS_GRPRFSRV_INTERNAL_SERVICE_FAILED;
  }

  int freeRC = 0, freeSysRC = 0, freeSysRSN = 0;
  
  freeRC = safeFree64v3(tmpResultBuffer, tmpResultBufferSize,
                        &freeSysRC, &freeSysRSN);
                        
  CMS_DEBUG2(globalArea, traceLevel, "GRPRFSRV: tmpBuff free'd @ 0x%p (%zu %d %d %d)\n",
            tmpResultBuffer, tmpResultBufferSize,
            freeRC, freeSysRC, freeSysRSN);

  tmpResultBuffer = NULL;
  
  cmCopyToSecondaryWithCallerKey(clientParmAddr, &localParmList,
                                 sizeof(ZISGenresProfileServiceParmList));

  CMS_DEBUG2(globalArea, traceLevel,
             "GRPRFSRV: status = %d, extracted = %d, RC = %d, "
             "internal RC = %d, RSN = %d, "
              "SAF RC = %d, RACF RC = %d, RSN = %d\n",
             status,
             localParmList.profilesExtracted, radminExtractRC,
             localParmList.internalServiceRC,
             localParmList.internalServiceRSN,
             localParmList.safStatus.safRC,
             localParmList.safStatus.racfRC,
             localParmList.safStatus.racfRSN);

  return status;                                        
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
