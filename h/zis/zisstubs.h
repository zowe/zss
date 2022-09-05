
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
#define ZIS_STUB_DYNZISUD 0 /* zisdynUndefinedStub  */
#define ZIS_STUB_ZISDYNSV 1 /* zisdynGetStubVersion mapped */
#define ZIS_STUB_ZISDYNPV 2 /* zisdynGetPluginVersion mapped */
#define ZIS_STUB_ZISGVRSN 3 /* zisGetServerVersion mapped */
#define ZIS_STUB_ZISLPADV 4 /* zisIsLPADevModeOn mapped */

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
#define ZIS_STUB_GETSOCDI 201 /* getSocketDebugID */
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
#define ZIS_STUB_MAKSPSOC 212 /* makePipeBasedSyntheticSocket */
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
#define ZIS_STUB_SOCKSEND 231 /* socketSend */
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

/* crossmemory, 360-379 */
#define ZIS_STUB_CMGETGA  360 /* cmsGetGlobalArea */
#define ZIS_STUB_CMTSAUTH 361 /* cmsTestAuth */
#define ZIS_STUB_CMCMSRCS 362 /* cmsCallService */
#define ZIS_STUB_CMCALLS2 363 /* cmsCallService2 */
#define ZIS_STUB_CMCALLS3 364 /* cmsCallService3 */
#define ZIS_STUB_CMCMSPRF 365 /* cmsPrintf */
#define ZIS_STUB_CMCMSVPF 366 /* vcmsPrintf */
#define ZIS_STUB_CMHEXDMP 367 /* cmsHexDump */
#define ZIS_STUB_CMGETPRM 368 /* cmsGetConfigParm */
#define ZIS_STUB_CMGETPRU 369 /* cmsGetConfigParmUnchecked */
#define ZIS_STUB_CMGETLOG 370 /* cmsGetPCLogLevel */
#define ZIS_STUB_CMGETSTS 371 /* cmsGetStatus */
#define ZIS_STUB_CMMKSNAM 372 /* cmsMakeServerName */

/* dynalloc, 380-409 */
#define ZIS_STUB_DYNASTXU 380 /* createSimpleTextUnit mapped */
#define ZIS_STUB_DYNASTX2 381 /* createSimpleTextUnit2 mapped */
#define ZIS_STUB_DYNACTXU 382 /* createCharTextUnit mapped */
#define ZIS_STUB_DYNACMTU 383 /* createCompoundTextUnit mapped */
#define ZIS_STUB_DYNAITXU 384 /* createIntTextUnit mapped */
#define ZIS_STUB_DYNAI1TU 385 /* createInt8TextUnit mapped */
#define ZIS_STUB_DYNAI2TU 386 /* createInt16TextUnit mapped */
#define ZIS_STUB_DYNAFTXU 387 /* freeTextUnit mapped */
#define ZIS_STUB_DYNAAIRD 388 /* AllocIntReader mapped */
#define ZIS_STUB_DYNASLIB 389 /* dynallocSharedLibrary mapped */
#define ZIS_STUB_DYNAUSSD 390 /* dynallocUSSDirectory mapped */
#define ZIS_STUB_DYNAUSSO 391 /* dynallocUSSOutput mapped */
#define ZIS_STUB_DYNASAPI 392 /* AllocForSAPI mapped */
#define ZIS_STUB_DYNADOUT 393 /* AllocForDynamicOutput mapped */
#define ZIS_STUB_DYNADDDN 394 /* DeallocDDName mapped */
#define ZIS_STUB_DYNAUALC 395 /* dynallocDataset mapped */
#define ZIS_STUB_DYNAUALM 396 /* dynallocDatasetMember mapped */
#define ZIS_STUB_DYNADALC 397 /* unallocDataset mapped */
#define ZIS_STUB_DYNALAD2 398 /* dynallocAllocDataset */
#define ZIS_STUB_DYNALUD2 399 /* dynallocUnallocDatasetByDDName */

/* idcams, 410-419 */
#define ZIS_STUB_RSIDCCCM 410 /* idcamsCreateCommand mapped */
#define ZIS_STUB_RSIDCALN 411 /* idcamsAddLineToCommand mapped */
#define ZIS_STUB_RSIDCEXE 412 /* idcamsExecuteCommand mapped */
#define ZIS_STUB_RSIDCDCM 413 /* idcamsDeleteCommand mapped */
#define ZIS_STUB_RSIDCPCO 414 /* idcamsPrintCommandOutput mapped */
#define ZIS_STUB_RSIDCDCO 415 /* idcamsDeleteCommandOutput mapped */

