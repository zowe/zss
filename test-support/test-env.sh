#!/bin/sh
# test-env.sh - the ONE place that knows anything site-specific.
#
# Source it:   . "$(dirname "$0")/test-env.sh"
#
# Everything else in test-support/ derives from these, so porting this toolkit
# to another system or another userid is editing this file (or exporting the
# same names) and nothing else. Every value defaults to something sensible for
# the current userid, so on a system laid out like ours it needs no editing at
# all.
#
# WHY THIS EXISTS: the first version of these scripts had one developer's
# paths and userid scattered through six files plus a YAML, which made them
# that developer's scripts rather than the squad's test infrastructure.

# ---- who ----
# Uppercased: MVS wants it that way, and dataset and job names are built from it.
ZSS_TEST_USERID="${ZSS_TEST_USERID:-${USER:-$(id -un)}}"
ZSS_TEST_USERID="$(echo "$ZSS_TEST_USERID" | tr '[:lower:]' '[:upper:]')"

# ---- where ----
# The zss repo clone. Derived from this file's location, so a clone anywhere works.
# $0 is the CALLER when this file is sourced, and is "-sh" when sourced from an
# interactive shell, so dirname "$0" is not reliable here. Callers that know
# where they are should set ZSS_TEST_SUPPORT first; the fallbacks cover the rest.
if [ -z "${ZSS_TEST_SUPPORT:-}" ]; then
  if [ -f "$(dirname "$0" 2>/dev/null)/test-env.sh" ]; then
    ZSS_TEST_SUPPORT="$(cd "$(dirname "$0")" && pwd)"
  elif [ -f "./test-env.sh" ]; then
    ZSS_TEST_SUPPORT="$(pwd)"
  else
    echo "test-env.sh: cannot locate test-support/; set ZSS_TEST_SUPPORT" >&2
    return 1 2>/dev/null || exit 1
  fi
fi
ZSS_ROOT="${ZSS_ROOT:-$(cd "$ZSS_TEST_SUPPORT/.." 2>/dev/null && pwd)}"

# Scratch instance tree: logs, plugins, product, instance. Created on demand.
# NOT inside the repo, so a git clean never destroys a running server's logs.
ZSS_TEST_INST="${ZSS_TEST_INST:-$HOME/zsstest}"

# ---- the test server ----
# 17557, deliberately NOT 7557: that is the shared production ZSS on our box and
# the drivers refuse it without --force. On a shared CI runner give each worker
# its own port.
ZSS_TEST_PORT="${ZSS_TEST_PORT:-17557}"
ZSS_TEST_ADDR="${ZSS_TEST_ADDR:-127.0.0.1}"

# TLS identity, created by provision-keyring-cert.sh.
ZSS_TEST_RING="${ZSS_TEST_RING:-ZWESRING}"
ZSS_TEST_LABEL="${ZSS_TEST_LABEL:-ZOWECERT}"
ZSS_TEST_KEYRING="${ZSS_TEST_KEYRING:-${ZSS_TEST_USERID}/${ZSS_TEST_RING}}"

# ---- batch ----
# MSGCLASS must be a HELD class on this system: TSO OUTPUT can only retrieve
# held output, and without it a job failure is invisible without SDSF. H is
# held on Marist; check yours with your sysprog or by submitting a job and
# trying OUTPUT on it.
ZSS_TEST_MSGCLASS="${ZSS_TEST_MSGCLASS:-H}"
# Accounting field. Bare "1" is the Marist convention.
ZSS_TEST_JOBACCT="${ZSS_TEST_JOBACCT:-1}"

# ---- node ----
# The drivers are Node with built-in modules only. If node is not already on
# PATH, say where it is.
ZSS_TEST_NODE_BIN="${ZSS_TEST_NODE_BIN:-}"
if [ -n "$ZSS_TEST_NODE_BIN" ]; then PATH="$ZSS_TEST_NODE_BIN:$PATH"; export PATH; fi

export ZSS_TEST_USERID ZSS_TEST_SUPPORT ZSS_ROOT ZSS_TEST_INST \
       ZSS_TEST_PORT ZSS_TEST_ADDR ZSS_TEST_RING ZSS_TEST_LABEL \
       ZSS_TEST_KEYRING ZSS_TEST_MSGCLASS ZSS_TEST_JOBACCT

# ---- site override ----
# test-env.local.sh is git-ignored and sourced LAST, so a site can pin anything
# above without editing a tracked file. Use it when your instance lives
# somewhere other than the committed default of \$HOME/zsstest.
_local="${ZSS_TEST_SUPPORT}/test-env.local.sh"
[ -f "$_local" ] && . "$_local"

zss_test_show_env() {
  echo "userid      $ZSS_TEST_USERID"
  echo "zss repo    $ZSS_ROOT"
  echo "instance    $ZSS_TEST_INST"
  echo "listener    https://$ZSS_TEST_ADDR:$ZSS_TEST_PORT"
  echo "keyring     $ZSS_TEST_KEYRING  label $ZSS_TEST_LABEL"
  echo "job msgcl   $ZSS_TEST_MSGCLASS (must be a HELD class)"
}
