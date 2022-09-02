
/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/

#ifndef ZIS_ZISSTUBS_H_
#define ZIS_ZISSTUBS_H_

/* Bump this number up everytime a stub is added to
   a release that makes it to a public merge like master or staging
   
   NEVER RE-USE a STUB NUMBER !!!!!!

   FULL BACKWARD COMPATIBILITY MUST BE MAINTAINED
   */

#define ZIS_STUBS_VERSION 2

/*
  How does a user check for compatibility?

  Since routines are NOT removed clients only want to know that the ZIS server
  is >= some version to know that routines will be there.

  Use case 1:  A ZIS Plugin

    A ZIS Plugin should in its pluginInit callback check if version is at least a certain number.

    int dynVersion = zisdynGetStubVersion();

  Use case 2:  A non-Plugin Metal C program

    int dynVersion = zisdynGetStubVersion();
    
 */

#define MAX_ZIS_STUBS 1000

/* Various ZIS APIs, 0-49 */
#define ZIS_STUB_DYNZISUD  0  /* zisdynUndefinedStub  */
#define ZIS_STUB_ZISDYNSV  1  /* zisdynGetStubVersion mapped */
#define ZIS_STUB_ZISDYNPV  2  /* zisdynGetPluginVersion mapped */
#define ZIS_STUB_ZISGVRSN  3  /* zisGetServerVersion mapped */
#define ZIS_STUB_ZISLPADV  4  /* zisIsLPADevModeOn mapped */

/* zis/client, 50-79 */
#define ZIS_STUB_ZISCSRVC 50 /* zisCallService */
#define ZIS_STUB_ZISCUSVC 51 /* zisCallServiceUnchecked */
#define ZIS_STUB_ZISCVSVC 52 /* zisCallVersionedService */
#define ZIS_STUB_ZISGDSNM 53 /* zisGetDefaultServerName */
#define ZIS_STUB_ZISCOPYD 54 /* zisCopyDataFromAddressSpace */
#define ZIS_STUB_ZISUNPWD 55 /* zisCheckUsernameAndPassword */
#define ZIS_STUB_ZISSAUTH 56 /* zisCheckEntity */
#define ZIS_STUB_ZISGETAC 57 /* zisGetSAFAccessLevel */
#define ZIS_STUB_ZISCNWMS 58 /* zisCallNWMService */
#define ZIS_STUB_ZISUPROF 59 /* zisExtractUserProfiles */
#define ZIS_STUB_ZISGRROF 60 /* zisExtractGenresProfiles */
#define ZIS_STUB_ZISGRAST 61 /* zisExtractGenresAccessList */
#define ZIS_STUB_ZISDFPRF 62 /* zisDefineProfile */
#define ZIS_STUB_ZISDLPRF 63 /* zisDeleteProfile */
#define ZIS_STUB_ZISGAPRF 64 /* zisGiveAccessToProfile */
#define ZIS_STUB_ZISRAPRF 65 /* zisRevokeAccessToProfile */
#define ZIS_STUB_ZISXGRPP 66 /* zisExtractGroupProfiles */
#define ZIS_STUB_ZISXGRPA 67 /* zisExtractGroupAccessList */
#define ZIS_STUB_ZISADDGP 68 /* zisAddGroup */
#define ZIS_STUB_ZISDELGP 69 /* zisDeleteGroup */
#define ZIS_STUB_ZISADDCN 70 /* zisConnectToGroup */
#define ZIS_STUB_ZISREMCN 71 /* zisRemoveFromGroup */
#define ZIS_STUB_ZISGVSAA 72 /* zisGenerateOrValidateSafIdtWithAppl */
#define ZIS_STUB_ZISGVSAF 73 /* zisGenerateOrValidateSafIdt */

/* zis/parm, 80-89 */
#define ZIS_STUB_ZISMAKPS 80 /* zisMakeParmSet */
#define ZIS_STUB_ZISREMPS 81 /* zisRemoveParmSet */
#define ZIS_STUB_ZISRDLIB 82 /* zisReadParmlib */
#define ZIS_STUB_ZISRDMPR 83 /* zisReadMainParms */
#define ZIS_STUB_ZISPUTPV 84 /* zisPutParmValue */
#define ZIS_STUB_ZISGETPV 85 /* zisGetParmValue */
#define ZIS_STUB_ZISLOADP 86 /* zisLoadParmsToCMServer */
#define ZIS_STUB_ZISITERP 87 /* zisIterateParms */

/* zis/plugin, 90-99 */
#define ZIS_STUB_ZISCPLGN 90 /* zisCreatePlugin mapped */
#define ZIS_STUB_ZISDSTPL 91 /* zisDestroyPlugin mapped */
#define ZIS_STUB_ZISPLGAS 92 /* zisPluginAddService mapped */
#define ZIS_STUB_ZISPLGCA 93 /* zisCreatePluginAnchor mapped */
#define ZIS_STUB_ZISPLGRM 94 /* zisRemovePluginAnchor mapped */

/* zis/service, 100-109 */
#define ZIS_STUB_ZISCRSVC 100 /* zisCreateService mapped */
#define ZIS_STUB_ZISCSWSV 101 /* zisCreateSpaceSwitchService mapped */
#define ZIS_STUB_ZISCCPSV 102 /* zisCreateCurrentPrimaryService mapped */
#define ZIS_STUB_ZISCSVCA 103 /* zisCreateServiceAnchor mapped */
#define ZIS_STUB_ZISRSVCA 104 /* zisRemoveServiceAnchor mapped */
#define ZIS_STUB_ZISUSVCA 105 /* zisUpdateServiceAnchor mapped */
#define ZIS_STUB_ZISUSAUT 106 /* zisServiceUseSpecificAuth mapped */