/* isgenq, 420-429 */
#define ZIS_STUB_ENQTRYX  420 /* isgenqTryExclusiveLock mapped */
#define ZIS_STUB_ENQGETX  421 /* isgenqGetExclusiveLock mapped */
#define ZIS_STUB_ENQTRYS  422 /* isgenqTrySharedLock mapped */
#define ZIS_STUB_ENQGETS  423 /* isgenqGetSharedLock mapped */
#define ZIS_STUB_ENQTSTX  424 /* isgenqTestExclusiveLock mapped */
#define ZIS_STUB_ENQTSTS  425 /* isgenqTestSharedLock mapped */
#define ZIS_STUB_ENQTEST  426 /* isgenqTestLock mapped */
#define ZIS_STUB_ENQREL   427 /* isgenqReleaseLock mapped */

/* le, 430-449 */
#define ZIS_STUB_LEGETCAA 430 /* getCAA */
#define ZIS_STUB_LESHWRTL 431 /* showRTL */
#define ZIS_STUB_LEMKRLEA 432 /* makeRLEAnchor */
#define ZIS_STUB_LEDLRLEA 433 /* deleteRLEAnchor */
#define ZIS_STUB_LEMKRLET 434 /* makeRLETask */
#define ZIS_STUB_LEDLRLET 435 /* deleteRLETask */
#define ZIS_STUB_LEINRLEE 436 /* initRLEEnvironment */
#define ZIS_STUB_LETMRLEE 437 /* termRLEEnvironment */
#define ZIS_STUB_LEMKFCAA 438 /* makeFakeCAA */
#define ZIS_STUB_LEARTCAA 439 /* abortIfUnsupportedCAA */

/* logging, 450-479 */
#define ZIS_STUB_MKLOGCTX 450 /* makeLoggingContext */
#define ZIS_STUB_MKLLGCTX 451 /* makeLocalLoggingContext */
#define ZIS_STUB_RMLOGCTX 452 /* removeLoggingContext */
#define ZIS_STUB_RMLLGCTX 453 /* removeLocalLoggingContext */
#define ZIS_STUB_GTLOGCTX 454 /* getLoggingContext */
#define ZIS_STUB_STLOGCTX 455 /* setLoggingContext */
#define ZIS_STUB_ZOWELOG  456 /* zowelog */
#define ZIS_STUB_ZOWEDUMP 457 /* zowedump */
#define ZIS_STUB_LGCFGDST 458 /* logConfigureDestination */
#define ZIS_STUB_LGCFGDS2 459 /* logConfigureDestination2 */
#define ZIS_STUB_LGCFGSTD 460 /* logConfigureStandardDestinations */
#define ZIS_STUB_LGCFGCMP 461 /* logConfigureComponent */
#define ZIS_STUB_LGSETLVL 462 /* logSetLevel */
#define ZIS_STUB_LGGETLVL 463 /* logGetLevel */
#define ZIS_STUB_LGSHTRCE 464 /* logShouldTraceInternal */
#define ZIS_STUB_LGGLOGCX 465 /* logGetExternalContext */
#define ZIS_STUB_LGSLOGCX 466 /* logSetExternalContext */
#define ZIS_STUB_LGPRSOUT 467 /* printStdout */
#define ZIS_STUB_LGPRSERR 468 /* printStderr */

/* lpa, 480-489 */
#define ZIS_STUB_LPAADD   480 /* lpaAdd mapped */
#define ZIS_STUB_LPADEL   481 /* lpaDelete mapped */

/* metalio, 490-509 */
#define ZIS_STUB_GTNMTKVL 490 /* getNameTokenValue */
#define ZIS_STUB_CRNMTKPR 491 /* createNameTokenPair */
#define ZIS_STUB_DLNMTKPR 492 /* deleteNameTokenPair */
#define ZIS_STUB_GTSYSOUT 493 /* getSYSOUTStruct */
#define ZIS_STUB_WTOPRNF  494 /* wtoPrintf */
#define ZIS_STUB_AWTOPRNF 495 /* authWTOPrintf */
#define ZIS_STUB_SENDWTO  496 /* sendWTO */
#define ZIS_STUB_QSAMPRNF 497 /* qsamPrintf */
#define ZIS_STUB_PRINTF   498 /* printf */
#define ZIS_STUB_FFLUSH   499 /* fflush */
#define ZIS_STUB_SETJMP   500 /* setjmp */
#define ZIS_STUB_LONGJMP  501 /* longjmp */

