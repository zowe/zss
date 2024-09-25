
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

#define ZIS_STUBS_VERSION 5

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
#define ZIS_STUB_DYNZISUD 0 /* zisdynUndefinedStub  */
#define ZIS_STUB_ZISDYNSV 1 /* zisdynGetStubVersion mapped */
#define ZIS_STUB_ZISDYNPV 2 /* zisdynGetPluginVersion mapped */
#define ZIS_STUB_ZISGVRSN 3 /* zisGetServerVersion mapped */
#define ZIS_STUB_ZISLPADV 4 /* zisIsLPADevModeOn mapped */
#define ZIS_STUB_ZISMDREG 5 /* zisIsModregOn mapped */

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
#define ZIS_STUB_ZISMAKPS 80 /* zisMakeParmSet mapped */
#define ZIS_STUB_ZISREMPS 81 /* zisRemoveParmSet mapped */
#define ZIS_STUB_ZISRDLIB 82 /* zisReadParmlib mapped */
#define ZIS_STUB_ZISRDMPR 83 /* zisReadMainParms mapped */
#define ZIS_STUB_ZISPUTPV 84 /* zisPutParmValue mapped */
#define ZIS_STUB_ZISGETPV 85 /* zisGetParmValue mapped */
#define ZIS_STUB_ZISLOADP 86 /* zisLoadParmsToCMServer mapped */
#define ZIS_STUB_ZISITERP 87 /* zisIterateParms mapped */

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

/* zis-aux/aux-guest, 110-119 */
#define ZIS_STUB_ZISAMMDR 110 /* zisAUXMakeModuleDescriptor */
#define ZIS_STUB_ZISADMDR 111 /* zisAUXDestroyModuleDescriptor */

/* zis-aux/aux-manager, 120-149 */
#define ZIS_STUB_ZISAMIN  120 /* zisauxMgrInit */
#define ZIS_STUB_ZISAMCL  121 /* zisauxMgrClean */
#define ZIS_STUB_ZISAMHS  122 /* zisauxMgrSetHostSTC */
#define ZIS_STUB_ZISAMST  123 /* zisauxMgrStartGuest */
#define ZIS_STUB_ZISAMST2 124 /* zisauxMgrStartGuest2 */
#define ZIS_STUB_ZISAMSP  125 /* zisauxMgrStopGuest */
#define ZIS_STUB_ZISAICMD 126 /* zisauxMgrInitCommand */
#define ZIS_STUB_ZISACCMD 127 /* zisauxMgrCleanCommand */
#define ZIS_STUB_ZISAIRQP 128 /* zisauxMgrInitRequestPayload */
#define ZIS_STUB_ZISACRQP 129 /* zisauxMgrCleanRequestPayload */
#define ZIS_STUB_ZISAIRSP 130 /* zisauxMgrInitRequestResponse */
#define ZIS_STUB_ZISACRSP 131 /* zisauxMgrCleanRequestResponse */
#define ZIS_STUB_ZISADRSP 132 /* zisauxMgrCopyRequestResponseData */
#define ZIS_STUB_ZISAMTM  133 /* zisauxMgrSendTermSignal */
#define ZIS_STUB_ZISAMCM  134 /* zisauxMgrSendCommand */
#define ZIS_STUB_ZISAMWK  135 /* zisauxMgrSendWork */
#define ZIS_STUB_ZISAMWT  136 /* zisauxMgrWaitForTerm */

/* zis-aux/aux-utils, 150-159 */
#define ZIS_STUB_AUXUPPFX 150 /* auxutilPrintWithPrefix */
#define ZIS_STUB_AUXUDMSG 151 /* auxutilDumpWithEmptyMessageID */
#define ZIS_STUB_AUXUTKNS 152 /* auxutilTokenizeString */
#define ZIS_STUB_AUXUGTCB 153 /* auxutilGetTCBKey */
#define ZIS_STUB_AUXUGCK  154 /* auxutilGetCallersKey */
#define ZIS_STUB_AUXUWT   155 /* auxutilWait */
#define ZIS_STUB_AUXUPST  156 /* auxutilPost */
#define ZIS_STUB_AUXUSLP  157 /* auxutilSleep */

/* alloc, 160-189 */
#define ZIS_STUB_MALLOC31 160 /* malloc31 */
#define ZIS_STUB_FREE31   161 /* free31 */
#define ZIS_STUB_SAFEMLLC 162 /* safeMalloc */
#define ZIS_STUB_SAFEMLC2 163 /* safeMalloc2 */
#define ZIS_STUB_SAFMLC31 164 /* safeMalloc31 */
#define ZIS_STUB_SAFMLC38 165 /* safeMalloc31Key8 */
#define ZIS_STUB_SAFEFREE 166 /* safeFree */
#define ZIS_STUB_SAFFRE31 167 /* safeFree31 */
#define ZIS_STUB_SAFFRE38 168 /* safeFree31Key8 */
#define ZIS_STUB_SAFFRE64 169 /* safeFree64 */
#define ZIS_STUB_SFFRE64T 170 /* safeFree64ByToken */
#define ZIS_STUB_SAFMLC64 171 /* safeMalloc64 */
#define ZIS_STUB_SFMLC64T 172 /* safeMalloc64ByToken */
#define ZIS_STUB_SAFRE642 173 /* safeFree64v2 */
#define ZIS_STUB_SAMLC642 174 /* safeMalloc64v2 */
#define ZIS_STUB_SAFRE643 175 /* safeFree64v3 */
#define ZIS_STUB_SAMLC643 176 /* safeMalloc64v3 */
#define ZIS_STUB_ALLCECSA 177 /* allocECSA */
#define ZIS_STUB_FREEECSA 178 /* freeECSA */

/* as, 190-199 */
#define ZIS_STUB_ASCREATE 190 /* addressSpaceCreate */
#define ZIS_STUB_ASCREATT 191 /* addressSpaceCreateWithTerm */
#define ZIS_STUB_ASTERMIN 192 /* addressSpaceTerminate */
#define ZIS_STUB_ASEXTRCT 193 /* addressSpaceExtractParm */

