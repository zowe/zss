/*
 * chunk640-harness.c -- drive the REAL zowe-common-c HTTP chunk parser to
 * verify PR #640 (chunked request-body size-overflow guard in
 * processHttpFragment). Two modes, one code path (the real parser):
 *
 *   chunk640-harness --self      run built-in chunked requests in-process and
 *                                assert the reassembled body. No socket/client.
 *                                This is the z/OS path (Marist has no curl, and
 *                                a driver script's own source charset is a
 *                                separate hazard). Works on WSL too, so the same
 *                                cases give cross-platform parity.
 *   chunk640-harness <port>      listen on 127.0.0.1:<port>, feed socket bytes
 *                                to the parser, echo the reassembled body. This
 *                                is the WSL path (drive with curl).
 *
 * Why a harness and not zssServer: #640 only touches processHttpFragment, which
 * runs during request *reading*, before dispatch/auth/TLS. So we link just that
 * path + real deps + no-op stubs for what the parser never reaches.
 *
 * Charset (z/OS): the parser works on ASCII wire bytes (as the real server's
 * socket delivers). In --self mode the request is a C string literal, which is
 * EBCDIC under xlclang, so we __etoa it to ASCII before feeding; the reassembled
 * body is ASCII, so we __atoe a copy to EBCDIC only for readable display. On WSL
 * (__ZOWE_OS_LINUX) all of this is a no-op -- literals are already ASCII.
 *
 * Oracle: multi-chunk "AAA"+"BBB"+"CCC" must reassemble to contentLength 9, body
 * "AAABBBCCC" -- the carry-forward memcpy (#640's changed region) runs for every
 * chunk after the first.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "zowetypes.h"   /* defines __ZOWE_OS_ZOS / __ZOWE_OS_LINUX */
#include "alloc.h"       /* makeShortLivedHeap */
#include "logging.h"     /* makeLoggingContext, logConfigure* -- httpserver traces need it */
#include "httpserver.h"  /* HttpRequestParser, processHttpFragment, dequeueHttpRequest */

/* __etoa_l / __atoe_l (in-place charset conversion) are declared by <stdlib.h>
 * on z/OS; we call them only under #ifdef __ZOWE_OS_ZOS. */

static void initLogging(void) {
  /* httpserver.c calls zowelog(NULL,...) while parsing; that resolves to the
   * global logging context, which must exist or getComponent derefs NULL. */
  LoggingContext *logContext = makeLoggingContext();
  logConfigureStandardDestinations(logContext);
  logConfigureComponent(logContext, LOG_COMP_HTTPSERVER, "httpserver",
                        LOG_DEST_PRINTF_STDOUT, ZOWE_LOG_INFO);
}

/* Feed one request through the real parser and check the reassembled body.
 * reqLit / wantBodyLit are C literals (EBCDIC on z/OS -> converted to ASCII). */
static int feedAndCheck(const char *name, const char *reqLit, int wantLen, const char *wantBodyLit) {
  char req[4096];
  int reqLen = (int) strlen(reqLit);
  memcpy(req, reqLit, reqLen);
#ifdef __ZOWE_OS_ZOS
  __etoa_l(req, reqLen);                       /* EBCDIC literal -> ASCII wire bytes */
#endif
  ShortLivedHeap *slh = makeShortLivedHeap(65536, 100);
  HttpRequestParser *parser = makeHttpRequestParser(slh);
  processHttpFragment(parser, req, reqLen);    /* <-- the #640 code path */
  HttpRequest *r = dequeueHttpRequest(parser);

  char want[520];
  int wl = (int) strlen(wantBodyLit);
  memcpy(want, wantBodyLit, wl); want[wl] = 0;
#ifdef __ZOWE_OS_ZOS
  __etoa_l(want, wl);                          /* expected -> ASCII, to match contentBody */
#endif
  int ok = r && r->contentLength == wantLen && r->contentBody
           && wl == wantLen && !memcmp(r->contentBody, want, wl);

  char disp[520]; int dl = 0;
  if (r && r->contentBody) {
    dl = r->contentLength < 512 ? r->contentLength : 512;
    memcpy(disp, r->contentBody, dl); disp[dl] = 0;
#ifdef __ZOWE_OS_ZOS
    __atoe_l(disp, dl);                        /* ASCII body -> EBCDIC for the shell */
#endif
  } else { disp[0] = 0; }

  printf("  %s %-16s contentLength=%d body=%s\n", ok ? "PASS" : "FAIL", name,
         r ? r->contentLength : -1, disp);
  return ok;
}

