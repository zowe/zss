/*
  This program and the accompanying materials are made available
  under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/**
 * check.ts - answer "is this system ready" in one pass.
 *
 * READ-ONLY. It creates nothing, starts nothing, submits nothing and changes
 * nothing, so it is safe to run at any time, including while somebody else is
 * using the system.
 *
 * It exists because readiness used to be something you discovered one failure
 * at a time: configure refuses, or the server starts and answers 401 to
 * everything because ZIS is not there. Every fact below was already known
 * privately by some other command; this gathers them and names the remedy.
 *
 * Checks that need z/OS report SKIP off-platform instead of failing, so the
 * same command is useful on a workstation for the configuration layer alone.
 */

import * as fs from 'node:fs';
import * as path from 'node:path';
import * as z from './zos';
import * as zis from './zis';
import * as zss from './zss';
import { TestEnv } from './env';
import { INSTANCE_DIRS } from './configure';
import { BASE_SCHEMAS } from './schemas';
import { verifyRing } from './cert';

export type State = 'ok' | 'fail' | 'skip';

export interface CheckResult {
  group: string;
  what: string;
  state: State;
  /** May contain newlines; the printer indents continuation lines. */
  detail: string;
  /** The command that fixes it. Collected into an ordered list at the end. */
  remedy?: string;
}

const ZOS_ONLY = 'needs z/OS; nothing to check here';

/* ---- configuration: no z/OS calls, so it answers everywhere ---- */

function siteConfig(env: TestEnv): CheckResult {
  const pinFile = path.join(env.support, 'test-env.local.json');
  const by = (src: (s: string) => boolean): string[] =>
    Object.entries(env.sources).filter(([, s]) => src(s)).map(([k]) => k);

  const pinned = by((s) => s === 'test-env.local.json');
  const fromEnv = by((s) => s.startsWith('ZSS_TEST_'));

  /* Reaching here means loadEnv parsed the file and accepted every value, so
     there is nothing left to fail; the useful content is what came from where. */
  const lines: string[] = [];
  if (pinned.length > 0) lines.push(`test-env.local.json pins ${pinned.join(', ')}`);
  if (fromEnv.length > 0) lines.push(`the environment supplies ${fromEnv.join(', ')}`);
  if (lines.length === 0) {
    lines.push(
      'nothing pinned; every value is derived from the userid and the directory layout',
      `to pin anything, write ${pinFile}`);
  }
  return { group: 'configuration', what: 'site values', state: 'ok', detail: lines.join('\n') };
}

function configFile(env: TestEnv): CheckResult {
  const g = 'configuration';
  const yaml = path.join(env.support, 'zowe.yaml');
  if (!fs.existsSync(yaml)) {
    return { group: g, what: 'zowe.yaml', state: 'fail', detail: 'not generated', remedy: 'zss-test configure' };
  }

  const text = fs.readFileSync(yaml, 'utf8');
  const leftover = [...new Set(text.match(/@[A-Z][A-Z0-9_]*@/g) ?? [])];
  if (leftover.length > 0) {
    return {
      group: g, what: 'zowe.yaml', state: 'fail',
      detail: `unsubstituted placeholders: ${leftover.join(', ')}`,
      remedy: 'zss-test configure --force',
    };
  }

  /* A generated file that no longer matches the configuration is the one way a
     pin appears to be ignored: configure leaves an existing zowe.yaml alone
     unless asked, so a changed port lives in env.ts and nowhere else. */
  const yamlPort = Number.parseInt(/port:\s*(\d+)/.exec(text)?.[1] ?? '', 10);
  const stale: string[] = [];
  if (Number.isFinite(yamlPort) && yamlPort !== env.port) {
    stale.push(`it says port ${yamlPort}, the configuration says ${env.port}`);
  }
  if (!text.includes(env.instance)) {
    stale.push(`it does not mention the instance directory ${env.instance}`);
  }
  if (stale.length > 0) {
    return {
      group: g, what: 'zowe.yaml', state: 'fail',
      detail: `generated from an older configuration:\n${stale.join('\n')}`,
      remedy: 'zss-test configure --force',
    };
  }
  return { group: g, what: 'zowe.yaml', state: 'ok', detail: 'substituted, and agrees with the values above' };
}

function instanceTree(env: TestEnv): CheckResult {
  const missing = INSTANCE_DIRS.filter((d) => !fs.existsSync(path.join(env.instance, d)));
  if (missing.length > 0) {
    return {
      group: 'configuration', what: 'instance tree', state: 'fail',
      detail: `${env.instance} is missing ${missing.join(', ')}`,
      remedy: 'zss-test configure',
    };
  }
  return {
    group: 'configuration', what: 'instance tree', state: 'ok',
    detail: `${env.instance} has ${INSTANCE_DIRS.join(', ')}`,
  };
}

