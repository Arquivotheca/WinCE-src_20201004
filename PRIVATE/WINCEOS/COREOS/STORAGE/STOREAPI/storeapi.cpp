//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <storemgr.h>

BOOL IsStorageManagerRunning()
{
    if ((GetFileAttributesW( L"\\StoreMgr") != -1) &&
        (GetFileAttributesW( L"\\StoreMgr") & FILE_ATTRIBUTE_TEMPORARY)) {
        return TRUE;
    } 
    return FALSE;
}

// Storage Management API's
HANDLE WINAPI OpenStore(LPCTSTR szDeviceName)
{
    TCHAR szFileName[MAX_PATH];
    memset( szFileName, 0, sizeof(szFileName));
    wcscpy( szFileName, L"\\StoreMgr\\");
    __try {
        wcsncat( szFileName, szDeviceName, MAX_PATH-15);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        SetLastError(ERROR_BAD_ARGUMENTS);
    }
    return CreateFile( szFileName, 0, 0, NULL, 0, 0, NULL);
}

BOOL   WINAPI DismountStore(HANDLE hStore)
{
    return (PSLDismountStore(hStore) == TRUE) ? TRUE : FALSE;
}

BOOL   WINAPI FormatStore(HANDLE hStore)
{
    return (PSLFormatStore(hStore) == TRUE) ? TRUE : FALSE;
}

HANDLE WINAPI FindFirstStore(PSTOREINFO pStoreInfo)
{
    return (FindFirstFileW( L"\\StoreMgr", (WIN32_FIND_DATA *)pStoreInfo));
}

BOOL   WINAPI FindNextStore(HANDLE hSearch, PSTOREINFO pStoreInfo)
{
    return (PSLFindNextStore( hSearch, pStoreInfo) == TRUE) ? TRUE : FALSE;
}

BOOL   WINAPI FindCloseStore(HANDLE hSearch)
{
    return (PSLFindCloseStore( hSearch) == TRUE) ? TRUE : FALSE;
}

BOOL   WINAPI GetStoreInfo(HANDLE hStore, PSTOREINFO pStoreInfo)
{
    return (PSLGetStoreInfo( hStore, pStoreInfo) == TRUE) ? TRUE : FALSE;
}

// Partition Management API's
BOOL   WINAPI CreatePartition(HANDLE hStore, LPCTSTR szPartitionName, SECTORNUM snNumSectors)
{
    return (PSLCreatePart( hStore, szPartitionName, 0, (DWORD)(snNumSectors >> 32), (DWORD)snNumSectors, TRUE) == TRUE) ? TRUE : FALSE;
}

BOOL WINAPI CreatePartitionEx(HANDLE hStore, LPCTSTR szPartitionName, BYTE bPartType, SECTORNUM snNumSectors)
{
    return (PSLCreatePart( hStore, szPartitionName, bPartType, (DWORD)(snNumSectors >> 32), (DWORD)snNumSectors, FALSE) == TRUE) ? TRUE : FALSE;
}

BOOL   WINAPI DeletePartition(HANDLE hStore, LPCTSTR szPartitionName)
{
    return (PSLDeletePartition(hStore, szPartitionName) == TRUE) ? TRUE : FALSE;
}

HANDLE WINAPI OpenPartition( HANDLE hStore, LPCTSTR szPartitionName)
{
    return PSLOpenPartition( hStore, szPartitionName);
}

BOOL   WINAPI MountPartition(HANDLE hPartition)
{
    return (PSLMountPartition( hPartition) == TRUE) ? TRUE : FALSE;
}

BOOL   WINAPI DismountPartition(HANDLE hPartition)
{
    return (PSLDismountPartition( hPartition) == TRUE) ? TRUE : FALSE;
}

BOOL   WINAPI RenamePartition(HANDLE hPartition, LPCTSTR szNewName)
{
    return (PSLRenamePartition( hPartition, szNewName) == TRUE) ? TRUE : FALSE;
}

BOOL   WINAPI SetPartitionAttributes(HANDLE hPartition, DWORD dwAttrs)
{
    return (PSLSetPartitionAttributes( hPartition, dwAttrs) == TRUE) ? TRUE : FALSE;
}

BOOL   WINAPI GetPartitionInfo(HANDLE hPartition, PPARTINFO pPartInfo)
{
    return (PSLGetPartitionInfo( hPartition, pPartInfo) == TRUE) ? TRUE : FALSE;
}

BOOL   WINAPI FormatPartition(HANDLE hPartition)
{
    return (PSLFormatPart( hPartition, 0, TRUE) == TRUE) ? TRUE : FALSE;
}

BOOL   WINAPI FormatPartitionEx(HANDLE hPartition, BYTE bPartType, BOOL bAuto)
{
    return (PSLFormatPart( hPartition, bPartType, FALSE) == TRUE) ? TRUE : FALSE;
}

HANDLE WINAPI FindFirstPartition(HANDLE hStore, PPARTINFO pPartInfo)
{
    return PSLFindFirstPartition( hStore, pPartInfo);
}

BOOL   WINAPI FindNextPartition(HANDLE hSearch, PPARTINFO pPartInfo)
{
    return (PSLFindNextPartition( hSearch, pPartInfo) == TRUE) ? TRUE : FALSE;
}

BOOL   WINAPI FindClosePartition(HANDLE hSearch)
{
    return (PSLFindClosePartition( hSearch) == TRUE) ? TRUE : FALSE;
}


