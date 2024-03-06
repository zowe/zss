#!/bin/bash
set -e # fail the script if we get a non zero exit code

# create old school profiles
zowe profiles create base my_base --host old.host.com --user admin > /dev/null
zowe profiles create sample my_sample --port 443 --user user1 --password 123456 > /dev/null

ZOWE_OPT_HOST=new.host.com zowe zcsp list profile-args --port 1337
