#!/bin/sh
# zis-start.sh - start OUR test ZIS by submitting its job.  Run on z/OS: sh zis-start.sh
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/zis-env.sh"

echo "=== start ZIS: SUBMIT ${ZIS_JCLLIB}(ZISRUN)  (job ${ZIS_JOB}, server '${ZIS_NAME}') ==="
tsocmd "SUBMIT '${ZIS_JCLLIB}(ZISRUN)'" 2>&1

echo
echo "Submitted. Verify:  sh zis-status.sh   (job ${ZIS_JOB} should be EXECUTING)"
echo "Then (re)start ZSS - on success it logs:"
echo "   ZWES1014I ZIS status - 'Ok' (name='${ZIS_NAME} ...', cmsRC='0' ...)"
