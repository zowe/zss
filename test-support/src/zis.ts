/*
  This program and the accompanying materials are made available
  under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/**
 * zis.ts - lifecycle for a private test ZIS.
 *
 * ZIS is the APF-authorized cross-memory server ZSS calls for privileged work.
 * It is needed for ANY authenticated test, not only privileged services:
 * safAuthenticate() in an APF_AUTHORIZED=0 build verifies passwords through
 * zisCheckUsernameAndPassword, so with no ZIS every request is 401 and a correct
 * password is indistinguishable from a wrong one.
 *
 * MODEL: a private batch JOB under the developer's own userid, not a started
 * task. No PROCLIB, no STARTED profile, no systems programmer. ZSS connects by
 * cross-memory NAME and cannot tell the difference.
 */

import * as path from 'path';
import * as z from './zos';
import { TestEnv, jobCard } from './env';

export interface Step {
  name: string;
  ok: boolean;
  detail: string;
}

const PARMLIB_ATTRS = 'SPACE(1,1) TRACKS DSORG(PO) DIR(5) RECFM(F B) LRECL(80)';
/* PDSE: ZIS binds program objects with long external names, which a plain PDS
   rejects with IEW2640E. */
const LOADLIB_ATTRS = 'SPACE(5,5) CYL DSORG(PO) DSNTYPE(LIBRARY) DIR(20) RECFM(U) BLKSIZE(6144)';

export function provisionDatasets(env: TestEnv): Step[] {
  return ([
    [env.zisLoadlib, LOADLIB_ATTRS],
    [env.zisParmlib, PARMLIB_ATTRS],
    [env.zisJcllib, PARMLIB_ATTRS],
  ] as [string, string][]).map(([dsn, attrs]) => {
    const r = z.allocateIfAbsent(dsn, attrs);
    return { name: dsn, ok: r !== 'failed', detail: r };
  });
}

export function buildZis(env: TestEnv): Step {
  /* build_zis.sh is the product's own build; shelling out to it is correct. */
  const r = z.run('sh', ['build_zis.sh'], { cwd: path.join(env.zssRoot, 'build') });
  /* TRAP: the ZSS build scripts cannot be trusted to report failure. The 64-bit
     one prints "Build zss64 successfully" after a failed compile, because
     _C89_ACCEPTABLE_RC=0 makes a SUCCESSFUL link return non-zero and the script
     treats non-zero as success. So verify the artifact, never the verdict. */
  const built = z.members(env.zisLoadlib).includes('ZWESIS01');
  const errors = (r.out.match(/ERROR CCN\d+/g) ?? []).length;
  return {
    name: 'build ZWESIS01',
    ok: built && errors === 0,
    detail: built
      ? (errors ? `ZWESIS01 present but ${errors} compile errors in the log` : 'ZWESIS01 present in the loadlib')
      : 'ZWESIS01 NOT in the loadlib - the build did not produce it',
  };
}

export function writeParmlib(env: TestEnv): Step {
  const src = path.join(env.zssRoot, 'samplib', 'zis', 'ZWESIP00');
  const r = z.run('cp', [src, `//'${env.zisParmlib}(ZWESIP${env.parmlibMember})'`]);
  return {
    name: `${env.zisParmlib}(ZWESIP${env.parmlibMember})`,
    ok: r.ok,
    detail: r.ok ? 'written (plugins commented out, so no AUX address space starts)' : r.out.trim(),
  };
}

export function writeServerJob(env: TestEnv): Step {
  /* SYSPRINT stays SYSOUT=*. TRAP: pointing it at a dataset abends S013-68 at
     OPEN, because ZWESIS01's own DCB conflicts with a hand-coded RECFM/LRECL,
     and the server never runs. The log is readable anyway because the job card
     carries a held MSGCLASS. */
  const jcl = [
    `//${env.zisJob} JOB ${jobCard(env)}`,
    '//*  Private test ZIS cross-memory server, run as a JOB (not a started task).',
    `//ZWESIS01 EXEC PGM=ZWESIS01,PARM='NAME=${env.zisName},MEM=${env.parmlibMember}'`,
    `//STEPLIB  DD DISP=SHR,DSN=${env.zisLoadlib}`,
    `//PARMLIB  DD DISP=SHR,DSN=${env.zisParmlib}`,
    '//SYSPRINT DD SYSOUT=*',
  ].join('\n');
  const r = z.writeMember(`${env.zisJcllib}(ZISRUN)`, jcl);
  return {
    name: `${env.zisJcllib}(ZISRUN)`,
    ok: r.ok,
    detail: r.ok ? `job ${env.zisJob}, server name ${env.zisName}` : r.out.trim(),
  };
}

export interface ApfResult extends Step {
  operand: string;
  issued: boolean;
  reply: string;
}

/**
 * APF-authorize the loadlib.
 *
 * Needed because ZWESIS01 is bound AC(1) and checks its own authorization at
 * startup, refusing with ZWES0117E otherwise. This must be re-run after every
 * IPL: authorization added with SETPROG is not persistent. A PROGxx PARMLIB
 * entry from a systems programmer makes it permanent and removes this step.
 */
