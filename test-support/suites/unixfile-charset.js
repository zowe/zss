'use strict';
/*
 * unixfile-charset.js - /unixfile charset regression suite (zss#828 family).
 *
 * Prereqs: a running ZSS and two server-side files in $ZSS_TEST_DIR:
 *   ascii.txt           "plain ascii line\n"                      (pure ASCII)
 *   utf8-multibyte.txt  "cafe" e-acute em-dash "au" coffee-cup CJK "ok\n"
 *                       = 636166c3a9e280946175e2989520e697a5e69cace8aa9e206f6b0a
 * The WSL sandbox (test-support/wsl/run.sh) creates both. On z/OS, tag the
 * utf8 file 1208 (see tests/unixfile-charset-repro).
 *
 * Cases 4-5 assert the FIXED behavior of the streaming converter
 * (zowe-common-c#630): a build with the old converter returns an empty body
 * for case 5 - that failure is this suite catching the regression.
 */
const UTF8 = '636166c3a9e280946175e2989520e697a5e69cace8aa9e206f6b0a';

module.exports = {
  name: 'unixfile-charset (#828)',
  cases: [
    { name: 'auth required: no credentials -> 401',
      path: '/unixfile/contents${DIR}/ascii.txt', auth: false,
      expect: { status: 401 } },

    { name: 'ascii identity: body round-trips exactly',
      path: '/unixfile/contents${DIR}/ascii.txt',
      expect: { status: 200, decodedAscii: 'plain ascii line\n' } },

    { name: 'utf8 default GET: 200 and never a silent empty body',
      path: '/unixfile/contents${DIR}/utf8-multibyte.txt',
      expect: { status: 200, nonEmpty: true } },

    { name: 'utf8 forced 1208->1208: exact byte round-trip',
      path: '/unixfile/contents${DIR}/utf8-multibyte.txt',
      query: 'force=enable&source=1208&target=1208',
      expect: { status: 200, decodedHex: UTF8 } },

    { name: 'utf8 forced 1208->819: lossy target substitutes, not empty (#828 core)',
      path: '/unixfile/contents${DIR}/utf8-multibyte.txt',
      query: 'force=enable&source=1208&target=819',
      expect: { status: 200, nonEmpty: true,
                hexIncludes: ['636166e9'],   /* "caf" + e-acute as real 819 0xE9 */
                hexExcludes: ['c3a9'] } },   /* no un-converted UTF-8 leaks through */

    /* Forced-encoding error contract: a request that forces a source/target
       CCSID the server cannot stream-convert (e.g. target=37) must be REJECTED
       up front (400), never answered with a silent empty 200. That contract
       lands with zowe-common-c#630; the assertion cases (expect status 400)
       are added here once servers carry it, so this suite stays green against
       both current and fixed builds until then. */
  ],
};
