#!/bin/sh
# zis-status.sh - is OUR test ZIS job up?  Run on z/OS: sh zis-status.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/zis-env.sh"

echo "=== ZIS status: job ${ZIS_JOB} (cross-memory server '${ZIS_NAME}') ==="
zis_show_env

echo; echo "--- TSO STATUS ${ZIS_JOB}  (returns HERE; EXECUTING = up, NOT FOUND = down) ---"
tsocmd "STATUS ${ZIS_JOB}" 2>&1

echo
echo "Fully shell-driven - no SDSF needed. The definitive FUNCTIONAL check is ZSS itself:"
echo "on connect it logs  ZWES1014I ZIS status 'Ok' ... cmsRC='0'  (cmsRC=12 = '${ZIS_NAME}' not up)."
echo "Production ZIS is ZWESIS_STD / jobname ZWE1SZ - NOT this one."
