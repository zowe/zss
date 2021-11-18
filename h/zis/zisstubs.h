#ifndef __ZOSSTEAM_ZISSTUBS__
#define __ZOSSTEAM_ZISSTUBS__ 1

#define MAX_ZIS_STUBS 1000

#define ZIS_STUB_SHR64TKN 10  /* shrmem64GetAddressSpaceToken  */
#define ZIS_STUB_SHR64ALC 11  /* shrmem64Alloc      */
#define ZIS_STUB_SHR64AL2 12  /* shrmem64Alloc2     */
#define ZIS_STUB_SHR64CAL 13  /* shrmem64CommonAlloc      */
#define ZIS_STUB_SHR64CA2 14  /* shrmem64CommonAlloc2     */
#define ZIS_STUB_SHR64REL 15  /* shrmem64Release    */
#define ZIS_STUB_SHR64REA 16  /* shrmem64ReleaseAll */

#define ZIS_STUB_SHR64GAC 17  /* shrmem64GetAccess     */
#define ZIS_STUB_SHR64GA2 18  /* shrmem64GetAccess2    */
#define ZIS_STUB_SHR64RAC 19  /* shrmem64RemoveAccess  */
#define ZIS_STUB_SHR64RA2 20  /* shrmem64RemoveAccess2  */

#define ZIS_STUB_CPL64CRE 30  /* iarcp64Create */
#define ZIS_STUB_CPL64GET 31  /* iarcp64Get    */
#define ZIS_STUB_CPL64FRE 32  /* iarcp64Free   */
#define ZIS_STUB_CPL64DEL 33  /* iarcp64Delete */

/* xlate */

#define ZIS_STUB_E2A      35  /* e2a */
#define ZIS_STUB_A2E      36  /* a2e */

#define ZIS_STUB_PEALLOC  40  /* peAlloc    */
#define ZIS_STUB_PEDALLOC 41  /* peDealloc  */
#define ZIS_STUB_PEPAUSE  42  /* pePause    */
#define ZIS_STUB_PERELEAS 43  /* peRelease  */
#define ZIS_STUB_PERTVNFO 44  /* peRetrieveInfo  */
#define ZIS_STUB_PERTVNF2 45  /* peRetrieveInfo2 */
#define ZIS_STUB_PETEST   46  /* peTest          */
#define ZIS_STUB_PETRNSFR 47  /* peTransfer      */

/* alloc 50-69 */

#define ZIS_STUB_SAFEMLLC 50 /* safeMalloc */
#define ZIS_STUB_SAFEMLC2 51 /* safeMalloc2 */
#define ZIS_STUB_SAFMLC31 52 /* safeMalloc31 */
#define ZIS_STUB_SAFMLC38 53 /* safeMalloc31Key8 */
#define ZIS_STUB_SAFFRE31 54 /* safeFree31 */
#define ZIS_STUB_SAFFRE38 55 /* safeFree31Key8 */
#define ZIS_STUB_SAFFRE64 56 /* safeFree64 */
#define ZIS_STUB_SFFRE64T 57 /* safeFree64ByToken */
#define ZIS_STUB_SAFMLC64 58 /* safeMalloc64 */
#define ZIS_STUB_SFMLC64T 59 /* safeMalloc64ByToken */
#define ZIS_STUB_SAFRE642 60 /* safeFree64v2 */
#define ZIS_STUB_SAMLC642 61 /* safeMalloc64v2 */
#define ZIS_STUB_SAFRE643 62 /* safeFree64v3 */
#define ZIS_STUB_SAMLC643 63 /* safeMalloc64v3 */
#define ZIS_STUB_SAFEFREE 64 /* safeFree */

/* utils 70-119 */

