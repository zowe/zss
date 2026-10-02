#!/bin/sh
# zis-stop.sh - stop OUR test ZIS job (graceful STOP; CANCEL as fallback). Run: sh zis-stop.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/zis-env.sh"

echo "=== stop ZIS: job ${ZIS_JOB} ==="

# Prefer STOP: the cross-memory server honors it (server.c MODIFY/STOP handler) and
# releases its PCs and global area on the way out, which a CANCEL does not do as
# tidily. STOP needs the operator path, so it can fail on systems where that is
# unavailable; TSO CANCEL then does the job, and it works here only because
# ZIS_JOB is the userid plus a character (see zis-env.sh).
zis_opercmd "P ${ZIS_JOB}"

echo
echo "--- waiting for it to go away ---"
_gone=no
for _i in 1 2 3 4 5 6; do
  sleep 5
  case "$(tsocmd "STATUS ${ZIS_JOB}" 2>&1)" in
    *"NOT FOUND"*) _gone=yes; break ;;
  esac
done

if [ "$_gone" = yes ]; then
  echo "   stopped."
else
  echo "   still there after 30s; STOP may not have reached it. Cancelling:"
  tsocmd "CANCEL ${ZIS_JOB} PURGE" 2>&1 | sed 's/^/     /'
  sleep 5
  tsocmd "STATUS ${ZIS_JOB}" 2>&1 | grep -v '^STATUS' | sed 's/^/     /'
fi

echo
echo "Verify:  sh zis-status.sh   (NOT FOUND = down)."