/* nametoken, 510-519 */
#define ZIS_STUB_NTPCREF  510 /* nameTokenCreate */
#define ZIS_STUB_NTPCRTPF 511 /* nameTokenCreatePersistent */
#define ZIS_STUB_NTPDELF  512 /* nameTokenDelete */
#define ZIS_STUB_NTPRETF  513 /* nameTokenRetrieve */

/* pause-element, 520-529 */
#define ZIS_STUB_PEALLOC  520 /* peAlloc    */
#define ZIS_STUB_PEDALLOC 521 /* peDealloc  */
#define ZIS_STUB_PEPAUSE  522 /* pePause    */
#define ZIS_STUB_PERELEAS 523 /* peRelease  */
#define ZIS_STUB_PERTVNFO 524 /* peRetrieveInfo  */
#define ZIS_STUB_PERTVNF2 525 /* peRetrieveInfo2 */
#define ZIS_STUB_PETEST   526 /* peTest          */
#define ZIS_STUB_PETRNSFR 527 /* peTransfer      */

/* pc, 530-549 */
#define ZIS_STUB_PCAXSET  530 /* pcSetAllAddressSpaceAuthority */
#define ZIS_STUB_PCLXRES  531 /* pcReserveLinkageIndex */
#define ZIS_STUB_PCLXFRE  532 /* pcFreeLinkageIndex */
#define ZIS_STUB_PCETCRED 533 /* pcMakeEntryTableDescriptor */
#define ZIS_STUB_PCETADD  534 /* pcAddToEntryTableDescriptor */
#define ZIS_STUB_PCETREMD 535 /* pcRemoveEntryTableDescriptor */
#define ZIS_STUB_PCETCRE  536 /* pcCreateEntryTable */
#define ZIS_STUB_PCETDES  537 /* pcDestroyEntryTable */
#define ZIS_STUB_PCETCON  538 /* pcConnectEntryTable */
#define ZIS_STUB_PCETDIS  539 /* pcDisconnectEntryTable */
#define ZIS_STUB_PCCALLR  540 /* pcCallRoutine */

/* qsam, 550-579 */
#define ZIS_STUB_MALLOC24 550 /* malloc24 */
#define ZIS_STUB_OPENSAM  551 /* openSAM */
#define ZIS_STUB_GTSAMLN  552 /* getSAMLength */
#define ZIS_STUB_GTSAMBS  553 /* getSAMBlksize */
#define ZIS_STUB_GTSAMLR  554 /* getSAMLrecl */
#define ZIS_STUB_GTSAMCC  555 /* getSAMCC */
#define ZIS_STUB_GTSAMRF  556 /* getSAMRecfm */
#define ZIS_STUB_PUTLINE  557 /* putline */
#define ZIS_STUB_GETLINE  558 /* getline */
#define ZIS_STUB_GTEODBUF 559 /* getEODADBuffer */
#define ZIS_STUB_FREODBUF 560 /* freeEODADBuffer */
#define ZIS_STUB_GETLINE2 561 /* getline2 */
#define ZIS_STUB_MKQSMBUF 562 /* makeQSAMBuffer */
#define ZIS_STUB_FRQSMBUF 563 /* freeQSAMBuffer */
#define ZIS_STUB_PUTLNEV  564 /* putlineV */
#define ZIS_STUB_GETLNEV  565 /* getlineV */
#define ZIS_STUB_CLOSESAM 566 /* closeSAM */
#define ZIS_STUB_OPENEXCP 567 /* openEXCP */
#define ZIS_STUB_CLOSEXCP 568 /* closeEXCP */
#define ZIS_STUB_HASVRLEN 569 /* hasVaryingRecordLength */
#define ZIS_STUB_BPAMDELM 570 /* bpamDeleteMember */
#define ZIS_STUB_BPAMFIND 571 /* bpamFind */
#define ZIS_STUB_BPAMREAD 572 /* bpamRead */
#define ZIS_STUB_BPAMRD2  573 /* bpamRead2 */