/* alloc, 110-139 */
#define ZIS_STUB_MALLOC31 110 /* malloc31 */
#define ZIS_STUB_FREE31   111 /* free31 */
#define ZIS_STUB_SAFEMLLC 112 /* safeMalloc */
#define ZIS_STUB_SAFEMLC2 113 /* safeMalloc2 */
#define ZIS_STUB_SAFMLC31 114 /* safeMalloc31 */
#define ZIS_STUB_SAFMLC38 115 /* safeMalloc31Key8 */
#define ZIS_STUB_SAFEFREE 116 /* safeFree */
#define ZIS_STUB_SAFFRE31 117 /* safeFree31 */
#define ZIS_STUB_SAFFRE38 118 /* safeFree31Key8 */
#define ZIS_STUB_SAFFRE64 119 /* safeFree64 */
#define ZIS_STUB_SFFRE64T 120 /* safeFree64ByToken */
#define ZIS_STUB_SAFMLC64 121 /* safeMalloc64 */
#define ZIS_STUB_SFMLC64T 122 /* safeMalloc64ByToken */
#define ZIS_STUB_SAFRE642 123 /* safeFree64v2 */
#define ZIS_STUB_SAMLC642 124 /* safeMalloc64v2 */
#define ZIS_STUB_SAFRE643 125 /* safeFree64v3 */
#define ZIS_STUB_SAMLC643 126 /* safeMalloc64v3 */
#define ZIS_STUB_ALLCECSA 127 /* allocECSA */
#define ZIS_STUB_FREEECSA 128 /* freeECSA */

/* bpxnet, 140-189 */
#define ZIS_STUB_GETLOCHN 140 /* getLocalHostName */
#define ZIS_STUB_GETSOCDI 141 /* getSocketDebugID */
#define ZIS_STUB_GETLOCAD 142 /* getLocalHostAddress */
#define ZIS_STUB_GETADRBN 143 /* getAddressByName */
#define ZIS_STUB_GETSOCNM 144 /* getSocketName */
#define ZIS_STUB_GETSOCN2 145 /* getSocketName2 */
#define ZIS_STUB_TCPCLIE1 146 /* tcpClient */
#define ZIS_STUB_GTSKTOPT 147 /* getSocketOption */
#define ZIS_STUB_TCPCLIE2 148 /* tcpClient2 */
#define ZIS_STUB_TCPSERVR 149 /* tcpServer */
#define ZIS_STUB_TCPCLIE3 150 /* tcpClient3 */
#define ZIS_STUB_TCPSERV2 151 /* tcpServer2 */
#define ZIS_STUB_MAKSPSOC 152 /* makePipeBasedSyntheticSocket */
#define ZIS_STUB_BPXSLEEP 153 /* bpxSleep */
#define ZIS_STUB_TCPIOCTR 154 /* tcpIOControl */
#define ZIS_STUB_UDPPEER  155 /* udpPeer */
#define ZIS_STUB_UDPRECVF 156 /* udpReceiveFrom */
#define ZIS_STUB_UDPSENDT 157 /* udpSendTo */
#define ZIS_STUB_MAKSOCST 158 /* makeSocketSet */
#define ZIS_STUB_FRESOCST 159 /* freeSocketSet */
#define ZIS_STUB_SOCSTADD 160 /* socketSetAdd */
#define ZIS_STUB_SOCSTREM 161 /* socketSetRemove */
#define ZIS_STUB_TCPSTTUS 162 /* tcpStatus */
#define ZIS_STUB_SOCREAD  163 /* socketRead */
#define ZIS_STUB_SOCWRITE 164 /* socketWrite */
#define ZIS_STUB_SETSKTTO 165 /* setSocketTimeout */
#define ZIS_STUB_SETSKTND 166 /* setSocketNoDelay */
#define ZIS_STUB_SETSKTWB 167 /* setSocketWriteBufferSize */
#define ZIS_STUB_SETSKTRB 168 /* setSocketReadBufferSize */
#define ZIS_STUB_SETSOCBM 169 /* setSocketBlockingMode */
#define ZIS_STUB_SETSKTOP 170 /* setSocketOption */
#define ZIS_STUB_SOCKSEND 171 /* socketSend */
#define ZIS_STUB_SOCACCPT 172 /* socketAccept */
#define ZIS_STUB_SOCCLOSE 173 /* socketClose */
#define ZIS_STUB_EXSELECT 174 /* extendedSelect */
#define ZIS_STUB_MACSOCAD 175 /* makeSocketAddr */
#define ZIS_STUB_MACSOCA6 176 /* makeSocketAddrIPv6 */
#define ZIS_STUB_FRESOCAD 177 /* freeSocketAddr */
#define ZIS_STUB_SOCFREE  178 /* socketFree */

/* cellpool, 190-199 */
#define ZIS_STUB_CPASIZE  190 /* cellpoolGetDWordAlignedSize */
#define ZIS_STUB_CPFREE   191 /* cellpoolFree */
#define ZIS_STUB_CPGET    192 /* cellpoolGet */
#define ZIS_STUB_CPDELETE 193 /* cellpoolDelete */
#define ZIS_STUB_CPBUILD  194 /* cellpoolBuild */

/* cmutils, 200-229 */
#define ZIS_STUB_CMCPWDK  200 /* cmCopyWithDestinationKey */
#define ZIS_STUB_CMCPWSK  201 /* cmCopyWithSourceKey */
#define ZIS_STUB_CMCPTSSK 202 /* cmCopyToSecondaryWithCallerKey */
#define ZIS_STUB_CMCPFSSK 203 /* cmCopyFromSecondaryWithCallerKey */
#define ZIS_STUB_CMCPTPSK 204 /* cmCopyToPrimaryWithCallerKey */
#define ZIS_STUB_CMCPFPSK 205 /* cmCopyFromPrimaryWithCallerKey */
#define ZIS_STUB_CMCPTHSK 206 /* cmCopyToHomeWithCallerKey */
#define ZIS_STUB_CMCPFHSK 207 /* cmCopyFromHomeWithCallerKey */
#define ZIS_STUB_CMCPYSKA 208 /* cmCopyWithSourceKeyAndALET */
#define ZIS_STUB_CMGAACEE 209 /* cmGetCallerAddressSpaceACEE */
#define ZIS_STUB_CMGTACEE 210 /* cmGetCallerTaskACEE */
#define ZIS_STUB_CMALLOC  211 /* cmAlloc */
#define ZIS_STUB_CMFREE   212 /* cmFree */
#define ZIS_STUB_CMALLOC2 213 /* cmAlloc2 */
#define ZIS_STUB_CMFREE2  214 /* cmFree2 */
#define ZIS_STUB_CMUMKMAP 215 /* makeCrossMemoryMap */
#define ZIS_STUB_CMURMMAP 216 /* removeCrossMemoryMap */
#define ZIS_STUB_CMUMAPP  217 /* crossMemoryMapPut */
#define ZIS_STUB_CMUMAPGH 218 /* crossMemoryMapGetHandle */
#define ZIS_STUB_CMUMAPGT 219 /* crossMemoryMapGet */
#define ZIS_STUB_CMUMAPIT 220 /* crossMemoryMapIterate */

