/*
  This program and the accompanying materials are made available
  under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/**
 * env.ts - the only module that knows anything site-specific.
 *
 * Every value defaults from the invoking userid or from this file's location, so
 * on a system laid out conventionally nothing needs setting. Anything can be
 * overridden by an environment variable of the same name, or pinned for a site
 * in test-env.local.json next to this directory, which is git-ignored.
 *
 * Porting the harness to another mainframe is reading this file and perhaps
 * writing four lines of JSON. That is the whole intent.
 */

import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import { iconvBuffer } from './zos';

export interface TestEnv {
  /** Uppercased: MVS wants it that way, and dataset and job names derive from it. */
  userid: string;
  /** This directory (test-support). */
  support: string;
  /** The zss repo clone above it. */
  zssRoot: string;
  /** Scratch tree for logs, plugins, product, instance. Deliberately outside the
   *  repo so a clean checkout never destroys a running server's logs. */
  instance: string;

  addr: string;
  port: number;

  ring: string;
  label: string;
  /** SAF key ring as the config wants it: owner/ring. */
  keyring: string;

  /** MUST be a HELD output class. TSO OUTPUT can only retrieve held output, so
   *  without this a job failure is invisible to anyone without SDSF. */
  msgclass: string;
  jobacct: string;

  /** The ZIS cross-memory server name; must match components.zss.crossMemoryServerName. */
  zisName: string;
  /** Job names MUST be the userid plus at least one character, or TSO STATUS and
   *  CANCEL refuse them and a server can start that cannot be stopped. */
  zisJob: string;
  cmdJob: string;
  /** Console name must NOT be the bare userid: SDSF activates one under that
   *  name, and the collision reports as an authority failure that it is not. */
  consoleName: string;

  zisLoadlib: string;
  zisParmlib: string;
  zisJcllib: string;
  parmlibMember: string;

  /** Where to look for a local Zowe runtime when fetching schemas offline. */
  zoweRuntime: string;
  /** Which zowe-install-packaging ref to fetch base schemas from. */
  schemaRef: string;

  /**
   * Where each of the above came from, keyed by field name.
   *
   * Printed by `zss-test env` so a site can see at a glance how little it
   * actually supplies. Knowing a value is a default rather than something
   * somebody chose is the difference between reading the configuration and
   * guessing at it.
   */
  sources: Record<string, string>;
}

function envStr(name: string, fallback: string): string {
  const v = process.env[name];
  return v !== undefined && v !== '' ? v : fallback;
}

/** Did an environment variable supply this, or did the fallback? */
function envSource(name: string, derived?: string): string {
  const v = process.env[name];
  if (v !== undefined && v !== '') return name;
  return derived ?? 'default';
}

/**
 * Read the site pins, accepting either encoding.
 *
 * On z/OS a developer who edits this file in ISPF, or writes it from an EBCDIC
 * shell, produces EBCDIC bytes; one who uploads it from a workstation produces
 * ASCII. Both are reasonable and neither is detectable from the name, so try
 * UTF-8 first and fall back to converting from IBM-1047. Failing on this would
 * be a confusing first experience for exactly the file a new site must edit.
 */
function readLocalPins(file: string): Partial<TestEnv> {
  const raw = fs.readFileSync(file);
  const attempts: { text: string; how: string }[] = [{ text: raw.toString('utf8'), how: 'as UTF-8/ASCII' }];
  try {
    attempts.push({ text: iconvBuffer(raw, 'IBM-1047', 'ISO8859-1').toString('utf8'), how: 'converted from IBM-1047' });
  } catch { /* iconv absent off-platform; the first attempt is all there is */ }

  for (const a of attempts) {
    try {
      const parsed = JSON.parse(a.text) as Partial<TestEnv>;
      if (parsed && typeof parsed === 'object') return parsed;
    } catch { /* try the next encoding */ }
  }
  throw new Error(
    `${file} is not valid JSON, read either as UTF-8 or converted from IBM-1047. ` +
    'It should look like: { "instance": "/path/to/zsstest", "port": 17600 }');
}

