/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

/*
 * allocTest - probes available memory in two z/OS address-space regions:
 *
 *   Above the 2 GB bar  (64-bit virtual, gated by MEMLIMIT)     via malloc()
 *   Below the 2 GB bar  (31-bit virtual, gated by ASSIZEMAX/ulimit -A)  via malloc31()
 *
 * Each region is probed with a decreasing power-of-2 loop from --max64 / --max31
 * down to --min.  The loop stops on the first successful allocation and reports
 * that size.  No pages are touched (no memset), so the probes complete in
 * microseconds even for multi-gigabyte sizes.
 *
 * Usage:
 *   alloc-test [--max64 <size>] [--max31 <size>] [--min <size>]
 *
 * Size format: plain bytes, or a suffix of K / M / G  (case-insensitive).
 *
 * Defaults:
 *   --max64  4G
 *   --max31  512M
 *   --min    32M
 *
 * Constraints enforced at startup:
 *   --min must be >= 1 and <= INT_MAX
 *   --min must be < --max64
 *   --min must be < --max31
 *
 * Exit codes:
 *   0  both regions found at least one successful size >= --min
 *   8  one or both regions could not allocate even --min bytes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#include "copyright.h"
#include "zowetypes.h"
#include "alloc.h"

#define ALLOC_STATUS_OK    0
#define ALLOC_STATUS_ERROR 8

#define MAX_PROBE_BYTES (16ULL * 1024 * 1024 * 1024)  /* 16G absolute ceiling */

/* ------------------------------------------------------------------ */
/* Size parsing                                                         */
/* ------------------------------------------------------------------ */

/*
 * parseSize - convert a human-readable size string to uint64_t bytes.
 * Accepts: plain decimal integer, or decimal + K / M / G suffix.
 * Returns 0 and sets *ok = FALSE on any parse error or overflow.
 */
static uint64_t parseSize(const char *str, int *ok) {
  if (!str || *str == '\0') {
    *ok = FALSE;
    return 0;
  }

  char *end;
  uint64_t value = (uint64_t)strtoull(str, &end, 10);

  if (end == str) {
    /* no digits consumed */
    *ok = FALSE;
    return 0;
  }

  /* optional suffix */
  if (*end == 'k' || *end == 'K') {
    if (value > (uint64_t)UINT64_MAX / 1024) {
      *ok = FALSE;
      return 0;
    }
    value *= 1024;
    end++;
  } else if (*end == 'm' || *end == 'M') {
    if (value > (uint64_t)UINT64_MAX / (1024 * 1024)) {
      *ok = FALSE;
      return 0;
    }
    value *= 1024 * 1024;
    end++;
  } else if (*end == 'g' || *end == 'G') {
    if (value > (uint64_t)UINT64_MAX / (1024 * 1024 * 1024)) {
      *ok = FALSE;
      return 0;
    }
    value *= 1024 * 1024 * 1024;
    end++;
  }

  if (*end != '\0') {
    /* trailing garbage */
    *ok = FALSE;
    return 0;
  }

  *ok = TRUE;
  return value;
}

/* Format a uint64_t byte count as a human-readable string (e.g. "4G", "512M"). */
static void formatSize(uint64_t bytes, char *buf, size_t bufLen) {
  if (bytes >= 1024 * 1024 * 1024 && bytes % (1024 * 1024 * 1024) == 0) {
    snprintf(buf, bufLen, "%lluG", (unsigned long long)(bytes / (1024 * 1024 * 1024)));
  } else if (bytes >= 1024 * 1024 && bytes % (1024 * 1024) == 0) {
    snprintf(buf, bufLen, "%lluM", (unsigned long long)(bytes / (1024 * 1024)));
  } else if (bytes >= 1024 && bytes % 1024 == 0) {
    snprintf(buf, bufLen, "%lluK", (unsigned long long)(bytes / 1024));
  } else {
    snprintf(buf, bufLen, "%llu", (unsigned long long)bytes);
  }
}

