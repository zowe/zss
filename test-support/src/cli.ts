/*
  This program and the accompanying materials are made available
  under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/**
 * cli.ts - the single entry point.
 *
 * Commands are a table of small handlers rather than one nested switch, so each
 * is readable on its own and adding one does not grow anything else. The usage
 * text is generated from the table, so it cannot drift out of date.
 *
 * Exit codes are meant for a pipeline:
 *   0  success
 *   1  the thing under test failed
 *   2  the harness could not run, or could not trust its own result
 */

import { spawnSync } from 'node:child_process';
import * as fs from 'node:fs';
import * as path from 'node:path';
import { loadEnv, describe, testEnvironment, TestEnv } from './env';
import * as zis from './zis';
import * as zss from './zss';
import { compare, report } from './abCompare';
import { fetchSchemas } from './schemas';
import { generateConfig } from './configure';
import { provision, trustAnchorHint } from './cert';

type Handler = (env: TestEnv, argv: string[]) => number | Promise<number>;

interface Command {
  usage: string;
  summary: string;
  run: Handler;
}

interface Step { name: string; ok: boolean; detail: string }

function argValue(argv: string[], name: string): string | undefined {
  const i = argv.indexOf(name);
  return i >= 0 ? argv[i + 1] : undefined;
}

function printSteps(steps: Step[]): boolean {
  let allOk = true;
  for (const s of steps) {
    console.log(`  ${s.ok ? 'ok  ' : 'FAIL'} ${s.name}`);
    if (s.detail) console.log(`       ${s.detail}`);
    if (!s.ok) allOk = false;
  }
  return allOk;
}

const INSTANCE_DIRS = ['logs', 'plugins', 'product', 'instance', 'schemas'];

async function getSchemas(env: TestEnv, argv: string[]): Promise<boolean> {
  const r = await fetchSchemas({
    into: path.join(env.instance, 'schemas'),
    ref: argValue(argv, '--ref') ?? env.schemaRef,
    fromInstall: argv.includes('--from-install'),
    runtime: env.zoweRuntime,
    log: (s) => console.log(s),
  });
  if (!r.ok) {
    console.log('  If this system has no outbound network:');
    console.log('    zss-test fetch-schemas --from-install');
  }
  return r.ok;
}

/* ---- ZIS subcommands ---- */

const zisCommands: Record<string, (env: TestEnv) => number> = {
  configure: (env) => {
    const groups: [string, Step[]][] = [
      ['1. datasets', zis.provisionDatasets(env)],
      ['2. build', [zis.buildZis(env)]],
      ['3. parmlib', [zis.writeParmlib(env)]],
      ['4. server job', [zis.writeServerJob(env)]],
      ['5. APF', [zis.apfAuthorize(env)]],
      ['6. ZWES.IS', [zis.checkZwesIs(env)]],
    ];
    let ok = true;
    for (const [title, steps] of groups) {
      console.log(`=== ${title} ===`);
      if (!printSteps(steps)) ok = false;
    }
    console.log('');
    console.log('APF added with SETPROG is lost at the next IPL. For something durable, ask');
    console.log(`a systems programmer to add ${env.zisLoadlib} to a PROGxx PARMLIB member.`);
    return ok ? 0 : 1;
  },

  apf: (env) => {
    const r = zis.apfAuthorize(env);
    printSteps([r]);
    if (!r.issued && r.reply) {
      console.log(r.reply.split('\n').map((l) => '       ' + l).join('\n'));
    }
    return r.ok ? 0 : 1;
  },

  start: (env) => {
    const r = zis.start(env);
    console.log(`  ${r.ok ? 'ok  ' : 'FAIL'} ZIS ${r.state}${r.jobid ? ' (' + r.jobid + ')' : ''}`);
    for (const d of r.diagnosis) console.log('       ' + d);
    if (!r.ok && r.diagnosis.some((d) => /Not APF-authorized/i.test(d))) {
      console.log('       -> run: zss-test zis apf    (and see the PROGxx note)');
    }
    return r.ok ? 0 : 1;
  },

  stop: (env) => {
    const r = zis.stop(env);
    console.log(`  ${r.stopped ? 'ok  ' : 'FAIL'} ${r.how}`);
    return r.stopped ? 0 : 1;
  },

  status: (env) => {
    const st = zis.status(env);
    console.log(`  ${st.found ? st.text : 'not running'}`);
    return st.executing ? 0 : 1;
  },
};