/* bpxnet, 200-249 */
#define ZIS_STUB_GETLOCHN 200 /* getLocalHostName */
/* #define ZIS_STUB_GETSOCDI 201 getSocketDebugID - no metal impl */
#define ZIS_STUB_GETLOCAD 202 /* getLocalHostAddress */
#define ZIS_STUB_GETADRBN 203 /* getAddressByName */
#define ZIS_STUB_GETSOCNM 204 /* getSocketName */
#define ZIS_STUB_GETSOCN2 205 /* getSocketName2 */
#define ZIS_STUB_TCPCLIE1 206 /* tcpClient */
#define ZIS_STUB_GTSKTOPT 207 /* getSocketOption */
#define ZIS_STUB_TCPCLIE2 208 /* tcpClient2 */
#define ZIS_STUB_TCPSERVR 209 /* tcpServer */
#define ZIS_STUB_TCPCLIE3 210 /* tcpClient3 */
#define ZIS_STUB_TCPSERV2 211 /* tcpServer2 */
/* #define ZIS_STUB_MAKSPSOC 212 makePipeBasedSyntheticSocket - no metal impl */
#define ZIS_STUB_BPXSLEEP 213 /* bpxSleep */
#define ZIS_STUB_TCPIOCTR 214 /* tcpIOControl */
#define ZIS_STUB_UDPPEER  215 /* udpPeer */
#define ZIS_STUB_UDPRECVF 216 /* udpReceiveFrom */
#define ZIS_STUB_UDPSENDT 217 /* udpSendTo */
#define ZIS_STUB_MAKSOCST 218 /* makeSocketSet */
#define ZIS_STUB_FRESOCST 219 /* freeSocketSet */
#define ZIS_STUB_SOCSTADD 220 /* socketSetAdd */
#define ZIS_STUB_SOCSTREM 221 /* socketSetRemove */
#define ZIS_STUB_TCPSTTUS 222 /* tcpStatus */
#define ZIS_STUB_SOCREAD  223 /* socketRead */
#define ZIS_STUB_SOCWRITE 224 /* socketWrite */
#define ZIS_STUB_SETSKTTO 225 /* setSocketTimeout */
#define ZIS_STUB_SETSKTND 226 /* setSocketNoDelay */
#define ZIS_STUB_SETSKTWB 227 /* setSocketWriteBufferSize */
#define ZIS_STUB_SETSKTRB 228 /* setSocketReadBufferSize */
#define ZIS_STUB_SETSOCBM 229 /* setSocketBlockingMode */
#define ZIS_STUB_SETSKTOP 230 /* setSocketOption */
/* #define ZIS_STUB_SOCKSEND 231 socketSend - no metal impl */
#define ZIS_STUB_SOCACCPT 232 /* socketAccept */
#define ZIS_STUB_SOCCLOSE 233 /* socketClose */
#define ZIS_STUB_EXSELECT 234 /* extendedSelect */
#define ZIS_STUB_MACSOCAD 235 /* makeSocketAddr */
#define ZIS_STUB_MACSOCA6 236 /* makeSocketAddrIPv6 */
#define ZIS_STUB_FRESOCAD 237 /* freeSocketAddr */
#define ZIS_STUB_SOCFREE  238 /* socketFree */

/* cellpool, 250-259 */
#define ZIS_STUB_CPASIZE  250 /* cellpoolGetDWordAlignedSize */
#define ZIS_STUB_CPFREE   251 /* cellpoolFree */
#define ZIS_STUB_CPGET    252 /* cellpoolGet */
#define ZIS_STUB_CPDELETE 253 /* cellpoolDelete */
#define ZIS_STUB_CPBUILD  254 /* cellpoolBuild */

/* cmutils, 260-289 */
#define ZIS_STUB_CMCPWDK  260 /* cmCopyWithDestinationKey */
#define ZIS_STUB_CMCPWSK  261 /* cmCopyWithSourceKey */
#define ZIS_STUB_CMCPTSSK 262 /* cmCopyToSecondaryWithCallerKey */
#define ZIS_STUB_CMCPFSSK 263 /* cmCopyFromSecondaryWithCallerKey */
#define ZIS_STUB_CMCPTPSK 264 /* cmCopyToPrimaryWithCallerKey */
#define ZIS_STUB_CMCPFPSK 265 /* cmCopyFromPrimaryWithCallerKey */
#define ZIS_STUB_CMCPTHSK 266 /* cmCopyToHomeWithCallerKey */
#define ZIS_STUB_CMCPFHSK 267 /* cmCopyFromHomeWithCallerKey */
#define ZIS_STUB_CMCPYSKA 268 /* cmCopyWithSourceKeyAndALET */
#define ZIS_STUB_CMGAACEE 269 /* cmGetCallerAddressSpaceACEE */
#define ZIS_STUB_CMGTACEE 270 /* cmGetCallerTaskACEE */
#define ZIS_STUB_CMALLOC  271 /* cmAlloc */
#define ZIS_STUB_CMFREE   272 /* cmFree */
#define ZIS_STUB_CMALLOC2 273 /* cmAlloc2 */
#define ZIS_STUB_CMFREE2  274 /* cmFree2 */
#define ZIS_STUB_CMUMKMAP 275 /* makeCrossMemoryMap */
#define ZIS_STUB_CMURMMAP 276 /* removeCrossMemoryMap */
#define ZIS_STUB_CMUMAPP  277 /* crossMemoryMapPut */
#define ZIS_STUB_CMUMAPGH 278 /* crossMemoryMapGetHandle */
#define ZIS_STUB_CMUMAPGT 279 /* crossMemoryMapGet */
#define ZIS_STUB_CMUMAPIT 280 /* crossMemoryMapIterate */

