#!/bin/sh
# zis-env.sh - shared parameters + helpers for the test-ZIS lifecycle scripts.
# Source it:   . ./zis-env.sh        (run these ON z/OS under /bin/sh)
#
# MODEL: our test ZIS runs as a PRIVATE, SHORT-LIVED BATCH JOB (not a started task).
# So it's per-dev, self-provisioned (<userid>.* datasets), needs no PROCLIB concat and
# no STARTED profile, and is ideal for testing changes to ZIS itself
# (edit ZIS source -> build_zis.sh -> resubmit). Deliberately distinct from the shared
# production ZWESIS_STD (jobname ZWE1SZ). Assumes the invoker holds RACF + operator authority.

# Force NATIVE z/OS tools first on PATH (sed/cat/tsocmd/xlc/ld). These scripts process
# EBCDIC samplib members + MVS datasets; the zopen (ASCII) tools that env.sh puts first
# would mangle them. This makes the toolkit work from ANY shell - a teammate need not
# know the env.sh/zopen charset gotcha. (env.sh tools remain available as a fallback.)
PATH="/bin:/usr/bin${PATH:+:$PATH}"; export PATH

# Shared site values live one level up, so there is a single place to edit.
# Guarded: these scripts are also usable standalone.
_up="$(cd "$(dirname "$0")/.." 2>/dev/null && pwd)"
[ -n "$_up" ] && [ -f "$_up/test-env.sh" ] && { ZSS_TEST_SUPPORT="$_up"; . "$_up/test-env.sh"; }

# ---- identity ----
ZIS_NAME="${ZIS_NAME:-ZWESIS_TST}"     # cross-memory server NAME; MUST equal
                                       # components.zss.crossMemoryServerName in zowe.yaml.
                                       # System-wide - make it unique if >1 run concurrently.
MEM="${MEM:-00}"                       # parmlib member suffix -> ZWESIP${MEM}

# ---- current userid (uppercased; matches build_zis.sh's $USER) ----
ZIS_USERID="${ZIS_USERID:-${ZSS_TEST_USERID:-${USER:-$(id -un)}}}"
ZIS_USERID="$(echo "$ZIS_USERID" | tr '[:lower:]' '[:upper:]')"
ZSS_USER="${ZSS_USER:-$ZIS_USERID}"    # ZSS userid that must hold ZWES.IS READ

# OUR ZIS job name (<=8). It MUST be the userid plus at least one character:
# TSO STATUS and CANCEL refuse any other name ("JOBNAME MUST BE YOUR USERID PLUS AT
# LEAST ONE CHARACTER"), so a job called ZWESISTS would start and then be impossible
# to stop or query from here. Measured on Marist 2026-10-02. Defined after
# ZIS_USERID because it is derived from it.
ZIS_JOB="${ZIS_JOB:-${ZIS_USERID}Z}"

# Separate name for the throwaway operator-command job. It must ALSO be userid +
# at least one character, and an MVS job name is at most 8, so it cannot be
# derived by appending to ZIS_JOB: a 7-character userid gives 8 + 1 = 9, and
# JES rejects the card.
ZIS_CMDJOB="${ZIS_CMDJOB:-${ZIS_USERID}C}"

# Extended-console name for that job. MUST NOT be the bare userid: TSO CONSOLE
# defaults the name to the userid, and SDSF activates a console under that same
# name, so a batch job competing with your own SDSF session gets
#   MCSOPER RETURN CODE X'00000004', REASON X'00000000'   (name already in use)
# which looks exactly like an authority failure and is not. Measured 2026-10-02:
# the same job succeeded when no SDSF session was open and failed while one was.
# 2-8 chars, must start alphabetic or #/$/@.
ZIS_CONSNAME="${ZIS_CONSNAME:-${ZIS_USERID}K}"

# ---- datasets (all <userid>.*, self-service) ----
ZIS_LOADLIB="${ZIS_LOADLIB:-${ZIS_USERID}.DEV.LOADLIB}"     # APF; build_zis.sh binds here
ZIS_PARMLIB="${ZIS_PARMLIB:-${ZIS_USERID}.ZWESIS.PARMLIB}"  # member ZWESIP${MEM}
ZIS_JCLLIB="${ZIS_JCLLIB:-${ZIS_USERID}.ZWESIS.JCL}"        # PDS: ZISRUN (server job) + ZISCMD scratch
ZIS_OUT="${ZIS_OUT:-${ZIS_USERID}.ZWESIS.ZISOUT}"           # server SYSPRINT (a dataset, so no SDSF needed)

ZSS_ROOT="${ZSS_ROOT:-$(cd "$(dirname "$0")/../.." 2>/dev/null && pwd)}"

# JOB statement accounting (EDIT for your shop). The Marist convention is a bare
# "JOB 1" - the accounting field is just "1".
# REGION=0M + TIME=NOLIMIT are NOT optional for the server: ZWESIS01 as a JOB runs
# forever, so a default job-class CPU limit would S322 it (an STC has no such limit).
# MSGCLASS=H is a HELD class on Marist, which is what makes the job log readable
# from here: TSO OUTPUT can only retrieve HELD output, and without it a failure is
# invisible unless you have SDSF. Verified 2026-10-02.
ZIS_JOBACCT="${ZIS_JOBACCT:-${ZSS_TEST_JOBACCT:-1},REGION=0M,TIME=NOLIMIT,MSGCLASS=${ZSS_TEST_MSGCLASS:-H}}"

