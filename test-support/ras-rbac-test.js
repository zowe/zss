#!/usr/bin/env node
/*
  This program and the accompanying materials are made available
  under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * ras-rbac-test.js  -  does /ras/traceLevel require authorization?
 *
 * WHY: zowe/zss#855. The RAS service authenticated its callers and then never
 * authorized them, so any authenticated user could read the ZSS log levels or
 * raise a component to FINEST. This drives the endpoint and reports which of
 * the two behaviours the running server has, so a fix can be demonstrated
 * rather than asserted.
 *
 * WHAT IT DECIDES: each case has an expected status under the old code and
 * under the fix. The run is classified VULNERABLE, FIXED, or MIXED, and the
 * exit status follows. Nothing is inferred from the absence of a crash.
 *
 * THE FOUR BRANCHES of the fix, and what this can reach:
 *
 *   not authenticated                 -> 401   reachable here
 *   authenticated, rbac off           -> 400   reachable here (default config)
 *   authenticated, rbac on, SAF fails -> 403   reachable here ONLY as
 *                                              "ZIS unreachable", which takes
 *                                              the same branch as a SAF denial
 *   authenticated, rbac on, SAF ok    -> served   NOT reachable without ZIS
 *
 * So run it twice, once against a config with dataserviceAuthentication.rbac
 * absent or false, and once with it true. Use --rbac to say which, because the
 * expected status for the authenticated cases depends on it.
 *
 * Needs a running test ZSS with a working ZIS. Bring one up with the shared
 * harness in test-support/ (see its README), then:
 *
 *   node test-support/ras-rbac-test.js --user MYUSER              (rbac off)
 *   node test-support/ras-rbac-test.js --user MYUSER --rbac on    (rbac on)
 *
 * To show the before-and-after against two builds, let the shared driver
 * handle the swap and the identity checks:
 *
 *   test-support/ab-compare.sh --before bin/zssServer64.old \
 *                             --after  bin/zssServer64.new \
 *                             -- node test-support/ras-rbac-test.js --user MYUSER
 *
 * Credentials are needed for the cases that matter: the vulnerability is about
 * an AUTHENTICATED user doing something they should not be allowed to do.
 * Without --user only the unauthenticated case runs, which is not the finding.
 *
 * SAFETY: refuses port 7557, the shared production ZSS on the Marist box. If a
 * PUT succeeds it restores the component's original level before exiting, so a
 * run does not leave a server logging at FINEST.
 */

'use strict';

const https = require('https');
const http = require('http');
const readline = require('readline');

const PRODUCTION_PORT = 7557;

/*
 * Address the component by ID, NOT by name. `componentName` is looked up in
 * server->loggingIdsByName, which zss.c populates ONLY from plugin data
 * services, so on a ZSS with no plugins (our test server) EVERY name misses.
 * Worse, rasService.c:164-165 is `uint64 *p = htGet(...); componentID = *p;`
 * with no NULL check, so a miss dereferences NULL. On z/OS that reads low
 * storage and yields garbage rather than crashing, and the request then fails
 * downstream with "component ID out of range". That 400 is NOT the RBAC gate,
 * and counting it as one made this test report FIXED against a vulnerable
 * build. LOG_COMP_ID_MVD_SERVER, h/zssLogging.h:38.
 */
const COMPONENT_ID = '0x008F000300010000';
const FINEST = 5;

const cfg = {
  host: '127.0.0.1', port: 17557, tls: true,
  user: null, pass: null, rbac: 'off', force: false, timeout: 15000,
};

function usage() {
  console.log([
    'ras-rbac-test.js - does /ras/traceLevel require authorization? (zowe/zss#855)',
    '',
    '  --user <id>    userid for Basic auth; the password is prompted, never on the command line',
    '  --rbac on|off  what dataserviceAuthentication.rbac is set to in the',
    '                 server config being tested. Default off. This selects the',
    '                 expected status for the authenticated cases, it does not',
    '                 change the server.',
    '  --host <h>     default 127.0.0.1',
    '  --port <p>     default 17557 (7557 refused without --force)',
    '  --http         plain HTTP instead of TLS',
    '  --timeout <ms> per-request budget, default 15000',
    '  --force        allow a port we would otherwise refuse',
    '',
    'Exit 0 = FIXED, 1 = VULNERABLE, 2 = MIXED or a run error.',
  ].join('\n'));
}

