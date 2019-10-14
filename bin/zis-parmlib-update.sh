#!/bin/sh
# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
# SPDX-License-Identifier: EPL-2.0
#
# Copyright Contributors to the Zowe Project.

# This script adds or updates pramaters in ZIS PARMLIB member specified in
# the provided SAMPLIB file.

# Make sure all files created in this script are only accesible by the owner.
umask go-rwx

# Prohibit undefined variables.
set -u

# ZIS-related global variables.
ZIS_PREFIX=${ZIS_PREFIX:-$USER.DEV}
ZIS_PARMLIB=${ZIS_PARMLIB:-$ZIS_PREFIX.PARMLIB}
ZIS_PARMLIB_MEMBER=${ZIS_PARMLIB_MEMBER:-ZWESIP00}

#################################################
# Generate a name for a temporary file in /tmp
#
# Globals:
#   RANDOM
# Arguments:
#   None
# Returns:
#   The generated file name
#################################################
make_temp_filename() {
  name=/tmp/zis-sample-install-$$-$RANDOM
  while [ -e $name ]
  do
    name=/tmp/zis-sample-install-$$-$RANDOM
  done
  echo $name
}

#################################################
# Try to resolve values that are defined using
# environmental variables, otherwise return
# the original value
#
# Globals:
#   None
# Arguments:
#   $1 Value from a key-value pair
# Returns:
#   * If an env variable is provided, its value
#     is returned on success
#   * If an env varible if provided and
#     the variable is not defined,
#     string VALUE_NOT_FOUND is returned
#   * The original value is returned
#################################################
resolve_env_parameter() {

  parm=$1

  # Are we dealing with an env variable based value?
  echo $parm | grep -E "^[$]{1}[A-Z0-9_]+$" 1>/dev/null
  # Yes, resolve the value using eval, otherwise return the value itself.
  if [ $? -eq 0 ]; then
    ( eval "echo $parm" 2>/dev/null )
    if [ $? -ne 0 ]; then
      echo "VALUE_NOT_FOUND"
    fi
  else
    echo $parm
  fi

}

#################################################
# Add (or update) a key-value pair to a PARMLIB
# member.
#
# Globals:
#   RANDOM
# Arguments:
#   $1 PARMLIB file to be updated
#   $2 Parameter to be added/updated
#   $3 Parameter value
# Returns:
#   0 on success, 8 on error.
#################################################
add_key_value_parm_to_parmlib() {(

  # Input parameters.
  parm_file=$1
  parm_key=$2
  parm_value=$(resolve_env_parameter $3)

  # Check if we recevied a non-empty value for the key (if the value has been
  # defined using an environmental variable).
  if [ "$parm_value" = "VALUE_NOT_FOUND" ]; then
    echo "error: value for $parm_key has not been provided"
    exit 8
  fi

  key_value="$parm_key=$parm_value"
  parm_file_updated="$parm_file"_updated

  # Make a temp file for sed.
  touch $parm_file_updated
  if [ $? -ne 0 ]; then
    exit 8
  fi

  # Make sure the temporary file produced by sed is removed.
  trap "rm $parm_file_updated" EXIT

  echo "info: adding/updaing $parm_key with value $parm_value"

  # Try to find the key in our parameter file.
  grep "^[[:blank:]]*$parm_key=" $parm_file 1>/dev/null

  # Does the key already exist? If yes, subtitute the value.
  if [ $? -eq 0 ]; then

    echo "info: parameter $parm_key found, updating its value"

    sed -E "s/(^[[:blank:]]*)$parm_key=(.*$)/$key_value/g" \
      $parm_file > $parm_file_updated
    if [ $? -ne 0 ]; then
      exit 8
    fi

    # Copy the updated file. Make sure the cp knows it's text and does the
    # correct conversion.
    cp -T -O u $parm_file_updated $parm_file
    if [ $? -ne 0 ]; then
      exit 8
    fi

  # The key is new, add it to the parameter file.
  elif [ $? -eq 1 ]; then

    echo "info: parameter $parm_key not found, adding it as a new value"
    echo $key_value >> $parm_file

  else
    exit 8
  fi

  exit 0
)}