/* collections, 290-349 */
#define ZIS_STUB_FBMGRCRT 290 /* fbMgrCreate */
#define ZIS_STUB_FBMGRALC 291 /* fbMgrAlloc */
#define ZIS_STUB_FBMGRFRE 292 /* fbMgrFree */
#define ZIS_STUB_FBMGRDST 293 /* fbMgrDestroy */
#define ZIS_STUB_HTCREATE 294 /* htCreate */
#define ZIS_STUB_HTALTER  295 /* htAlter */
#define ZIS_STUB_HTGET    296 /* htGet */
#define ZIS_STUB_HTINTGET 297 /* htIntGet */
#define ZIS_STUB_HTPUT    298 /* htPut */
#define ZIS_STUB_HTINTPUT 299 /* htIntPut */
#define ZIS_STUB_HTUINTPT 300 /* htUIntPut */
#define ZIS_STUB_HTUINTGT 301 /* htUIntGet */
#define ZIS_STUB_HTREMOVE 302 /* htRemove */
#define ZIS_STUB_HTMAP    303 /* htMap */
#define ZIS_STUB_HTMAP2   304 /* htMap2 */
#define ZIS_STUB_HTPRUNE  305 /* htPrune */
#define ZIS_STUB_HTDSTROY 306 /* htDestroy */
#define ZIS_STUB_HTCOUNT  307 /* htCount */
#define ZIS_STUB_STRNGHSH 308 /* stringHash */
#define ZIS_STUB_STRNGCMP 309 /* stringCompare */
#define ZIS_STUB_MKLRUCHE 310 /* makeLRUCache */
#define ZIS_STUB_DSLRUCHE 311 /* destroyLRUCache */
#define ZIS_STUB_LRUGET   312 /* lruGet */
#define ZIS_STUB_LRUSTORE 313 /* lruStore */
#define ZIS_STUB_LNHTCRTE 314 /* lhtCreate */
#define ZIS_STUB_LNHTALTR 315 /* lhtAlter */
#define ZIS_STUB_LNHDSTRY 316 /* lhtDestroy */
#define ZIS_STUB_LNHTGET  317 /* lhtGet */
#define ZIS_STUB_LNHTPUT  318 /* lhtPut */
#define ZIS_STUB_LNHTREMV 319 /* lhtRemove */
#define ZIS_STUB_LNHTMAP  320 /* lhtMap */
#define ZIS_STUB_MAKELCFQ 321 /* makeQueue */
#define ZIS_STUB_DSTRLCFQ 322 /* destroyQueue */
#define ZIS_STUB_QENQUEUE 323 /* qEnqueue */
#define ZIS_STUB_QDEQUEUE 324 /* qDequeue */
#define ZIS_STUB_QINSERT  325 /* qInsert */
#define ZIS_STUB_QREMOVE  326 /* qRemove */
#define ZIS_STUB_ALSTMAKE 327 /* makeArrayList */
#define ZIS_STUB_ALSTFREE 328 /* arrayListFree */
#define ZIS_STUB_ALSTADD  329 /* arrayListAdd */
#define ZIS_STUB_ALSTELMT 330 /* arrayListElement */
#define ZIS_STUB_ALINEMAR 331 /* initEmbeddedArrayList */
#define ZIS_STUB_ALSTSORT 332 /* arrayListSort */
#define ZIS_STUB_ALSHLCPY 333 /* arrayListShallowCopy */

/* cpool64, 350-359 */
#define ZIS_STUB_CPL64CRE 350 /* iarcp64Create */
#define ZIS_STUB_CPL64GET 351 /* iarcp64Get    */
#define ZIS_STUB_CPL64FRE 352 /* iarcp64Free   */
#define ZIS_STUB_CPL64DEL 353 /* iarcp64Delete */

/* crossmemory, 360-399 */
/* #define ZIS_STUB_CMINILOG 360 cmsInitializeLogging - not in CMS_CLIENT */
/* #define ZIS_STUB_CMMCMSRV 361 makeCrossMemoryServer - not in CMS_CLIENT */
/* #define ZIS_STUB_CMMCMSR2 362 makeCrossMemoryServer2  - not in CMS_CLIENT */
/* #define ZIS_STUB_CMMCRSRV 363 removeCrossMemoryServer - not in CMS_CLIENT */
/* #define ZIS_STUB_CMCMSSPP 364 cmsSetPoolParameters - not in CMS_CLIENT */
/* #define ZIS_STUB_CMCMSRSR 365 cmsRegisterService - not in CMS_CLIENT */
/* #define ZIS_STUB_CMCMAINL 366 cmsStartMainLoop - not in CMS_CLIENT */
#define ZIS_STUB_CMGETGA  367 /* cmsGetGlobalArea */
/* #define ZIS_STUB_CMADDPRM 368 cmsAddConfigParm - not in CMS_CLIENT */
#define ZIS_STUB_CMTSAUTH 369 /* cmsTestAuth */
#define ZIS_STUB_CMCMSRCS 370 /* cmsCallService */
#define ZIS_STUB_CMCALLS2 371 /* cmsCallService2 */
#define ZIS_STUB_CMCALLS3 372 /* cmsCallService3 */
#define ZIS_STUB_CMCMSPRF 373 /* cmsPrintf */
#define ZIS_STUB_CMCMSVPF 374 /* vcmsPrintf */
#define ZIS_STUB_CMHEXDMP 375 /* cmsHexDump */
#define ZIS_STUB_CMGETPRM 376 /* cmsGetConfigParm */
#define ZIS_STUB_CMGETPRU 377 /* cmsGetConfigParmUnchecked */
#define ZIS_STUB_CMGETLOG 378 /* cmsGetPCLogLevel */
#define ZIS_STUB_CMGETSTS 379 /* cmsGetStatus */
#define ZIS_STUB_CMMKSNAM 380 /* cmsMakeServerName */
/* #define ZIS_STUB_CMECSAAL 381 cmsAllocateECSAStorage - not in CMS_CLIENT */
/* #define ZIS_STUB_CMECSAFR 382 cmsFreeECSAStorage - not in CMS_CLIENT */
/* #define ZIS_STUB_CMECSAA2 383 cmsAllocateECSAStorage2 - not in CMS_CLIENT */
/* #define ZIS_STUB_CMECSAF2 384 cmsFreeECSAStorage2 - not in CMS_CLIENT */
#define ZIS_STUB_CMGETPRX 385 /* cmsGetConfigParmExt */
#define ZIS_STUB_CMGETPUX 386 /* cmsGetConfigParmExtUnchecked */