#define ZIS_STUB_STRCPSAF 70 /* strcopy_safe */
#define ZIS_STUB_INDEXOF  71 /* indexOf */
#define ZIS_STUB_IDXSTR   72 /* indexOfString */
#define ZIS_STUB_LIDXSTR  73 /* lastIndexOfString */
#define ZIS_STUB_IDXSTRNS 74 /* indexOfStringInsensitive */
#define ZIS_STUB_PSINTINT 75 /* parseInitialInt */
#define ZIS_STUB_NULLTERM 76 /* nullTerminate */
#define ZIS_STUB_TKGTDCML 77 /* tknGetDecimal */
#define ZIS_STUB_TKGTQOTD 78 /* tknGetQuoted */
#define ZIS_STUB_TKGTANUM 79 /* tknGetAlphanumeric */
#define ZIS_STUB_TKGTSTND 80 /* tknGetStandard */
#define ZIS_STUB_TKGTNWSP 81 /* tknGetNonWhitespace */
#define ZIS_STUB_TKGTTERM 82 /* tknGetTerminating */
#define ZIS_STUB_TKTXEQLS 83 /* tknTextEquals */
#define ZIS_STUB_FREETKN  84 /* freeToken */
#define ZIS_STUB_TKNLNGTH 85 /* tknLength */
#define ZIS_STUB_DMPBUFFR 86 /* dumpbuffer */
#define ZIS_STUB_DMPBUFFA 87 /* dumpbufferA */
#define ZIS_STUB_SMPHXFIL 88 /* simpleHexFill */
#define ZIS_STUB_SMPHXPRT 89 /* simpleHexPrint */
#define ZIS_STUB_SMPHXPRL 90 /* simpleHexPrintLower */
#define ZIS_STUB_DMPBFFR2 91 /* dumpbuffer2 */
#define ZIS_STUB_DMPBFFRS 92 /* dumpBufferToStream */
#define ZIS_STUB_CMPIGNCS 93 /* compareIgnoringCase */
#define ZIS_STUB_STRUPCAS 94 /* strupcase */
#define ZIS_STUB_MAKESLH  95 /* makeShortLivedHeap */
#define ZIS_STUB_MAKSLH64 96 /* makeShortLivedHeap64 */
#define ZIS_STUB_SLHALLOC 97 /* SLHAlloc */
#define ZIS_STUB_NYMALLOC 98 /* noisyMalloc */
#define ZIS_STUB_DECODB32 99 /* base32Decode */
#define ZIS_STUB_ENCODB32 100 /* base32Encode */
#define ZIS_STUB_DECODB64 101 /* decodeBase64 */
#define ZIS_STUB_ENCODB64 102 /* encodeBase64 */
#define ZIS_STUB_CLNURLPV 103 /* cleanURLParamValue */
#define ZIS_STUB_PCTENCOD 104 /* percentEncode */
#define ZIS_STUB_DSTUNASC 105 /* destructivelyUnasciify */
#define ZIS_STUB_MAKESLST 106 /* makeStringList */
#define ZIS_STUB_SLSTLEN  107 /* stringListLength */
#define ZIS_STUB_SLSTPRNT 108 /* stringListPrint */
#define ZIS_STUB_SLSTCTNS 109 /* stringListContains */
#define ZIS_STUB_SLSTLAST 110 /* stringListLast */
#define ZIS_STUB_ADSLSTUQ 111 /* addToStringListUnique */
#define ZIS_STUB_ADSLST   112 /* addToStringList */
#define ZIS_STUB_SLSTELT1 113 /* firstStringListElt */
#define ZIS_STUB_STRCNCAT 114 /* stringConcatenate */
#define ZIS_STUB_MKBFCHST 115 /* makeBufferCharStream */
#define ZIS_STUB_CSTRPOSN 116 /* charStreamPosition */
#define ZIS_STUB_CSTRGET  117 /* charStreamGet */
#define ZIS_STUB_CSTREOF  118 /* charStreamEOF */
#define ZIS_STUB_CSTRCLOS 119 /* charStreamClose */
#define ZIS_STUB_CSTRFREE  65 /* charStreamFree */

