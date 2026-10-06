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
 * THE BRANCHES of the fix, and what this can reach:
 *
 *   not authenticated                  -> 401  reachable here, though it is the
 *                                              service authType that refuses,
 *                                              not the RBAC gate
 *   authenticated, rbac off            -> 400  reachable here (default config)
 *   rbac on, SAF says no               -> 403  reachable here. SAF RC 8 is an
 *                                              explicit denial and SAF RC 4 is
 *                                              no covering profile; the gate
 *                                              treats both as not permitted,
 *                                              so a system with no ZLUX.*
 *                                              profiles defined reaches this
 *   rbac on, could not ask SAF         -> 500  ZIS unreachable, SAF abended, or
 *                                              ZIS refusing the ZOWE class.
 *                                              Reachable by stopping ZIS
 *   rbac on, SAF permits               -> served  needs the profile defined and
 *                                                 permitted, so it needs a
 *                                                 security administrator
 *
 * The 403 and 500 cases were one branch in the first draft of the fix, which
 * made a broken ZIS indistinguishable from a denial. They are separate now, so
 * "you are not permitted" and "I could not find out" no longer look alike.
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
 *   test-support/zss-test compare --before bin/zssServer64.old \
 *                                 --after  bin/zssServer64.new \
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

const https = require('node:https');
const http = require('node:http');

/* Keystrokes written as escapes. These were literal control bytes in the
   source, which no reviewer can see and no diff shows honestly. */
const KEY_ENTER = ['\r', '\n', '\u0004'];   /* return, newline, Ctrl-D */
const KEY_INTERRUPT = '\u0003';               /* Ctrl-C */
const KEY_ERASE = ['\u007f', '\b'];          /* DEL, backspace */

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

/*
 * Check what comes off the command line before it reaches a socket. A mistyped
 * port used to become NaN and the request went somewhere unintended, and a
 * hostname is held to the characters a hostname or address literal can contain,
 * so this driver cannot be aimed somewhere arbitrary by accident.
 */
function numberArg(name, raw, lo, hi) {
  const n = Number.parseInt(raw, 10);
  if (!Number.isInteger(n) || n < lo || n > hi) {
    console.error(name + ' takes an integer from ' + lo + ' to ' + hi + ', not "' + raw + '"');
    process.exit(2);
  }
  return n;
}

function hostArg(raw) {
  if (!/^[A-Za-z0-9.:_-]{1,253}$/.test(raw)) {
    console.error('--host takes a hostname or IP address, not "' + raw + '"');
    process.exit(2);
  }
  return raw;
}

/* Strip trailing CR and LF without a regex: /[\r\n]+$/ backtracks on a long
   line that does not end in one, and a password may legitimately end in a
   space, which trimEnd() would eat. */