/* ---- server subcommands ---- */

const serverCommands: Record<string, (env: TestEnv) => number> = {
  start: (env) => {
    const r = zss.start(env);
    if (!r.ok) { console.log('  FAIL ' + r.why); return 1; }
    console.log('  ok   ' + r.version);
    if (r.zis) console.log('  ' + (r.zisOk ? 'ok   ' : 'WARN ') + r.zis);
    if (!r.zisOk) {
      console.log('       Without ZIS, ZSS cannot authenticate anyone and every request is 401.');
    }
    return 0;
  },

  stop: (env) => {
    const r = zss.stop(env);
    const what = r.killed.length ? 'stopped pid ' + r.killed.join(', ') : 'was not running';
    console.log(`  ${r.stopped ? 'ok  ' : 'FAIL'} ${what}`);
    return r.stopped ? 0 : 1;
  },

  status: (env) => {
    const pids = zss.running(env);
    const log = zss.logFile(env);
    const lines = fs.existsSync(log) ? fs.readFileSync(log, 'utf8').split('\n') : [];
    const interesting = ['ZWES1013I', 'ZWES1014I']
      .map((k) => lines.find((l) => l.includes(k)))
      .filter((l): l is string => l !== undefined);

    if (pids.length > 0) {
      /* start() rotates the log before launching, so these lines belong to the
         process that is running now. */
      console.log(`  running, pid ${pids.join(', ')}`);
      for (const l of interesting) console.log('  ' + l.trim());
      return 0;
    }

    /* Nothing is running, so the log describes a PREVIOUS run. Printing those
       lines unlabelled says the server is healthy when it is not there at all,
       which is the kind of stale answer this harness exists to prevent. */
    console.log('  not running');
    if (interesting.length > 0) {
      const when = fs.statSync(log).mtime.toISOString().replace('T', ' ').slice(0, 19);
      console.log(`  the log is from an EARLIER run, last written ${when} UTC:`);
      for (const l of interesting) console.log('    (stale) ' + l.trim());
    }
    return 1;
  },
};

function dispatchSub(group: string, table: Record<string, (env: TestEnv) => number>,
                     env: TestEnv, sub: string | undefined): number {
  const handler = sub ? table[sub] : undefined;
  if (!handler) {
    console.error(`usage: zss-test ${group} ${Object.keys(table).join('|')}`);
    return 2;
  }
  return handler(env);
}

/* ---- top-level commands ---- */

const COMPARE_USAGE = 'compare --before IMAGE --after IMAGE -- TEST COMMAND';

