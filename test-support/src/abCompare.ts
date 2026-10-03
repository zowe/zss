/*
  This program and the accompanying materials are made available
  under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/**
 * abCompare.ts - run the same test against two builds and prove which answered.
 *
 * A harness that cannot say which build answered is worse than none. An earlier
 * shell version of this silently tested the same already-running server twice
 * and reported success both times, because its process check contained an
 * unresolved ".." that never matched what ps prints, so nothing was stopped and
 * the start refusal was discarded.
 *
 * So this guarantees two things before it believes any result:
 *   1. each run really did restart the server (a ZWES1013I line appeared),
 *   2. the two runs reported DIFFERENT build stamps.
 *
 * The third guard, that a result carries the expected REASON and not merely the
 * expected status code, belongs in each test: the same status can arise for an
 * unrelated cause, which is how a pre-fix build once looked fixed.
 */

import { spawnSync } from 'node:child_process';
import { TestEnv } from './env';
import * as zss from './zss';

export interface Phase {
  label: string;
  image: string;
  version: string | null;
  zis: string | null;
  zisOk: boolean;
  testExit: number | null;
}

export interface Comparison {
  phases: Phase[];
  genuine: boolean;
  why?: string;
}

type Say = (s: string) => void;

/** One phase: swap the build in, start it, run the test. Separated from compare()
 *  so each half is readable: this is the mechanics, compare() is the argument. */
function runPhase(env: TestEnv, label: string, image: string,
                  command: string[], say: Say): Phase | { failed: string } {
  say('');
  say('================================================================');
  say(` ${label}`);
  say(` ${image}`);
  say('================================================================');

  const swap = zss.useBuild(env, image);
  if (!swap.ok) return { failed: `${label}: ${swap.why}` };

  const started = zss.start(env);
  if (!started.ok) return { failed: `${label}: ${started.why}` };

  say(`  ${started.version}`);
  if (started.zis) say(`  ${started.zis}`);
  if (!started.zisOk) {
    say('  WARNING: ZIS is not Ok, so ZSS cannot authenticate anyone. Any');
    say('           authenticated case will be 401 and will say nothing.');
  }
  say('');

  const cmd = command[0];
  if (cmd === undefined) return { failed: 'empty test command' };

  /* The test owns its own verdict; this only reports the exit status. */
  const r = spawnSync(cmd, command.slice(1), { stdio: 'inherit' });
  const exit = typeof r.status === 'number' ? r.status : null;
  say(`  (test exit ${exit === null ? 'signal' : exit})`);

  return {
    label, image,
    version: started.version,
    zis: started.zis,
    zisOk: started.zisOk,
    testExit: exit,
  };
}

export function compare(env: TestEnv, opts: {
  before: string;
  after: string;
  command: string[];
  onPhase?: (p: Phase) => void;
  log?: Say;
}): Comparison {
  const say = opts.log ?? ((s: string) => console.log(s));
  const phases: Phase[] = [];

  for (const [label, image] of [['BEFORE', opts.before], ['AFTER', opts.after]] as [string, string][]) {
    const result = runPhase(env, label, image, opts.command, say);
    if ('failed' in result) return { phases, genuine: false, why: result.failed };
    phases.push(result);
    if (opts.onPhase) opts.onPhase(result);
  }

  const [b, a] = phases;
  if (!b || !a) return { phases, genuine: false, why: 'a phase did not complete' };
  if (b.version === a.version) {
    return {
      phases, genuine: false,
      why: `both runs reported the same build stamp, so the same server answered twice:\n  ${b.version}`,
    };
  }
  return { phases, genuine: true };
}

export function report(c: Comparison, log: (s: string) => void = console.log): number {
  log('');
  log('================================================================');
  if (!c.genuine) {
    log(' INVALID COMPARISON');
    log('================================================================');
    log(c.why ?? 'unknown');
    log('');
    log('The result blocks above mean nothing. Check that the two images really');
    log('differ, that nothing else holds the port, and that the binary is writable.');
    return 2;
  }
  log(' comparison was genuine - two different builds answered');
  log('================================================================');
  for (const p of c.phases) log(`  ${p.label.padEnd(7)} ${p.version}`);
  log('');
  log('The test server is left running on the AFTER build.');
  return 0;
}
