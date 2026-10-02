#!/bin/sh
# configure-test-env.sh - generate the site-specific bits of the test harness.
#
#   sh test-support/configure-test-env.sh          # show what it would use, then do it
#   ZSS_TEST_PORT=17600 sh test-support/configure-test-env.sh
#
# Idempotent. Creates:
#   zowe.yaml           from zowe.yaml.template, substituting test-env.sh values
#   $ZSS_TEST_INST/...  the logs, plugins, product and instance directories
#
# Both are git-ignored: they are site state, not source. Edit
# zowe.yaml.template and re-run, or export a different value and re-run.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ZSS_TEST_SUPPORT="$HERE"; . "$HERE/test-env.sh"

echo "=== configuration this will use ==="
zss_test_show_env
echo ""

T="$HERE/zowe.yaml.template"
Y="$HERE/zowe.yaml"
[ -f "$T" ] || { echo "ERROR: $T not found"; exit 1; }

if [ -f "$Y" ] && [ "${1:-}" != "--force" ]; then
  echo "$Y already exists; leaving it alone."
  echo "  Regenerate with:  sh $0 --force"
else
  # sed with a | delimiter: the values are paths and contain /.
  sed -e "s|@INST@|$ZSS_TEST_INST|g" \
      -e "s|@PORT@|$ZSS_TEST_PORT|g" \
      -e "s|@ADDR@|$ZSS_TEST_ADDR|g" \
      -e "s|@KEYRING@|$ZSS_TEST_KEYRING|g" \
      -e "s|@LABEL@|$ZSS_TEST_LABEL|g" \
      "$T" > "$Y.tmp" || { echo "ERROR: substitution failed"; rm -f "$Y.tmp"; exit 1; }
  if grep -q '@[A-Z][A-Z]*@' "$Y.tmp"; then
    echo "ERROR: unsubstituted placeholders remain:"
    grep -n '@[A-Z][A-Z]*@' "$Y.tmp" | sed 's/^/     /'
    rm -f "$Y.tmp"; exit 1
  fi
  mv "$Y.tmp" "$Y"
  echo "wrote $Y"
fi

for d in logs plugins product instance; do
  if [ -d "$ZSS_TEST_INST/$d" ]; then echo "  [exists] $ZSS_TEST_INST/$d"
  else mkdir -p "$ZSS_TEST_INST/$d" && echo "  [create] $ZSS_TEST_INST/$d"; fi
done

echo ""
echo "=== next ==="
echo "1. TLS identity (once):   sh $HERE/provision-keyring-cert.sh"
echo "2. ZIS (needed for ANY authenticated test - see ../dev-test-deploy runbook):"
echo "     sh $HERE/zis/zis-configure.sh     # once"
echo "     sh $HERE/zis/zis-apf.sh           # after every IPL"
echo "     sh $HERE/zis/zis-start.sh"
echo "3. The server:            sh $HERE/start-zss.sh"
echo "   success = ZWES1014I ZIS status - 'Ok' ... cmsRC='0'"