# ---- helper: pull a job's log back into a dataset and print it ----
# Needs the job to have run with a HELD MSGCLASS (see ZIS_JOBACCT).
zis_joblog() {   # $1 = jobname  $2 = jobid (e.g. JOB03658)
  _jo="${ZIS_JCLLIB%.JCL}.JOBOUT"
  tsocmd "DELETE '$_jo'" >/dev/null 2>&1
  tsocmd "OUTPUT $1($2) PRINT('$_jo') KEEP" 2>&1 | sed 's/^/   /'
  cat "//'$_jo'" 2>/dev/null
}

# ---- helper: write stdin to a PDS member ----
# '> //dsn(member)' SILENTLY no-ops on z/OS USS (rc=0, no member written); cp from a temp
# USS file works. Use this for every member write.
zis_writemember() {   # $1 = dsn(member) ; content on stdin
  _t="/tmp/zisw.$$"; cat > "$_t"; cp "$_t" "//'$1'"; rm -f "$_t"
}

# ---- helper: issue an MVS operator command ----
#
# NOT via a JCL "// COMMAND" statement. Measured on Marist 2026-10-02: a job
# carrying one sits in WAITING FOR EXECUTION forever and never runs, while the
# identical job without it completes in seconds. It fails silently: SUBMIT
# reports success and nothing ever happens.
#
# Instead run TSO CONSOLE under IKJEFT01 in a batch job and capture SYSTSPRT to a
# dataset so the reply can be read back here. CONSOLE ACTIVATE is accepted for
# this userid. The job name is ${ZIS_CMDJOB} so TSO STATUS/CANCEL can manage it.
zis_opercmd() {
  _c="$1"; echo ">> MVS command: $_c"
  _out="${ZIS_JCLLIB%.JCL}.CONOUT"
  tsocmd "DELETE '$_out'" >/dev/null 2>&1
  zis_writemember "${ZIS_JCLLIB}(ZISCMD)" <<EOJ
//${ZIS_CMDJOB} JOB ${ZIS_JOBACCT}
//TSO     EXEC PGM=IKJEFT01,DYNAMNBR=20
//SYSTSPRT DD DSN=$_out,DISP=(NEW,CATLG),
//            SPACE=(TRK,(2,2)),DCB=(RECFM=FB,LRECL=137,BLKSIZE=1370)
//SYSTSIN  DD *
CONSPROF SOLDISPLAY(YES) SOLNUM(100)
CONSOLE ACTIVATE NAME(${ZIS_CONSNAME})
CONSOLE SYSCMD(-
${_c})
CONSOLE DEACTIVATE
/*
EOJ
  tsocmd "SUBMIT '${ZIS_JCLLIB}(ZISCMD)'" 2>&1
  echo "   submitted as ${ZIS_CMDJOB}; reply (if any) lands in $_out"
  echo "   read it with:  cat \"//'$_out'\""
}

# ---- true (rc 0) if the last zis_opercmd could NOT issue the command ----
# Two ways it fails here, both seen on Marist 2026-10-02:
#   console NAME already in use  -> "MCSOPER RETURN CODE" (see ZIS_CONSNAME)
#   SYSTSIN line past column 72  -> "TERMINATING PARENTHESIS" (truncated command)
zis_opercmd_failed() {
  _out="${ZIS_JCLLIB%.JCL}.CONOUT"
  case "$(cat "//'$_out'" 2>/dev/null)" in
    *"MCSOPER RETURN CODE"*|*"TERMINATING PARENTHESIS"*|*"COULD NOT BE PARSED"*|*"NOT ACTIVE"*) return 0 ;;
    *) return 1 ;;
  esac
}

zis_show_env() {
  echo "ZIS_NAME=$ZIS_NAME (cross-memory)  ZIS_JOB=$ZIS_JOB (MVS job)  userid=$ZIS_USERID"
  echo "loadlib=$ZIS_LOADLIB  parmlib=$ZIS_PARMLIB(ZWESIP${MEM})  jcllib=$ZIS_JCLLIB"
  echo "ZSS_USER (needs ZWES.IS READ)=$ZSS_USER"
}

# true (rc 0) if the dataset is cataloged
zis_dsn_exists() {
  case "$(tsocmd "LISTDS '$1'" 2>&1)" in
    *"NOT IN CATALOG"*|*"NOT FOUND"*|*"INVALID DATA SET NAME"*) return 1 ;;
    *) return 0 ;;
  esac
}
# create a dataset only if absent (idempotent)
zis_provision() {   # $1=dsn  $2=ALLOCATE attrs
  if zis_dsn_exists "$1"; then echo "   [exists ] $1"
  # ALLOC NEW CATALOG persists the dataset; the ddname frees when this tsocmd's TSO ends (no FREE needed):
  else echo "   [create ] $1"; tsocmd "ALLOC FI(ZTPROV) DA('$1') NEW CATALOG $2" 2>&1; fi
}
