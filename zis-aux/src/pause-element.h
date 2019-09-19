

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef SRC_PAUSE_ELEMENT_H_
#define SRC_PAUSE_ELEMENT_H_

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stdbool.h>
#include <metal/stdint.h>
#else
#include <stdbool.h>
#include <stdint.h>
#endif

#pragma enum(4)

typedef enum PEAuth_tag {
  PE_AUTH_UNAUTHORIZED = 0,
  PE_AUTH_AUTHORIZED = 1,
  PE_AUTH_UNAUTH_CHECKPOINTOK = 2,
  PE_AUTH_AUTH_CHECKPOINTOK = 3,
} PEAuth;

typedef enum PELinkage_tag {
  PE_LINKAGE_SVC = 0,
  PE_LINKAGE_BRANCH = 1,
  PE_LINKAGE_UNTRUSTED_PET = 2,
} PELinkage;

typedef enum PEState_tag {
  PE_STATUS_PRERELEASED = 1,
  PE_STATUS_RESET = 2,
  PE_STATUS_RELEASED = 64,
  PE_STATUS_PAUSED = 128,
} PEState;

#pragma enum(reset)

ZOWE_PRAGMA_PACK

typedef struct PET_tag {
  union {
    char value[16];
    struct {
      uint64_t part1;
      uint64_t part2;
    };
  };
} PET;

typedef struct PEStoken_tag {
  uint64_t value;
} PEStoken;

typedef struct PEReleaseCode_tag {
  int value : 24;
} PEReleaseCode;

typedef struct PEInfo_tag {
  PEStoken ownerStoken;
  PEStoken currentStoken;
  PEState state;
  PEAuth authLevel;
  PEReleaseCode releaseCode;
} PEInfo;

ZOWE_PRAGMA_PACK_RESET

#ifndef __LONGNAME__

#define peAlloc PETALLOC
#define peDealloc PETDEALC
#define pePause PETPAUSE
#define peRelease PETRLS
#define peRetrieveInfo PETINFO
#define peTest PETTEST
#define peTranfer PETTRNFR

#endif


/**
 * @brief Allocates a new pause element (see the IEAVAPE2 doc for details).
 *
 * @param result The pause element token associated with the pause element.
 * @param ownerStoken specifies the STOKEN of the address space which is to be
 * considered the owner of the new pause element.
 * @param releaseCode The release code which will be returned to a paused DU
 * if the system deallocates the pause element.
 * @param authLevel The auth level of the pause element being allocated.
 * @param isBranchLinkage The services routine will be invoked via a branch
 * instruction. The caller must be in both key 0 and supervisor state.
 *
 * @return The IEAVAPE2 return code value.
 */
int peAlloc(PET *result,
            const PEStoken *ownerStoken,
            PEReleaseCode releaseCode,
            PEAuth authLevel,
            bool isBranchLinkage);

/**
 * @brief Deallocates a pause element (see the IEAVDPE2 doc for details).
 *
 * @param token The pause element token associated with the pause element.
 * @param isBranchLinkage The services routine will be invoked via a branch
 * instruction. The caller must be in both key 0 and supervisor state.
 *
 * @return The IEAVDPE2 return code value.
 */
int peDealloc(PET *token, bool isBranchLinkage);

/**
 * @brief Pauses the current task or SRB nondispatchable (see the IEAVPSE2 doc
 * for details).
 *
 * @param token The pause element token associated with the pause element.
 * @param newToken A new pause element token that identifies the pause element
 * identified by the original token.
 * @param releaseCode The release code specified by the issuer of the release
 * call.
 * @param isBranchLinkage The services routine will be invoked via a branch
 * instruction. The caller must be in both key 0 and supervisor state.
 *
 * @return The IEAVPSE2 return code value.
 */
int pePause(const PET *token, PET *newToken,
            PEReleaseCode *releaseCode,
            bool isBranchLinkage);

/**
 * @brief Releases a task or SRB that has been paused, or keeps a task or
 * SRB from being paused (see the IEAVRLS2 doc for details).
 *
 * @param token The pause element token associated with the pause element used
 * to pause a task or SRB.
 * @param releaseCode The release code to be returned to the paused task or SRB.
 * @param isBranchLinkage The services routine will be invoked via a branch
 * instruction. The caller must be in both key 0 and supervisor state.
 *
 * @return The IEAVRLS2 return code value.
 */
int peRelease(const PET *token,
              PEReleaseCode releaseCode,
              bool isBranchLinkage);

/**
 * @brief Gets information about a pause element (see the IEAVRPI2 doc for
 * details).
 *
 * @param token The pause element token associated with the pause element.
 * @param info The pause element information.
 * @param isBranchLinkage The services routine will be invoked via a branch
 * instruction. The caller must be in both key 0 and supervisor state.
 *
 * @return The IEAVRPI2 return code value.
 */
int peRetrieveInfo(const PET *token,
                   PEInfo *info,
                   bool isBranchLinkage);

/**
 * @brief Test a pause element and determines its state. The caller is
 * responsible for providing any needed recovery. The call will ABEND if a bad
 * pause element token is provided (see the IEAVTPE doc for details).
 *
 * @param token The pause element token associated with the pause element.
 * @param state The pause element state.
 * @param releaseCode The release code specified by the issuer of the release
 * call.
 *
 * @return The IEAVTPE return code value.
 */
int peTest(const PET *token,
           PEState *state,
           PEReleaseCode *releaseCode);

/**
 * @brief Release a paused task or SRB, and, when possible, give the task or
 * SRB immediate control. Optionally the function pauses the task or SRB under
 * which makes the transfer request. If the caller does not request that its
 * task or SRB to be paused, the task or SRB remain dispatchable (see the
 * IEAVXFR2 doc for details).
 *
 * @param token The pause element token associated with the pause element used
 * to pause a task or SRB.
 * @param newToken A new pause element token that identifies the pause element
 * identified by the original token.
 * @param releaseCode The release code specified by the issuer of the release
 * or transfer call.
 * @param targetToken Specifies the pause element token that is associated with
 * a pause element that is being used or will be used to pause a task or SRB.
 * If the task or SRB is paused, it will be released.
 * @targetReleaseCode The release code returned to a paused task or SRB.
 * @param isBranchLinkage The services routine will be invoked via a branch
 * instruction. The caller must be in both key 0 and supervisor state.
 *
 * @return The IEAVXFR2 return code value.
 */
int peTranfer(const PET *token, PET *newToken,
              PEReleaseCode *releaseCode,
              const PET *targetToken,
              PEReleaseCode targetReleaseCode,
              bool isBranchLinkage);

#endif /* SRC_PAUSE_ELEMENT_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