/* dynalloc, 400-429 */
#define ZIS_STUB_DYNASTXU 400 /* createSimpleTextUnit mapped */
#define ZIS_STUB_DYNASTX2 401 /* createSimpleTextUnit2 mapped */
#define ZIS_STUB_DYNACTXU 402 /* createCharTextUnit mapped */
#define ZIS_STUB_DYNACMTU 403 /* createCompoundTextUnit mapped */
#define ZIS_STUB_DYNAITXU 404 /* createIntTextUnit mapped */
#define ZIS_STUB_DYNAI1TU 405 /* createInt8TextUnit mapped */
#define ZIS_STUB_DYNAI2TU 406 /* createInt16TextUnit mapped */
#define ZIS_STUB_DYNAFTXU 407 /* freeTextUnit mapped */
#define ZIS_STUB_DYNAAIRD 408 /* AllocIntReader mapped */
#define ZIS_STUB_DYNASLIB 409 /* dynallocSharedLibrary mapped */
#define ZIS_STUB_DYNAUSSD 410 /* dynallocUSSDirectory mapped */
#define ZIS_STUB_DYNAUSSO 411 /* dynallocUSSOutput mapped */
#define ZIS_STUB_DYNASAPI 412 /* AllocForSAPI mapped */
#define ZIS_STUB_DYNADOUT 413 /* AllocForDynamicOutput mapped */
#define ZIS_STUB_DYNADDDN 414 /* DeallocDDName mapped */
#define ZIS_STUB_DYNAUALC 415 /* dynallocDataset mapped */
#define ZIS_STUB_DYNAUALM 416 /* dynallocDatasetMember mapped */
#define ZIS_STUB_DYNADALC 417 /* unallocDataset mapped */
#define ZIS_STUB_DYNALAD2 418 /* dynallocAllocDataset */
#define ZIS_STUB_DYNALUD2 419 /* dynallocUnallocDatasetByDDName */

/* idcams, 430-439 */
#define ZIS_STUB_RSIDCCCM 430 /* idcamsCreateCommand mapped */
#define ZIS_STUB_RSIDCALN 431 /* idcamsAddLineToCommand mapped */
#define ZIS_STUB_RSIDCEXE 432 /* idcamsExecuteCommand mapped */
#define ZIS_STUB_RSIDCDCM 433 /* idcamsDeleteCommand mapped */
#define ZIS_STUB_RSIDCPCO 434 /* idcamsPrintCommandOutput mapped */
#define ZIS_STUB_RSIDCDCO 435 /* idcamsDeleteCommandOutput mapped */

/* isgenq, 440-449 */
#define ZIS_STUB_ENQTRYX  440 /* isgenqTryExclusiveLock mapped */
#define ZIS_STUB_ENQGETX  441 /* isgenqGetExclusiveLock mapped */
#define ZIS_STUB_ENQTRYS  442 /* isgenqTrySharedLock mapped */
#define ZIS_STUB_ENQGETS  443 /* isgenqGetSharedLock mapped */
#define ZIS_STUB_ENQTSTX  444 /* isgenqTestExclusiveLock mapped */
#define ZIS_STUB_ENQTSTS  445 /* isgenqTestSharedLock mapped */
#define ZIS_STUB_ENQTEST  446 /* isgenqTestLock mapped */
#define ZIS_STUB_ENQREL   447 /* isgenqReleaseLock mapped */

/* le, 450-469 */
#define ZIS_STUB_LEGETCAA 450 /* getCAA */
#define ZIS_STUB_LESHWRTL 451 /* showRTL */
#define ZIS_STUB_LEMKRLEA 452 /* makeRLEAnchor */
#define ZIS_STUB_LEDLRLEA 453 /* deleteRLEAnchor */
#define ZIS_STUB_LEMKRLET 454 /* makeRLETask */
#define ZIS_STUB_LEDLRLET 455 /* deleteRLETask */
#define ZIS_STUB_LEINRLEE 456 /* initRLEEnvironment */
#define ZIS_STUB_LETMRLEE 457 /* termRLEEnvironment */
#define ZIS_STUB_LEMKFCAA 458 /* makeFakeCAA */
#define ZIS_STUB_LEARTCAA 459 /* abortIfUnsupportedCAA */
#define ZIS_STUB_LESETANC 460 /* setRLEApplicationAnchor */
#define ZIS_STUB_LEGETANC 461 /* getRLEApplicationAnchor */

/* logging, 470-499 */
#define ZIS_STUB_MKLOGCTX 470 /* makeLoggingContext */
#define ZIS_STUB_MKLLGCTX 471 /* makeLocalLoggingContext */
#define ZIS_STUB_RMLOGCTX 472 /* removeLoggingContext */
#define ZIS_STUB_RMLLGCTX 473 /* removeLocalLoggingContext */
#define ZIS_STUB_GTLOGCTX 474 /* getLoggingContext */
#define ZIS_STUB_STLOGCTX 475 /* setLoggingContext */
#define ZIS_STUB_ZOWELOG  476 /* zowelog */
#define ZIS_STUB_ZOWEDUMP 477 /* zowedump */
#define ZIS_STUB_LGCFGDST 478 /* logConfigureDestination */
#define ZIS_STUB_LGCFGDS2 479 /* logConfigureDestination2 */
#define ZIS_STUB_LGCFGSTD 480 /* logConfigureStandardDestinations */
#define ZIS_STUB_LGCFGCMP 481 /* logConfigureComponent */
#define ZIS_STUB_LGSETLVL 482 /* logSetLevel */
#define ZIS_STUB_LGGETLVL 483 /* logGetLevel */
#define ZIS_STUB_LGSHTRCE 484 /* logShouldTraceInternal */
/* #define ZIS_STUB_LGGLOGCX 485 logGetExternalContext - provider stub */
/* #define ZIS_STUB_LGSLOGCX 486 logSetExternalContext - provider stub */
#define ZIS_STUB_LGPRSOUT 487 /* printStdout */
#define ZIS_STUB_LGPRSERR 488 /* printStderr */

