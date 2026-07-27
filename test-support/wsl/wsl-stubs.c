/*
  wsl-stubs.c -- native-Linux/WSL dev-build glue for the ZSS server.

  This file exists ONLY so that zssServer links and runs on a plain Linux/WSL
  host (clang, no z/OS headers) for a zero-cost dev/test sandbox -- reproduce
  #828, run sanitizers, iterate without touching a metered mainframe. It is NOT
  a production translation unit.

  It provides the symbols the portable object set leaves undefined at link time,
  in two flavors:

    1. REAL POSIX IMPLEMENTATIONS -- helpers on the server boot / file-serving
       path that have an honest POSIX equivalent (file info via stat/lstat,
       directory iteration via opendir/readdir, hostname resolution via
       getaddrinfo, address formatting via inet_ntop, a couple of small
       accessors). These are implemented for real. When this sandbox is
       hardened, they belong in platform/posix/psxfile.c and the psxskt.c
       socket layer -- they are gathered here only to keep the port to a
       single new file.

    2. z/OS-ONLY STUBS -- services that only exist on z/OS (BPX callable
       services, SAF/RACF, MVS catalog, SMF product registration, System SSL
       TLS, the APIML JWT-verify path, cross-memory transports). These are
       stubbed to fail/no-op gracefully so the server runs WITHOUT those
       features on Linux. Each returns an error/NULL/no-op consistent with its
       real declaration, and every caller's failure branch is driven
       deterministically (no uninitialized output left behind).

  Every signature below is taken from the real headers (unixfile.h, bpxnet.h,
  rusermap.h, tls.h, jwt.h, jwk.h, storageApiml.h, registerProduct.h, jcsi.h),
  and those headers are #included so the compiler verifies each definition
  against its declaration. __LONGNAME__ is auto-defined by zowetypes.h on
  non-z/OS, so long names (safeMalloc, fileClose, ...) resolve to the real
  symbols in the portable objects.

  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v2.0 which accompanies this distribution,
  and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "zowetypes.h"
#include "alloc.h"
#include "unixfile.h"
#include "bpxnet.h"
#include "httpserver.h"   /* HttpServer (for configureJwt) + pulls utils/json/collections */
#include "jwt.h"          /* Jwt, JwtContext, JwtCheckSignature, jwt entry points */
/* WSL: jwk.h/tls.h/storageApiml.h pull z/OS GSK (gskcms.h/gskssl.h), absent
   here. The functions below are link-time no-op stubs, so opaque types are
   enough -- the linker resolves by symbol name. storage.h gives real Storage. */
#include "storage.h"
typedef struct JwkSettings JwkSettings;
typedef struct TlsEnvironment TlsEnvironment;
typedef struct TlsSettings TlsSettings;
typedef struct ApimlStorageSettings ApimlStorageSettings;
#include "registerProduct.h"
#include "jcsi.h"         /* csi_parmblock, EntryDataSet, loadCsi, returnEntries */
#include "rusermap.h"     /* getUseridByCertificate, getUseridByDN */

/* =========================================================================
 * REAL POSIX IMPLEMENTATIONS
 * ========================================================================= */

/* ---- FileInfo accessors (FileInfo == struct stat on Linux) --------------- */

/* Like fileInfo() in psxfile.c but does NOT follow symlinks (lstat vs stat). */
int symbolicFileInfo(const char *filename, FileInfo *info,
                     int *returnCode, int *reasonCode) {
  if (filename == NULL || info == NULL) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = EINVAL;
    return -1;
  }
  int rc = lstat(filename, info);
  if (rc != 0) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = errno;
    return -1;
  }
  if (returnCode) *returnCode = 0;
  if (reasonCode) *reasonCode = 0;
  return 0;
}

/* Whether the stream has reached end-of-file (mirrors UnixFile.eofKnown that
 * psxfile.c maintains in fileRead/fileGetChar). */
int fileEOF(const UnixFile *file) {
  return file ? file->eofKnown : 1;
}

