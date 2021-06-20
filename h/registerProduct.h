

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __REGISTERPRODUCT__
#define __REGISTERPRODUCT__

/**
 * @brief The function register product to SMF, record type (89) for
 * IBM Sub-Capacity Reporting Tool (SCRT)
 *
 * @param productReg   This should contain 'enable' to enable the registration.
 * @param productID    This should contain the max 8 bytes product ID.
 * @param productVer   This should contain the max 8 bytes production version. e.g. 1.0
 * @param productOwner This should contain the max 16 bytes product owner. e.g. 'IBM CORP'
 * @param productName  This should contain the max 16 bytes product name. e.g. 'IBM Z Dist Zowe'
 *
 * @const char arrays:
 *   MODULE_REGISTER_USAGE (8 byes) this must be "IFAUSAGE"
 *   PROD_QUAL - product qualifier (8 bytes)  is "NONE    "
 *
 * @return One of the RC_ZSS_PREG_NULL_xx return codes or the return code from SVC 109.
 */

void registerProduct(char *productReg, char *productPID, char *productVer,
                     char *productOwner, char *productName);

#endif

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