/* lpa, 500-509 */
#define ZIS_STUB_LPAADD   500 /* lpaAdd mapped */
#define ZIS_STUB_LPADEL   501 /* lpaDelete mapped */

/* metalio, 510-529 */
/* #define ZIS_STUB_GTNMTKFT 510 getNameTokenFunctionTable - defunct */
#define ZIS_STUB_GTNMTKVL 511 /* getNameTokenValue */
#define ZIS_STUB_CRNMTKPR 512 /* createNameTokenPair */
#define ZIS_STUB_DLNMTKPR 513 /* deleteNameTokenPair */
/* #define ZIS_STUB_INITSYTS 514 initSysouts - not used */
#define ZIS_STUB_GTSYSOUT 515 /* getSYSOUTStruct */
#define ZIS_STUB_WTOPRNF  516 /* wtoPrintf */
#define ZIS_STUB_AWTOPRNF 517 /* authWTOPrintf */
#define ZIS_STUB_SENDWTO  518 /* sendWTO */
#define ZIS_STUB_QSAMPRNF 519 /* qsamPrintf */
#define ZIS_STUB_PRINTF   520 /* printf */
#define ZIS_STUB_FFLUSH   521 /* fflush */
#define ZIS_STUB_SETJMP   522 /* setjmp */
#define ZIS_STUB_LONGJMP  523 /* longjmp */

/* nametoken, 530-539 */
#define ZIS_STUB_NTPCREF  530 /* nameTokenCreate */
#define ZIS_STUB_NTPCRTPF 531 /* nameTokenCreatePersistent */
#define ZIS_STUB_NTPDELF  532 /* nameTokenDelete */
#define ZIS_STUB_NTPRETF  533 /* nameTokenRetrieve */

/* pause-element, 540-549 */
#define ZIS_STUB_PEALLOC  540 /* peAlloc    */
#define ZIS_STUB_PEDALLOC 541 /* peDealloc  */
#define ZIS_STUB_PEPAUSE  542 /* pePause    */
#define ZIS_STUB_PERELEAS 543 /* peRelease  */
#define ZIS_STUB_PERTVNFO 544 /* peRetrieveInfo  */
#define ZIS_STUB_PERTVNF2 545 /* peRetrieveInfo2 */
#define ZIS_STUB_PETEST   546 /* peTest          */
#define ZIS_STUB_PETRNSFR 547 /* peTransfer      */

/* pc, 550-569 */
#define ZIS_STUB_PCAXSET  550 /* pcSetAllAddressSpaceAuthority */
#define ZIS_STUB_PCLXRES  551 /* pcReserveLinkageIndex */
#define ZIS_STUB_PCLXFRE  552 /* pcFreeLinkageIndex */
#define ZIS_STUB_PCETCRED 553 /* pcMakeEntryTableDescriptor */
#define ZIS_STUB_PCETADD  554 /* pcAddToEntryTableDescriptor */
#define ZIS_STUB_PCETREMD 555 /* pcRemoveEntryTableDescriptor */
#define ZIS_STUB_PCETCRE  556 /* pcCreateEntryTable */
#define ZIS_STUB_PCETDES  557 /* pcDestroyEntryTable */
#define ZIS_STUB_PCETCON  558 /* pcConnectEntryTable */
#define ZIS_STUB_PCETDIS  559 /* pcDisconnectEntryTable */
#define ZIS_STUB_PCCALLR  560 /* pcCallRoutine */

/* qsam, 570-599 */
#define ZIS_STUB_MALLOC24 570 /* malloc24 */
#define ZIS_STUB_OPENSAM  571 /* openSAM */
#define ZIS_STUB_GTSAMLN  572 /* getSAMLength */
#define ZIS_STUB_GTSAMBS  573 /* getSAMBlksize */
#define ZIS_STUB_GTSAMLR  574 /* getSAMLrecl */
#define ZIS_STUB_GTSAMCC  575 /* getSAMCC */
#define ZIS_STUB_GTSAMRF  576 /* getSAMRecfm */
#define ZIS_STUB_PUTLINE  577 /* putline */
#define ZIS_STUB_GETLINE  578 /* getline */
#define ZIS_STUB_GTEODBUF 579 /* getEODADBuffer */
#define ZIS_STUB_FREODBUF 580 /* freeEODADBuffer */
#define ZIS_STUB_GETLINE2 581 /* getline2 */
#define ZIS_STUB_MKQSMBUF 582 /* makeQSAMBuffer */
#define ZIS_STUB_FRQSMBUF 583 /* freeQSAMBuffer */
#define ZIS_STUB_PUTLNEV  584 /* putlineV */
#define ZIS_STUB_GETLNEV  585 /* getlineV */
#define ZIS_STUB_CLOSESAM 586 /* closeSAM */
#define ZIS_STUB_OPENEXCP 587 /* openEXCP */
#define ZIS_STUB_CLOSEXCP 588 /* closeEXCP */
#define ZIS_STUB_HASVRLEN 589 /* hasVaryingRecordLength */
#define ZIS_STUB_BPAMDELM 590 /* bpamDeleteMember */
#define ZIS_STUB_BPAMFIND 591 /* bpamFind */
#define ZIS_STUB_BPAMREAD 592 /* bpamRead */
#define ZIS_STUB_BPAMRD2  593 /* bpamRead2 */

