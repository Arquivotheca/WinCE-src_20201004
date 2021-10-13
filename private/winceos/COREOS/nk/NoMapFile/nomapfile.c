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
/*
 *              NK Kernel loader code
 *
 *
 * Module Name:
 *
 *              mapfile.c
 *
 * Abstract:
 *
 *              This file implements the NK kernel mapped file routines
 *
 */

#include "kernel.h"
#include "pager.h"

const PFNVOID MapMthds[] = {
    (PFNVOID) 0,
    (PFNVOID) 0,
};

const CINFO cinfMap = {0};
DLIST g_MapList;

LPVOID MAPMapView (PPROCESS pprc, HANDLE hMap, DWORD dwAccess, DWORD dwOfstHigh, DWORD dwOfstLow, DWORD cbSize)
{
    return NULL;
}

BOOL MAPUnmapView (PPROCESS pprc, LPVOID pAddr)
{
    return FALSE;
}

BOOL MAPFlushView (PPROCESS pprc, LPVOID pAddr, DWORD cbSize)
{
    return FALSE;
}

HANDLE WINAPI
NKCreateFileMapping (
    PPROCESS pprc,
    HANDLE hFile, 
    LPSECURITY_ATTRIBUTES lpsa, 
    DWORD flProtect, 
    DWORD dwMaximumSizeHigh, 
    DWORD dwMaximumSizeLow, 
    LPCTSTR lpName 
    ) {
    return NULL;
}

void MAPUnmapAllViews (PPROCESS pprc)
{
}

BOOL MapfileInit (void)
{
    InitDList (&g_MapList);
    return FALSE;
}

void PageOutFiles (BOOL fCritical)
{
}

DWORD PageFromMapper (PPROCESS pprc, DWORD dwAddr, BOOL fWrite)
{
    return PAGEIN_FAILURE;
}

// MapVM* functions which are called by other parts of the kernel

BOOL MapVMAddPage (PFSMAP pfsmap, ULARGE_INTEGER liFileOffset, LPVOID pPage, WORD InitialCount)
{
    return FALSE;
}

BOOL MapVMFreePage (PFSMAP pfsmap, ULARGE_INTEGER liFileOffset)
{
    return FALSE;
}

#ifdef ARM
BOOL MAPUncacheViews (PPROCESS pprc, DWORD dwAddr, DWORD cbSize)
{
    return FALSE;
}

#endif

BOOL
RegisterForCaching (
    const FSCacheExports* pFSFuncs,
    NKCacheExports* pNKFuncs
    )
{
    KSetLastError (pCurThread, ERROR_NOT_SUPPORTED);
    return FALSE;
}

void 
WriteBackAllFiles ()
{
    KSetLastError (pCurThread, ERROR_NOT_SUPPORTED);
}
