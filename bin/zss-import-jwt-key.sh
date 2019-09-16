#!/bin/sh
# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.

mktemp1()
{
  FILE=/tmp/zss-$$-$RANDOM
  while [ -e $FILE ]
  do
    FILE=/tmp/zss-$$-$RANDOM
  done
  echo $FILE
}

if [ "$#" -ne 2 ]
then
   >&2  cat <<EOF
Usage: $0 <keystore file> <PKCS#11 token name>
EOF
  exit 1
fi

SRC_KEYSTORE=$1
TOKEN=$2

>&2 echo "Will import keypair 'jwtsecret' from $SRC_KEYSTORE into $TOKEN"
trap '>&2 echo Failure; exit 1' ERR

>&2 echo "Checking if $TOKEN exists and its contents..."
if ! /bin/tsocmd "RACDCERT LISTTOKEN('$TOKEN')"
then
  /bin/tsocmd "RACDCERT ADDTOKEN('$TOKEN')"
fi

SRC_KEYSTORE_PASSWORD="${SRC_KEYSTORE_PASSWORD:-password}"
TMP_KEYSTORE_PASSWORD=$SRC_KEYSTORE_PASSWORD

TMP_KEYSTORE=`mktemp1`
trap 'rm $TMP_KEYSTORE' exit

>&2 echo "Extracting 'jwtsecret' from $SRC_KEYSTORE..."
keytool -importkeystore \
  -srckeystore $SRC_KEYSTORE -srcstoretype PKCS12 \
  -destkeystore $TMP_KEYSTORE -deststoretype PKCS12 \
  -srcalias jwtsecret -srcstorepass $SRC_KEYSTORE_PASSWORD \
	-deststorepass $TMP_KEYSTORE_PASSWORD

chmod go-rwx $TMP_KEYSTORE

>&2 echo "Adding 'jwtsecret' to $TOKEN..."
echo $TMP_KEYSTORE_PASSWORD |gskkyman -i -t $TOKEN -l jwtsecret -p $TMP_KEYSTORE

>&2 echo "Done"

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.