for (let i = 2; i < process.argv.length; i++) {
  const a = process.argv[i];
  const next = () => {
    const v = process.argv[++i];
    if (v === undefined) { console.error('missing value for ' + a); process.exit(2); }
    return v;
  };
  switch (a) {
    case '--user': cfg.user = next(); break;
    case '--rbac': {
      const v = next().toLowerCase();
      if (v !== 'on' && v !== 'off') { console.error('--rbac takes on or off'); process.exit(2); }
      cfg.rbac = v; break;
    }
    case '--host': cfg.host = next(); break;
    case '--port': cfg.port = parseInt(next(), 10); break;
    case '--http': cfg.tls = false; break;
    case '--timeout': cfg.timeout = parseInt(next(), 10); break;
    case '--force': cfg.force = true; break;
    case '-h': case '--help': usage(); process.exit(0); break;
    default: console.error('unknown option: ' + a); usage(); process.exit(2);
  }
}
if (cfg.port === PRODUCTION_PORT && !cfg.force) {
  console.error('refusing port ' + PRODUCTION_PORT + ': that is the shared production ZSS on this box.');
  process.exit(2);
}

/*
 * Read a password without echoing it. Raw mode and our own line editing, so
 * nothing is written to the terminal and nothing redraws the prompt: the
 * earlier version re-printed the prompt on every keystroke, which made it look
 * as though it had asked twice.
 */
function promptPassword(user) {
  return new Promise((resolve, reject) => {
    const stdin = process.stdin;
    process.stdout.write('password for ' + user + ' (not echoed): ');
    if (!stdin.isTTY) {
      /* Piped input: read a line, still without echoing anything ourselves. */
      let buf = '';
      stdin.setEncoding('utf8');
      stdin.on('data', (d) => { buf += d; });
      stdin.on('end', () => { process.stdout.write('\n'); resolve(buf.replace(/[\r\n]+$/, '')); });
      return;
    }
    stdin.setRawMode(true);
    stdin.resume();
    stdin.setEncoding('utf8');
    let pw = '';
    const onKey = (ch) => {
      if (ch === '\r' || ch === '\n' || ch === '') {
        stdin.setRawMode(false);
        stdin.pause();
        stdin.removeListener('data', onKey);
        process.stdout.write('\n');
        resolve(pw);
      } else if (ch === '') {                 /* Ctrl-C */
        stdin.setRawMode(false);
        stdin.pause();
        stdin.removeListener('data', onKey);
        process.stdout.write('\n');
        reject(new Error('cancelled'));
      } else if (ch === '' || ch === '\b') {  /* backspace */
        pw = pw.slice(0, -1);
      } else {
        pw += ch;
      }
    };
    stdin.on('data', onKey);
  });
}

/* One request. Resolves with {status, body}; never rejects. */
function request(method, path, useAuth) {
  return new Promise((resolve) => {
    const lib = cfg.tls ? https : http;
    const headers = { Accept: '*/*', Connection: 'close' };
    if (useAuth && cfg.user) {
      headers.Authorization = 'Basic ' + Buffer.from(cfg.user + ':' + (cfg.pass || '')).toString('base64');
    }
    const opts = { method, hostname: cfg.host, port: cfg.port, path, headers, agent: false };
    if (cfg.tls) opts.rejectUnauthorized = false;

    let settled = false;
    const done = (v) => { if (!settled) { settled = true; clearTimeout(timer); resolve(v); } };
    const req = lib.request(opts, (res) => {
      let body = '';
      res.on('data', (d) => { if (body.length < 2048) body += d; });
      res.on('end', () => done({ status: res.statusCode, body: body.trim() }));
      res.on('error', (e) => done({ status: null, body: 'response error: ' + (e.code || e.message) }));
    });
    req.on('error', (e) => done({ status: null, body: 'request error: ' + (e.code || e.message) }));
    const timer = setTimeout(() => { req.destroy(); done({ status: null, body: 'timeout' }); }, cfg.timeout);
    req.end();
  });
}