export function apfAuthorize(env: TestEnv): ApfResult {
  const place = z.datasetPlacement(env.zisLoadlib);
  if (!place.sms && !place.volume) {
    return {
      name: 'APF', ok: false, issued: false, operand: '', reply: '',
      detail: `cannot tell whether ${env.zisLoadlib} is SMS-managed, and no volume found in the catalog`,
    };
  }
  const operand = place.sms ? 'SMS' : `VOLUME=${place.volume}`;
  const r = z.operatorCommand({
    command: `SETPROG APF,ADD,DSNAME=${env.zisLoadlib},${operand}`,
    jobname: env.cmdJob,
    consoleName: env.consoleName,
    jcllib: env.zisJcllib,
    jobacct: jobCard(env),
    replyDsn: replyDsn(env),
  });
  return {
    name: 'APF', ok: r.issued, issued: r.issued, operand, reply: r.reply,
    detail: r.issued
      ? `SETPROG issued with ,${operand}. Verify by starting ZIS; SETPROG APF is lost at the next IPL.`
      : `not issued: ${r.why ?? 'unknown'}`,
  };
}

function replyDsn(env: TestEnv): string {
  return env.zisJcllib.replace(/\.JCL$/, '') + '.CONOUT';
}

function jobOutDsn(env: TestEnv): string {
  return env.zisJcllib.replace(/\.JCL$/, '') + '.JOBOUT';
}

/** What ZWES.IS access the ZSS userid actually has. Asking is the point: on a
 *  system with an existing Zowe install the profile is already defined and owned
 *  by someone else and the userid usually already holds READ, so trying to
 *  define and permit it produces three refusals that hide the one useful fact. */
export function checkZwesIs(env: TestEnv): Step {
  const out = z.tso('RLIST FACILITY ZWES.IS AUTHUSER').out;
  if (/NOT FOUND|NOT DEFINED/i.test(out)) {
    return { name: 'ZWES.IS', ok: false, detail: 'not defined; a security administrator must define it and grant READ' };
  }
  const m = out.match(/YOUR ACCESS[\s\S]{0,200}/i);
  const seg = m ? m[0] : out;
  const enough = /\b(READ|UPDATE|CONTROL|ALTER)\b/.test(seg);
  return {
    name: 'ZWES.IS',
    ok: enough,
    detail: enough
      ? `defined, and ${env.userid} has sufficient access`
      : `defined, but ${env.userid} lacks READ; ask a security administrator`,
  };
}

export interface StartResult {
  ok: boolean;
  jobid: string | null;
  state: string;
  /** The ZWES messages, when it failed. ZIS names its own problems clearly. */
  diagnosis: string[];
}

export function start(env: TestEnv, waitSeconds = 45): StartResult {
  const s = z.submit(`${env.zisJcllib}(ZISRUN)`);
  if (!s.ok || !s.jobid) {
    return { ok: false, jobid: null, state: 'SUBMIT failed', diagnosis: [s.out.trim()] };
  }
  const deadline = Date.now() + waitSeconds * 1000;
  let st = z.jobStatus(env.zisJob);
  while (Date.now() < deadline && !st.executing && !st.onOutputQueue) {
    z.sleepSeconds(5);
    st = z.jobStatus(env.zisJob);
  }
  if (st.executing) return { ok: true, jobid: s.jobid, state: st.text, diagnosis: [] };

  /* It ended, so read why. ZIS reports its own failures in plain messages. */
  const log = z.jobLog(env.zisJob, s.jobid, jobOutDsn(env)) ?? '';
  const msgs = log.split('\n')
    .filter((l) => /ZWES\d{4}[IEWA]/.test(l) && !/ZWES0101I/.test(l))
    .map((l) => (l.match(/ZWES\d{4}[IEWA].*/) ?? [l])[0].trim());
  const seen = new Set<string>();
  const diagnosis = msgs.filter((m) => (seen.has(m) ? false : (seen.add(m), true)));
  if (!log) {
    diagnosis.push('No job log retrievable. The job card needs a HELD MSGCLASS for TSO OUTPUT to see it.');
  }
  return { ok: false, jobid: s.jobid, state: st.text || 'ended', diagnosis };
}

export function status(env: TestEnv): z.JobState {
  return z.jobStatus(env.zisJob);
}

/** Stop ZIS. Prefer the operator STOP, which lets the server release its
 *  cross-memory resources tidily; fall back to TSO CANCEL, which works only
 *  because the job name satisfies the userid-plus-one rule. */
export function stop(env: TestEnv): { stopped: boolean; how: string } {
  const before = z.jobStatus(env.zisJob);
  if (!before.found) return { stopped: true, how: 'was not running' };

  z.operatorCommand({
    command: `P ${env.zisJob}`,
    jobname: env.cmdJob,
    consoleName: env.consoleName,
    jcllib: env.zisJcllib,
    jobacct: jobCard(env),
    replyDsn: replyDsn(env),
    waitSeconds: 30,
  });

  for (let i = 0; i < 6; i++) {
    z.sleepSeconds(5);
    if (!z.jobStatus(env.zisJob).found) return { stopped: true, how: 'STOP' };
  }

  const st = z.jobStatus(env.zisJob);
  if (st.jobid) {
    z.cancelJob(env.zisJob, st.jobid);
    z.sleepSeconds(5);
    if (!z.jobStatus(env.zisJob).found) return { stopped: true, how: 'CANCEL (STOP did not take)' };
  }
  return { stopped: false, how: `still ${z.jobStatus(env.zisJob).text}` };
}