/* radmin, 600-619 */
#define ZIS_STUB_RADMXPRF 600 /* radminExtractProfiles mapped */
#define ZIS_STUB_RADMXBUP 601 /* radminExtractBasicUserProfileInfo mapped */
#define ZIS_STUB_RADMXBRP 602 /* radminExtractBasicGenresProfileInfo mapped */
#define ZIS_STUB_RADMXBGP 603 /* radminExtractBasicGroupProfileInfo mapped */
#define ZIS_STUB_RADMXPAL 604 /* radminExtractGenresAccessList mapped */
#define ZIS_STUB_RADMXGAL 605 /* radminExtractGroupAccessList mapped */
#define ZIS_STUB_RADMPREA 606 /* radminPerformResAction mapped */
#define ZIS_STUB_RADMPGRA 607 /* radminPerformGroupAction mapped */
#define ZIS_STUB_RADMPCNA 608 /* radminPerformConnectionAction mapped */
#define ZIS_STUB_RADMRCMD 609 /* radminRunRACFCommand mapped */

/* recovery, 620-639 */
#define ZIS_STUB_RCVRESRR 620 /* recoveryEstablishRouter */
#define ZIS_STUB_RCVRRSRR 621 /* recoveryRemoveRouter */
#define ZIS_STUB_RCVRTRES 622 /* recoveryIsRouterEstablished */
#define ZIS_STUB_RCVRPSFR 623 /* recoveryPush */
#define ZIS_STUB_RCVRPPFR 624 /* recoveryPop */
#define ZIS_STUB_RCVRSDTL 625 /* recoverySetDumpTitle */
#define ZIS_STUB_RCVRSFLV 626 /* recoverySetFlagValue */
#define ZIS_STUB_RCVRECST 627 /* recoveryEnableCurrentState */
#define ZIS_STUB_RCVRDCST 628 /* recoveryDisableCurrentState */
#define ZIS_STUB_RCVRURSI 629 /* recoveryUpdateRouterServiceInfo */
#define ZIS_STUB_RCVRUSSI 630 /* recoveryUpdateStateServiceInfo */
#define ZIS_STUB_RCVRGACD 631 /* recoveryGetABENDCode */
/* #define ZIS_STUB_RCVRNFNE 632 - defunct */
/* #define ZIS_STUB_SHRCVRST 633 - defunct */

/* resmgr, 640-649 */
#define ZIS_STUB_RMGRATRM 640 /* resmgrAddTaskResourceManager */
#define ZIS_STUB_RMGRAARM 641 /* resmgrAddAddressSpaceResourceManager */
#define ZIS_STUB_RMGRDTRM 642 /* resmgrDeleteTaskResourceManager */
#define ZIS_STUB_RMGRDARM 643 /* resmgrDeleteAddressSpaceResourceManager */

/* scheduling, 650-659 */
#define ZIS_STUB_SCHZOSWT 650 /* zosWait */
#define ZIS_STUB_SCHZOSWL 651 /* zosWaitList */
#define ZIS_STUB_SCHZOSPT 652 /* zosPost */
#define ZIS_STUB_SCHSRLET 653 /* startRLETask */

/* shrmem64, 660-679 */
#define ZIS_STUB_SHR64TKN 660 /* shrmem64GetAddressSpaceToken */
#define ZIS_STUB_SHR64ALC 661 /* shrmem64Alloc */
#define ZIS_STUB_SHR64AL2 662 /* shrmem64Alloc2 */
#define ZIS_STUB_SHR64CAL 663 /* shrmem64CommonAlloc */
#define ZIS_STUB_SHR64CA2 664 /* shrmem64CommonAlloc2 */
#define ZIS_STUB_SHR64REL 665 /* shrmem64Release */
#define ZIS_STUB_SHR64REA 666 /* shrmem64ReleaseAll */
#define ZIS_STUB_SHR64GAC 667 /* shrmem64GetAccess */
#define ZIS_STUB_SHR64GA2 668 /* shrmem64GetAccess2 */
#define ZIS_STUB_SHR64RAC 669 /* shrmem64RemoveAccess */
#define ZIS_STUB_SHR64RA2 670 /* shrmem64RemoveAccess2 */

/* timeutls, 680-699 */
#define ZIS_STUB_GETSTCK  680 /* getSTCK */
#define ZIS_STUB_GETSTCKU 681 /* getSTCKU */
#define ZIS_STUB_CONVTODL 682 /* convertTODToLocal */
#define ZIS_STUB_TZDIFFOR 683 /* timeZoneDifferenceFor */
#define ZIS_STUB_STCKCONV 684 /* stckToTimestamp */
#define ZIS_STUB_CONVTOD  685 /* timestampToSTCK */
#define ZIS_STUB_MIDNIGHT 686 /* timeFromMidnight */
#define ZIS_STUB_STCKYYMD 687 /* stckFromYYYYMMDD */
#define ZIS_STUB_ELPSTIME 688 /* elapsedTime */
#define ZIS_STUB_STCKUNIX 689 /* stckToUnix */
#define ZIS_STUB_STCK2USM 690 /* stckToUnixSecondsAndMicros */
#define ZIS_STUB_CONVUNIX 691 /* unixToTimestamp */
#define ZIS_STUB_GTDAYMNT 692 /* getDayAndMonth */
#define ZIS_STUB_SNPRNTLT 693 /* snprintLocalTime */

