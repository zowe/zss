#!/usr/bin/env node
'use strict';
/*
 * zsstest-run.js - scriptable assertion runner for ZSS webservice tests.
 *
 * Runs a suite of HTTP cases against a live ZSS and reports pass/fail in a
 * TAP-like format. Node built-ins only. The same suite runs against any tier:
 * the WSL sandbox (test-support/wsl/run.sh), a single-user z/OS ZSS, or a
 * multi-user TLS ZSS - only the environment differs.
 *
 *   ZSS_URL=http://127.0.0.1:8544 ZSS_USER=x ZSS_PASS=x \
 *     node zsstest-run.js suites/unixfile-charset.js
 *
 * env: ZSS_URL  base url (http: or https:)
 *      ZSS_USER/ZSS_PASS  basic auth (suite cases may opt out with auth:false)
 *      ZSS_CA   PEM file to verify TLS; unset = verification disabled (test box)
 *      ZSS_TEST_DIR  server-side directory suites use to locate test files
 *
 * A suite exports { name, cases: [...] }; each case:
 *   { name, path,            // request path; ${DIR} expands to ZSS_TEST_DIR
 *     query,                 // optional query string
 *     auth,                  // default true; false = send no Authorization
 *     expect: {
 *       status,              // exact HTTP status
 *       nonEmpty,            // decoded body length > 0
 *       decodedHex,          // decoded body equals this hex string exactly
 *       decodedAscii,        // decoded body equals this ASCII string exactly
 *       hexIncludes,         // array of hex substrings the decoded body contains
 *       hexExcludes,         // array of hex substrings it must NOT contain
 *     } }
 * /unixfile success bodies are whole-body base64; the runner decodes them and
 * asserts against the DECODED bytes (shown as hex on failure - never raw).
 */
const fs = require('fs');
const path = require('path');

const base = process.env.ZSS_URL || 'http://127.0.0.1:8544';
const user = process.env.ZSS_USER || '';
const pass = process.env.ZSS_PASS || '';
const caFile = process.env.ZSS_CA;
const testDir = process.env.ZSS_TEST_DIR || '';

const suiteFile = process.argv[2];
if (!suiteFile) {
  console.error('usage: node zsstest-run.js <suite.js>');
  process.exit(2);
}
const suite = require(path.resolve(suiteFile));

function request(caze) {
  const q = caze.query ? '?' + caze.query : '';
  const p = caze.path.replace('${DIR}', testDir);
  const u = new URL(base + p + q);
  const lib = u.protocol === 'https:' ? require('https') : require('http');
  const headers = {};
  if (caze.auth !== false && (user || pass)) {
    headers.Authorization = 'Basic ' + Buffer.from(user + ':' + pass).toString('base64');
  }
  const opts = {
    method: 'GET', hostname: u.hostname, port: u.port,
    path: u.pathname + u.search, headers: headers,
    rejectUnauthorized: !!caFile,
  };
  if (caFile) opts.ca = fs.readFileSync(caFile);
  return new Promise(function (resolve, reject) {
    const req = lib.request(opts, function (res) {
      const chunks = [];
      res.on('data', function (d) { chunks.push(d); });
      res.on('end', function () { resolve({ status: res.statusCode, raw: Buffer.concat(chunks) }); });
    });
    req.on('error', reject);
    req.end();
  });
}

/* /unixfile success = whole-body ASCII base64; decode when it looks like it. */
function decodeBody(raw) {
  const s = raw.toString('ascii').trim();
  if (s.length && /^[A-Za-z0-9+/=\s]+$/.test(s)) {
    try { return Buffer.from(s, 'base64'); } catch (e) { /* fall through */ }
  }
  return raw;
}

function check(caze, res) {
  const e = caze.expect || {};
  const body = decodeBody(res.raw);
  const hex = body.toString('hex');
  const problems = [];
  if (e.status !== undefined && res.status !== e.status) {
    problems.push('status ' + res.status + ' != ' + e.status);
  }
  if (e.nonEmpty && body.length === 0) problems.push('decoded body is empty');
  if (e.decodedHex !== undefined && hex !== e.decodedHex) {
    problems.push('decoded hex mismatch: got ' + hex);
  }
  if (e.decodedAscii !== undefined && body.toString('latin1') !== e.decodedAscii) {
    problems.push('decoded text mismatch: got hex ' + hex);
  }
  (e.hexIncludes || []).forEach(function (h) {
    if (hex.indexOf(h) < 0) problems.push('missing hex ' + h + ' in ' + hex);
  });
  (e.hexExcludes || []).forEach(function (h) {
    if (hex.indexOf(h) >= 0) problems.push('forbidden hex ' + h + ' present');
  });
  return problems;
}

async function main() {
  console.log('# suite: ' + suite.name + '   target: ' + base);
  console.log('1..' + suite.cases.length);
  let fails = 0;
  let xfails = 0;
  for (let i = 0; i < suite.cases.length; i++) {
    const caze = suite.cases[i];
    let problems;
    try {
      problems = check(caze, await request(caze));
    } catch (err) {
      problems = ['request failed: ' + err.message];
    }
    // xfail: a case that reproduces a known-open bug. A failure is EXPECTED
    // (reported as a TAP TODO, not a suite failure) until the bug is fixed; if
    // it unexpectedly PASSES (xpass) that IS a suite failure, so the repro gets
    // removed once the fix lands. Set caze.xfail to a short reason/issue ref.
    var xfail = caze.xfail;
    if (problems.length === 0) {
      if (xfail) {
        fails++;
        console.log('ok ' + (i + 1) + ' - ' + caze.name + ' # XPASS -- ' + xfail + ' (bug may be fixed; remove xfail)');
      } else {
        console.log('ok ' + (i + 1) + ' - ' + caze.name);
      }
    } else {
      if (xfail) {
        xfails++;
        console.log('not ok ' + (i + 1) + ' - ' + caze.name + ' # TODO xfail: ' + xfail);
        problems.forEach(function (p) { console.log('#   ' + p); });
      } else {
        fails++;
        console.log('not ok ' + (i + 1) + ' - ' + caze.name);
        problems.forEach(function (p) { console.log('#   ' + p); });
      }
    }
  }
  var passed = suite.cases.length - fails - xfails;
  console.log('# ' + passed + '/' + suite.cases.length + ' passed'
              + (xfails ? ', ' + xfails + ' xfail (known bugs)' : '')
              + (fails ? ', ' + fails + ' FAILED' : ''));
  process.exit(fails ? 1 : 0);
}

main();