/* collections, 230-289 */
#define ZIS_STUB_FBMGRCRT 230 /* fbMgrCreate */
#define ZIS_STUB_FBMGRALC 231 /* fbMgrAlloc */
#define ZIS_STUB_FBMGRFRE 232 /* fbMgrFree */
#define ZIS_STUB_FBMGRDST 233 /* fbMgrDestroy */
#define ZIS_STUB_HTCREATE 234 /* htCreate */
#define ZIS_STUB_HTALTER  235 /* htAlter */
#define ZIS_STUB_HTGET    236 /* htGet */
#define ZIS_STUB_HTINTGET 237 /* htIntGet */
#define ZIS_STUB_HTPUT    238 /* htPut */
#define ZIS_STUB_HTINTPUT 239 /* htIntPut */
#define ZIS_STUB_HTUINTPT 240 /* htUIntPut */
#define ZIS_STUB_HTUINTGT 241 /* htUIntGet */
#define ZIS_STUB_HTREMOVE 242 /* htRemove */
#define ZIS_STUB_HTMAP    243 /* htMap */
#define ZIS_STUB_HTMAP2   244 /* htMap2 */
#define ZIS_STUB_HTPRUNE  245 /* htPrune */
#define ZIS_STUB_HTDSTROY 246 /* htDestroy */
#define ZIS_STUB_HTCOUNT  247 /* htCount */
#define ZIS_STUB_STRNGHSH 248 /* stringHash */
#define ZIS_STUB_STRNGCMP 249 /* stringCompare */
#define ZIS_STUB_MKLRUCHE 250 /* makeLRUCache */
#define ZIS_STUB_DSLRUCHE 251 /* destroyLRUCache */
#define ZIS_STUB_LRUGET   252 /* lruGet */
#define ZIS_STUB_LRUSTORE 253 /* lruStore */
#define ZIS_STUB_LNHTCRTE 254 /* lhtCreate */
#define ZIS_STUB_LNHTALTR 255 /* lhtAlter */
#define ZIS_STUB_LNHDSTRY 256 /* lhtDestroy */
#define ZIS_STUB_LNHTGET  257 /* lhtGet */
#define ZIS_STUB_LNHTPUT  258 /* lhtPut */
#define ZIS_STUB_LNHTREMV 259 /* lhtRemove */
#define ZIS_STUB_LNHTMAP  260 /* lhtMap */
#define ZIS_STUB_MAKELCFQ 261 /* makeQueue */
#define ZIS_STUB_DSTRLCFQ 262 /* destroyQueue */
#define ZIS_STUB_QENQUEUE 263 /* qEnqueue */
#define ZIS_STUB_QDEQUEUE 264 /* qDequeue */
#define ZIS_STUB_QINSERT  265 /* qInsert */
#define ZIS_STUB_QREMOVE  266 /* qRemove */
#define ZIS_STUB_ALSTMAKE 267 /* makeArrayList */
#define ZIS_STUB_ALSTFREE 268 /* arrayListFree */
#define ZIS_STUB_ALSTADD  269 /* arrayListAdd */
#define ZIS_STUB_ALSTELMT 270 /* arrayListElement */
#define ZIS_STUB_ALINEMAR 271 /* initEmbeddedArrayList */
#define ZIS_STUB_ALSTSORT 272 /* arrayListSort */
#define ZIS_STUB_ALSHLCPY 273 /* arrayListShallowCopy */

/* cpool64, 290-299 */
#define ZIS_STUB_CPL64CRE 290  /* iarcp64Create */
#define ZIS_STUB_CPL64GET 291  /* iarcp64Get    */
#define ZIS_STUB_CPL64FRE 292  /* iarcp64Free   */
#define ZIS_STUB_CPL64DEL 293  /* iarcp64Delete */

/* crossmemory, 300-349 */
/* #define ZIS_STUB_CMINILOG 300 cmsInitializeLogging not in CMS_CLIENT */
/* #define ZIS_STUB_CMMCMSRV 301 makeCrossMemoryServer not in CMS_CLIENT */
/* #define ZIS_STUB_CMMCMSR2 302 makeCrossMemoryServer2  not in CMS_CLIENT */
/* #define ZIS_STUB_CMMCRSRV 303 removeCrossMemoryServer not in CMS_CLIENT */
/* #define ZIS_STUB_CMCMSSPP 304 cmsSetPoolParameters not in CMS_CLIENT */
/* #define ZIS_STUB_CMCMSRSR 305 cmsRegisterService not in CMS_CLIENT */
/* #define ZIS_STUB_CMCMAINL 306 cmsStartMainLoop not in CMS_CLIENT */
#define ZIS_STUB_CMGETGA  307 /* cmsGetGlobalArea */
/* #define ZIS_STUB_CMADDPRM 308 cmsAddConfigParm not in CMS_CLIENT */
#define ZIS_STUB_CMTSAUTH 309 /* cmsTestAuth */
#define ZIS_STUB_CMCMSRCS 330 /* cmsCallService */
#define ZIS_STUB_CMCALLS2 331 /* cmsCallService2 */
#define ZIS_STUB_CMCALLS3 332 /* cmsCallService3 */
#define ZIS_STUB_CMCMSPRF 333 /* cmsPrintf */
#define ZIS_STUB_CMCMSVPF 334 /* vcmsPrintf */
#define ZIS_STUB_CMHEXDMP 335 /* cmsHexDump */
#define ZIS_STUB_CMGETPRM 336 /* cmsGetConfigParm */
#define ZIS_STUB_CMGETPRU 337 /* cmsGetConfigParmUnchecked */
#define ZIS_STUB_CMGETLOG 338 /* cmsGetPCLogLevel */
#define ZIS_STUB_CMGETSTS 339 /* cmsGetStatus */
#define ZIS_STUB_CMMKSNAM 340 /* cmsMakeServerName */
/* #define ZIS_STUB_CMECSAAL 341 cmsAllocateECSAStorage not in CMS_CLIENT */
/* #define ZIS_STUB_CMECSAFR 342 cmsFreeECSAStorage not in CMS_CLIENT */
/* #define ZIS_STUB_CMECSAA2 343 cmsAllocateECSAStorage2 not in CMS_CLIENT */
/* #define ZIS_STUB_CMECSAF2 344 cmsFreeECSAStorage2 not in CMS_CLIENT */

