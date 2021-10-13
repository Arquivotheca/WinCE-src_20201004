//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
#include "udfs.h"

BOOL UDFSMatchesWildcard(DWORD len, LPWSTR lpWild, DWORD len2, LPSTR lpFile, DWORD dwCharSize)
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

