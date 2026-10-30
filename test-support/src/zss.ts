/*
  This program and the accompanying materials are made available
  under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/**
 * zss.ts - start, stop and inspect the standalone test ZSS.
 *
 * The server binds loopback only and uses a SAF key ring, so there is no
 * password or keystore file anywhere.
 */

import { spawn } from 'node:child_process';
import * as fs from 'node:fs';
import * as path from 'node:path';
import * as z from './zos';
import { TestEnv } from './env';

/** 7557 is the shared production ZSS on our test system. Refusing it by default
 *  is deliberate: a stray --port on a shared box should not be able to stop it. */
export const PRODUCTION_PORT = 7557;

export function binary(env: TestEnv): string {
  return path.join(env.zssRoot, 'bin', 'zssServer64');
}

export function logFile(env: TestEnv): string {
  return path.join(env.instance, 'logs', 'zss.out');
}

/** Processes running OUR server, matched on the RESOLVED path.
 *
 *  TRAP: ps prints the resolved path, so matching on anything containing ".."
 *  silently finds nothing. That let an A/B comparison run twice against the same
 *  already-running build and report success both times. Hence path.resolve. */
export function running(env: TestEnv): number[] {
  return z.pidsMatching(path.resolve(binary(env)));
}

export function stop(env: TestEnv, timeoutSeconds = 20): { stopped: boolean; killed: number[] } {
  const first = running(env);
  if (first.length === 0) return { stopped: true, killed: [] };
  z.killPids(first);
  const deadline = Date.now() + timeoutSeconds * 1000;
  while (Date.now() < deadline) {
    z.sleepSeconds(2);
    if (running(env).length === 0) return { stopped: true, killed: first };
  }
  const left = running(env);
  z.killPids(left, true);
  z.sleepSeconds(2);
  return { stopped: running(env).length === 0, killed: first };
}

export interface StartResult {
  ok: boolean;
  pid: number | null;
  /** The ZWES1013I line, which names the build that is actually answering. */
  version: string | null;
  /** The ZWES1014I line: ZIS must report cmsRC='0' for authentication to work. */
  zis: string | null;
  zisOk: boolean;
  why?: string;
}

/**
 * Launch the server detached and wait until it says it has started.
 *
 * Waiting for ZWES1013I rather than for the process to exist is the point: a
 * process that is up but has not logged a version has not necessarily bound the
 * port, and any test that ran then would be answered by whatever else is
 * listening.
 */
export function start(env: TestEnv, waitSeconds = 40): StartResult {
  const bin = binary(env);
  if (!fs.existsSync(bin)) return { ok: false, pid: null, version: null, zis: null, zisOk: false, why: `${bin} not found` };
  if (env.port === PRODUCTION_PORT) {
    return { ok: false, pid: null, version: null, zis: null, zisOk: false,
             why: `refusing port ${PRODUCTION_PORT}: that is the shared production ZSS` };
  }
  if (running(env).length > 0) {
    return { ok: false, pid: null, version: null, zis: null, zisOk: false,
             why: `our server is already running (pid ${running(env).join(', ')}); stop it first` };
  }

  const base = path.join(env.instance, 'schemas');
  const schemas = [
    path.join(env.zssRoot, 'schemas', 'zowe-schema.json'),
    path.join(env.zssRoot, 'schemas', 'zss-config.json'),
    path.join(base, 'zowe-yaml-schema.json'),
    path.join(base, 'server-common.json'),
  ];
  for (const s of schemas.slice(2)) {
    if (!fs.existsSync(s)) {
      return { ok: false, pid: null, version: null, zis: null, zisOk: false,
               why: `${s} missing. Run: node lib/cli.js fetch-schemas` };
    }
  }

  const log = logFile(env);
  fs.mkdirSync(path.dirname(log), { recursive: true });
  if (fs.existsSync(log)) fs.renameSync(log, log + '.prev');

  const out = fs.openSync(log, 'a');
  const child = spawn(bin, ['--schemas', schemas.join(':'), '--configs', `FILE(${path.join(env.support, 'zowe.yaml')})`],
    { detached: true, stdio: ['ignore', out, out] });
  child.unref();
  fs.closeSync(out);

  const deadline = Date.now() + waitSeconds * 1000;
  let version: string | null = null;
  while (Date.now() < deadline) {
    z.sleepSeconds(3);
    const text = fs.existsSync(log) ? fs.readFileSync(log, 'utf8') : '';
    version = text.split('\n').find((l) => l.includes('ZWES1013I')) ?? null;
    if (version) break;
  }
  if (!version) {
    return { ok: false, pid: child.pid ?? null, version: null, zis: null, zisOk: false,
             why: `no ZWES1013I in ${log} after ${waitSeconds}s - the server did not start` };
  }

  const text = fs.readFileSync(log, 'utf8');
  const zis = text.split('\n').find((l) => l.includes('ZWES1014I')) ?? null;
  return {
    ok: true,
    pid: child.pid ?? null,
    version: version.trim(),
    zis: zis ? zis.trim() : null,
    zisOk: !!zis && zis.includes("cmsRC='0'"),
  };
}

/** Swap in a different build and restart. Used by the A/B comparator; separate
 *  from start() because the swap is the step that silently fails if the process
 *  check misses, and it deserves its own error. */
export function useBuild(env: TestEnv, image: string): { ok: boolean; why?: string } {
  if (!fs.existsSync(image)) return { ok: false, why: `${image} not found` };
  const s = stop(env);
  if (!s.stopped) return { ok: false, why: `could not stop pid ${running(env).join(', ')}` };
  fs.copyFileSync(image, binary(env));
  if (z.isZos) z.extattrProgramControlled(binary(env));
  return { ok: true };
}
