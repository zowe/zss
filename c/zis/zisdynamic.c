#include <metal/metal.h>
#include <metal/stdbool.h>
#include <metal/stddef.h>
#include <metal/stdint.h>
#include <metal/stdlib.h>
#include <metal/string.h>


#include "zvt.h"
#include "zowetypes.h"
#include "alloc.h"
#include "utils.h"
#include "qsam.h"
#include "metalio.h"
#include "collections.h"
#include "logging.h"
#include "zos.h"
#include "le.h"
#include "unixfile.h"
#include "recovery.h"
#include "zos.h"
#include "shrmem64.h"
#include "cmutils.h"
#include "pause-element.h"
#include "scheduling.h"
#include "xlate.h"

#include "cmutils.h"
#include "crossmemory.h"
#include "shrmem64.h"
#include "cpool64.h"
#include "zis/zisstubs.h"
#include "zis/plugin.h"
#include "zis/server.h"
#include "zis/service.h"
#include "zis/client.h"
#include "zis/zisstubs.h"

typedef struct DynamicPluginData_tag {
  uint64_t initTime;
  char     unused[56];

} DynamicPluginData;

static int installDynamicLinkageVector(ZISContext *context, void *dynamicLinkageVector){
  ZISServerAnchor *serverAnchor = context->zisAnchor;
  
  if (serverAnchor->flags & ZIS_SERVER_ANCHOR_FLAG_DYNLINK){
    CrossMemoryServerGlobalArea *cmsGA = context->cmsGA;
    cmsGA->dynamicLinkageVector = dynamicLinkageVector;
    return 0;
  } else{
    return 12;
  }
}



