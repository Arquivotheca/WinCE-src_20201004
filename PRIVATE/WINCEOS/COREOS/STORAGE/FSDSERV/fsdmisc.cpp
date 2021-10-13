//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    fsdmisc.cpp

Abstract:

    This file contains miscellaneous routines.

--*/

#include "fsdmgrp.h"


/*  InitList - Initialize a doubly-linked list
 */

void InitList(PDLINK pdl)
{
    pdl->next = pdl->prev = pdl;
}


/*  IsListEmpty - Return TRUE if list is empty
 */

BOOL IsListEmpty(PDLINK pdl)
{
    return pdl->next == pdl;
}


/*  AddListItem - Add item to list
 */

void AddListItem(PDLINK pdlIns, PDLINK pdlNew)
{
    pdlNew->prev = pdlIns;
    (pdlNew->next = pdlIns->next)->prev = pdlNew;
    pdlIns->next = pdlNew;
}


/*  RemoveListItem - Remove item from list
 */

void RemoveListItem(PDLINK pdl)
{
    pdl->prev->next = pdl->next;
    pdl->next->prev = pdl->prev;
}


/*  CompareFSDs - Compare the identity of two FSDs
 *
 *  ENTRY
 *      hFSD1 - handle to first FSD
 *      hFSD2 - handle to second FSD
 *
 *  EXIT
 *      TRUE if match, FALSE if not
 */

BOOL CompareFSDs(HMODULE hFSD1,HMODULE hFSD2)
{
#ifdef DEBUG
    DWORD cch1, cch2;
    WCHAR wsTmp1[MAX_PATH];
    WCHAR wsTmp2[MAX_PATH];

    cch1 = GetModuleFileName(hFSD1, wsTmp1, ARRAY_SIZE(wsTmp1));
    cch2 = GetModuleFileName(hFSD2, wsTmp2, ARRAY_SIZE(wsTmp2));

    ASSERT(hFSD1 != hFSD2 || cch1 == cch2 && wcsicmp(wsTmp1, wsTmp2) == 0);
#endif
    return hFSD1 == hFSD2;
}


/*  GetFSDProcAddress - Get a procedure address for an FSD
 *
 *  ENTRY
 *      pFSD -> FSD structure
 *      pwsBaseFunc -> base procedure name
 *
 *  EXIT
 *      Procedure address, or NULL if it doesn't exist
 */

FARPROC GetFSDProcAddress(PFSD pFSD, PCWSTR pwsBaseFunc)
{
    FARPROC lpFunc;
    WCHAR wsTmp[MAX_PATH];

    if (lpFunc = FsdGetProcAddress( pFSD->hFSD, pwsBaseFunc)) {
        return lpFunc;
    }

    wcsncpy(wsTmp, pFSD->wsFSD, MAX_PATH);
    wsTmp[MAX_PATH-1] = L'\0';
    wcsncat(wsTmp, pwsBaseFunc, MAX_PATH - wcslen(wsTmp));
    wsTmp[MAX_PATH-1] = L'\0';

    return FsdGetProcAddress(pFSD->hFSD, wsTmp);
}


/*  GetFSDProcArray - Get a set of procedure addresses for an FSD
 *
 *  ENTRY
 *      pFSD -> FSD structure
 *      apfnFuncs -> array of procedure addresses to be filled in
 *      apfnStubs -> array of stub procedures to be used in case a procedure doesn't exist
 *      apwsBaseFuncs -> array of base procedure names (NULL if no function required for this entry)
 *      cFuncs == number of entries in all the above arrays
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (eg, a required function is missing
 *      and no stub has been provided either).
 */

BOOL GetFSDProcArray(PFSD pFSD, PFNAPI *apfnFuncs, CONST PFNAPI *apfnStubs, CONST PCWSTR *apwsBaseFuncs, int cFuncs)
{
    int i;

    for (i=0; i<cFuncs; i++) {
        // If a function name has been supplied and no function address is known yet, look it up
        if (apwsBaseFuncs[i] != NULL && apfnFuncs[i] == NULL) {
            apfnFuncs[i] = (PFNAPI)GetFSDProcAddress(pFSD, apwsBaseFuncs[i]);
            // If the look-up failed, then if a stub is available, use that instead;  otherwise abort
            if (apfnFuncs[i] == NULL) {
                if (apfnStubs[i])
                    apfnFuncs[i] = apfnStubs[i];
                else
                    return FALSE;       // give up as soon as any required (ie, non-stubbed) function is missing
            }
        }
    }
    return TRUE;
}


/*  GetAFSName - Get the name currently associated with an AFS index
 *
 *  ENTRY
 *      iAFS == AFS index
 *      pwsAFS -> buffer, or NULL to query size of name
 *      cchMax == max chars allowed in buffer (including terminating NULL), ignored if pwsAFS is NULL
 *
 *  EXIT
 *      Length of AFS name in characters, NOT including terminating NULL, or 0 if error
 */
#ifdef UNDER_CE
int GetAFSName(int iAFS, PWSTR pwsAFS, int cchMax)
{
    CEGUID SystemGUID;
    DWORD dwError = ERROR_SUCCESS;
    int cch;
    CEOIDINFOEX oi;

    memset( &SystemGUID, 0, sizeof(CEGUID));
    memset( &oi, 0, sizeof(CEOIDINFOEX));
    oi.wVersion = CEOIDINFOEX_VERSION;
    if (!CeOidGetInfoEx2(&SystemGUID, OIDFROMAFS(iAFS), &oi)) {
        dwError = GetLastError();
    }    
    if (dwError != ERROR_INVALID_PARAMETER) {
        cch = wcslen(oi.infDirectory.szDirName);
        if (1 == cch) {
            // this the root file system, remove the slash from the name before passing to the FSD
            DEBUGCHK((L'\\' == oi.infDirectory.szDirName[0]) || (L'/' == oi.infDirectory.szDirName[0]));
            cch = 0;
            oi.infDirectory.szDirName[0] = L'\0';
        }
        if (pwsAFS) {
            if (cch >= cchMax)
                cch = cchMax-1;
            memcpy(pwsAFS, oi.infDirectory.szDirName, cch*sizeof(oi.infDirectory.szDirName[0]));
            pwsAFS[cch] = 0;
        }
    }    
    return (dwError == ERROR_SUCCESS) ? cch : 0;
}
#endif