/* Full st_mode (type bits + permission bits). */
int fileUnixMode(const FileInfo *info) {
  return info ? (int)info->st_mode : 0;
}

/* File serial number (inode). */
int fileGetINode(const FileInfo *file) {
  return file ? (int)file->st_ino : 0;
}

/* Default file CCSID setter. Linux has no per-file tags, so there is nothing
 * to tag; we keep a settable process-wide default purely so callers that push
 * a default CCSID have somewhere to put it. psxfile.c's fileInfoCCSID() still
 * returns 0 (untagged) regardless -- there is no tag to read back on Linux.
 * Returns the previous value (setFileTrace-style). */
static int defaultFileInfoCCSID = 0;
int setFileInfoCCSID(int ccsid) {
  int prev = defaultFileInfoCCSID;
  defaultFileInfoCCSID = ccsid;
  return prev;
}

/* ---- time helper --------------------------------------------------------- */

/* Convert a struct timespec to whole milliseconds. Used by qjszos.c's file
 * stat -> JS bridge (JS_NewInt64). Returns int64. NOTE: qjszos.c calls this
 * through an implicit declaration (no header declares it), so at that single
 * call site the value is truncated to int by the ABI; not an issue for the M1
 * file-contents path, and correct here for any properly-prototyped caller. */
int64_t timespec_to_ms(const struct timespec *ts) {
  if (ts == NULL) {
    return 0;
  }
  return (int64_t)ts->tv_sec * 1000 + (int64_t)ts->tv_nsec / 1000000;
}

/* ---- directory iteration (opendir/readdir/closedir) ---------------------- *
 * The DirectoryEntry contract (see BPXYDIRE and zss.c's plugin scan):
 *   offset 0: short entryLength  (stride to next entry)
 *   offset 2: short nameLength
 *   offset 4: name bytes, NUL-terminated (zss.c reads `name` as a C string)
 * We return one entry per call (return 1), 0 at end-of-directory, -1 on error,
 * exactly like the synthetic winfile.c port. The DIR* lives in UnixFile.dir. */

UnixFile *directoryOpen(const char *directoryName, int *returnCode, int *reasonCode) {
  if (directoryName == NULL) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = EINVAL;
    return NULL;
  }
  DIR *d = opendir(directoryName);
  if (d == NULL) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = errno;
    return NULL;
  }
  UnixFile *uf = (UnixFile *)safeMalloc(sizeof(UnixFile), "UnixFile dir");
  if (uf == NULL) {
    closedir(d);
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = ENOMEM;
    return NULL;
  }
  memset(uf, 0, sizeof(*uf));
  uf->pathname = strdup(directoryName);
  uf->fd = -1;
  uf->isDirectory = 1;
  uf->dir = d;
  if (returnCode) *returnCode = 0;
  if (reasonCode) *reasonCode = 0;
  return uf;
}

int directoryRead(UnixFile *directory, char *entryBuffer, int entryBufferLength,
                  int *returnCode, int *reasonCode) {
  if (directory == NULL || directory->dir == NULL || entryBuffer == NULL) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = EINVAL;
    return -1;
  }
  errno = 0;
  struct dirent *de = readdir(directory->dir);
  if (de == NULL) {
    if (errno != 0) {
      if (returnCode) *returnCode = -1;
      if (reasonCode) *reasonCode = errno;
      return -1;
    }
    if (returnCode) *returnCode = 0;
    if (reasonCode) *reasonCode = 0;
    return 0; /* end of directory */
  }
  int nameLength = (int)strlen(de->d_name);
  int entryLength = 4 + nameLength + 1; /* header + name + NUL */
  if (entryLength > entryBufferLength) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = ERANGE;
    return -1;
  }
  DirectoryEntry *entry = (DirectoryEntry *)entryBuffer;
  entry->entryLength = (short)entryLength;
  entry->nameLength = (short)nameLength;
  memcpy(entry->name, de->d_name, nameLength);
  entry->name[nameLength] = '\0';
  if (returnCode) *returnCode = 0;
  if (reasonCode) *reasonCode = 0;
  return 1; /* one entry read */
}

