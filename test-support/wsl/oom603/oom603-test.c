/*
 * oom603-test.c -- WSL smoke test for PR #603 ("avoid ABEND when receiving
 * request content and chunks"). The portable, in-CI-able version of ChongZhou's
 * z/OS-only bigreqtest.c.
 *
 * #603 changes the request-body allocation in processHttpFragment to call
 * SLHAlloc2(..., suppressAbend=true): when the parser's short-lived heap can't
 * hold the body, it must return httpReasonCode 500 instead of SLHAlloc's
 * deliberate NULL-write ABEND (which on WSL is a SEGV, on z/OS an S0C4).
 *
 * We drive the REAL parser with a small SLH (2x64K) and an oversized body, both
 * fixed-length and chunked, and assert 500 + no crash. A normal small request
 * must still be served. Run the same binary built from the base tree and it
 * SEGVs at the ABEND -- that A/B is what proves the fix matters.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zowetypes.h"
#include "alloc.h"
#include "logging.h"
#include "httpserver.h"

static int drive(const char *name, int blocks, const char *req, int wantReason, int wantServed) {
  ShortLivedHeap *slh = makeShortLivedHeap(65536, blocks);
  HttpRequestParser *parser = makeHttpRequestParser(slh);
  processHttpFragment(parser, (char *)req, (int)strlen(req));   /* the #603 code path */
  HttpRequest *r = dequeueHttpRequest(parser);
  int served = (r != NULL);
  int reason = parser->httpReasonCode;
  int ok = (reason == wantReason) && (served == wantServed);
  printf("  %s %-22s httpReasonCode=%d served=%d\n", ok ? "PASS" : "FAIL", name, reason, served);
  SLHFree(slh);
  return ok;
}

int main(void) {
  LoggingContext *lc = makeLoggingContext();
  logConfigureStandardDestinations(lc);
  logConfigureComponent(lc, LOG_COMP_HTTPSERVER, "httpserver", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);

  int ok = 1;
  /* fixed-body: Content-Length 500000 >> 2x64K heap -> alloc fails at header end */
  ok &= drive("fixed-body OOM", 2,
    "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 500000\r\n\r\n", 500, 0);
  /* chunked: one chunk of 0x7a120 (500000) bytes -> alloc fails at the size line */
  ok &= drive("chunked OOM", 2,
    "POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n7a120\r\n", 500, 0);
  /* regression: a normal small chunked body still parses on the small heap */
  ok &= drive("normal small request", 2,
    "POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nAAA\r\n0\r\n\r\n", 0, 1);

  printf(ok ? "== #603 WSL smoke: PASS (OOM -> 500, no ABEND; normal still served) ==\n"
            : "== #603 WSL smoke: FAIL ==\n");
  return ok ? 0 : 1;
}
