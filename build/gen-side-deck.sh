#!/bin/bash
FILES="httpserver.o utils.o dataservice.o json.o alloc.o logging.o"

cd tmp-zss
dd if=zssServer.x  conv=unblock cbs=80  of=side-deck
nm $FILES |grep  ' T ' |awk '{ print $3 }' |sort -u >relevant-functions
cat relevant-functions |while read fn; do grep   ",'"$fn"'" side-deck; done > side-deck-filtered
# dd doesn't support file tags, while cat does. 
cat side-deck-filtered |dd conv=block cbs=80 bs=80 > zss.x
mkdir -p ../../lib
cp zss.x ../../lib
echo Side deck ../../lib/zss.x build successfully out of $FILES  