/* isgenq, 350-359 */
#define ZIS_STUB_ENQTRYX  350 /* isgenqTryExclusiveLock mapped */
#define ZIS_STUB_ENQGETX  351 /* isgenqGetExclusiveLock mapped */
#define ZIS_STUB_ENQTRYS  352 /* isgenqTrySharedLock mapped */
#define ZIS_STUB_ENQGETS  353 /* isgenqGetSharedLock mapped */
#define ZIS_STUB_ENQTSTX  354 /* isgenqTestExclusiveLock mapped */
#define ZIS_STUB_ENQTSTS  355 /* isgenqTestSharedLock mapped */
#define ZIS_STUB_ENQTEST  356 /* isgenqTestLock mapped */
#define ZIS_STUB_ENQREL   357 /* isgenqReleaseLock mapped */

/* le, 360-379 */
#define ZIS_STUB_LEGETCAA 360 /* getCAA */
#define ZIS_STUB_LESHWRTL 361 /* showRTL */
#define ZIS_STUB_LEMKRLEA 362 /* makeRLEAnchor */
#define ZIS_STUB_LEDLRLEA 363 /* deleteRLEAnchor */
#define ZIS_STUB_LEMKRLET 364 /* makeRLETask */
#define ZIS_STUB_LEDLRLET 365 /* deleteRLETask */
#define ZIS_STUB_LEINRLEE 366 /* initRLEEnvironment */
#define ZIS_STUB_LETMRLEE 367 /* termRLEEnvironment */
#define ZIS_STUB_LEMKFCAA 368 /* makeFakeCAA */
#define ZIS_STUB_LEARTCAA 369 /* abortIfUnsupportedCAA */

/* logging, 380-409 */
#define ZIS_STUB_MKLOGCTX 380 /* makeLoggingContext */
#define ZIS_STUB_MKLLGCTX 381 /* makeLocalLoggingContext */
#define ZIS_STUB_RMLOGCTX 382 /* removeLoggingContext */
#define ZIS_STUB_RMLLGCTX 383 /* removeLocalLoggingContext */
#define ZIS_STUB_GTLOGCTX 384 /* getLoggingContext */
#define ZIS_STUB_STLOGCTX 385 /* setLoggingContext */
#define ZIS_STUB_ZOWELOG  386 /* zowelog */
#define ZIS_STUB_ZOWEDUMP 387 /* zowedump */
#define ZIS_STUB_LGCFGDST 388 /* logConfigureDestination */
#define ZIS_STUB_LGCFGDS2 389 /* logConfigureDestination2 */
#define ZIS_STUB_LGCFGSTD 390 /* logConfigureStandardDestinations */
#define ZIS_STUB_LGCFGCMP 391 /* logConfigureComponent */
#define ZIS_STUB_LGSETLVL 392 /* logSetLevel */
#define ZIS_STUB_LGGETLVL 393 /* logGetLevel */
#define ZIS_STUB_LGSHTRCE 394 /* logShouldTraceInternal */
#define ZIS_STUB_LGGLOGCX 395 /* logGetExternalContext */
#define ZIS_STUB_LGSLOGCX 396 /* logSetExternalContext */
#define ZIS_STUB_LGPRSOUT 397 /* printStdout */
#define ZIS_STUB_LGPRSERR 398 /* printStderr */

/* lpa, 410-419 */
#define ZIS_STUB_LPAADD   410 /* lpaAdd mapped */
#define ZIS_STUB_LPADEL   411 /* lpaDelete mapped */

/* metalio, 420-439 */
/* #define ZIS_STUB_GTNMTKFT 420 getNameTokenFunctionTable - Defunct */
#define ZIS_STUB_GTNMTKVL 421 /* getNameTokenValue */
#define ZIS_STUB_CRNMTKPR 422 /* createNameTokenPair */
#define ZIS_STUB_DLNMTKPR 423 /* deleteNameTokenPair */
/* #define ZIS_STUB_INITSYTS 424 initSysouts - Not used */
#define ZIS_STUB_GTSYSOUT 425 /* getSYSOUTStruct */
#define ZIS_STUB_WTOPRNF  426 /* wtoPrintf */
#define ZIS_STUB_AWTOPRNF 427 /* authWTOPrintf */
#define ZIS_STUB_SENDWTO  428 /* sendWTO */
#define ZIS_STUB_QSAMPRNF 429 /* qsamPrintf */
#define ZIS_STUB_PRINTF   430 /* printf */
#define ZIS_STUB_FFLUSH   431 /* fflush */
#define ZIS_STUB_SETJMP   432 /* setjmp */
#define ZIS_STUB_LONGJMP  433 /* longjmp */

/* nametoken, 440-449 */
#define ZIS_STUB_NTPCREF  440 /* nameTokenCreate */
#define ZIS_STUB_NTPCRTPF 441 /* nameTokenCreatePersistent */
#define ZIS_STUB_NTPDELF  442 /* nameTokenDelete */
#define ZIS_STUB_NTPRETF  443 /* nameTokenRetrieve */

/* pause-element, 450-459 */
#define ZIS_STUB_PEALLOC  450  /* peAlloc    */
#define ZIS_STUB_PEDALLOC 451  /* peDealloc  */
#define ZIS_STUB_PEPAUSE  452  /* pePause    */
#define ZIS_STUB_PERELEAS 453  /* peRelease  */
#define ZIS_STUB_PERTVNFO 454  /* peRetrieveInfo  */
#define ZIS_STUB_PERTVNF2 455  /* peRetrieveInfo2 */
#define ZIS_STUB_PETEST   456  /* peTest          */
#define ZIS_STUB_PETRNSFR 457  /* peTransfer      */

