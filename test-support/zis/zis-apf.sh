#!/bin/sh
# zis-apf.sh - APF-authorize the test ZIS loadlib, and say whether it worked.
#
#   sh test-support/zis/zis-apf.sh
#
# WHY A SEPARATE SCRIPT: APF authorization added with SETPROG is lost at the next
# IPL, so this is needed again every time the system comes back. Re-running the
# whole zis-configure.sh for it would rebuild ZIS, which takes minutes.
#
# WHY IT IS NEEDED AT ALL: ZWESIS01 is bound AC(1) and checks its own
# authorization at startup. Without an APF-authorized STEPLIB it reports
#   ZWES0117E Not APF-authorized (4)
#   ZWES0011E ZSS Cross-Memory server not started, RC = 80
# and ends with RC=0008. Measured on Marist 2026-10-02.
#
# PERMANENCE: for something that survives an IPL a systems programmer has to add
# the library to a PROGxx PARMLIB member. This script is the per-IPL stopgap.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/zis-env.sh"

echo "=== APF-authorize ${ZIS_LOADLIB} ==="

# SMS-managed and non-SMS libraries take different operands and the wrong one is
# rejected. An SMS-managed dataset reports a STORAGECLASS in the catalog; a
# non-SMS one reports only a VOLSER.
_lc="$(tsocmd "LISTCAT ENT('${ZIS_LOADLIB}') ALL" 2>&1)"
case "$_lc" in
  *STORAGECLASS*|*STORCLAS*) _apf="SMS"; echo "  SMS-managed" ;;
  *)
    _vol="$(echo "$_lc" | sed -n 's/.*VOLSER-*\([A-Z0-9]\{1,6\}\) .*/\1/p' | head -1)"
    if [ -z "$_vol" ]; then
      echo "  ERROR: cannot determine whether ${ZIS_LOADLIB} is SMS-managed, and no"
      echo "         VOLSER found in the catalog. Nothing attempted."
      exit 1
    fi
    _apf="VOLUME=${_vol}"; echo "  non-SMS, volume ${_vol}"
    ;;
esac

zis_opercmd "SETPROG APF,ADD,DSNAME=${ZIS_LOADLIB},${_apf}"

echo
echo "--- waiting for the command job to finish ---"
for _i in 1 2 3 4 5 6 7 8; do
  sleep 5
  case "$(tsocmd "STATUS ${ZIS_CMDJOB}" 2>&1)" in
    *"NOT FOUND"*|*"OUTPUT QUEUE"*) break ;;
  esac
done

_out="${ZIS_JCLLIB%.JCL}.CONOUT"
echo "--- $_out ---"
cat "//'$_out'" 2>/dev/null | sed 's/^/  /'

_cmd="SETPROG APF,ADD,DSNAME=${ZIS_LOADLIB},${_apf}"

echo
if zis_opercmd_failed; then
  echo "=== THE COMMAND WAS NOT ISSUED ==="
  echo "The reply above shows why. The usual cause here is that this userid has no"
  echo "extended MCS console authority, so TSO CONSOLE cannot activate at all:"
  echo "  AN ERROR OCCURRED DURING CONSOLE INITIALIZATION ... MCSOPER RETURN CODE"
  echo "That needs READ on MVS.MCSOPER.* in the OPERCMDS class, which is a security"
  echo "administrator's call, not something this script can arrange."
  echo
  echo "DO IT IN SDSF INSTEAD. On any SDSF panel, type this on the command line:"
  echo
  echo "    /${_cmd}"
  echo
  echo "then confirm with  D PROG,APF  and look for:"
  echo "  FORMAT=DYNAMIC                      <- if STATIC, SETPROG cannot add anything"
  echo "                                         and only a PROGxx member + IPL will do"
  echo "  an entry for ${ZIS_LOADLIB}"
  echo
else
  echo "=== the command appears to have been issued ==="
  echo "Confirm with  D PROG,APF  in SDSF: look for FORMAT=DYNAMIC and an entry for"
  echo "${ZIS_LOADLIB}."
  echo
fi

echo "=== then prove it the way that actually matters ==="
echo "ZIS checks its own authorization at startup, so it is the real verdict:"
echo
echo "   sh $HERE/zis-start.sh"
echo "   tsocmd \"STATUS ${ZIS_JOB}\""
echo
echo "   EXECUTING                -> APF took; ZIS is up."
echo "   ON OUTPUT QUEUE / absent -> read the reason with:"
echo "     . $HERE/zis-env.sh; zis_joblog ${ZIS_JOB} <jobid> | grep ZWES"
echo "   'ZWES0117E Not APF-authorized (4)' means APF still is not in effect."
echo
echo "REMEMBER: SETPROG APF is lost at the next IPL. For something durable a"
echo "systems programmer must add ${ZIS_LOADLIB} to a PROGxx PARMLIB member."