/* radmin, 580-599 */
#define ZIS_STUB_RADMXUP  580 /* radminExtractUserProfiles mapped */
#define ZIS_STUB_RADMXBUP 581 /* radminExtractBasicUserProfileInfo mapped */
#define ZIS_STUB_RADMXBRP 582 /* radminExtractBasicGenresProfileInfo mapped */
#define ZIS_STUB_RADMXBGP 583 /* radminExtractBasicGroupProfileInfo mapped */
#define ZIS_STUB_RADMXPAL 584 /* radminExtractGenresAccessList mapped */
#define ZIS_STUB_RADMXGAL 585 /* radminExtractGroupAccessList mapped */
#define ZIS_STUB_RADMPREA 586 /* radminPerformResAction mapped */
#define ZIS_STUB_RADMPGRA 587 /* radminPerformGroupAction mapped */
#define ZIS_STUB_RADMPCNA 588 /* radminPerformConnectionAction mapped */
#define ZIS_STUB_RADMRCMD 589 /* radminRunRACFCommand mapped */

/* recovery, 600-619 */
#define ZIS_STUB_RCVRESRR 600 /* recoveryEstablishRouter */
#define ZIS_STUB_RCVRRSRR 601 /* recoveryRemoveRouter */
#define ZIS_STUB_RCVRTRES 602 /* recoveryIsRouterEstablished */
#define ZIS_STUB_RCVRPSFR 603 /* recoveryPush */
#define ZIS_STUB_RCVRPPFR 604 /* recoveryPop */
#define ZIS_STUB_RCVRSDTL 605 /* recoverySetDumpTitle */
#define ZIS_STUB_RCVRSFLV 606 /* recoverySetFlagValue */
#define ZIS_STUB_RCVRECST 607 /* recoveryEnableCurrentState */
#define ZIS_STUB_RCVRDCST 608 /* recoveryDisableCurrentState */
#define ZIS_STUB_RCVRURSI 609 /* recoveryUpdateRouterServiceInfo */
#define ZIS_STUB_RCVRUSSI 610 /* recoveryUpdateStateServiceInfo */
#define ZIS_STUB_RCVRGACD 611 /* recoveryGetABENDCode */

/* resmgr, 620-629 */
#define ZIS_STUB_RMGRATRM 620 /* resmgrAddTaskResourceManager */
#define ZIS_STUB_RMGRAARM 621 /* resmgrAddAddressSpaceResourceManager */
#define ZIS_STUB_RMGRDTRM 622 /* resmgrDeleteTaskResourceManager */
#define ZIS_STUB_RMGRDARM 623 /* resmgrDeleteAddressSpaceResourceManager */

/* scheduling, 630-639 */
#define ZIS_STUB_SCHZOSWT 630 /* zosWait */
#define ZIS_STUB_SCHZOSWL 631 /* zosWaitList */
#define ZIS_STUB_SCHZOSPT 632 /* zosPost */
#define ZIS_STUB_SCHSRLET 633 /* startRLETask */

/* shrmem64, 640-659 */
#define ZIS_STUB_SHR64TKN 640 /* shrmem64GetAddressSpaceToken */
#define ZIS_STUB_SHR64ALC 641 /* shrmem64Alloc */
#define ZIS_STUB_SHR64AL2 642 /* shrmem64Alloc2 */
#define ZIS_STUB_SHR64CAL 643 /* shrmem64CommonAlloc */
#define ZIS_STUB_SHR64CA2 644 /* shrmem64CommonAlloc2 */
#define ZIS_STUB_SHR64REL 645 /* shrmem64Release */
#define ZIS_STUB_SHR64REA 646 /* shrmem64ReleaseAll */
#define ZIS_STUB_SHR64GAC 647 /* shrmem64GetAccess */
#define ZIS_STUB_SHR64GA2 648 /* shrmem64GetAccess2 */
#define ZIS_STUB_SHR64RAC 649 /* shrmem64RemoveAccess */
#define ZIS_STUB_SHR64RA2 650 /* shrmem64RemoveAccess2 */

