

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
#include "utils.h"

#include "zis/services/tssparsing.h"

/* If an error has occured, it will be the first thing printed
 * in the output. We should use this information to prescreen for
 * any errors before any more parsing happens.
 */
bool tssIsErrorDetected(const char *str, unsigned int len) {
  if (len < TSS_MESSAGE_ID_MAX_LEN) {
    return FALSE;
  }
  
  if (!memcmp(str, TSS_FUNCTION_END, strlen(TSS_FUNCTION_END))) {
    if (memcmp(str, TSS_COMMAND_SUCCESS, strlen(TSS_COMMAND_SUCCESS))) {
      return TRUE;
    }
  }
 
  return FALSE;
}

/* If a line is reached that starts with the prefix "TSS",
 * then we have reached the end of the output. Followed by
 * "TSS" is the reason and return codes.
 */
bool tssIsOutputDone(const char *str, unsigned int len) {
  if (len < strlen(TSS_FUNCTION_END)) {
    return FALSE;
  }
  
  if (!memcmp(str, TSS_FUNCTION_END, strlen(TSS_FUNCTION_END))) {
    return TRUE;
  }
  
  return FALSE;
}

/* Searches the keys found for a match. This determines whether
 * the key you are looking for is present on the current line of
 * output.
 */
int tssFindDesiredKey(const char *searchStr, TSSKeyData *keys) {
  int searchLen = strlen(searchStr);
  
  for (int i = 0; i < 2; i++) {
    if (keys[i].keyLength == searchLen) {
      if (!memcmp(keys[i].keyName, searchStr, searchLen)) {
        return i;
      }
    }
  }
 
  return -1;
}

/* A helper function that fills in the names of the keys
 * based on the determined offsets on the output.
 */
static int tssFindNameForKeys(const char *str, TSSKeyData *keys) {
  for (int i = 0; i < 2; i++) {
    memcpy(keys[i].keyName, str+keys[i].startPos, keys[i].keyLength);
  }
}

/* All of the values found are space padded to be consistent
 * with the rest of the API. This function is used to find the
 * length without the padding.
 */
int tssGetActualValueLength(const char *str, unsigned int len) {
  for (int i = len - 1; i >= 0; i--) {
    if (str[i] != ' ' && str[i] != '\0') {
      return i + 1;
    }
  }
  
  return -1;
}

/* In the output, the length of the line is not zero even
 * if there is no relevant data. You must check for a line
 * full of spaces.
 */
bool tssIsLineBlank(const char *str, unsigned int len) {
  for (int i = 0; i < len; i++) {
    if (str[i] != ' ') {
      return FALSE;
    }
  }
  return TRUE;
}

/* Calls a series of helper functions that parse the
 * current line of output and fill in the keys in the
 * TSSKeyData struct array.
 */
int tssParseLineForKeys(const char *str, unsigned int len, TSSKeyData *keys) {  
  if (tssIsLineBlank(str, len)) {
    return 0;
  }
  
  int numberOfKeys = tssFindNumberOfKeys(str, len, 0);
  if (numberOfKeys > 2 || numberOfKeys == 0) {
    return -1;
  }
  
  int status = tssFindAllPossibleKeys(str, len, keys, numberOfKeys);
  if (status == -1) {
    return -1;
  }
  
  tssFindNameForKeys(str, keys);
  
  return numberOfKeys;
}

/* A helper function that parses the current line of output to see if there are
 * any keys based on our criteria.
 */
static int tssFindNumberOfKeys(const char *str, int unsigned len, int startPos) {
  int searchLen = strlen(TSS_KEY_DELIMITER);
  int lastPossibleStart = len-searchLen;
  int pos = startPos;
  int keyCount = 0;

  if (startPos > lastPossibleStart){
    return 0;
  }
  
  while (pos <= lastPossibleStart){
    if (!memcmp(str+pos,TSS_KEY_DELIMITER,searchLen)){
      keyCount++;
    }
    pos++;
  }
  
  return keyCount;
}

/* A helper function that finds the start of the key by going
 * character by character from the delimiter. If we find two spaces in
 * a row, we denote that as the start of the key.
 */
static int tssFindStartOfKeyReverse(const char *str, unsigned int len, int startPos) {
  int currPos = startPos;
  while (currPos > 1) {
    if (str[currPos-1] == ' ' && str[currPos-2] == ' ') {
      return currPos;
    }
    currPos--;
  }
  
  return -1;
}

/* A helper function that finds the end of the key by looking
 * for the first non space from the delimiter.
 */
static int tssFindEndOfKeyReverse(const char *str, unsigned int len, int startPos) {
  int currPos = startPos;
  while (currPos >= 0) {
    if (str[currPos] != ' ') {
      return currPos;
    }
    currPos--;
  }
  
  return -1;
}

/* A helper function that returns the other index of the array
 * where the key is stored.
 */
static int tssGetOtherKeyIndex(int keyIndex) {
  if (keyIndex == 0) {
    return 1;
  }
  else {
    return 0;
  }
}

/* A helper function that finds the value of a key if there is only one key
 * on the current line of output. It copies the value into a buffer and pads
 * it with spaces.
 */
