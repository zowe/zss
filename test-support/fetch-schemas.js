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
 * fetch-schemas.js  -  get the Zowe base schemas instead of keeping copies.
 *
 * The ZSS config schema refers by $id to two schemas that belong to Zowe, not
 * to ZSS: server-base (zowe-yaml-schema.json) and server-common. The config
 * manager must have all of them loaded to resolve those references.
 *
 * WHY FETCH RATHER THAN KEEP COPIES: a copy in this repository is a snapshot of
 * one moment. Zowe's own schemas change between releases, by about 11KB between
 * the two installs on our test system, so a copy silently drifts and the test
 * server ends up validating its configuration against rules Zowe no longer
 * uses. Fetching makes the version explicit and the drift impossible.
 *
 * WHY THEY CANNOT SIMPLY BE READ FROM THE INSTALL: every installed copy we have
 * seen is UNTAGGED, and the EBCDIC configuration manager reads untagged ASCII as
 * EBCDIC and sees nothing but garbage. So whatever the source, the bytes have to
 * be converted to IBM-1047 and tagged. This does that.
 *
 *   node fetch-schemas.js                      # into $ZSS_TEST_INST/schemas
 *   node fetch-schemas.js --ref v2.x/staging
 *   node fetch-schemas.js --from-install        # copy from the local Zowe runtime
 *   node fetch-schemas.js --out /some/dir
 *
 * On z/OS the result is converted with iconv and tagged IBM-1047. Elsewhere the
 * bytes are written as they arrived, which is what a Linux configmgr wants.
 */

'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const https = require('https');
const { execFileSync } = require('child_process');

/* The two schemas, and the $id each must declare once fetched. Checking the $id
   is the point: it is what the config manager resolves against, so a file that
   does not declare the right one is useless no matter what it is named. */
const SCHEMAS = [
  { file: 'zowe-yaml-schema.json', id: 'https://zowe.org/schemas/v2/server-base' },
  { file: 'server-common.json',    id: 'https://zowe.org/schemas/v2/server-common' },
];

const REPO = 'zowe/zowe-install-packaging';
const IS_ZOS = os.platform() === 'os390' || process.platform === 'os390';

const cfg = {
  ref: process.env.ZSS_TEST_SCHEMA_REF || 'v3.x/staging',
  out: process.env.ZSS_TEST_INST ? path.join(process.env.ZSS_TEST_INST, 'schemas') : null,
  fromInstall: false,
  runtime: process.env.ZSS_TEST_ZOWE_RUNTIME || '/usr/lpp/zowe',
};

for (let i = 2; i < process.argv.length; i++) {
  const a = process.argv[i];
  const next = () => {
    const v = process.argv[++i];
    if (v === undefined) { console.error('missing value for ' + a); process.exit(2); }
    return v;
  };
  switch (a) {
    case '--ref': cfg.ref = next(); break;
    case '--out': cfg.out = next(); break;
    case '--from-install': cfg.fromInstall = true; break;
    case '--runtime': cfg.runtime = next(); cfg.fromInstall = true; break;
    case '-h': case '--help':
      console.log([
        'fetch-schemas.js - get the Zowe base schemas the ZSS config schema refers to',
        '',
        '  --ref <branch|tag>  which ' + REPO + ' ref to fetch (default v3.x/staging)',
        '  --from-install      copy from a local Zowe runtime instead of fetching',
        '  --runtime <dir>     that runtime (default /usr/lpp/zowe); implies --from-install',
        '  --out <dir>         where to write (default $ZSS_TEST_INST/schemas)',
        '',
        'On z/OS the output is converted to IBM-1047 and tagged, because the',
        'EBCDIC config manager cannot read untagged ASCII.',
      ].join('\n'));
      process.exit(0);
      break;
    default: console.error('unknown option: ' + a); process.exit(2);
  }
}

if (!cfg.out) {
  console.error('ERROR: no output directory. Set ZSS_TEST_INST (test-env.sh does) or pass --out.');
  process.exit(2);
}