/* timeutls, 660-679 */
#define ZIS_STUB_GETSTCK  660 /* getSTCK */
#define ZIS_STUB_GETSTCKU 661 /* getSTCKU */
#define ZIS_STUB_CONVTODL 662 /* convertTODToLocal */
#define ZIS_STUB_TZDIFFOR 663 /* timeZoneDifferenceFor */
#define ZIS_STUB_STCKCONV 664 /* stckToTimestamp */
#define ZIS_STUB_CONVTOD  665 /* timestampToSTCK */
#define ZIS_STUB_MIDNIGHT 666 /* timeFromMidnight */
#define ZIS_STUB_STCKYYMD 667 /* stckFromYYYYMMDD */
#define ZIS_STUB_ELPSTIME 668 /* elapsedTime */
#define ZIS_STUB_STCKUNIX 669 /* stckToUnix */
#define ZIS_STUB_STCK2USM 670 /* stckToUnixSecondsAndMicros */
#define ZIS_STUB_CONVUNIX 671 /* unixToTimestamp */
#define ZIS_STUB_GTDAYMNT 672 /* getDayAndMonth */
#define ZIS_STUB_SNPRNTLT 673 /* snprintLocalTime */

/* utils, 680-779 */
#define ZIS_STUB_STRCPSAF 680 /* strcopy_safe */
#define ZIS_STUB_INDEXOF  681 /* indexOf */
#define ZIS_STUB_IDXSTR   682 /* indexOfString */
#define ZIS_STUB_LIDXSTR  683 /* lastIndexOfString */
#define ZIS_STUB_LINDEXOF 684 /* lastIndexOf */
#define ZIS_STUB_IDXSTRNS 685 /* indexOfStringInsensitive */
#define ZIS_STUB_ISZEROS  686 /* isZeros */
#define ZIS_STUB_ISBLANKS 687 /* isBlanks */
#define ZIS_STUB_HASTEXT  688 /* hasText */
#define ZIS_STUB_PARSEINT 689 /* parseInt */
#define ZIS_STUB_PSINTINT 690 /* parseInitialInt */
#define ZIS_STUB_NULLTERM 691 /* nullTerminate */
#define ZIS_STUB_ISCHARAN 692 /* isCharAN */
#define ZIS_STUB_TKGTDCML 693 /* tknGetDecimal */
#define ZIS_STUB_TKGTQOTD 694 /* tknGetQuoted */
#define ZIS_STUB_TKGTANUM 695 /* tknGetAlphanumeric */
#define ZIS_STUB_TKGTSTND 696 /* tknGetStandard */
#define ZIS_STUB_TKGTNWSP 697 /* tknGetNonWhitespace */
#define ZIS_STUB_TKGTTERM 698 /* tknGetTerminating */
#define ZIS_STUB_TKTXEQLS 699 /* tknTextEquals */
#define ZIS_STUB_FREETKN  700 /* freeToken */
#define ZIS_STUB_TKNTEXT  701 /* tknText */
#define ZIS_STUB_TKNINT   702 /* tknInt */
#define ZIS_STUB_TKNLNGTH 703 /* tknLength */
#define ZIS_STUB_DMPBUFFR 704 /* dumpbuffer */
#define ZIS_STUB_DMPBUFFA 705 /* dumpbufferA */
#define ZIS_STUB_HEXFILL  706 /* hexFill */
#define ZIS_STUB_SMPHXFIL 707 /* simpleHexFill */
#define ZIS_STUB_SMPHXPRT 708 /* simpleHexPrint */
#define ZIS_STUB_SMPHXPRL 709 /* simpleHexPrintLower */
#define ZIS_STUB_HEXDUMP  710 /* hexdump */
#define ZIS_STUB_DMPBFFR2 711 /* dumpbuffer2 */
#define ZIS_STUB_DMPBFFRS 712 /* dumpBufferToStream */
#define ZIS_STUB_CMPIGNCS 713 /* compareIgnoringCase */
#define ZIS_STUB_STRUPCAS 714 /* strupcase */
#define ZIS_STUB_MAKESLH  715 /* makeShortLivedHeap */
#define ZIS_STUB_MAKSLH64 716 /* makeShortLivedHeap64 */
#define ZIS_STUB_SLHALLOC 717 /* SLHAlloc */
#define ZIS_STUB_SLHFREE  718 /* SLHFree */
#define ZIS_STUB_NYMALLOC 719 /* noisyMalloc */
#define ZIS_STUB_DECODB32 720 /* base32Encode */
#define ZIS_STUB_ENCODB32 721 /* base32Decode */
#define ZIS_STUB_DECODB64 722 /* decodeBase64 */
#define ZIS_STUB_DECDB64U 723 /* decodeBase64Unterminated */
#define ZIS_STUB_ENCODB64 724 /* encodeBase64 */
#define ZIS_STUB_ENCDB64N 725 /* encodeBase64NoAlloc */
#define ZIS_STUB_B642BURL 726 /* base64ToBase64url */
#define ZIS_STUB_B64URLTB 727 /* base64urlToBase64 */
#define ZIS_STUB_CLNURLPV 728 /* cleanURLParamValue */
#define ZIS_STUB_PCTENCOD 729 /* percentEncode */
#define ZIS_STUB_DSTUNASC 730 /* destructivelyUnasciify */
#define ZIS_STUB_MAKESLST 731 /* makeStringList */
#define ZIS_STUB_SLSTLEN  732 /* stringListLength */
#define ZIS_STUB_SLSTPRNT 733 /* stringListPrint */
#define ZIS_STUB_SLSTCTNS 734 /* stringListContains */
#define ZIS_STUB_SLSTLAST 735 /* stringListLast */
#define ZIS_STUB_ADSLSTUQ 736 /* addToStringListUnique */
#define ZIS_STUB_ADSLST   737 /* addToStringList */
#define ZIS_STUB_SLSTELT1 738 /* firstStringListElt */
#define ZIS_STUB_STRCNCAT 739 /* stringConcatenate */
#define ZIS_STUB_MKBFCHST 740 /* makeBufferCharStream */
#define ZIS_STUB_CSTRPOSN 741 /* charStreamPosition */
#define ZIS_STUB_CSTRGET  742 /* charStreamGet */
#define ZIS_STUB_CSTREOF  743 /* charStreamEOF */
#define ZIS_STUB_CSTRCLOS 744 /* charStreamClose */
#define ZIS_STUB_CSTRFREE 745 /* charStreamFree */
#define ZIS_STUB_PADWSPCS 746 /* padWithSpaces */
#define ZIS_STUB_RPLTRMNL 747 /* replaceTerminateNulls */
#define ZIS_STUB_CNVINTST 748 /* convertIntToString */
#define ZIS_STUB_HEXTODEC 749 /* hexToDec */
#define ZIS_STUB_DECTOHEX 750 /* decToHex */
#define ZIS_STUB_COMPSEQS 751 /* compareSequences */
#define ZIS_STUB_DEC2OCT  752 /* decimalToOctal */
#define ZIS_STUB_UNX2ISO  753 /* convertUnixToISO */
#define ZIS_STUB_MATCHWLD 754 /* matchWithWildcards */
#define ZIS_STUB_STRISDIG 755 /* stringIsDigit */
#define ZIS_STUB_TRMRIGHT 756 /* trimRight */
#define ZIS_STUB_ISPASPHR 757 /* isPassPhrase */