/* qsam, 460-489 */
#define ZIS_STUB_MALLOC24 460 /* malloc24 */
#define ZIS_STUB_OPENSAM  461 /* openSAM */
#define ZIS_STUB_GTSAMLN  462 /* getSAMLength */
#define ZIS_STUB_GTSAMBS  463 /* getSAMBlksize */
#define ZIS_STUB_GTSAMLR  464 /* getSAMLrecl */
#define ZIS_STUB_GTSAMCC  465 /* getSAMCC */
#define ZIS_STUB_GTSAMRF  466 /* getSAMRecfm */
#define ZIS_STUB_PUTLINE  467 /* putline */
#define ZIS_STUB_GETLINE  468 /* getline */
#define ZIS_STUB_GTEODBUF 469 /* getEODADBuffer */
#define ZIS_STUB_FREODBUF 470 /* freeEODADBuffer */
#define ZIS_STUB_GETLINE2 471 /* getline2 */
#define ZIS_STUB_MKQSMBUF 472 /* makeQSAMBuffer */
#define ZIS_STUB_FRQSMBUF 473 /* freeQSAMBuffer */
#define ZIS_STUB_PUTLNEV  474 /* putlineV */
#define ZIS_STUB_GETLNEV  475 /* getlineV */
#define ZIS_STUB_CLOSESAM 476 /* closeSAM */
#define ZIS_STUB_OPENEXCP 477 /* openEXCP */
#define ZIS_STUB_CLOSEXCP 478 /* closeEXCP */
#define ZIS_STUB_HASVRLEN 479 /* hasVaryingRecordLength */
#define ZIS_STUB_BPAMDELM 480 /* bpamDeleteMember */
#define ZIS_STUB_BPAMFIND 481 /* bpamFind */
#define ZIS_STUB_BPAMREAD 482 /* bpamRead */
#define ZIS_STUB_BPAMRD2  483 /* bpamRead2 */

/* radmin, 490-509 */
#define ZIS_STUB_RADMXUP  490 /* radminExtractUserProfiles mapped */
#define ZIS_STUB_RADMXBUP 491 /* radminExtractBasicUserProfileInfo mapped */
#define ZIS_STUB_RADMXBRP 492 /* radminExtractBasicGenresProfileInfo mapped */
#define ZIS_STUB_RADMXBGP 493 /* radminExtractBasicGroupProfileInfo mapped */
#define ZIS_STUB_RADMXPAL 494 /* radminExtractGenresAccessList mapped */
#define ZIS_STUB_RADMXGAL 495 /* radminExtractGroupAccessList mapped */
#define ZIS_STUB_RADMPREA 496 /* radminPerformResAction mapped */
#define ZIS_STUB_RADMPGRA 497 /* radminPerformGroupAction mapped */
#define ZIS_STUB_RADMPCNA 498 /* radminPerformConnectionAction mapped */
#define ZIS_STUB_RADMRCMD 499 /* radminRunRACFCommand mapped */

/* recovery, 510-529 */
#define ZIS_STUB_RCVRESRR 510 /* recoveryEstablishRouter */
#define ZIS_STUB_RCVRRSRR 511 /* recoveryRemoveRouter */
#define ZIS_STUB_RCVRTRES 512 /* recoveryIsRouterEstablished */
#define ZIS_STUB_RCVRPSFR 513 /* recoveryPush */
#define ZIS_STUB_RCVRPPFR 514 /* recoveryPop */
#define ZIS_STUB_RCVRSDTL 515 /* recoverySetDumpTitle */
#define ZIS_STUB_RCVRSFLV 516 /* recoverySetFlagValue */
#define ZIS_STUB_RCVRECST 517 /* recoveryEnableCurrentState */
#define ZIS_STUB_RCVRDCST 518 /* recoveryDisableCurrentState */
#define ZIS_STUB_RCVRURSI 519 /* recoveryUpdateRouterServiceInfo */
#define ZIS_STUB_RCVRUSSI 520 /* recoveryUpdateStateServiceInfo */
#define ZIS_STUB_RCVRGACD 521 /* recoveryGetABENDCode */
/* #define ZIS_STUB_RCVRNFNE 522 - defunct */
/* #define ZIS_STUB_SHRCVRST 523 - defunct  */

/* scheduling, 530-539 */
#define ZIS_STUB_SCHZOSWT 530 /* zosWait */
#define ZIS_STUB_SCHZOSWL 531 /* zosWaitList */
#define ZIS_STUB_SCHZOSPT 532 /* zosPost */
#define ZIS_STUB_SCHSRLET 533 /* startRLETask */

/* shrmem64, 540-559 */
#define ZIS_STUB_SHR64TKN 540 /* shrmem64GetAddressSpaceToken */
#define ZIS_STUB_SHR64ALC 541 /* shrmem64Alloc */
#define ZIS_STUB_SHR64AL2 542 /* shrmem64Alloc2 */
#define ZIS_STUB_SHR64CAL 543 /* shrmem64CommonAlloc */
#define ZIS_STUB_SHR64CA2 544 /* shrmem64CommonAlloc2 */
#define ZIS_STUB_SHR64REL 545 /* shrmem64Release */
#define ZIS_STUB_SHR64REA 546 /* shrmem64ReleaseAll */
#define ZIS_STUB_SHR64GAC 547 /* shrmem64GetAccess */
#define ZIS_STUB_SHR64GA2 548 /* shrmem64GetAccess2 */
#define ZIS_STUB_SHR64RAC 549 /* shrmem64RemoveAccess */
#define ZIS_STUB_SHR64RA2 550 /* shrmem64RemoveAccess2 */

