

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
        foo = returnJSONRow(array[0], array[1]);
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
