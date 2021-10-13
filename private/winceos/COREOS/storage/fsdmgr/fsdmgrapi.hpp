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

#ifndef __FSDMGRAPI_HPP
#define __FSDMGRAPI_HPP

// These are all APIs exported by FSDMGR for FSDs to use. They are also defined in
// sdk\inc\fsdmgr.h
EXTERN_C BOOL FSDMGR_AdvertiseInterface (const GUID *pGuid, LPCWSTR lpszName, BOOL fAdd);
EXTERN_C int FSDMGR_GetVolumeName(HVOL pVol, __out_ecount(MountNameChars) WCHAR* pMountName,
    int MountNameChars);
EXTERN_C BOOL FSDMGR_GetRegistryValue (PDSK pDsk, const WCHAR* pValueName, DWORD* pValue);
EXTERN_C BOOL FSDMGR_GetRegistryString (PDSK pDsk, const WCHAR* pValueName,
    __out_ecount(ValueSize) WCHAR* pValue, DWORD ValueSize);
EXTERN_C BOOL FSDMGR_GetRegistryFlag (PDSK pDsk, const WCHAR* pValueName, DWORD* pFlag, DWORD SetBit);
EXTERN_C DWORD FSDMGR_ScanVolume (PDSK pDsk, LPVOID pParams);
EXTERN_C DWORD FSDMGR_FormatVolume (PDSK pDsk, LPVOID pParams);
EXTERN_C DWORD FSDMGR_CleanVolume (PDSK pDsk, LPVOID pParams);
EXTERN_C LRESULT FSDMGR_GetMountFlags(HVOL hVol, DWORD* pMountFlags);
EXTERN_C BOOL FSDMGR_GetDiskName(PDSK pDsk, WCHAR *pDiskName);
EXTERN_C HVOL FSDMGR_GetVolumeHandle (PDSK pDsk);
EXTERN_C HVOL FSDMGR_RegisterVolume (PDSK pDsk, const WCHAR* pMountName,
        PVOLUME VolumeContext);
EXTERN_C HANDLE FSDMGR_CreateSearchHandle(HVOL pVol, HANDLE hProc, PSEARCH pSearch);
EXTERN_C HANDLE FSDMGR_CreateFileHandle(HVOL pVol, HANDLE hProc, PFILE pFile);
EXTERN_C void FSDMGR_DeregisterVolume(HVOL pVol);
EXTERN_C BOOL FSDMGR_DiskIoControl(PDSK pDsk, DWORD IoControlCode, void* pInBuf, DWORD InBufSize,
    void* pOutBuf, DWORD OutBufSize, DWORD* pBytesReturned, OVERLAPPED* pOverlapped);
EXTERN_C DWORD FSDMGR_GetDiskInfo(PDSK pDsk, FSD_DISK_INFO *pfdi);
EXTERN_C DWORD FSDMGR_GetPartInfo(PDSK pDsk, PARTINFO *pPartInfo);
EXTERN_C DWORD FSDMGR_GetStoreInfo(PDSK pDsk, STOREINFO *pStoreInfo);
EXTERN_C DWORD FSDMGR_ReadDisk(PDSK pDsk, DWORD dwSector, DWORD cSectors, PBYTE pBuffer, DWORD cbBuffer);
EXTERN_C DWORD FSDMGR_WriteDisk(PDSK pDsk, DWORD dwSector, DWORD cSectors, PBYTE pBuffer, DWORD cbBuffer);
EXTERN_C DWORD FSDMGR_ReadDiskEx(FSD_SCATTER_GATHER_INFO *pfsgi, FSD_SCATTER_GATHER_RESULTS *pfsgr);
EXTERN_C DWORD FSDMGR_WriteDiskEx(FSD_SCATTER_GATHER_INFO *pfsgi, FSD_SCATTER_GATHER_RESULTS *pfsgr);
#endif // __FSDMGRAPI_HPP

