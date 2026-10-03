/*
  This program and the accompanying materials are made available
  under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/**
 * zos.ts - the only module that shells out.
 *
 * Every z/OS facility this harness needs is wrapped exactly once here, so the
 * rest of the code is ordinary TypeScript. There are far fewer of these than
 * one expects: TSO commands, writing a PDS member, submitting and watching a
 * job, file tagging and code-page conversion. Everything else that used to be
 * shell was templating, polling and parsing, which belongs in a language with
 * data structures.
 *
 * Several of the functions here exist to encode a trap that cost real time.
 * Those are commented where they are, not in a list somewhere else.
 */

import { execFileSync, ExecFileSyncOptions } from 'node:child_process';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';

/* Node's own type for platform does not list os390, even though Node on z/OS
   reports exactly that, so this compares as a plain string. */
export const isZos = String(process.platform) === 'os390' || String(os.platform()) === 'os390';

/** Result of a command: never throws for a non-zero exit, because on z/OS a
 *  non-zero return code is frequently informational rather than a failure. */
export interface Ran {
  ok: boolean;
  code: number;
  out: string;
}

/**
 * Directories a system tool is allowed to come from, in order.
 *
 * Resolving a tool to an absolute path means PATH cannot decide what runs. That
 * matters more than usual here: this harness issues TSO and operator commands,
 * so a `tsocmd` picked up from a writable directory earlier on PATH would be
 * running privileged work on our behalf. These are the standard z/OS locations
 * plus the usual Unix ones so the module still works off-platform.
 */
const TOOL_DIRS = ['/bin', '/usr/bin', '/usr/sbin', '/usr/local/bin'];

const resolved = new Map<string, string>();

/** Absolute path for a system tool, or the bare name if it is somewhere else
 *  (in which case PATH decides, and the caller has chosen that). */
export function resolveTool(name: string): string {
  if (name.includes('/')) return name;
  const hit = resolved.get(name);
  if (hit !== undefined) return hit;
  for (const d of TOOL_DIRS) {
    const p = `${d}/${name}`;
    try {
      fs.accessSync(p, fs.constants.X_OK);
      resolved.set(name, p);
      return p;
    } catch { /* keep looking */ }
  }
  resolved.set(name, name);
  return name;
}

export function run(cmd: string, args: string[], opts: ExecFileSyncOptions = {}): Ran {
  try {
    /* Capture stderr as well as stdout. tsocmd echoes the command it was given
       on stderr, and letting that through makes every call look like it printed
       something unexpected. Callers want one string, not two streams. */
    const out = execFileSync(resolveTool(cmd), args, {
      encoding: 'utf8',
      maxBuffer: 32 * 1024 * 1024,
      stdio: ['ignore', 'pipe', 'pipe'],
      ...opts,
    }) as unknown as string;
    return { ok: true, code: 0, out: out ?? '' };
  } catch (e: any) {
    const out = [e?.stdout, e?.stderr].filter(Boolean).join('');
    return { ok: false, code: typeof e?.status === 'number' ? e.status : -1, out: String(out || e?.message || '') };
  }
}

/** A TSO command. Returns its output whatever the return code: RACF and the
 *  catalog routinely say useful things while exiting non-zero. */
export function tso(command: string): Ran {
  return run('tsocmd', [command]);
}

/**
 * Write text into a PDS member.
 *
 * TRAP: shell redirection into `//'DSN(MEMBER)'` silently does nothing on z/OS -
 * return code 0, no member written. Copying from a temporary file is what
 * actually works, so that is what this does.
 */
export function writeMember(dsnMember: string, content: string): Ran {
  const tmp = path.join(os.tmpdir(), `zoswm.${process.pid}.${Date.now()}`);
  fs.writeFileSync(tmp, content.endsWith('\n') ? content : content + '\n');
  try {
    return run('cp', [tmp, `//'${dsnMember}'`]);
  } finally {
    try { fs.unlinkSync(tmp); } catch { /* best effort */ }
  }
}

export function readDataset(dsn: string): string | null {
  const r = run('cat', [`//'${dsn}'`]);
  return r.ok ? r.out : null;
}

/** True when the dataset is cataloged. */
export function datasetExists(dsn: string): boolean {
  const out = tso(`LISTDS '${dsn}'`).out.toUpperCase();
  return !/NOT IN CATALOG|NOT FOUND|INVALID DATA SET NAME/.test(out);
}