/* vsam, 780-809 */
#define ZIS_STUB_MAKEACB  780 /* makeACB */
#define ZIS_STUB_VOPCLACB 781 /* opencloseACB */
#define ZIS_STUB_OPENACB  782 /* openACB */
#define ZIS_STUB_CLOSEACB 783 /* closeACB */
#define ZIS_STUB_MODRPL   784 /* modRPL */
#define ZIS_STUB_VSPOINT  785 /* point */
#define ZIS_STUB_VPUTREC  786 /* putRecord */
#define ZIS_STUB_VGETREC  787 /* getRecord */
#define ZIS_STUB_VMKDBUFF 788 /* makeDataBuffer */
#define ZIS_STUB_VFRDBUFF 789 /* freeDataBuffer */
#define ZIS_STUB_VPNBYRBA 790 /* pointByRBA */
#define ZIS_STUB_VPNBYKEY 791 /* pointByKey */
#define ZIS_STUB_VPNBYREC 792 /* pointByRecord */
#define ZIS_STUB_VPNBYCI  793 /* pointByCI */
#define ZIS_STUB_VALLOCDS 794 /* allocateDataset */
#define ZIS_STUB_VDEFAIX  795 /* defineAIX */
#define ZIS_STUB_DELCLUST 796 /* deleteCluster */

/* xlate, 810-819 */
#define ZIS_STUB_E2A      810 /* e2a */
#define ZIS_STUB_A2E      811 /* a2e */