/* logging 120-139 */

#define ZIS_STUB_MKLOGCTX 120 /* makeLoggingContext */
#define ZIS_STUB_MKLLGCTX 121 /* makeLocalLoggingContext */
#define ZIS_STUB_RMLOGCTX 123 /* removeLoggingContext */
#define ZIS_STUB_RMLLGCTX 124 /* removeLocalLoggingContext */
#define ZIS_STUB_GTLOGCTX 125 /* getLoggingContext */
#define ZIS_STUB_STLOGCTX 126 /* setLoggingContext */
#define ZIS_STUB_LGCFGDST 127 /* logConfigureDestination */
#define ZIS_STUB_LGCFGDS2 128 /* logConfigureDestination2 */
#define ZIS_STUB_LGCFGSTD 129 /* logConfigureStandardDestinations */
#define ZIS_STUB_LGCFGCMP 130 /* logConfigureComponent */
#define ZIS_STUB_LGSHTRCE 131 /* logShouldTraceInternal */
/* #define ZIS_STUB_LGGLOGCX 132 logGetExternalContext - provider stub */
/* #define ZIS_STUB_LGSLOGCX 133 logSetExternalContext - provider stub */
#define ZIS_STUB_LGPRSOUT 134 /* printStdout */
#define ZIS_STUB_LGPRSERR 135 /* printStderr */
#define ZIS_STUB_ZOWELOG  136 /* zowelog */


/* metalio 140-149 */

/* #define ZIS_STUB_GTNMTKFT 140 getNameTokenFunctionTable - Defunct */
#define ZIS_STUB_GTNMTKVL 141 /* getNameTokenValue */
#define ZIS_STUB_CRNMTKPR 142 /* createNameTokenPair */
/* #define ZIS_STUB_DLNMTKPR 143 - not used */
/* #define ZIS_STUB_INITSYTS 144 - not used */
#define ZIS_STUB_GTSYSOUT 145 /* getSYSOUTStruct */
#define ZIS_STUB_WTOPRNF  146 /* wtoPrintf */
#define ZIS_STUB_AWTOPRNF 147 /* authWTOPrintf */
#define ZIS_STUB_SENDWTO  148 /* sendWTO */
#define ZIS_STUB_QSAMPRNF 149 /* qsamPrintf */


/* qsam 150-169 */

#define ZIS_STUB_HASVRLEN 150 /* hasVaryingRecordLength */
#define ZIS_STUB_MKQSMBUF 151 /* makeQSAMBuffer */
#define ZIS_STUB_FRQSMBUF 152 /* freeQSAMBuffer */
#define ZIS_STUB_PUTLNEV  153 /* putlineV */
#define ZIS_STUB_GETLNEV  154 /* getlineV */
#define ZIS_STUB_GTEODBUF 155 /* getEODADBuffer */
#define ZIS_STUB_FREODBUF 156 /* freeEODADBuffer */
#define ZIS_STUB_GTSAMLN  157 /* getSAMLength */
#define ZIS_STUB_GTSAMBS  158 /* getSAMBlksize */
#define ZIS_STUB_GTSAMLR  159 /* getSAMLrecl */
#define ZIS_STUB_GTSAMCC  160 /* getSAMCC */
#define ZIS_STUB_GTSAMRF  161 /* getSAMRecfm */
#define ZIS_STUB_BPAMFIND 162 /* bpamFind */
#define ZIS_STUB_BPAMREAD 163 /* bpamRead */
#define ZIS_STUB_BPAMRD2  164 /* bpamRead2 */
#define ZIS_STUB_OPENEXCP 165 /* openEXCP */
#define ZIS_STUB_CLOSEXCP 166 /* closeEXCP */

/* le 170-179 */