/* ------------------------------------------------------------------ */
/* Argument helpers                                                     */
/* ------------------------------------------------------------------ */

static const char *getKeywordArg(const char *key, int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], key) == 0 && (i + 1) < argc) {
      return argv[i + 1];
    }
  }
  return NULL;
}

static int hasFlag(const char *flag, int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], flag) == 0) {
      return TRUE;
    }
  }
  return FALSE;
}

/* ------------------------------------------------------------------ */
/* Probe loops                                                          */
/* ------------------------------------------------------------------ */

/*
 * probe64 - probe above-bar (64-bit) memory using stdlib malloc().
 * On z/OS LE compiled -q64, malloc() allocates above the 2 GB bar and is
 * gated by MEMLIMIT.  Returns the largest successful allocation in bytes,
 * or 0 if even minBytes could not be allocated.
 */
static uint64_t probe64(uint64_t maxBytes, uint64_t minBytes) {
  for (uint64_t size = maxBytes; size >= minBytes; size >>= 1) {
    void *p = malloc((size_t)size);
    if (p != NULL) {
      free(p);
      return size;
    }
  }
  return 0;
}

/*
 * probe31 - probe below-bar (31-bit) memory using malloc31() from alloc.c.
 * malloc31() calls __malloc31() on z/OS LE, which allocates below the 2 GB
 * bar and is gated by ASSIZEMAX / ulimit -A.
 *
 * malloc31() takes an int, so maxBytes is clamped to INT_MAX.
 * No memset is performed; pages are not touched.
 */