static int initZISDynamic(struct ZISContext_tag *context,
			  ZISPlugin *plugin,
			  ZISPluginAnchor *anchor) {
  DynamicPluginData *pluginData = (DynamicPluginData *)&anchor->pluginData;
  
  __asm(" STCK %0 " : "=m"(pluginData->initTime) :);
  
  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO, "Initializing ZIS Dynamic Base plugin (Mk1) anchor=0x%p pluginData=0x%p\n",
	 anchor,pluginData);

  void **stubVector = 
    (void*)cmAlloc(sizeof(void*)*MAX_ZIS_STUBS, ZVTE_SUBPOOL, ZVTE_KEY);

  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO, "Built Stub Vector at 0x%p, but before ZVTE install\n",
	  stubVector);

  ZISServerAnchor *serverAnchor = context->zisAnchor;
  CAA *caa = (CAA*)getCAA();

  int installStatus = 0;
  if (stubVector){
    int wasProblem = supervisorMode(TRUE);

    zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO, "ZIS Dynamic (Mk2) in SUPERVISOR ZISServerAnchor at 0x%p caa=0x%p\n",
	    serverAnchor,caa);

    int oldKey = setKey(0);

    if (caa){
      RLETask *rleTask = caa->rleTask;
      RLEAnchor *rleAnchor = rleTask->anchor;
      rleAnchor->metalDynamicLinkageVector = stubVector;
    }

    /* special initialization for slot 0 and all unused slots */
    for (int s=0; s<MAX_ZIS_STUBS; s++){
      stubVector[s] = (void*)dynamicZISUndefinedStub;
    }

    stubVector[ZIS_STUB_DYNZISVR] = (void*)dynamicZISVersion;
    stubVector[ZIS_STUB_SHR64TKN] = (void*)shrmem64GetAddressSpaceToken;
    stubVector[ZIS_STUB_SHR64ALC] = (void*)shrmem64Alloc;
    stubVector[ZIS_STUB_SHR64AL2] = (void*)shrmem64Alloc2;
    stubVector[ZIS_STUB_SHR64CAL] = (void*)shrmem64CommonAlloc;
    stubVector[ZIS_STUB_SHR64CA2] = (void*)shrmem64CommonAlloc2;
    stubVector[ZIS_STUB_SHR64REL] = (void*)shrmem64Release;
    stubVector[ZIS_STUB_SHR64REA] = (void*)shrmem64ReleaseAll;
    stubVector[ZIS_STUB_SHR64GAC] = (void*)shrmem64GetAccess;
    stubVector[ZIS_STUB_SHR64GA2] = (void*)shrmem64GetAccess2;
    stubVector[ZIS_STUB_SHR64RAC] = (void*)shrmem64RemoveAccess;
    stubVector[ZIS_STUB_SHR64RA2] = (void*)shrmem64RemoveAccess2;
    stubVector[ZIS_STUB_CPL64CRE] = (void*)iarcp64Create;
    stubVector[ZIS_STUB_CPL64GET] = (void*)iarcp64Get;
    stubVector[ZIS_STUB_CPL64FRE] = (void*)iarcp64Free;
    stubVector[ZIS_STUB_CPL64DEL] = (void*)iarcp64Delete;
    stubVector[ZIS_STUB_E2A     ] = (void*)e2a;
    stubVector[ZIS_STUB_A2E     ] = (void*)a2e;
    stubVector[ZIS_STUB_PEALLOC ] = (void*)peAlloc;
    stubVector[ZIS_STUB_PEDALLOC] = (void*)peDealloc;
    stubVector[ZIS_STUB_PEPAUSE ] = (void*)pePause;
    stubVector[ZIS_STUB_PERELEAS] = (void*)peRelease;
    stubVector[ZIS_STUB_PERTVNFO] = (void*)peRetrieveInfo;
    stubVector[ZIS_STUB_PERTVNF2] = (void*)peRetrieveInfo2;
    stubVector[ZIS_STUB_PETEST  ] = (void*)peTest;
    stubVector[ZIS_STUB_PETRNSFR] = (void*)peTransfer;
    stubVector[ZIS_STUB_SAFEMLLC] = (void*)safeMalloc;
    stubVector[ZIS_STUB_SAFEMLC2] = (void*)safeMalloc2;
    stubVector[ZIS_STUB_SAFMLC31] = (void*)safeMalloc31;
    stubVector[ZIS_STUB_SAFMLC38] = (void*)safeMalloc31Key8;
    stubVector[ZIS_STUB_SAFFRE31] = (void*)safeFree31;
    stubVector[ZIS_STUB_SAFFRE38] = (void*)safeFree31Key8;
    stubVector[ZIS_STUB_SAFFRE64] = (void*)safeFree64;
    stubVector[ZIS_STUB_SFFRE64T] = (void*)safeFree64ByToken;
    stubVector[ZIS_STUB_SAFMLC64] = (void*)safeMalloc64;
    stubVector[ZIS_STUB_SFMLC64T] = (void*)safeMalloc64ByToken;
    stubVector[ZIS_STUB_SAFRE642] = (void*)safeFree64v2;
    stubVector[ZIS_STUB_SAMLC642] = (void*)safeMalloc64v2;
    stubVector[ZIS_STUB_SAFRE643] = (void*)safeFree64v3;
    stubVector[ZIS_STUB_SAMLC643] = (void*)safeMalloc64v3;
    stubVector[ZIS_STUB_SAFEFREE] = (void*)safeFree;
    stubVector[ZIS_STUB_STRCPSAF] = (void*)strcopy_safe;
    stubVector[ZIS_STUB_INDEXOF ] = (void*)indexOf;
    stubVector[ZIS_STUB_IDXSTR  ] = (void*)indexOfString;
    stubVector[ZIS_STUB_LIDXSTR ] = (void*)lastIndexOfString;
    stubVector[ZIS_STUB_IDXSTRNS] = (void*)indexOfStringInsensitive;
    stubVector[ZIS_STUB_PSINTINT] = (void*)parseInitialInt;
    stubVector[ZIS_STUB_NULLTERM] = (void*)nullTerminate;
    stubVector[ZIS_STUB_TKGTDCML] = (void*)tknGetDecimal;
    stubVector[ZIS_STUB_TKGTQOTD] = (void*)tknGetQuoted;
    stubVector[ZIS_STUB_TKGTANUM] = (void*)tknGetAlphanumeric;
    stubVector[ZIS_STUB_TKGTSTND] = (void*)tknGetStandard;
    stubVector[ZIS_STUB_TKGTNWSP] = (void*)tknGetNonWhitespace;
    stubVector[ZIS_STUB_TKGTTERM] = (void*)tknGetTerminating;
    stubVector[ZIS_STUB_TKTXEQLS] = (void*)tknTextEquals;
    stubVector[ZIS_STUB_FREETKN ] = (void*)freeToken;
    stubVector[ZIS_STUB_TKNLNGTH] = (void*)tknLength;
    stubVector[ZIS_STUB_DMPBUFFR] = (void*)dumpbuffer;
    stubVector[ZIS_STUB_DMPBUFFA] = (void*)dumpbufferA;
    stubVector[ZIS_STUB_SMPHXFIL] = (void*)simpleHexFill;
    stubVector[ZIS_STUB_SMPHXPRT] = (void*)simpleHexPrint;
    stubVector[ZIS_STUB_SMPHXPRL] = (void*)simpleHexPrintLower;
    stubVector[ZIS_STUB_DMPBFFR2] = (void*)dumpbuffer2;
    stubVector[ZIS_STUB_DMPBFFRS] = (void*)dumpBufferToStream;
    stubVector[ZIS_STUB_CMPIGNCS] = (void*)compareIgnoringCase;
    stubVector[ZIS_STUB_STRUPCAS] = (void*)strupcase;
    stubVector[ZIS_STUB_MAKESLH ] = (void*)makeShortLivedHeap;
    stubVector[ZIS_STUB_MAKSLH64] = (void*)makeShortLivedHeap64;
    stubVector[ZIS_STUB_SLHALLOC] = (void*)SLHAlloc;
    stubVector[ZIS_STUB_NYMALLOC] = (void*)noisyMalloc;
    stubVector[ZIS_STUB_DECODB32] = (void*)base32Decode;
    stubVector[ZIS_STUB_ENCODB32] = (void*)base32Encode;
    stubVector[ZIS_STUB_DECODB64] = (void*)decodeBase64;
    stubVector[ZIS_STUB_ENCODB64] = (void*)encodeBase64;
    stubVector[ZIS_STUB_CLNURLPV] = (void*)cleanURLParamValue;
    stubVector[ZIS_STUB_PCTENCOD] = (void*)percentEncode;
    stubVector[ZIS_STUB_DSTUNASC] = (void*)destructivelyUnasciify;
    stubVector[ZIS_STUB_MAKESLST] = (void*)makeStringList;
    stubVector[ZIS_STUB_SLSTLEN ] = (void*)stringListLength;
    stubVector[ZIS_STUB_SLSTPRNT] = (void*)stringListPrint;
    stubVector[ZIS_STUB_SLSTCTNS] = (void*)stringListContains;
    stubVector[ZIS_STUB_SLSTLAST] = (void*)stringListLast;
    stubVector[ZIS_STUB_ADSLSTUQ] = (void*)addToStringListUnique;
    stubVector[ZIS_STUB_ADSLST  ] = (void*)addToStringList;
    stubVector[ZIS_STUB_SLSTELT1] = (void*)firstStringListElt;
    stubVector[ZIS_STUB_STRCNCAT] = (void*)stringConcatenate;
    stubVector[ZIS_STUB_MKBFCHST] = (void*)makeBufferCharStream;
    stubVector[ZIS_STUB_CSTRPOSN] = (void*)charStreamPosition;
    stubVector[ZIS_STUB_CSTRGET ] = (void*)charStreamGet;
    stubVector[ZIS_STUB_CSTREOF ] = (void*)charStreamEOF;
    stubVector[ZIS_STUB_CSTRCLOS] = (void*)charStreamClose;
    stubVector[ZIS_STUB_CSTRFREE] = (void*)charStreamFree;
    stubVector[ZIS_STUB_MKLOGCTX] = (void*)makeLoggingContext;
    stubVector[ZIS_STUB_MKLLGCTX] = (void*)makeLocalLoggingContext;
    stubVector[ZIS_STUB_RMLOGCTX] = (void*)removeLoggingContext;
    stubVector[ZIS_STUB_RMLLGCTX] = (void*)removeLocalLoggingContext;
    stubVector[ZIS_STUB_GTLOGCTX] = (void*)getLoggingContext;
    stubVector[ZIS_STUB_STLOGCTX] = (void*)setLoggingContext;
    stubVector[ZIS_STUB_LGCFGDST] = (void*)logConfigureDestination;
    stubVector[ZIS_STUB_LGCFGDS2] = (void*)logConfigureDestination2;
    stubVector[ZIS_STUB_LGCFGSTD] = (void*)logConfigureStandardDestinations;
    stubVector[ZIS_STUB_LGCFGCMP] = (void*)logConfigureComponent;
    stubVector[ZIS_STUB_LGSHTRCE] = (void*)logShouldTraceInternal;
    stubVector[ZIS_STUB_LGPRSOUT] = (void*)printStdout;
    stubVector[ZIS_STUB_LGPRSERR] = (void*)printStderr;
    stubVector[ZIS_STUB_ZOWELOG ] = (void*)zowelog;
    stubVector[ZIS_STUB_GTNMTKVL] = (void*)getNameTokenValue;
    stubVector[ZIS_STUB_CRNMTKPR] = (void*)createNameTokenPair;
    stubVector[ZIS_STUB_GTSYSOUT] = (void*)getSYSOUTStruct;
    stubVector[ZIS_STUB_WTOPRNF ] = (void*)wtoPrintf;
    stubVector[ZIS_STUB_AWTOPRNF] = (void*)authWTOPrintf;
    stubVector[ZIS_STUB_SENDWTO ] = (void*)sendWTO;
    stubVector[ZIS_STUB_QSAMPRNF] = (void*)qsamPrintf;
    stubVector[ZIS_STUB_HASVRLEN] = (void*)hasVaryingRecordLength;
    stubVector[ZIS_STUB_MKQSMBUF] = (void*)makeQSAMBuffer;
    stubVector[ZIS_STUB_FRQSMBUF] = (void*)freeQSAMBuffer;
    stubVector[ZIS_STUB_PUTLNEV ] = (void*)putlineV;
    stubVector[ZIS_STUB_GETLNEV ] = (void*)getlineV;
    stubVector[ZIS_STUB_GTEODBUF] = (void*)getEODADBuffer;
    stubVector[ZIS_STUB_FREODBUF] = (void*)freeEODADBuffer;
    stubVector[ZIS_STUB_GTSAMLN ] = (void*)getSAMLength;
    stubVector[ZIS_STUB_GTSAMBS ] = (void*)getSAMBlksize;
    stubVector[ZIS_STUB_GTSAMLR ] = (void*)getSAMLrecl;
    stubVector[ZIS_STUB_GTSAMCC ] = (void*)getSAMCC;
    stubVector[ZIS_STUB_GTSAMRF ] = (void*)getSAMRecfm;
    stubVector[ZIS_STUB_BPAMFIND] = (void*)bpamFind;
    stubVector[ZIS_STUB_BPAMREAD] = (void*)bpamRead;
    stubVector[ZIS_STUB_BPAMRD2 ] = (void*)bpamRead2;
    stubVector[ZIS_STUB_OPENEXCP] = (void*)openEXCP;
    stubVector[ZIS_STUB_CLOSEXCP] = (void*)closeEXCP;
    stubVector[ZIS_STUB_LEGETCAA] = (void*)getCAA;
    stubVector[ZIS_STUB_LESHWRTL] = (void*)showRTL;
    stubVector[ZIS_STUB_LEMKRLEA] = (void*)makeRLEAnchor;
    stubVector[ZIS_STUB_LEDLRLEA] = (void*)deleteRLEAnchor;
    stubVector[ZIS_STUB_LEMKRLET] = (void*)makeRLETask;
    stubVector[ZIS_STUB_LEDLRLET] = (void*)deleteRLETask;
    stubVector[ZIS_STUB_LEINRLEE] = (void*)initRLEEnvironment;
    stubVector[ZIS_STUB_LETMRLEE] = (void*)termRLEEnvironment;
    stubVector[ZIS_STUB_LEMKFCAA] = (void*)makeFakeCAA;
    stubVector[ZIS_STUB_LEARTCAA] = (void*)abortIfUnsupportedCAA;
    stubVector[ZIS_STUB_SCHZOSWT] = (void*)zosWait;
    stubVector[ZIS_STUB_SCHZOSWL] = (void*)zosWaitList;
    stubVector[ZIS_STUB_SCHZOSPT] = (void*)zosPost;
    stubVector[ZIS_STUB_SCHSRLET] = (void*)startRLETask;
    stubVector[ZIS_STUB_CMCPWDK ] = (void*)cmCopyWithDestinationKey;
    stubVector[ZIS_STUB_CMCPWSK ] = (void*)cmCopyWithSourceKey;
    stubVector[ZIS_STUB_CMCPTSSK] = (void*)cmCopyToSecondaryWithCallerKey;
    stubVector[ZIS_STUB_CMCPFSSK] = (void*)cmCopyFromSecondaryWithCallerKey;
    stubVector[ZIS_STUB_CMCPTPSK] = (void*)cmCopyToPrimaryWithCallerKey;
    stubVector[ZIS_STUB_CMCPFPSK] = (void*)cmCopyFromPrimaryWithCallerKey;
    stubVector[ZIS_STUB_CMCPTHSK] = (void*)cmCopyToHomeWithCallerKey;
    stubVector[ZIS_STUB_CMCPFHSK] = (void*)cmCopyFromHomeWithCallerKey;
    stubVector[ZIS_STUB_CMCPYSKA] = (void*)cmCopyWithSourceKeyAndALET;
    stubVector[ZIS_STUB_CMGAACEE] = (void*)cmGetCallerAddressSpaceACEE;
    stubVector[ZIS_STUB_CMGTACEE] = (void*)cmGetCallerTaskACEE;
    stubVector[ZIS_STUB_CMALLOC ] = (void*)cmAlloc;
    stubVector[ZIS_STUB_CMFREE  ] = (void*)cmFree;
    stubVector[ZIS_STUB_CMALLOC2] = (void*)cmAlloc2;
    stubVector[ZIS_STUB_CMFREE2 ] = (void*)cmFree2;
    stubVector[ZIS_STUB_CMUMKMAP] = (void*)makeCrossMemoryMap;
    stubVector[ZIS_STUB_CMURMMAP] = (void*)removeCrossMemoryMap;
    stubVector[ZIS_STUB_CMUMAPP ] = (void*)crossMemoryMapPut;
    stubVector[ZIS_STUB_CMUMAPGH] = (void*)crossMemoryMapGetHandle;
    stubVector[ZIS_STUB_CMUMAPGT] = (void*)crossMemoryMapGet;
    stubVector[ZIS_STUB_CMUMAPIT] = (void*)crossMemoryMapIterate;
    stubVector[ZIS_STUB_RCVRESRR] = (void*)recoveryEstablishRouter;
    stubVector[ZIS_STUB_RCVRRSRR] = (void*)recoveryRemoveRouter;
    stubVector[ZIS_STUB_RCVRTRES] = (void*)recoveryIsRouterEstablished;
    stubVector[ZIS_STUB_RCVRPSFR] = (void*)recoveryPush;
    stubVector[ZIS_STUB_RCVRPPFR] = (void*)recoveryPop;
    stubVector[ZIS_STUB_RCVRSDTL] = (void*)recoverySetDumpTitle;
    stubVector[ZIS_STUB_RCVRSFLV] = (void*)recoverySetFlagValue;
    stubVector[ZIS_STUB_RCVRECST] = (void*)recoveryEnableCurrentState;
    stubVector[ZIS_STUB_RCVRDCST] = (void*)recoveryDisableCurrentState;
    stubVector[ZIS_STUB_RCVRURSI] = (void*)recoveryUpdateRouterServiceInfo;
    stubVector[ZIS_STUB_RCVRUSSI] = (void*)recoveryUpdateStateServiceInfo;
    stubVector[ZIS_STUB_RCVRGACD] = (void*)recoveryGetABENDCode;
    stubVector[ZIS_STUB_ZISCRSVC] = (void*)zisCreateService;
    stubVector[ZIS_STUB_ZISCSWSV] = (void*)zisCreateSpaceSwitchService;
    stubVector[ZIS_STUB_ZISCCPSV] = (void*)zisCreateCurrentPrimaryService;
    stubVector[ZIS_STUB_ZISCSVCA] = (void*)zisCreateServiceAnchor;
    stubVector[ZIS_STUB_ZISRSVCA] = (void*)zisRemoveServiceAnchor;
    stubVector[ZIS_STUB_ZISUSVCA] = (void*)zisUpdateServiceAnchor;
    stubVector[ZIS_STUB_ZISUSAUT] = (void*)zisServiceUseSpecificAuth;
    stubVector[ZIS_STUB_FILEOPEN] = (void*)fileOpen;
    stubVector[ZIS_STUB_FILEREAD] = (void*)fileRead;
    stubVector[ZIS_STUB_FILWRITE] = (void*)fileWrite;
    stubVector[ZIS_STUB_FILGTCHR] = (void*)fileGetChar;
    stubVector[ZIS_STUB_FILECOPY] = (void*)fileCopy;
    stubVector[ZIS_STUB_FILRNAME] = (void*)fileRename;
    stubVector[ZIS_STUB_FILDLETE] = (void*)fileDelete;
    stubVector[ZIS_STUB_FILEINFO] = (void*)fileInfo;
    stubVector[ZIS_STUB_FNFOISDR] = (void*)fileInfoIsDirectory;
    stubVector[ZIS_STUB_FNFOSIZE] = (void*)fileInfoSize;
    stubVector[ZIS_STUB_FNFOCCID] = (void*)fileInfoCCSID;
    stubVector[ZIS_STUB_FILUXCRT] = (void*)fileInfoUnixCreationTime;
    stubVector[ZIS_STUB_FILISOEF] = (void*)fileEOF;
    stubVector[ZIS_STUB_FILGINOD] = (void*)fileGetINode;
    stubVector[ZIS_STUB_FILDEVID] = (void*)fileGetDeviceID;
    stubVector[ZIS_STUB_FILCLOSE] = (void*)fileClose;
    stubVector[ZIS_STUB_ZISCPLGN] = (void*)zisCreatePlugin;
    stubVector[ZIS_STUB_ZISPLGAS] = (void*)zisPluginAddService;
    stubVector[ZIS_STUB_ZISPLGCA] = (void*)zisCreatePluginAnchor;
    stubVector[ZIS_STUB_ZISPLGRM] = (void*)zisRemovePluginAnchor;
    stubVector[ZIS_STUB_GETTCB  ] = (void*)getTCB;
    stubVector[ZIS_STUB_GETSTCB ] = (void*)getSTCB;
    stubVector[ZIS_STUB_GETOTCB ] = (void*)getOTCB;
    stubVector[ZIS_STUB_GETASCB ] = (void*)getASCB;
    stubVector[ZIS_STUB_GETASSB ] = (void*)getASSB;
    stubVector[ZIS_STUB_GETASXB ] = (void*)getASXB;
    stubVector[ZIS_STUB_SETKEY  ] = (void*)setKey;
    stubVector[ZIS_STUB_CMGETGA ] = (void*)cmsGetGlobalArea;
    stubVector[ZIS_STUB_CMCMSRCS] = (void*)cmsCallService;
    stubVector[ZIS_STUB_CMCALLS2] = (void*)cmsCallService2;
    stubVector[ZIS_STUB_CMCALLS3] = (void*)cmsCallService3;
    stubVector[ZIS_STUB_CMCMSPRF] = (void*)cmsPrintf;
    stubVector[ZIS_STUB_CMCMSVPF] = (void*)vcmsPrintf;
    stubVector[ZIS_STUB_CMGETPRM] = (void*)cmsGetConfigParm;
    stubVector[ZIS_STUB_CMGETPRU] = (void*)cmsGetConfigParmUnchecked;
    stubVector[ZIS_STUB_CMGETLOG] = (void*)cmsGetPCLogLevel;
    stubVector[ZIS_STUB_CMGETSTS] = (void*)cmsGetStatus;
    stubVector[ZIS_STUB_CMMKSNAM] = (void*)cmsMakeServerName;
    stubVector[ZIS_STUB_ZISCSRVC] = (void*)zisCallService;
    stubVector[ZIS_STUB_ZISGDSNM] = (void*)zisGetDefaultServerName;
    stubVector[ZIS_STUB_ZISCOPYD] = (void*)zisCopyDataFromAddressSpace;
    stubVector[ZIS_STUB_ZISUNPWD] = (void*)zisCheckUsernameAndPassword;
    stubVector[ZIS_STUB_ZISSAUTH] = (void*)zisCheckEntity;
    stubVector[ZIS_STUB_ZISGETAC] = (void*)zisGetSAFAccessLevel;
    stubVector[ZIS_STUB_ZISCNWMS] = (void*)zisCallNWMService;
    stubVector[ZIS_STUB_ZISUPROF] = (void*)zisExtractUserProfiles;
    stubVector[ZIS_STUB_ZISGRROF] = (void*)zisExtractGenresProfiles;
    stubVector[ZIS_STUB_ZISGRAST] = (void*)zisExtractGenresAccessList;
    stubVector[ZIS_STUB_ZISDFPRF] = (void*)zisDefineProfile;
    stubVector[ZIS_STUB_ZISDLPRF] = (void*)zisDeleteProfile;
    stubVector[ZIS_STUB_ZISGAPRF] = (void*)zisGiveAccessToProfile;
    stubVector[ZIS_STUB_ZISRAPRF] = (void*)zisRevokeAccessToProfile;
    stubVector[ZIS_STUB_ZISXGRPP] = (void*)zisExtractGroupProfiles;
    stubVector[ZIS_STUB_ZISXGRPA] = (void*)zisExtractGroupAccessList;
    stubVector[ZIS_STUB_ZISADDGP] = (void*)zisAddGroup;
    stubVector[ZIS_STUB_ZISDELGP] = (void*)zisDeleteGroup;
    stubVector[ZIS_STUB_ZISADDCN] = (void*)zisConnectToGroup;
    stubVector[ZIS_STUB_ZISREMCN] = (void*)zisRemoveFromGroup;

    installStatus = installDynamicLinkageVector(context,stubVector);
    setKey(oldKey);

    if (wasProblem){
      supervisorMode(FALSE);
    }

    if (installStatus){
      zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_WARNING, "Could not install dynamic linkage stub vector, rc=%d\n",
	      installStatus);
    }
    
  }

  zowelog(NULL, LOG_COMP_STCBASE, ZOWE_LOG_INFO, "Stub Vector installed in CMS Global Area\n");

  /* jam this into our favorite zvte */

  return RC_ZIS_PLUGIN_OK;
}