/* zos, 820-879 */
#define ZIS_STUB_EXTRPSW  820 /* extractPSW */
#define ZIS_STUB_SUPRMODE 821 /* supervisorMode */
#define ZIS_STUB_SETKEY   822 /* setKey */
#define ZIS_STUB_DDEXISTS 823 /* ddnameExists */
#define ZIS_STUB_ATOMINCR 824 /* atomicIncrement */
#define ZIS_STUB_GTCRACEE 825 /* getCurrentACEE */
#define ZIS_STUB_GTFCHTCB 826 /* getFirstChildTCB */
#define ZIS_STUB_GTPRTTCB 827 /* getParentTCB */
#define ZIS_STUB_GTNXSTCB 828 /* getNextSiblingTCB */
#define ZIS_STUB_GETCVT   829 /* getCVT */
#define ZIS_STUB_GETATCVT 830 /* getATCVT */
#define ZIS_STUB_GETCSTBL 831 /* getIEACSTBL */
#define ZIS_STUB_GETCVTPR 832 /* getCVTPrefix */
#define ZIS_STUB_GETECVT  833 /* getECVT */
#define ZIS_STUB_GETTCB   834 /* getTCB */
#define ZIS_STUB_GETSTCB  835 /* getSTCB */
#define ZIS_STUB_GETOTCB  836 /* getOTCB */
#define ZIS_STUB_GETASCB  837 /* getASCB */
#define ZIS_STUB_GETASXB  838 /* getASXB */
#define ZIS_STUB_GETASSB  839 /* getASSB */
#define ZIS_STUB_GETJSAB  840 /* getJSAB */
#define ZIS_STUB_GTSPLXNM 841 /* getSysplexName */
#define ZIS_STUB_GTSYSTNM 842 /* getSystemName */
#define ZIS_STUB_GETDSAB  843 /* getDSAB */
#define ZIS_STUB_DSABOMVS 844 /* dsabIsOMVS */
#define ZIS_STUB_LOCATE   845 /* locate */
#define ZIS_STUB_GETR13   846 /* getR13 */
#define ZIS_STUB_GETR12   847 /* getR12 */
#define ZIS_STUB_GETASCBJ 848 /* getASCBJobname */
#define ZIS_STUB_LOADBYNA 849 /* loadByName */
#define ZIS_STUB_LOADBNML 850 /* loadByNameLocally */
#define ZIS_STUB_ZOSCLCKD 851 /* isCallerLocked */
#define ZIS_STUB_ZOSCSRB  852 /* isCallerSRB */
#define ZIS_STUB_ZOSCXMEM 853 /* isCallerCrossMemory */
#define ZIS_STUB_GADSACEE 854 /* getAddressSpaceAcee */
#define ZIS_STUB_GTSKACEE 855 /* getTaskAcee */
#define ZIS_STUB_STSKACEE 856 /* setTaskAcee */
#define ZIS_STUB_SAFVRIFY 857 /* safVerify */
#define ZIS_STUB_SAFVRFY2 858 /* safVerify2 */
#define ZIS_STUB_SAFVRFY3 859 /* safVerify3 */
#define ZIS_STUB_SAFVRFY4 860 /* safVerify4 */
#define ZIS_STUB_SAFVRFY5 861 /* safVerify5 */
#define ZIS_STUB_SAFVRFY6 862 /* safVerify6 */
#define ZIS_STUB_SAFVRFY7 863 /* safVerify7 */
#define ZIS_STUB_SAFAUTH  864 /* safAuth */
#define ZIS_STUB_SAFAUTHS 865 /* safAuthStatus */
#define ZIS_STUB_SAFSTAT  866 /* safStat */
#define ZIS_STUB_GETSAFPL 867 /* getSafProfileMaxLen */

/* zvt, 880-889 */
#define ZIS_STUB_ZVTINIT  880 /* zvtInit mapped */
#define ZIS_STUB_ZVTGET   881 /* zvtGet mapped */
#define ZIS_STUB_ZVTAENTR 882 /* zvtAllocEntry mapped */
#define ZIS_STUB_ZVTFENTR 883 /* zvtFreeEntry mapped */
#define ZIS_STUB_ZVTGXMLR 884 /* zvtGetCMSLookupRoutineAnchor mapped */
#define ZIS_STUB_ZVTSXMLR 885 /* zvtSetCMSLookupRoutineAnchor mapped */

#endif /* ZIS_ZISSTUBS_H_ */

/*
  This program and the accompanying materials are
  made available under the terms of the Eclipse Public License v2.0 which accompanies
  this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html

  SPDX-License-Identifier: EPL-2.0

  Copyright Contributors to the Zowe Project.
*/
