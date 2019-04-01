

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/


#ifdef METTLE 
#include <metal/metal.h>
#include <metal/stddef.h>
#include <metal/stdio.h>
#include <metal/stdlib.h>
#include <metal/string.h>
#include <metal/stdarg.h>  
#include "metalio.h"
#include "qsam.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>  
#include <pthread.h>
#endif

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "collections.h"
#include "bpxnet.h"
#include "socketmgmt.h"
#include "zos.h"
#include "le.h"
#include "json.h"
#include "httpserver.h"
#include "dataservice.h"
#include "vtam.h"
#include "discovery.h"
#include "serviceUtils.h"
#include "crossmemory.h"

#define NA_PROPRIETARY_INFO "N/A - proprietary information"

typedef struct DiscoveryServiceState_tag{
  ZOSModel *model;
  hashtable *emitterTable;
} DiscoveryServiceState;

static hashtable *getEmitterTable(DataService *service){
  return ((DiscoveryServiceState*)service->extension)->emitterTable;
}

static TableEmitter *getTableMetadataEmitter(DataService *service, char *globalTableIdentifier){
  hashtable *tableEmitters = ((DiscoveryServiceState*)service->extension)->emitterTable;

  return (TableEmitter*)htGet(tableEmitters,globalTableIdentifier);
}

#define STDOUT_FILENO 1

static jsonPrinter *startResponse(HttpResponse *response){
  if (response->standaloneTestMode){
    jsonPrinter *p = makeJsonPrinter(STDOUT_FILENO);
    jsonEnablePrettyPrint(p);
    return p;
  } else{
    jsonPrinter *p = respondWithJsonPrinter(response);
    setResponseStatus(response,200,"OK");
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);
    return p;
  }
}

static void addSubsystems(jsonPrinter *p, char *key, SoftwareInstance *subsystemList){
  jsonStartArray(p,key);
  while (subsystemList){
    SoftwareInstance *info = subsystemList;
    jsonStartObject(p,NULL);
    jsonAddString(p,"t","subystem");
    jsonAddString(p,"subsystemType",key);
    jsonAddString(p,"name",info->bestName);
    jsonEndObject(p);
    subsystemList = subsystemList->next;
  }
  jsonEndArray(p);
}

#define JSON_STRING_SIZE 260

static int getSignificantLength(char *s, int len){
  int start = len;
  int i;
  if (len > JSON_STRING_SIZE-1){
    start = JSON_STRING_SIZE-1;
  }
  /* printf("value below, len=%d, start=%d\n",len,start);
     dumpbuffer(value,16); */
  for (i=start; i>0; i--){
    if (s[i-1] > 0x41){
      break;
    }
  }
  return i;
}


static void jsonStringOut(jsonPrinter *p, char *key, char *value, int len){
  char buffer[JSON_STRING_SIZE];
  int significantLength = getSignificantLength(value,len);
  memcpy(buffer,value,significantLength);
  buffer[significantLength] = 0;
  jsonAddString(p,key,buffer);
}

static void serveIMSData(ZOSModel         *model,
                         jsonPrinter      *p,
                         char             *subsystemBestName,
                         char             *tableName){
  /* name, type, description, columnWidth
     use scaleFactor, left/right justified, and pinnedLeft for the first column */
  /* addStringColumnInfo(p,"name","C","IMS SUBSYSTEM NAME",8,1); */
  if (!strcmp(tableName,"database")){
    startTypeInfo(p, "imsDatabases", "IMS Databases");
    addStringColumnInfo(p, "subsystemName", "Subsystem", "Subsystem Name", 1, 8);
    addStringColumnInfo(p, "databaseName", "Database", "Database Name", 1, 8);
    addStringColumnInfo(p, "databaseType", "Database Type", "Database Type", -1, 10);
    addStringColumnInfo(p, "status", "Status", "Status", -1, 10);
    endTypeInfo(p);
  } else if (!strcmp(tableName,"program")){
    startTypeInfo(p, "imsPrograms", "IMS Programs");
    addStringColumnInfo(p, "subsystemName", "Subsystem", "Subsystem Name", 1, 8);
    addStringColumnInfo(p, "programName", "Program", "Program Name", 1, 8);
    endTypeInfo(p);
  } else {
    startTypeInfo(p,"cicsUnknown","CICS Unknown");
    endTypeInfo(p);
  }
  jsonStartArray(p,"rows");
  SoftwareInstance *subsystemList = model->imsSubsystems;
  while (subsystemList){
    SoftwareInstance *info = subsystemList;
    char *name = info->bestName;
    if (info->type == SOFTWARE_TYPE_IMS){                        /* check is a little redundant, but defensive coding wins */
      if (!strcmp(tableName,"database")){
        jsonStartObject(p, NULL);
        jsonStringOut(p, "subsystemName", name, 8);
        jsonAddString(p, "databaseName", NA_PROPRIETARY_INFO);
        jsonAddString(p, "databaseType", NA_PROPRIETARY_INFO);
        jsonAddString(p, "status", NA_PROPRIETARY_INFO);
        jsonEndObject(p);
      } else if (!strcmp(tableName,"program")){
        jsonStartObject(p, NULL);
        jsonStringOut(p, "subsystemName", name, 8);
        jsonAddString(p, "programName", NA_PROPRIETARY_INFO);
        jsonEndObject(p);
      }
    }
    subsystemList = subsystemList->next;
  }
  jsonEndArray(p);
}


