#!/bin/sh

ZIS_PREFIX=${ZIS_PREFIX:-$USER.DEV}
ZIS_PARMLIB=${ZIS_PARMLIB:-$ZIS_PREFIX.PARMLIB}
ZIS_LOADLIB=${ZIS_LOADLIB:-$ZIS_PREFIX.LOADLIB}

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
            "or $ZIS_LOADLIB"
  exit 255
}  

add-plugin-to-libs()
{(
  TMPFILE=`mktemp1`

  trap "handle-failure" ERR

  cp -T "//'$ZIS_PARMLIB(ZWESIP00)'" $TMPFILE
  trap "rm $TMPFILE" EXIT

  chmod go-rwx $TMPFILE

  if add-plugin-to-parmfile $TMPFILE
  then
    >&2  echo "Updating $ZIS_PARMLIB(ZWESIP00)..."
    cp -T -v $TMPFILE "//'$ZIS_PARMLIB(ZWESIP00)'"
  fi
  
  >&2 echo "Installing plugin $LMODFILE to $ZIS_LOADLIB($LMODNAME) and $ZIS_PARMLIB"
  cp -X -v $LMODFILE "//'$ZIS_LOADLIB'" 
)}

trap "exit 1" ERR

if [ "$#" -lt 2 ] 
then
  >&2  cat <<EOF
Usage: $0 <file> <plugin name>
Environment variables: 
        ZIS_PREFIX: default destination. <ZIS_PREFIX>.LOADLIB and <ZIS_PREFIX>.PARMLIB are used
        if ZIS_PARMLIB is set then it overrides the parmlib
        if ZIS_LOADLIB is set then it overrides the loadlib
EOF
  exit 1
fi

LMODFILE=$1
PLUGINID=$2

LMODNAME=`basename $LMODFILE |tr a-z A-Z`

add-plugin-to-libs