const commands: Record<string, Command> = {
  env: {
    usage: 'env',
    summary: 'show the resolved configuration',
    run: (env) => { console.log(describe(env)); return 0; },
  },

  'fetch-schemas': {
    usage: 'fetch-schemas [--ref REF] [--from-install]',
    summary: 'get the Zowe base schemas the config refers to',
    run: async (env, argv) => (await getSchemas(env, argv) ? 0 : 1),
  },

  configure: {
    usage: 'configure [--force]',
    summary: 'generate zowe.yaml, make the instance tree, fetch schemas',
    run: async (env, argv) => {
      console.log('=== configuration ===');
      console.log(describe(env));
      console.log('');

      const g = generateConfig(env, argv.includes('--force'));
      console.log(`  ${g.ok ? 'ok  ' : 'FAIL'} ${g.detail}`);

      for (const d of INSTANCE_DIRS) {
        const p = path.join(env.instance, d);
        const existed = fs.existsSync(p);
        if (!existed) fs.mkdirSync(p, { recursive: true });
        console.log(`  ok   ${existed ? 'exists ' : 'created'} ${p}`);
      }

      console.log('');
      console.log('=== Zowe base schemas ===');
      const schemasOk = await getSchemas(env, argv);

      console.log('');
      console.log('=== next ===');
      console.log('  TLS identity (once):  zss-test cert');
      console.log('  bring up ZIS:         zss-test zis configure && zss-test zis apf && zss-test zis start');
      console.log('  start the server:     zss-test server start');
      return g.ok && schemasOk ? 0 : 1;
    },
  },

  cert: {
    usage: 'cert [--host HOST]',
    summary: 'SAF key ring and server certificate',
    run: (env, argv) => {
      const steps = provision(env, { host: argValue(argv, '--host') });
      let ok = true;
      for (const st of steps) {
        const note = st.tolerated && !st.ok ? '  (tolerated)' : '';
        console.log(`  ${st.ok ? 'ok  ' : 'FAIL'} ${st.what}${note}`);
        if (!st.ok && st.out) {
          console.log(st.out.split('\n').map((l) => '       ' + l).join('\n'));
        }
        if (!st.ok) ok = false;
      }
      console.log('');
      console.log('To let a client trust this authority, export it:');
      console.log('  ' + trustAnchorHint(env));
      return ok ? 0 : 1;
    },
  },

  zis: {
    usage: 'zis configure|apf|start|stop|status',
    summary: 'the private test ZIS',
    run: (env, argv) => dispatchSub('zis', zisCommands, env, argv[1]),
  },

  server: {
    usage: 'server start|stop|status',
    summary: 'the standalone test ZSS',
    run: (env, argv) => dispatchSub('server', serverCommands, env, argv[1]),
  },

  run: {
    usage: 'run -- TEST COMMAND',
    summary: 'start the server if needed, then run a test against it',
    run: (env, argv) => {
      const dashdash = argv.indexOf('--');
      const command = dashdash >= 0 ? argv.slice(dashdash + 1) : [];
      const cmd = command[0];
      if (!cmd) { console.error('usage: zss-test run -- TEST COMMAND'); return 2; }

      if (zss.running(env).length === 0) {
        const s = zss.start(env);
        if (!s.ok) { console.log('  FAIL ' + s.why); return 2; }
        console.log('  ok   ' + s.version);
        if (s.zis && !s.zisOk) {
          console.log('  WARN ' + s.zis);
          console.log('       Without ZIS nothing can authenticate; every request is 401.');
        }
      }
      for (const [k, v] of Object.entries(testEnvironment(env))) {
        console.log(`  ${k}=${v}`);
      }
      console.log('');
      const r = spawnSync(cmd, command.slice(1), {
        stdio: 'inherit',
        env: { ...process.env, ...testEnvironment(env) },
      });
      return typeof r.status === 'number' ? r.status : 2;
    },
  },

  compare: {
    usage: COMPARE_USAGE,
    summary: 'run a test against two builds and prove which one answered',
    run: (env, argv) => {
      const before = argValue(argv, '--before');
      const after = argValue(argv, '--after');
      const dashdash = argv.indexOf('--');
      const command = dashdash >= 0 ? argv.slice(dashdash + 1) : [];
      if (!before || !after || command.length === 0) {
        console.error('usage: zss-test ' + COMPARE_USAGE);
        return 2;
      }
      return report(compare(env, { before, after, command }));
    },
  },
};

function usage(): void {
  console.log('zss-test - harness for a standalone ZSS (and ZIS)');
  console.log('');
  const width = 46;
  for (const c of Object.values(commands)) {
    /* A usage line longer than the column gets its summary underneath rather
       than run together with it. */
    if (c.usage.length > width) {
      console.log('  ' + c.usage);
      console.log('  ' + ' '.repeat(width) + c.summary);
    } else {
      console.log('  ' + c.usage.padEnd(width) + c.summary);
    }
  }
}

async function main(): Promise<number> {
  const argv = process.argv.slice(2);
  const name = argv[0];
  if (!name || name === '-h' || name === '--help') { usage(); return 0; }

  const command = commands[name];
  if (!command) {
    console.error('unknown command: ' + name);
    usage();
    return 2;
  }

  let env: TestEnv;
  try {
    env = loadEnv();
  } catch (e: unknown) {
    console.error('configuration error: ' + (e instanceof Error ? e.message : String(e)));
    return 2;
  }

  return command.run(env, argv);
}

main().then(
  (code) => process.exit(code),
  (e: unknown) => {
    console.error('zss-test failed: ' + (e instanceof Error ? (e.stack ?? e.message) : String(e)));
    process.exit(2);
  });
