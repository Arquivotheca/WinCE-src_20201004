//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _TOKEN_PRIV_H_
#define _TOKEN_PRIV_H_

#include <adb.h>
#include <adbi.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOKENINFO ADBI_SECURITY_INFO
typedef TOKENINFO *PTOKENINFO;
typedef const TOKENINFO *PCTOKENINFO;

typedef BOOL (* PFN_ENUMTOKEN) (PCCESID pSid, LPVOID pUserData);

// macros to access field in SAMUSRINFO
#define FirstSidOfToken(pTok)      FirstSidOfPSI(pTok)
#define NextSidOfToken(pSid)       NextSid(pSid)
#define ExPrivOfToken(pTok)        ExPrivOfPSI(pTok)

//
// function to enumerate Token SIDS, inlined for it can be used in multiple places
//
__inline BOOL EnumerateToken (PCTOKENINFO pTok, PFN_ENUMTOKEN pfnEnum, LPVOID pUserData)
{
    PCCESID pSid = FirstSidOfToken (pTok);
    DWORD i;
    // totoal number of sids is pTok->cAllGroups + 1
    for (i = 0; i <= pTok->cAllGroups; i ++, pSid = NextSidOfToken (pSid)) {
        if (!pfnEnum (pSid, pUserData)) {
            return FALSE;
        }
    }
    return TRUE;
}

__inline PCTOKENINFO GetCurrentTokenPtr (void)
{
    return (PCTOKENINFO) UserKInfo[KINX_PTR_CURTOKEN];
}

#ifdef __cplusplus
}
#endif


#endif // _TOKEN_PRIV_H_

