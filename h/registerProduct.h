

const char *MODULE_REGISTER_USAGE = "IFAUSAGE";

char productReg[COMMON_PATH_MAX];
char productPID[COMMON_PATH_MAX];
char productVer[COMMON_PATH_MAX];
char productOwner[COMMON_PATH_MAX];
char productName[COMMON_PATH_MAX];
char productFeature[COMMON_PATH_MAX];

typedef struct IFAARGS {
  int prefix;
  char id[8];
  short listlen;
  char version;
  char request;
  char prodowner[16];
  char prodname[16];
  char prodvers[8];
  char prodqual[8];
  char prodid[8];
  char domain;
  char scope;
  char rsv0001;
  char flags;
  char *__ptr32 prtoken_addr;
  char *__ptr32 begtime_addr;
  char *__ptr32 data_addr;
  char xformat;
  char rsv0002[3];
  char *__ptr32 currentdata_addr;
  char *__ptr32 enddata_addr;
  char *__ptr32 endtime_addr;
} IFAARGS_t;


