#!/bin/bash
trap 'exit 1' ERR

ZSS=${ZSS:-$PWD/..}

DEFS=$ZSS/build/side-deck-defs
FILES="`cat $DEFS/files`"

cd $ZSS/build/tmp-zss

dd if=zssServer.x  conv=unblock cbs=80  of=side-deck

rm -f  relevant-functions
for f in $FILES
do 
  if [ -f $DEFS/$f-bl ]
  then
    echo "Found blacklist file for $f"
    nm $f |grep ' T ' |awk '{ print $3 }' |grep -v -f $DEFS/$f-bl >>relevant-functions
  elif [ -f $DEFS/$f-wl ]
  then
    echo "Found whilelist file for $f"
    nm $f |grep ' T ' |awk '{ print $3 }' |grep -f $DEFS/$f-wl >>relevant-functions 
  else
    echo "Adding all functions from $f to the side deck"
    nm $f |grep ' T ' |awk '{ print $3 }' >>relevant-functions
  fi
done
sort -u -o relevant-functions relevant-functions 

cat relevant-functions |while read fn; do grep   ",'"$fn"'" side-deck; done >side-deck-filtered

# dd doesn't support file tags, while cat does. 
cat side-deck-filtered |dd conv=block cbs=80 bs=80  |sed 's/\(.\{79\}\)./\1\n/g' >zss.x

mkdir -p $ZSS/lib
cp zss.x $ZSS/lib
echo Side deck $ZSS/lib/zss.x build successfully out of $FILES  