/* utils, 560-659 */
#define ZIS_STUB_STRCPSAF 560 /* strcopy_safe */
#define ZIS_STUB_INDEXOF  561 /* indexOf */
#define ZIS_STUB_IDXSTR   562 /* indexOfString */
#define ZIS_STUB_LIDXSTR  563 /* lastIndexOfString */
#define ZIS_STUB_LINDEXOF 564 /* lastIndexOf */
#define ZIS_STUB_IDXSTRNS 565 /* indexOfStringInsensitive */
#define ZIS_STUB_ISZEROS  566 /* isZeros */
#define ZIS_STUB_ISBLANKS 567 /* isBlanks */
#define ZIS_STUB_HASTEXT  568 /* hasText */
#define ZIS_STUB_PARSEINT 569 /* parseInt */
#define ZIS_STUB_PSINTINT 570 /* parseInitialInt */
#define ZIS_STUB_NULLTERM 571 /* nullTerminate */
#define ZIS_STUB_ISCHARAN 572 /* isCharAN */
#define ZIS_STUB_TKGTDCML 573 /* tknGetDecimal */
#define ZIS_STUB_TKGTQOTD 574 /* tknGetQuoted */
#define ZIS_STUB_TKGTANUM 575 /* tknGetAlphanumeric */
#define ZIS_STUB_TKGTSTND 576 /* tknGetStandard */
#define ZIS_STUB_TKGTNWSP 577 /* tknGetNonWhitespace */
#define ZIS_STUB_TKGTTERM 578 /* tknGetTerminating */
#define ZIS_STUB_TKTXEQLS 579 /* tknTextEquals */
#define ZIS_STUB_FREETKN  580 /* freeToken */
#define ZIS_STUB_TKNTEXT  581 /* tknText */
#define ZIS_STUB_TKNINT   582 /* tknInt */
#define ZIS_STUB_TKNLNGTH 583 /* tknLength */
#define ZIS_STUB_DMPBUFFR 584 /* dumpbuffer */
#define ZIS_STUB_DMPBUFFA 585 /* dumpbufferA */
#define ZIS_STUB_HEXFILL  586 /* hexFill */
#define ZIS_STUB_SMPHXFIL 587 /* simpleHexFill */
#define ZIS_STUB_SMPHXPRT 588 /* simpleHexPrint */
#define ZIS_STUB_SMPHXPRL 589 /* simpleHexPrintLower */
#define ZIS_STUB_HEXDUMP  590 /* hexdump */
#define ZIS_STUB_DMPBFFR2 591 /* dumpbuffer2 */
#define ZIS_STUB_DMPBFFRS 592 /* dumpBufferToStream */
#define ZIS_STUB_CMPIGNCS 593 /* compareIgnoringCase */
#define ZIS_STUB_STRUPCAS 594 /* strupcase */
#define ZIS_STUB_MAKESLH  595 /* makeShortLivedHeap */
#define ZIS_STUB_MAKSLH64 596 /* makeShortLivedHeap64 */
#define ZIS_STUB_SLHALLOC 597 /* SLHAlloc */
#define ZIS_STUB_SLHFREE  598 /* SLHFree */
#define ZIS_STUB_NYMALLOC 599 /* noisyMalloc */
#define ZIS_STUB_DECODB32 600 /* base32Encode */
#define ZIS_STUB_ENCODB32 601 /* base32Decode */
#define ZIS_STUB_DECODB64 602 /* decodeBase64 */
#define ZIS_STUB_DECDB64U 603 /* decodeBase64Unterminated */
#define ZIS_STUB_ENCODB64 604 /* encodeBase64 */
#define ZIS_STUB_ENCDB64N 605 /* encodeBase64NoAlloc */
#define ZIS_STUB_B642BURL 606 /* base64ToBase64url */
#define ZIS_STUB_B64URLTB 607 /* base64urlToBase64 */
#define ZIS_STUB_CLNURLPV 608 /* cleanURLParamValue */
#define ZIS_STUB_PCTENCOD 609 /* percentEncode */
#define ZIS_STUB_DSTUNASC 610 /* destructivelyUnasciify */
#define ZIS_STUB_MAKESLST 611 /* makeStringList */
#define ZIS_STUB_SLSTLEN  612 /* stringListLength */
#define ZIS_STUB_SLSTPRNT 613 /* stringListPrint */
#define ZIS_STUB_SLSTCTNS 614 /* stringListContains */
#define ZIS_STUB_SLSTLAST 615 /* stringListLast */
#define ZIS_STUB_ADSLSTUQ 616 /* addToStringListUnique */
#define ZIS_STUB_ADSLST   617 /* addToStringList */
#define ZIS_STUB_SLSTELT1 618 /* firstStringListElt */
#define ZIS_STUB_STRCNCAT 619 /* stringConcatenate */
#define ZIS_STUB_MKBFCHST 620 /* makeBufferCharStream */
#define ZIS_STUB_CSTRPOSN 621 /* charStreamPosition */
#define ZIS_STUB_CSTRGET  622 /* charStreamGet */
#define ZIS_STUB_CSTREOF  623 /* charStreamEOF */
#define ZIS_STUB_CSTRCLOS 624 /* charStreamClose */
#define ZIS_STUB_CSTRFREE 625 /* charStreamFree */
#define ZIS_STUB_PADWSPCS 626 /* padWithSpaces */
#define ZIS_STUB_RPLTRMNL 627 /* replaceTerminateNulls */
#define ZIS_STUB_CNVINTST 628 /* convertIntToString */
#define ZIS_STUB_HEXTODEC 629 /* hexToDec */
#define ZIS_STUB_DECTOHEX 630 /* decToHex */
#define ZIS_STUB_COMPSEQS 631 /* compareSequences */
#define ZIS_STUB_DEC2OCT  632 /* decimalToOctal */
#define ZIS_STUB_UNX2ISO  633 /* convertUnixToISO */
#define ZIS_STUB_MATCHWLD 634 /* matchWithWildcards */
#define ZIS_STUB_STRISDIG 635 /* stringIsDigit */
#define ZIS_STUB_TRMRIGHT 636 /* trimRight */
#define ZIS_STUB_ISPASPHR 637 /* isPassPhrase */

/* xlate, 660-669 */
#define ZIS_STUB_E2A      660 /* e2a */
#define ZIS_STUB_A2E      661 /* a2e */