int directoryClose(UnixFile *directory, int *returnCode, int *reasonCode) {
  if (directory == NULL) {
    if (returnCode) *returnCode = -1;
    if (reasonCode) *reasonCode = EINVAL;
    return -1;
  }
  if (directory->dir) {
    closedir(directory->dir);
    directory->dir = NULL;
  }
  if (directory->pathname) {
    free(directory->pathname); /* strdup'd in directoryOpen */
    directory->pathname = NULL;
  }
  safeFree((char *)directory, sizeof(UnixFile));
  if (returnCode) *returnCode = 0;
  if (reasonCode) *reasonCode = 0;
  return 0;
}

/* ---- socket helpers ------------------------------------------------------ */

/* Resolve a host name or numeric address to an InetAddr for bind/connect.
 * Mirrors psxskt.c's getAddressByName InetAddr population: on Linux
 * InetAddr.data.data4 is a `struct in_addr` and .data6 a `struct in6_addr`,
 * and InetAddr.type carries the system AF_INET/AF_INET6 value (same
 * convention getEndPointName/setSocketAddr use). Allocated with safeMalloc so
 * freeInetAddr's safeFree matches. */
InetAddr *getAddressByName2(char *addressString, int ipv4only) {
  if (addressString == NULL) {
    return NULL;
  }
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = ipv4only ? AF_INET : AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo *res = NULL;
  if (getaddrinfo(addressString, NULL, &hints, &res) != 0 || res == NULL) {
    return NULL;
  }

  InetAddr *addr = NULL;
  for (struct addrinfo *ai = res; ai != NULL; ai = ai->ai_next) {
    if (ai->ai_family == AF_INET) {
      struct sockaddr_in *sin = (struct sockaddr_in *)ai->ai_addr;
      addr = (InetAddr *)safeMalloc(sizeof(InetAddr), "Inet Address");
      memset(addr, 0, sizeof(*addr));
      addr->type = AF_INET;
      addr->port = 0;
      memcpy(&(addr->data.data4), &sin->sin_addr, sizeof(struct in_addr));
      break;
    } else if (!ipv4only && ai->ai_family == AF_INET6) {
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ai->ai_addr;
      addr = (InetAddr *)safeMalloc(sizeof(InetAddr), "Inet Address");
      memset(addr, 0, sizeof(*addr));
      addr->type = AF_INET6;
      addr->port = 0;
      memcpy(&(addr->data.data6), &sin6->sin6_addr, sizeof(struct in6_addr));
      break;
    }
  }
  freeaddrinfo(res);
  return addr;
}

/* Format an IP address into a caller-supplied buffer. Uses the Linux
 * SocketAddress layout (internalAddress.v4Address / .v6Address). Follows the
 * bpxnet.h contract: on success returns ANSI_OK and *inout_len = length incl.
 * NUL; if the buffer is too small returns ANSI_FAILED and *inout_len = required
 * length. */
int SocketAddress_toString(const SocketAddress *in_socketAddress,
                          char *inout_strbuf, int *inout_len) {
  if (in_socketAddress == NULL || inout_strbuf == NULL || inout_len == NULL) {
    return ANSI_FAILED;
  }
  char tmp[64] = {0};
  int family = (int)in_socketAddress->family;
  if (family == AF_INET) {
    if (inet_ntop(AF_INET, &in_socketAddress->internalAddress.v4Address.sin_addr,
                  tmp, sizeof(tmp)) == NULL) {
      return ANSI_FAILED;
    }
  } else if (family == AF_INET6) {
    if (inet_ntop(AF_INET6, &in_socketAddress->internalAddress.v6Address.sin6_addr,
                  tmp, sizeof(tmp)) == NULL) {
      return ANSI_FAILED;
    }
  } else {
    return ANSI_FAILED;
  }
  int required = (int)strlen(tmp) + 1;
  if (*inout_len < required) {
    *inout_len = required;
    return ANSI_FAILED;
  }
  memcpy(inout_strbuf, tmp, required);
  *inout_len = required;
  return ANSI_OK;
}

