//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*--
Module Name: HTTPSTR.H
Abstract: Common HTTP strings
--*/



//
//    common HTTP headers
//

extern const CHAR  cszAccept_Ranges[];
extern const DWORD ccAccept_Ranges;
extern const CHAR  cszAccept[];
extern const DWORD ccAccept;
extern const CHAR  cszAllow[];
extern const DWORD ccAllow;
extern const CHAR  cszAuthorization[];
extern const DWORD ccAuthorization;
extern const CHAR  cszConnection[];
extern const DWORD ccConnection;
extern const CHAR  cszContent_Length[];
extern const DWORD ccContent_Length;
extern const CHAR  cszContent_Location[];
extern const DWORD ccContent_Location;
extern const CHAR  cszContent_Range[];
extern const DWORD ccContent_Range;
extern const CHAR  cszContent_Type[];
extern const DWORD ccContent_Type;
extern const CHAR  cszCookie[];
extern const DWORD ccCookie;
extern const CHAR  cszDate[];
extern const DWORD ccDate;
extern const CHAR  cszDepth[];
extern const DWORD ccDepth;
extern const CHAR  cszDestination[];
extern const DWORD ccDestination;
extern const CHAR  cszETag[];
extern const DWORD ccETag;
extern const CHAR  cszHost[];
extern const DWORD ccHost;
extern const CHAR  cszIfToken[];
extern const DWORD ccIfToken;
extern const CHAR  cszIf_Match[];
extern const DWORD ccIf_Match;
extern const CHAR  cszIf_Modified_Since[];
extern const DWORD ccIf_Modified_Since;
extern const CHAR  cszIf_None_Match[];
extern const DWORD ccIf_None_Match;
extern const CHAR  cszIf_None_State_Match[];
extern const DWORD ccIf_None_State_Match;
extern const CHAR  cszIf_Range[];
extern const DWORD ccIf_Range;
extern const CHAR  cszIf_State_Match[];
extern const DWORD ccIf_State_Match;
extern const CHAR  cszIf_Unmodified_Since[];
extern const DWORD ccIf_Unmodified_Since;
extern const CHAR  cszLocation[];
extern const DWORD ccLocation;
extern const CHAR  cszLockInfo[];
extern const DWORD ccLockInfo;
extern const CHAR  cszLockToken[];
extern const DWORD ccLockToken;
extern const CHAR  cszOverwrite[];
extern const DWORD ccOverwrite;
extern const CHAR  cszAllowRename[];
extern const DWORD ccAllowRename;
extern const CHAR  cszPublic[];
extern const DWORD ccPublic;
extern const CHAR  cszRange[];
extern const DWORD ccRange;
extern const CHAR  cszReferer[];
extern const DWORD ccReferer;
extern const CHAR  cszServer[];
extern const DWORD ccServer;
extern const CHAR  cszTimeOut[];
extern const DWORD ccTimeOut;
extern const CHAR  cszTransfer_Encoding[];
extern const DWORD ccTransfer_Encoding;
extern const CHAR  cszTranslate[];
extern const DWORD ccTranslate;
extern const CHAR  cszWWW_Authenticate[];
extern const DWORD ccWWW_Authenticate;

extern const CHAR  cszDateFmtGMT[];
extern const CHAR* rgWkday[];
extern const CHAR* rgMonth[];

extern const CHAR  cszNone[];
extern const DWORD ccNone;
extern const CHAR  cszChunked[];
extern const DWORD ccChunked;

extern const CHAR  cszBasic[];
extern const DWORD ccBasic;
extern const CHAR  cszNTLM[];
extern const DWORD ccNTLM;
extern const CHAR  cszNegotiate[];
extern const DWORD ccNegotiate;


extern const CHAR  cszBytes[];
extern const DWORD ccBytes;
extern const CHAR  cszAnd[];
extern const DWORD ccAnd;
extern const CHAR  cszOr[];
extern const DWORD ccOr;
extern const CHAR  cszMS_Author_Via_Dav[];
extern const DWORD ccMS_Author_Via_Dav;


extern const CHAR  cszHttpPrefix[];
extern const DWORD ccHttpPrefix;
extern const CHAR  cszHttpsPrefix[];
extern const DWORD ccHttpsPrefix;


/* "WWW-Authenticate: Basic realm=\"Microsoft-WinCE\"\r\n" unless realm is overridden in registry */
#define BASIC_AUTH_FORMAT                  "%s %s realm=\"%s\""
#define NTLM_AUTH_FORMAT                   "%s %s" 
#define NEGOTIATE_AUTH_FORMAT              "%s %s"

#define BASIC_AUTH_STR(realmInfo)          cszWWW_Authenticate,cszBasic,realmInfo
#define NTLM_AUTH_STR                      cszWWW_Authenticate,cszNTLM
#define NEGOTIATE_AUTH_STR                 cszWWW_Authenticate,cszNegotiate

extern const CHAR  cszRespStatus[];
extern const DWORD ccRespStatus;
extern const CHAR  cszRespServer[];
extern const DWORD ccRespServer;
extern const CHAR  cszRespType[];
extern const DWORD ccRespType;
extern const CHAR  cszRespLength[];
extern const DWORD ccRespLength;

extern const CHAR  cszRespDate[];
extern const DWORD ccRespDate;
extern const CHAR  cszRespLocation[];
extern const DWORD ccRespLocation;

extern const CHAR  cszConnectionResp[];
extern const DWORD ccConnectionResp;
extern const CHAR  cszConnClose[];
extern const DWORD ccConnClose;
extern const CHAR  cszKeepAlive[];
extern const DWORD ccKeepAlive;
extern const CHAR  cszCRLF[];
extern const DWORD ccCRLF;
extern const CHAR  cszRespLastMod[];
extern const DWORD ccRespLastMod;


extern const CHAR  cszOpaquelocktokenPrefix[];
extern const DWORD ccOpaquelocktokenPrefix;
