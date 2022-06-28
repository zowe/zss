
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef __DSUTILS__
#define __DSUTILS__ 1

#include "httpserver.h"

#define INDEXED_DSCB 96

static char defaultDatasetTypesAllowed[3] = {'A','D','X'};
static char *defaultCSIFields[] ={ "NAME    ", "TYPE    ", "VOLSER  "};
static int defaultCSIFieldCount = 3;

const int DSCB_TRACE = FALSE;

/**
 * @brief Contains the name of a member in a dataset.
 */
typedef struct DatasetMemberName_tag {
  char value[8]; /* space-padded */
} DatasetMemberName;

/**
 * @brief Contains what DD name is used.
 */
typedef struct DDName_tag {
  char value[8]; /* space-padded */
} DDName;

/**
 * @brief Contains the name of a dataset.
 */
typedef struct DatasetName_tag {
  char value[44]; /* space-padded */
} DatasetName;

/**
 * @brief Contains the serial volume of a dataset.
 */
typedef struct Volser_tag {
  char value[6]; /* space-padded */
} Volser;

/**
 * @brief Gets the serial volume of a dataset.
 *
 * @param dataset dataset to be called when the function is
 * invoked. To be validated first and get its value for memcpy.
 * @param volser To pass its value and to be used in memcpy.
 * @return 0 is returned when the function has no error.
 * -1 is returned when an error occurred 
 */
int dsutilsGetVolserForDataset(const DatasetName *dataset, Volser *volser);

/**
 * @brief Gets the record format of a dataset.
 *
 * @param dscb a parameter to get the details of the physical
 * characteristics of a dataset.
 * @return U is returned when Undefined-length dataset.
 * F is returned when it is a fixed block and V for variable block.
 */
char dsutilsGetRecordLengthType(const char *dscb);

/**
 * @brief Gets the max record length of a dataset.
 *
 * @param dscb a parameter to get the details of the physical
 * characteristics of a dataset.
 * @return lrecl is the maximum length of a dataset .
 */
int dsutilsGetMaxRecordLength(const char *dscb);

/**
 * @brief Gets the Data Set Control Block of a dataset.
 *
 * @param dsname a parameter to get the dataset name.
 * @param dsnamelength a parameter to get the dataset length.
 * @param volser a parameter to get dataset the volume serial.
 * @param volserlength a parameter to get the volume serial length.
 * @param dscb1 a parameter to get the details of the physical
 * characteristics of a dataset.
 * @return 0 is successful obtaining the dscb.
 */
int dsutilsObtainDSCB1(const char *dsname, unsigned int dsnameLength, const char *volser, unsigned int volserLength, char *dscb1);

/**
 * @brief Gets the length of a dataset or respond an error.
 *
 * @param dsn a parameter to get the dataset name
 * @param ddpath a parameter to get dataset path
 * @return lrecl return the length of a dataset 
 * or this function encountered error 
 */
int dsutilsGetLreclOr(const DatasetName *dsn, const char *ddPath);

/**
 * @brief Validate if the dataset path is valid.
 *
 * @param path a parameter to get the dataset path
 * @return true if the path is valid and false if is not
 */
bool dsutilsIsDatasetPathValid(const char *path);

/**
 * @brief Extract dataset name and member name
 *
 * @param datasetpath a parameter to get the dataset path
 * @param dsn a parameter to get the dataset name
 * @param membername a parameter to get the member name
 */
void dsutilsExtractDatasetAndMemberName(const char *datasetPath, DatasetName *dsn, DatasetMemberName *memberName);

#endif


/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