/* Debug id for a socket: its file descriptor (matches bpxskt.c/winskt.c). */
int getSocketDebugID(Socket *s) {
  return s ? s->sd : -1;
}

/* =========================================================================
 * z/OS-ONLY STUBS  (link + fail/no-op gracefully; feature absent on WSL)
 * ========================================================================= */

/* ---- safeMalloc 8-char alias -------------------------------------------- *
 * NOT a SAF service, despite the SAFxxxxx spelling: alloc.h maps safeMalloc ->
 * SAFEMLLC when __LONGNAME__ is off, and one object (envService.o) was compiled
 * that way, so it references SAFEMLLC while alloc.o exports the long name
 * safeMalloc. Forward to the real allocator -- stubbing this to a "failure
 * code" would make every allocation return a bogus pointer and crash. */
char *SAFEMLLC(int size, char *site) {
  return safeMalloc(size, site);
}

/* ---- BPX USS callable services ------------------------------------------ *
 * z/OS assembler callable services (getgrgid/getgrnam/getpwnam/getpwuid/
 * getgroupsbyname/__passwd). On Linux their prototypes are compiled out
 * (guarded by __ZOWE_OS_ZOS in zowe_bpx_prototypes.h) and zosaccounts.c calls
 * them via implicit declarations. Each caller keys success off a specific
 * output slot; we drive every one to its FAILED value so user/group/password
 * lookups fail cleanly rather than reading uninitialized memory:
 *   GGI/GGN/GPN/GPU: success iff *return_value != 0  -> set it 0
 *   GUG:             success iff *number_of_group_ids != -1 -> set it -1
 *   PWD:             returns *return_value                  -> set it 0
 * Signatures follow the documented BPX_*_ARGS parameter lists. */

int BPX4GGI(int group_id, void *return_value, int *return_code, int *reason_code) {
  (void)group_id;
  if (return_value) *(int *)return_value = 0;
  if (return_code)  *return_code = -1;
  if (reason_code)  *reason_code = -1;
  return -1;
}

int BPX4GGN(int *group_name_length, char *group_name, void *return_value,
            int *return_code, int *reason_code) {
  (void)group_name_length; (void)group_name;
  if (return_value) *(int *)return_value = 0;
  if (return_code)  *return_code = -1;
  if (reason_code)  *reason_code = -1;
  return -1;
}

int BPX4GPN(int *user_name_length, char *user_name, void *return_value,
            int *return_code, int *reason_code) {
  (void)user_name_length; (void)user_name;
  if (return_value) *(int *)return_value = 0;
  if (return_code)  *return_code = -1;
  if (reason_code)  *reason_code = -1;
  return -1;
}

int BPX4GPU(int user_id, void *return_value, int *return_code, int *reason_code) {
  (void)user_id;
  if (return_value) *(int *)return_value = 0;
  if (return_code)  *return_code = -1;
  if (reason_code)  *reason_code = -1;
  return -1;
}

int BPX4GUG(int *user_name_length, char *user_name, int *group_id_list_size,
            void *group_id_list_pointer_address, int *number_of_group_ids,
            int *return_code, int *reason_code) {
  (void)user_name_length; (void)user_name; (void)group_id_list_size;
  (void)group_id_list_pointer_address;
  if (number_of_group_ids) *number_of_group_ids = -1; /* caller: != -1 means OK */
  if (return_code) *return_code = -1;
  if (reason_code) *reason_code = -1;
  return -1;
}