export function loadEnv(supportDir?: string): TestEnv {
  const support = supportDir ?? path.resolve(__dirname, '..');
  const zssRoot = path.resolve(support, '..');

  let userid = envStr('ZSS_TEST_USERID', process.env.USER ?? os.userInfo().username ?? 'UNKNOWN');
  userid = userid.toUpperCase();

  const ring = envStr('ZSS_TEST_RING', 'ZWESRING');
  const label = envStr('ZSS_TEST_LABEL', 'ZOWECERT');

  const env: TestEnv = {
    userid,
    support,
    zssRoot,
    instance: envStr('ZSS_TEST_INST', path.join(os.homedir(), 'zsstest')),

    addr: envStr('ZSS_TEST_ADDR', '127.0.0.1'),
    /* 17557 deliberately, not 7557: that is the shared production ZSS on our
       test system, and the drivers refuse it without an explicit override. */
    port: Number.parseInt(envStr('ZSS_TEST_PORT', '17557'), 10),

    ring,
    label,
    keyring: envStr('ZSS_TEST_KEYRING', `${userid}/${ring}`),

    msgclass: envStr('ZSS_TEST_MSGCLASS', 'H'),
    jobacct: envStr('ZSS_TEST_JOBACCT', '1'),

    zisName: envStr('ZSS_TEST_ZIS_NAME', 'ZWESIS_TST'),
    zisJob: envStr('ZSS_TEST_ZIS_JOB', `${userid}Z`),
    cmdJob: envStr('ZSS_TEST_CMD_JOB', `${userid}C`),
    consoleName: envStr('ZSS_TEST_CONSOLE', `${userid}K`),

    zisLoadlib: envStr('ZSS_TEST_ZIS_LOADLIB', `${userid}.DEV.LOADLIB`),
    zisParmlib: envStr('ZSS_TEST_ZIS_PARMLIB', `${userid}.ZWESIS.PARMLIB`),
    zisJcllib: envStr('ZSS_TEST_ZIS_JCLLIB', `${userid}.ZWESIS.JCL`),
    parmlibMember: envStr('ZSS_TEST_ZIS_MEM', '00'),

    zoweRuntime: envStr('ZSS_TEST_ZOWE_RUNTIME', '/usr/lpp/zowe'),
    schemaRef: envStr('ZSS_TEST_SCHEMA_REF', 'v3.x/staging'),

    sources: {},
  };

  env.sources = {
    userid: envSource('ZSS_TEST_USERID', 'the logged-on user'),
    support: 'this directory',
    zssRoot: 'the directory above this one',
    instance: envSource('ZSS_TEST_INST', 'default, \$HOME/zsstest'),
    addr: envSource('ZSS_TEST_ADDR'),
    port: envSource('ZSS_TEST_PORT'),
    ring: envSource('ZSS_TEST_RING'),
    label: envSource('ZSS_TEST_LABEL'),
    keyring: envSource('ZSS_TEST_KEYRING', 'built from the userid and ring'),
    msgclass: envSource('ZSS_TEST_MSGCLASS'),
    jobacct: envSource('ZSS_TEST_JOBACCT'),
    zisName: envSource('ZSS_TEST_ZIS_NAME'),
    zisJob: envSource('ZSS_TEST_ZIS_JOB', 'built from the userid'),
    cmdJob: envSource('ZSS_TEST_CMD_JOB', 'built from the userid'),
    consoleName: envSource('ZSS_TEST_CONSOLE', 'built from the userid'),
    zisLoadlib: envSource('ZSS_TEST_ZIS_LOADLIB', 'built from the userid'),
    zisParmlib: envSource('ZSS_TEST_ZIS_PARMLIB', 'built from the userid'),
    zisJcllib: envSource('ZSS_TEST_ZIS_JCLLIB', 'built from the userid'),
    parmlibMember: envSource('ZSS_TEST_ZIS_MEM'),
    zoweRuntime: envSource('ZSS_TEST_ZOWE_RUNTIME'),
    schemaRef: envSource('ZSS_TEST_SCHEMA_REF'),
  };

  /* Site pins, applied last so they beat the derived defaults. Git-ignored.
     Anything they set is recorded as coming from that file, which is the whole
     point: it should be visible that a human chose it. */
  const localFile = path.join(support, 'test-env.local.json');
  if (fs.existsSync(localFile)) {
    const pins = readLocalPins(localFile);
    for (const k of Object.keys(pins)) env.sources[k] = 'test-env.local.json';
    Object.assign(env, pins);
  }

  /* Names that MUST satisfy the userid-plus-one rule, checked here rather than
     discovered when a job turns out to be unstoppable. */
  for (const [what, name] of [['zisJob', env.zisJob], ['cmdJob', env.cmdJob]] as [string, string][]) {
    if (!name.startsWith(env.userid) || name.length <= env.userid.length) {
      throw new Error(
        `${what} is "${name}" but must be the userid (${env.userid}) plus at least one character: ` +
        'TSO STATUS and CANCEL refuse any other name, so the job would run and could not be stopped.');
    }
    if (name.length > 8) throw new Error(`${what} "${name}" is longer than 8 characters; JES will reject the card.`);
  }
  if (env.consoleName === env.userid) {
    throw new Error(
      `consoleName must not be the bare userid: SDSF activates a console under that name and the ` +
      `collision reports as MCSOPER RETURN CODE X'4', which looks like an authority failure and is not.`);
  }

  return env;
}