static void serveCICSData(ZOSModel         *model,
                          jsonPrinter      *p,
                          char             *subsystemBestName,
                          char             *tableName){
  /* name, type, description, columnWidth
     use scaleFactor, left/right justified, and pinnedLeft for the first column */
  if (!strcmp(tableName,"files")){
    startTypeInfo(p,"cicsFiles","CICS Files");
    addStringColumnInfo(p, "subsystemName", "Subsystem", "Subsystem Name", 1, 8);
    addStringColumnInfo(p, "fileName", "File", "File Name", 1, 8);
    addStringColumnInfo(p, "dsn", "Dataset Name", "Dataset Name", 1, 44);
    addStringColumnInfo(p, "isOpen", "Open", "Open", 1, 8);
    endTypeInfo(p);
  } else if (!strcmp(tableName,"transactions")){
    startTypeInfo(p,"cicsTransactions","CICS Transactions");
    addStringColumnInfo(p, "subsystemName", "Subsystem", "Subsystem Name", 1, 8);
    addStringColumnInfo(p, "transactionName", "Transaction", "Transaction Name", 1, 8);
    addStringColumnInfo(p, "programName", "Program Name", "Program Name", 1, 8);
    addStringColumnInfo(p, "profileName", "Profile Name", "Profile Name", 1, 8);
    endTypeInfo(p);
  } else if (!strcmp(tableName,"programs")){
    startTypeInfo(p,"cicsPrograms","CICS Programs");
    addStringColumnInfo(p, "subsystemName", "Subsystem", "Subsystem Name", 1, 8);
    addStringColumnInfo(p, "programName", "Program", "Program Name", 1, 8);
    endTypeInfo(p);
  } else{
    startTypeInfo(p,"cicsUnknown","CICS Unknown");
    endTypeInfo(p);
  }
  jsonStartArray(p,"rows");
  SoftwareInstance *subsystemList = model->cicsSubsystems;
  printf("before CICS list subsystemList 0x%x\n",subsystemList);
  while (subsystemList){

    SoftwareInstance *info = subsystemList;
    char *name = info->bestName;
    printf("CICS list elt=0x%x, best=%s type=0x%llx\n",info,info->bestName,info->type);
    if (info->type == SOFTWARE_TYPE_CICS){                        /* check is a little redundant, but defensive coding wins */
      if (!strcmp(tableName,"files")){
        jsonStartObject(p,NULL);
        jsonAddLimitedString(p, "subsystemName", name, 8);
        jsonAddString(p, "fileName", NA_PROPRIETARY_INFO);
        jsonAddString(p, "dsn", NA_PROPRIETARY_INFO);
        jsonAddBoolean(p,"isOpen", false);
        jsonEndObject(p);
      } else if (!strcmp(tableName,"transactions")){
        jsonStartObject(p,NULL);
        jsonAddLimitedString(p, "subsystemName", name, 8);
        jsonAddString(p, "transactionName", NA_PROPRIETARY_INFO);
        jsonAddString(p, "programName", NA_PROPRIETARY_INFO);
        jsonAddString(p, "profileName", NA_PROPRIETARY_INFO);
        jsonEndObject(p);
      } else if (!strcmp(tableName,"programs")){
        jsonStartObject(p, NULL);
        jsonAddLimitedString(p, "subsystemName", name, 8);
        jsonAddString(p, "programName", NA_PROPRIETARY_INFO);
        jsonEndObject(p);
      }
    }
    subsystemList = subsystemList->next;
  }
  jsonEndArray(p);
  
}