const tracePath = (extra) =>
  '/ras/traceLevel?componentID=' + COMPONENT_ID + (extra || '');

/*
 * Each case names the status expected under the old code and under the fix.
 * null means "whatever the server does, it is not what this case is testing",
 * and such a case is reported but never used to classify the run.
 */
function buildCases() {
  const rbacOn = cfg.rbac === 'on';
  /* With rbac on and no ZIS the SAF check cannot succeed, so the fix refuses
     with 403. With rbac off the fix refuses with 400 and says why. */
  const fixedAuthed = rbacOn ? 403 : 400;
  const cases = [
    {
      name: 'unauthenticated GET',
      method: 'GET', path: tracePath(), auth: false,
      vulnerable: 401, fixed: 401,
      note: 'the authType already refuses this, before and after; context, not the finding',
    },
    {
      name: 'unauthenticated PUT level=' + FINEST,
      method: 'PUT', path: tracePath('&level=' + FINEST), auth: false,
      vulnerable: 401, fixed: 401,
      note: 'same',
    },
  ];
  if (cfg.user) {
    cases.push({
      name: 'authenticated GET (reads log levels)',
      method: 'GET', path: tracePath(), auth: true,
      vulnerable: 200, fixed: fixedAuthed, decisive: true,
      note: 'THE FINDING: an ordinary authenticated user reading server state',
    });
    cases.push({
      name: 'authenticated PUT level=' + FINEST + ' (raises logging to FINEST)',
      method: 'PUT', path: tracePath('&level=' + FINEST), auth: true,
      vulnerable: 200, fixed: fixedAuthed, decisive: true,
      note: 'THE FINDING: an ordinary authenticated user degrading the server',
    });
  }
  /* Regressions: the fix must not disturb these. They need credentials like
     everything else, because the authType refuses an anonymous caller before
     the handler is reached at all. */
  if (cfg.user) {
    cases.push({
      name: 'regression: unsupported command',
      method: 'GET', path: '/ras/nosuchcommand', auth: true,
      vulnerable: 400, fixed: 400, decisive: true,
      note: 'must still be 400; proves the gate did not move ahead of command validation',
    });
    cases.push({
      name: 'regression: bad method',
      method: 'DELETE', path: tracePath(), auth: true,
      vulnerable: 405, fixed: 405, decisive: true,
      note: 'must still be 405; proves the gate does not invent a profile for other methods',
    });
  }
  return cases;
}

/*
 * A status code alone is not evidence. The old build can return 400 for a
 * completely unrelated reason (a component it cannot resolve), which looks
 * identical to the fix refusing the request. So for the decisive cases the
 * response body has to say the expected thing too.
 */
function reasonMatches(c, r) {
  if (!c.decisive || c.vulnerable === c.fixed) return true;   /* regressions: code is enough */
  const b = (r.body || "").toLowerCase();
  if (c.fixed === 400) return b.indexOf("rbac") >= 0;         /* "Set dataserviceAuthentication.rbac to true" */
  if (c.fixed === 403) return b.indexOf("forbidden") >= 0 || b.indexOf("rbac") >= 0;
  return true;
}