/**
 * The contract between the harness and a test.
 *
 * A test is any program. It does not import anything from here and does not
 * need to be written in TypeScript. The harness brings a server up, exports
 * these variables, and runs the test; the test reads them to find the server it
 * is supposed to talk to.
 *
 * Without this a test has to hardcode a port, which is wrong the moment a CI
 * runner gives each worker its own.
 */
export function testEnvironment(env: TestEnv): Record<string, string> {
  return {
    ZSS_TEST_ADDR: env.addr,
    ZSS_TEST_PORT: String(env.port),
    ZSS_TEST_URL: `https://${env.addr}:${env.port}`,
    ZSS_TEST_USERID: env.userid,
    ZSS_TEST_INST: env.instance,
    ZSS_TEST_LOG: `${env.instance}/logs/zss.out`,
  };
}

/** The JOB statement accounting field, with the held message class folded in.
 *  REGION and TIME are not optional for the ZIS server: it runs until stopped,
 *  so a default job-class CPU limit would abend it. */
export function jobCard(env: TestEnv): string {
  return `${env.jobacct},REGION=0M,TIME=NOLIMIT,MSGCLASS=${env.msgclass}`;
}

export function describe(env: TestEnv): string {
  const rows: [string, string, string][] = [
    ['userid', env.userid, env.sources.userid ?? ''],
    ['zss repo', env.zssRoot, env.sources.zssRoot ?? ''],
    ['instance', env.instance, env.sources.instance ?? ''],
    /* Address and port are separate rows on purpose: combining them loses the
       provenance of whichever one a CI runner overrode. */
    ['address', env.addr, env.sources.addr ?? ''],
    ['port', String(env.port), env.sources.port ?? ''],
    ['keyring', env.keyring, env.sources.keyring ?? ''],
    ['cert label', env.label, env.sources.label ?? ''],
    ['ZIS name', env.zisName, env.sources.zisName ?? ''],
    ['ZIS job', env.zisJob, env.sources.zisJob ?? ''],
    ['ZIS loadlib', env.zisLoadlib, env.sources.zisLoadlib ?? ''],
    ['job msgclass', `${env.msgclass}   (must be a HELD class)`, env.sources.msgclass ?? ''],
    ['schema ref', env.schemaRef, env.sources.schemaRef ?? ''],
  ];
  const w = Math.max(...rows.map((r) => r[1].length));
  const out = rows.map(([k, v, src]) =>
    `${k.padEnd(13)}${v.padEnd(w + 2)}${src}`);

  const supplied = rows.filter(([, , s]) => s === 'test-env.local.json' || s.startsWith('ZSS_TEST_')).length;
  out.push('');
  out.push(supplied === 0
    ? 'Nothing above was supplied by this site. All of it is derived or default.'
    : `${supplied} of ${rows.length} values were supplied by this site; the rest are derived or default.`);
  return out.join('\n');
}