static void serveDB2Data(ZOSModel         *model,
                         jsonPrinter      *p,
                         char             *subsystemBestName,
                         char             *tableName){

  if (!strcmp(tableName,"configuration")){
    startTypeInfo(p,"db2Configuration","DB2 Configuration");
    addStringColumnInfo(p, "subsystemName", "Subsystem", "Subsystem Name", 1, 4);
    addBooleanColumnInfo(p, "isActive", "IsActive", "Is Active", 1, 8);
    addNumberColumnInfo(p, "version", "Version", "Version", 1, 2);
    addNumberColumnInfo(p, "release", "Release", "Release", 1, 2);
    addNumberColumnInfo(p, "modLevel", "ModLevel", "Mod Level", 1, 2);
    addStringColumnInfo(p, "masterJobname", "MasterJobname", "Master Jobname", 5, 8);
    addNumberColumnInfo(p, "masterASID", "MasterASID", "Master ASID", 4, 4);     /* should be hex */
    addStringColumnInfo(p, "domainName", "DomainName", "Domain Name", 1, 100);
    addStringColumnInfo(p, "location", "Location", "Location", 8, 8);
    addNumberColumnInfo(p, "listenerPort", "ListenerPort", "Listener Port", 4, 5);
    addNumberColumnInfo(p, "securePort", "SecurePort", "Secure Port", 4, 5);
    endTypeInfo(p);
  } else{
    startTypeInfo(p,"db2Unknown","DB2 Unknown");
    endTypeInfo(p);
  }
  jsonStartArray(p,"rows");
  SoftwareInstance *subsystemList = model->db2Subsystems;
  while (subsystemList){
    SoftwareInstance *info = subsystemList;
    char *name = info->bestName;
    if (info->type == SOFTWARE_TYPE_DB2){                        /* check is a little redundant, but defensive coding wins */
      if (!strcmp(tableName,"configuration")){
        jsonStartObject(p, NULL);
        jsonAddLimitedString(p, "subsystemName", name, 8);
        jsonAddBoolean(p, "isActive", false);
        jsonAddInt(p, "version", -1);
        jsonAddInt(p, "release", -1);
        jsonAddInt(p, "mod", -1);
        jsonAddString(p, "masterJobname", NA_PROPRIETARY_INFO);
        jsonAddInt(p, "masterASID", -1);
        jsonAddString(p, "domainName", NA_PROPRIETARY_INFO);
        jsonAddString(p, "location", NA_PROPRIETARY_INFO);
        jsonAddInt(p, "listenerPort", -1);
        jsonAddInt(p, "securePort", -1);
        jsonEndObject(p);
      }
    }
    subsystemList = subsystemList->next;
  }
  jsonEndArray(p);
  
}



static int serveSubsystemData(HttpService *service, 
                              HttpRequest *request,
                              HttpResponse *response,
                              int subsystemType,
                              char *subsystemBestName, /* NULL if not specified */
                              char *tableName){        /* must be specified if subsystemType is specific */
  
  DataService *dataService = (DataService*)service->userPointer;
  DiscoveryServiceState *discoveryServiceState = (DiscoveryServiceState*)dataService->extension;

  jsonPrinter *p = startResponse(response);
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "should serve discovery stuff\n");
  ZOSModel *model = discoveryServiceState->model;
  SoftwareInstance *instanceList = zModelGetSoftware(model);
  printf("done with discovery, or reusing cache \n");
  jsonStart(p);

  if (subsystemType == SOFTWARE_TYPE_ALL){
    jsonStartObject(p,"SUBSYSTEMS");
    addSubsystems(p,"DB2",model->db2Subsystems);
    addSubsystems(p,"IMS",model->imsSubsystems);
    addSubsystems(p,"CICS",model->cicsSubsystems);
    addSubsystems(p,"MQ",model->mqSubsystems);
    jsonEndObject(p);
  } else if (subsystemType == SOFTWARE_TYPE_ALL_SIMPLE){
      jsonStartObject(p,"SUBSYSTEMS");
      addSubsystems(p,"DB2",model->db2Subsystems);
      addSubsystems(p,"MQ",model->mqSubsystems);
      jsonEndObject(p);
  } else if (subsystemType == SOFTWARE_TYPE_IMS){
    serveIMSData(model,p,subsystemBestName,tableName);
  } else if (subsystemType == SOFTWARE_TYPE_CICS){
    serveCICSData(model,p,subsystemBestName,tableName);
  } else if (subsystemType == SOFTWARE_TYPE_DB2){
    serveDB2Data(model,p,subsystemBestName,tableName);
  }
  jsonEnd(p);
  finishResponse(response);
  printf("done with ss response\n");
  fflush(stdout);
  return 0;
}

