

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
#include "json.h"

extern char **environ;

static int startsWith(const char *pre, const char *str) {
  size_t lenpre = strlen(pre),
          lenstr = strlen(str);

  if(lenstr < lenpre) {
    return 1;
  }
  
  if(memcmp(pre, str, lenpre) == 0) {
    return 0;
  } else {
    return 1;
  }
}

static void splitEnvKeyValue(char buf[], char* array[]) {
  array[0] = strtok (buf, "=");
  array[1] = strtok (NULL, "=");
}

static char* returnJSONRow(const char* key, const char* val) {
  size_t len=sizeof(char)*(strlen(key)+strlen(val)+6);
  char *row = malloc(len);
  if(isNumber(val)==0) {
    snprintf(row, len, "\"%s\":%s", key, val);
  } else {
    snprintf(row, len, "\"%s\":\"%s\"", key, val);
  }
  return row;
}

static int isNumber(char s[]) 
{ 
  int i;
  for (i = 0; i < strlen(s); i++) 
      if (!isdigit(s[i])) 
          return 1; 

  return 0; 
}

static JsonObject *returnJsonObj(ShortLivedHeap *slh, char buf[]) {
  char errorBuffer[512] = { 0 };
  Json *envSettings = jsonParseString(slh, buf, errorBuffer, sizeof(errorBuffer));
  JsonObject *envSettingsJsonObject = NULL;
  if (jsonIsObject(envSettings)) {
    envSettingsJsonObject = jsonAsObject(envSettings);
  }

  return envSettingsJsonObject;
}

static JsonObject *envVarsToObject(const char *prefix) {
  int i=0;
  int j=0;
  char *foo;
  char *array[2];
  char *envJsonStr=malloc(sizeof(char)*1024);
  ShortLivedHeap *slh = makeShortLivedHeap(0x40000, 0x40);
  JsonObject *envSettings;

  strcpy(envJsonStr,"{ ");
  if(prefix == NULL) {
    prefix="";
  }

  while(environ[i] != NULL)
  {
    if(startsWith(prefix, environ[i])==0) {
      j++;
      splitEnvKeyValue(environ[i], array);
      foo = returnJSONRow(array[0], array[1]);
      if(j>1) {
        strcat(envJsonStr," , ");
      }
      strcat(envJsonStr, foo);
      free(foo);
    }
    i++;
  }

  strcat(envJsonStr," }");
  /*printf("%s\n",envJsonStr);*/

  envSettings = returnJsonObj(slh, envJsonStr);
  return envSettings;
}

JsonObject *readEnvSettings(const char *prefix) {
    return envVarsToObject(prefix);
}

/*
int main(int argc, char *argv[])
{
    JsonObject *envSettings=environmentVarsToObject("ZWED");
    int port = jsonObjectGetNumber(envSettings, "ZWED_PORT");
    printf("abc %d\n", port);
    return 0;
}
*/


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/