static int selfTest(void) {
  int ok = 1;
  ok &= feedAndCheck("single-chunk",
    "POST /e HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n"
    "9\r\nAAABBBCCC\r\n0\r\n\r\n", 9, "AAABBBCCC");
  ok &= feedAndCheck("multi-chunk-3",
    "POST /e HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n"
    "3\r\nAAA\r\n3\r\nBBB\r\n3\r\nCCC\r\n0\r\n\r\n", 9, "AAABBBCCC");
  ok &= feedAndCheck("multi-chunk-4",
    "POST /e HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n"
    "1\r\n1\r\n2\r\n22\r\n3\r\n333\r\n4\r\n4444\r\n0\r\n\r\n", 10, "1223334444");
  printf(ok ? "== RESULT: PASS (real #640 code reassembles chunked bodies) ==\n"
            : "== RESULT: FAIL ==\n");
  return ok;
}

static int serveMode(int port) {
  int ls = socket(AF_INET, SOCK_STREAM, 0);
  int one = 1;
  setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
  if (bind(ls, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
  listen(ls, 4);
  fprintf(stderr, "chunk640-harness: listening on 127.0.0.1:%d\n", port);
  fflush(stderr);

  for (;;) {
    int cs = accept(ls, NULL, NULL);
    if (cs < 0) continue;
    ShortLivedHeap *slh = makeShortLivedHeap(65536, 100);
    HttpRequestParser *parser = makeHttpRequestParser(slh);
    HttpRequest *req = NULL;
    char buf[4096];
    while (req == NULL) {
      int n = read(cs, buf, sizeof(buf));
      if (n <= 0) break;
      processHttpFragment(parser, buf, n);       /* <-- the #640 code path */
      if (parser->httpReasonCode >= 400) break;
      req = dequeueHttpRequest(parser);
    }
    char resp[1024]; int rlen;
    if (req && req->contentBody && req->contentLength > 0) {
      int m = req->contentLength < 512 ? req->contentLength : 512;
      char body[513];
      memcpy(body, req->contentBody, m); body[m] = 0;
#ifdef __ZOWE_OS_ZOS
      __atoe_l(body, m);   /* make body EBCDIC so the whole resp is uniform before -> ASCII */
#endif
      rlen = snprintf(resp, sizeof(resp),
               "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n"
               "reassembled contentLength=%d body=%s\n", req->contentLength, body);
    } else if (parser->httpReasonCode >= 400) {
      rlen = snprintf(resp, sizeof(resp),
               "HTTP/1.1 %d Rejected\r\nConnection: close\r\n\r\n"
               "parser rejected, httpReasonCode=%d\n", parser->httpReasonCode, parser->httpReasonCode);
    } else {
      rlen = snprintf(resp, sizeof(resp),
               "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\nno body parsed\n");
    }
    if (rlen >= (int)sizeof(resp)) rlen = sizeof(resp) - 1;
#ifdef __ZOWE_OS_ZOS
    __etoa_l(resp, rlen);  /* uniform-EBCDIC response -> ASCII wire bytes for the client */
#endif
    write(cs, resp, rlen);
    close(cs);
  }
  return 0;
}

int main(int argc, char **argv) {
  initLogging();
  if (argc > 1 && strcmp(argv[1], "--self") == 0) {
    return selfTest() ? 0 : 1;
  }
  return serveMode(argc > 1 ? atoi(argv[1]) : 8599);
}