/* utils, 700-799 */
#define ZIS_STUB_STRCPSAF 700 /* strcopy_safe */
#define ZIS_STUB_INDEXOF  701 /* indexOf */
#define ZIS_STUB_IDXSTR   702 /* indexOfString */
#define ZIS_STUB_LIDXSTR  703 /* lastIndexOfString */
#define ZIS_STUB_LINDEXOF 704 /* lastIndexOf */
#define ZIS_STUB_IDXSTRNS 705 /* indexOfStringInsensitive */
#define ZIS_STUB_ISZEROS  706 /* isZeros */
#define ZIS_STUB_ISBLANKS 707 /* isBlanks */
#define ZIS_STUB_HASTEXT  708 /* hasText */
#define ZIS_STUB_PARSEINT 709 /* parseInt */
#define ZIS_STUB_PSINTINT 710 /* parseInitialInt */
#define ZIS_STUB_NULLTERM 711 /* nullTerminate */
#define ZIS_STUB_ISCHARAN 712 /* isCharAN */
#define ZIS_STUB_TKGTDCML 713 /* tknGetDecimal */
#define ZIS_STUB_TKGTQOTD 714 /* tknGetQuoted */
#define ZIS_STUB_TKGTANUM 715 /* tknGetAlphanumeric */
#define ZIS_STUB_TKGTSTND 716 /* tknGetStandard */
#define ZIS_STUB_TKGTNWSP 717 /* tknGetNonWhitespace */
#define ZIS_STUB_TKGTTERM 718 /* tknGetTerminating */
#define ZIS_STUB_TKTXEQLS 719 /* tknTextEquals */
#define ZIS_STUB_FREETKN  720 /* freeToken */
#define ZIS_STUB_TKNTEXT  721 /* tknText */
#define ZIS_STUB_TKNINT   722 /* tknInt */
#define ZIS_STUB_TKNLNGTH 723 /* tknLength */
#define ZIS_STUB_DMPBUFFR 724 /* dumpbuffer */
#define ZIS_STUB_DMPBUFFA 725 /* dumpbufferA */
#define ZIS_STUB_HEXFILL  726 /* hexFill */
#define ZIS_STUB_SMPHXFIL 727 /* simpleHexFill */
#define ZIS_STUB_SMPHXPRT 728 /* simpleHexPrint */
#define ZIS_STUB_SMPHXPRL 729 /* simpleHexPrintLower */
#define ZIS_STUB_HEXDUMP  730 /* hexdump */
#define ZIS_STUB_DMPBFFR2 731 /* dumpbuffer2 */
#define ZIS_STUB_DMPBFFRS 732 /* dumpBufferToStream */
#define ZIS_STUB_CMPIGNCS 733 /* compareIgnoringCase */
#define ZIS_STUB_STRUPCAS 734 /* strupcase */
#define ZIS_STUB_MAKESLH  735 /* makeShortLivedHeap */
#define ZIS_STUB_MAKSLH64 736 /* makeShortLivedHeap64 */
#define ZIS_STUB_SLHALLOC 737 /* SLHAlloc */
#define ZIS_STUB_SLHFREE  738 /* SLHFree */
#define ZIS_STUB_NYMALLOC 739 /* noisyMalloc */
#define ZIS_STUB_DECODB32 740 /* base32Encode */
#define ZIS_STUB_ENCODB32 741 /* base32Decode */
#define ZIS_STUB_DECODB64 742 /* decodeBase64 */
#define ZIS_STUB_DECDB64U 743 /* decodeBase64Unterminated */
#define ZIS_STUB_ENCODB64 744 /* encodeBase64 */
#define ZIS_STUB_ENCDB64N 745 /* encodeBase64NoAlloc */
#define ZIS_STUB_B642BURL 746 /* base64ToBase64url */
#define ZIS_STUB_B64URLTB 747 /* base64urlToBase64 */
#define ZIS_STUB_CLNURLPV 748 /* cleanURLParamValue */
#define ZIS_STUB_PCTENCOD 749 /* percentEncode */
#define ZIS_STUB_DSTUNASC 750 /* destructivelyUnasciify */
#define ZIS_STUB_MAKESLST 751 /* makeStringList */
#define ZIS_STUB_SLSTLEN  752 /* stringListLength */
#define ZIS_STUB_SLSTPRNT 753 /* stringListPrint */
#define ZIS_STUB_SLSTCTNS 754 /* stringListContains */
#define ZIS_STUB_SLSTLAST 755 /* stringListLast */
#define ZIS_STUB_ADSLSTUQ 756 /* addToStringListUnique */
#define ZIS_STUB_ADSLST   757 /* addToStringList */
#define ZIS_STUB_SLSTELT1 758 /* firstStringListElt */
#define ZIS_STUB_STRCNCAT 759 /* stringConcatenate */
#define ZIS_STUB_MKBFCHST 760 /* makeBufferCharStream */
#define ZIS_STUB_CSTRPOSN 761 /* charStreamPosition */
#define ZIS_STUB_CSTRGET  762 /* charStreamGet */
#define ZIS_STUB_CSTREOF  763 /* charStreamEOF */
#define ZIS_STUB_CSTRCLOS 764 /* charStreamClose */
#define ZIS_STUB_CSTRFREE 765 /* charStreamFree */
#define ZIS_STUB_PADWSPCS 766 /* padWithSpaces */
#define ZIS_STUB_RPLTRMNL 767 /* replaceTerminateNulls */
#define ZIS_STUB_CNVINTST 768 /* convertIntToString */
#define ZIS_STUB_HEXTODEC 769 /* hexToDec */
#define ZIS_STUB_DECTOHEX 770 /* decToHex */
#define ZIS_STUB_COMPSEQS 771 /* compareSequences */
#define ZIS_STUB_DEC2OCT  772 /* decimalToOctal */
#define ZIS_STUB_UNX2ISO  773 /* convertUnixToISO */
#define ZIS_STUB_MATCHWLD 774 /* matchWithWildcards */
#define ZIS_STUB_STRISDIG 775 /* stringIsDigit */
#define ZIS_STUB_TRMRIGHT 776 /* trimRight */
#define ZIS_STUB_ISPASPHR 777 /* isPassPhrase */

