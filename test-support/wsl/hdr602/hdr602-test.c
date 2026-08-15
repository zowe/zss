/*
 * hdr602-test.c -- WSL test for PR #602 ("fix potential buf overrun in
 * statusReason and headerName" in the HTTP *client* response parser).
 *
 * #602 adds bounds checks to processHttpResponseFragment. The statusReason
 * check is correct. But pushCharToHeaderName checks
 *     hrp->headerNameLength >= sizeof(hrp->headerNameLength) - 1
 * i.e. sizeof the int LENGTH field (4), not sizeof(hrp->headerName) (the array)
 * -- so it caps header names at 3 bytes and rejects every real HTTP response.
 * ChongZhou's own test can't catch it: both the bug and the true limit return
 * ANSI_FAILED, so his long-header test passes for the wrong reason.
 *
 * This test discriminates: a NORMAL response with a normal header ("Content-
 * Length") must parse (ret != ANSI_FAILED). With the shipped #602 it fails; with
 * the one-word correction (sizeof(hrp->headerName)) it passes. A pathologically
 * long header name must still fail-safe (ANSI_FAILED, no overrun/crash).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "utils.h"
#include "httpclient.h"

#define HTTP_STATE_RESP_STATUS_VERSION 1
#define MAXRESP 65536

static HttpResponseParser *mk(void) {
  ShortLivedHeap *slh = makeShortLivedHeap(sizeof(HttpResponseParser) + MAXRESP, 8);
  HttpResponseParser *p = (HttpResponseParser *)SLHAlloc(slh, sizeof(HttpResponseParser));
  memset(p, 0, sizeof(HttpResponseParser));
  p->slh = slh;
  p->state = HTTP_STATE_RESP_STATUS_VERSION;
  p->specifiedContentLength = -1;
  return p;
}

int main(void) {
  LoggingContext *lc = makeLoggingContext();
  logConfigureStandardDestinations(lc);
  logConfigureComponent(lc, LOG_COMP_HTTPCLIENT, "httpclient", LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);

  int ok = 1;

  /* DISCRIMINATOR: a normal response with a real header must NOT be rejected. */
  {
    HttpResponseParser *p = mk();
    HttpClientResponse *r = NULL;
    char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    int ret = processHttpResponseFragment(p, resp, (int)strlen(resp), &r);
    int pass = (ret != ANSI_FAILED);
    printf("  %s normal response w/ 'Content-Length' header -> ret=%d %s\n",
           pass ? "PASS" : "FAIL", ret,
           pass ? "" : "(#602 as-shipped caps header names at 3 chars!)");
    ok &= pass;
  }

  /* Fix must still fail-safe on a pathologically long header name (no overrun). */
  {
    HttpResponseParser *p = mk();
    HttpClientResponse *r = NULL;
    char resp[4096];
    char big[2001];
    memset(big, 'H', 2000);
    big[2000] = '\0';
    int n = snprintf(resp, sizeof(resp), "HTTP/1.1 200 OK\r\n%s: v\r\n\r\n", big);
    int ret = processHttpResponseFragment(p, resp, n, &r);
    int pass = (ret == ANSI_FAILED);
    printf("  %s long (2000-char) header name -> ret=%d (want ANSI_FAILED, no overrun)\n",
           pass ? "PASS" : "FAIL", ret);
    ok &= pass;
  }

  printf(ok ? "== #602 hdr test: PASS ==\n" : "== #602 hdr test: FAIL ==\n");
  return ok ? 0 : 1;
}
