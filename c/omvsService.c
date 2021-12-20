

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
  
  SPDX-License-Identifier: EPL-2.0
  
  Copyright Contributors to the Zowe Project.
*/

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "charsets.h"
#include "bpxnet.h"
#include "socketmgmt.h"
#include "httpserver.h"
#include "json.h"

#include "radmin.h"
#include "omvsService.h"
#include "serviceUtils.h"

/* Note:
 *
 * Please see: https://www.ibm.com/support/knowledgecenter/SSLTBW_2.1.0/com.ibm.zos.v2r1.icha400/listusr.htm
 *
 * It states that you can always view the details of the segment of your
 * own racf profile.
 */

static int parseOMVSSegment(RadminAPIStatus status, const RadminCommandOutput *result, void *userData);
static int serveOMVSSegment(HttpService *service, HttpResponse *response);
static int issueRACFCommand(char *command, OMVSSegment *omvs);

/* The max character count in a RACF
 * user id is 8. The command itself
 * is 16.
 *
 * ie.
 *
 * LISTUSER (TS5873) OMVS
 */
#define COMMAND_BUFFER_SIZE 24

int installOMVSService(HttpServer *server)
{
  HttpService *httpService = makeGeneratedService("OMVS_Service", "/omvs/**");
  httpService->authType = SERVICE_AUTH_NATIVE_WITH_SESSION_TOKEN;
  httpService->serviceFunction = &serveOMVSSegment;
  httpService->runInSubtask = FALSE;
  httpService->doImpersonation = FALSE;
  registerHttpService(server, httpService);

  return 0;
}

static int serveOMVSSegment(HttpService *service, HttpResponse *response)
{
  HttpRequest *request = response->request;
  jsonPrinter *p = respondWithJsonPrinter(response);
  char *routeFrag = stringListPrint(request->parsedFile, 1, 1000, "/", 0);
  char *route = stringConcatenate(response->slh, "/", routeFrag);

  if (strlen(request->username) > 8)
  {
    setResponseStatus(response, 500, "Internal Server Error");
    setDefaultJSONRESTHeaders(response);
    writeHeader(response);

    jsonStart(p);
    printErrorResponseMetadata(p);
    jsonAddString(p, "error", "Invalid Username");
    jsonEnd(p);

    finishResponse(response);

    return 0;
  }

  char command[COMMAND_BUFFER_SIZE + 1];
  sprintf(command, "LISTUSER (%s) OMVS", request->username);

  OMVSSegment *__ptr32 omvs = (OMVSSegment*__ptr32)safeMalloc31(sizeof(OMVSSegment),"OMVSSegment31");
  /* If the request method is GET */
  if(!strcmp(request->method, methodGET))
  {
    /* Issue the RACF command that
     * gets the OMVS segment.
     */
    int status = issueRACFCommand(command, omvs);
    if (status != 0)
    {
      setResponseStatus(response, 500, "Internal Server Error");
      setDefaultJSONRESTHeaders(response);
      writeHeader(response);

      jsonStart(p);
      printErrorResponseMetadata(p);
      jsonAddString(p, "error", "Could not issue RACF command");
      jsonEnd(p);
    }
    else
    {
      setResponseStatus(response, 200, "OK");
      setDefaultJSONRESTHeaders(response);
      writeHeader(response);

      jsonStart(p);
      jsonAddString(p, "uid", omvs->uid);
      jsonAddString(p, "home", omvs->home);
      jsonAddString(p, "program", omvs->program);
      jsonAddString(p, "cpuTimeMax", omvs->cpuTimeMax);
      jsonAddString(p, "assizeMax", omvs->assizeMax);
      jsonAddString(p, "fileProcMax", omvs->fileProcMax);
      jsonAddString(p, "procUserMax", omvs->procUserMax);
      jsonAddString(p, "threadsMax", omvs->threadsMax);
      jsonAddString(p, "mMapAreaMax", omvs->mMapAreaMax);
      jsonEnd(p);
    }
  }
  else
  {
    setResponseStatus(response, 405, "Method Not Allowed");
    setDefaultJSONRESTHeaders(response);
    addStringHeader(response, "Allow", "GET");
    writeHeader(response);

    jsonStart(p);
    printErrorResponseMetadata(p);
    jsonAddString(p, "error", "Only GET is supported");
    jsonEnd(p);
  }
  finishResponse(response);

  return 0;
}

