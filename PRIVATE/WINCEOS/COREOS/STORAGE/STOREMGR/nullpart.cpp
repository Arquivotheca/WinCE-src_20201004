//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <storemain.h>

DWORD PD_OpenStore(HANDLE hDisk, PDWORD pdwStoreId)
{
    *pdwStoreId = (DWORD)hDisk;
    return ERROR_SUCCESS;
}

void PD_CloseStore(DWORD dwStoreId)
{
}

DWORD PD_FormatStore(DWORD dwStoreId)
{
    return ERROR_GEN_FAILURE;
}

DWORD PD_IsStoreFormatted(DWORD dwStoreId)
{
    return ERROR_SUCCESS;
}

DWORD PD_GetStoreInfo(DWORD dwStoreId, PD_STOREINFO *pInfo)
{
    DISK_INFO di;
    DWORD dwRet;
    memset( &di, 0, sizeof(DISK_INFO));
    memset( pInfo, 0, sizeof(PD_STOREINFO));
    pInfo->cbSize = sizeof(PD_STOREINFO);
    if (!DeviceIoControl((HANDLE)dwStoreId, IOCTL_DISK_GETINFO, &di, sizeof(DISK_INFO), &di, sizeof(DISK_INFO), &dwRet, NULL)) {
        DeviceIoControl((HANDLE)dwStoreId, DISK_IOCTL_GETINFO, &di, sizeof(DISK_INFO), &di, sizeof(DISK_INFO), &dwRet, NULL);
    }
    pInfo->dwBytesPerSector = di.di_bytes_per_sect;
    pInfo->snNumSectors = di.di_total_sectors;
    return TRUE;
}

DWORD PD_CreatePartition(DWORD dwStoreId, LPCTSTR szPartName, BYTE bPartType, SECTORNUM numSectors, BOOL bAuto)
{
    return ERROR_GEN_FAILURE;
}

DWORD PD_DeletePartition(DWORD dwStoreId, LPCTSTR szPartName)
{
    return ERROR_GEN_FAILURE;
}

DWORD PD_RenamePartition(DWORD dwStoreId, LPCTSTR szOldName, LPCTSTR szNewName)
{
    return ERROR_GEN_FAILURE;
}

DWORD PD_SetPartitionAttrs(DWORD dwStoreId, LPCTSTR szPartName, DWORD dwAttr)
{
    return ERROR_GEN_FAILURE;
}

DWORD PD_FormatPartition(DWORD dwStoreId, LPCTSTR szPartName, BYTE bPartType, BOOL bAuto)
{
    return ERROR_GEN_FAILURE;
}
DWORD PD_GetPartitionInfo(DWORD dwStoreId, LPCTSTR szPartName, PD_PARTINFO *pInfo)
{
    DISK_INFO di;
    DWORD dwRet;
    memset( &di, 0, sizeof(DISK_INFO));
    memset( pInfo, 0, sizeof(PD_PARTINFO));
    pInfo->cbSize = sizeof(PD_PARTINFO);
    if (!DeviceIoControl((HANDLE)dwStoreId, IOCTL_DISK_GETINFO, &di, sizeof(DISK_INFO), &di, sizeof(DISK_INFO), &dwRet, NULL)) {
        DeviceIoControl((HANDLE)dwStoreId, DISK_IOCTL_GETINFO, &di, sizeof(DISK_INFO), &di, sizeof(DISK_INFO), &dwRet, NULL);
    }
    pInfo->snNumSectors = di.di_total_sectors;
    pInfo->bPartType = 0x4;
    return TRUE;
}

DWORD PD_FindPartitionStart(DWORD dwStoreId, PDWORD pdwSearchId)
{
    DWORD *pdwStoreId = new DWORD;
    if (!pdwStoreId) {
        return ERROR_OUTOFMEMORY;
    }    
    *pdwStoreId = dwStoreId;
    *pdwSearchId = (DWORD)pdwStoreId;
    return ERROR_SUCCESS;
}

DWORD PD_FindPartitionNext(DWORD dwSearchId, PD_PARTINFO *pInfo)
{
    DWORD *pdwStoreId = (PDWORD) dwSearchId;
    if (*pdwStoreId) {
        DWORD dwRet;
        DISK_INFO di;
        memset( &di, 0, sizeof(DISK_INFO));
        memset( pInfo, 0, sizeof(PD_PARTINFO));
        pInfo->cbSize = sizeof(PD_PARTINFO);
        if (!DeviceIoControl((HANDLE)*pdwStoreId, IOCTL_DISK_GETINFO, &di, sizeof(DISK_INFO), &di, sizeof(DISK_INFO), &dwRet, NULL)) {
            DeviceIoControl((HANDLE)*pdwStoreId, DISK_IOCTL_GETINFO, &di, sizeof(DISK_INFO), &di, sizeof(DISK_INFO), &dwRet, NULL);
        }
        pInfo->snNumSectors = di.di_total_sectors;
        pInfo->bPartType = 0x4;        
        wcscpy( pInfo->szPartitionName, L"PART00");
        *pdwStoreId = NULL;
        return ERROR_SUCCESS;
    }
    return ERROR_NO_MORE_FILES;                
}

void PD_FindPartitionClose(DWORD dwSearchId)
{
    delete (DWORD *)dwSearchId;
}    

DWORD PD_OpenPartition(DWORD dwStoreId, LPCTSTR szPartName, LPDWORD pdwPartitionId)
{
    *pdwPartitionId = dwStoreId;
    return ERROR_SUCCESS;
}

void PD_ClosePartition(DWORD dwPartitionId)
{
}

DWORD PD_DeviceIoControl(DWORD dwPartitionId, DWORD dwCode, PBYTE pInBuf, DWORD nInBufSize, PBYTE pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned)
{
    if (DeviceIoControl( (HANDLE)dwPartitionId, dwCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, NULL)){
        return ERROR_SUCCESS;
    } 
    return GetLastError();
}
