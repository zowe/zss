

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alloc.h"
#include "json.h"

#define RC_CHECK_VALID_HEX_FAILED 100

extern char **environ;

static bool isNumber(const char s[]) 
{ 
  int i;
  for (i = 0; i < strlen(s); i++) 
      if (!isdigit(s[i])) 
          return false; 

  return true; 
}

static bool startsWith(const char *pre, const char *str) {
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : memcmp(pre, str, lenpre) == 0;
}

static char* splitEnvKeyValue(char buf[], char* array[]) {
  char *bufCpy;
  if(buf != NULL){
    int bufferLen = strlen(buf);
    bufCpy = (char*) safeMalloc(bufferLen + 1, "buffer copy");
    memcpy(bufCpy, buf, bufferLen);
    array[0] = strtok (bufCpy, "=");
    array[1] = strtok (NULL, "=");
  }
  return bufCpy;
}

static char* returnJSONRow(const char* key, const char* val) {
  size_t len=sizeof(char)*(strlen(key)+strlen(val)+6);
  char *row = malloc(len);
  if(isNumber(val) || (val[0] == '[') || strcmp(val,"true")==0 || strcmp(val,"false")==0) {
    snprintf(row, len, "\"%s\":%s", key, val);
  } 
  else if((val[0] == '{')) {
    free(row);
    return NULL;
  }
  else {
    snprintf(row, len, "\"%s\":\"%s\"", key, val);
  }
  return row;
}

static JsonObject *returnJsonObj(ShortLivedHeap *slh, char buf[]) {
  char errorBuffer[512] = { 0 };
  Json *envSettings = jsonParseString(slh, buf, errorBuffer, sizeof(errorBuffer));
  JsonObject *envSettingsJsonObject = NULL;
  if (envSettings != NULL && jsonIsObject(envSettings)) {
    envSettingsJsonObject = jsonAsObject(envSettings);
  }

  return envSettingsJsonObject;
}

static char checkValidHex(char h)
{
  if (h >= '0' && h <= '9') {
    return h - '0';
  } else if (h >= 'a' && h <= 'f') {
    return h - 'a' + 10;
  } else if (h >= 'A' && h <= 'F') {
    return h - 'A' + 10;
  } else {
    return RC_CHECK_VALID_HEX_FAILED;
  }
}

static char convertHexToChar(char h1, char h2)
{
  char decodedChar = 0;
  char hexCheck1 = checkValidHex(h1);
  char hexCheck2 = checkValidHex(h2);
  if (hexCheck1!=RC_CHECK_VALID_HEX_FAILED && hexCheck2!=RC_CHECK_VALID_HEX_FAILED) {
    decodedChar = (hexCheck1 << 4) | hexCheck2;
    a2e(&decodedChar,1);
  }
  return decodedChar;
}

#define MAX_ENCODED_UNDER_SCORE_COUNT 4
static void decodeEnvKey(const char *encodedKey, const size_t keyLen, char *decodedKey)
{
  int count = 0, j = 0, skipPrint = 0;
  char decodedChar = 0;  

  for (int i=0; encodedKey[i]; i++) {
    if (encodedKey[i] == '_') {
      if (i+3 < keyLen && encodedKey[i+1] == 'x') { 
        decodedChar = convertHexToChar(encodedKey[i+2],encodedKey[i+3]); 
        if(decodedChar) {
          i = i+3;
        } 
      }

      if(!decodedChar && count < MAX_ENCODED_UNDER_SCORE_COUNT) {
        count++;
        if(i+1 < keyLen) {
          continue;
        } else { 
          skipPrint = 1; //last char in key, print count tracked char and skip last char as it is already added to count
        }   
      } 
    }

    if(count > 0) {
      switch (count) {
        case 1:
        case 2:
          decodedKey[j++] = '_';
          break;
        case 3:
          decodedKey[j++] = '-';
          break;
        case 4:
          decodedKey[j++] = '.';
          break;
      }
    }

    if(decodedChar == 0) {
      if (!skipPrint) {
        if (encodedKey[i] == '_' && count == MAX_ENCODED_UNDER_SCORE_COUNT) {
          i--;
        } else { 
          decodedKey[j++] = encodedKey[i];
        }
      }
    } else {
        decodedKey[j++] = decodedChar;
    }

    count=0; decodedChar=0; skipPrint=0;
  }
  decodedKey[j] = '\0';  
}

