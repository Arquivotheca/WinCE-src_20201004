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
//+-------------------------------------------------------------------------
//
//
//  File:       
//          udfshelper.cpp
//
//  Contents:   
//          This file implements various utility functions.
//
//  Functions:
//          UDFSMatchesWildcard
//
//--------------------------------------------------------------------------
#include "cdfs.h"

BOOL UDFSMatchesWildcard( DWORD len, 
                          __in_ecount(len) LPWSTR lpWild, 
                          DWORD len2, 
                          __in_ecount(len2) LPSTR lpFile, 
                          DWORD dwCharSize )
{
    while (len && len2) {
        if (*lpWild == L'?') {
            // skip current target character
            len2--;
            lpFile += dwCharSize;
            len--;
            lpWild++;
            continue;
        }
        if (*lpWild == L'*') {
            lpWild++;
            if (--len) {
                while (len2) {
                    if (UDFSMatchesWildcard(len,lpWild,len2,lpFile, dwCharSize)) // recurse
                        return TRUE;
                    len2 --;
                    lpFile += dwCharSize;
                }
                return FALSE;
            }
            return TRUE;
        }
        else if (dwCharSize == 1) {
            // compare current wide wildcard character with current narrow target character
            if ((*lpWild >= 256) || (tolower(*lpFile) != tolower(*lpWild))) {
                return FALSE;
            }
        }
        else if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, lpWild, 1, (LPWSTR)lpFile, 1) != CSTR_EQUAL) {
            return FALSE;
        }
        len--;
        lpWild++;
        len2 --;
        lpFile += dwCharSize;
    }
    // target matches wildcard mask, succeed
    if (!len && !len2) {
        return TRUE;
    }
    // wildcard mask has been spent and target has characters remaining, fail
    if (!len) {
        return FALSE;
    }
    // target has been spent; succeed only if wildcard characters remain
    while (len--) {
        if (*lpWild != L'*' && *lpWild != '?') {
            return FALSE;
        }
        lpWild++;
    }
    return TRUE;
}

