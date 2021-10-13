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
#define BUILDING_FS_STUBS
#include <windows.h>

// From fsmain thunks
extern "C" CEGUID SystemGUID;

extern "C" HANDLE xxx_CeOpenDatabaseEx2(PCEGUID  pguid, PCEOID poid, LPWSTR lpszName, SORTORDERSPECEX* pSort, DWORD dwFlags, CENOTIFYREQUEST* pRequest);
extern "C"  HANDLE xxx_CeFindFirstDatabaseEx(PCEGUID pguid, DWORD dwClassID);




// Helper for converting between SORTORDERSPEC versions.
// ToEx is lossless
static void CopySortsToEx(SORTORDERSPECEX* pDest, SORTORDERSPEC* pSrc, WORD wNumSorts)
{
    WORD wSort;

    for (wSort = 0; (wSort < wNumSorts) && (wSort < CEDB_MAXSORTORDER); wSort++) {
        pDest->wVersion = SORTORDERSPECEX_VERSION;
        pDest->wNumProps = 1;
        pDest->rgPropID[0] = pSrc->propid;
        pDest->rgdwFlags[0] = pSrc->dwFlags;

        // Since there is only one prop in the Ex version, the unique flag
        // is kept there.  But in the Ex2 version, it's in the keyflags.
        if (pDest->rgdwFlags[0] & CEDB_SORT_UNIQUE) {
            pDest->rgdwFlags[0] &= ~CEDB_SORT_UNIQUE;
            pDest->wKeyFlags = CEDB_SORT_UNIQUE;
        } else {
            pDest->wKeyFlags = 0;
        }

        pDest++;
        pSrc++;
    }
}

// Helper for converting between SORTORDERSPEC versions.  Exported by tfsmain.c.
// FromEx could throw away some information
extern void CopySortsFromEx(SORTORDERSPEC* pDest, SORTORDERSPECEX* pSrc, WORD wNumSorts);


//
// SDK exports
//


extern "C"
HANDLE xxx_CeFindFirstDatabase(DWORD dwClassID)
{
    return CeFindFirstDatabaseEx_Trap(&SystemGUID, dwClassID);
}

//------------------------------------------------------------------------------
// CeFindNextDatabase