static JsonObject *envVarsToObject(const char *prefix) {
  int i=0;
  int j=0;
  char *foo;
  char *array[2];
  char envJsonStr[4096];
  ShortLivedHeap *slh = makeShortLivedHeap(0x40000, 0x40);
  JsonObject *envSettings;
  int pos = 0;
  
  pos += snprintf(envJsonStr + pos, sizeof(envJsonStr) - pos, "{ ");
  if(prefix == NULL) {
    prefix="";
  }

  while(environ[i] != NULL)
  {
    if(startsWith(prefix, environ[i])) {
      char* buffer = splitEnvKeyValue(environ[i], array);
      if(array[1]!=NULL) {
        j++;
        size_t envKeyLen = strlen(array[0]);
        char envKey[envKeyLen+1];
        memset(envKey, '\0', envKeyLen+1);
        decodeEnvKey(array[0], envKeyLen, envKey);
        foo = returnJSONRow(envKey, array[1]);
        if(foo!=NULL) {
          if(j>1) {
            pos += snprintf(envJsonStr + pos, sizeof(envJsonStr) - pos, ", ");
          }
          pos += snprintf(envJsonStr + pos, sizeof(envJsonStr) - pos, "%s", foo);
          free(foo);
        }
      }
      free(buffer);
    }
    i++;
  }

  pos += snprintf(envJsonStr + pos, sizeof(envJsonStr) - pos, " }");

  envSettings = returnJsonObj(slh, envJsonStr);
  return envSettings;
}

JsonObject *readEnvSettings(const char *prefix) {
    return envVarsToObject(prefix);
}


/*
  // Will move these to test
  // valid json key-value from shell environment
  export ZWED_AGENT_PORT=1234
  export ZWED_PLUGIN_PATH=/u/user
  export ZWED_BOOLEAN_TRUE_TEST=true
  export ZWED_BOOLEAN_FALSE_TEST=false
  export ZWED_ARRAY_TEST=[30,20]
  export ZWED_ARRAY_TEST2=[true,false]

// NOT SUPPORRTED: they both loose quotes
  export ZWED_ARRAY_TEST=["a","b"] // not supported array of strings
  export ZWED_JSON_TEST={"a":2,"c":"ddd"} //not supported json values
*/

/*int main(int argc, char *argv[])
{
    JsonObject *envSettings = envVarsToObject("ZWED");
    JsonObject *envSettingsDup = envVarsToObject("ZWED");
    JsonProperty *currentPropOrig = jsonObjectGetFirstProperty(envSettings);
    JsonProperty *currentPropDup = jsonObjectGetFirstProperty(envSettingsDup);
    //this loop tests whether the length of each JsonObject is the same
    while (currentPropOrig != NULL) {
      if (currentPropDup == NULL) {
        printf("EXTERN CHAR **ENVRION TEST FAILED\n");
        break;
      }
      if ((jsonObjectGetNextProperty(currentPropOrig) == NULL
            && jsonObjectGetNextProperty(currentPropDup) != NULL)
            || (jsonObjectGetNextProperty(currentPropOrig) != NULL
            && jsonObjectGetNextProperty(currentPropDup) == NULL)) {
        printf("EXTERN CHAR **ENVRION TEST FAILED\n");
        break;
      }
      currentPropDup = jsonObjectGetNextProperty(currentPropDup);
      currentPropOrig = jsonObjectGetNextProperty(currentPropOrig);
    }
    printf("Port: %d\n", jsonObjectGetNumber(envSettings, "ZWED_AGENT_PORT"));
    printf("Plugin Dir: %s\n", jsonObjectGetString(envSettings, "ZWED_PLUGIN_PATH"));
    printf("TRUE TEST: %s\n", jsonObjectGetBoolean(envSettings, "ZWED_BOOLEAN_TRUE_TEST")? "true":"false");
    printf("FALSE TEST: %s\n", jsonObjectGetBoolean(envSettings, "ZWED_BOOLEAN_FALSE_TEST")? "true":"false");

    
    //  printf("JSON TEST: %s\n", jsonObjectGetObject(envSettings, "ZWED_JSON_TEST"));
    
    JsonArray *zwed_array_test = jsonObjectGetArray(envSettings, "ZWED_ARRAY_TEST");
    if (zwed_array_test && jsonArrayGetCount(zwed_array_test) > 0) {
      for (int i = 0; i < jsonArrayGetCount(zwed_array_test); i++) {
        Json *item = jsonArrayGetItem(zwed_array_test, i);
        if (jsonIsNumber(item)) {
          printf("Num Arr[%d]: %d\n",i, jsonAsNumber(item));
        }
      }
    }

    JsonArray *zwed_array_test2 = jsonObjectGetArray(envSettings, "ZWED_ARRAY_TEST2");
    if (zwed_array_test2 && jsonArrayGetCount(zwed_array_test2) > 0) {
      for (int i = 0; i < jsonArrayGetCount(zwed_array_test2); i++) {
        Json *item = jsonArrayGetItem(zwed_array_test2, i);
        if (jsonIsBoolean(item)) {
          printf("Bool Arr[%d]: %s\n",i, jsonAsBoolean(item)? "true":"false");
        }
      }
    }
    return 0;
}*/


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/