static int termZISDynamic(struct ZISContext_tag *context,
			  ZISPlugin *plugin,
			  ZISPluginAnchor *anchor) {

  DynamicPluginData *pluginData = (DynamicPluginData *)&anchor->pluginData;

  pluginData->initTime = -1;

  return RC_ZIS_PLUGIN_OK;
}

static int handleZISDynamicCommands(struct ZISContext_tag *context,
				    ZISPlugin *plugin,
				    ZISPluginAnchor *anchor,
				    const CMSModifyCommand *command,
				    CMSModifyCommandStatus *status) {
  
  if (command->commandVerb == NULL) {
    return RC_ZIS_PLUGIN_OK;
  }

  if (command->argCount != 1) {
    return RC_ZIS_PLUGIN_OK;
  }

  if (!strcmp(command->commandVerb, "D") ||
      !strcmp(command->commandVerb, "DIS") ||
      !strcmp(command->commandVerb, "DISPLAY")) {

    if (!strcmp(command->args[0], "STATUS")) {

      DynamicPluginData *pluginData = (DynamicPluginData *)&anchor->pluginData;

      /* We can use zowelog but I don't want to link with a lot of unnecessary
       * object files.  */
      cmsPrintf(&context->cmServer->name,
                "Inzpect plug-in v%d - anchor = 0x%p, init TOD = %16.16llX\n",
                plugin->pluginVersion, anchor, pluginData->initTime);

      *status = CMS_MODIFY_COMMAND_STATUS_CONSUMED;
    }

  }

  return RC_ZIS_PLUGIN_OK;
}

void dynamicZISUndefinedStub(void){
  __asm(" ABEND 777,REASON=8 ":::);
}

int dynamicZISVersion(){
  return ZIS_STUBS_VERSION;
}

ZISPlugin *getPluginDescriptor() {
  ZISPluginName pluginName = {.text = "ZISDYNAMIC      "};
  ZISPluginNickname pluginNickname = {.text = "ZDYN"};

  ZISPlugin *plugin = zisCreatePlugin(pluginName, pluginNickname,
                                      initZISDynamic,  /* function */
				      termZISDynamic,
				      handleZISDynamicCommands,
                                      1,  /* version */
				      0,  /* service count */
				      ZIS_PLUGIN_FLAG_LPA);
  if (plugin == NULL) {
    return NULL;
  }

  return plugin;
}