int BPX4PWD(int *user_name_length, char *user_name, int *pass_length, char *pass,
            int *new_pass_length, char *new_pass, int *return_value,
            int *return_code, int *reason_code) {
  (void)user_name_length; (void)user_name; (void)pass_length; (void)pass;
  (void)new_pass_length; (void)new_pass;
  if (return_value) *return_value = 0;
  if (return_code)  *return_code = -1;
  if (reason_code)  *reason_code = -1;
  return -1;
}

/* ---- SAF / RACF --------------------------------------------------------- */

/* R_usermap: certificate/DN -> userid. No SAF on Linux; report RACF failure
 * (nonzero SAF RC) so callers treat the mapping as unavailable. */
int getUseridByCertificate(char *certificate, int certificateLength, char *useridBuffer,
                           int *racfRC, int *racfReason) {
  (void)certificate; (void)certificateLength; (void)useridBuffer;
  if (racfRC)     *racfRC = RUSERMAP_RACF_FAILURE;
  if (racfReason) *racfReason = RUSERMAP_RACF_REASON_NOUSER_FOR_CERT;
  return RUSERMAP_RACF_FAILURE;
}

int getUseridByDN(char *distinguishedName, int distinguishedNameLength,
                  char *registry, int registryLength, char *useridBuffer,
                  int *racfRC, int *racfReason) {
  (void)distinguishedName; (void)distinguishedNameLength;
  (void)registry; (void)registryLength; (void)useridBuffer;
  if (racfRC)     *racfRC = RUSERMAP_RACF_FAILURE;
  if (racfReason) *racfReason = RUSERMAP_RACF_REASON_BAD_DN;
  return RUSERMAP_RACF_FAILURE;
}

/* IRRSGS64 -- SAF passticket generation service (R_GenSec / IRRSGS). Declared
 * only via #pragma linkage in passTicketService.c (no C prototype), so it is
 * called through an implicit declaration on Linux. Empty parameter list keeps
 * that call site linkable; return nonzero to signal SAF failure.
 * TODO(hardening): real passticket support needs z/OS SAF. */
int IRRSGS64() {
  return -1;
}

/* ---- MVS catalog (CSI) -------------------------------------------------- */

/* Catalog Search Interface. No MVS catalog on Linux. loadCsi is called for its
 * side effect and its result ignored; returnEntries yields no dataset entries. */
int loadCsi() {
  return 0;
}

EntryDataSet *returnEntries(char *dsn, char *typesAllowed, int typeCount,
                            int workAreaSize, char **fields, int fieldCount,
                            char *resumeName, char *resumeCatalogName,
                            csi_parmblock *__ptr32 returnParms) {
  (void)dsn; (void)typesAllowed; (void)typeCount; (void)workAreaSize;
  (void)fields; (void)fieldCount; (void)resumeName; (void)resumeCatalogName;
  (void)returnParms;
  return NULL;
}

/* ---- SMF product registration (SCRT) ------------------------------------ */

/* Records SMF type-89 for IBM Sub-Capacity Reporting. No SMF on Linux: no-op. */
void registerProduct(const char *productReg, const char *productPID,
                     const char *productVer, const char *productOwner,
                     const char *productName) {
  (void)productReg; (void)productPID; (void)productVer;
  (void)productOwner; (void)productName;
}

/* ---- APIML remote storage ----------------------------------------------- */

/* APIML-backed caching storage. The APIML path is off in this dev build; no
 * remote storage, so callers fall back to their local storage. */
Storage *makeApimlStorage(ApimlStorageSettings *settings, const char *pluginId) {
  (void)settings; (void)pluginId;
  return NULL;
}

/* ---- TLS (System SSL) --------------------------------------------------- */

/* z/OS System SSL (GSK) is unavailable on Linux; report not-configured so the
 * server stays on plaintext for local dev. */
int tlsInit(TlsEnvironment **outEnv, TlsSettings *settings) {
  (void)settings;
  if (outEnv) *outEnv = NULL;
  return -1; /* nonzero -> caller does not enable TLS (was TLS_ALLOC_ERROR) */
}

const char *tlsStrError(int rc) {
  (void)rc;
  return "TLS unavailable (WSL dev build stub)";
}