function baseSchemas(env: TestEnv): CheckResult {
  const g = 'configuration';
  const dir = path.join(env.instance, 'schemas');
  const problems: string[] = [];

  for (const s of BASE_SCHEMAS) {
    const p = path.join(dir, s.file);
    if (!fs.existsSync(p)) { problems.push(`${s.file} is missing`); continue; }
    if (fs.statSync(p).size === 0) { problems.push(`${s.file} is empty`); continue; }
    /* Untagged is the failure that looks like success: the EBCDIC config
       manager reads untagged ASCII as EBCDIC and rejects the schema as
       malformed, which reads as a config error rather than an encoding one. */
    if (z.isZos) {
      const tag = z.fileTag(p);
      if (tag !== 'IBM-1047') problems.push(`${s.file} is tagged ${tag ?? 'untagged'}, not IBM-1047`);
    }
  }

  if (problems.length > 0) {
    return {
      group: g, what: 'Zowe base schemas', state: 'fail',
      detail: problems.join('\n'),
      remedy: 'zss-test fetch-schemas          (or --from-install with no outbound network)',
    };
  }

  /* Provenance, so "where did these come from" is answerable later. */
  let where = 'no FETCHED.json, so the source is unrecorded';
  try {
    const f = JSON.parse(fs.readFileSync(path.join(dir, 'FETCHED.json'), 'utf8')) as
      { fetchedAt?: string; ref?: string | null; source?: string };
    const when = (f.fetchedAt ?? '').slice(0, 10);
    where = f.ref ? `fetched ${when} from ${f.ref}` : `taken ${when} from ${f.source ?? 'a local install'}`;
  } catch { /* leave the default */ }

  const tagged = z.isZos ? ', tagged IBM-1047' : '';
  return {
    group: g, what: 'Zowe base schemas', state: 'ok',
    detail: `${BASE_SCHEMAS.map((s) => s.file).join(' and ')} present${tagged}\n${where}`,
  };
}

/* ---- TLS identity ---- */

function tlsIdentity(env: TestEnv): CheckResult {
  const g = 'TLS identity';
  const what = `key ring ${env.keyring}`;
  if (!z.isZos) return { group: g, what, state: 'skip', detail: ZOS_ONLY };

  const r = verifyRing(env);
  return r.ok
    ? { group: g, what, state: 'ok', detail: r.out }
    : { group: g, what, state: 'fail', detail: r.out, remedy: 'zss-test cert' };
}

/* ---- ZIS ---- */

function zisChecks(env: TestEnv): CheckResult[] {
  const g = 'ZIS';
  const wanted: [string, string, string][] = [
    [env.zisLoadlib, 'ZWESIS01', 'the server program'],
    [env.zisParmlib, `ZWESIP${env.parmlibMember}`, 'its parameters'],
    [env.zisJcllib, 'ZISRUN', 'the job that runs it'],
  ];

  if (!z.isZos) {
    return [
      ...wanted.map(([dsn, mem]): CheckResult =>
        ({ group: g, what: `${dsn}(${mem})`, state: 'skip', detail: ZOS_ONLY })),
      { group: g, what: 'ZWES.IS FACILITY access', state: 'skip', detail: ZOS_ONLY },
      { group: g, what: 'ZIS running', state: 'skip', detail: ZOS_ONLY },
    ];
  }

  const out: CheckResult[] = [];

  /* One LISTDS per dataset rather than per member. */
  for (const [dsn, mem, why] of wanted) {
    const has = z.members(dsn).includes(mem);
    out.push({
      group: g, what: `${dsn}(${mem})`,
      state: has ? 'ok' : 'fail',
      detail: has ? why : `not there (${why})`,
      remedy: has ? undefined : 'zss-test zis configure',
    });
  }

  const saf = zis.checkZwesIs(env);
  out.push({
    group: g, what: 'ZWES.IS FACILITY access',
    state: saf.ok ? 'ok' : 'fail',
    detail: saf.detail,
    /* Not something this harness can fix: it is somebody's RACF decision. */
    remedy: saf.ok ? undefined : 'ask a security administrator; no command here can grant it',
  });

  /* EXECUTING is also the proof that APF took, which is why APF is not a
     separate check: an un-authorized load library makes ZIS end immediately
     with ZWES0117E, so a running ZIS cannot be un-authorized. */
  const st = zis.status(env);
  let zisDetail: string;
  if (st.executing) {
    zisDetail = `job ${env.zisJob} is EXECUTING, so ${env.zisLoadlib} is APF-authorized`;
  } else if (st.found) {
    zisDetail = `job ${env.zisJob} is ${st.text}`;
  } else {
    zisDetail = `job ${env.zisJob} is not running`;
  }
  out.push({
    group: g, what: 'ZIS running',
    state: st.executing ? 'ok' : 'fail',
    detail: zisDetail,
    remedy: st.executing ? undefined : 'zss-test zis apf && zss-test zis start',
  });

  return out;
}