extern "C"
CEOID xxx_CeFindNextDatabase(HANDLE hEnum)
{
    return CeFindNextDatabaseEx_Trap(hEnum, NULL);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeCreateDatabase

// Separate helper function required to avoid having try/except and constructor
// inside same context
static CEOID DoCreateDatabaseEx2(PCEGUID pguid, CEDBASEINFOEX *pexInfo)
{
    return CeCreateDatabaseEx2_Trap(pguid, pexInfo);;
}

extern "C"
CEOID xxx_CeCreateDatabaseEx(PCEGUID pguid, CEDBASEINFO *pInfo)
{
    CEDBASEINFOEX exInfo;

    __try {

        // wNumRecords, dwSize, ftLastModified do not need to be copied

        exInfo.wVersion = CEDBASEINFOEX_VERSION;
        exInfo.dwFlags = pInfo->dwFlags;
        VERIFY (SUCCEEDED (StringCchCopyW (exInfo.szDbaseName, _countof(pInfo->szDbaseName), pInfo->szDbaseName)));
        exInfo.dwDbaseType = pInfo->dwDbaseType;
        exInfo.wNumSortOrder = pInfo->wNumSortOrder;

        CopySortsToEx(exInfo.rgSortSpecs, pInfo->rgSortSpecs, exInfo.wNumSortOrder);

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    return DoCreateDatabaseEx2(pguid, &exInfo);
}

extern "C"
CEOID xxx_CeCreateDatabase(LPWSTR lpszName, DWORD dwClassID, WORD wNumSortOrder,
                           SORTORDERSPEC *rgSortSpecs)
{
    CEDBASEINFOEX exInfo;

    __try {

        // wNumRecords, dwSize, ftLastModified do not need to be assigned

        exInfo.wVersion = CEDBASEINFOEX_VERSION;
        exInfo.dwFlags = CEDB_VALIDCREATE;
        exInfo.dwDbaseType = dwClassID;
        VERIFY (SUCCEEDED (StringCchCopyW (exInfo.szDbaseName, _countof(exInfo.szDbaseName), lpszName)));
        exInfo.wNumSortOrder = wNumSortOrder;

        CopySortsToEx(exInfo.rgSortSpecs, rgSortSpecs, wNumSortOrder);

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    return DoCreateDatabaseEx2(&SystemGUID, &exInfo);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeSetDatabaseInfo

// Separate helper function required to avoid having try/except and constructor
// inside same context
static CEOID DoSetDatabaseInfoEx2(PCEGUID pguid, CEOID oidDbase, CEDBASEINFOEX *pexInfo)
{
    return CeSetDatabaseInfoEx2_Trap(pguid, oidDbase, pexInfo);
}

extern "C"
BOOL xxx_CeSetDatabaseInfoEx(PCEGUID pguid, CEOID oidDbase, CEDBASEINFO *pNewInfo)
{
    CEDBASEINFOEX exInfo;

    __try {

        // wNumRecords, dwSize do not need to be copied

        exInfo.wVersion = CEDBASEINFOEX_VERSION;
        exInfo.dwFlags = pNewInfo->dwFlags;
        VERIFY (SUCCEEDED (StringCchCopyW (exInfo.szDbaseName, _countof(pNewInfo->szDbaseName), pNewInfo->szDbaseName)));
        exInfo.dwDbaseType = pNewInfo->dwDbaseType;
        exInfo.wNumSortOrder = pNewInfo->wNumSortOrder;
        exInfo.ftLastModified.dwHighDateTime = pNewInfo->ftLastModified.dwHighDateTime;
        exInfo.ftLastModified.dwLowDateTime = pNewInfo->ftLastModified.dwLowDateTime;        

        CopySortsToEx(exInfo.rgSortSpecs, pNewInfo->rgSortSpecs, exInfo.wNumSortOrder);

    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    return DoSetDatabaseInfoEx2(pguid, oidDbase, &exInfo);
}

extern "C"
BOOL xxx_CeSetDatabaseInfo(CEOID oidDbase, CEDBASEINFO *pNewInfo)
{
    return xxx_CeSetDatabaseInfoEx(&SystemGUID, oidDbase, pNewInfo);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeOpenDatabase


extern "C"
HANDLE
xxx_CeOpenDatabaseEx(
                     PCEGUID             pguid,
                     PCEOID              poid,
                     LPWSTR              lpszName,
                     CEPROPID            propid,
                     DWORD               dwFlags,
                     CENOTIFYREQUEST*    pRequest
                     )
{
    SORTORDERSPECEX sort;

    sort.wVersion = SORTORDERSPECEX_VERSION;
    sort.wNumProps = 1;
    sort.rgPropID[0] = propid;
    sort.rgdwFlags[0] = 0; // No flags to pass

    return xxx_CeOpenDatabaseEx2(pguid, poid, lpszName, &sort, dwFlags, pRequest);
}

extern "C"
HANDLE xxx_CeOpenDatabase(PCEOID poid, LPWSTR lpszName, CEPROPID propid,
                          DWORD dwFlags, HWND hwndNotify)
{
    SORTORDERSPECEX sort;
    CENOTIFYREQUEST req;

    sort.wVersion = SORTORDERSPECEX_VERSION;
    sort.wNumProps = 1;
    sort.rgPropID[0] = propid;
    sort.rgdwFlags[0] = 0; // No flags to pass

    req.dwSize = sizeof(req);
    req.hwnd = hwndNotify;
    req.dwFlags = 0;
    req.hHeap = 0;

    return xxx_CeOpenDatabaseEx2(&SystemGUID, poid, lpszName, &sort, dwFlags, &req);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// CeDeleteDatabase

extern "C"
BOOL xxx_CeDeleteDatabase(CEOID oid)
{
    return CeDeleteDatabaseEx_Trap(&SystemGUID, oid);
}


extern "C"
BOOL xxx_CeFreeNotification(PCENOTIFYREQUEST pRequest, PCENOTIFICATION pNotify)
{
    return CeFreeNotification_Trap(pRequest, pNotify, pNotify->dwSize);
}

