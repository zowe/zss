/*
  This program and the accompanying materials are made available
  under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/**
 * schemas.ts - get the Zowe base schemas rather than keeping copies.
 *
 * The ZSS config schema refers by $id to two schemas that belong to Zowe:
 * server-base (zowe-yaml-schema.json) and server-common. The config manager must
 * have all of them loaded to resolve those references.
 *
 * WHY NOT KEEP COPIES: a copy in the repository is a snapshot of one moment.
 * These change between Zowe releases, by about 11KB between the two installs on
 * our test system, so a copy drifts and the server ends up validated against
 * rules Zowe no longer uses.
 *
 * WHY NOT JUST READ THE INSTALLED ONES: every installed copy we have seen is
 * UNTAGGED, and the EBCDIC configuration manager reads untagged ASCII as EBCDIC
 * and sees garbage. So whatever the source, the bytes must be converted to
 * IBM-1047 and tagged, which is what this does.
 */

import * as fs from 'fs';
import * as https from 'https';
import * as path from 'path';
import * as z from './zos';

const REPO = 'zowe/zowe-install-packaging';

/** Each file and the $id it must declare. Checking the $id is the point: it is
 *  what the config manager resolves against, so a file declaring the wrong one
 *  is useless whatever it is named, and an error page declares none at all. */
const WANTED: { file: string; id: string }[] = [
  { file: 'zowe-yaml-schema.json', id: 'https://zowe.org/schemas/v2/server-base' },
  { file: 'server-common.json', id: 'https://zowe.org/schemas/v2/server-common' },
];

function get(url: string, redirectsLeft = 5): Promise<Buffer> {
  return new Promise((resolve, reject) => {
    const req = https.get(url, { timeout: 20000 }, (res) => {
      const code = res.statusCode ?? 0;
      if (code >= 300 && code < 400 && res.headers.location) {
        res.resume();
        if (redirectsLeft <= 0) return reject(new Error('too many redirects'));
        return get(res.headers.location, redirectsLeft - 1).then(resolve, reject);
      }
      if (code !== 200) { res.resume(); return reject(new Error(`HTTP ${code}`)); }
      const chunks: Buffer[] = [];
      res.on('data', (d: Buffer) => chunks.push(d));
      res.on('end', () => resolve(Buffer.concat(chunks)));
      res.on('error', reject);
    });
    req.on('error', reject);
    req.on('timeout', () => req.destroy(new Error('timeout')));
  });
}

export interface FetchResult {
  ok: boolean;
  wrote: string[];
  failed: { file: string; why: string }[];
}

export async function fetchSchemas(opts: {
  into: string;
  ref: string;
  fromInstall: boolean;
  runtime: string;
  log?: (s: string) => void;
}): Promise<FetchResult> {
  const say = opts.log ?? (() => { /* quiet */ });
  const source = opts.fromInstall
    ? path.join(opts.runtime, 'schemas')
    : `https://raw.githubusercontent.com/${REPO}/${opts.ref}/schemas`;

  fs.mkdirSync(opts.into, { recursive: true });
  say(`  from ${source}`);
  say(`  into ${opts.into}`);

  const wrote: string[] = [];
  const failed: { file: string; why: string }[] = [];

  for (const w of WANTED) {
    let buf: Buffer;
    try {
      buf = opts.fromInstall
        ? fs.readFileSync(path.join(opts.runtime, 'schemas', w.file))
        : await get(`${source}/${w.file}`);
    } catch (e: any) {
      failed.push({ file: w.file, why: e?.message ?? String(e) });
      say(`  FAIL ${w.file}: ${e?.message ?? e}`);
      continue;
    }

    /* Validate BEFORE converting: this is the only moment the bytes are still in
       an encoding this process can parse. Otherwise a 404 page would be
       faithfully converted to EBCDIC, tagged, and handed to the server. */
    let id: unknown;
    try {
      id = (JSON.parse(buf.toString('utf8')) as { $id?: unknown }).$id;
    } catch (e: any) {
      failed.push({ file: w.file, why: 'not valid JSON' });
      say(`  FAIL ${w.file}: not valid JSON (${e?.message ?? e})`);
      continue;
    }
    if (id !== w.id) {
      failed.push({ file: w.file, why: `declares $id ${String(id)}` });
      say(`  FAIL ${w.file}: declares $id "${String(id)}", expected "${w.id}"`);
      continue;
    }

    const dest = path.join(opts.into, w.file);
    let how: string;
    if (z.isZos) {
      fs.writeFileSync(dest, z.iconvBuffer(buf, 'ISO8859-1', 'IBM-1047'));
      z.tagFile(dest, 'IBM-1047');
      how = 'converted to IBM-1047 and tagged';
    } else {
      fs.writeFileSync(dest, buf);
      how = 'as-is (not z/OS)';
    }
    wrote.push(w.file);
    say(`  ok   ${w.file.padEnd(24)} ${buf.length} bytes, ${how}`);
  }

  /* Record what was taken, so a surprise months from now is answerable. */
  fs.writeFileSync(path.join(opts.into, 'FETCHED.json'),
    JSON.stringify({
      fetchedAt: new Date().toISOString(),
      source,
      ref: opts.fromInstall ? null : opts.ref,
      files: wrote,
    }, null, 2) + '\n');

  return { ok: failed.length === 0, wrote, failed };
}