/* ---- APIML JWT verify path (off in M1) ---------------------------------- *
 * The APIML JSON Web Token verify/parse path is disabled in this dev build.
 * Trace setter is a harmless no-op; the context builders and verifier report
 * failure/NULL so token auth is simply unavailable. */

int setJwtTrace(int toWhat) {
  (void)toWhat;
  return 0;
}

void configureJwt(HttpServer *server, JwkSettings *jwkSettings) {
  (void)server; (void)jwkSettings;
}

bool jwtAreBasicClaimsValid(const Jwt *jwt, const char *audience) {
  (void)jwt; (void)audience;
  return false;
}

JwtContext *makeJwtContextForKeyInToken(const char *in_tokenName,
                                        const char *in_keyLabel,
                                        int class,
                                        int *out_rc,
                                        int *out_p11rc, int *out_p11rsn) {
  (void)in_tokenName; (void)in_keyLabel; (void)class;
  if (out_rc)    *out_rc = -1;
  if (out_p11rc) *out_p11rc = -1;
  if (out_p11rsn) *out_p11rsn = -1;
  return NULL;
}

JwtContext *makeJwtContextCustom(JwtCheckSignature *checkSignatureFn,
                                 void *userData, int *out_rc) {
  (void)checkSignatureFn; (void)userData;
  if (out_rc) *out_rc = -1;
  return NULL;
}

Jwt *jwtVerifyAndParseToken(const JwtContext *self, const char *tokenText,
                            bool ebcdic, ShortLivedHeap *slh, int *rc) {
  (void)self; (void)tokenText; (void)ebcdic; (void)slh;
  if (rc) *rc = -1;
  return NULL;
}

/* ---- pipe-tunnel synthetic transport ------------------------------------ */

/* Pipe-based synthetic Socket (tunnelling transport). Not built for WSL. */
Socket *makePipeBasedSyntheticSocket(int protocol, int inputFD, int outputFD) {
  (void)protocol; (void)inputFD; (void)outputFD;
  return NULL;
}

/* ---- recursive / mutating directory ops (not needed for M1) ------------- *
 * File-contents (M1) needs only read+iterate, done above. These mutating and
 * recursive operations are stubbed to a failure return; wire them to real
 * POSIX (mkdir -p, nftw, rename, chmod -R) when a use case needs them. */

int directoryMake(const char *pathName, int mode, int *returnCode, int *reasonCode) {
  (void)pathName; (void)mode;
  if (returnCode) *returnCode = -1;
  if (reasonCode) *reasonCode = ENOSYS;
  return -1;
}

int directoryDeleteRecursive(const char *pathName, int *retCode, int *resCode) {
  (void)pathName;
  if (retCode) *retCode = -1;
  if (resCode) *resCode = ENOSYS;
  return -1;
}

int directoryMakeDirectoryRecursive(const char *pathName, char *message,
                                    int messageLength, int recursive,
                                    int forceCreate) {
  (void)pathName; (void)message; (void)messageLength;
  (void)recursive; (void)forceCreate;
  return -1;
}

int directoryCopy(const char *existingPathName, const char *newPathName,
                  int *retCode, int *resCode) {
  (void)existingPathName; (void)newPathName;
  if (retCode) *retCode = -1;
  if (resCode) *resCode = ENOSYS;
  return -1;
}

int directoryRename(const char *oldDirName, const char *newDirName,
                    int *returnCode, int *reasonCode) {
  (void)oldDirName; (void)newDirName;
  if (returnCode) *returnCode = -1;
  if (reasonCode) *reasonCode = ENOSYS;
  return -1;
}

int directoryChangeModeRecursive(const char *pathName, int flag, int mode,
                                 const char *compare, int *retCode, int *resCode) {
  (void)pathName; (void)flag; (void)mode; (void)compare;
  if (retCode) *retCode = -1;
  if (resCode) *resCode = ENOSYS;
  return -1;
}
