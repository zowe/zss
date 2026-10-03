#!/bin/sh
# zis-configure.sh - one-time setup of OUR private test ZIS (runs as a batch JOB).
#
# Run UNDER PLAIN /bin/sh (NOT env.sh/zopen bash): it touches EBCDIC samplib members +
# MVS datasets with NATIVE tools; zopen (ASCII) sed/cat would mangle them.
#   ->   sh <zss-repo>/test-support/zis/zis-configure.sh
#
# Idempotent. Assumes the invoker holds RACF + operator (SETPROG) authority.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; . "$HERE/zis-env.sh"
SAMP="$ZSS_ROOT/samplib/zis"

echo "############ Configuring private test ZIS (JOB model) ############"; zis_show_env

# ---------------------------------------------------------------------------
echo; echo "## 1. Provision datasets under your HLQ (idempotent: create only if absent) ##"
zis_provision "$ZIS_LOADLIB" "SPACE(5,5) CYL DSORG(PO) DSNTYPE(LIBRARY) DIR(20) RECFM(U) BLKSIZE(6144)"  # PDSE: ZIS binds program objects w/ long external names (IEW2640E on a plain PDS)
zis_provision "$ZIS_PARMLIB" "SPACE(1,1) TRACKS DSORG(PO) DIR(5) RECFM(F B) LRECL(80)"
zis_provision "$ZIS_JCLLIB"  "SPACE(1,1) TRACKS DSORG(PO) DIR(5) RECFM(F B) LRECL(80)"

# ---------------------------------------------------------------------------
echo; echo "## 2. Build ZIS load modules (ZWESIS01 + ZWESAUX, AC(1)) into the loadlib ##"
( cd "$ZSS_ROOT/build" && sh build_zis.sh ) || { echo "BUILD FAILED - fix before continuing"; exit 1; }
# assert the build actually produced the module (shell-returnable, no SDSF):
if tsocmd "LISTDS '$ZIS_LOADLIB' MEMBERS" 2>&1 | grep -q "ZWESIS01"; then
  echo "   [ok] ZWESIS01 present in $ZIS_LOADLIB"
else
  echo "   [FAIL] ZWESIS01 not in $ZIS_LOADLIB - build did not produce it"; exit 1
fi

# ---------------------------------------------------------------------------
echo; echo "## 3. parmlib member (plugins commented out -> no AUX address space needed) ##"
cp "$SAMP/ZWESIP00" "//'${ZIS_PARMLIB}(ZWESIP${MEM})'"
echo "   wrote ${ZIS_PARMLIB}(ZWESIP${MEM})"

# ---------------------------------------------------------------------------
echo; echo "## 4. Generate the ZIS server JOB (member ZISRUN) ##"
zis_writemember "${ZIS_JCLLIB}(ZISRUN)" <<EOJ
//${ZIS_JOB} JOB ${ZIS_JOBACCT}
//*  Private test ZIS cross-memory server, run as a JOB (not a started task).
//*  SYSPRINT stays SYSOUT=*. Do NOT point it at a dataset: ZWESIS01's own DCB
//*  conflicts with a hand-coded RECFM/LRECL and OPEN fails S013-68 before the
//*  server does anything (measured 2026-10-02). The job log, SYSPRINT included,
//*  is retrievable because ZIS_JOBACCT carries a HELD MSGCLASS - see zis-env.sh.
//ZWESIS01 EXEC PGM=ZWESIS01,PARM='NAME=${ZIS_NAME},MEM=${MEM}'
//STEPLIB  DD DISP=SHR,DSN=${ZIS_LOADLIB}
//PARMLIB  DD DISP=SHR,DSN=${ZIS_PARMLIB}
//SYSPRINT DD SYSOUT=*
EOJ
echo "   wrote ${ZIS_JCLLIB}(ZISRUN)  (job ${ZIS_JOB}, PGM=ZWESIS01, NAME=${ZIS_NAME})"

# ---------------------------------------------------------------------------
echo; echo "## 5. APF-authorize the loadlib (AC(1) modules need an APF library) ##"
# Delegated to zis-apf.sh, which is also what you re-run after an IPL (SETPROG
# APF is not permanent). Keeping one copy of the SMS-vs-VOLUME logic.
sh "$HERE/zis-apf.sh"

# ---------------------------------------------------------------------------
echo; echo "## 6. PPT (KEY 4, NON-SWAPPABLE - by program name, system-wide) ##"
echo "   ZWESIS01 needs:  PPT PGMNAME(ZWESIS01) KEY(4) NOSWAP   (samplib/zis/ZWESISCH)"
echo "   Production ZIS already runs ZWESIS01, so this is almost certainly set already."
echo "   Verify:  D PPT   (look for ZWESIS01). If absent, add to SCHEDxx + SET SCH=xx."

# ---------------------------------------------------------------------------
echo; echo "## 7. RACF: grant your ZSS userid READ on ZWES.IS (the PC-call gate) ##"
# Ask what the access already IS before trying to change it. On a box with an
# existing Zowe install the profile is defined and owned by someone else, and the
# userid usually already holds READ, so the RDEFINE/PERMIT below are both
# unnecessary and refused. Reporting "NOT AUTHORIZED" three times and guessing at
# the cause (as this used to) hides the one fact that matters.
_rl="$(tsocmd "RLIST FACILITY ZWES.IS AUTHUSER" 2>&1)"
case "$_rl" in
  *"NOT FOUND"*|*"NOT DEFINED"*)
    echo "   ZWES.IS is NOT defined. Defining it and granting ${ZSS_USER} READ:"
    tsocmd "RDEFINE FACILITY ZWES.IS UACC(NONE)" 2>&1 | sed 's/^/     /'
    tsocmd "PERMIT ZWES.IS CLASS(FACILITY) ACCESS(READ) ID(${ZSS_USER})" 2>&1 | sed 's/^/     /'
    tsocmd "SETROPTS RACLIST(FACILITY) REFRESH" 2>&1 | sed 's/^/     /'
    echo "   If those say NOT AUTHORIZED, you lack RACF authority here: ask a"
    echo "   security administrator for READ on ZWES.IS in the FACILITY class."
    ;;
  *)
    echo "   ZWES.IS is already defined. Your access:"
    echo "$_rl" | sed -n '/YOUR ACCESS/,+2p' | sed 's/^/     /'
    case "$_rl" in
      *READ*|*UPDATE*|*ALTER*|*CONTROL*)
        echo "   -> sufficient; nothing to change." ;;
      *)
        echo "   -> NOT sufficient. ${ZSS_USER} needs READ on ZWES.IS; ask a security"
        echo "      administrator, since defining and permitting need authority you"
        echo "      may not hold." ;;
    esac
    ;;
esac
echo "   (the ZIS job runs under YOUR userid, so no STARTED profile is needed)"

echo; echo "############ Done.  Next: sh zis-start.sh  then  sh zis-status.sh ############"
echo "Then point ZSS at it (zowe.yaml crossMemoryServerName: ${ZIS_NAME}) and restart ZSS;"
echo "success = ZSS logs  ZWES1014I ZIS status 'Ok' ... cmsRC='0'."
