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
 *   zss-test env                      show the resolved configuration
 *   zss-test configure                generate zowe.yaml, make the instance tree, fetch schemas
 *   zss-test fetch-schemas [--ref R]  get the Zowe base schemas
 *   zss-test cert [--host H]          SAF key ring and server certificate
 *   zss-test zis configure|apf|start|stop|status
 *   zss-test server start|stop|status
 *   zss-test compare --before A --after B -- <test command>
 *
 * Exit codes are meant for a pipeline: 0 success, 1 the thing under test
 * failed, 2 the harness could not run or could not trust its own result.
 */

import * as fs from 'fs';
import * as path from 'path';
import { loadEnv, describe, TestEnv } from './env';
import * as z from './zos';
import * as zis from './zis';
import * as zss from './zss';
import { compare, report } from './abCompare';
import { fetchSchemas } from './schemas';
import { generateConfig } from './configure';
import { provision, trustAnchorHint } from './cert';

function usage(): void {
  const lines = (fs.readFileSync(__filename.replace(/\.js$/, '.js'), 'utf8').match(/\*\s{3}zss-test[^\n]*/g) ?? [])
    .map((l) => l.replace(/^\*\s{3}/, '  '));
  console.log('zss-test - harness for a standalone ZSS (and ZIS)\n');
  if (lines.length) console.log(lines.join('\n'));
  else console.log('  env | configure | fetch-schemas | zis <cmd> | server <cmd> | compare');
}

function printSteps(steps: zis.Step[]): boolean {
  let allOk = true;
  for (const s of steps) {
    console.log(`  ${s.ok ? 'ok  ' : 'FAIL'} ${s.name}`);
    if (s.detail) console.log(`       ${s.detail}`);
    if (!s.ok) allOk = false;
  }
  return allOk;
}

function argValue(args: string[], name: string): string | undefined {
  const i = args.indexOf(name);
  return i >= 0 ? args[i + 1] : undefined;
}