function stripEol(str) {
  let end = str.length;
  while (end > 0 && (str[end - 1] === '\n' || str[end - 1] === '\r')) end--;
  return str.slice(0, end);
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
    case '--host': cfg.host = hostArg(next()); break;
    case '--port': cfg.port = numberArg('--port', next(), 1, 65535); break;
    case '--http': cfg.tls = false; break;
    case '--timeout': cfg.timeout = numberArg('--timeout', next(), 1, 600000); break;
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
      stdin.on('end', () => { process.stdout.write('\n'); resolve(stripEol(buf)); });
      return;
    }
    stdin.setRawMode(true);
    stdin.resume();
    stdin.setEncoding('utf8');
    let pw = '';
    const onKey = (ch) => {
      if (KEY_ENTER.includes(ch)) {
        stdin.setRawMode(false);
        stdin.pause();
        stdin.removeListener('data', onKey);
        process.stdout.write('\n');
        resolve(pw);
      } else if (ch === KEY_INTERRUPT) {                 /* Ctrl-C */
        stdin.setRawMode(false);
        stdin.pause();
        stdin.removeListener('data', onKey);
        process.stdout.write('\n');
        reject(new Error('cancelled'));
      } else if (KEY_ERASE.includes(ch)) {  /* backspace */
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
    cases.push(
      {
        name: 'authenticated GET (reads log levels)',
        method: 'GET', path: tracePath(), auth: true,
        vulnerable: 200, fixed: fixedAuthed, decisive: true,
        note: 'THE FINDING: an ordinary authenticated user reading server state',
      },
      {
        name: 'authenticated PUT level=' + FINEST + ' (raises logging to FINEST)',
        method: 'PUT', path: tracePath('&level=' + FINEST), auth: true,
        vulnerable: 200, fixed: fixedAuthed, decisive: true,
        note: 'THE FINDING: an ordinary authenticated user degrading the server',
      },
      /* Regressions: the fix must not disturb these. They need credentials like
         everything else, because the authType refuses an anonymous caller
         before the handler is reached at all. */
      {
        name: 'regression: unsupported command',
        method: 'GET', path: '/ras/nosuchcommand', auth: true,
        vulnerable: 400, fixed: 400, decisive: true,
        note: 'must still be 400; proves the gate did not move ahead of command validation',
      },
      {
        name: 'regression: bad method',
        method: 'DELETE', path: tracePath(), auth: true,
        vulnerable: 405, fixed: 405, decisive: true,
        note: 'must still be 405; proves the gate does not invent a profile for other methods',
      },
      {
        /* zss#890. Both builds answer 400, so only the reason separates them:
           the old one dereferences the NULL htGet miss, reads storage at
           address 0, and fails later with "component ID out of range". */
        name: 'unknown componentName (#890)',
        method: 'GET', path: '/ras/traceLevel?componentName=nosuchcomponent', auth: true,
        vulnerable: 400, fixed: 400, decisive: true,
        reason: 'unknown componentName',
        note: 'the NULL hashtable result must be refused, not dereferenced',
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
  const b = (r.body || '').toLowerCase();
  /* A case may name the text it expects, which is the only way to decide one
     where both builds answer with the same status code. */
  if (c.reason) return b.includes(c.reason.toLowerCase());
  if (!c.decisive || c.vulnerable === c.fixed) return true;   /* regressions: code is enough */
  if (c.fixed === 400) return b.includes('rbac');   /* "Set dataserviceAuthentication.rbac to true" */
  if (c.fixed === 403) return b.includes('forbidden') || b.includes('rbac');
  return true;
}

/*
 * What one case proves: the verdict text, and which tallies it feeds. Kept
 * apart from printing because this is the part that decides whether a run is
 * evidence at all, and it should be readable without the formatting around it.
 */
function classify(c, r) {
  if (!c.decisive) {
    if (c.fixed === null) return { verdict: '', fixed: 0, vulnerable: 0, oddity: 0 };
    const text = r.status === c.fixed ? 'as expected' : 'NOTE: differs';
    return { verdict: text, fixed: 0, vulnerable: 0, oddity: 0 };
  }
  /* Both expectations agree, so the code alone is pass or fail unless the case
     named a reason, in which case the body is what separates the two builds. */
  if (c.vulnerable === c.fixed) {
    if (r.status !== c.fixed) {
      return { verdict: 'REGRESSED', fixed: 0, vulnerable: 0, oddity: 1 };
    }
    if (c.reason && !reasonMatches(c, r)) {
      return { verdict: 'RIGHT CODE, WRONG REASON', fixed: 0, vulnerable: 1, oddity: 0 };
    }
    return { verdict: 'ok', fixed: 1, vulnerable: 1, oddity: 0 };
  }
  if (r.status === c.fixed && reasonMatches(c, r)) {
    return { verdict: 'fixed', fixed: 1, vulnerable: 0, oddity: 0 };
  }
  if (r.status === c.fixed) {
    return { verdict: 'RIGHT CODE, WRONG REASON', fixed: 0, vulnerable: 0, oddity: 1 };
  }
  if (r.status === c.vulnerable) {
    return { verdict: 'VULNERABLE', fixed: 0, vulnerable: 1, oddity: 0 };
  }
  return { verdict: 'unexpected', fixed: 0, vulnerable: 0, oddity: 1 };
}

/*
 * One case at a time, deliberately. A PUT changes the server's log level, so
 * these requests are not independent and must not be raced: the regression
 * cases and the restore step both depend on the order.
 */
async function runCases(cases) {
  const results = [];
  for (const c of cases) {
    // NOSONAR sequential on purpose: the cases share server state, see above
    results.push({ c, r: await request(c.method, c.path, c.auth) });
  }
  return results;
}

function printTable(results) {
  console.log('  ' + 'case'.padEnd(46) + 'got   old   fix   verdict');
  console.log('  ' + '-'.repeat(46) + '----- ----- ----- -------');
  const tally = { decisive: 0, fixed: 0, vulnerable: 0, oddity: 0 };
  const show = (n) => String(n === null ? '-' : n).padStart(5);
  for (const { c, r } of results) {
    const v = classify(c, r);
    if (c.decisive) tally.decisive++;
    tally.fixed += v.fixed;
    tally.vulnerable += v.vulnerable;
    tally.oddity += v.oddity;
    console.log('  ' + c.name.padEnd(46) + show(r.status) + ' '
                + show(c.vulnerable) + ' ' + show(c.fixed) + '  ' + v.verdict);
    if (r.body) console.log('      body: ' + r.body.replace(/\s+/g, ' ').slice(0, 150));
  }
  return tally;
}

/* If a PUT actually changed the level, put it back. */
async function restoreLevel(results) {
  if (!results.some(({ c, r }) => c.method === 'PUT' && r.status === 200)) return;
  const back = await request('PUT', tracePath('&level=0'), true);
  console.log('');
  console.log('  a PUT succeeded, so the level was restored to 0: status ' + back.status);
}

/*
 * Before blaming the endpoint, check that the server accepted our identity at
 * all. If every authenticated case came back 401 then authentication failed,
 * which says nothing about authorization. The usual cause on a standalone test
 * server is that ZSS is built with APF_AUTHORIZED=0, so safAuthenticate()
 * verifies the password through ZIS (zisCheckUsernameAndPassword); with no ZIS
 * every password is rejected and a correct one looks exactly like a wrong one.
 */
function authenticationFailed(results) {
  if (!cfg.user) return false;
  const authed = results.filter(({ c }) => c.auth);
  if (authed.length === 0 || !authed.every(({ r }) => r.status === 401)) return false;
  console.log('AUTHENTICATION FAILED, so this run says nothing about authorization.');
  console.log('Every authenticated case came back 401. Check, in order:');
  console.log('  1. ZIS. The server log shows ZWES1014I; it must say cmsRC=0. A build with');
  console.log('     APF_AUTHORIZED=0 verifies passwords through ZIS, so without it a correct');
  console.log('     password is rejected exactly like a wrong one.');
  console.log('  2. The userid and password themselves, against the same system.');
  console.log('Re-run once ZWES1014I reports success.');
  return true;
}

function finalVerdict(t) {
  if (!t.decisive) {
    console.log('INCONCLUSIVE: no decisive case ran. Pass --user to test what #855 is about,');
    console.log('              which is an authenticated user acting without authorization.');
    return 2;
  }
  if (t.oddity) {
    console.log('MIXED: ' + t.oddity + ' case(s) matched neither expectation. Read the table above.');
    return 2;
  }
  if (t.fixed === t.decisive) {
    console.log('FIXED: every decisive case refused the unauthorized request and the');
    console.log('       regression cases still behave as before.');
    return 0;
  }
  if (t.vulnerable === t.decisive) {
    console.log('VULNERABLE: an authenticated user can read and set the server log levels.');
    console.log('            This is zowe/zss#855.');
    return 1;
  }
  console.log('MIXED: some cases fixed, some vulnerable. Read the table above.');
  return 2;
}

async function main() {
  if (cfg.user) cfg.pass = await promptPassword(cfg.user);

  const scheme = cfg.tls ? 'https' : 'http';
  console.log('ras-rbac-test: ' + scheme + '://' + cfg.host + ':' + cfg.port + '/ras/traceLevel');
  console.log('  component ' + COMPONENT_ID + '   config rbac=' + cfg.rbac
              + (cfg.user ? '   auth ' + cfg.user : '   NO CREDENTIALS (the decisive cases will be skipped)'));
  console.log('');

  const results = await runCases(buildCases());
  const tally = printTable(results);
  await restoreLevel(results);
  console.log('');

  return authenticationFailed(results) ? 2 : finalVerdict(tally);
}

void (async () => {
  try {
    process.exit(await main());
  } catch (e) {
    console.error('ras-rbac-test failed: ' + (e?.stack ?? e));
    process.exit(2);
  }
})();
