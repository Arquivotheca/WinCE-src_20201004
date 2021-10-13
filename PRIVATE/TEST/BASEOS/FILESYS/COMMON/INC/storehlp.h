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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef __STOREHLP_H__
#define __STOREHLP_H__

#include <windows.h>
#include <storemgr.h>
#include <diskio.h>
#include <fatutil.h>

//
// C++ Interface Definition
//

// an interface to all the information about a mounted partition
class CFSInfo {
public:
    CFSInfo(LPCWSTR pszRootDirectory = NULL);
    ~CFSInfo();

    const LPWSTR RootDirectory(LPWSTR pszRootDirectory = NULL,
        UINT cLen = 0);

    const LPCE_VOLUME_INFO CFSInfo::GetVolumeInfo(LPCE_VOLUME_INFO pVolumeInfo = NULL);
    const PPARTINFO PartInfo(PPARTINFO pPartInfo = NULL);
    const PSTOREINFO StoreInfo(PSTOREINFO pStoreInfo = NULL);

    const BOOL GetDiskFreeSpaceEx(PULARGE_INTEGER pFreeBytesAvailable,
        PULARGE_INTEGER pTotalNumberOfBytes,
        PULARGE_INTEGER pTotalNumberOfFreeBytes);

    const BOOL Format(PFORMAT_OPTIONS pFormatOptions);
    const BOOL FormatEx(PFORMAT_PARAMS pFormatParams);

    const BOOL Scan(PSCAN_OPTIONS pScanOptions);
    const BOOL ScanEx(PSCAN_PARAMS pScanParams);

    const BOOL Defrag(PDEFRAG_OPTIONS pDefragOptions);
    const BOOL DefragEx(PDEFRAG_PARAMS pDefragParams);
    
    const BOOL Mount(void);
    const BOOL Dismount(void);

    static const LPWSTR FindStorageWithProfile(LPCWSTR pszProfile,
        __out_ecount(cLen) LPWSTR pszRootDirectory,
        UINT cLen);

private:
    static const WCHAR DEF_ROOT_DIRECTORY[];
    CE_VOLUME_INFO m_volInfo;
    PARTINFO m_partInfo;
    STOREINFO m_storeInfo;
    HANDLE m_hStore;
    HANDLE m_hPart;
};

//
// C Interface Definition
//

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __PARTDATA {
    HANDLE hStore;
    HANDLE hPart;
    STOREINFO storeInfo;
    PARTINFO partInfo;

// don't want to touch these
    HANDLE hFindStore;
    HANDLE hFindPart;    
    WCHAR szProfileMatch[PROFILENAMESIZE];
    
} PARTDATA, *PPARTDATA;

BOOL Stg_RefreshStore (
    IN LPCWSTR pszStore
    );

BOOL Stg_RefreshAllStores (
    );

BOOL Stg_GetStoreInfoByPath (
    IN LPCWSTR pszPath, 
    OUT PSTOREINFO pStoreInfo, 
    OUT PPARTINFO pPartInfo
    );

PPARTDATA Stg_FindFirstPart (
    IN LPCWSTR pszProfileMatch
    );

BOOL Stg_FindNextPart (
    IN PPARTDATA pInfo
    );

BOOL Stg_FindClosePart (
    IN PPARTDATA pInfo
    );

BOOL Stg_QueryProfileRegValue(
    IN PPARTDATA pPartData,
    IN LPCWSTR pszValueName, 
    OUT PDWORD pType,
    OUT LPBYTE pData,
    IN OUT PDWORD pcbData
    );
    

BOOL Stg_FormatPart (
    IN HANDLE hPart,
    IN PFORMAT_OPTIONS pFormatOpts
    );

BOOL Stg_FormatPartEx (
    IN HANDLE hPart,
    IN PFORMAT_PARAMS pFormatParams
    );

BOOL Stg_ScanPart (
    IN HANDLE hPart,
    IN PSCAN_OPTIONS pScanOpts
    );

BOOL Stg_ScanPartEx (
    IN HANDLE hPart,
    IN PSCAN_PARAMS pScanParams
    );

BOOL Stg_DefragPart (
    IN HANDLE hPart,
    IN PDEFRAG_OPTIONS pDefragOpts
    );

BOOL Stg_DefragPartEx (
    IN HANDLE hPart,
    IN PDEFRAG_PARAMS pDefragOpts
    );


BOOL Stg_MountPart (
    IN HANDLE hPart
    );

BOOL Stg_DismountPart (
    IN HANDLE hPart
    );

#ifdef __cplusplus
};
#endif

#endif //__STOREHLP_H__