static uint64_t probe31(uint64_t maxBytes, uint64_t minBytes) {
  /* malloc31 signature is: char *malloc31(int size) */
  uint64_t startBytes = maxBytes;
  if (startBytes > (uint64_t)INT_MAX) {
    startBytes = (uint64_t)INT_MAX;
  }

  for (uint64_t size = startBytes; size >= minBytes; size >>= 1) {
    char *p = malloc31((int)size);
    if (p != NULL) {
      free31(p, (int)size);
      return size;
    }
  }
  return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {

  if (argc == 1 || hasFlag("-h", argc, argv) || hasFlag("--help", argc, argv)) {
    printf("alloc-test - Probes available memory in two z/OS address-space regions.\n");
    printf("  Regions probed:\n");
    printf("    64-bit (above 2GB bar) via malloc()    -- gated by MEMLIMIT\n");
    printf("    31-bit (below 2GB bar) via malloc31()  -- gated by ASSIZEMAX / ulimit -A\n");
    printf("  Format: alloc-test [--max64 <size>] [--max31 <size>] [--min <size>]\n");
    printf("  Size format: plain bytes, or suffix K/M/G (e.g. 4G, 512M, 65536K)\n");
    printf("  Defaults: --max64 4G  --max31 512M  --min 32M\n");
    printf("  Exit values: 0 if both regions allocated at least --min bytes, 8 otherwise\n");
    return ALLOC_STATUS_OK;
  }

  /* ---- parse arguments ---- */
  int ok;
  const char *max64Str = getKeywordArg("--max64", argc, argv);
  const char *max31Str = getKeywordArg("--max31", argc, argv);
  const char *minStr   = getKeywordArg("--min",   argc, argv);

  uint64_t max64    = 4ULL * 1024 * 1024 * 1024;  /* 4G  — ULL required: 4G overflows int32 */
  uint64_t max31    = 512 * 1024 * 1024;           /* 512M */
  uint64_t minBytes = 32 * 1024 * 1024;            /* 32M  */

  if (max64Str != NULL) {
    ok = FALSE;
    max64 = parseSize(max64Str, &ok);
    if (!ok || max64 == 0) {
      fprintf(stderr, "Error: invalid --max64 value '%s'\n", max64Str);
      return ALLOC_STATUS_ERROR;
    }
    if (max64 > MAX_PROBE_BYTES) {
      fprintf(stderr, "Error: --max64 value '%s' exceeds the 16G absolute maximum\n", max64Str);
      return ALLOC_STATUS_ERROR;
    }
  }

  if (max31Str != NULL) {
    ok = FALSE;
    max31 = parseSize(max31Str, &ok);
    if (!ok || max31 == 0) {
      fprintf(stderr, "Error: invalid --max31 value '%s'\n", max31Str);
      return ALLOC_STATUS_ERROR;
    }
    if (max31 > MAX_PROBE_BYTES) {
      fprintf(stderr, "Error: --max31 value '%s' exceeds the 16G absolute maximum\n", max31Str);
      return ALLOC_STATUS_ERROR;
    }
    /* malloc31() takes int; cap at INT_MAX */
    if (max31 > (uint64_t)INT_MAX) {
      fprintf(stderr, "Error: --max31 value '%s' exceeds INT_MAX (%d); "
              "31-bit allocations are limited to 2GB\n", max31Str, INT_MAX);
      return ALLOC_STATUS_ERROR;
    }
  }

  if (minStr != NULL) {
    ok = FALSE;
    minBytes = parseSize(minStr, &ok);
    if (!ok || minBytes == 0) {
      fprintf(stderr, "Error: invalid --min value '%s'\n", minStr);
      return ALLOC_STATUS_ERROR;
    }
  }

  /* ---- validate --min constraints ---- */
  if (minBytes > (uint64_t)INT_MAX) {
    fprintf(stderr, "Error: --min value exceeds INT_MAX (%d); "
            "must be a valid 31-bit allocation size\n", INT_MAX);
    return ALLOC_STATUS_ERROR;
  }
  if (minBytes >= max64) {
    char minBuf[32], maxBuf[32];
    formatSize(minBytes, minBuf, sizeof(minBuf));
    formatSize(max64, maxBuf, sizeof(maxBuf));
    fprintf(stderr, "Error: --min (%s) must be less than --max64 (%s)\n",
            minBuf, maxBuf);
    return ALLOC_STATUS_ERROR;
  }
  if (minBytes >= max31) {
    char minBuf[32], maxBuf[32];
    formatSize(minBytes, minBuf, sizeof(minBuf));
    formatSize(max31, maxBuf, sizeof(maxBuf));
    fprintf(stderr, "Error: --min (%s) must be less than --max31 (%s)\n",
            minBuf, maxBuf);
    return ALLOC_STATUS_ERROR;
  }

  /* ---- run probes ---- */
  char sizeBuf[32];
  int status = ALLOC_STATUS_OK;

  /* 64-bit probe */
  uint64_t result64 = probe64(max64, minBytes);
  if (result64 > 0) {
    formatSize(result64, sizeBuf, sizeof(sizeBuf));
    printf("64-bit allocation succeeded at %s\n", sizeBuf);
  } else {
    formatSize(minBytes, sizeBuf, sizeof(sizeBuf));
    printf("64-bit allocation failed: could not allocate %s above the 2GB bar "
           "(check MEMLIMIT in RACF/TSS/ACF2 OMVS segment)\n", sizeBuf);
    status = ALLOC_STATUS_ERROR;
  }

  /* 31-bit probe */
  uint64_t result31 = probe31(max31, minBytes);
  if (result31 > 0) {
    formatSize(result31, sizeBuf, sizeof(sizeBuf));
    printf("31-bit allocation succeeded at %s\n", sizeBuf);
  } else {
    formatSize(minBytes, sizeBuf, sizeof(sizeBuf));
    printf("31-bit allocation failed: could not allocate %s below the 2GB bar "
           "(check ASSIZEMAX in RACF/TSS/ACF2 OMVS segment or ulimit -A)\n", sizeBuf);
    status = ALLOC_STATUS_ERROR;
  }

  return status;
}

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