#define ZIS_STUB_LEGETCAA 170 /* getCAA */
#define ZIS_STUB_LESHWRTL 171 /* showRTL */
#define ZIS_STUB_LEMKRLEA 172 /* makeRLEAnchor */
#define ZIS_STUB_LEDLRLEA 173 /* deleteRLEAnchor */
#define ZIS_STUB_LEMKRLET 174 /* makeRLETask */
#define ZIS_STUB_LEDLRLET 175 /* deleteRLETask */
#define ZIS_STUB_LEINRLEE 176 /* initRLEEnvironment */
#define ZIS_STUB_LETMRLEE 177 /* termRLEEnvironment */
#define ZIS_STUB_LEMKFCAA 178 /* makeFakeCAA */
#define ZIS_STUB_LEARTCAA 179 /* abortIfUnsupportedCAA */

/* scheduling 180-189 */

#define ZIS_STUB_SCHZOSWT 180 /* zosWait */
#define ZIS_STUB_SCHZOSWL 181 /* zosWaitList */
#define ZIS_STUB_SCHZOSPT 182 /* zosPost */
#define ZIS_STUB_SCHSRLET 183 /* startRLETask */

/* cmutils 190-209 */

#define ZIS_STUB_CMCPWDK  190 /* cmCopyWithDestinationKey */
#define ZIS_STUB_CMCPWSK  191 /* cmCopyWithSourceKey */
#define ZIS_STUB_CMCPTSSK 192 /* cmCopyToSecondaryWithCallerKey */
#define ZIS_STUB_CMCPFSSK 193 /* cmCopyFromSecondaryWithCallerKey */
#define ZIS_STUB_CMCPTPSK 194 /* cmCopyToPrimaryWithCallerKey */
#define ZIS_STUB_CMCPFPSK 195 /* cmCopyFromPrimaryWithCallerKey */
#define ZIS_STUB_CMCPTHSK 196 /* cmCopyToHomeWithCallerKey */
#define ZIS_STUB_CMCPFHSK 197 /* cmCopyFromHomeWithCallerKey */
#define ZIS_STUB_CMCPYSKA 198 /* cmCopyWithSourceKeyAndALET */
#define ZIS_STUB_CMGAACEE 199 /* cmGetCallerAddressSpaceACEE */
#define ZIS_STUB_CMGTACEE 200 /* cmGetCallerTaskACEE */
#define ZIS_STUB_CMALLOC  201 /* cmAlloc */
#define ZIS_STUB_CMFREE   202 /* cmFree */
#define ZIS_STUB_CMALLOC2 203 /* cmAlloc2 */
#define ZIS_STUB_CMFREE2  204 /* cmFree2 */
#define ZIS_STUB_CMUMKMAP 205 /* makeCrossMemoryMap */
#define ZIS_STUB_CMURMMAP 206 /* removeCrossMemoryMap */
#define ZIS_STUB_CMUMAPP  207 /* crossMemoryMapPut */
#define ZIS_STUB_CMUMAPGH 208 /* crossMemoryMapGetHandle */
#define ZIS_STUB_CMUMAPGT 209 /* crossMemoryMapGet */
#define ZIS_STUB_CMUMAPIT 210 /* crossMemoryMapIterate */

/* recovery 220-239 */

#define ZIS_STUB_RCVRESRR 220 /* recoveryEstablishRouter */
#define ZIS_STUB_RCVRRSRR 221 /* recoveryRemoveRouter */
#define ZIS_STUB_RCVRTRES 222 /* recoveryIsRouterEstablished */
#define ZIS_STUB_RCVRPSFR 223 /* recoveryPush */
#define ZIS_STUB_RCVRPPFR 224 /* recoveryPop */
#define ZIS_STUB_RCVRSDTL 225 /* recoverySetDumpTitle */
#define ZIS_STUB_RCVRSFLV 226 /* recoverySetFlagValue */
#define ZIS_STUB_RCVRECST 227 /* recoveryEnableCurrentState */
#define ZIS_STUB_RCVRDCST 228 /* recoveryDisableCurrentState */
#define ZIS_STUB_RCVRURSI 229 /* recoveryUpdateRouterServiceInfo */
#define ZIS_STUB_RCVRUSSI 230 /* recoveryUpdateStateServiceInfo */
#define ZIS_STUB_RCVRGACD 231 /* recoveryGetABENDCode */
/* #define ZIS_STUB_RCVRNFNE 232 - defunct */
/* #define ZIS_STUB_SHRCVRST 233 - defunct  */ 

