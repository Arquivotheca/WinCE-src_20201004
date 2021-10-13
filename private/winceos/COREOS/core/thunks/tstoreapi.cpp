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
#include <storemgr.h>


// API required by FindCloseStore and FindClosePartition.
EXTERN_C BOOL WINAPI xxx_FindClose(HANDLE hFindFile);


// Separate helper function required to avoid having try/except and constructor
// inside same context
static HANDLE DoCreateFile(LPTSTR szFileName)
{
    // Always request maximum allowed access when opening a store because the
    // caller has no way of requesting specific rights via OpenStore.
    return CreateFileW_Trap (szFileName, MAXIMUM_ALLOWED, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
}

// Storage Management API's
EXTERN_C HANDLE WINAPI xxx_OpenStore (LPCWSTR szDeviceName)
{
    TCHAR   szFileName[MAX_PATH];
    HRESULT hr;
    
    __try {
        hr = StringCchPrintfW (szFileName, MAX_PATH, L"\\StoreMgr\\%s", szDeviceName);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        hr = STRSAFE_E_INVALID_PARAMETER;
    }
    
    // OpenStore maps to CreateFileW
    return SUCCEEDED (hr)
         ? DoCreateFile(szFileName)
         : (SetLastError(ERROR_INVALID_PARAMETER), INVALID_HANDLE_VALUE);
}

EXTERN_C BOOL WINAPI xxx_DismountStore (HANDLE hStore)
{
    return DismountStore_Trap (hStore);   
}

EXTERN_C BOOL WINAPI xxx_FormatStore (HANDLE hStore)
{
    return FormatStore_Trap (hStore);
}

EXTERN_C HANDLE WINAPI xxx_FindFirstStore (PSTOREINFO pStoreInfo)
{
    // FindFirstStore maps to FindFirstFileW
    return FindFirstFileW_Trap (L"\\StoreMgr", (WIN32_FIND_DATA *)pStoreInfo, sizeof (STOREINFO));
}

EXTERN_C BOOL WINAPI xxx_FindNextStore (HANDLE hSearch, PSTOREINFO pStoreInfo)
{
    return FindNextFileW_Trap (hSearch, (WIN32_FIND_DATA*)pStoreInfo, sizeof(STOREINFO));
}

EXTERN_C BOOL WINAPI xxx_FindCloseStore (HANDLE hSearch)
{
    return xxx_FindClose (hSearch);
}

EXTERN_C BOOL WINAPI xxx_GetStoreInfo (HANDLE hStore, PSTOREINFO pStoreInfo)
{
    return GetStoreInfo_Trap (hStore, pStoreInfo);
}

// Partition Management API's
EXTERN_C BOOL WINAPI xxx_CreatePartition (HANDLE hStore, LPCWSTR szPartitionName, SECTORNUM snNumSectors)
{
    return CreatePartition_Trap (hStore, szPartitionName, 0, (DWORD)(snNumSectors >> 32), (DWORD)snNumSectors, TRUE);
}

EXTERN_C BOOL WINAPI xxx_CreatePartitionEx (HANDLE hStore, LPCWSTR szPartitionName, BYTE bPartType, SECTORNUM snNumSectors)
{
    return CreatePartition_Trap (hStore, szPartitionName, bPartType, (DWORD)(snNumSectors >> 32), (DWORD)snNumSectors, FALSE);
}

EXTERN_C BOOL WINAPI xxx_DeletePartition (HANDLE hStore, LPCWSTR szPartitionName)
{
    return DeletePartition_Trap (hStore, szPartitionName);
}

EXTERN_C HANDLE WINAPI xxx_OpenPartition (HANDLE hStore, LPCWSTR szPartitionName)
{
    HANDLE h = OpenPartition_Trap (hStore, szPartitionName);
    if ((DWORD)0xFFFFFFFF == (DWORD)h) {
        // When passed an invalid handle, this API will route to the kernel's HT_FILE
        // error handler, which will return 0xFFFFFFFF. Fix sign-extension on 64-bit
        // platforms by re-mapping the return value to INVALID_HANDLE_VALUE.
        h = INVALID_HANDLE_VALUE;
    }
    return h;
}

EXTERN_C BOOL WINAPI xxx_MountPartition (HANDLE hPartition)
{
    return MountPartition_Trap (hPartition);
}

EXTERN_C BOOL WINAPI xxx_DismountPartition (HANDLE hPartition)
{
    return DismountPartition_Trap (hPartition);
}

EXTERN_C BOOL WINAPI xxx_RenamePartition (HANDLE hPartition, LPCWSTR szNewName)
{
    return RenamePartition_Trap (hPartition, szNewName);
}

EXTERN_C BOOL WINAPI xxx_SetPartitionAttributes (HANDLE hPartition, DWORD dwAttrs)
{
    return SetPartitionAttributes_Trap (hPartition, dwAttrs);
}

EXTERN_C BOOL WINAPI xxx_GetPartitionInfo(HANDLE hPartition, PPARTINFO pPartInfo)
{
    return GetPartitionInfo_Trap (hPartition, pPartInfo);
}

EXTERN_C BOOL WINAPI xxx_FormatPartition (HANDLE hPartition)
{
    return FormatPartition_Trap (hPartition, 0, TRUE);
}

EXTERN_C BOOL WINAPI xxx_FormatPartitionEx (HANDLE hPartition, BYTE bPartType, BOOL bAuto)
{
    return FormatPartition_Trap (hPartition, bPartType, bAuto);
}

EXTERN_C HANDLE WINAPI xxx_FindFirstPartition (HANDLE hStore, PPARTINFO pPartInfo)
{
    HANDLE h = FindFirstPartition_Trap (hStore, pPartInfo);
    if ((DWORD)0xFFFFFFFF == (DWORD)h) {
        // When passed an invalid handle, this API will route to the kernel's HT_FILE
        // error handler, which will return 0xFFFFFFFF. Fix sign-extension on 64-bit
        // platforms by re-mapping the return value to INVALID_HANDLE_VALUE.
        h = INVALID_HANDLE_VALUE;
    }
    return h;
}

EXTERN_C BOOL WINAPI xxx_FindNextPartition (HANDLE hSearch, PPARTINFO pPartInfo)
{
    return FindNextFileW_Trap (hSearch, (WIN32_FIND_DATA*)pPartInfo, sizeof(PARTINFO));
}

EXTERN_C BOOL WINAPI xxx_FindClosePartition(HANDLE hSearch)
{
    return xxx_FindClose (hSearch);
}