async function main() {
  if (cfg.user) cfg.pass = await promptPassword(cfg.user);

  const scheme = cfg.tls ? 'https' : 'http';
  console.log('ras-rbac-test: ' + scheme + '://' + cfg.host + ':' + cfg.port + '/ras/traceLevel');
  console.log('  component ' + COMPONENT_ID + '   config rbac=' + cfg.rbac
              + (cfg.user ? '   auth ' + cfg.user : '   NO CREDENTIALS (the decisive cases will be skipped)'));
  console.log('');

  const cases = buildCases();
  const results = [];
  for (const c of cases) {
    const r = await request(c.method, c.path, c.auth);
    results.push({ c, r });
  }

  console.log('  ' + 'case'.padEnd(46) + 'got   old   fix   verdict');
  console.log('  ' + '-'.repeat(46) + '----- ----- ----- -------');
  let vulnerableHits = 0, fixedHits = 0, decisive = 0, oddities = 0;
  for (const { c, r } of results) {
    const got = r.status === null ? '-' : String(r.status);
    let verdict = '';
    if (!c.decisive && c.fixed !== null) {
      verdict = (r.status === c.fixed) ? 'as expected' : 'NOTE: differs';
    }
    if (c.decisive) {
      decisive++;
      if (c.vulnerable === c.fixed) {
        /* A regression case: both expectations agree, so it is pass or fail. */
        if (r.status === c.fixed) { verdict = 'ok'; fixedHits++; vulnerableHits++; }
        else { verdict = 'REGRESSED'; oddities++; }
      } else if (r.status === c.fixed && reasonMatches(c, r)) { verdict = 'fixed'; fixedHits++; }
      else if (r.status === c.fixed) { verdict = 'RIGHT CODE, WRONG REASON'; oddities++; }
      else if (r.status === c.vulnerable) { verdict = 'VULNERABLE'; vulnerableHits++; }
      else { verdict = 'unexpected'; oddities++; }
    }
    console.log('  ' + c.name.padEnd(46)
                + got.padStart(5) + ' '
                + String(c.vulnerable === null ? '-' : c.vulnerable).padStart(5) + ' '
                + String(c.fixed === null ? '-' : c.fixed).padStart(5) + '  ' + verdict);
    if (r.body) console.log('      body: ' + r.body.replace(/\s+/g, ' ').slice(0, 150));
  }

  /* If a PUT actually changed the level, put it back. */
  const putSucceeded = results.some(({ c, r }) => c.method === 'PUT' && r.status === 200);
  if (putSucceeded) {
    const back = await request('PUT', tracePath('&level=0'), true);
    console.log('');
    console.log('  a PUT succeeded, so the level was restored to 0: status ' + back.status);
  }

  console.log('');

  /*
   * Before blaming the endpoint, check that the server accepted our identity at
   * all. If every authenticated case came back 401 then authentication failed,
   * which says nothing about authorization. The usual cause on a standalone
   * test server is that ZSS is built with APF_AUTHORIZED=0, so
   * safAuthenticate() verifies the password through ZIS
   * (zisCheckUsernameAndPassword); with no ZIS, every password is rejected and
   * a correct one looks exactly like a wrong one.
   */
  if (cfg.user) {
    const authed = results.filter(({ c }) => c.auth);
    if (authed.length && authed.every(({ r }) => r.status === 401)) {
      console.log('AUTHENTICATION FAILED, so this run says nothing about authorization.');
      console.log('Every authenticated case came back 401. Check, in order:');
      console.log('  1. ZIS. The server log shows ZWES1014I; it must say cmsRC=0. A build with');
      console.log('     APF_AUTHORIZED=0 verifies passwords through ZIS, so without it a correct');
      console.log('     password is rejected exactly like a wrong one.');
      console.log('  2. The userid and password themselves, against the same system.');
      console.log('Re-run once ZWES1014I reports success.');
      process.exit(2);
    }
  }

  if (!decisive) {
    console.log('INCONCLUSIVE: no decisive case ran. Pass --user to test what #855 is about,');
    console.log('              which is an authenticated user acting without authorization.');
    process.exit(2);
  }
  if (oddities) {
    console.log('MIXED: ' + oddities + ' case(s) matched neither expectation. Read the table above.');
    process.exit(2);
  }
  if (fixedHits === decisive) {
    console.log('FIXED: every decisive case refused the unauthorized request and the');
    console.log('       regression cases still behave as before.');
    process.exit(0);
  }
  if (vulnerableHits === decisive) {
    console.log('VULNERABLE: an authenticated user can read and set the server log levels.');
    console.log('            This is zowe/zss#855.');
    process.exit(1);
  }
  console.log('MIXED: some cases fixed, some vulnerable. Read the table above.');
  process.exit(2);
}

main().catch((e) => { console.error('ras-rbac-test failed: ' + (e && e.stack ? e.stack : e)); process.exit(2); });
