

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef SRC_PC_H_
#define SRC_PC_H_

#ifdef METTLE
#include <metal/metal.h>
#include <metal/stdbool.h>
#include <metal/stdint.h>
#else
#include "stdbool.h"
#include "stdint.h"
#endif

#include "zowetypes.h"

ZOWE_PRAGMA_PACK

typedef struct PCLatentParmList_tag {
  void * __ptr32 parm1;
  void * __ptr32 parm2;
} PCLatentParmList;

typedef struct PCLinkageIndex {
  uint32_t sequenceNumber;
  uint32_t pcNumber;
} PCLinkageIndex;

ZOWE_PRAGMA_PACK_RESET

#ifndef __LONGNAME__
#define pcSetAllAddressSpaceAuthority PCAXSET
#define pcReserveLinkageIndex PCLXRES
#define pcFreeLinkageIndex PCLXFRE
#define pcMakeEntryTableDescriptor PCETCRED
#define pcAddToEntryTableDescriptor PCETADD
#define pcRemoveEntryTableDescriptor PCETREMD
#define pcCreateEntryTable PCETCRE
#define pcDestroyEntryTable PCETDES
#define pcConnectEntryTable PCETCON
#define pcDisconnectEntryTable PCETDIS
#define pcCallRoutine PCCALLR
#endif

typedef enum PCLinkageIndexSize_tag {
  LX_SIZE_12,
  LX_SIZE_16,
  LX_SIZE_23,
  LX_SIZE_24,
} PCLinkageIndexSize;

typedef uint32_t PCEntryTableToken;

typedef struct ETD_tag EntryTableDescriptor;

/**
 * @brief Sets the authorization index of the home address space to
 * the value specified by the caller.
 *
 * @param reasonCode AXSET return code value.
 *
 * @return One of the RC_PC_xx return codes.
 */
int pcSetAllAddressSpaceAuthority(int *reasonCode);

/**
 * @brief Reserves a linkage index for the caller's use.
 *
 * @param isSystem Specifies whether or not the linkage index is reserved
 * for system connections.
 * @param isReusable Specifies whether or not the linkage index is reusable.
 * @param indexSize The maximum size of the linkage index.
 * @param result The result linkage index value.
 * @param reasonCode LXRES return code value.
 *
 * @return One of the RC_PC_xx return codes.
 */
int pcReserveLinkageIndex(bool isSystem, bool isReusable,
                          PCLinkageIndexSize indexSize,
                          PCLinkageIndex *result,
                          int *reasonCode);

/**
 * @brief Frees a linkage index.
 *
 * @param index The linkage index to be freed.
 * @param forced Free the linkage index even if entry tables are currently
 * connected to it
 * @param reasonCode LXFRE return code value.
 *
 * @return One of the RC_PC_xx return codes.
 */
int pcFreeLinkageIndex(PCLinkageIndex index, bool forced, int *reasonCode);

/**
 * @brief Allocates a table entry descriptor.
 *
 * @return The table entry descriptor address.
 */
EntryTableDescriptor *pcMakeEntryTableDescriptor(void);

/**
 * @brief Add a new entry to a table entry descriptor.
 *
 * @param descriptor The descriptor that will contain the new entry.
 * @param routine The routine to be invoked in the corresponding PC.
 * @param routineParameter1 The first parameter of the routine's latent
 * parameter list.
 * @param routineParameter2 The second parameter of the routine's latent
 * parameter list.
 * @param isSASNOld Specifies whether the PC routine will execute with SASN
 * equal to the caller's PASN.
 * @param isAMODE64 Specifies whether the PC routine is AMODE 64.
 * @param isSUP Specifies whether the PC routine will run in SUP state.
 * @param isSpaceSwitch Specifies whether the PC routine is a space switch
 * routine.
 * @param key The key in which the PC routine will be executed.
 *
 * @return One of the RC_PC_xx return codes.
 */
int pcAddToEntryTableDescriptor(EntryTableDescriptor *descriptor,
                                int (* __ptr32 routine)(void),
                                uint32_t routineParameter1,
                                uint32_t routineParameter2,
                                bool isSASNOld,
                                bool isAMODE64,
                                bool isSUP,
                                bool isSpaceSwitch,
                                int key);

/**
 * @brief Removes a table entry descriptor.
 *
 */
void pcRemoveEntryTableDescriptor(EntryTableDescriptor *descriptor);

/**
 * @brief Builds a PC entry table using the descriptions of each entry.
 *
 * @param descriptor The descriptor containing the description of the entries.
 * @param resultToken The token associated with the new entry table.
 * @param reasonCode ETCRE return code value.
 *
 * @return One of the RC_PC_xx return codes.
 */
int pcCreateEntryTable(const EntryTableDescriptor *descriptor,
                       PCEntryTableToken *resultToken,
                       int *reasonCode);

/**
 * @brief Destroys a PC entry table.
 *
 * @param token The token associated with the entry table.
 * @param purge Specifies whether the entry table is to be disconnected from
 * all linkage tables and then destroyed.
 * @param reasonCode ETDES return code value.
 *
 * @return One of the RC_PC_xx return codes.
 */
int pcDestroyEntryTable(PCEntryTableToken token, bool purge, int *reasonCode);

/**
 * @brief Connects a previously created entry table to the specified linkage
 * index table in the current home address space.
 *
 * @param token The token associated with the entry table.
 * @param index Specifies the linkage index to which the specified entry table
 * is to be connected.
 * @param reasonCode ETCON return code value.
 *
 * @return One of the RC_PC_xx return codes.
 */
int pcConnectEntryTable(PCEntryTableToken token, PCLinkageIndex index,
                        int *reasonCode);

/**
 * @brief Disconnects a previously connected entry table from the specified
 * linkage index table in the current home address space.
 *
 * @param token The token associated with the entry table.
 * @param reasonCode ETCDIS return code value.
 *
 * @return One of the RC_PC_xx return codes.
 */
int pcDisconnectEntryTable(PCEntryTableToken token, int *reasonCode);

/**
 * @brief Calls the PC routine associated with the specified PC and sequence
 * numbers.
 *
 * @param pcNumber The PC number of the routine (found in PCLinkageIndex).
 * @param sequenceNumber The sequence number of the routine
 * (found in PCLinkageIndex).
 * @param parmBlock The parameter block passed the routine via R1.
 *
 * @return The R15 value of the routine.
 */
int pcCallRoutine(uint32_t pcNumber, uint32_t sequenceNumber, void *parmBlock);

#define RC_PC_OK            0
#define RC_PC_AXSET_FAILED  8
#define RC_PC_LXRES_FAILED  9
#define RC_PC_LXFRE_FAILED  10
#define RC_PC_ETD_FULL      11
#define RC_PC_ETCRE_FAILED  12
#define RC_PC_ETDES_FAILED  13
#define RC_PC_ETCON_FAILED  14
#define RC_PC_ETDIS_FAILED  15

#endif /* SRC_PC_H_ */


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