/* zos, 670-729 */
#define ZIS_STUB_EXTRPSW  670 /* extractPSW */
#define ZIS_STUB_SUPRMODE 671 /* supervisorMode */
#define ZIS_STUB_SETKEY   672 /* setKey */
#define ZIS_STUB_DDEXISTS 673 /* ddnameExists */
#define ZIS_STUB_ATOMINCR 674 /* atomicIncrement */
#define ZIS_STUB_GTCRACEE 675 /* getCurrentACEE */
#define ZIS_STUB_GTFCHTCB 676 /* getFirstChildTCB */
#define ZIS_STUB_GTPRTTCB 677 /* getParentTCB */
#define ZIS_STUB_GTNXSTCB 678 /* getNextSiblingTCB */
#define ZIS_STUB_GETCVT   679 /* getCVT */
#define ZIS_STUB_GETATCVT 680 /* getATCVT */
#define ZIS_STUB_GETCSTBL 681 /* getIEACSTBL */
#define ZIS_STUB_GETCVTPR 682 /* getCVTPrefix */
#define ZIS_STUB_GETECVT  683 /* getECVT */
#define ZIS_STUB_GETTCB   684 /* getTCB */
#define ZIS_STUB_GETSTCB  685 /* getSTCB */
#define ZIS_STUB_GETOTCB  686 /* getOTCB */
#define ZIS_STUB_GETASCB  687 /* getASCB */
#define ZIS_STUB_GETASXB  688 /* getASXB */
#define ZIS_STUB_GETASSB  689 /* getASSB */
#define ZIS_STUB_GETJSAB  690 /* getJSAB */
#define ZIS_STUB_GTSPLXNM 691 /* getSysplexName */
#define ZIS_STUB_GTSYSTNM 692 /* getSystemName */
#define ZIS_STUB_GETDSAB  693 /* getDSAB */
#define ZIS_STUB_DSABOMVS 694 /* dsabIsOMVS */
#define ZIS_STUB_LOCATE   695 /* locate */
#define ZIS_STUB_GETR13   696 /* getR13 */
#define ZIS_STUB_GETR12   697 /* getR12 */
#define ZIS_STUB_GETASCBJ 698 /* getASCBJobname */
#define ZIS_STUB_LOADBYNA 699 /* loadByName */
#define ZIS_STUB_LOADBNML 700 /* loadByNameLocally */
#define ZIS_STUB_ZOSCLCKD 701 /* isCallerLocked */
#define ZIS_STUB_ZOSCSRB  702 /* isCallerSRB */
#define ZIS_STUB_ZOSCXMEM 703 /* isCallerCrossMemory */
#define ZIS_STUB_GADSACEE 704 /* getAddressSpaceAcee */
#define ZIS_STUB_GTSKACEE 705 /* getTaskAcee */
#define ZIS_STUB_STSKACEE 706 /* setTaskAcee */
#define ZIS_STUB_SAFVRIFY 707 /* safVerify */
#define ZIS_STUB_SAFVRFY2 708 /* safVerify2 */
#define ZIS_STUB_SAFVRFY3 709 /* safVerify3 */
#define ZIS_STUB_SAFVRFY4 710 /* safVerify4 */
#define ZIS_STUB_SAFVRFY5 711 /* safVerify5 */
#define ZIS_STUB_SAFVRFY6 712 /* safVerify6 */
#define ZIS_STUB_SAFVRFY7 713 /* safVerify7 */
#define ZIS_STUB_SAFAUTH  714 /* safAuth */
#define ZIS_STUB_SAFAUTHS 715 /* safAuthStatus */
#define ZIS_STUB_SAFSTAT  716 /* safStat */
#define ZIS_STUB_GETSAFPL 717 /* getSafProfileMaxLen */

/* zvt, 730-739 */
#define ZIS_STUB_ZVTINIT  730 /* zvtInit mapped */
#define ZIS_STUB_ZVTGET   731 /* zvtGet mapped */
#define ZIS_STUB_ZVTAENTR 732 /* zvtAllocEntry mapped */
#define ZIS_STUB_ZVTFENTR 733 /* zvtFreeEntry mapped */
#define ZIS_STUB_ZVTGXMLR 734 /* zvtGetCMSLookupRoutineAnchor mapped */
#define ZIS_STUB_ZVTSXMLR 735 /* zvtSetCMSLookupRoutineAnchor mapped */

/* dynalloc, 740-769 */
#define ZIS_STUB_DYNASTXU 740 /* createSimpleTextUnit mapped */
#define ZIS_STUB_DYNASTX2 741 /* createSimpleTextUnit2 mapped */
#define ZIS_STUB_DYNACTXU 742 /* createCharTextUnit mapped */
#define ZIS_STUB_DYNACMTU 743 /* createCompoundTextUnit mapped */
#define ZIS_STUB_DYNAITXU 744 /* createIntTextUnit mapped */
#define ZIS_STUB_DYNAI1TU 745 /* createInt8TextUnit mapped */
#define ZIS_STUB_DYNAI2TU 746 /* createInt16TextUnit mapped */
#define ZIS_STUB_DYNAFTXU 747 /* freeTextUnit mapped */
#define ZIS_STUB_DYNAAIRD 748 /* AllocIntReader mapped */
#define ZIS_STUB_DYNASLIB 749 /* dynallocSharedLibrary mapped */
#define ZIS_STUB_DYNAUSSD 750 /* dynallocUSSDirectory mapped */
#define ZIS_STUB_DYNAUSSO 751 /* dynallocUSSOutput mapped */
#define ZIS_STUB_DYNASAPI 752 /* AllocForSAPI mapped */
#define ZIS_STUB_DYNADOUT 753 /* AllocForDynamicOutput mapped */
#define ZIS_STUB_DYNADDDN 754 /* DeallocDDName mapped */
#define ZIS_STUB_DYNAUALC 755 /* dynallocDataset mapped */
#define ZIS_STUB_DYNAUALM 756 /* dynallocDatasetMember mapped */
#define ZIS_STUB_DYNADALC 757 /* unallocDataset mapped */
#define ZIS_STUB_DYNALAD2 758 /* dynallocAllocDataset */
#define ZIS_STUB_DYNALUD2 759 /* dynallocUnallocDatasetByDDName */

/* vsam, 770-789 */
#define ZIS_STUB_MAKEACB  770 /* makeACB */
#define ZIS_STUB_VOPCLACB 771 /* opencloseACB */
#define ZIS_STUB_OPENACB  772 /* openACB */
#define ZIS_STUB_CLOSEACB 773 /* closeACB */
#define ZIS_STUB_MODRPL   774 /* modRPL */
#define ZIS_STUB_VSPOINT  775 /* point */
#define ZIS_STUB_VPUTREC  776 /* putRecord */
#define ZIS_STUB_VGETREC  777 /* getRecord */
#define ZIS_STUB_VMKDBUFF 778 /* makeDataBuffer */
#define ZIS_STUB_VFRDBUFF 779 /* freeDataBuffer */
#define ZIS_STUB_VPNBYRBA 780 /* pointByRBA */
#define ZIS_STUB_VPNBYKEY 781 /* pointByKey */
#define ZIS_STUB_VPNBYREC 782 /* pointByRecord */
#define ZIS_STUB_VPNBYCI  783 /* pointByCI */
#define ZIS_STUB_VALLOCDS 784 /* allocateDataset */
#define ZIS_STUB_VDEFAIX  785 /* defineAIX */
#define ZIS_STUB_DELCLUST 786 /* deleteCluster */