static void emitTableMetadata(jsonPrinter *p, TableEmitter *emitter, char *uniqueName){
  jsonAddString(p,"resultMetaDataSchemaVersion","1.0");
  jsonAddString(p,"serviceVersion","1.0");
  jsonAddString(p,"resultType","table");
  jsonStartObject(p,"metaData");

  jsonStartObject(p,"tableMetaData");
  jsonAddString(p,"globalIdentifier",uniqueName);
  jsonEndObject(p);

  jsonStartArray(p,"columnMetaData");
  emitter->columnMetadataWriter(p);
  jsonEndArray(p);

  jsonEndObject(p);
}

static int serveTN3270Data(HttpService *service, 
                           HttpRequest *request,
                           HttpResponse *response){
  DataService *dataService = (DataService*)service->userPointer;
  DiscoveryServiceState *serviceState = (DiscoveryServiceState*)dataService->extension;
  /* should show example with 
     jsonPrinter *respondWithJsonPrinter(HttpResponse *response){
     */

  jsonPrinter *p = startResponse(response);
  char *luname = NULL;
  char *ipAddress = NULL;
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "*** NYI *** should serve discovery stuff\n");
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_INFO, "done with discovery\n");
  jsonStart(p);

  startTypeInfo(p,"tn3270","TN3270 Info");
  addStringColumnInfo(p, "luname", "LUName", "LU Name", 1, 8);
  addStringColumnInfo(p, "vtamApplicationJobname", "VTAMApplicationJobname", "VTAM Application Jobname", 1, 8);
  addNumberColumnInfo(p, "vtamApplicationASID", "VTAMApplicationASID", "VTAM Application ASID", 4, 4);
  addStringColumnInfo(p, "networkAddress", "NetworkAddress", "Network Address", 1, 8);
  addStringColumnInfo(p, "softwareType", "SoftwareType", "Software Type", 1, 30);
  addStringColumnInfo(p, "softwareSubtype", "SoftwareSubtype", "Software Subtype", 1, 30);
  addBooleanColumnInfo(p, "isTSO", "IsTSO", "Is ISPF", 3, 5);
  addBooleanColumnInfo(p, "isISPF", "IsISPF", "Is ISPF", 3, 5);
  addStringColumnInfo(p, "ispfPanelID", "ISPFPanelID", "ISPF Panel ID", 1, 8);
  endTypeInfo(p);

  DiscoveryContext *context = makeDiscoveryContext(NULL,serviceState->model);
  if (luname = getQueryParam(request,"luname")){
    findSessions(context,SESSION_KEY_TYPE_LUNAME,0,luname);
  } else if (ipAddress = getQueryParam(request,"ip4Address")){
    findSessions(context,SESSION_KEY_TYPE_IP4_STRING,0,luname);
  } else if (ipAddress = getQueryParam(request,"ip6Address")){
    findSessions(context,SESSION_KEY_TYPE_IP6_STRING,0,luname);
  } else{
    findSessions(context,SESSION_KEY_TYPE_ALL,0,luname);
    printf("get'em all\n");
  }
  /* rows begin here */

  jsonStartArray(p,"rows");

  TN3270Info *info = context->tn3270Infos;
  while (info){  
    jsonStartObject(p,NULL);
    
    jsonAddLimitedString(p,"luname",info->clientLUName,8);
    jsonAddString(p,"networkAddress",info->addressString);
    jsonAddLimitedString(p,"vtamApplicationJobname",info->vtamApplicationJobname,8);
    jsonAddInt(p,"vtamApplicationASID",info->vtamApplicationASID);
    jsonAddString(p,"softwareType",getSoftwareTypeName(info));
    jsonAddString(p,"softwareSubtype",getSoftwareSubtypeName(info));
    if (info->flags & TN3270_IS_TSO){
      jsonAddBoolean(p,"isTSO",TRUE);
      if (info->flags & TN3270_IS_ISPF){
        jsonAddBoolean(p,"isISPF",TRUE);
        jsonAddLimitedString(p,"ispfPanelID",info->ispfPanelid,8);
      }
    } else{
      jsonAddBoolean(p,"isTSO",FALSE);
      /* hmmm 
         Here, add some more particulars 

         */
    }
    jsonEndObject(p);
    info = info->next;
  } 
  jsonEndArray(p);
  jsonEnd(p);
  freeDiscoveryContext(context);
  finishResponse(response);
  printf("done with system response\n"); 
  fflush(stdout);
  return 0;

}