/* zis/plugin */

/* zis/service 240-249 pragmaMAP - not #define if no  __LONGNAME__ */

#define ZIS_STUB_ZISCRSVC 240 /* zisCreateService mapped */
#define ZIS_STUB_ZISCSWSV 241 /* zisCreateSpaceSwitchService mapped */
#define ZIS_STUB_ZISCCPSV 242 /* zisCreateCurrentPrimaryService mapped */
#define ZIS_STUB_ZISCSVCA 243 /* zisCreateServiceAnchor mapped */
#define ZIS_STUB_ZISRSVCA 244 /* zisRemoveServiceAnchor mapped */
#define ZIS_STUB_ZISUSVCA 245 /* zisUpdateServiceAnchor mapped */
#define ZIS_STUB_ZISUSAUT 246 /* zisServiceUseSpecificAuth mapped */

/* zosfile, unixfile 250-289 */

#define ZIS_STUB_FILEOPEN 250 /* fileOpen */
#define ZIS_STUB_FILEREAD 251 /* fileRead */
#define ZIS_STUB_FILWRITE 252 /* fileWrite */
#define ZIS_STUB_FILGTCHR 253 /* fileGetChar */
#define ZIS_STUB_FILECOPY 254 /* fileCopy */
#define ZIS_STUB_FILRNAME 255 /* fileRename */
#define ZIS_STUB_FILDLETE 256 /* fileDelete */

#define ZIS_STUB_FILEINFO 260 /* fileInfo */
#define ZIS_STUB_FNFOISDR 261 /* fileInfoIsDirectory */
#define ZIS_STUB_FNFOSIZE 262 /* fileInfoSize */
#define ZIS_STUB_FNFOCCID 263 /* fileInfoCCSID */
#define ZIS_STUB_FILUXCRT 264 /* fileInfoUnixCreationTime */
#define ZIS_STUB_FILISOEF 265 /* fileEOF */
#define ZIS_STUB_FILGINOD 266 /* fileGetINode */
#define ZIS_STUB_FILDEVID 267 /* fileGetDeviceID */
#define ZIS_STUB_FILCLOSE 268 /* fileClose */

/* zis/plugin 290-299 - These are pragmaMAP not ifndef __LONGNAME__ */
/* #define ZIS_STUB_ZISAPLGN 290 does not EXIST */
#define ZIS_STUB_ZISCPLGN 291 /* zisCreatePlugin mapped */
/* #define ZIS_STUB_ZISFPLGN 292 does not EXIST */
#define ZIS_STUB_ZISPLGAS 293 /* zisPluginAddService mapped */
#define ZIS_STUB_ZISPLGCA 294 /* zisCreatePluginAnchor mapped */
#define ZIS_STUB_ZISPLGRM 295 /* zisRemovePluginAnchor mapped */

/* zos 300-329 */

#define ZIS_STUB_GETTCB   300 /* getTCB */
#define ZIS_STUB_GETSTCB  301 /* getSTCB */
#define ZIS_STUB_GETOTCB  302 /* getOTCB */
#define ZIS_STUB_GETASCB  303 /* getASCB */
#define ZIS_STUB_GETASSB  304 /* getASSB */
#define ZIS_STUB_GETASXB  305 /* getASXB */

#define ZIS_STUB_SETKEY   310 /* setKey */

/* crossmemory 330-359 */