/* idcams, 790-799 */
#define ZIS_STUB_RSIDCCCM 790 /* idcamsCreateCommand mapped */
#define ZIS_STUB_RSIDCALN 791 /* idcamsAddLineToCommand mapped */
#define ZIS_STUB_RSIDCEXE 792 /* idcamsExecuteCommand mapped */
#define ZIS_STUB_RSIDCDCM 793 /* idcamsDeleteCommand mapped */
#define ZIS_STUB_RSIDCPCO 794 /* idcamsPrintCommandOutput mapped */
#define ZIS_STUB_RSIDCDCO 795 /* idcamsDeleteCommandOutput mapped */

/* timeutls, 780-819 */
#define ZIS_STUB_GETSTCK  800 /* getSTCK */
#define ZIS_STUB_GETSTCKU 801 /* getSTCKU */
#define ZIS_STUB_CONVTODL 802 /* convertTODToLocal */
#define ZIS_STUB_TZDIFFOR 803 /* timeZoneDifferenceFor */
#define ZIS_STUB_STCKCONV 804 /* stckToTimestamp */
#define ZIS_STUB_CONVTOD  805 /* timestampToSTCK */
#define ZIS_STUB_MIDNIGHT 806 /* timeFromMidnight */
#define ZIS_STUB_STCKYYMD 807 /* stckFromYYYYMMDD */
#define ZIS_STUB_ELPSTIME 808 /* elapsedTime */
#define ZIS_STUB_STCKUNIX 809 /* stckToUnix */
#define ZIS_STUB_STCK2USM 810 /* stckToUnixSecondsAndMicros */
#define ZIS_STUB_CONVUNIX 811 /* unixToTimestamp */
#define ZIS_STUB_GTDAYMNT 812 /* getDayAndMonth */
#define ZIS_STUB_SNPRNTLT 813 /* snprintLocalTime */

/* as, 820-829 */
#define ZIS_STUB_ASCREATE 820 /* addressSpaceCreate */
#define ZIS_STUB_ASCREATT 821 /* addressSpaceCreateWithTerm */
#define ZIS_STUB_ASTERMIN 822 /* addressSpaceTerminate */
#define ZIS_STUB_ASEXTRCT 823 /* addressSpaceExtractParm */

/* pc, 830-849 */
#define ZIS_STUB_PCAXSET  830 /* pcSetAllAddressSpaceAuthority */
#define ZIS_STUB_PCLXRES  831 /* pcReserveLinkageIndex */
#define ZIS_STUB_PCLXFRE  832 /* pcFreeLinkageIndex */
#define ZIS_STUB_PCETCRED 833 /* pcMakeEntryTableDescriptor */
#define ZIS_STUB_PCETADD  834 /* pcAddToEntryTableDescriptor */
#define ZIS_STUB_PCETREMD 835 /* pcRemoveEntryTableDescriptor */
#define ZIS_STUB_PCETCRE  836 /* pcCreateEntryTable */
#define ZIS_STUB_PCETDES  837 /* pcDestroyEntryTable */
#define ZIS_STUB_PCETCON  838 /* pcConnectEntryTable */
#define ZIS_STUB_PCETDIS  839 /* pcDisconnectEntryTable */
#define ZIS_STUB_PCCALLR  840 /* pcCallRoutine */

/* zis-aux/aux-guest, 850-859 */
#define ZIS_STUB_ZISAMMDR 850 /* zisAUXMakeModuleDescriptor */
#define ZIS_STUB_ZISADMDR 851 /* zisAUXDestroyModuleDescriptor */

/* zis-aux/aux-manager, 860-879 */
#define ZIS_STUB_ZISAMIN  860 /* zisauxMgrInit */
#define ZIS_STUB_ZISAMCL  861 /* zisauxMgrClean */
#define ZIS_STUB_ZISAMHS  862 /* zisauxMgrSetHostSTC */
#define ZIS_STUB_ZISAMST  863 /* zisauxMgrStartGuest */
#define ZIS_STUB_ZISAMST2 864 /* zisauxMgrStartGuest2 */
#define ZIS_STUB_ZISAMSP  865 /* zisauxMgrStopGuest */
#define ZIS_STUB_ZISAICMD 866 /* zisauxMgrInitCommand */
#define ZIS_STUB_ZISACCMD 867 /* zisauxMgrCleanCommand */
#define ZIS_STUB_ZISAIRQP 868 /* zisauxMgrInitRequestPayload */
#define ZIS_STUB_ZISACRQP 869 /* zisauxMgrCleanRequestPayload */
#define ZIS_STUB_ZISAIRSP 870 /* zisauxMgrInitRequestResponse */
#define ZIS_STUB_ZISACRSP 871 /* zisauxMgrCleanRequestResponse */
#define ZIS_STUB_ZISADRSP 872 /* zisauxMgrCopyRequestResponseData */
#define ZIS_STUB_ZISAMTM  873 /* zisauxMgrSendTermSignal */
#define ZIS_STUB_ZISAMCM  874 /* zisauxMgrSendCommand */
#define ZIS_STUB_ZISAMWK  875 /* zisauxMgrSendWork */
#define ZIS_STUB_ZISAMWT  876 /* zisauxMgrWaitForTerm */

/* zis-aux/aux-utils, 880-889 */
#define ZIS_STUB_AUXUPPFX 880 /* auxutilPrintWithPrefix */
#define ZIS_STUB_AUXUDMSG 881 /* auxutilDumpWithEmptyMessageID */
#define ZIS_STUB_AUXUTKNS 882 /* auxutilTokenizeString */
#define ZIS_STUB_AUXUGTCB 883 /* auxutilGetTCBKey */
#define ZIS_STUB_AUXUGCK  884 /* auxutilGetCallersKey */
#define ZIS_STUB_AUXUWT   885 /* auxutilWait */
#define ZIS_STUB_AUXUPST  886 /* auxutilPost */
#define ZIS_STUB_AUXUSLP  887 /* auxutilSleep */

#endif /* ZIS_ZISSTUBS_H_ */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
