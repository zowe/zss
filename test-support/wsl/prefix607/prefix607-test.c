/*
 * prefix607-test.c -- WSL regression test for PR #607 ("Correct string length in
 * calling compareIgnoringCase").
 *
 * Bug: header matching used compareIgnoringCase(name, "Transfer-Encoding",
 * headerNameLength), a length-bounded PREFIX match -- so a received header named
 * "Transfer" (8 chars) matched "Transfer-Encoding" (plausible request-smuggling
 * relevance: a differential vs an upstream proxy). #607 adds
 * compareStringsIgnoringCase (full, null-terminated) and switches the header
 * sites to it.
 *
 * These cases use SAME-CASE strings on purpose: compareIgnoringCase's case
 * folding is EBCDIC-only (upchar uses c|0x40), so on ASCII/WSL case-insensitive
 * comparison does not work -- that's a pre-existing upchar portability gap, NOT
 * #607. The length/null logic #607 actually changed is charset-independent, so
 * same-case cases verify the fix off-platform. (ChongZhou's own tests/strcmptest.c
 * is a genuine unit test but uses mixed case, so it only passes on z/OS.)
 */
#include <stdio.h>
#include "utils.h"

#define CHECK(cond) do { printf("  %-6s %s\n", (cond) ? "PASS" : "FAIL", #cond); if(!(cond)) ok = 0; } while (0)

int main(void) {
  int ok = 1;

  /* the OLD behavior base callers relied on: length-bounded prefix match */
  CHECK(compareIgnoringCase("Transfer", "Transfer-Encoding", 8) == 0);        /* prefix "matches" */

  /* #607: full-string comparison rejects the prefix but keeps the exact match */
  CHECK(compareStringsIgnoringCase("Transfer", "Transfer-Encoding") != 0);    /* rejected */
  CHECK(compareStringsIgnoringCase("Transfer-Encoding", "Transfer-Encoding") == 0); /* exact ok */
  CHECK(compareStringsIgnoringCase("Content", "Content-Length") != 0);        /* another prefix rejected */
  CHECK(compareStringsIgnoringCase("Content-Length", "Content-Length") == 0);

  printf(ok ? "== #607 prefix test: PASS ==\n" : "== #607 prefix test: FAIL ==\n");
  return ok ? 0 : 1;
}