export function allocateIfAbsent(dsn: string, attrs: string): 'exists' | 'created' | 'failed' {
  if (datasetExists(dsn)) return 'exists';
  const r = tso(`ALLOC FI(ZTPROV) DA('${dsn}') NEW CATALOG ${attrs}`);
  return r.ok || datasetExists(dsn) ? 'created' : 'failed';
}

export function members(dsn: string): string[] {
  const out = tso(`LISTDS '${dsn}' MEMBERS`).out;
  const i = out.indexOf('--MEMBERS--');
  if (i < 0) return [];
  return out.slice(i + '--MEMBERS--'.length)
    .split('\n').map((s) => s.trim())
    .filter((s) => s.length > 0 && /^[A-Z#$@][A-Z0-9#$@]{0,7}$/.test(s));
}

/** SMS-managed datasets and others take different APF operands, and the wrong
 *  one is rejected. An SMS-managed dataset reports a storage class; a non-SMS
 *  one reports only a volume. */
export function datasetPlacement(dsn: string): { sms: boolean; volume: string | null } {
  const out = tso(`LISTCAT ENT('${dsn}') ALL`).out;
  if (/STORAGECLASS|STORCLAS/i.test(out)) return { sms: true, volume: null };
  const m = /VOLSER-*([A-Z0-9]{1,6})\s/.exec(out);
  return { sms: false, volume: m?.[1] ?? null };
}

export interface JobState {
  found: boolean;
  jobid: string | null;
  /** Raw TSO wording, kept because it is more informative than a flag. */
  text: string;
  executing: boolean;
  onOutputQueue: boolean;
  waiting: boolean;
}

/**
 * TRAP: TSO STATUS and CANCEL refuse any job whose name is not the userid plus
 * at least one character. A job named otherwise still RUNS, it simply cannot be
 * queried or stopped from here, which is how a server once started that could
 * not be shut down. Callers should derive names accordingly.
 */
export function jobStatus(jobname: string): JobState {
  const out = tso(`STATUS ${jobname}`).out;
  const line = out.split('\n').find((l) => l.includes(jobname) && !l.startsWith('STATUS')) ?? '';
  const idm = /\((JOB\d+)\)/.exec(line);
  return {
    found: !/NOT FOUND/i.test(line) && line.trim().length > 0,
    jobid: idm ? idm[1] : null,
    text: line.trim(),
    executing: /EXECUTING/i.test(line),
    onOutputQueue: /OUTPUT QUEUE/i.test(line),
    waiting: /WAITING FOR EXECUTION/i.test(line),
  };
}

export function submit(dsnMember: string): { ok: boolean; jobid: string | null; out: string } {
  const r = tso(`SUBMIT '${dsnMember}'`);
  const m = /\((JOB\d+)\)/.exec(r.out);
  return { ok: r.ok, jobid: m ? m[1] : null, out: r.out };
}

export function cancelJob(jobname: string, jobid: string): Ran {
  return tso(`CANCEL ${jobname}(${jobid}) PURGE`);
}

/**
 * Retrieve a job's log.
 *
 * TRAP: TSO OUTPUT can only see HELD output. A job whose message class is not
 * held returns "NO HELD OUTPUT FOR JOB", and its failure is then invisible
 * without SDSF. Submit with a held MSGCLASS if you intend to read the log.
 */
export function jobLog(jobname: string, jobid: string, intoDsn: string): string | null {
  tso(`DELETE '${intoDsn}'`);
  const r = tso(`OUTPUT ${jobname}(${jobid}) PRINT('${intoDsn}') KEEP`);
  if (/NO HELD OUTPUT/i.test(r.out)) return null;
  return readDataset(intoDsn);
}

export interface OperCmdResult {
  issued: boolean;
  reply: string;
  why?: string;
}

/**
 * Issue an MVS operator command and read the reply.
 *
 * TRAP 1: a JCL `// COMMAND` statement is not merely refused here, it hangs. The
 * job sits in WAITING FOR EXECUTION indefinitely while SUBMIT reports success.
 * So this drives TSO CONSOLE under IKJEFT01 instead.
 *
 * TRAP 2: TSO CONSOLE defaults the console name to the userid, and SDSF
 * activates a console under that same name. A batch job competing with your own
 * SDSF session fails with `MCSOPER RETURN CODE X'4'`, which reads exactly like
 * an authority refusal and is not. Hence the explicit, distinct console name.
 *
 * TRAP 3: SYSTSIN stops at column 72. A long command loses its closing
 * parenthesis, so it goes on its own continuation line.
 */
export function operatorCommand(opts: {
  command: string;
  jobname: string;
  consoleName: string;
  jcllib: string;
  jobacct: string;
  replyDsn: string;
  waitSeconds?: number;
}): OperCmdResult {
  const { command, jobname, consoleName, jcllib, jobacct, replyDsn } = opts;
  tso(`DELETE '${replyDsn}'`);

  const jcl = [
    `//${jobname} JOB ${jobacct}`,
    '//TSO     EXEC PGM=IKJEFT01,DYNAMNBR=20',
    `//SYSTSPRT DD DSN=${replyDsn},DISP=(NEW,CATLG),`,
    '//            SPACE=(TRK,(2,2)),DCB=(RECFM=FB,LRECL=137,BLKSIZE=1370)',
    '//SYSTSIN  DD *',
    'CONSPROF SOLDISPLAY(YES) SOLNUM(100)',
    `CONSOLE ACTIVATE NAME(${consoleName})`,
    'CONSOLE SYSCMD(-',
    `${command})`,
    'CONSOLE DEACTIVATE',
    '/*',
  ].join('\n');

  writeMember(`${jcllib}(ZISCMD)`, jcl);
  const s = submit(`${jcllib}(ZISCMD)`);
  if (!s.ok) return { issued: false, reply: s.out, why: 'SUBMIT failed' };

  const deadline = Date.now() + (opts.waitSeconds ?? 60) * 1000;
  while (Date.now() < deadline) {
    sleepSeconds(5);
    const st = jobStatus(jobname);
    if (!st.found || st.onOutputQueue) break;
  }

  const reply = readDataset(replyDsn) ?? '';
  if (/MCSOPER RETURN CODE/i.test(reply)) {
    return { issued: false, reply, why: 'console could not activate (name already in use, or no authority)' };
  }
  if (/TERMINATING PARENTHESIS|COULD NOT BE PARSED/i.test(reply)) {
    return { issued: false, reply, why: 'the command was truncated or malformed' };
  }
  return { issued: true, reply };
}

/** Processes whose command line contains `needle`.
 *
 *  TRAP: ps prints the RESOLVED path, so a needle containing ".." never matches.
 *  Resolve before calling, or nothing is ever found and nothing is ever stopped.
 */
export function pidsMatching(needle: string): number[] {
  const out = run('ps', ['-ef']).out;
  return out.split('\n')
    .filter((l) => l.includes(needle) && !l.includes('grep'))
    .map((l) => Number.parseInt(l.trim().split(/\s+/)[1], 10))
    .filter((n) => Number.isFinite(n));
}

export function killPids(pids: number[], hard = false): void {
  for (const p of pids) {
    try { process.kill(p, hard ? 'SIGKILL' : 'SIGTERM'); } catch { /* already gone */ }
  }
}

/** Blocking sleep. The whole harness is a sequence of waits on a mainframe, so
 *  a synchronous one keeps the code readable and the ordering obvious. */
export function sleepSeconds(n: number): void {
  const until = Date.now() + n * 1000;
  // Atomics.wait on a shared buffer sleeps without burning CPU.
  const sab = new Int32Array(new SharedArrayBuffer(4));
  while (Date.now() < until) {
    Atomics.wait(sab, 0, 0, Math.min(500, until - Date.now()));
  }
}

export function tagFile(file: string, codepage: string): Ran {
  return run('chtag', ['-t', '-c', codepage, file]);
}

/** Convert bytes between code pages. Needed because the EBCDIC configuration
 *  manager reads an untagged ASCII file as EBCDIC and sees garbage. */
export function iconvBuffer(buf: Buffer, from: string, to: string): Buffer {
  const tmp = path.join(os.tmpdir(), `zosic.${process.pid}.${Date.now()}`);
  fs.writeFileSync(tmp, buf);
  try {
    tagFile(tmp, from);
    return execFileSync(resolveTool('iconv'), ['-f', from, '-t', to, tmp], { maxBuffer: 64 * 1024 * 1024 });
  } finally {
    try { fs.unlinkSync(tmp); } catch { /* best effort */ }
  }
}

export function hostname(): string {
  for (const c of [['hostname', []], ['uname', ['-n']]] as [string, string[]][]) {
    const r = run(c[0], c[1]);
    const v = r.out.trim().split('\n')[0]?.trim();
    if (r.ok && v) return v;
  }
  return 'localhost';
}

export function extattrProgramControlled(file: string): Ran {
  return run('extattr', ['+p', file]);
}
