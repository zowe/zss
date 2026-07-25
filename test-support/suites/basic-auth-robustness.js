'use strict';
/*
 * basic-auth-robustness.js -- regression suite for extractBasicAuth
 * (zowe-common-c #647). The old parser mishandled malformed
 * `Authorization: Basic <base64>` values: a decoded string with no colon hit
 * `if (colonPos)` with colonPos == -1 and entered the block, writing
 * authString[-1] and over-copying the password buffer; base64 that fails to
 * decode (length not a multiple of 4) returned -1 and was used as an index/
 * length. Both are out-of-bounds. #647 rejects these up front with FALSE.
 *
 * Oracle: a malformed credential must yield a clean 401 AND leave the server
 * serving (the trailing liveness case). On a build WITHOUT #647, an ASan
 * sandbox reports the overflow; a plain build may crash (connection reset) or,
 * on z/OS, abend or silently corrupt -- so the liveness case is the portable
 * tell. Drive with header-override; runs against any ZSS (WSL sandbox or z/OS)
 * via ZSS_URL/USER/PASS.
 *
 * STATUS: accumulated on the test-infra branch; A/B against an ASan sandbox
 * still to be run. Expectations encode the FIXED (#647) behavior.
 */
function basicHeader(raw) { return { Authorization: 'Basic ' + raw }; }
const b64 = (s) => Buffer.from(s).toString('base64');

module.exports = {
  name: 'basic-auth robustness (#647)',
  cases: [
    { name: 'malformed: decoded credential has no colon -> clean 401, no OOB',
      path: '/unixfile/contents${DIR}/ascii.txt',
      headers: basicHeader(b64('nocolonhere')),
      expect: { status: 401 } },

    { name: 'malformed: base64 not a multiple of 4 -> clean 401, no negative decode',
      path: '/unixfile/contents${DIR}/ascii.txt',
      headers: basicHeader('bm9jb2xvbg'),   /* "nocolon" minus padding: decode fails (-1) */
      expect: { status: 401 } },

    { name: 'malformed: empty credential -> clean 401',
      path: '/unixfile/contents${DIR}/ascii.txt',
      headers: basicHeader(''),
      expect: { status: 401 } },

    { name: 'liveness: server still serves a valid request after the malformed ones',
      path: '/unixfile/contents${DIR}/ascii.txt',
      expect: { status: 200, decodedAscii: 'plain ascii line\n' } },
  ],
};
