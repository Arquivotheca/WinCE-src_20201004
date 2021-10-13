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
    if (1 == dwCharSize)
    {
        BYTE pbWild[MAX_PATH];
        int cbWild = WideCharToMultiByte(CP_OEMCP, 0, lpWild, len, reinterpret_cast<LPSTR>(pbWild), MAX_PATH, NULL, NULL);

        if (cbWild)
        {
            return UDFSMatchesWildcardA(cbWild, reinterpret_cast<LPSTR>(pbWild), len2, lpFile);
        }
    }

    return UDFSMatchesWildcardW(len, lpWild, len2, reinterpret_cast<LPWSTR>(lpFile));
}

BOOL UDFSMatchesWildcardW( DWORD len, 
                          __in_ecount(len) LPWSTR lpWild, 
                          DWORD len2, 
                          __in_ecount(len2) LPWSTR lpFile )
{
    while (len && len2) {
        if (*lpWild == L'?') {
            // skip current target character
            len2--;
            lpFile++;
            len--;
            lpWild++;
            continue;
        }
        if (*lpWild == L'*') {
            lpWild++;
            if (--len) {
                while (len2) {
                    if (UDFSMatchesWildcardW(len,lpWild,len2,lpFile)) // recurse
                        return TRUE;
                    len2--;
                    lpFile++;
                }
                return FALSE;
            }
            return TRUE;
        }
        else if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, lpWild, 1, lpFile, 1) != CSTR_EQUAL) {
            return FALSE;
        }
        len--;
        lpWild++;
        len2--;
        lpFile++;
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
        if (*lpWild != L'*' && *lpWild != L'?') {
            return FALSE;
        }
        lpWild++;
    }
    return TRUE;
}

BOOL UDFSMatchesWildcardA( DWORD len, 
                          __in_ecount(len) LPSTR lpWild, 
                          DWORD len2, 
                          __in_ecount(len2) LPSTR lpFile )
{
    while (len && len2) {
        if (*lpWild == '?') {
            // skip current target character
            len2--;
            lpFile++;
            len--;
            lpWild++;
            continue;
        }
        if (*lpWild == '*') {
            lpWild++;
            if (--len) {
                while (len2) {
                    if (UDFSMatchesWildcardA(len,lpWild,len2,lpFile)) // recurse
                        return TRUE;
                    len2--;
                    lpFile++;
                }
                return FALSE;
            }
            return TRUE;
        }
        else if (tolower(*lpFile) != tolower(*lpWild)) {
            return FALSE;
        }
        len--;
        lpWild++;
        len2--;
        lpFile++;
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
        if (*lpWild != '*' && *lpWild != '?') {
            return FALSE;
        }
        lpWild++;
    }
    return TRUE;
}

