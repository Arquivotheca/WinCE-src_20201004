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
#include "storeincludes.hpp"

DWORD PD_OpenStore(HANDLE hDisk, PDWORD pdwStoreId)
{
    *pdwStoreId = (DWORD)hDisk;
    return ERROR_SUCCESS;
}

void PD_CloseStore(DWORD /* dwStoreId */)
{
}

DWORD PD_FormatStore(DWORD dwStoreId)
{
    DWORD dwRet;
    BOOL fSuccess = FALSE;
    STORAGEDEVICEINFO sdi = {0};
    
    // first query the device and make sure it is not marked as read-only
    sdi.cbSize = sizeof(sdi);
    if (!DeviceIoControl((HANDLE)dwStoreId, IOCTL_DISK_DEVICE_INFO, &sdi, sizeof(sdi), NULL, 0, &dwRet, NULL) ||
        !(sdi.dwDeviceFlags & STORAGE_DEVICE_FLAG_READONLY)) {
        // try to low-level format the storage device
        fSuccess = DeviceIoControl((HANDLE)dwStoreId, IOCTL_DISK_FORMAT_MEDIA, NULL, 0, NULL, 0, &dwRet, NULL);
        if (!fSuccess) {
            // try the legacy DISK_IOCTL* code if the IOCTL_DISK_ version fails
            fSuccess = DeviceIoControl((HANDLE)dwStoreId, DISK_IOCTL_FORMAT_MEDIA, NULL, 0, NULL, 0, &dwRet, NULL);
        }
    }
    return fSuccess ? ERROR_SUCCESS : ERROR_GEN_FAILURE;
}

DWORD PD_IsStoreFormatted(DWORD /* dwStoreId */)
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

DWORD PD_CreatePartition(DWORD /* dwStoreId */, LPCTSTR /* szPartName */, BYTE /* bPartType */, SECTORNUM /* numSectors */, BOOL /* bAuto */)
{
    return ERROR_GEN_FAILURE;
}

DWORD PD_DeletePartition(DWORD /* dwStoreId */, LPCTSTR /* szPartName */)
{
    return ERROR_GEN_FAILURE;
}

DWORD PD_RenamePartition(DWORD /* dwStoreId */, LPCTSTR /* szOldName */, LPCTSTR /* szNewName */)
{
    return ERROR_GEN_FAILURE;
}

DWORD PD_SetPartitionAttrs(DWORD /* dwStoreId */, LPCTSTR /* szPartName */, DWORD /* dwAttr */)
{
    return ERROR_GEN_FAILURE;
}

DWORD PD_SetPartitionSize(DWORD /* dwStoreId */, LPCTSTR /* szPartName */, SECTORNUM /* numSectors */)
{
    return ERROR_GEN_FAILURE;
}

DWORD PD_FormatPartition(DWORD dwStoreId, LPCTSTR /* szPartName */, BYTE /* bPartType */, BOOL /* bAuto */)
{
    // just format the entire store
    return PD_FormatStore(dwStoreId);
}
DWORD PD_GetPartitionInfo(DWORD dwStoreId, LPCTSTR /* szPartName */, PD_PARTINFO *pInfo)
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
        StringCbCopyW( pInfo->szPartitionName, sizeof (pInfo->szPartitionName), L"PART00");
        *pdwStoreId = NULL;
        return ERROR_SUCCESS;
    }
    return ERROR_NO_MORE_FILES;
}

void PD_FindPartitionClose(DWORD dwSearchId)
{
    delete (DWORD *)dwSearchId;
}    

DWORD PD_OpenPartition(DWORD dwStoreId, LPCTSTR /* szPartName */, LPDWORD pdwPartitionId)
{
    *pdwPartitionId = dwStoreId;
    return ERROR_SUCCESS;
}

void PD_ClosePartition(DWORD /*( dwPartitionId */)
{
}

DWORD PD_DeviceIoControl(DWORD dwPartitionId, DWORD dwCode, __in_bcount (nInBufSize) PBYTE pInBuf, DWORD nInBufSize, __out_bcount(nOutBufSize) PBYTE pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned)
{
    DWORD dwError = ERROR_SUCCESS;
    // Nullpart doesn't explicitly handle any IOCTLs so we need to forward the IOCTL here so
    // that we preserve the caller status and the next IOCTL handler will know whether or not
    // to marshal embedded poitners.
#ifdef UNDER_CE
    if (ForwardDeviceIoControl((HANDLE)dwPartitionId, dwCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, NULL)){
        return ERROR_SUCCESS;
    }
#else
    if (STG_DeviceIoControl(reinterpret_cast<PSTOREHANDLE> (dwPartitionId), dwCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, NULL)){
        return ERROR_SUCCESS;
    }
#endif // !UNDER_CE
    dwError = GetLastError();
    if (ERROR_SUCCESS == dwError) {
        return ERROR_GEN_FAILURE;
    }
    return dwError;
}

DWORD PD_MediaChangeEvent (DWORD /* StoreId */, STORAGE_MEDIA_CHANGE_EVENT_TYPE /* EventId */, STORAGE_MEDIA_ATTACH_RESULT* /* pResult */)
{
    return ERROR_NOT_SUPPORTED;
}