#################################################
# Clean up and print info/error messages upon
# return from update_parmlib()
#
# Globals:
#   TEMP_PARMLIB_MEMBER
#   LINENO
# Arguments:
#   None
# Returns:
#   0 on success, 8 on error.
#################################################
handle_update_parmlib_exit() {

  function_rc=$?
  trap_rc=0

  if [ $function_rc -eq 0 ]; then
    echo "info: successfully updated $ZIS_PARMLIB($ZIS_PARMLIB_MEMBER)"
    trap_rc=0
  else
    >&2 echo "error: script failed, RC = $function_rc"
    trap_rc=8
  fi

  if [ -e $TEMP_PARMLIB_MEMBER ]; then
    rm $TEMP_PARMLIB_MEMBER
  fi

  exit $trap_rc
}

#################################################
# Update the ZIS PARMLIB member with the values
# from the specified SAMPLIB file.
#
# Globals:
#   ZIS_PARMLIB
#   ZIS_PARMLIB_MEMBER
#   TEMP_PARMLIB_MEMBER
#   BASH_REMATCH
# Arguments:
#   $1 SAMPLIB file
# Returns:
#   0 on success, 8 on error.
#################################################
update_parmlib() {(

  # Generate a name for the temporary PARMLIB file.
  TEMP_PARMLIB_MEMBER=$(make_temp_filename)

  # Setup out exit handler.
  trap "handle_update_parmlib_exit" EXIT

  # Copy the ZIS PARMLIB member to a temporary file.
  cp -T "//'$ZIS_PARMLIB($ZIS_PARMLIB_MEMBER)'" $TEMP_PARMLIB_MEMBER
  if [ $? -ne 0 ]; then
    exit 8
  fi


  # Input parameters.
  parm_file=$1

  echo "info: updating $ZIS_PARMLIB($ZIS_PARMLIB_MEMBER) with the values from\
  $parm_file"

  # Go over the provided SAMPLIB file, pick key-value pairs and update the ZIS
  # PARMLIB member with those new values.
  while IFS= read -r line; do
    key_value_pattern="^[[:blank:]]*[[:alnum:].]*[[:blank:]]*=\
[[:blank:]]*[[:graph:]]*[[:blank:]]*$"
    echo $line | grep $key_value_pattern 1>/dev/null
    if [ $? -eq 0 ]; then
      # Extract the key/value and update the PARMLIB.
      echo $line | \
        (
          IFS='=' read key value ;
          add_key_value_parm_to_parmlib $TEMP_PARMLIB_MEMBER $key $value
        )
      if [ $? -ne 0 ]; then
        exit 8
      fi
    fi
  done < $parm_file

  # Copy the updated PARMLIB member back to the original location.
  cp -T -O u -v $TEMP_PARMLIB_MEMBER "//'$ZIS_PARMLIB($ZIS_PARMLIB_MEMBER)'"
  if [ $? -ne 0 ]; then
    >&2  echo "error: failed to update PARMLIB. Make sure no running ZIS" \
              "instance or any other process (e.g. ISPF Editor) has locked" \
              "$ZIS_PARMLIB."
    exit 8
  fi

  exit 0
)}

# Check the input parameters and print the help info if necessary.
if [ "$#" -lt 1 ]
then
  1>&2 cat <<EOF
Usage: $0 <samplib_file>
Environment variables:
        ZIS_PREFIX: install destination. The default value is $USER.DEV, if
          ZIS_PREFIX is set, it overrides the default destination.
        ZIS_PARMLIB: PARMLIB destination. The default value is
          <ZIS_PREFIX>.PARMLIB. If ZIS_PARMLIB is set then it overrides
          the default value.
        ZIS_PARMLIB_MEMBER: PARMLIB member to be updated. The default value is
          ZWESIP00. If ZIS_PARMLIB_MEMBER is set then it overrides the default
          value.
EOF
  exit 8
fi

# Input parameters of the script.
samplib_file=$1

# Go update the ZIS PARMLIB member with the specified SAMPLIB file's values.
update_parmlib $samplib_file
exit $?


# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
# SPDX-License-Identifier: EPL-2.0
#
# Copyright Contributors to the Zowe Project.