function get(url, redirectsLeft) {
  return new Promise((resolve, reject) => {
    const req = https.get(url, { timeout: 20000 }, (res) => {
      if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
        res.resume();
        if (!redirectsLeft) return reject(new Error('too many redirects'));
        return get(res.headers.location, redirectsLeft - 1).then(resolve, reject);
      }
      if (res.statusCode !== 200) {
        res.resume();
        return reject(new Error('HTTP ' + res.statusCode + ' for ' + url));
      }
      const chunks = [];
      res.on('data', (d) => chunks.push(d));
      res.on('end', () => resolve(Buffer.concat(chunks)));
      res.on('error', reject);
    });
    req.on('error', reject);
    req.on('timeout', () => { req.destroy(new Error('timeout')); });
  });
}

/* Write ASCII bytes where the local configmgr can read them. On z/OS that means
   converting to IBM-1047 and tagging, because an untagged ASCII file is read as
   EBCDIC and is unusable - which is the whole reason copies existed. */
function writeForPlatform(buf, dest) {
  if (!IS_ZOS) { fs.writeFileSync(dest, buf); return 'as-is (not z/OS)'; }
  const tmp = dest + '.ascii.' + process.pid;
  fs.writeFileSync(tmp, buf);
  try {
    execFileSync('chtag', ['-t', '-c', 'ISO8859-1', tmp], { stdio: 'ignore' });
    const ebcdic = execFileSync('iconv', ['-f', 'ISO8859-1', '-t', 'IBM-1047', tmp],
                                { maxBuffer: 16 * 1024 * 1024 });
    fs.writeFileSync(dest, ebcdic);
    execFileSync('chtag', ['-t', '-c', 'IBM-1047', dest], { stdio: 'ignore' });
    return 'converted to IBM-1047 and tagged';
  } finally {
    try { fs.unlinkSync(tmp); } catch (e) { /* best effort */ }
  }
}

async function main() {
  fs.mkdirSync(cfg.out, { recursive: true });
  const source = cfg.fromInstall
    ? cfg.runtime + '/schemas'
    : 'https://raw.githubusercontent.com/' + REPO + '/' + cfg.ref + '/schemas';
  console.log('fetch-schemas: ' + SCHEMAS.length + ' schemas');
  console.log('  from ' + source);
  console.log('  into ' + cfg.out);
  console.log('');

  let failed = 0;
  for (const s of SCHEMAS) {
    let buf;
    try {
      if (cfg.fromInstall) {
        const src = path.join(cfg.runtime, 'schemas', s.file);
        if (!fs.existsSync(src)) throw new Error('not found: ' + src);
        buf = fs.readFileSync(src);
      } else {
        buf = await get(source + '/' + s.file, 5);
      }
    } catch (e) {
      console.log('  FAILED ' + s.file + ': ' + e.message);
      failed++;
      continue;
    }

    /* Validate BEFORE converting: it is the only point at which the bytes are
       still in an encoding this process can parse. A 404 page would otherwise
       be faithfully converted to EBCDIC and tagged. */
    let json;
    try {
      json = JSON.parse(buf.toString('utf8'));
    } catch (e) {
      console.log('  FAILED ' + s.file + ': not valid JSON (' + e.message + ')');
      failed++;
      continue;
    }
    if (json.$id !== s.id) {
      console.log('  FAILED ' + s.file + ': declares $id "' + json.$id + '", expected "' + s.id + '"');
      failed++;
      continue;
    }

    const how = writeForPlatform(buf, path.join(cfg.out, s.file));
    console.log('  ok     ' + s.file.padEnd(24) + buf.length + ' bytes, ' + how);
  }

  /* Record what was taken, so a surprise months from now is answerable. */
  const stamp = {
    fetchedAt: new Date().toISOString(),
    source,
    ref: cfg.fromInstall ? null : cfg.ref,
    files: SCHEMAS.map((s) => s.file),
  };
  fs.writeFileSync(path.join(cfg.out, 'FETCHED.json'), JSON.stringify(stamp, null, 2) + '\n');

  console.log('');
  if (failed) {
    console.log(failed + ' of ' + SCHEMAS.length + ' failed. The test server will not start without them.');
    console.log('If this system has no outbound network, use:  --from-install [--runtime <dir>]');
    process.exit(1);
  }
  console.log('Done. start-zss.sh picks these up from ' + cfg.out + '.');
}

main().catch((e) => { console.error('fetch-schemas failed: ' + (e && e.stack ? e.stack : e)); process.exit(2); });