static int parseOMVSSegment(RadminAPIStatus status, const RadminCommandOutput *result, void *userData)
{
  OMVSSegment *seg = userData;

  if (status.racfRC != 0)
  {
#ifdef DEBUG
    printf("Error: reason: %d, return: %d\n", status.racfRSN, status.racfRC);
#endif

    return status.racfRC;
  }

  /* We don't care about
   * any fields that aren't
   * in the OMVS segment.
   */
  int foundOMVS = FALSE;

  /* Please see below:
   *
   *   https://www.ibm.com/support/knowledgecenter/SSLTBW_2.1.0/com.ibm.zos.v2r1.ichd100/outmes.htm#outmes
   *
   * We must iterate through the message entries
   *
   */
  RadminVLText *curr = (RadminVLText *)&result->firstMessageEntry;
  while(curr->length != 0)
  {
    /* If we have found the OMVS segment, then
     * we should start getting the fields.
     */
    if (!strcmp(curr->text, OMVS_SEGMENT_MARKER))
    {
      foundOMVS = TRUE;
    }
    if (foundOMVS)
    {
      if (indexOfString(curr->text, curr->length, "UID", 0) != -1)
      {
        if (curr->length + 1 > sizeof(seg->uid))
        {
          return -1;
        }
        memcpy(seg->uid, curr->text+UID_OFFSET, curr->length+1);
      }

      if (indexOfString(curr->text, curr->length, "HOME", 0) != -1)
      {
        if (curr->length + 1 > sizeof(seg->home))
        {
          return -1;
        }
        memcpy(seg->home, curr->text+HOME_OFFSET, curr->length+1);
      }

      if (indexOfString(curr->text, curr->length, "PROGRAM", 0) != -1)
      {
        if (curr->length + 1 > sizeof(seg->program))
        {
          return -1;
        }
        memcpy(seg->program, curr->text+PROGRAM_OFFSET, curr->length+1);
      }

      if (indexOfString(curr->text, curr->length, "CPUTIMEMAX", 0) != -1)
      {
        if (curr->length + 1 > sizeof(seg->cpuTimeMax))
        {
          return -1;
        }
        memcpy(seg->cpuTimeMax, curr->text+CPU_TIME_MAX_OFFSET, curr->length+1);
      }

      if (indexOfString(curr->text, curr->length, "ASSIZEMAX", 0) != -1)
      {
        if (curr->length + 1 > sizeof(seg->assizeMax))
        {
          return -1;
        }
        memcpy(seg->assizeMax, curr->text+ASSIZEMAX_OFFSET, curr->length+1);
      }

      if (indexOfString(curr->text, curr->length, "FILEPROCMAX", 0) != -1)
      {
        if (curr->length + 1 > sizeof(seg->fileProcMax))
        {
          return -1;
        }
        memcpy(seg->fileProcMax, curr->text+FILE_PROC_MAX_OFFSET, curr->length+1);
      }

      if (indexOfString(curr->text, curr->length, "PROCUSERMAX", 0) != -1)
      {
        if (curr->length + 1 > sizeof(seg->procUserMax))
        {
          return -1;
        }
        memcpy(seg->procUserMax, curr->text+PROC_USER_MAX_OFFSET, curr->length+1);
      }

      if (indexOfString(curr->text, curr->length, "THREADSMAX", 0) != -1)
      {
        if (curr->length + 1 > sizeof(seg->threadsMax))
        {
          return -1;
        }
        memcpy(seg->threadsMax, curr->text+THREADS_MAX_OFFSET, curr->length+1);
      }

      if (indexOfString(curr->text, curr->length, "MMAPAREAMAX", 0) != -1)
      {
        if (curr->length + 1 > sizeof(seg->mMapAreaMax))
        {
          return -1;
        }
        memcpy(seg->mMapAreaMax, curr->text+M_MAP_AREA_MAX_OFFSET, curr->length+1);
      }
    }
    curr = (RadminVLText *)(curr->text + curr->length);
  }

  return 0;
}

static int issueRACFCommand(char *command, OMVSSegment *omvs)
{
  RadminCallerAuthInfo authInfo = {0};
  RadminStatus status;

  int radminRC = radminRunRACFCommand(
      authInfo,
      command,
      parseOMVSSegment,
      omvs,
      &status
  );

  if (radminRC != RC_RADMIN_OK)
  {
#ifdef DEBUG
    printf("error: radminRC: %d, safRC: %d, racfRC: %d, racfRSN: %d\n", radminRC, status.safRC, status.racfRC, status.racfRSN);
#endif
    return radminRC;
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

