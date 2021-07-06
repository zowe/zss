#!/bin/sh
# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.

ZIS_PREFIX=${ZIS_PREFIX:-$USER.DEV}
ZIS_PARMLIB=${ZIS_PARMLIB:-$ZIS_PREFIX.PARMLIB}
ZIS_PARMLIB_MEMBER=${ZIS_PARMLIB_MEMBER:-ZWESIP00}
ZIS_PLUGINLIB=${ZIS_PLUGINLIB:-ZIS_LOADLIB:-$ZIS_PREFIX.PLUGINLIB}

mktemp1()
{
  FILE=/tmp/zss-plugin-install-$$-$RANDOM
  while [ -e $FILE ]
  do
    FILE=/tmp/zss-plugin-install-$$-$RANDOM
  done
  echo $FILE
}

add-plugin-to-parmfile()
{
  PARMFILE=$1

  LINE_PREFIX="ZWES.PLUGIN.$PLUGINID="
  LINE="$LINE_PREFIX""$LMODNAME"
  if egrep -q "^$LINE_PREFIX" $PARMFILE 
  then
    if egrep -q "^$LINE\$" $PARMFILE
    then
      >&2  echo "Skipping updating $ZIS_PARMLIB: plugin already registered" 
      return 1
    else 
      >&2  echo "Can't update $ZIS_PARMLIB: the plugin is already"\
                "registered and points to a different load module"
      exit 255
    fi
  fi
  echo $LINE >> $PARMFILE
  return 0
}

handle-failure()
{
  >&2  echo "Installing plugin $PLUGINID failed. Make sure no running ZIS instance"\
            "or any other process (e.g. ISPF Editor) has locked $ZIS_PARMLIB"\
            "or $ZIS_PLUGINLIB"
  exit 255
}  

add-plugin-to-libs()
{(
  TMPFILE=`mktemp1`

  trap "handle-failure" ERR

  cp -T "//'$ZIS_PARMLIB($ZIS_PARMLIB_MEMBER)'" $TMPFILE
  trap "rm $TMPFILE" EXIT

  chmod go-rwx $TMPFILE

  if add-plugin-to-parmfile $TMPFILE
  then
    >&2  echo "Updating $ZIS_PARMLIB($ZIS_PARMLIB_MEMBER)..."
    cp -T -v $TMPFILE "//'$ZIS_PARMLIB($ZIS_PARMLIB_MEMBER)'"
  fi
  
  >&2 echo "Installing plugin $LMODFILE to $ZIS_PLUGINLIB($LMODNAME) and $ZIS_PARMLIB"
  cp -X -v $LMODFILE "//'$ZIS_PLUGINLIB'" 

)}

trap "exit 1" ERR

#################################################
# Deploy the load libraries into the ZIS PLUIGINLIB.
#
# Globals:
#   ZIS_PLUGINLIB
# Arguments:
#   $1 directory with the load libraries to be
#      deployed
# Returns:
#   0 on success, 8 on error.
#################################################
deploy-loadlib() {

  loadlib_dir=$1

  cp -X -v $loadlib_dir/* "//'$ZIS_PLUGINLIB'"
  if [ $? -ne 0 ]; then
    >&2  echo "error: failed to update PLUGINLIB. Make sure no running ZIS" \
              "instance or any other process (e.g. ISPF Editor) has locked" \
               "$ZIS_PLUGINLIB."
    return 8
  fi

  echo "info: copied load libraries from $loadlib_dir to $ZIS_PLUGINLIB"

  return 0
}

#################################################
# Deploy the sample libraries into the ZIS PARMLIB.
#
# Globals:
#   None
# Arguments:
#   $1 directory with the sample libraries to be
#      deployed
# Returns:
#   0 on success, 8 on error.
#################################################
deploy-samplib() {

  samplib_dir=$1

  for samp_file in $samplib_dir/*; do
    sh zis-parmlib-update.sh $samp_file
    if [ $? -ne 0 ]; then
      return 8
    fi
  done

  return 0
}

#################################################
# Perform simple installation.
#
# Globals:
#   None
# Arguments:
#   $1 plug-in load library to be installed
# Returns:
#   0 on success, 255 on error.
#################################################
handle-simple-install() {

  LMODFILE=$1
  PLUGINID=$2

  LMODNAME=`basename $LMODFILE |tr a-z A-Z`

  add-plugin-to-libs

  return $?
}

#################################################
# Perform extended installation.
#
# Globals:
#   None
# Arguments:
#   $1 directory with the load libraries to be
#      deployed
#   $2 directory with the sample libraries to be
#      deployed
# Returns:
#   0 on success, 8 on error.
#################################################
handle-extended-intall() {(

  loadlib_dir=$1
  samplib_dir=$2

  deploy-loadlib $loadlib_dir
  loadlib_rc=$?
  if [ $loadlib_rc -ne 0 ]; then
    return $loadlib_rc
  fi

  deploy-samplib $samplib_dir
  samplib_rc=$?
  if [ $samplib_rc -ne 0 ]; then
    return $samplib_rc
  fi

  return 0
)}

print-usage() {
  >&2  cat <<EOF
Usage: $0 <file> <plugin name>
       $0 simple-install <file> <plugin name>
       $0 extended-install <loadlib-directory> <samplib-directory>
Environment variables: 
        ZIS_PREFIX: install destination. The default value is $USER.DEV, if
          ZIS_PREFIX is set, it overrides the default destination.
        ZIS_PARMLIB: PARMLIB destination. The default value is
          <ZIS_PREFIX>.PARMLIB. If ZIS_PARMLIB is set then it overrides
          the default value.
        ZIS_PARMLIB_MEMBER: PARMLIB member to be updated. The default value is
          ZWESIP00. If ZIS_PARMLIB_MEMBER is set then it overrides the default
          value.
        ZIS_PLUGINLIB: PLUGINLIB destination. The default value is
          <ZIS_PREFIX>.PLUGINLIB. If ZIS_PLUGINLIB is set then it overrides
          the default value.
EOF
}

if [ "$#" -lt 2 ]; then
  print-usage
  exit 1
fi

if [ $1 = "simple-install" ]; then

  if [ "$#" -lt 3 ]; then
    print-usage
    exit 1
  fi

  handle-simple-install $2 $3
  exit $?

elif [ $1 = "extended-install" ]; then

  if [ "$#" -lt 3 ]; then
    print-usage
    exit 1
  fi

  handle-extended-intall $2 $3
  exit $?

else

  if [ "$#" -lt 2 ]; then
    print-usage
    exit 1
  fi

  # Handle simple-install, which is also the default behavior of the script.
  handle-simple-install $1 $2
  exit $?

fi

# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
# 
# SPDX-License-Identifier: EPL-2.0
# 
# Copyright Contributors to the Zowe Project.