/* vsam, 800-829 */
#define ZIS_STUB_MAKEACB  800 /* makeACB */
#define ZIS_STUB_VOPCLACB 801 /* opencloseACB */
#define ZIS_STUB_OPENACB  802 /* openACB */
#define ZIS_STUB_CLOSEACB 803 /* closeACB */
#define ZIS_STUB_MODRPL   804 /* modRPL */
#define ZIS_STUB_VSPOINT  805 /* point */
#define ZIS_STUB_VPUTREC  806 /* putRecord */
#define ZIS_STUB_VGETREC  807 /* getRecord */
/* #define ZIS_STUB_VMKDBUFF 808 makeDataBuffer - no body */
/* #define ZIS_STUB_VFRDBUFF 809 freeDataBuffer - no body */
#define ZIS_STUB_VPNBYRBA 810 /* pointByRBA */
#define ZIS_STUB_VPNBYKEY 811 /* pointByKey */
#define ZIS_STUB_VPNBYREC 812 /* pointByRecord */
#define ZIS_STUB_VPNBYCI  813 /* pointByCI */
#define ZIS_STUB_VALLOCDS 814 /* allocateDataset */
#define ZIS_STUB_VDEFAIX  815 /* defineAIX */
#define ZIS_STUB_DELCLUST 816 /* deleteCluster */

/* xlate, 830-839 */
#define ZIS_STUB_E2A      830 /* e2a */
#define ZIS_STUB_A2E      831 /* a2e */

/* zos, 840-899 */
#define ZIS_STUB_EXTRPSW  840 /* extractPSW */
#define ZIS_STUB_SUPRMODE 841 /* supervisorMode */
#define ZIS_STUB_SETKEY   842 /* setKey */
#define ZIS_STUB_DDEXISTS 843 /* ddnameExists */
#define ZIS_STUB_ATOMINCR 844 /* atomicIncrement */
#define ZIS_STUB_GTCRACEE 845 /* getCurrentACEE */
#define ZIS_STUB_GTFCHTCB 846 /* getFirstChildTCB */
#define ZIS_STUB_GTPRTTCB 847 /* getParentTCB */
#define ZIS_STUB_GTNXSTCB 848 /* getNextSiblingTCB */
#define ZIS_STUB_GETCVT   849 /* getCVT */
#define ZIS_STUB_GETATCVT 850 /* getATCVT */
#define ZIS_STUB_GETCSTBL 851 /* getIEACSTBL */
#define ZIS_STUB_GETCVTPR 852 /* getCVTPrefix */
#define ZIS_STUB_GETECVT  853 /* getECVT */
#define ZIS_STUB_GETTCB   854 /* getTCB */
#define ZIS_STUB_GETSTCB  855 /* getSTCB */
#define ZIS_STUB_GETOTCB  856 /* getOTCB */
#define ZIS_STUB_GETASCB  857 /* getASCB */
#define ZIS_STUB_GETASXB  858 /* getASXB */
#define ZIS_STUB_GETASSB  859 /* getASSB */
#define ZIS_STUB_GETJSAB  860 /* getJSAB */
#define ZIS_STUB_GTSPLXNM 861 /* getSysplexName */
#define ZIS_STUB_GTSYSTNM 862 /* getSystemName */
#define ZIS_STUB_GETDSAB  863 /* getDSAB */
#define ZIS_STUB_DSABOMVS 864 /* dsabIsOMVS */
#define ZIS_STUB_LOCATE   865 /* locate */
#define ZIS_STUB_GETR13   866 /* getR13 */
#define ZIS_STUB_GETR12   867 /* getR12 */
#define ZIS_STUB_GETASCBJ 868 /* getASCBJobname */
#define ZIS_STUB_LOADBYNA 869 /* loadByName */
#define ZIS_STUB_LOADBNML 870 /* loadByNameLocally */
#define ZIS_STUB_ZOSCLCKD 871 /* isCallerLocked */
#define ZIS_STUB_ZOSCSRB  872 /* isCallerSRB */
#define ZIS_STUB_ZOSCXMEM 873 /* isCallerCrossMemory */
#define ZIS_STUB_GADSACEE 874 /* getAddressSpaceAcee */
#define ZIS_STUB_GTSKACEE 875 /* getTaskAcee */
#define ZIS_STUB_STSKACEE 876 /* setTaskAcee */
#define ZIS_STUB_SAFVRIFY 877 /* safVerify */
#define ZIS_STUB_SAFVRFY2 878 /* safVerify2 */
#define ZIS_STUB_SAFVRFY3 879 /* safVerify3 */
#define ZIS_STUB_SAFVRFY4 880 /* safVerify4 */
#define ZIS_STUB_SAFVRFY5 881 /* safVerify5 */
#define ZIS_STUB_SAFVRFY6 882 /* safVerify6 */
#define ZIS_STUB_SAFVRFY7 883 /* safVerify7 */
#define ZIS_STUB_SAFAUTH  884 /* safAuth */
#define ZIS_STUB_SAFAUTHS 885 /* safAuthStatus */
#define ZIS_STUB_SAFSTAT  886 /* safStat */
/* #define ZIS_STUB_GETSAFPL 887 getSafProfileMaxLen - no body */

/* zvt, 900-909 */
#define ZIS_STUB_ZVTINIT  900 /* zvtInit mapped */
#define ZIS_STUB_ZVTGET   901 /* zvtGet mapped */
#define ZIS_STUB_ZVTAENTR 902 /* zvtAllocEntry mapped */
#define ZIS_STUB_ZVTFENTR 903 /* zvtFreeEntry mapped */
#define ZIS_STUB_ZVTGXMLR 904 /* zvtGetCMSLookupRoutineAnchor mapped */
#define ZIS_STUB_ZVTSXMLR 905 /* zvtSetCMSLookupRoutineAnchor mapped */

/* modreg, 915-920 */
#define ZIS_STUB_MODRRGST 915 /* modregRegister mapped */

#endif /* ZIS_ZISSTUBS_H_ */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