/* #define ZIS_STUB_CMINILOG 330 cmsInitializeLogging not in CMS_CLIENT */
/* #define ZIS_STUB_CMMCMSRV 331 makeCrossMemoryServer not in CMS_CLIENT */
/* #define ZIS_STUB_CMMCMSR2 332 makeCrossMemoryServer2  not in CMS_CLIENT */
/* #define ZIS_STUB_CMMCRSRV 333 removeCrossMemoryServer not in CMS_CLIENT */
/* #define ZIS_STUB_CMCMSSPP 334 cmsSetPoolParameters not in CMS_CLIENT */
/* #define ZIS_STUB_CMCMSRSR 335 cmsRegisterService not in CMS_CLIENT */
/* #define ZIS_STUB_CMCMAINL 336 cmsStartMainLoop not in CMS_CLIENT */
#define ZIS_STUB_CMGETGA  337 /* cmsGetGlobalArea */
/* #define ZIS_STUB_CMADDPRM 338 cmsAddConfigParm not in CMS_CLIENT */
#define ZIS_STUB_CMCMSRCS 339 /* cmsCallService */
#define ZIS_STUB_CMCALLS2 340 /* cmsCallService2 */
#define ZIS_STUB_CMCALLS3 341 /* cmsCallService3 */
#define ZIS_STUB_CMCMSPRF 342 /* cmsPrintf */
#define ZIS_STUB_CMCMSVPF 343 /* vcmsPrintf */
#define ZIS_STUB_CMGETPRM 344 /* cmsGetConfigParm */
#define ZIS_STUB_CMGETPRU 345 /* cmsGetConfigParmUnchecked */
#define ZIS_STUB_CMGETLOG 346 /* cmsGetPCLogLevel */
#define ZIS_STUB_CMGETSTS 347 /* cmsGetStatus */
#define ZIS_STUB_CMMKSNAM 348 /* cmsMakeServerName */
/* #define ZIS_STUB_CMECSAAL 349 cmsAllocateECSAStorage not in CMS_CLIENT */
/* #define ZIS_STUB_CMECSAFR 350 cmsFreeECSAStorage not in CMS_CLIENT */
/* #define ZIS_STUB_CMECSAA2 351 cmsAllocateECSAStorage2 not in CMS_CLIENT */
/* #define ZIS_STUB_CMECSAF2 352 cmsFreeECSAStorage2 not in CMS_CLIENT */

/* zis client 360-389 */
#define ZIS_STUB_ZISCSRVC 360 /* zisCallService */
#define ZIS_STUB_ZISGDSNM 361 /* zisGetDefaultServerName */
#define ZIS_STUB_ZISCOPYD 362 /* zisCopyDataFromAddressSpace */
#define ZIS_STUB_ZISUNPWD 363 /* zisCheckUsernameAndPassword */
#define ZIS_STUB_ZISSAUTH 364 /* zisCheckEntity */
#define ZIS_STUB_ZISGETAC 365 /* zisGetSAFAccessLevel */
#define ZIS_STUB_ZISCNWMS 366 /* zisCallNWMService */
#define ZIS_STUB_ZISUPROF 367 /* zisExtractUserProfiles */
#define ZIS_STUB_ZISGRROF 368 /* zisExtractGenresProfiles */
#define ZIS_STUB_ZISGRAST 369 /* zisExtractGenresAccessList */
#define ZIS_STUB_ZISDFPRF 370 /* zisDefineProfile */
#define ZIS_STUB_ZISDLPRF 371 /* zisDeleteProfile */
#define ZIS_STUB_ZISGAPRF 372 /* zisGiveAccessToProfile */
#define ZIS_STUB_ZISRAPRF 373 /* zisRevokeAccessToProfile */
/* #define ZIS_STUB_ZISCAPRF 374 */ /* zisCheckAccessToProfile */
#define ZIS_STUB_ZISXGRPP 375 /* zisExtractGroupProfiles */
#define ZIS_STUB_ZISXGRPA 376 /* zisExtractGroupAccessList */
#define ZIS_STUB_ZISADDGP 377 /* zisAddGroup */
#define ZIS_STUB_ZISDELGP 378 /* zisDeleteGroup */
#define ZIS_STUB_ZISADDCN 379 /* zisConnectToGroup */
#define ZIS_STUB_ZISREMCN 380 /* zisRemoveFromGroup */

