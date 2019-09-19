

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stdbool.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#include <metal/stdio.h>
#include <metal/string.h>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#endif

#include "zowetypes.h"
#include "zos.h"

#include "pause-element.h"

#ifndef __ZOWE_OS_ZOS
#error z/OS targets are supported only
#endif

#pragma linkage(IEAVAPE2,OS)
#pragma linkage(IEAVDPE2,OS)
#pragma linkage(IEAVPSE2,OS)
#pragma linkage(IEAVRLS2,OS)
#pragma linkage(IEAVRPI2,OS)
#pragma linkage(IEAVTPE,OS)
#pragma linkage(IEAVXFR2,OS)

#pragma linkage(IEA4APE2,OS)
#pragma linkage(IEA4DPE2,OS)
#pragma linkage(IEA4PSE2,OS)
#pragma linkage(IEA4RLS2,OS)
#pragma linkage(IEA4RPI2,OS)
#pragma linkage(IEA4TPE,OS)
#pragma linkage(IEA4XFR2,OS)

#ifdef _LP64
#define IEAVAPE2 IEA4APE2
#define IEAVDPE2 IEA4DPE2
#define IEAVPSE2 IEA4PSE2
#define IEAVRLS2 IEA4RLS2
#define IEAVRPI2 IEA4RPI2
#define IEAVTPE  IEA4TPE
#define IEAVXFR2 IEA4XFR2
#endif

int peAlloc(PET *result,
            const PEStoken *ownerStoken,
            PEReleaseCode releaseCode,
            PEAuth authLevel,
            bool isBranchLinkage) {

  int32_t rc = 0;
  PEStoken stoken = {0};
  if (ownerStoken) {
    stoken = *ownerStoken;
  }
  int32_t linkage = isBranchLinkage ? PE_LINKAGE_BRANCH : PE_LINKAGE_SVC;

  int wasProblemState;
  int key;
  if (isBranchLinkage) {
    wasProblemState = supervisorMode(TRUE);
    key = setKey(0);
  }

  IEAVAPE2(&rc, &authLevel, result, &stoken, &releaseCode, &linkage);

  if (isBranchLinkage) {
    setKey(key);
    if (wasProblemState) {
     supervisorMode(FALSE);
    }
  }

  return rc;
}

int peDealloc(PET *token, bool isBranchLinkage) {

  int32_t rc = 0;
  int32_t linkage = isBranchLinkage ? PE_LINKAGE_BRANCH : PE_LINKAGE_SVC;

  int wasProblemState;
  int key;
  if (isBranchLinkage) {
    wasProblemState = supervisorMode(TRUE);
    key = setKey(0);
  }

  IEAVDPE2(&rc, token, &linkage);

  if (isBranchLinkage) {
    setKey(key);
    if (wasProblemState) {
     supervisorMode(FALSE);
    }
  }

  return rc;
}

int pePause(const PET *token, PET *newToken,
            PEReleaseCode *releaseCode,
            bool isBranchLinkage) {

  int32_t rc = 0;
  int32_t linkage = isBranchLinkage ? PE_LINKAGE_BRANCH : PE_LINKAGE_SVC;

  int wasProblemState;
  int key;
  if (isBranchLinkage) {
    wasProblemState = supervisorMode(TRUE);
    key = setKey(0);
  }

  IEAVPSE2(&rc, token, newToken, releaseCode, &linkage);

  if (isBranchLinkage) {
    setKey(key);
    if (wasProblemState) {
     supervisorMode(FALSE);
    }
  }

  return rc;
}

int peRelease(const PET *token,
              PEReleaseCode releaseCode,
              bool isBranchLinkage) {

  int32_t rc = 0;
  int32_t linkage = isBranchLinkage ? PE_LINKAGE_BRANCH : PE_LINKAGE_SVC;

  int wasProblemState;
  int key;
  if (isBranchLinkage) {
    wasProblemState = supervisorMode(TRUE);
    key = setKey(0);
  }

  IEAVRLS2(&rc, token, releaseCode, &linkage);

  if (isBranchLinkage) {
    setKey(key);
    if (wasProblemState) {
     supervisorMode(FALSE);
    }
  }

  return rc;
}

int peRetrieveInfo(const PET *token,
                   PEInfo *info,
                   bool isBranchLinkage) {

  int32_t rc = 0;
  int32_t linkage = isBranchLinkage ? PE_LINKAGE_BRANCH : PE_LINKAGE_SVC;

  int wasProblemState;
  int key;
  if (isBranchLinkage) {
    wasProblemState = supervisorMode(TRUE);
    key = setKey(0);
  }

  IEAVRPI2(&rc, &info->authLevel, token, &linkage,
           &info->ownerStoken, &info->currentStoken,
           &info->state, &info->releaseCode);

  if (isBranchLinkage) {
    setKey(key);
    if (wasProblemState) {
     supervisorMode(FALSE);
    }
  }

  return rc;
}

int peTest(const PET *token,
           PEState *state,
           PEReleaseCode *releaseCode) {

  int32_t rc = 0;
  IEAVTPE(&rc, token, state, releaseCode);

  return rc;
}

int peTranfer(const PET *token, PET *newToken,
              PEReleaseCode *releaseCode,
              const PET *targetToken,
              PEReleaseCode targetReleaseCode,
              bool isBranchLinkage) {

  int32_t rc = 0;
  int32_t linkage = isBranchLinkage ? PE_LINKAGE_BRANCH : PE_LINKAGE_SVC;

  int wasProblemState;
  int key;
  if (isBranchLinkage) {
    wasProblemState = supervisorMode(TRUE);
    key = setKey(0);
  }

  IEAVXFR2(&rc, token, newToken, releaseCode, targetToken, &targetReleaseCode,
           &linkage);

  if (isBranchLinkage) {
    setKey(key);
    if (wasProblemState) {
     supervisorMode(FALSE);
    }
  }

  return rc;
}


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

