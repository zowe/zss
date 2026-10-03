/*
  This program and the accompanying materials are made available
  under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/**
 * configure.ts - generate the site-specific config from the template.
 *
 * zowe.yaml is site state, not source: it is git-ignored and regenerated.
 * zowe.yaml.template is the thing to edit for a change everyone should get.
 */

import * as fs from 'node:fs';
import * as path from 'node:path';
import { TestEnv } from './env';

export interface GenerateResult {
  ok: boolean;
  detail: string;
}

export function generateConfig(env: TestEnv, force: boolean): GenerateResult {
  const template = path.join(env.support, 'zowe.yaml.template');
  const target = path.join(env.support, 'zowe.yaml');

  if (!fs.existsSync(template)) {
    return { ok: false, detail: `${template} not found` };
  }
  if (fs.existsSync(target) && !force) {
    return { ok: true, detail: `${target} exists; leaving it alone (pass --force to regenerate)` };
  }

  const values: Record<string, string> = {
    INST: env.instance,
    PORT: String(env.port),
    ADDR: env.addr,
    KEYRING: env.keyring,
    LABEL: env.label,
    ZISNAME: env.zisName,
  };

  let text = fs.readFileSync(template, 'utf8');
  for (const [k, v] of Object.entries(values)) {
    text = text.split(`@${k}@`).join(v);
  }

  /* Refuse to write a half-substituted config: the server would start, validate,
     and behave in a way nobody could explain. */
  const leftover = text.match(/@[A-Z][A-Z0-9_]*@/g);
  if (leftover) {
    return { ok: false, detail: `template has placeholders this build does not know: ${[...new Set(leftover)].join(', ')}` };
  }

  fs.writeFileSync(target, text);
  return { ok: true, detail: `wrote ${target}` };
}