static int serveSystemData(HttpService *service, 
                           HttpRequest *request,
                           HttpResponse *response,
                           char *tableName){
  /* addressSpaces, vtamSessions */
  if (!strcmp(tableName,"tn3270")){
    serveTN3270Data(service,request,response);
  } else{
    respondWithError(response,HTTP_STATUS_NOT_FOUND,"no such system data table");
  }
  return 0;
}

static int serveDiscoveryData(HttpService *service, HttpResponse *response){
  /* 
     1) get second level identifier 
     2) make jsonPrinter with stdout (if possible)
     3) add keyword params,

     Current plugin URL scheme

     host:port
     MVD           1
     plugins       2
     identifier    (com.rs.zosmf.zossubsystems) 3
     services      4
     data          5 from plugin def 
   */
  HttpRequest *request = response->request;
  StringListElt *parsedFileTail = firstStringListElt(request->parsedFile);
  char *firstLevelName =  NULL;
  char *secondLevelName = NULL;
  char *thirdLevelName =  NULL;
  char *fourthLevelName = NULL;
  char *fifthLevelName = NULL;
  zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "calling serveDiscoveryData()  !!!!!!\n");
  int count = 0;
  while (parsedFileTail){
    count++;
    zowelog(NULL, LOG_COMP_DISCOVERY, ZOWE_LOG_DEBUG2, "frag(%d) = %s\n",count,parsedFileTail->string);
    switch (count){
    case 6:
      firstLevelName = parsedFileTail->string;
      break;
    case 7:
      secondLevelName = parsedFileTail->string;
      break;
    case 8:
      thirdLevelName = parsedFileTail->string;
      break;
    case 9:
      fourthLevelName = parsedFileTail->string;
      break;
    case 10:
      fifthLevelName = parsedFileTail->string;
      break;
    }
    parsedFileTail = parsedFileTail->next;
  }
  if (!strcmp(firstLevelName,"zosDiscovery")){
    if (!strcmp(secondLevelName,"simple")){
      if (thirdLevelName == NULL) {
        printf("zosDiscovery third level name not given\n");
      } else if (!strcmp(thirdLevelName,"subsystems")){
        serveSubsystemData(service, request, response, SOFTWARE_TYPE_ALL_SIMPLE,
                           NULL, fourthLevelName);
      } else{
        printf("zosDiscovery third level name not known %s\n",thirdLevelName);
      }

    } else if (!strcmp(secondLevelName,"system")){
      serveSystemData(service,request,response,thirdLevelName);

    } else if (!secondLevelName){
      printf("zosDiscovery second level name not given\n");
    } else{
      printf("zosDiscovery second level name not known %s\n",secondLevelName);
    }
  } else{
    printf("unknown zosDiscovery first level name=%s\n",firstLevelName);
  }
  return 0;
}

int zosDiscoveryServiceInstaller(DataService *dataService, HttpServer *server){
  printf("%s begin\n", __FUNCTION__);
  HttpService *httpService = makeHttpDataService(dataService, server);
  if (httpService != NULL) {
    httpService->serviceFunction = serveDiscoveryData;
    httpService->userPointer = dataService;
    httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
    dataService->externalAPI = NULL;
    hashtable *emitterTable = htCreate(257,stringHash,stringCompare,NULL,NULL);
    DiscoveryServiceState *state = (DiscoveryServiceState*)safeMalloc(sizeof(DiscoveryServiceState),"DiscoveryServiceState");
    memset(state,0,sizeof(DiscoveryServiceState));
    state->emitterTable = emitterTable;
    CrossMemoryServerName *priviligedServerName = getConfiguredProperty(server, HTTP_SERVER_PRIVILEGED_SERVER_PROPERTY);
    state->model = makeZOSModel(priviligedServerName);
    dataService->extension = state;
    printf("%s end\n", __FUNCTION__);
  }
  return 0;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

