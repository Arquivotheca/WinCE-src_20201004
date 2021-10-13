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
//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//
//  File:       U T I L S . C P P
//
//  Contents:   Functions to send and receive (process) subscription requests
//
//  Notes:
//
//  Author:     danielwe   7 Oct 1999
//
//----------------------------------------------------------------------------

#include <ssdppch.h>
#pragma hdrstop

#include <httpext.h>

#include "ssdpparser.h"
#include "ssdpnetwork.h"

#include "HttpChannel.h"
#include "http_status.h"
#include "url_verifier.h"
#include "upnp_config.h"
#include "dlna.h"

const CHAR c_szUrlPrefix[]             = "http://";


//+---------------------------------------------------------------------------
//
//  Function:   DwParseTime
//
//  Purpose:    Parses the Timeout header for a subscription
//
//  Arguments:
//      szTime [in]     Timeout value in the format defined by RFC2518
//
//  Returns:    Timeout value in SECONDS
//
//  Author:     danielwe   13 Oct 1999
//
//  Notes:  NYI
//
DWORD DwParseTime(LPCSTR szTime)
{
    DWORD dwTimeoutSeconds;

    if(0 == strcmp(szTime, "Second-infinite"))
    {
        dwTimeoutSeconds = upnp_config::max_sub_timeout();
    }
    else
    {
        if(1 != sscanf(szTime, "Second-%d", &dwTimeoutSeconds))
        {
            dwTimeoutSeconds = upnp_config::default_sub_timeout();
        }

        if(dwTimeoutSeconds < upnp_config::min_sub_timeout())
        {
            dwTimeoutSeconds = upnp_config::min_sub_timeout();
        }

        if(dwTimeoutSeconds > upnp_config::max_sub_timeout())
        {
            dwTimeoutSeconds = upnp_config::max_sub_timeout();
        }
    }
            
    return dwTimeoutSeconds;
}


//+---------------------------------------------------------------------------
//
//  Function:   ParseCallbackUrl
//
//  Purpose:    Given the Callback URL header value, determines the first
//              http:// URL.
//
//  Arguments:
//      szCallbackUrl [in]  URL to process (comes from Callback: header)
//      pszOut        [out] Returns the callback URL
//
//  Returns:    TRUE if URL format is valid, FALSE if not or if out of
//              memory.
//
//  Author:     danielwe   13 Oct 1999
//
//  Notes:
//
BOOL ParseCallbackUrl(LPCSTR szCallbackUrl, LPSTR *pszOut)
{
    CONST INT   c_cchPrefix = strlen(c_szUrlPrefix);
    LPSTR      szTemp;
    LPSTR      pchPos;
    LPSTR      szOrig = NULL;
    BOOL        fResult = FALSE;

    // NOTE: This function will return http:// as a URL.. Corner case, but
    // not catastrophic

    Assert(szCallbackUrl);
    Assert(pszOut);

    *pszOut = NULL;

    // Copy the original URL so we can lowercase
    //
    szTemp = _strdup(szCallbackUrl);
    szOrig = szTemp;
    _strlwr(szTemp);

    // Look for http://
    //
    pchPos = strstr(szTemp, c_szUrlPrefix);
    if (pchPos)
    {
        // Look for the closing '>'
        szTemp = pchPos + c_cchPrefix;
        while (*szTemp && *szTemp != '>')
        {
            szTemp++;
        }

        if (*szTemp)
        {
            ULONG     cchOut;

            Assert(*szTemp == '>');

            // Allocate space for the URL, and copy it in
            //
            cchOut = szTemp - pchPos;   //georgej: removed + 1
            __assume(cchOut < ULONG_MAX);
            *pszOut = (LPSTR) malloc(cchOut + 1);
            if (*pszOut)
            {
                strncpy(*pszOut, szCallbackUrl + (pchPos - szOrig), cchOut);
                (*pszOut)[cchOut] = 0;
                fResult = TRUE;
            }
        }
    }

    free(szOrig);

    TraceResult("ParseCallbackUrl", fResult);
    return fResult;
}


//+---------------------------------------------------------------------------
//
//  Function:   SzGetNewSid
//
//  Purpose:    Returns a new "uuid:<sid>" identifier
//
//  Arguments:
//      (none)
//
//  Returns:    Newly allocated SID string
//
//  Author:     danielwe   13 Oct 1999
//
//  Notes:      Caller must free the returned string
//
LPSTR SzGetNewSid()
{
#ifndef UNDER_CE
    CHAR           szSid[256];
    UUID            uuid;
    unsigned char * szUuid;

    UuidCreate(&uuid);
    UuidToStringA(&uuid, &szUuid);
    sprintf(szSid, "uuid:%s", szUuid);
    RpcStringFreeA(&szUuid);
    return _strdup(szSid);
#else
    PCHAR   pszSid = (PCHAR)malloc(42);
    LONGLONG uuid64 = GenerateUUID64();
    if (pszSid)
    {
        // put guid64 into string.  Next byte contains variant 10xxxxxxb.
        sprintf(pszSid, "uuid:%04x%04x-%04x-%04x-a000-000000000000", (WORD)uuid64, *((WORD*)&uuid64 + 1), *((WORD*)&uuid64 + 2), *((WORD*)&uuid64 + 3));
    }
    return pszSid;
#endif
}


bool EncodeForXML(LPCWSTR pwsz, ce::wstring* pstr)
{
    wchar_t        aCharacters[] = {L'<', L'>', L'\''};
    wchar_t*    aEntityReferences[] = {L"&lt;", L"&gt;", L"&apos;"};
    bool        bReplaced;
    
    pstr->reserve(static_cast<ce::wstring::size_type>(1.1 * wcslen(pwsz)));
    pstr->resize(0);
    
    for(const wchar_t* pwch = pwsz; *pwch; ++pwch)
    {
        bReplaced = false;

        for(int i = 0; i < sizeof(aCharacters)/sizeof(*aCharacters); ++i)
            if(*pwch == aCharacters[i])
            {
                if(!pstr->append(aEntityReferences[i]))
                {
                    return false;
                }
                bReplaced = true;
                break;
            }

        if(!bReplaced)
        {
            if(!pstr->append(pwch, 1))
            {
                return false;
            }
        }
    }

    return true;
}