static int tssFindValueSingleKey(const char *str, unsigned int len, TSSKeyData *keys, int keyIndex,
                                 char *valueBuffer, unsigned int valueBufferSize) {
  int valStartPos = keys[keyIndex].delimPos + TSS_KEY_DELIMITER_LEN;
  int valEndPos = -1;

  if (valStartPos + valueBufferSize >= len - 1) {
    valEndPos = len - 1;
  }
  else {
    valEndPos = valStartPos + valueBufferSize - 1;
  }
  
  int valLen = valEndPos - valStartPos + 1;
  if (valLen > valueBufferSize) {
    return -1;
  }
  
  memcpy(valueBuffer, str+valStartPos, valLen);
  
  padWithSpaces(valueBuffer, valueBufferSize, 1, 1);
  
  return 0;
}

/* A helper function that finds the value of a key if there are two keys
 * on the current line of output. It copies the value into a buffer and pads
 * it with spaces. This function is needed in order to not go too far and
 * into the other key value pair.
 */
static int tssFindValueDoubleKey(const char *str, unsigned int len, TSSKeyData *keys, int keyIndex,
                                 char *valueBuffer, unsigned int valueBufferSize) {
  int otherKeyIndex = -1;
  otherKeyIndex = tssGetOtherKeyIndex(keyIndex);
  
  int valStartPos = keys[keyIndex].delimPos + TSS_KEY_DELIMITER_LEN;
  int valEndPos = -1;
  
  if (valStartPos + valueBufferSize >= len - 1) {
    valEndPos = len - 1;
  }
  else {
    valEndPos = valStartPos + valueBufferSize - 1;
  }
  
  if (valEndPos >= keys[otherKeyIndex].startPos && valEndPos <= keys[otherKeyIndex].endPos) {
    valEndPos = keys[otherKeyIndex].startPos - 3;
  }
  
  int valLen = valEndPos - valStartPos + 1;
  if (valLen > valueBufferSize) {
    return -1;
  }
  
  memcpy(valueBuffer, str+valStartPos, valLen);
  
  padWithSpaces(valueBuffer, valueBufferSize, 1, 1);
  
  return 0;
}

/* Calls a series of helper functions that find the value of a key
 * and return it in a buffer to the caller.
 */ 
int tssFindValueForKey(const char *str, unsigned int len, TSSKeyData *keys, int keyIndex,
                       int numberOfKeys, char *valueBuffer, unsigned int valueBufferSize) {
  int status = -1;
  switch(numberOfKeys) {
    case 1: status = tssFindValueSingleKey(str, len, keys, keyIndex, valueBuffer,
                                        valueBufferSize);
            return status;
    case 2: status = tssFindValueDoubleKey(str, len, keys, keyIndex, valueBuffer,
                                        valueBufferSize);
            return status;
    default: return status;
  }                   
}

/* A helper function that finds the offsets of the key and fills
 * it into the TSSKeyData struct. This function deals with one key
 * on the line of output.
 */
static int tssFindSingleKey(const char *str, unsigned int len, TSSKeyData *keys) {
  if (str[0] != ' ') {
    keys[0].startPos = 0;
  }
  else {
    for (int i = 0; i < len - 1; i++) {
      if (str[i] != ' ') {
        keys[0].startPos = i;
        break;
      }
    }
    
    if (keys[0].startPos == -1) {
      return -1;
    }
  }
  
  int firstKeyDelimPos = indexOfString((char *)str, len, TSS_KEY_DELIMITER, 0);
  if (firstKeyDelimPos == -1) {
    return -1;
  }
  
  keys[0].delimPos = firstKeyDelimPos;
  
  int firstKeyEndPos = tssFindEndOfKeyReverse(str, len, firstKeyDelimPos);
  if (firstKeyEndPos == -1) {
    return -1;
  }
  
  keys[0].endPos = firstKeyEndPos;
  
  int keyLen = keys[0].endPos - keys[0].startPos + 1;
  if (keyLen > TSS_MAX_KEY_LENGTH) {
    return -1;
  }
  
  keys[0].keyLength = keyLen;
  
  return 0;
}

/* A helper function that finds the offsets of the key and fills
 * it into the TSSKeyData struct. This function deals with two keys
 * on the same line of output.
 */
static int tssFindDoubleKey(const char *str, unsigned int len, TSSKeyData *keys) {
  int status = tssFindSingleKey(str, len, keys);
  if (status == -1) {
    return -1;
  }
  
  int secondKeyDelimPos = len - strlen(TSS_KEY_DELIMITER);
  secondKeyDelimPos = lastIndexOfString((char *)str, len, TSS_KEY_DELIMITER);
  if (secondKeyDelimPos == -1) {
    return -1;
  }
  
  keys[1].delimPos = secondKeyDelimPos;
    
  int secondKeyEndPos = tssFindEndOfKeyReverse(str, len, secondKeyDelimPos);
  if (secondKeyEndPos == -1) {
    return -1;
  }
  
  keys[1].endPos = secondKeyEndPos;
    
  int secondKeyStartPos = tssFindStartOfKeyReverse(str, len, secondKeyEndPos);
  if (secondKeyStartPos == -1) {
    return -1;
  }
  
  keys[1].startPos = secondKeyStartPos;
  
  int keyLen = keys[1].endPos - keys[1].startPos + 1;
  if (keyLen > TSS_MAX_KEY_LENGTH) {
    return -1;
  }
  
  keys[1].keyLength = keyLen;

  return 0;
}

/* A helper function that finds the offsets of the key based on
 * the number of keys found in the current line of output.
 */
static int tssFindAllPossibleKeys(const char *str, unsigned int len, TSSKeyData *keys, int numberOfKeys) {
  int status = -1;
  switch(numberOfKeys) {
    case 1: status = tssFindSingleKey(str, len, keys);
            return status;
    case 2: status = tssFindDoubleKey(str, len, keys);
            return status;
    default: return status;
  }
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