/* zvt, xml, json  ? */

// zss/h/zis/parm.h
#define ZIS_STUB_ZISMAKPS 1000 /* zisMakeParmSet */
#define ZIS_STUB_ZISREMPS 1001 /* zisRemoveParmSet */
#define ZIS_STUB_ZISRDLIB 1002 /* zisReadParmlib */
#define ZIS_STUB_ZISRDMPR 1003 /* zisReadMainParms */
#define ZIS_STUB_ZISPUTPV 1004 /* zisPutParmValue */
#define ZIS_STUB_ZISGETPV 1005 /* zisGetParmValue */
#define ZIS_STUB_ZISLOADP 1006 /* zisLoadParmsToCMServer */
#define ZIS_STUB_ZISITERP 1007 /* zisIterateParms */

// zss/h/zis/plugin.h
#define ZIS_STUB_ZISDESTP 1008 /* zisDestroyPlugin */

// zowe-common-c/h/logging.h
#define ZIS_STUB_ZOWEDUMP 1009 /* zowedump */

// zowe-common-c/h/nametoken.h
#define ZIS_STUB_NTPCREF 1010 /* nameTokenCreate */
#define ZIS_STUB_NTPCRTPF 1011 /* nameTokenCreatePersistent */
#define ZIS_STUB_NTPDELF 1012 /* nameTokenDelete */
#define ZIS_STUB_NTPRETF 1013 /* nameTokenRetrieve */

// zowe-common-c/h/crossmemory.h
#define ZIS_STUB_CMSHEXDM 1014 /* cmsHexDump */

// zss/h/zis/client.h
#define ZIS_STUB_ZISCUSVC 1015 /* zisCallServiceUnchecked */

// zowe-common-c/h/zos.h
#define ZIS_STUB_SUPRMODE 1016 /* supervisorMode */
#define ZIS_STUB_ZOSCLCKD 1017 /* isCallerLocked */
#define ZIS_STUB_ZOSCSRB 1018 /* isCallerSRB */
#define ZIS_STUB_GETASCBJ 1019 /* getASCBJobname */ 

// zowe-common-c/h/utils.h
#define ZIS_STUB_SLHFREE 1020 /* SLHFree */

// zowe-common-c/h/bpxnet.h
#define ZIS_STUB_SKTWRITE 1021 /* socketWrite */
#define ZIS_STUB_SKTCLOSE 1022 /* socketClose */
#define ZIS_STUB_SKTREAD 1023 /* socketRead */
#define ZIS_STUB_TCPSERV 1024 /* tcpServer */
#define ZIS_STUB_GETADDBN 1025 /* getAddressByName */


// zowe-common-c/h/collections.h
#define ZIS_STUB_QDEQUEUE 1026 /* qDequeue */
#define ZIS_STUB_QENQUEUE 1027 /* qEnqueue */
#define ZIS_STUB_MAKEQUEU 1028 /* makeQueue */

// zowe-common-c/h/cellpool.h
#define ZIS_STUB_CPFREE 1029 /* cellpoolFree */
#define ZIS_STUB_CPGET 1030 /* cellpoolGet */
#define ZIS_STUB_CPDELETE 1031 /* cellpoolDelete */
#define ZIS_STUB_CPBUILD 1033 /* cellpoolBuild */

// zowe-common-c/h/lpa.h
#define ZIS_STUB_LPAADD 1034 /* lpaAdd */
#define ZIS_STUB_LPADEL 1035 /* lpaDelete */

// zowe-common-c/h/crossmemory.h
#define ZIS_STUB_CMADDPRM 1036 /* cmsAddConfigParm */

#endif