/* ---- the server ---- */

function serverChecks(env: TestEnv): CheckResult[] {
  const g = 'server';
  const out: CheckResult[] = [];
  const bin = zss.binary(env);

  const built = fs.existsSync(bin);
  out.push({
    group: g, what: 'zssServer64 built',
    state: built ? 'ok' : 'fail',
    detail: built ? bin : `${bin} not found`,
    remedy: built ? undefined : 'build it: sh build_zss64.sh in the build directory',
  });

  if (!z.isZos) {
    out.push({ group: g, what: 'answering', state: 'skip', detail: ZOS_ONLY });
    return out;
  }

  const pids = zss.running(env);
  if (pids.length === 0) {
    out.push({
      group: g, what: 'answering', state: 'fail',
      detail: 'not running',
      remedy: 'zss-test server start',
    });
    return out;
  }

  /* start() rotates the log before launching, so these lines belong to the
     process that is running now and not to some earlier one. */
  const log = zss.logFile(env);
  const text = fs.existsSync(log) ? fs.readFileSync(log, 'utf8') : '';
  const cms = /ZWES1014I.*/.exec(text)?.[0]?.trim();
  const reachedZis = cms !== undefined && /cmsRC='0'/.test(cms);
  out.push(
    {
      group: g, what: 'answering', state: 'ok',
      detail: `listening on https://${env.addr}:${env.port}, pid ${pids.join(', ')}`,
    },
    {
      group: g, what: 'server reached ZIS',
      state: reachedZis ? 'ok' : 'fail',
      detail: cms ?? 'no ZWES1014I in the log, so the server never asked ZIS anything',
      /* Worth its own row: without it nothing authenticates and every test gets
         401 whatever it sends, which looks like a broken test, not a broken ZIS. */
      remedy: reachedZis ? undefined : 'zss-test zis start && zss-test server stop && zss-test server start',
    });
  return out;
}

export function checkAll(env: TestEnv): CheckResult[] {
  return [
    siteConfig(env),
    configFile(env),
    instanceTree(env),
    baseSchemas(env),
    tlsIdentity(env),
    ...zisChecks(env),
    ...serverChecks(env),
  ];
}

/** Print the results grouped, then the remedies in order. Returns the exit
 *  code: 0 ready, 1 not ready. */
const WIDTH = 34;

const MARK: Record<State, string> = { ok: 'ok  ', fail: 'FAIL', skip: 'skip' };

function printRows(results: CheckResult[], log: (s: string) => void): void {
  let group = '';
  for (const r of results) {
    if (r.group !== group) { group = r.group; log(`=== ${group} ===`); }
    const [first, ...rest] = r.detail.split('\n');
    log(`  ${MARK[r.state]}  ${r.what.padEnd(WIDTH)}${first ?? ''}`);
    for (const l of rest) log(`        ${' '.repeat(WIDTH)}${l}`);
  }
}

/**
 * The commands that fix what failed, in the order the steps depend on each
 * other, which is the order the checks run in. Deduplicated because one command
 * fixes several rows, and a weaker form of a command already in the list is
 * dropped: being told to run both `configure` and `configure --force` is noise,
 * not a two-step plan.
 */
function remediesFor(failed: CheckResult[]): string[] {
  const seen = new Set<string>();
  const all: string[] = [];
  for (const r of failed) {
    if (r.remedy !== undefined && !seen.has(r.remedy)) {
      seen.add(r.remedy);
      all.push(r.remedy);
    }
  }
  return all.filter((c) => !all.some((o) => o !== c && o.startsWith(c + ' ')));
}

export function report(results: CheckResult[], log: (s: string) => void = console.log): number {
  printRows(results, log);

  const failed = results.filter((r) => r.state === 'fail');
  const checked = results.filter((r) => r.state !== 'skip').length;
  const skipped = results.length - checked;
  const aside = skipped > 0 ? `, ${skipped} skipped off-platform` : '';

  log('');
  if (failed.length === 0) {
    log(`READY: ${checked} checks passed${aside}.`);
    return 0;
  }

  log(`NOT READY: ${failed.length} of ${checked} checks failed${aside}.`);
  log('');

  const remedies = remediesFor(failed);
  if (remedies.length > 0) {
    log('In order:');
    for (const c of remedies) log('  ' + c);
  }
  return 1;
}
