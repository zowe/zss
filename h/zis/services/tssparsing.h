

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef TSS_PARSING_H_
#define TSS_PARSING_H_

#define TSS_KEY_DELIMITER " = "
#define TSS_KEY_DELIMITER_LEN 3

#define TSS_ADMIN_INDICATOR "-SC"
#define TSS_ADMIN_INDICATOR_LEN 3

#define TSS_MAX_KEY_LENGTH 10
#define TSS_MAX_NUM_OF_KEYS 2

#define TSS_FUNCTION_END "TSS"
#define TSS_MESSAGE_ID_MAX_LEN 8
#define TSS_SERVICE_MSG_MAX_LEN 17
#define TSS_COMMAND_SUCCESS "TSS0300I"
#define TSS_COMMAND_FAILURE "TSS0301I"

#define TSS_MAX_VALUE_LEN 255

/* Search Keys */
#define TSS_ACCESSOR_ID_KEY             "ACCESSORID"
#define TSS_DEFAULT_GROUP_KEY           "DFLTGRP"
#define TSS_NAME_KEY                    "NAME"
#define TSS_DEPARTMENT_KEY              "DEPT ACID"
#define TSS_OWNER_KEY                   "OWNER("
#define TSS_XAUTH_KEY                   "XAUTH"
#define TSS_ACID_KEY                    "ACID("
#define TSS_ACCESS_KEY                  "ACCESS"
#define TSS_ACIDS_KEY                   "ACIDS"

/* Stores information about a specific
 * key.
 */
typedef struct TSSKeyData_t {
  unsigned int startPos;
  unsigned int endPos;
  unsigned int delimPos;
  char keyName[TSS_MAX_KEY_LENGTH];
  int keyLength;
} TSSKeyData;

bool tssIsLineBlank(const char *str, unsigned int len);

bool tssIsErrorDetected(const char *str, unsigned int len);

bool tssIsOutputDone(const char *str, unsigned int len);

int tssGetActualValueLength(const char *str, unsigned int len);
  
int tssParseLineForKeys(const char *str, unsigned int len, TSSKeyData *keys);

int tssFindValueForKey(const char *str, unsigned int len, TSSKeyData *keys, int keyIndex,
                    int numberOfKeys, char *valueBuffer, unsigned int valueBufferSize);
                    
int tssFindDesiredKey(const char *searchStr, TSSKeyData *keys);

#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
