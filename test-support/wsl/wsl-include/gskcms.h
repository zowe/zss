#ifndef ZOWE_WSL_GSKCMS_STUB_H
#define ZOWE_WSL_GSKCMS_STUB_H
/*
 * Minimal non-z/OS stub for <gskcms.h> (z/OS System SSL Certificate Management).
 *
 * Why this exists: jwk.h includes <gskcms.h> unconditionally and embeds one GSK
 * type by value (x509_public_key_info) in JwkContext. On z/OS that comes from
 * the 3300-line System SSL header; on WSL/Linux there is no GSK, and the whole
 * JWK/APIML path is disabled in this dev build, so the type is never touched at
 * runtime -- only its existence and a plausible size matter so headers parse and
 * containing structs size correctly. Files that actually call the GSK or CMS
 * routines (tls.c, jwk.c, httpclient.c) still fail to compile here and are
 * self-excluded by build.sh; their symbols are satisfied by wsl-stubs.c.
 *
 * This lives in the repo (test-support/wsl/wsl-include) so the WSL build stays
 * self-contained and cannot silently decay again.
 */

/* Opaque, over-sized on purpose: never dereferenced on non-z/OS. */
typedef struct x509_public_key_info_stub {
  unsigned char opaque[512];
} x509_public_key_info;

#endif /* ZOWE_WSL_GSKCMS_STUB_H */