async function main(): Promise<number> {
  const argv = process.argv.slice(2);
  const cmd = argv[0];
  if (!cmd || cmd === '-h' || cmd === '--help') { usage(); return 0; }

  let env: TestEnv;
  try {
    env = loadEnv();
  } catch (e: any) {
    console.error('configuration error: ' + (e?.message ?? e));
    return 2;
  }

  switch (cmd) {
    case 'env':
      console.log(describe(env));
      return 0;

    case 'fetch-schemas': {
      const ref = argValue(argv, '--ref') ?? env.schemaRef;
      const fromInstall = argv.includes('--from-install');
      const r = await fetchSchemas({
        into: path.join(env.instance, 'schemas'),
        ref,
        fromInstall,
        runtime: env.zoweRuntime,
        log: (s) => console.log(s),
      });
      return r.ok ? 0 : 1;
    }

    case 'configure': {
      console.log('=== configuration ===');
      console.log(describe(env));
      console.log('');
      const g = generateConfig(env, argv.includes('--force'));
      console.log(`  ${g.ok ? 'ok  ' : 'FAIL'} ${g.detail}`);
      for (const d of ['logs', 'plugins', 'product', 'instance', 'schemas']) {
        const p = path.join(env.instance, d);
        const existed = fs.existsSync(p);
        if (!existed) fs.mkdirSync(p, { recursive: true });
        console.log(`  ${existed ? 'ok   exists' : 'ok   created'} ${p}`);
      }
      console.log('');
      console.log('=== Zowe base schemas ===');
      const s = await fetchSchemas({
        into: path.join(env.instance, 'schemas'),
        ref: env.schemaRef,
        fromInstall: false,
        runtime: env.zoweRuntime,
        log: (m) => console.log(m),
      });
      if (!s.ok) {
        console.log('  If this system has no outbound network:');
        console.log('    zss-test fetch-schemas --from-install');
      }
      console.log('');
      console.log('=== next ===');
      console.log('  provision the TLS identity (once):  zss-test cert');
      console.log('  bring up ZIS:                       zss-test zis configure && zss-test zis apf && zss-test zis start');
      console.log('  start the server:                   zss-test server start');
      return g.ok && s.ok ? 0 : 1;
    }

    case 'cert': {
      const steps = provision(env, { host: argValue(argv, '--host') });
      let ok = true;
      for (const st of steps) {
        console.log(`  ${st.ok ? 'ok  ' : 'FAIL'} ${st.what}${st.tolerated && st.out ? '  (non-zero tolerated)' : ''}`);
        if (!st.ok && st.out) console.log(st.out.split('\n').map((l) => '       ' + l).join('\n'));
        if (!st.ok) ok = false;
      }
      console.log('');
      console.log('To let a client trust this authority, export it:');
      console.log('  ' + trustAnchorHint(env));
      return ok ? 0 : 1;
    }

    case 'zis': {
      const sub = argv[1];
      switch (sub) {
        case 'configure': {
          console.log('=== 1. datasets ==='); const a = printSteps(zis.provisionDatasets(env));
          console.log('=== 2. build ===');    const b = printSteps([zis.buildZis(env)]);
          console.log('=== 3. parmlib ===');  const c = printSteps([zis.writeParmlib(env)]);
          console.log('=== 4. server job ==='); const d = printSteps([zis.writeServerJob(env)]);
          console.log('=== 5. APF ===');      const e = printSteps([zis.apfAuthorize(env)]);
          console.log('=== 6. ZWES.IS ===');  const f = printSteps([zis.checkZwesIs(env)]);
          console.log('');
          console.log('APF added with SETPROG is lost at the next IPL. For something durable, ask a');
          console.log(`systems programmer to add ${env.zisLoadlib} to a PROGxx PARMLIB member.`);
          return a && b && c && d && e && f ? 0 : 1;
        }
        case 'apf': {
          const r = zis.apfAuthorize(env);
          printSteps([r]);
          if (!r.issued && r.reply) console.log(r.reply.split('\n').map((l) => '       ' + l).join('\n'));
          return r.ok ? 0 : 1;
        }
        case 'start': {
          const r = zis.start(env);
          console.log(`  ${r.ok ? 'ok  ' : 'FAIL'} ZIS ${r.state}${r.jobid ? ' (' + r.jobid + ')' : ''}`);
          for (const d of r.diagnosis) console.log('       ' + d);
          if (!r.ok && r.diagnosis.some((d) => /Not APF-authorized/i.test(d))) {
            console.log('       -> run: zss-test zis apf    (and see the PROGxx note)');
          }
          return r.ok ? 0 : 1;
        }
        case 'stop': {
          const r = zis.stop(env);
          console.log(`  ${r.stopped ? 'ok  ' : 'FAIL'} ${r.how}`);
          return r.stopped ? 0 : 1;
        }
        case 'status': {
          const st = zis.status(env);
          console.log(`  ${st.found ? st.text : 'not running'}`);
          return st.executing ? 0 : 1;
        }
        default: usage(); return 2;
      }
    }

    case 'server': {
      const sub = argv[1];
      switch (sub) {
        case 'start': {
          const r = zss.start(env);
          if (!r.ok) { console.log('  FAIL ' + r.why); return 1; }
          console.log('  ok   ' + r.version);
          if (r.zis) console.log('  ' + (r.zisOk ? 'ok   ' : 'WARN ') + r.zis);
          if (!r.zisOk) console.log('       ZSS cannot authenticate anyone without ZIS; every request will be 401.');
          return 0;
        }
        case 'stop': {
          const r = zss.stop(env);
          console.log(`  ${r.stopped ? 'ok' : 'FAIL'} ${r.killed.length ? 'stopped pid ' + r.killed.join(', ') : 'was not running'}`);
          return r.stopped ? 0 : 1;
        }
        case 'status': {
          const pids = zss.running(env);
          console.log(pids.length ? `  running, pid ${pids.join(', ')}` : '  not running');
          const log = zss.logFile(env);
          if (fs.existsSync(log)) {
            for (const key of ['ZWES1013I', 'ZWES1014I']) {
              const line = fs.readFileSync(log, 'utf8').split('\n').find((l) => l.includes(key));
              if (line) console.log('  ' + line.trim());
            }
          }
          return pids.length ? 0 : 1;
        }
        default: usage(); return 2;
      }
    }

    case 'compare': {
      const before = argValue(argv, '--before');
      const after = argValue(argv, '--after');
      const dashdash = argv.indexOf('--');
      const command = dashdash >= 0 ? argv.slice(dashdash + 1) : [];
      if (!before || !after || command.length === 0) {
        console.error('usage: compare --before <image> --after <image> -- <test command>');
        return 2;
      }
      const c = compare(env, { before, after, command });
      return report(c);
    }

    default:
      console.error('unknown command: ' + cmd);
      usage();
      return 2;
  }
}

main().then((code) => process.exit(code),
  (e) => { console.error('zss-test failed: ' + (e?.stack ?? e)); process.exit(2); });
