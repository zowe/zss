/*
  This program and the accompanying materials are made available
  under the terms of the Eclipse Public License v2.0 which
  accompanies this distribution, and is available at
  https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/**
 * cert.ts - a SAF key ring and server certificate for the test ZSS.
 *
 * Distilled from the ZWEKRING sample, independent of zwe. Creates a local
 * certificate authority, a server certificate signed by it, and a ring holding
 * both, all owned by the invoking userid and all disposable.
 *
 * There is no password anywhere, which is the reason to prefer a SAF key ring
 * over a PKCS#12 file for testing: the ring is protected by SAF, so nothing has
 * to be stored or passed around.
 *
 * Needs CONTROL on IRR.DIGTCERT.GENCERT, .ADDRING and .CONNECT. The final
 * refresh needs RACF SPECIAL and is allowed to fail: the ring still works for
 * the userid that owns it.
 */

import * as z from './zos';
import { TestEnv } from './env';

export interface CertStep {
  what: string;
  ok: boolean;
  /** RACF says useful things while exiting non-zero, so the text is kept. */
  out: string;
  /** True when a non-zero result is expected and harmless. */
  tolerated?: boolean;
}

export interface CertOptions {
  /** Certificate CN and SAN domain. Defaults to this system's hostname, because
   *  a certificate naming someone else's machine is worse than useless. */
  host?: string;
  ip?: string;
  caLabel?: string;
  notAfter?: string;
  /** Cosmetic for a throwaway authority; no location, deliberately. */
  dn?: string;
}

export function provision(env: TestEnv, opts: CertOptions = {}): CertStep[] {
  const host = opts.host ?? z.hostname();
  const ip = opts.ip ?? env.addr;
  const caLabel = opts.caLabel ?? 'ZOWELOCALCA';
  const notAfter = opts.notAfter ?? '2031-12-31';
  const dn = opts.dn ?? "OU('ZOWE') O('Zowe Test') C('US')";
  const id = env.userid;
  const steps: CertStep[] = [];

  const step = (what: string, command: string, tolerated = false): CertStep => {
    const r = z.tso(command);
    const s: CertStep = { what, ok: r.ok || tolerated, out: r.out.trim(), tolerated };
    steps.push(s);
    return s;
  };

  /* ADDRING is tolerated: a ring that already exists is the normal case on a
     re-run, and RACF reports that as an error. */
  step(`key ring ${id}/${env.ring}`, `RACDCERT ADDRING(${env.ring}) ID(${id})`, true);

  step(`local CA ${caLabel}`,
    `RACDCERT GENCERT CERTAUTH SUBJECTSDN(CN('ZOWE LOCAL CA') ${dn}) SIZE(2048) ` +
    `NOTAFTER(DATE(${notAfter})) WITHLABEL('${caLabel}') KEYUSAGE(CERTSIGN)`, true);

  step('connect the CA to the ring',
    `RACDCERT CONNECT(CERTAUTH LABEL('${caLabel}') RING(${env.ring}) USAGE(CERTAUTH)) ID(${id})`, true);

  step(`server certificate CN=${host}`,
    `RACDCERT GENCERT ID(${id}) SUBJECTSDN(CN('${host}') ${dn}) SIZE(2048) ` +
    `NOTAFTER(DATE(${notAfter})) WITHLABEL('${env.label}') KEYUSAGE(HANDSHAKE) ` +
    `ALTNAME(IP(${ip}) DOMAIN('${host}')) SIGNWITH(CERTAUTH LABEL('${caLabel}'))`, true);

  step('connect the server certificate as default',
    `RACDCERT CONNECT(ID(${id}) LABEL('${env.label}') RING(${env.ring}) USAGE(PERSONAL) DEFAULT) ID(${id})`, true);

  /* Needs RACF SPECIAL. Tolerated because the ring is usable by its owner
     without it, and a developer userid will not have it. */
  step('refresh DIGTCERT/DIGTRING (needs RACF SPECIAL)',
    'SETROPTS RACLIST(DIGTCERT,DIGTRING) REFRESH', true);

  /* The one that actually decides whether this worked. */
  const listing = z.tso(`RACDCERT LISTRING(${env.ring}) ID(${id})`);
  const hasServer = listing.out.includes(env.label);
  const hasCa = listing.out.includes(caLabel);
  steps.push({
    what: 'verify the ring holds both certificates',
    ok: hasServer && hasCa,
    out: hasServer && hasCa
      ? `${env.label} and ${caLabel} are both connected to ${env.ring}`
      : `ring listing does not show ${!hasServer ? env.label : ''}${!hasServer && !hasCa ? ' and ' : ''}${!hasCa ? caLabel : ''}\n${listing.out.trim()}`,
  });

  return steps;
}

/** How to export the authority so a client can trust it. Not done here: the
 *  drivers in this harness do not verify the certificate, so they do not need
 *  it, and writing a PEM nobody asked for is clutter. */
export function trustAnchorHint(env: TestEnv, caLabel = 'ZOWELOCALCA'): string {
  return `tsocmd "RACDCERT CERTAUTH EXPORT(LABEL('${caLabel}')) ` +
         `DSN('${env.userid}.ZOWECA.PEM') FORMAT(CERTB64)"`;
}
