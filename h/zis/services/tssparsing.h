

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef TSS_PARSING_H_
#define TSS_PARSING_H_

#define KEY_DELIMITER " = "
#define KEY_DELIMITER_LEN 3

#define MAX_KEY_LENGTH 10
#define MAX_NUM_OF_KEYS 2

#define FUNCTION_END "TSS"
#define TSS_MESSAGE_ID_MAX_LEN 8
#define TSS_COMMAND_SUCCESS "TSS0300I"
#define TSS_COMMAND_FAILURE "TSS0301I"

#define MAX_VALUE_LEN 255

/* Search Keys & Max Value Lengths */
#define ACCESSOR_ID_KEY             "ACCESSORID"
#define ACCESSOR_ID_VALUE_MAX_LEN   8

#define DEFAULT_GROUP_KEY           "DFLTGRP"
#define DEFAULT_GROUP_VALUE_MAX_LEN 8

#define NAME_KEY                    "NAME"
#define NAME_VALUE_MAX_LEN          32

/* Stores information about a specific
 * key.
 */
typedef struct KeyData_t {
  unsigned int startPos;
  unsigned int endPos;
  unsigned int delimPos;
  char keyName[MAX_KEY_LENGTH];
  int keyLength;
} KeyData;

bool tssIsErrorDetected(const char *str, int len);

bool tssIsOutputDone(const char *str, int len);

int tssGetActualValueLength(const char *str, int len);
  
int tssParseLineForKeys(const char *str, int len, KeyData *keys);

int tssFindValueForKey(const char *str, int len, KeyData *keys, int keyIndex,
                    int numberOfKeys, char *valueBuffer, int valueBufferSize);
                    
int tssFindDesiredKey(const char *searchStr, KeyData *keys);

#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
