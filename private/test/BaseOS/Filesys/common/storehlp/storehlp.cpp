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
#include <storehlp.h>
#include <pfsioctl.h>
#include "bootpart.h"
#include "fathlp.h"
#include "bpb.h"
#include "fsdspyex.h"


//
// C++ Interface
//

const WCHAR DEF_ROOT_DIRECTORY[] = L"\\Storage Card";

// One more defition-copy
#define IOCTL_DISK_GET_VOLUME_PARAMETERS CTL_CODE(IOCTL_DISK_BASE, 0x0081, METHOD_BUFFERED, FILE_ANY_ACCESS)

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
CFSInfo::CFSInfo(LPCWSTR pszRootDirectory)
{
    m_hStore = INVALID_HANDLE_VALUE;
    m_hPart = INVALID_HANDLE_VALUE;

    if(!Stg_GetStoreInfoByPath(pszRootDirectory, &m_storeInfo, &m_partInfo)) {
        ::memset(&m_storeInfo, 0, sizeof(STOREINFO));
        ::memset(&m_partInfo, 0, sizeof(PARTINFO));
    }
    m_hStore = ::OpenStore(m_storeInfo.szDeviceName);
    if(INVALID_HANDLE_VALUE != m_hStore) {
        m_hPart = ::OpenPartition(m_hStore, m_partInfo.szPartitionName);
    }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
CFSInfo::~CFSInfo() 
{
    if(INVALID_HANDLE_VALUE != m_hStore) {
        ::CloseHandle(m_hStore);
    }
    if(INVALID_HANDLE_VALUE != m_hPart) {
        ::CloseHandle(m_hPart);
    }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
const LPCE_VOLUME_INFO CFSInfo::GetVolumeInfo(LPCE_VOLUME_INFO pVolumeInfo)
{
    m_volInfo.cbSize = sizeof(CE_VOLUME_INFO);

    if (!CeGetVolumeInfo(RootDirectory(NULL, 0), CeVolumeInfoLevelStandard, &m_volInfo)) {
        return NULL;
    }

    if (pVolumeInfo) {
        memcpy(pVolumeInfo, &m_volInfo, sizeof(CE_VOLUME_INFO));
        return pVolumeInfo;
    } else {
        return &m_volInfo;
    }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
const BOOL CFSInfo::GetDiskFreeSpaceEx(PULARGE_INTEGER pFreeBytesAvailable,
        PULARGE_INTEGER pTotalNumberOfBytes,
        PULARGE_INTEGER pTotalNumberOfFreeBytes)
{
    return ::GetDiskFreeSpaceEx(RootDirectory(NULL, 0),
        pFreeBytesAvailable,
        pTotalNumberOfBytes,
        pTotalNumberOfFreeBytes);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
const LPWSTR CFSInfo::RootDirectory(LPWSTR pszRoot, UINT cLen)
{
    if(INVALID_HANDLE_VALUE == m_hStore || INVALID_HANDLE_VALUE == m_hPart) {
        return NULL;
    }
            
    if(pszRoot && cLen) {
        VERIFY(SUCCEEDED(::StringCchCopy(pszRoot, cLen, m_partInfo.szVolumeName)));
            return pszRoot;
    } else {
        return m_partInfo.szVolumeName;
    }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
const PPARTINFO CFSInfo::PartInfo(PPARTINFO pPartInfo)
{
    if(INVALID_HANDLE_VALUE == m_hStore || INVALID_HANDLE_VALUE == m_hPart) {
        return NULL;
    }
    
    if(pPartInfo) {
        ::memcpy(pPartInfo, &m_partInfo, sizeof(PARTINFO));
        return pPartInfo;
    } else {
        return &m_partInfo;
    }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
const PSTOREINFO CFSInfo::StoreInfo(PSTOREINFO pStoreInfo)
{
    if(INVALID_HANDLE_VALUE == m_hStore || INVALID_HANDLE_VALUE == m_hPart) {
        return NULL;
    }
        
    if(pStoreInfo) {
        ::memcpy(pStoreInfo, &m_storeInfo, sizeof(STOREINFO));
        return pStoreInfo;
    } else {
        return &m_storeInfo;
    }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
const BOOL CFSInfo::Mount(void)
{
    if(INVALID_HANDLE_VALUE == m_hStore || INVALID_HANDLE_VALUE == m_hPart) {
        return FALSE;
    }
    return ::Stg_MountPart(m_hPart);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
const BOOL CFSInfo::Dismount(void)
{
    if(INVALID_HANDLE_VALUE == m_hStore || INVALID_HANDLE_VALUE == m_hPart) {
        return FALSE;
    }
    return ::Stg_DismountPart(m_hPart);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
const BOOL CFSInfo::Format(PFORMAT_OPTIONS pFormatOptions)
{
    if(INVALID_HANDLE_VALUE == m_hStore || INVALID_HANDLE_VALUE == m_hPart) {
        return FALSE;
    }
    return ::Stg_FormatPart(m_hPart, pFormatOptions);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
const BOOL CFSInfo::FormatEx(PFORMAT_PARAMS pFormatParams)
{
    if(INVALID_HANDLE_VALUE == m_hStore || INVALID_HANDLE_VALUE == m_hPart) {
        return FALSE;
    }
    return ::Stg_FormatPartEx(m_hPart, pFormatParams);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
const BOOL CFSInfo::Scan(PSCAN_OPTIONS pScanOptions)
{
    if(INVALID_HANDLE_VALUE == m_hStore || INVALID_HANDLE_VALUE == m_hPart) {
        return FALSE;
    }

    return ::Stg_ScanPart(m_hPart, pScanOptions);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
const BOOL CFSInfo::ScanEx(PSCAN_PARAMS pScanParams)
{
    if(INVALID_HANDLE_VALUE == m_hStore || INVALID_HANDLE_VALUE == m_hPart) {
        return FALSE;
    }

    return ::Stg_ScanPartEx(m_hPart, pScanParams);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
const BOOL CFSInfo::Defrag(PDEFRAG_OPTIONS pDefragOptions)
{
    if(INVALID_HANDLE_VALUE == m_hStore || INVALID_HANDLE_VALUE == m_hPart) {
        return FALSE;
    }
    return ::Stg_DefragPart(m_hPart, pDefragOptions);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
const BOOL CFSInfo::DefragEx(PDEFRAG_PARAMS pDefragParams)
{
    if(INVALID_HANDLE_VALUE == m_hStore || INVALID_HANDLE_VALUE == m_hPart) {
        return FALSE;
    }
    return ::Stg_DefragPartEx(m_hPart, pDefragParams);
}


// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
const LPWSTR CFSInfo::FindStorageWithProfile(
    LPCWSTR pszProfile,
    __out_ecount(cLen) LPWSTR pszRootDirectory,
    UINT cLen,
    BOOL bReadOnlyOk /*TRUE*/)
{
    PPARTDATA pInfo = NULL;

    pInfo = Stg_FindFirstPart(pszProfile, bReadOnlyOk);

    if (!pInfo)
    {
        return NULL;
    }

    if (NULL != pszRootDirectory && cLen > VOLUMENAMESIZE)
    {
        memcpy(pszRootDirectory, pInfo->partInfo.szVolumeName, VOLUMENAMESIZE);
    }

    // Close all the handles opened during discovery
    if (INVALID_HANDLE_VALUE != pInfo->hStore)
    {
        CloseHandle(pInfo->hStore);
    }

    if (INVALID_HANDLE_VALUE != pInfo->hPart)
    {
        CloseHandle(pInfo->hPart);
    }

    if (INVALID_HANDLE_VALUE != pInfo->hFindStore)
    {
        FindCloseStore(pInfo->hFindStore);
    }

    if (INVALID_HANDLE_VALUE != pInfo->hFindPart)
    {
        FindClosePartition(pInfo->hFindPart);
    }

    return pszRootDirectory;
}

void CFSInfo::DoOutputStoreInfo()
{
    StoreHlp::OutputStoreInfo();
}

// -----------------------------------------------------------------------------
// Waits for PNP system to advertize FATFS_MOUNT_GUID. Extracts drive's path
// information, and matches it to the expected value.
//
// pszExpectedFolderName : full name or prefix of exepected mountpoint:
//                         e.g. Storage Card (must not have slash)
// pszMountedFolderName  : receives actual mount point
// cLen : size of pszMountedFolderName buffer
// fExactFolderNameMatch : if TRUE and pszExpectedFolderName is not NULL,
//                         will wait for drive with matching path to mount
//                         if FALSE and pszExpectedFolderName is not NULL,
//                         will wait for drive that starts with matching path
// dwWaitTimeout         : mount wait timeout in milliseconds or INFINITE
// -----------------------------------------------------------------------------
const BOOL CFSInfo::WaitForFATFSMount(
    IN LPCWSTR pszExpectedFolderName,
    __out_ecount(cLen) LPWSTR pszMountedFolderName,
    UINT cLen,
    BOOL fExactFolderNameMatch,
    DWORD dwWaitTimeout)
{
    BOOL fRet = FALSE;
    BOOL fDeviceFound = FALSE;
    HANDLE hMsgQueue = INVALID_HANDLE_VALUE;
    HANDLE hNotifications = INVALID_HANDLE_VALUE;
    DWORD dwQueueData[(sizeof(DEVDETAIL) + MAX_PATH)/sizeof(DWORD)] = {0};
    DWORD dwRead = 0;
    DWORD dwFlags = 0;
    DEVDETAIL *pDevDetail = (DEVDETAIL*)dwQueueData;
    DWORD dwQueueItemSize = (sizeof(DEVDETAIL) + MAX_PATH);
    MSGQUEUEOPTIONS msgqopts = {0};
    msgqopts.dwSize = sizeof(MSGQUEUEOPTIONS);
    msgqopts.dwFlags = 0;
    msgqopts.cbMaxMessage = dwQueueItemSize;
    msgqopts.bReadAccess = TRUE;

    // Create a message Queue
    hMsgQueue = CreateMsgQueue(NULL, &msgqopts);
    if (INVALID_HANDLE_VALUE == hMsgQueue)
    {
        NKDbgPrintfW(L"Error : CFSInfo::WaitForFATFSMount() : CreateMsgQueue() failed with error = %u\r\n", GetLastError());
        goto done;
    }

    // register for FAT volume notifications
    hNotifications = RequestDeviceNotifications(&FATFS_MOUNT_GUID, hMsgQueue, FALSE);
    if (INVALID_HANDLE_VALUE == hNotifications)
    {
        NKDbgPrintfW(L"Error : CFSInfo::WaitForFATFSMount() : RequestDeviceNotifications() failed with error = %u\r\n", GetLastError());
        goto done;
    }

    // Wait until the FATFS_MOUNT_GUID is signaled
    while (ReadMsgQueue(hMsgQueue, pDevDetail, dwQueueItemSize, &dwRead, dwWaitTimeout, &dwFlags))
    {
        if (pDevDetail->fAttached)
        {
            // If pszExpectedFolderName is NULL, return any attached device
            // If pszExpectedFolderName is not NULL, and fExactFolderNameMatch is TRUE, return path that matches exactly
            // If pszExpectedFolderName is not NULL, and fExactFolderNameMatch is FALSE, return path that starts with the string
            // specified by pszExpectedFolderName
            if ((!pszExpectedFolderName) ||
                (pszExpectedFolderName && fExactFolderNameMatch && (0 == _tcsncmp(pDevDetail->szName, pszExpectedFolderName, MAX_PATH))) ||
                (pszExpectedFolderName && !fExactFolderNameMatch && (pDevDetail->szName == _tcsstr(pDevDetail->szName, pszExpectedFolderName))))
            {
                fDeviceFound = TRUE;
                VERIFY(SUCCEEDED(StringCchPrintf(pszMountedFolderName, cLen, _T("\\%s"), pDevDetail->szName)));
                break;
            }
        }
    }
    if (!fDeviceFound)
    {
        NKDbgPrintfW(L"Error : CFSInfo::WaitForFATFSMount() : did not detect FATFS_MOUNT_GUID for specified device\r\n");
        goto done;
    }

    fRet = TRUE;
done:
    if (INVALID_HANDLE_VALUE != hMsgQueue)
    {
        CloseHandle(hMsgQueue);
    }
    if (INVALID_HANDLE_VALUE != hNotifications)
    {
        StopDeviceNotifications(hNotifications);
    }
    return fRet;
}

// -----------------------------------------------------------------------------
// Retrieve logical sector mappings for the specified file handle
// -----------------------------------------------------------------------------
const PSECTOR_LIST_ENTRY CFSInfo::GetFileSectorInfo(IN HANDLE hFileHandle, IN OUT LPDWORD pdwBytesReturned)
{
    PSECTOR_LIST_ENTRY pSectorRuns = NULL;
    DWORD dwBytesReturned = 0;
    DWORD dwBufferSize = 0;
    *pdwBytesReturned = 0;

    // Send special FSCTL command to get list of sector runs for this file
    // this is only supported by EXFAT.dll managed filesystems
    if(!DeviceIoControl(hFileHandle, FSCTL_GET_LOGICAL_SECTOR_LIST, NULL, 0, NULL, 0, &dwBytesReturned, 0)){
        NKDbgPrintfW(L"Error : CFSInfo::GetFileSectorInfo() : DeviceIoControl(FSCTL_GET_LOGICAL_SECTOR_LIST) failed with error = %u (is the file on [T]exFAT/FAT filesystem?)\r\n", GetLastError());
        goto done;
    }
    // Sanity check return value
    if (dwBytesReturned < sizeof(SECTOR_LIST_ENTRY))
    {
        NKDbgPrintfW(L"Error : CFSInfo::GetFileSectorInfo() : DeviceIoControl(FSCTL_GET_LOGICAL_SECTOR_LIST) returned invalid buffer size\r\n");
        goto done;
    }

    // Allocate buffer sufficient to hold the entire set of sector runs
    pSectorRuns = (PSECTOR_LIST_ENTRY) LocalAlloc(LPTR, dwBytesReturned);
    if (NULL == pSectorRuns)
    {
        NKDbgPrintfW(L"Error : CFSInfo::GetFileSectorInfo() : out of memory\r\n");
        goto done;
    }

    // Get sector runs
    dwBufferSize = dwBytesReturned;
    if ((!DeviceIoControl(hFileHandle, FSCTL_GET_LOGICAL_SECTOR_LIST, NULL, 0, (PVOID) pSectorRuns, dwBufferSize, &dwBytesReturned, 0)) ||
        (dwBytesReturned != dwBufferSize))
    {
        NKDbgPrintfW(L"Error : CFSInfo::GetFileSectorInfo() : DeviceIoControl(FSCTL_GET_LOGICAL_SECTOR_LIST) failed\r\n");
        LocalFree(pSectorRuns);
        pSectorRuns = NULL;
        goto done;
    }
    *pdwBytesReturned = dwBytesReturned;
    // Success
done:
    return pSectorRuns;
}

// -----------------------------------------------------------------------------
// Print out sector run info for specified file
// -----------------------------------------------------------------------------
const BOOL CFSInfo::OutputFileSectorInfo(IN LPCTSTR pszFileName)
{
    BOOL fRet = FALSE;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    PSECTOR_LIST_ENTRY pSectorRuns = NULL;
    DWORD dwBytesReturned = 0;

    // Get a handle to the specified file
    hFile = CreateFile(pszFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == hFile)
    {
        NKDbgPrintfW(L"Error : CFSInfo::OutputFileSectorInfo() : CreateFile(%s) failed with error = %u\r\n", pszFileName, GetLastError());
        goto done;
    }

    // Get a list of sector runs for this file
    pSectorRuns = GetFileSectorInfo(hFile, &dwBytesReturned);
    if (NULL == pSectorRuns)
    {
        NKDbgPrintfW(L"Error : CFSInfo::OutputFileSectorInfo() : GetFileSectorInfo() failed for file %s", pszFileName);
        goto done;
    }

    // Print out sector runs
    NKDbgPrintfW(L"Info : sector run information for file %s\r\n", pszFileName);
    for(DWORD dwI = 0; dwI < (dwBytesReturned / sizeof(SECTOR_LIST_ENTRY)); dwI++)
    {
        NKDbgPrintfW(L"     : %u) : %u - %u\r\n", dwI, pSectorRuns[dwI].dwStartSector, pSectorRuns[dwI].dwStartSector + pSectorRuns[dwI].dwNumSectors - 1);
    }

    // Success
    fRet = TRUE;
done:
    if (INVALID_HANDLE_VALUE != hFile)
    {
        CloseHandle(hFile);
    }
    if (NULL != pSectorRuns)
    {
        LocalFree(pSectorRuns);
    }
    return fRet;
}

// -----------------------------------------------------------------------------
// Send FSCTL_GIVE_ACCESS_TO_THREAD command to fsdspy filter
// -----------------------------------------------------------------------------
const BOOL CFSInfo::EnableVolumeAccess()
{
    BOOL fRet = FALSE;
    DWORD dwLength = 0;
    if (!CeFsIoControlW(RootDirectory(NULL, 0),
        FSCTL_GIVE_ACCESS_TO_THREAD,
        NULL,
        0,
        NULL, 
        0,
        &dwLength,
        NULL))
    {
        NKDbgPrintfW(L"Error : CFSInfo::EnableVolumeAccess() : volume failed to respond to FSCTL_GIVE_ACCESS_TO_THREAD command\r\n");
        goto done;
    }
    fRet = TRUE;
done:
    return fRet;
}

// -----------------------------------------------------------------------------
// Send FSCTL_STOP_ACCESS_TO_THREAD command to fsdspy filter
// -----------------------------------------------------------------------------
const BOOL CFSInfo::DisableVolumeAccess()
{
    BOOL fRet = FALSE;
    DWORD dwLength = 0;
    if (!CeFsIoControlW(RootDirectory(NULL, 0),
        FSCTL_STOP_ACCESS_TO_THREAD,
        NULL,
        0,
        NULL, 
        0,
        &dwLength,
        NULL))
    {
        NKDbgPrintfW(L"Error : CFSInfo::EnableVolumeAccess() : volume failed to respond to FSCTL_STOP_ACCESS_TO_THREAD command\r\n");
        goto done;
    }
    fRet = TRUE;
done:
    return fRet;
}


//
// C Interface Implementation
//

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL Stg_RefreshStore (
    IN LPCWSTR pszStore
    )
{
    WCHAR szStoreName[MAX_PATH];

    VERIFY(SUCCEEDED(StringCchPrintf(szStoreName, MAX_PATH, TEXT("\\StoreMgr\\%s"), pszStore)));
    NKDbgPrintfW(L"refreshing store \"%s\"...\r\n", pszStore);
    return MoveFile(szStoreName, szStoreName);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL Stg_RefreshAllStores (
    )
{
    BOOL fRet = TRUE;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    STOREINFO storeInfo = {0};
    
    storeInfo.cbSize = sizeof(STOREINFO);
    hFind = FindFirstStore(&storeInfo);

    if(INVALID_HANDLE_VALUE != hFind) {
        do {
            if(!Stg_RefreshStore(storeInfo.szDeviceName)) {
                NKDbgPrintfW( L"unable to refresh store \"%s\"\r\n", storeInfo.szDeviceName);
                fRet = FALSE;
            }
        } while(FindNextStore(hFind, &storeInfo));
        FindCloseStore(hFind);
    }

    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL Stg_GetStoreInfoByPath (
    IN LPCWSTR pszPath, 
    OUT PSTOREINFO pStoreInfo, 
    OUT PPARTINFO pPartInfo
    )
{
    if(!pszPath || !pszPath[0] || !pStoreInfo || !pPartInfo) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    PPARTDATA pInfo = Stg_FindFirstPart(NULL);
    if(NULL == pInfo) {
        return FALSE;
    }

    // Only copy the mount-point portion of the path
    TCHAR szMountPoint[MAX_PATH];
    VERIFY(SUCCEEDED(StringCchCopy(szMountPoint, MAX_PATH, pszPath)));
    TCHAR * szSlash = _tcsstr(szMountPoint, _T("\\"));
    
    if (szSlash) {
        if (szSlash == szMountPoint) {
            szSlash = _tcsstr(szMountPoint + 1, _T("\\"));
        }
        
        if (szSlash) {
            *szSlash = _T('\0');
        }
    }

    BOOL fFound = FALSE;
    do
    {
        if(pInfo->partInfo.szVolumeName[0] &&
           (0 == wcsicmp(szMountPoint, pInfo->partInfo.szVolumeName) ||
            0 == wcsicmp(szMountPoint+1, pInfo->partInfo.szVolumeName))) {
            memcpy(pStoreInfo, &pInfo->storeInfo, sizeof(STOREINFO));
            memcpy(pPartInfo, &pInfo->partInfo, sizeof(PARTINFO));
            fFound = TRUE;
        }
        
    } while(!fFound && Stg_FindNextPart(pInfo));

    VERIFY(Stg_FindClosePart(pInfo));
    
    return fFound; 
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PPARTDATA Stg_FindFirstPart (
    IN LPCWSTR pszProfileMatch,
    IN BOOL bReadOnlyOk /*TRUE*/
    )
{
    PPARTDATA pInfo = (PPARTDATA)LocalAlloc(LPTR, sizeof(PARTDATA));
    if(NULL == pInfo) {
        return NULL;
    }
    
    // if pszProfileMatch is NULL, don't try to match profile
    if(!pszProfileMatch) {
        VERIFY(SUCCEEDED(StringCchCopy(pInfo->szProfileMatch, PROFILENAMESIZE, L"*")));
    } else {
        VERIFY(SUCCEEDED(StringCchCopy(pInfo->szProfileMatch, PROFILENAMESIZE, pszProfileMatch)));
    }

    pInfo->storeInfo.cbSize = sizeof(STOREINFO);
    pInfo->hFindStore = FindFirstStore(&pInfo->storeInfo);
    if(NULL == pInfo->hFindStore || INVALID_HANDLE_VALUE == pInfo->hFindStore) {
        LocalFree(pInfo);
        return NULL;
    }

    do
    {
        // did the caller request a specific profile or ANY profile?
        if(0 != wcscmp(pInfo->szProfileMatch, L"*")) {
            // loop until a store matches the profile
            while(0 != wcsicmp(pInfo->szProfileMatch, pInfo->storeInfo.sdi.szProfile)) {
                // if we cannot find another store, then bail out
                if(!FindNextStore(pInfo->hFindStore, &pInfo->storeInfo)) {
                    FindCloseStore(pInfo->hFindStore);
                    NKDbgPrintfW( L"no storage devices match the profile \"%s\"\r\n", pInfo->szProfileMatch);
                    LocalFree(pInfo);
                    SetLastError(ERROR_NO_MORE_ITEMS);
                    return NULL;
                }
            }
        }

        pInfo->hStore = OpenStore(pInfo->storeInfo.szDeviceName);
        if(NULL != pInfo->hStore && INVALID_HANDLE_VALUE != pInfo->hStore) {
            // search for a partition on this store
            pInfo->partInfo.cbSize = sizeof(PARTINFO);
            pInfo->hFindPart = FindFirstPartition(pInfo->hStore, &pInfo->partInfo);

            if(NULL == pInfo->hFindPart || INVALID_HANDLE_VALUE == pInfo->hFindPart)
            {
                //If there were no partitions in this store try to find the next store...
                if(!FindNextStore(pInfo->hFindStore, &pInfo->storeInfo)) 
                {
                    goto DONE;
                }
            }
            else if(!bReadOnlyOk )
            {
                // being picky, need a writable partition                    
                do
                {
                    if( StoreHlp::IsPathWriteable( pInfo->partInfo.szVolumeName ) )
                    {                    
                        NKDbgPrintfW(L"Partition is writeable\r\n");
                        break;
                    }
                    else
                    {
                        NKDbgPrintfW(L"Partition is read-only\r\n");
                    }
                }
                while(FindNextPartition(pInfo->hFindPart, &pInfo->partInfo));
            }
        }

    } while(NULL == pInfo->hFindPart || 
                        INVALID_HANDLE_VALUE == pInfo->hFindPart);

    // can only get here if we found a valid partition...
    ASSERT(INVALID_HANDLE_VALUE != pInfo->hStore && NULL != pInfo->hStore);
    ASSERT(INVALID_HANDLE_VALUE != pInfo->hFindPart && NULL != pInfo->hFindPart);
    ASSERT(INVALID_HANDLE_VALUE != pInfo->hFindStore && NULL != pInfo->hFindStore);

    // open a handle to the partition
    pInfo->hPart = OpenPartition(pInfo->hStore, pInfo->partInfo.szPartitionName);

    return pInfo;

DONE:
    FindCloseStore(pInfo->hFindStore);
    CloseHandle(pInfo->hStore);
    NKDbgPrintfW(L"no storage devices match the profile \"%s\"\r\n", pInfo->szProfileMatch);
    LocalFree(pInfo);
    SetLastError(ERROR_NO_MORE_ITEMS);
    return NULL;

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL Stg_FindNextPart (
    IN OUT PPARTDATA pInfo
    )
{    
    if(NULL == pInfo) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(NULL == pInfo->hFindStore || INVALID_HANDLE_VALUE == pInfo->hFindStore) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
        
    if(NULL == pInfo->hFindPart || INVALID_HANDLE_VALUE == pInfo->hFindPart) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(NULL == pInfo->hStore || INVALID_HANDLE_VALUE == pInfo->hStore) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // close the open partition handle
    VERIFY(CloseHandle(pInfo->hPart));
    pInfo->hPart = NULL;

    // search for the next partition on this store
    pInfo->partInfo.cbSize = sizeof(PARTINFO);
    if(!FindNextPartition(pInfo->hFindPart, &pInfo->partInfo)) {
        FindClosePartition(pInfo->hFindPart);        
        pInfo->hFindPart = NULL;
        CloseHandle(pInfo->hStore);
        pInfo->hStore = NULL;
        do
        {
            // didn't find another partition on this store... check the next
            do
            {
                pInfo->storeInfo.cbSize = sizeof(STOREINFO);
                if(!FindNextStore(pInfo->hFindStore, &pInfo->storeInfo)) {
                    FindCloseStore(pInfo->hFindStore);
                    pInfo->hFindStore = NULL;
                    // didn't find another store, fail the call
                    SetLastError(ERROR_NO_MORE_ITEMS);
                    return FALSE; 
                }
            } while(0 != wcscmp(pInfo->szProfileMatch, L"*") && 
                    0 != wcscmp(pInfo->szProfileMatch, pInfo->storeInfo.sdi.szProfile));

            // found another store matching the wildcard
            pInfo->hStore = OpenStore(pInfo->storeInfo.szDeviceName);
            if(NULL != pInfo->hStore && INVALID_HANDLE_VALUE != pInfo->hStore) {
                pInfo->partInfo.cbSize = sizeof(PARTINFO);
                pInfo->hFindPart = FindFirstPartition(pInfo->hStore, &pInfo->partInfo);
            }
        } while(INVALID_HANDLE_VALUE == pInfo->hFindPart ||
                            NULL == pInfo->hFindPart);
    }   

    pInfo->hPart = OpenPartition(pInfo->hStore, pInfo->partInfo.szPartitionName);
   
    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL Stg_FindClosePart (
    IN PPARTDATA pInfo
    )
{
    if(NULL == pInfo) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if(NULL != pInfo->hFindStore && INVALID_HANDLE_VALUE != pInfo->hFindStore) {
        VERIFY(FindCloseStore(pInfo->hFindStore));
        pInfo->hFindStore = NULL;
    }
    
    if(NULL != pInfo->hFindPart && INVALID_HANDLE_VALUE != pInfo->hFindPart) {
        VERIFY(FindClosePartition(pInfo->hFindPart));
        pInfo->hFindPart = NULL;
    }

    if(NULL != pInfo->hStore && INVALID_HANDLE_VALUE != pInfo->hStore) {
        VERIFY(CloseHandle(pInfo->hStore));
        pInfo->hStore = NULL;
    }

    if(NULL != pInfo->hPart && INVALID_HANDLE_VALUE != pInfo->hPart) {
        VERIFY(CloseHandle(pInfo->hPart));
        pInfo->hPart = NULL;
    }

    VERIFY(NULL == LocalFree(pInfo));
    
    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL Stg_QueryProfileRegValue(
    IN PPARTDATA pPartData,
    IN LPCWSTR pszValueName, 
    OUT PDWORD pType,
    OUT LPBYTE pData,
    IN OUT PDWORD pcbData
    )
{
    HKEY hKey = NULL;
    DWORD lastErr;
    BOOL fRet = FALSE;
    WCHAR szKey[MAX_PATH];
    
    if(!pPartData || !pszValueName || !pType || !pData) {
        SetLastError(ERROR_INVALID_PARAMETER);
        goto done;
    }

    VERIFY(SUCCEEDED(StringCchPrintf(szKey, MAX_PATH, L"System\\StorageManager\\Profiles\\%s",
        pPartData->storeInfo.sdi.szProfile)));
    lastErr = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szKey, 0, NULL, &hKey);
    if(ERROR_SUCCESS != lastErr) {
        SetLastError(lastErr);
        goto done;
    }

    lastErr = RegQueryValueEx(hKey, pszValueName, NULL, pType, pData, pcbData);
    if(ERROR_SUCCESS != lastErr) {
        SetLastError(lastErr);
        goto done;
    }

    fRet = TRUE;

done:
    if(NULL != hKey) {
        VERIFY(ERROR_SUCCESS == RegCloseKey(hKey));
    }
    return fRet;
        
}

// format/scan/defrag volume export from fatutil.dll or other storage utility 
// library
typedef DWORD (*PFN_UTILITY)(HANDLE hFile, PDISK_INFO pdi, LPVOID pOpts, 
    LPVOID pfnProgress, LPVOID pfnMessage);
// Definition of callback to display a message or to prompt user
typedef BOOL (*PFN_MESSAGE)(LPTSTR szMessage, LPTSTR szCaption, BOOL fYesNo);

static const LPWSTR FORMAT_EXPORT_NAME      = L"FormatVolume";
static const LPWSTR FORMATEX_EXPORT_NAME    = L"FormatVolumeEx";
static const LPWSTR SCAN_EXPORT_NAME        = L"ScanVolume";
static const LPWSTR SCANEX_EXPORT_NAME      = L"ScanVolumeEx";
static const LPWSTR DEFRAG_EXPORT_NAME      = L"DefragVolume";
static const LPWSTR DEFRAGEX_EXPORT_NAME    = L"DefragVolumeEx";

static const UINT MAX_LIB_NAME_LEN      = 15;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static LPWSTR LookupUtilName(HANDLE hPart, __out_ecount(cUtil) LPWSTR pszUtil, UINT cUtil) 
{
    PARTINFO partInfo = {0};
    partInfo.cbSize = sizeof(PARTINFO);

    if(NULL == hPart) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    if(!GetPartitionInfo(hPart, &partInfo)) {
        return NULL;
    }    

    if(0 == wcsicmp(partInfo.szFileSys, L"FATFSD.DLL")) {
        VERIFY(SUCCEEDED(StringCchCopy(pszUtil, cUtil, L"FATUTIL.DLL")));
        return pszUtil;
    }
    
    if(0 == wcsicmp(partInfo.szFileSys, L"EXFAT.DLL")) {
        VERIFY(SUCCEEDED(StringCchCopy(pszUtil, cUtil, L"FATUTIL.DLL")));
        return pszUtil;
    }

    // add other utility library names here

    return NULL;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static VOID FormatVolumeProgress(DWORD dwPercent) 
{
    NKDbgPrintfW(L"format volume: %u%% complete\r\n", dwPercent);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL FormatVolumeMessage(LPWSTR pszMessage, LPWSTR pszCaption, BOOL fYesNo)
{
    NKDbgPrintfW(L"format volume: %s : %s %s\r\n", pszMessage, pszCaption, fYesNo);
    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL Stg_FormatPart (
    IN HANDLE hPart,
    IN PFORMAT_OPTIONS pFormatOpts
    )
{
    DWORD dwRet = ERROR_SUCCESS;
    DWORD dwBytes = 0;
    DISK_INFO diskInfo = {0};
    PFN_FORMATVOLUME pfnFormatVolume = NULL;
    WCHAR szLibrary[MAX_LIB_NAME_LEN] = L"";
    HINSTANCE hUtilLib = NULL;

    ASSERT(NULL != hPart && INVALID_HANDLE_VALUE != hPart);
    if(NULL == hPart || INVALID_HANDLE_VALUE == hPart) {
        return FALSE;
    }
    
    // load the designated utility libarry
    hUtilLib = LoadLibrary(LookupUtilName(hPart, szLibrary, MAX_LIB_NAME_LEN));
    if(!hUtilLib) {
        NKDbgPrintfW(L"LoadLibrary(%s) failed! Error %u\r\n", szLibrary, GetLastError());
        return FALSE;
    }

    // load the format function
    pfnFormatVolume = (PFN_FORMATVOLUME)GetProcAddressW(hUtilLib, FORMAT_EXPORT_NAME);
    if(NULL == pfnFormatVolume) {
        NKDbgPrintfW(L"GetProcAddress(0x%08X, \"%s\")\r\n", hUtilLib, FORMAT_EXPORT_NAME);
        goto done;
    }
    
    if (!DeviceIoControl(hPart, DISK_IOCTL_GETINFO, &diskInfo, sizeof(DISK_INFO), 
        &diskInfo, sizeof(DISK_INFO), &dwBytes, NULL)) {
        NKDbgPrintfW(L"DeviceIoControl(DISK_IOCTL_GETINFO) failed! Error %u\r\n", GetLastError());
        goto done;
    }

    // dismount the partition before formatting
    if(!(pFormatOpts->dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !DismountPartition(hPart)){
        NKDbgPrintfW(L"DismountPartition() failed! Error %u\r\n", GetLastError());
        goto done;
    }

    if(pFormatOpts) {
        NKDbgPrintfW(L"format volume: tfat=%s full=%s fatver=%u rootdirs=%u clusterbytes=%u\r\n", 
            (pFormatOpts->dwFlags & FATUTIL_FORMAT_TFAT) ? L"TRUE" : L"FALSE", 
            (pFormatOpts->dwFlags & FATUTIL_FULL_FORMAT) ? L"TRUE" : L"FALSE", 
            pFormatOpts->dwFatVersion, pFormatOpts->dwRootEntries, pFormatOpts->dwClusSize);
    }

    // format the partition
    dwRet = (pfnFormatVolume)(hPart, &diskInfo, pFormatOpts, FormatVolumeProgress, FormatVolumeMessage);

    // remount the partitoin
    if(!(pFormatOpts->dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !MountPartition(hPart)) {
        NKDbgPrintfW(L"MountPartition() failed! Error %u\r\n", GetLastError());
        dwRet = ERROR_GEN_FAILURE;
        goto done;
    }
    
done:    
    if(hUtilLib) FreeLibrary(hUtilLib);
    SetLastError(dwRet);
    return (ERROR_SUCCESS == dwRet);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL Stg_FormatPartEx (
    IN HANDLE hPart,
    IN PFORMAT_PARAMS pFormatParams
    )
{
    DWORD dwRet = ERROR_GEN_FAILURE;
    PFN_FORMATVOLUMEEX pfnFormatVolumeEx = NULL;
    WCHAR szLibrary[MAX_LIB_NAME_LEN] = L"";
    HINSTANCE hUtilLib = NULL;

    ASSERT(NULL != hPart && INVALID_HANDLE_VALUE != hPart);
    if(NULL == hPart || INVALID_HANDLE_VALUE == hPart) {
        return FALSE;
    }
    
    ASSERT(NULL != pFormatParams);
    if (NULL == pFormatParams) {
        // bad params
        return FALSE;
    }

    // load the designated utility libarry
    hUtilLib = LoadLibrary(LookupUtilName(hPart, szLibrary, MAX_LIB_NAME_LEN));
    if(!hUtilLib) {
        NKDbgPrintfW(L"LoadLibrary(%s) failed! Error %u\r\n", szLibrary, GetLastError());
        return FALSE;
    }

    // load the defrag function
    pfnFormatVolumeEx = (PFN_FORMATVOLUMEEX)GetProcAddressW(hUtilLib, FORMATEX_EXPORT_NAME);
    if(NULL == pfnFormatVolumeEx) {
        NKDbgPrintfW(L"GetProcAddress(0x%08X, \"%s\")\r\n", hUtilLib, FORMATEX_EXPORT_NAME);
        goto done;
    }

    // dismount the partition before formatting
    if(!(pFormatParams->fo.dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !DismountPartition(hPart)){
        NKDbgPrintfW(L"DismountPartition() failed! Error %u\r\n", GetLastError());
        goto done;
    }

    NKDbgPrintfW(L"format volume: tfat=%s full=%s fatver=%u roodirs=%u clusterbytes=%u\r\n",
        (pFormatParams->fo.dwFlags & FATUTIL_FORMAT_TFAT) ? L"TRUE" : L"FALSE",
        (pFormatParams->fo.dwFlags & FATUTIL_FULL_FORMAT) ? L"TRUE" : L"FALSE",
        pFormatParams->fo.dwFatVersion, pFormatParams->fo.dwRootEntries, pFormatParams->fo.dwClusSize);

    // ScanEx the partition
    dwRet = (pfnFormatVolumeEx)(hPart, pFormatParams);

    if(ERROR_SUCCESS == dwRet) {
        NKDbgPrintfW(L"format volume: total sectors:        %u\r\n", pFormatParams->fr.dwTotalSectors);
        NKDbgPrintfW(L"format volume: sectors per FAT:      %u\r\n", pFormatParams->fr.dwSectorsPerFat);
        NKDbgPrintfW(L"format volume: sectors per cluster:  %u\r\n", pFormatParams->fr.dwSectorsPerCluster);
        NKDbgPrintfW(L"format volume: root sectors:         %u\r\n", pFormatParams->fr.dwRootSectors);
        NKDbgPrintfW(L"format volume: reserved sectors:     %u\r\n", pFormatParams->fr.dwReservedSectors);
        NKDbgPrintfW(L"format volume: number of FATs:       %u\r\n", pFormatParams->fr.dwNumFats);
        NKDbgPrintfW(L"format volume: FAT version:          FAT%u\r\n", pFormatParams->fr.dwFatVersion);
        if(0 != pFormatParams->fr.dwSectorsPerCluster) {
            NKDbgPrintfW(L"format volume: clusters:             %u\r\n", pFormatParams->fr.dwTotalSectors / pFormatParams->fr.dwSectorsPerCluster);
        }
    }

    // remount the partition
    if(!(pFormatParams->fo.dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !MountPartition(hPart)) {
        NKDbgPrintfW(L"MountPartition() failed! Error %u\r\n", GetLastError());
        dwRet = ERROR_GEN_FAILURE;
        goto done;
    }
    
done:    
    if(hUtilLib) FreeLibrary(hUtilLib);
    SetLastError(dwRet);
    return (ERROR_SUCCESS == dwRet);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static VOID ScanVolumeProgress(DWORD dwPercent) 
{
    NKDbgPrintfW(L"scan volume: %u%% complete\r\n", dwPercent);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL ScanVolumeMessage(LPWSTR pszMessage, LPWSTR pszCaption, BOOL fYesNo)
{
    NKDbgPrintfW(L"scan volume: %s : %s %s\r\n", pszMessage, pszCaption, fYesNo);
    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL Stg_ScanPart (
    IN HANDLE hPart,
    IN PSCAN_OPTIONS pScanOpts
    )
{
    DWORD dwRet = ERROR_GEN_FAILURE;
    DWORD dwBytes = 0;
    DISK_INFO diskInfo = {0};
    PFN_SCANVOLUME pfnScanVolume = NULL;
    WCHAR szLibrary[MAX_LIB_NAME_LEN] = L"";
    HINSTANCE hUtilLib = NULL;

    ASSERT(NULL != hPart && INVALID_HANDLE_VALUE != hPart);
    if(NULL == hPart || INVALID_HANDLE_VALUE == hPart) {
        return FALSE;
    }

    // load the designated utility libarry
    hUtilLib = LoadLibrary(LookupUtilName(hPart, szLibrary, MAX_LIB_NAME_LEN));
    if(!hUtilLib) {
        NKDbgPrintfW(L"LoadLibrary(%s) failed! Error %u\r\n", szLibrary, GetLastError());
        return FALSE;
    }

    // load the scan function
    pfnScanVolume = (PFN_SCANVOLUME)GetProcAddressW(hUtilLib, SCAN_EXPORT_NAME);
    if(NULL == pfnScanVolume) {
        NKDbgPrintfW(L"GetProcAddress(0x%08X, \"%s\")\r\n", hUtilLib, SCAN_EXPORT_NAME);
        goto done;
    }

    
    if (!DeviceIoControl(hPart, DISK_IOCTL_GETINFO, &diskInfo, sizeof(DISK_INFO), 
        &diskInfo, sizeof(DISK_INFO), &dwBytes, NULL)) {
        NKDbgPrintfW(L"DeviceIoControl(DISK_IOCTL_GETINFO) failed! Error %u\r\n", GetLastError());
        goto done;
    }

    // dismount the partition before scanning
    if(!(pScanOpts->dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !DismountPartition(hPart)){
        NKDbgPrintfW(L"DismountPartition() failed! Error %u\r\n", GetLastError());
        goto done;
    }

    if(pScanOpts) {
        NKDbgPrintfW(L"scan volume: fat=%u verifyfix=%s\r\n", pScanOpts->dwFatToUse, 
            (pScanOpts->dwFlags & FATUTIL_SCAN_VERIFY_FIX) ? L"TRUE" : L"FALSE");
    }

    // scan the partition
    dwRet = (pfnScanVolume)(hPart, &diskInfo, pScanOpts, ScanVolumeProgress, ScanVolumeMessage);

    // remount the partition
    if(!(pScanOpts->dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !MountPartition(hPart)) {
        NKDbgPrintfW(L"MountPartition() failed! Error %u\r\n", GetLastError());
        dwRet = ERROR_GEN_FAILURE;
        goto done;
    }
    
done:    
    if(hUtilLib) FreeLibrary(hUtilLib);
    return (ERROR_SUCCESS == dwRet);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL Stg_ScanPartEx (
    IN HANDLE hPart,
    IN PSCAN_PARAMS pScanParams
    )
{
    DWORD dwRet = ERROR_GEN_FAILURE;
    PFN_SCANVOLUMEEX pfnScanVolumeEx = NULL;
    WCHAR szLibrary[MAX_LIB_NAME_LEN] = L"";
    HINSTANCE hUtilLib = NULL;

    ASSERT(NULL != hPart && INVALID_HANDLE_VALUE != hPart);
    ASSERT(NULL != pScanParams);
    if(NULL == hPart || INVALID_HANDLE_VALUE == hPart || NULL == pScanParams) {
        return FALSE;
    }

    // load the designated utility libarry
    hUtilLib = LoadLibrary(LookupUtilName(hPart, szLibrary, MAX_LIB_NAME_LEN));
    if(!hUtilLib) {
        NKDbgPrintfW(L"LoadLibrary(%s) failed! Error %u\r\n", szLibrary, GetLastError());
        return FALSE;
    }    

    // load the ScanEx function
    pfnScanVolumeEx = (PFN_SCANVOLUMEEX)GetProcAddressW(hUtilLib, SCANEX_EXPORT_NAME);
    if(NULL == pfnScanVolumeEx) {
        NKDbgPrintfW(L"GetProcAddress(0x%08X, \"%s\")\r\n", hUtilLib, SCANEX_EXPORT_NAME);
        goto done;
    }

    // dismount the partition before defragmenting
    if(!(pScanParams->so.dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !DismountPartition(hPart)){
        NKDbgPrintfW(L"DismountPartition() failed! Error %u\r\n", GetLastError());
        goto done;
    }
    
    NKDbgPrintfW(L"scan volume: fat=%u verifyfix=%s\r\n", pScanParams->so.dwFatToUse, 
        (pScanParams->so.dwFlags & FATUTIL_SCAN_VERIFY_FIX) ? L"TRUE" : L"FALSE");

    // ScanEx the partition
    dwRet = (pfnScanVolumeEx)(hPart, pScanParams);

    if(ERROR_SUCCESS == dwRet) {
        NKDbgPrintfW(L"defrag volume: lost clusters:       %u\r\n", pScanParams->sr.dwLostClusters);
        NKDbgPrintfW(L"defrag volume: invalid clusters:    %u\r\n", pScanParams->sr.dwInvalidClusters);
        NKDbgPrintfW(L"defrag volume: lost chains:         %u\r\n", pScanParams->sr.dwLostChains);
        NKDbgPrintfW(L"defrag volume: invalid directories: %u\r\n", pScanParams->sr.dwInvalidDirs);
        NKDbgPrintfW(L"defrag volume: invalid files:       %u\r\n", pScanParams->sr.dwInvalidFiles);
        NKDbgPrintfW(L"defrag volume: total errors:        %u\r\n", pScanParams->sr.dwTotalErrors);
        NKDbgPrintfW(L"defrag volume: percent fragmented:  %u%%\r\n", pScanParams->sr.dwPercentFrag);
        NKDbgPrintfW(L"defrag volume: all fats consistent: %s\r\n", pScanParams->sr.fConsistentFats ? L"TRUE" : L"FALSE");
        NKDbgPrintfW(L"defrag volume: errors not fixed:    %s\r\n", pScanParams->sr.fErrorNotFixed ? L"TRUE" : L"FALSE");
    }

    // remount the partition
    if(!(pScanParams->so.dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !MountPartition(hPart)) {
        NKDbgPrintfW(L"MountPartition() failed! Error %u\r\n", GetLastError());
        dwRet = ERROR_GEN_FAILURE;
        goto done;
    }
    
done:    
    if(hUtilLib) FreeLibrary(hUtilLib);
    return (ERROR_SUCCESS == dwRet);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static VOID DefragVolumeProgress(DWORD dwPercent) 
{
    NKDbgPrintfW(L"defrag volume: %u%% complete\r\n", dwPercent);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL DefragVolumeMessage(LPWSTR pszMessage, LPWSTR pszCaption, BOOL fYesNo)
{
    NKDbgPrintfW(L"defrag volume: %s : %s %s\r\n", pszMessage, pszCaption, fYesNo);
    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL Stg_MountPart (
    IN HANDLE hPart
    )
{
    ASSERT(NULL != hPart && INVALID_HANDLE_VALUE != hPart);
    if(NULL == hPart || INVALID_HANDLE_VALUE == hPart) {
        return FALSE;
    }

    return MountPartition(hPart);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL Stg_DismountPart(
    IN HANDLE hPart
    )
{
    ASSERT(NULL != hPart && INVALID_HANDLE_VALUE != hPart);
    if(NULL == hPart || INVALID_HANDLE_VALUE == hPart) {
        return FALSE;
    }

    return DismountPartition(hPart);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL Stg_DefragPart (
    IN HANDLE hPart,
    IN PDEFRAG_OPTIONS pDefragOpts
    )
{
    DWORD dwRet = ERROR_GEN_FAILURE;
    DWORD dwBytes = 0;
    DISK_INFO diskInfo = {0};
    PFN_UTILITY pfnDefragVolume = NULL;
    WCHAR szLibrary[MAX_LIB_NAME_LEN] = L"";
    HINSTANCE hUtilLib = NULL;

    ASSERT(NULL != hPart && INVALID_HANDLE_VALUE != hPart);
    if(NULL == hPart || INVALID_HANDLE_VALUE == hPart) {
        return FALSE;
    }

    // load the designated utility libarry
    hUtilLib = LoadLibrary(LookupUtilName(hPart, szLibrary, MAX_LIB_NAME_LEN));
    if(!hUtilLib) {
        NKDbgPrintfW(L"LoadLibrary(%s) failed! Error %u\r\n", szLibrary, GetLastError());
        return FALSE;
    }

    // load the defrag function
    pfnDefragVolume = (PFN_UTILITY)GetProcAddressW(hUtilLib, DEFRAG_EXPORT_NAME);
    if(NULL == pfnDefragVolume) {
        NKDbgPrintfW(L"GetProcAddress(0x%08X, \"%s\")\r\n", hUtilLib, DEFRAG_EXPORT_NAME);
        goto done;
    }
    
    if (!DeviceIoControl(hPart, DISK_IOCTL_GETINFO, &diskInfo, sizeof(DISK_INFO), 
        &diskInfo, sizeof(DISK_INFO), &dwBytes, NULL)) {
        NKDbgPrintfW(L"DeviceIoControl(DISK_IOCTL_GETINFO) failed! Error %u\r\n", GetLastError());
        goto done;
    }

    // dismount the partition before defragmenting
    if(!(pDefragOpts->dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !DismountPartition(hPart)){
        NKDbgPrintfW(L"DismountPartition() failed! Error %u\r\n", GetLastError());
        goto done;
    }

    NKDbgPrintfW(L"defrag volume: fat=%u verifyfix=%s\r\n", pDefragOpts->dwFatToUse, 
        (pDefragOpts->dwFlags & FATUTIL_SCAN_VERIFY_FIX) ? L"TRUE" : L"FALSE");

    // defragment the partition
    dwRet = (pfnDefragVolume)(hPart, &diskInfo, pDefragOpts, DefragVolumeProgress, DefragVolumeMessage);

    // remount the partitoin
    if(!(pDefragOpts->dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !MountPartition(hPart)) {
        NKDbgPrintfW(L"MountPartition() failed! Error %u\r\n", GetLastError());
        dwRet = ERROR_GEN_FAILURE;
        goto done;
    }
    
done:    
    if(hUtilLib) FreeLibrary(hUtilLib);
    return (ERROR_SUCCESS == dwRet);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL Stg_DefragPartEx (
    IN HANDLE hPart,
    IN PDEFRAG_PARAMS pDefragParams
    )
{
    DWORD dwRet = ERROR_GEN_FAILURE;
    PFN_DEFRAGVOLUMEEX pfnDefragVolumeEx = NULL;
    WCHAR szLibrary[MAX_LIB_NAME_LEN] = L"";
    HINSTANCE hUtilLib = NULL;

    ASSERT(NULL != hPart && INVALID_HANDLE_VALUE != hPart);
    ASSERT(NULL != pDefragParams);
    if(NULL == hPart || INVALID_HANDLE_VALUE == hPart || NULL == pDefragParams) {
        return FALSE;
    }

    // load the designated utility libarry
    hUtilLib = LoadLibrary(LookupUtilName(hPart, szLibrary, MAX_LIB_NAME_LEN));
    if(!hUtilLib) {
        NKDbgPrintfW(L"LoadLibrary(%s) failed! Error %u\r\n", szLibrary, GetLastError());
        return FALSE;
    }    

    // load the ScanEx function
    pfnDefragVolumeEx = (PFN_DEFRAGVOLUMEEX)GetProcAddressW(hUtilLib, DEFRAGEX_EXPORT_NAME);
    if(NULL == pfnDefragVolumeEx) {
        NKDbgPrintfW(L"GetProcAddress(0x%08X, \"%s\")\r\n", hUtilLib, DEFRAGEX_EXPORT_NAME);
        goto done;
    }

    // dismount the partition before defragmenting
    if(!(pDefragParams->dopt.dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !DismountPartition(hPart)){
        NKDbgPrintfW(L"DismountPartition() failed! Error %u\r\n", GetLastError());
        goto done;
    }

    NKDbgPrintfW(L"defrag volume: fat=%u verifyfix=%s\r\n", pDefragParams->dopt.dwFatToUse, 
        (pDefragParams->dopt.dwFlags & FATUTIL_SCAN_VERIFY_FIX) ? L"TRUE" : L"FALSE");

    // DefragEx the partition
    dwRet = (pfnDefragVolumeEx)(hPart, pDefragParams);

    if(ERROR_SUCCESS == dwRet) {
        NKDbgPrintfW(L"defrag volume: lost clusters:       %u\r\n", pDefragParams->dr.sr.dwLostClusters);
        NKDbgPrintfW(L"defrag volume: invalid clusters:    %u\r\n", pDefragParams->dr.sr.dwInvalidClusters);
        NKDbgPrintfW(L"defrag volume: lost chains:         %u\r\n", pDefragParams->dr.sr.dwLostChains);
        NKDbgPrintfW(L"defrag volume: invalid directories: %u\r\n", pDefragParams->dr.sr.dwInvalidDirs);
        NKDbgPrintfW(L"defrag volume: invalid files:       %u\r\n", pDefragParams->dr.sr.dwInvalidFiles);
        NKDbgPrintfW(L"defrag volume: total errors:        %u\r\n", pDefragParams->dr.sr.dwTotalErrors);
        NKDbgPrintfW(L"defrag volume: percent fragmented:  %u%%\r\n", pDefragParams->dr.sr.dwPercentFrag);
        NKDbgPrintfW(L"defrag volume: all fats consistent: %s\r\n", pDefragParams->dr.sr.fConsistentFats ? L"TRUE" : L"FALSE");
        NKDbgPrintfW(L"defrag volume: errors not fixed:    %s\r\n", pDefragParams->dr.sr.fErrorNotFixed ? L"TRUE" : L"FALSE");
    }

    // remount the partition
    if(!(pDefragParams->dopt.dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !MountPartition(hPart)) {
        NKDbgPrintfW(L"MountPartition() failed! Error %u\r\n", GetLastError());
        dwRet = ERROR_GEN_FAILURE;
        goto done;
    }
    
done:    
    if(hUtilLib) FreeLibrary(hUtilLib);
    return (ERROR_SUCCESS == dwRet);
}


namespace StoreHlp {

// Helper function to check the existence of a file/directory
BOOL IsFileExists(LPCWSTR szFile)
{
    return GetFileAttributes(szFile) == 0xFFFFFFFF ? FALSE : TRUE;
}

// Helper function: Generate random buffer one byte at a time
BOOL GenerateRandomPattern(__out_ecount(cbPattern) BYTE* pbPattern, 
                                    size_t cbPattern)
{
    SYSTEMTIME st = {0,};
    UINT uiRand = 0;

    // Set the seed number from time & tick count
    GetSystemTime(&st);
    srand(st.wSecond*1000 + GetTickCount());

    if (cbPattern == 0)
        return FALSE;

    for (UINT i = 0; i < cbPattern; i++)
    {
        rand_s(&uiRand);
        pbPattern[i] = uiRand % 256;
    }

    return TRUE;

}

// Helper function: Return true if the root path is either exFAT or texFAT
BOOL IsExFAT(LPCTSTR szRootPath)
{
    BOOL fRet = FALSE;
    CE_VOLUME_INFO volInfo = {0,};

    // First of all, get the current cluster's size
    if (!CeGetVolumeInfo(szRootPath, CeVolumeInfoLevelStandard, &volInfo))
    {
        NKDbgPrintfW(L"\tERROR: CeGetVolumeInfo(). Err=%u\r\n", GetLastError());
        goto done;
    }

    if (volInfo.dwFlags & CE_VOLUME_FLAG_64BIT_FILES_SUPPORTED)
    {
        fRet = TRUE;
        goto done;
    }

done:
    return fRet;
}

// Helper function: Return true if the root path is obj-store
BOOL IsObjStore(LPCTSTR szRootPath)
{
    BOOL fRet = FALSE;
    CE_VOLUME_INFO volInfo = {0,};

    // First of all, get the current cluster's size
    if (!CeGetVolumeInfo(szRootPath, CeVolumeInfoLevelStandard, &volInfo))
    {
        NKDbgPrintfW(L"\tERROR: CeGetVolumeInfo(). Err=%u\r\n", GetLastError());
        goto done;
    }

    if ( (volInfo.dwFlags & CE_VOLUME_FLAG_RAMFS))
    {
        fRet = TRUE;
        goto done;
    }

done:
    return fRet;
}


///////////////////////////////////////////////////////////////////////////////
//
// FillDiskSpace
//  Fill disk space x bytes
//  There are several methods of filling up space (determined by the enum parameter)
//
//  NOTE: Current limitation: OneFileWithWriteFile can only support writing a file < 4GB.
//
//  Parameters:
//      szRootPath(IN)  Test root path
//      ulBytes(IN)  Number of bytes to be filled
//      fillSpaceMethod(IN)  Method of filling disk space
//
//  Return value:
//      S_OK        Success
//
//      On failures:
//      E_INVALIDARG    Invalid parameter
//      E_FAIL          Check GetLastError() to find out further
//
///////////////////////////////////////////////////////////////////////////////
HRESULT FillDiskSpace (
    IN LPCWSTR szRootPath,
    IN ULARGE_INTEGER ulBytes,
    IN FillSpaceMethod fillSpaceMethod
    )
{
    HRESULT hr = E_FAIL;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    TCHAR szFullPath[MAX_PATH];
    TCHAR szFilePrefix[] = L"FillDisk";
    LONG lByteLo = 0;
    LONG lByteHi = 0;
    UINT i=0;
    UINT j=0;
    BYTE* pbPattern = NULL;
    size_t cbPattern = 0;
    DWORD dwBytes = 0;
    SYSTEM_INFO sysInfo;
    DWORD dwPageSize = 0;
    DWORD dwNumBytes = 0;
    DWORD dwNumWrites = 0;
    DWORD dwRemainder = 0;    

    // Create the test root directory as needed
    // Check to see if there's an existing file with same name (delete the file if it exists)
    if (IsFileExists(szRootPath))
    {
        if (!(GetFileAttributes(szRootPath) & FILE_ATTRIBUTE_DIRECTORY))
        {
            // Delete the file
            DeleteFile(szRootPath);
            CreateDirectory(szRootPath, NULL);
        }
    }
    else
        CreateDirectory(szRootPath, NULL);

    switch(fillSpaceMethod) 
    {        
        case OneFileWithSetFilePointer:
        case OneFileWithWriteFile:
            // Construct the file name
            i=0;
            lByteLo = ulBytes.LowPart;
            lByteHi = ulBytes.HighPart;

            do {
                StringCchPrintf(szFullPath, MAX_PATH, L"%s\\%s%u", szRootPath, szFilePrefix, ++i);

            } while (IsFileExists(szFullPath));
                
            // Create 1 test file
            NKDbgPrintfW(L"\tCreate file \"%s\"\r\n", szFullPath);
            hFile = CreateFile(szFullPath, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

            if (fillSpaceMethod == OneFileWithSetFilePointer)
            {
                // Advance file pointer by dwBytes        
                if(0xFFFFFFFF == SetFilePointer(hFile, lByteLo, &lByteHi, FILE_BEGIN) &&
                   NO_ERROR != GetLastError())
                {
                    NKDbgPrintfW(L"\tERROR: SetFilePointer(). Err=%u\r\n", GetLastError());
                    hr = E_FAIL;
                    goto EXIT;
                }
            }
            else
            {
                // This fille disk method only support writing a file < 4GB.                
                cbPattern = 10*1024; // Try to use 10k buffer

                // Generate random buffer for the pattern
                pbPattern = new BYTE[cbPattern];

                // Check for the result, bail out if we're out of memory
                if (!pbPattern)
                {
                    NKDbgPrintfW(L"\tERROR: Allocating memory\r\n");
                    goto EXIT;
                }

                GenerateRandomPattern(pbPattern, cbPattern);

                dwNumBytes = (DWORD) lByteLo;
                // Find out how many times we should write the pattern to the file
                dwNumWrites = dwNumBytes / cbPattern;
                dwRemainder = dwNumBytes % cbPattern;                

                // Write the pattern to the file
                for (DWORD i = 0; i < dwNumWrites; i++)
                {
                    if (!WriteFile(hFile, pbPattern, cbPattern, &dwBytes, NULL) || (dwBytes != cbPattern)) 
                    {
                        NKDbgPrintfW(L"\tERROR: WriteFile(). Err=%u\r\n", GetLastError());
                        goto EXIT;
                    }
                }

                // Write the remainder, 1 byte at a time
                for (DWORD i = 0; i < dwRemainder; i++)
                {
                    if (!WriteFile(hFile, &(pbPattern[i]), 1, &dwBytes, NULL) || (dwBytes != 1)) 
                    {
                        NKDbgPrintfW(L"\tERROR: WriteFile() on remainder. Err=%u\r\n", GetLastError());
                        goto EXIT;
                    }
                }

                delete [] pbPattern;
                pbPattern = NULL;
            }

            // Mark end of file
            if (!SetEndOfFile(hFile))
            {
                NKDbgPrintfW(L"\tERROR: SetEndOfFile(). Err=%u\r\n", GetLastError());
                hr = E_FAIL;
                goto EXIT;
            }
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;
                        
            break;


        case ManyFilesWithSetFilePointer:
        case ManyFilesWithWriteFile:

            // Chunk up fill space to multiple files
            ULARGE_INTEGER ulChunkSize;
            ULARGE_INTEGER ulCurrentByte;

            // Try to write 640k at a time
            ulChunkSize.LowPart = 640*1024;
            ulChunkSize.HighPart = 0;

            ulCurrentByte.LowPart = 0;
            ulCurrentByte.HighPart = 0;

            lByteLo = ulChunkSize.LowPart;
            lByteHi = ulChunkSize.HighPart;

            i=0;
            NKDbgPrintfW(L"\tCreate many files under \"%s\"\r\n", szRootPath);
            while ( (ulCurrentByte.QuadPart + ulChunkSize.QuadPart)< ulBytes.QuadPart)
            {
                // Create and write the file a chunk at a time
                do {
                    StringCchPrintf(szFullPath, MAX_PATH, L"%s\\%s%u", szRootPath, szFilePrefix, ++i);

                } while (IsFileExists(szFullPath));

                hFile = CreateFile(szFullPath, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

                if (fillSpaceMethod == ManyFilesWithSetFilePointer)
                {
                    // Advance file pointer by the specified chunk size
                    if(0xFFFFFFFF == SetFilePointer(hFile, lByteLo, &lByteHi, FILE_BEGIN) &&
                       NO_ERROR != GetLastError())
                    {
                        NKDbgPrintfW(L"\tERROR: [%d] SetFilePointer() on %s. Err=%u\r\n", j, szFullPath, GetLastError());
                        hr = E_FAIL;
                        goto EXIT;
                    }
                }
                else
                {
                    // This fille disk method only support writing a file < 4GB.                
                    cbPattern = ulChunkSize.LowPart;

                    // Generate random buffer for the pattern
                    pbPattern = new BYTE[cbPattern];

                    // Check for the result, bail out if we're out of memory
                    if (!pbPattern)
                    {
                        NKDbgPrintfW(L"\tERROR: Allocating memory\r\n");
                        goto EXIT;
                    }

                    GenerateRandomPattern(pbPattern, cbPattern);

                    if (!WriteFile(hFile, pbPattern, cbPattern, &dwBytes, NULL) || (dwBytes != cbPattern)) 
                    {
                        NKDbgPrintfW(L"\tERROR: WriteFile(). Err=%u\r\n", GetLastError());
                        goto EXIT;
                    }

                    delete [] pbPattern;
                    pbPattern = NULL;
                }

                // Mark end of file
                if (!SetEndOfFile(hFile))
                {
                    NKDbgPrintfW(L"\tERROR: SetEndOfFile(). Err=%u\r\n", GetLastError());
                    hr = E_FAIL;
                    goto EXIT;
                }
                CloseHandle(hFile);
                hFile = INVALID_HANDLE_VALUE;

                ulCurrentByte.QuadPart += ulChunkSize.QuadPart;
                j++;
            }

            NKDbgPrintfW(L"\tCreated %d files. Last file created=\"%s\"\r\n", j, szFullPath);

            // Write the remaining, if any
            if (ulBytes.QuadPart > ulCurrentByte.QuadPart)
            {
                // WriteFile ulBytes.QuadPart - ulCurrentByte.QuadPart
                // Generate file name
                do {
                    StringCchPrintf(szFullPath, MAX_PATH, L"%s\\%s%u", szRootPath, szFilePrefix, ++i);

                } while (IsFileExists(szFullPath));                

                // Write/SetFilePointer for the remaining bytes
                lByteLo = ulBytes.LowPart - ulCurrentByte.LowPart;
                lByteHi = 0;

                NKDbgPrintfW(L"\tCreating one more file of size %d, \"%s\"\r\n", lByteLo, szFullPath);
                hFile = CreateFile(szFullPath, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                
                if (fillSpaceMethod == ManyFilesWithSetFilePointer)
                {
                    // Advance file pointer by the specified chunk size
                    if(0xFFFFFFFF == SetFilePointer(hFile, lByteLo, &lByteHi, FILE_BEGIN) &&
                       NO_ERROR != GetLastError())
                    {
                        NKDbgPrintfW(L"\tERROR: [%d] SetFilePointer() on %s. Err=%u\r\n", j, szFullPath, GetLastError());
                        hr = E_FAIL;
                        goto EXIT;
                    }
                }
                else
                {
                    // This fille disk method only support writing a file < 4GB.                
                    cbPattern = lByteLo;

                    // Generate random buffer for the pattern
                    pbPattern = new BYTE[cbPattern];

                    // Check for the result, bail out if we're out of memory
                    if (!pbPattern)
                    {
                        NKDbgPrintfW(L"\tERROR: Allocating memory\r\n");
                        goto EXIT;
                    }

                    GenerateRandomPattern(pbPattern, cbPattern);

                    if (!WriteFile(hFile, pbPattern, cbPattern, &dwBytes, NULL) || (dwBytes != cbPattern)) 
                    {
                        NKDbgPrintfW(L"\tERROR: WriteFile(). Err=%u\r\n", GetLastError());
                        goto EXIT;
                    }

                    delete [] pbPattern;
                    pbPattern = NULL;
                }

                // Mark end of file
                if (!SetEndOfFile(hFile))
                {
                    NKDbgPrintfW(L"\tERROR: SetEndOfFile(). Err=%u\r\n", GetLastError());
                    hr = E_FAIL;
                    goto EXIT;
                }
                CloseHandle(hFile);
                hFile = INVALID_HANDLE_VALUE;
            }
            
            break;

        case OneFileWithWriteFileWithSeek:
            // Just write 100k at the end
            cbPattern = 100*1024;
            i=0;
            lByteLo = ulBytes.LowPart - cbPattern;
            lByteHi = ulBytes.HighPart;

            do {
                StringCchPrintf(szFullPath, MAX_PATH, L"%s\\%s%u", szRootPath, szFilePrefix, ++i);

            } while (IsFileExists(szFullPath));
                
            // Create 1 test file
            NKDbgPrintfW(L"\tCreate file \"%s\"\r\n", szFullPath);
            hFile = CreateFile(szFullPath, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

            // Generate random buffer for the pattern
            pbPattern = new BYTE[cbPattern];

            // Check for the result, bail out if we're out of memory
            if (!pbPattern)
            {
                NKDbgPrintfW(L"\tERROR: Allocating memory\r\n");
                goto EXIT;
            }

            GenerateRandomPattern(pbPattern, cbPattern);

            if (!WriteFileWithSeek(hFile, pbPattern, cbPattern, &dwBytes, NULL, lByteLo, lByteHi) || (dwBytes != cbPattern)) 
            {
                NKDbgPrintfW(L"\tERROR: WriteFileWithSeek(). Err=%u\r\n", GetLastError());
                goto EXIT;
            }

            delete [] pbPattern;
            pbPattern = NULL;

            // Mark end of file
            if (!SetEndOfFile(hFile))
            {
                NKDbgPrintfW(L"\tERROR: SetEndOfFile(). Err=%u\r\n", GetLastError());
                hr = E_FAIL;
                goto EXIT;
            }
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;

            break;

        case OneFileWriteFileGather:
            // Only support writing a file < 4GB for now
            FILE_SEGMENT_ELEMENT aWriteSeg[1];
            GetSystemInfo(&sysInfo);
            dwPageSize = sysInfo.dwPageSize;
            
            // Construct the file name
            i=0;
        
            do {
                StringCchPrintf(szFullPath, MAX_PATH, L"%s\\%s%u", szRootPath, szFilePrefix, ++i);

            } while (IsFileExists(szFullPath));
                
            // Create 1 test file
            NKDbgPrintfW(L"\tCreate file \"%s\"\r\n", szFullPath);
            hFile = CreateFile(szFullPath, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

            cbPattern = dwPageSize; // Use page size

            // Generate random buffer for the pattern
            pbPattern = new BYTE[cbPattern];

            // Check for the result, bail out if we're out of memory
            if (!pbPattern)
            {
                NKDbgPrintfW(L"\tERROR: Allocating memory\r\n");
                goto EXIT;
            }

            GenerateRandomPattern(pbPattern, cbPattern);

            aWriteSeg[0].Buffer = pbPattern;

            // Find out how many times we should write the pattern to the file
            dwNumBytes = ulBytes.LowPart;
            dwNumWrites = dwNumBytes / cbPattern;
            dwRemainder = dwNumBytes % cbPattern;                

            // Write the pattern to the file
            for (DWORD i = 0; i < dwNumWrites; i++)
            {
                if (!WriteFileGather(hFile, aWriteSeg, cbPattern, NULL, NULL))
                {
                    NKDbgPrintfW(L"\tERROR: WriteFileGather(). Err=%u (i=%d)\r\n", GetLastError(), i);
                    goto EXIT;
                }

            }

            // Write an extra page size if necessary
            if (dwRemainder > 0)
            {
                if (!WriteFileGather(hFile, aWriteSeg, cbPattern, NULL, NULL))
                {
                    NKDbgPrintfW(L"\tERROR: WriteFileGather() on remainder. Err=%u\r\n", GetLastError());
                    goto EXIT;
                }
            }

            delete [] pbPattern;
            pbPattern = NULL;
            
            // Mark end of file
            if (!SetEndOfFile(hFile))
            {
                NKDbgPrintfW(L"\tERROR: SetEndOfFile(). Err=%u\r\n", GetLastError());
                hr = E_FAIL;
                goto EXIT;
            }
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;

            break;
            
            // Feel free to add more methods

        default:
            NKDbgPrintfW(L"\tUnknown method [%u]\r\n", fillSpaceMethod);
            hr = E_FAIL;
        
            break;
    }

    hr = S_OK;

EXIT:
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);

    if(pbPattern)
        delete [] pbPattern;

    return hr;
}


///////////////////////////////////////////////////////////////////////////////
//
// FillDiskSpaceMinusXBytes
//  Fill disk space all the way and only leave x bytes left
//  There are several methods of filling up space (determined by the enum parameter)
//
//  Parameters:
//      szRootPath(IN)  Test root path
//      ulBytesLeft(IN)  Number of bytes to be left on disk
//      fillSpaceMethod(IN)  Method of filling disk space
//
//  Return value:
//      S_OK        Success
//
//      On failures:
//      E_INVALIDARG    Invalid parameter
//      E_FAIL          Check GetLastError() to find out further
//
///////////////////////////////////////////////////////////////////////////////
HRESULT FillDiskSpaceMinusXBytes (
    IN LPCWSTR szRootPath,
    IN ULARGE_INTEGER ulBytesLeft,
    IN FillSpaceMethod fillSpaceMethod
    )
{
    HRESULT hr = E_FAIL;
    ULARGE_INTEGER fb, tb, tfb, ulFillBytes;

    // Do calculation on how many bytes we need to fill. First, get the available disk space
    if(!GetDiskFreeSpaceEx(szRootPath, &fb, &tb, &tfb))
    {
        NKDbgPrintfW(L"GetDiskFreeSpaceEx Failed! Err=%u\r\n", GetLastError());
        hr = E_FAIL;
        goto EXIT;
    }

    if (fb.QuadPart > ulBytesLeft.QuadPart)
    {
        ulFillBytes.QuadPart = fb.QuadPart - ulBytesLeft.QuadPart;
        NKDbgPrintfW(L"\tFill disk space for %I64u bytes\r\n", ulFillBytes.QuadPart);
        hr = FillDiskSpace (szRootPath, ulFillBytes, fillSpaceMethod);
    }
    else
    {
        NKDbgPrintfW(L"\tNot enough space: requested [%I64u] > available [%I64u]\r\n", 
                        ulBytesLeft.QuadPart, fb.QuadPart);
        hr = E_FAIL;
        goto EXIT;
    }

EXIT:
    return hr;
}



///////////////////////////////////////////////////////////////////////////////
//
// FillDiskSpaceMinusXPercent
//  Fill disk space all the way and only leave x percent left
//  There are several methods of filling up space (determined by the enum parameter)
//
//  NOTE: the precission is 100 bytes
//
//  Parameters:
//      szRootPath(IN)  Test root path
//      dwPercentLeft(IN)  Percentage of disk space left
//      fillSpaceMethod(IN)  Method of filling disk space
//
//  Return value:
//      S_OK        Success
//
//      On failures:
//      E_INVALIDARG    Invalid parameter
//      E_FAIL          Check GetLastError() to find out further
//
///////////////////////////////////////////////////////////////////////////////
HRESULT FillDiskSpaceMinusXPercent (
    IN LPCWSTR szRootPath,
    IN DWORD dwPercentLeft,
    IN FillSpaceMethod fillSpaceMethod
    )
{
    HRESULT hr = E_FAIL;
    ULARGE_INTEGER fb, tb, tfb, ulFillBytes, ulBytesLeft, ulPercentLeft;

    // Do calculation on how many bytes we need to fill. First, get the available disk space
    if(!GetDiskFreeSpaceEx(szRootPath, &fb, &tb, &tfb))
    {
        NKDbgPrintfW(L"GetDiskFreeSpaceEx Failed! Err=%u\r\n", GetLastError());
        hr = E_FAIL;
        goto EXIT;
    }

    NKDbgPrintfW(L"\tFill disk space, leaving %d%% disk space\r\n", dwPercentLeft);

    // Do some math to figure out how many bytes fo fill
    ulPercentLeft.HighPart = 0;
    ulPercentLeft.LowPart = (ULONG) dwPercentLeft;
    ulBytesLeft.QuadPart = (ulPercentLeft.QuadPart * tb.QuadPart) / 100ULL;

    if (fb.QuadPart > ulBytesLeft.QuadPart)
    {
        ulFillBytes.QuadPart = fb.QuadPart - ulBytesLeft.QuadPart;
        NKDbgPrintfW(L"\tFill disk space for %I64u bytes\r\n", ulFillBytes.QuadPart);
        hr = FillDiskSpace (szRootPath, ulFillBytes, fillSpaceMethod);
    }
    else
    {
        NKDbgPrintfW(L"\tNot enough space: requested [%I64u] > available [%I64u]\r\n", 
                        ulBytesLeft.QuadPart, fb.QuadPart);
        hr = E_FAIL;
        goto EXIT;
    }

EXIT:
    return hr;
}



///////////////////////////////////////////////////////////////////////////////
//
// PrintFreeDiskSpace
//
//  Parameters:
//      szRootPath(IN)  Root path of volume to check free disk space
//
//  Return value:
//      TRUE if success, FALSE otherwise
//
///////////////////////////////////////////////////////////////////////////////
BOOL PrintFreeDiskSpace (LPCWSTR szRootPath)
{
    ULARGE_INTEGER fb, tb, tfb;
    BOOL fRet = FALSE;

    if(!GetDiskFreeSpaceEx(szRootPath, &fb, &tb, &tfb))
    {
        NKDbgPrintfW(L"GetDiskFreeSpaceEx Failed! Err=%u\r\n", GetLastError());
        goto EXIT;
    }

    NKDbgPrintfW(L"\tFree bytes avail to caller=%I64u\r\n", fb.QuadPart);
    NKDbgPrintfW(L"\tTotal bytes=%I64u\r\n", tb.QuadPart);
    NKDbgPrintfW(L"\tTotal free bytes=%I64u\r\n", tfb.QuadPart);

    fRet = TRUE;
    
EXIT:
    return fRet;
}


///////////////////////////////////////////////////////////////////////////////
//
// DeleteTreeRecursive
//      As DeleteDirectory deletes the directory only if its empty. 
//      This function delete all the files in the given directory(even the subdirectories 
//      and the files in it).
//
//  Parameters:
//      szPath(IN)  Path of the directory to be deleted
//      fDeleteRoot(IN)  Specify if the root should be deleted or not.
//
//  Return value:
//      TRUE if success, FALSE otherwise
//
///////////////////////////////////////////////////////////////////////////////
BOOL DeleteTreeRecursive(LPTSTR szPath, BOOL fDeleteRoot) 
{
    BOOL fResult = TRUE;
    size_t ccLen = 0;        
   
    // Locate the end of the path.
    StringCchLength(szPath, MAX_PATH, &ccLen);
    LPTSTR szAppend = szPath + ccLen;

    // Append our search spec to the path.
    StringCchCopy(szAppend, MAX_PATH, TEXT("*"));

    // Start the file/subdirectory find.
    WIN32_FIND_DATA w32fd;
    HANDLE hFind = FindFirstFile(szPath, &w32fd);

    // Loop on each file/subdirectory in this directory.
    if (hFind != INVALID_HANDLE_VALUE) 
    {
        do 
        {         
            // Check to see if the find is a directory.
            if (w32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {          
                // Make sure the directory is not "." or ".."
                if (_tcscmp(w32fd.cFileName, TEXT(".")) && _tcscmp(w32fd.cFileName, TEXT(".."))) 
                {
                    // Append the directory to our path and recursive into it.
                    StringCchCopy(szAppend, MAX_PATH, w32fd.cFileName);
                    StringCchCat(szAppend, MAX_PATH,TEXT("\\"));
                    fResult &= DeleteTreeRecursive(szPath, FALSE);
                }
                // Otherwise the find must be a file.
            }
            else 
            {
                // Append the file to our path.
                StringCchCopy(szAppend, MAX_PATH, w32fd.cFileName);

                // If the file is read-only, change to read/write.
                if (w32fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) 
                {
                   if (!SetFileAttributes(szPath, FILE_ATTRIBUTE_NORMAL)) 
                    NKDbgPrintfW(L"\tCould not set attributes: \"%s\" [%u]\r\n", szPath, GetLastError());
                }
                // Delete the file.
                if (!DeleteFile(szPath)) 
                {
                    NKDbgPrintfW(L"\tCould not delete: \"%s\" [%u]\r\n", szPath, GetLastError());
                    fResult = FALSE;
                }
                }

          // Get next file/directory.
        } while (FindNextFile(hFind, &w32fd));
      // Close our find.
      FindClose(hFind);
    }

    if (fDeleteRoot)
    {
        // Delete the directory;
        *(szAppend - 1) = TEXT('\0');
        if (!RemoveDirectory(szPath)) 
        {
            NKDbgPrintfW(L"\tCould not remove: \"%s\" [%u]\r\n", szPath, GetLastError());
            return FALSE;
        }
    }
    return fResult;
}


///////////////////////////////////////////////////////////////////////////////
//
// DeleteTree
//      This fucntion will in turn calls the DeleteTreeRecursize
//
//  Parameters:
//      szPath(IN)  Path of the directory to be deleted
//      fDeleteRoot(IN)  Specify if the root should be deleted or not.
//
//  Return value:
//      TRUE if success, FALSE otherwise
//
///////////////////////////////////////////////////////////////////////////////
BOOL DeleteTree(LPCTSTR szPath, BOOL fDeleteRoot) 
{
    TCHAR szBuf[MAX_PATH];
    size_t ccLen = 0;        

    NKDbgPrintfW(L"\tCleaning up \"%s\"...\r\n", szPath);

    // Copy the path to a temp buffer.
    StringCchCopy(szBuf,MAX_PATH,szPath);

    szBuf[MAX_PATH-1]=_T('\0');

    // Get the length of the path.
    StringCchLength(szPath, MAX_PATH, &ccLen);

    if(ccLen<=0)
    {
        NKDbgPrintfW(L"\tLength of the directory path is invalid\r\n");
        return FALSE;
    }
    // Ensure the path ends with a wack.
    if (szBuf[ccLen - 1] != TEXT('\\')) {
        szBuf[ccLen++] = TEXT('\\');
        szBuf[ccLen] = TEXT('\0');
    }

    return DeleteTreeRecursive(szBuf, fDeleteRoot);
}


BOOL IsPathWriteable( const TCHAR* szRootPath )
{
    BOOL bFileWritten = FALSE;
    TCHAR szFullPath[MAX_PATH];
    static const TCHAR* szFileName = L"f000.tmp";
    HANDLE hFile = INVALID_HANDLE_VALUE;         
    StringCchPrintf(szFullPath, MAX_PATH, L"%s\\%s", szRootPath, szFileName);
                
    NKDbgPrintfW(L"Create File (%s)\r\n", szFullPath);
    hFile = CreateFile(
        szFullPath, 
        GENERIC_READ | GENERIC_WRITE, 
        0, 
        NULL,
        CREATE_ALWAYS, 
        FILE_ATTRIBUTE_NORMAL, 
        NULL);

    if( hFile != INVALID_HANDLE_VALUE )
    {
        //Writable....
        bFileWritten = TRUE;
        CloseHandle( hFile );
        if( !DeleteFile( szFullPath ) )
        {
            NKDbgPrintfW(L"WARNING: Couldn't Delete File:(%s)\r\n", szFullPath);
        }        
        hFile = INVALID_HANDLE_VALUE;
    }

    return bFileWritten;

}//IsPathWriteable




//------------------------------------------------------------------------------
// Name:        OutputStoreInfo
// Description: Iterates through all registered stores dumping out the 
//                information associated with STOREINFO, PARTINFO and CE_VOLUME_INFO
//------------------------------------------------------------------------------
void OutputStoreInfo()
{
    NKDbgPrintfW(L"Reporting Device Storage and Partition Information....\r\n");
    STOREINFO sStoreInfo;
    PARTINFO partInfo;
    memset(&sStoreInfo, 0, sizeof(STOREINFO));
    sStoreInfo.cbSize = sizeof(STOREINFO);
    HANDLE hStore = INVALID_HANDLE_VALUE;
    HANDLE hOpenStore = INVALID_HANDLE_VALUE;
    HANDLE hPart = INVALID_HANDLE_VALUE;

    //Start by finding our first storage device
    hStore= FindFirstStore( &sStoreInfo );
    if( hStore == INVALID_HANDLE_VALUE )
    {
        NKDbgPrintfW(L"ERROR: No Registered Stores Found!\r\n");
        return;
    }
    
    do
    {
        //Debug out what we've got:
        DumpStoreInfo( sStoreInfo );

        hOpenStore = OpenStore(sStoreInfo.szDeviceName);
        if( hOpenStore == INVALID_HANDLE_VALUE )
        {    
            NKDbgPrintfW(L"ERROR: Could Not Open Store!\r\n");
            return;
        }

        //Look for each  partition...
        memset(&partInfo, 0, sizeof(PARTINFO));
        partInfo.cbSize = sizeof(PARTINFO);
        hPart = FindFirstPartition( hOpenStore , &partInfo );
        if( hOpenStore == INVALID_HANDLE_VALUE )
        {    
            NKDbgPrintfW(L"ERROR: Could Not Find Paritions!\r\n");
        }
        else
        {
            // For each removable partition...
            do
            {
                DumpPartInfo( partInfo );
                CE_VOLUME_INFO volInfo;
                CeGetVolumeInfo( partInfo.szVolumeName, CeVolumeInfoLevelStandard, &volInfo );
                DumpVolumeInfo( volInfo );
            } while( FindNextPartition(hPart, &partInfo) );
        }
        
        //Close our open
        CloseHandle( hOpenStore );
        NKDbgPrintfW(L".\r\n");
    }while( FindNextStore( hStore, &sStoreInfo ));
}//OutputStoreInfo


//------------------------------------------------------------------------------
// Name:        GetPartitionFatFormat
// Description: Determines the fat format for a given partition using the best
//              available method. 
//------------------------------------------------------------------------------
DISK_FORMAT_TYPE GetPartitionFatFormat( PARTINFO &sPartInfo  )
{
    DISK_FORMAT_TYPE sCurFatFormat = FORMAT_UNKNOWN;    
    FATINFO sFatInfo;
    
    TCHAR szVol[MAX_PATH];
    HANDLE hVol = INVALID_HANDLE_VALUE;
    DEVPB volpb = {0};
    DWORD cb = 0;
    
    swprintf_s(szVol, MAX_PATH, _T("%s\\VOL:"), sPartInfo.szVolumeName);
    hVol = ::CreateFile(szVol, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if(hVol == INVALID_HANDLE_VALUE)
    {
        NKDbgPrintfW(L" ERROR CreateFile(%s) FAILED\r\n", szVol);
        goto DONE;
    }
    else
    {
        if(!::DeviceIoControl(hVol, IOCTL_DISK_GET_VOLUME_PARAMETERS, NULL, 0, &volpb,
            sizeof(volpb), &cb, NULL) || cb != sizeof(volpb))
        {
            NKDbgPrintfW(L" ERROR DeviceIoControl(IOCTL_DISK_GET_VOLUME_PARAMETERS) FAILED\r\n");
            goto DONE;
        }
        else
        {
            
            sFatInfo.dwMediaDescriptor   = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_MediaDescriptor;
            sFatInfo.dwBytesPerSector    = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_BytesPerSector;
            sFatInfo.dwSectorsPerCluster = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_SectorsPerCluster;
            sFatInfo.dwTotalSectors      = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_TotalSectors ?
                (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_TotalSectors :
                (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_BigTotalSectors;
            sFatInfo.dwReservedSecters   = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_ReservedSectors;
            sFatInfo.dwHiddenSectors     = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_HiddenSectors;
            sFatInfo.dwSectorsPerTrack   = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_SectorsPerTrack;
            sFatInfo.dwHeads             = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_Heads;
            sFatInfo.dwSectorsPerFAT     = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_SectorsPerFAT;
            sFatInfo.dwNumberOfFATs      = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_NumberOfFATs;
            sFatInfo.dwRootEntries       = (DWORD)volpb.DevPrm.ExtDevPB.BPB.oldBPB.BPB_RootEntries;

            // calculate the FAT version by determining the total count of clusters
            DWORD dwNumClusters = 0;
            DWORD dwRootSectors = (32 * sFatInfo.dwRootEntries + sFatInfo.dwBytesPerSector - 1) / sFatInfo.dwBytesPerSector;
            DWORD dwStartDataSector = sFatInfo.dwReservedSecters + dwRootSectors + sFatInfo.dwNumberOfFATs * sFatInfo.dwSectorsPerFAT;
            DWORD dwDataSectors = sFatInfo.dwTotalSectors - dwStartDataSector;
            
            
            if( sFatInfo.dwSectorsPerCluster != 0 )
            {
                dwNumClusters = dwDataSectors / sFatInfo.dwSectorsPerCluster;
            }
            else
            {
                NKDbgPrintfW(L"!!!ERROR: DIVIDE BY ZERO ATTEMPT\r\n"); 
            }

            if(dwNumClusters >= 65525)
            {
                sCurFatFormat = FORMAT_FAT32;
            } 
            else if(dwNumClusters >= 4085)
            {
                sCurFatFormat = FORMAT_FAT16;
            }
            else if( (dwNumClusters < 4085) && (dwNumClusters > 0) )
            {
                sCurFatFormat = FORMAT_FAT12;
            }
            else
            {
                sCurFatFormat = FORMAT_UNKNOWN;
            }
        }
    }

DONE:
    return sCurFatFormat;
}//GetPartitionFatFormat


//
// This helper function gets storage path based on profile it passes
//
BOOL GetStoragePathByProfile(LPWSTR pszProfileName, LPWSTR pszPath, DWORD cchPath)
{
    BOOL fRet = FALSE;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    HANDLE hPartition = INVALID_HANDLE_VALUE;
    HANDLE hStore = INVALID_HANDLE_VALUE;
    STOREINFO storeInfo = {0};
    storeInfo.cbSize = sizeof(STOREINFO);
    PARTINFO partInfo = {0,}; 
    partInfo.cbSize = sizeof(partInfo);
    
    hFind = FindFirstStore(&storeInfo);

    if(INVALID_HANDLE_VALUE != hFind)
    {
        do
        {
            if(0 == _wcsnicmp(pszProfileName, storeInfo.sdi.szProfile, PROFILENAMESIZE))
            {
                hStore = OpenStore(storeInfo.szDeviceName);
                if(hStore == INVALID_HANDLE_VALUE)
                    break; 

                //Find first volume on store
                hPartition = FindFirstPartition(hStore, &partInfo);
                StringCchCopy(pszPath, cchPath, partInfo.szVolumeName);
                FindClosePartition(hPartition);
                CloseHandle(hStore);
                fRet = TRUE;
                break;
            }
        } while (FindNextStore(hFind, &storeInfo));
        
        VERIFY(FindCloseStore(hFind));
    }

    return fRet;
}


//
// Computes the file checksum by adding up DWORDs (and BYTEs if remaining bytes
// is less than DWORD size 
//
DWORD ComputeCheckSumDword(LPBYTE lpBuffer, DWORD cbBufferSize)
{
    DWORD *lpdwCurPointer = (DWORD*)lpBuffer;
    BYTE *pbCurPointer = NULL;
    DWORD dwBytesRemaining = cbBufferSize;
    DWORD dwCheckSum = 0;

    // If the buffer is empty
    if (!lpBuffer || dwBytesRemaining == 0)
        goto done;

    // Compute the DWORDs first
    while (dwBytesRemaining >= sizeof(DWORD))
    {
        dwCheckSum += *lpdwCurPointer;
        lpdwCurPointer++;
        dwBytesRemaining -= sizeof(DWORD);
    }

    // Compute the BYTEs
    pbCurPointer = (BYTE*) lpdwCurPointer;
    while (dwBytesRemaining > 0)
    {
        dwCheckSum += (DWORD) *pbCurPointer;
        pbCurPointer++;
        dwBytesRemaining --;
    }

done:
    return dwCheckSum;
}


//
// Get a DWORD-size checksum of the file specified by the parameter.
// Note: Only file < 4GB is supported
//
BOOL GetFileChecksum(LPCTSTR pszPath, LPDWORD lpdwFileChkSum)
{
    BOOL fRet = FALSE;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    PBYTE pBuffer = NULL;
    DWORD dwFileSize = 0;
    DWORD dwNextReadSize = 0;
    DWORD dwBytesRemaining = 0;
    DWORD dwBytesRead = 0;
    BYTE bActualCheckSum = 0;
    BOOL fLastRead = FALSE;

    *lpdwFileChkSum = 0;

    pBuffer = (PBYTE)LocalAlloc(LMEM_FIXED, TEST_READ_BUFFER_SIZE);
    if (!pBuffer)
    {
        NKDbgPrintfW(L"Error : StoreHlp::GetFileChecksum : Error allocating memory\r\n");
        goto done;
    }

    // Open the file
    hFile = CreateFile(
        pszPath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        NKDbgPrintfW(L"Error : StoreHlp::GetFileChecksum : CreateFile failed. Err=%u\r\n", GetLastError());
        goto done;
    }

    // Get file size
    dwFileSize = GetFileSize(hFile, NULL);
    dwBytesRemaining = dwFileSize;

    // Read in file data
    while(!fLastRead && dwBytesRemaining)
    {
        // Calculate read size
        if (dwBytesRemaining <= TEST_READ_BUFFER_SIZE)
        {
            dwNextReadSize = dwBytesRemaining;
            fLastRead = TRUE;
        }
        else
        {
            dwNextReadSize = TEST_READ_BUFFER_SIZE;
            fLastRead = FALSE;
        }
        
        if (!ReadFile(hFile, pBuffer, dwNextReadSize, &dwBytesRead, NULL) || dwBytesRead != dwNextReadSize)
        {
            NKDbgPrintfW(L"Error : StoreHlp::GetFileChecksum : ReadFile failed. ReadSize[%u] ReadPassed[%u] Err=%u\r\n", 
                dwBytesRead, dwNextReadSize, GetLastError());
            goto done;
        }

        // Compute new checksum
        *lpdwFileChkSum += ComputeCheckSumDword(pBuffer, dwBytesRead);
        dwBytesRemaining -= dwBytesRead;
    }

    fRet = TRUE;
done:
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);    
    if(pBuffer) LocalFree(pBuffer);
    return fRet;
}


}//namespace: StoreHlp

//------------------------------------------------------------------------------
// Name:        DumpVolumeInfo
// Description: Pretty dump of pertinent CE_VOLUME_INFO structure information
//------------------------------------------------------------------------------
void DumpVolumeInfo(CE_VOLUME_INFO& volInfo )
{
    NKDbgPrintfW(L".\r\n");
    NKDbgPrintfW(L"######VOLUME INFORMATION\r\n");        
    NKDbgPrintfW(L"...Partition Name:...(%s)\r\n", volInfo.szStoreName);
    NKDbgPrintfW(L"...Store Name:.......(%s)\r\n", volInfo.szPartitionName);
    NKDbgPrintfW(L"...Block Size:.......(%d)\r\n", volInfo.dwBlockSize);
    NKDbgPrintfW(L"...Volume Attributes:\r\n");
    Translate(volInfo.dwAttributes, &PrettyPrintVolumeAttrib);
    NKDbgPrintfW(L"...Volume Flags:\r\n");
    Translate(volInfo.dwFlags, &PrettyPrintVolumeFlags);
}//DumpVolumeInfo



//------------------------------------------------------------------------------
// Name:        DumpStoreInfo
// Description: Pretty dump of pertinent STOREINFO structure information
//------------------------------------------------------------------------------
void DumpStoreInfo(STOREINFO& sStoreInfo)
{
    SYSTEMTIME st;
        NKDbgPrintfW(L".\r\n");
        NKDbgPrintfW(L"##STORE INFORMATION\r\n");        
        NKDbgPrintfW(L"Device Name:.........(%s)\r\n", sStoreInfo.szDeviceName);
        NKDbgPrintfW(L"...Profile Name:.....(%s)\r\n", sStoreInfo.sdi.szProfile);
        NKDbgPrintfW(L"...Store Name:.......(%s)\r\n", sStoreInfo.szStoreName);
        NKDbgPrintfW(L"...Partition Count:..(%d)\r\n", sStoreInfo.dwPartitionCount);
        NKDbgPrintfW(L"...Mount Count:......(%d)\r\n", sStoreInfo.dwMountCount);
        NKDbgPrintfW(L"...Sector Count:.....(%ld)\r\n", sStoreInfo.snNumSectors);
        NKDbgPrintfW(L"...Bytes/Sector:.....(%d)\r\n", sStoreInfo.dwBytesPerSector);
        NKDbgPrintfW(L"...Free Sectors:.....(%ld)\r\n", sStoreInfo.snFreeSectors);
        NKDbgPrintfW(L"...Largest Free Blk:.(%ld)\r\n", sStoreInfo.snBiggestPartCreatable);
        FileTimeToSystemTime( &sStoreInfo.ftCreated, &st );
        NKDbgPrintfW(L"...Created:..........(%02d/%02d/%d %02d:%02d:%02d)\r\n", 
        st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond);
        FileTimeToSystemTime( &sStoreInfo.ftLastModified, &st );
        NKDbgPrintfW(L"...Last Modified:....(%02d/%02d/%d %02d:%02d:%02d)\r\n", 
        st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond);
        NKDbgPrintfW(L"...Device Type(s):\r\n");
        Translate(sStoreInfo.dwDeviceType, &PrettyPrintDevType);
        NKDbgPrintfW(L"...Device Flags:\r\n");
        Translate(sStoreInfo.dwDeviceFlags, &PrettyPrintDevFlags);
        NKDbgPrintfW(L"...Device Class:\r\n");
        Translate(sStoreInfo.dwDeviceClass, &PrettyPrintDevClass);
        NKDbgPrintfW(L"...Store Attributes:\r\n");
        Translate(sStoreInfo.dwAttributes, &PrettyPrintDevFlags);

}//DumpStoreInfo


//------------------------------------------------------------------------------
// Name:        DumpPartInfo
// Description: Pretty dump of pertinent PARTINFO structure information
//------------------------------------------------------------------------------
void DumpPartInfo(PARTINFO& sPartInfo)
{
    SYSTEMTIME st;
    
    NKDbgPrintfW(L".\r\n");
    NKDbgPrintfW(L"####PARTITION INFORMATION\r\n");        
    NKDbgPrintfW(L"Partition Name:......(%s)\r\n", sPartInfo.szPartitionName);
    NKDbgPrintfW(L"...File System:......(%s)\r\n", sPartInfo.szFileSys );
    NKDbgPrintfW(L"...Volume Name:......(%s)\r\n", sPartInfo.szVolumeName);
    NKDbgPrintfW(L"...Sector Count:.....(%d)\r\n", sPartInfo.snNumSectors);
    NKDbgPrintfW(L"...Sector Count:.....(%d)\r\n", sPartInfo.snNumSectors);
    FileTimeToSystemTime( &sPartInfo.ftCreated, &st );
    NKDbgPrintfW(L"...Created:..........(%02d/%02d/%d %02d:%02d:%02d)\r\n", 
        st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond);
    FileTimeToSystemTime( &sPartInfo.ftLastModified, &st );
    NKDbgPrintfW(L"...Last Modified:....(%02d/%02d/%d %02d:%02d:%02d)\r\n", 
        st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond);
    NKDbgPrintfW(L"...Attribute(s):\r\n");
    Translate( sPartInfo.dwAttributes, &PrettyPrintPartAttrib );
    NKDbgPrintfW(L"...Partition Type:\r\n");
    Translate(sPartInfo.bPartType, &PrettyPrintPartType);
        

}//DumpPartInfo


//------------------------------------------------------------------------------
// Name:        Translate
// Description: Common function which takes the flag to be decoded and a 
//                pointer to the decoding/printing function. Iterates through the
//                first (lsb - little endian) 32-bits of that flag masking each one
//                off and asking the callback to print to the display the human
//                readable name for it. 
//------------------------------------------------------------------------------
void Translate(DWORD dwValue, void (*printCallback)(DWORD) )
{
    if( printCallback )
    {
        DWORD  dwMask = 1;    
        BOOL bFoundType = FALSE;
        for( int i = 0; i<32;i++ )
        {
           if(dwValue & dwMask )
           {
                (*printCallback)( dwMask );
                bFoundType = TRUE;
           }
           dwMask <<= 1;
        }

        if( !bFoundType )
        {
            NKDbgPrintfW(L" ......(0x%08X) Not Recognized\r\n", dwValue);
        }
    }
    else
    {
        NKDbgPrintfW(L"Invalid Callback!!!\r\n");
    }
}//Translate


//------------------------------------------------------------------------------
// Name:        PrettyPrintDevType
// Description: Attribute pretty print
//------------------------------------------------------------------------------
void PrettyPrintDevType(DWORD dwMask)
{
    switch(dwMask)
    {
        case STORAGE_DEVICE_TYPE_ATA:
            NKDbgPrintfW(L"......STORAGE_DEVICE_TYPE_ATA\r\n");
            break;
        case STORAGE_DEVICE_TYPE_ATAPI:
            NKDbgPrintfW(L"......STORAGE_DEVICE_TYPE_ATAPI\r\n");
            break;
        case STORAGE_DEVICE_TYPE_CDROM:
            NKDbgPrintfW(L"......STORAGE_DEVICE_TYPE_CDROM\r\n");
            break;
        case STORAGE_DEVICE_TYPE_DOC:
            NKDbgPrintfW(L"......STORAGE_DEVICE_TYPE_DOC\r\n");
            break;
        case STORAGE_DEVICE_TYPE_DVD:
            NKDbgPrintfW(L"......STORAGE_DEVICE_TYPE_DVD\r\n");
            break;
        case STORAGE_DEVICE_TYPE_FLASH:
            NKDbgPrintfW(L"......STORAGE_DEVICE_TYPE_FLASH\r\n");
            break;
        case STORAGE_DEVICE_TYPE_PCIIDE:
            NKDbgPrintfW(L"......STORAGE_DEVICE_TYPE_PCIIDE\r\n");
            break;
        case STORAGE_DEVICE_TYPE_REMOVABLE_DRIVE:
            NKDbgPrintfW(L"......STORAGE_DEVICE_TYPE_REMOVABLE_DRIVE\r\n");
            break;
        case STORAGE_DEVICE_TYPE_REMOVABLE_MEDIA:
            NKDbgPrintfW(L"......STORAGE_DEVICE_TYPE_REMOVABLE_MEDIA\r\n");
            break;
        case STORAGE_DEVICE_TYPE_SRAM:
            NKDbgPrintfW(L"......STORAGE_DEVICE_TYPE_SRAM\r\n");
            break;
        case STORAGE_DEVICE_TYPE_UNKNOWN:
            NKDbgPrintfW(L"......STORAGE_DEVICE_TYPE_UNKNOWN\r\n");
            break;
        case STORAGE_DEVICE_TYPE_USB:
            NKDbgPrintfW(L"......STORAGE_DEVICE_TYPE_USB\r\n");
            break;
        default:
            NKDbgPrintfW(L"......UNKNOWN DEVICE (0x%08X)\r\n",dwMask);
            break;
    }
}//PrettyPrintDevType

//------------------------------------------------------------------------------
// Name:        PrettyPrintDevFlags
// Description: Decodes the structure attribute and prints out the result
//------------------------------------------------------------------------------
void PrettyPrintDevFlags( DWORD dwMask )
{
    switch( dwMask )
    {
    case STORAGE_DEVICE_FLAG_MEDIASENSE:
        NKDbgPrintfW(L"......STORAGE_DEVICE_FLAG_MEDIASENSE\r\n");
        break;
    case STORAGE_DEVICE_FLAG_READONLY:
        NKDbgPrintfW(L"......STORAGE_DEVICE_FLAG_READONLY\r\n");
        break;
    case STORAGE_DEVICE_FLAG_READWRITE:
        NKDbgPrintfW(L"......STORAGE_DEVICE_FLAG_READWRITE\r\n");
        break;
    case STORAGE_DEVICE_FLAG_TRANSACTED:
        NKDbgPrintfW(L"......STORAGE_DEVICE_FLAG_TRANSACTED\r\n");
        break;
     default:
         NKDbgPrintfW(L"......UNKNOWN DEVICE (0x%08X)\r\n",dwMask);
         break;
    }
}//PrettyPrintDevFlags

//------------------------------------------------------------------------------
// Name:        PrettyPrintDevClass
// Description: Pretty print attribute
//------------------------------------------------------------------------------
void PrettyPrintDevClass( DWORD dwMask)
{
    switch( dwMask)
    {
    case STORAGE_DEVICE_CLASS_BLOCK:
        NKDbgPrintfW(L"......STORAGE_DEVICE_CLASS_BLOCK\r\n");
        break;
    case STORAGE_DEVICE_CLASS_MULTIMEDIA:
        NKDbgPrintfW(L"......STORAGE_DEVICE_CLASS_MULTIMEDIA\r\n");   
        break;
    default:
         NKDbgPrintfW(L"......UNKNOWN DEVICE (0x%08X)\r\n",dwMask);
        break;
    }
}//PrettyPrintDevClass

//------------------------------------------------------------------------------
// Name:        PrettyPrintStoreAttrib
// Description: Decodes the structure attribute and prints out the result
//------------------------------------------------------------------------------
void PrettyPrintStoreAttrib(DWORD dwMask)
{
    switch( dwMask)
    {
    case STORE_ATTRIBUTE_AUTOFORMAT:
        NKDbgPrintfW(L"......STORE_ATTRIBUTE_AUTOFORMAT\r\n");
        break;
    case STORE_ATTRIBUTE_AUTOMOUNT:
        NKDbgPrintfW(L"......STORE_ATTRIBUTE_AUTOMOUNT\r\n");
        break;
    case STORE_ATTRIBUTE_AUTOPART:
        NKDbgPrintfW(L"......STORE_ATTRIBUTE_AUTOPART\r\n");
        break;
    case STORE_ATTRIBUTE_READONLY:
        NKDbgPrintfW(L"......STORE_ATTRIBUTE_READONLY\r\n");
        break;
    case STORE_ATTRIBUTE_REMOVABLE:
        NKDbgPrintfW(L"......STORE_ATTRIBUTE_REMOVABLE\r\n");
        break;
    case STORE_ATTRIBUTE_UNFORMATTED:
        NKDbgPrintfW(L"......STORE_ATTRIBUTE_UNFORMATTED\r\n");
        break;
    default:
        NKDbgPrintfW(L"......UNKNOWN ATTRIBUTE (0x%08X)\r\n",dwMask);
        break;
    }
}//PrettyPrintStoreAttrib


//------------------------------------------------------------------------------
// Name:        PrettyPrintStoreAttrib
// Description: Decodes the structure attribute and prints out the result
//------------------------------------------------------------------------------
void PrettyPrintVolumeAttrib(DWORD dwMask)
{
    switch( dwMask)
    {
    case CE_VOLUME_ATTRIBUTE_BOOT:
        NKDbgPrintfW(L"......CE_VOLUME_ATTRIBUTE_BOOT\r\n");
        break;
    case CE_VOLUME_ATTRIBUTE_HIDDEN:
        NKDbgPrintfW(L"......CE_VOLUME_ATTRIBUTE_HIDDEN\r\n");
        break;
    case CE_VOLUME_ATTRIBUTE_READONLY:
        NKDbgPrintfW(L"......CE_VOLUME_ATTRIBUTE_READONLY\r\n");
        break;
    case CE_VOLUME_ATTRIBUTE_REMOVABLE:
        NKDbgPrintfW(L"......CE_VOLUME_ATTRIBUTE_REMOVABLE\r\n");
        break;
    case CE_VOLUME_ATTRIBUTE_SYSTEM:
        NKDbgPrintfW(L"......CE_VOLUME_ATTRIBUTE_SYSTEM\r\n");
        break;
    default:
        NKDbgPrintfW(L"......UNKNOWN ATTRIBUTE (0x%08X)\r\n", dwMask);
        break;
    }
}//PrettyPrintVolumeAttrib


//------------------------------------------------------------------------------
// Name:        PrettyPrintStoreAttrib
// Description: Decodes the structure attribute and prints out the result
//------------------------------------------------------------------------------
void PrettyPrintVolumeFlags(DWORD dwMask)
{
    switch( dwMask)
    {
    case CE_VOLUME_FLAG_NETWORK:
        NKDbgPrintfW(L"......CE_VOLUME_FLAG_NETWORK\r\n");
        break;
    case CE_VOLUME_FLAG_LOCKFILE_SUPPORTED:
        NKDbgPrintfW(L"......CE_VOLUME_FLAG_LOCKFILE_SUPPORTED\r\n");
        break;
    case CE_VOLUME_FLAG_STORE:
        NKDbgPrintfW(L"......CE_VOLUME_FLAG_STORE\r\n");
        break;
    case CE_VOLUME_FLAG_WFSC_SUPPORTED:
        NKDbgPrintfW(L"......CE_VOLUME_FLAG_WFSC_SUPPORTED\r\n");
        break;
    case CE_VOLUME_TRANSACTION_SAFE:
        NKDbgPrintfW(L"......CE_VOLUME_TRANSACTION_SAFE\r\n");
        break;
    case CE_VOLUME_FLAG_RAMFS:
        NKDbgPrintfW(L"......CE_VOLUME_FLAG_RAMFS\r\n");
        break;
    case CE_VOLUME_FLAG_FILE_SECURITY_SUPPORTED:
        NKDbgPrintfW(L"......CE_VOLUME_FLAG_FILE_SECURITY_SUPPORTED\r\n");
        break;
    case CE_VOLUME_FLAG_64BIT_FILES_SUPPORTED:
        NKDbgPrintfW(L"......CE_VOLUME_FLAG_64BIT_FILES_SUPPORTED\r\n");
        break;
    default:
        NKDbgPrintfW(L"......UNKNOWN FLAG (0x%08X)\r\n", dwMask);
        break;
    }
}//PrettyPrintStoreAttrib

//------------------------------------------------------------------------------
// Name:        PrettyPrintPartAttrib
// Description: Pretty dump of pertinent PARTINFO structure information
//------------------------------------------------------------------------------
void PrettyPrintPartAttrib(DWORD dwMask)
{
    switch( dwMask)
    {
    case PARTITION_ATTRIBUTE_EXPENDABLE:
        NKDbgPrintfW(L"......PARTITION_ATTRIBUTE_EXPENDABLE\r\n");
        break;
    case PARTITION_ATTRIBUTE_READONLY:
        NKDbgPrintfW(L"......PARTITION_ATTRIBUTE_READONLY\r\n");
        break;
    case PARTITION_ATTRIBUTE_BOOT :
        NKDbgPrintfW(L"......PARTITION_ATTRIBUTE_BOOT\r\n");
        break;
    case PARTITION_ATTRIBUTE_AUTOFORMAT :
        NKDbgPrintfW(L"......PARTITION_ATTRIBUTE_AUTOFORMAT\r\n");
        break;
    case PARTITION_ATTRIBUTE_MOUNTED :
        NKDbgPrintfW(L"......PARTITION_ATTRIBUTE_MOUNTED\r\n");
        break;
    default:
        NKDbgPrintfW(L"......UNKNOWN ATTRIBUTE (0x%08X)\r\n",dwMask);
        break;
    }
}//PrettyPrintPartAttrib


//------------------------------------------------------------------------------
// Name:        PrettyPrintPartType
// Description: Pretty dump of pertinent PARTINFO structure information
//------------------------------------------------------------------------------
void PrettyPrintPartType(DWORD dwMask)
{
    switch( dwMask)
    {
    case PART_UNKNOWN:
        NKDbgPrintfW(L"......PART_UNKNOWN\r\n");
        break;
    case PART_DOS2_FAT:
        NKDbgPrintfW(L"......PART_DOS2_FAT\r\n");
        break;
    case PART_DOS3_FAT :
        NKDbgPrintfW(L"......PART_DOS3_FAT\r\n");
        break;
    case PART_DOS4_FAT:
        NKDbgPrintfW(L"......PART_DOS4_FAT\r\n");
        break;
    case PART_DOS32:
        NKDbgPrintfW(L"......PART_DOS32\r\n");
        break;
    case PART_DOS32X13:
        NKDbgPrintfW(L"......PART_DOS32X13\r\n");
        break;
    case PART_DOSX13 :
        NKDbgPrintfW(L"......PART_DOSX13\r\n");
        break;
    case PART_DOSX13X:
        NKDbgPrintfW(L"......PART_DOSX13X\r\n");
        break;
    default:
        NKDbgPrintfW(L"......UNKNOWN ATTRIBUTE (0x%08X)\r\n",dwMask);
        break;
    }
}//PrettyPrintPartType


// Implementation for CTestFileList helper class


//
// Initialize function
//
void CTestFileList::Init()
{
    m_pHead = NULL;
    m_pTail = NULL;
    m_pCurrent = NULL;

    InitializeCriticalSection(&m_csTestFileList);
}


//
// De-initialize function
//
void CTestFileList::Deinit()
{
    // Destroy the list
    Destroy();
    DeleteCriticalSection(&m_csTestFileList);
}


//
// Default constructor
//
CTestFileList::CTestFileList()
{
    Init();
}


//
// Destructor
//
CTestFileList::~CTestFileList()
{
    Deinit();
}

//Though we have this function as part of thelper.h function, using that function here will break all the other tests that are using storehlp
//so i am duplicating the code here
//+ ==============================================================================
//This function should be right after DeleteFile failed because of access denied, in case if you want to delete it anyway
//  Create file with FILE_FLAG_DELETE_ON_CLOSE
//  returns FALSE on failure
//- ==============================================================================
BOOL SetDeleteOnClose(wchar_t * szFile)
{
    HANDLE hFile  =  INVALID_HANDLE_VALUE;
    BOOL bRet = FALSE;
    DWORD dwLastError = GetLastError();    
    if(GetLastError() == ERROR_SHARING_VIOLATION)
    {
        hFile  = CreateFile(szFile, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL |FILE_FLAG_WRITE_THROUGH| FILE_FLAG_DELETE_ON_CLOSE, NULL);
        if (INVALID_HANDLE_VALUE == hFile)
        {
            hFile  = CreateFile(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH|FILE_FLAG_DELETE_ON_CLOSE, NULL);
            if (INVALID_HANDLE_VALUE == hFile)
            {
                NKDbgPrintfW(L"DeleteOnClose: create file %s failed,Error: %u\r\n",szFile,GetLastError());
                goto ErrorReturn;
            }
        }
        if(!CloseHandle(hFile))
        {
            NKDbgPrintfW(L"DeleteOnClose: CloseHandle on %s failed,Error: %u\r\n",szFile,GetLastError());
            goto ErrorReturn;
        }
    }
    bRet = TRUE;
ErrorReturn:
    SetLastError(dwLastError);
    return bRet;
}

//
// Destroy all nodes and delete all files/directories
//
BOOL CTestFileList::Destroy()
{
    BOOL fRet = TRUE;
    TEST_FILE_NODE* pCurrent = NULL;

    EnterCriticalSection(&m_csTestFileList);

    // Go through each node and delete
    while (m_pHead)
    {
        pCurrent = m_pHead;
        m_pHead = m_pHead->pNext;

        if (pCurrent->tfi.fDirectory)
        {
            //NKDbgPrintfW(L"CTestFileList: RemoveDirectory %s", pCurrent->tfi.szFullPath));
            if (!RemoveDirectory(pCurrent->tfi.szFullPath))    
            {
                NKDbgPrintfW(L"CTestFileList: RemoveDirectory on %s failed. Err=%u\r\n", pCurrent->tfi.szFullPath, GetLastError());
                fRet = FALSE;
            }
        }
        else
        {
            //NKDbgPrintfW(L"CTestFileList: DeleteFile %s", pCurrent->tfi.szFullPath));
            if (!DeleteFile(pCurrent->tfi.szFullPath))
            {
                NKDbgPrintfW(L"CTestFileList: DeleteFile on %s failed. Err=%u\r\n", pCurrent->tfi.szFullPath, GetLastError());
                SetDeleteOnClose(pCurrent->tfi.szFullPath);
                fRet = FALSE;
            }
        }

        delete pCurrent;
    }

    m_pTail = NULL;
    m_pCurrent = NULL;

    LeaveCriticalSection(&m_csTestFileList);
    return fRet;
}


//
// Delete
//    Find a node that matches the path parameter, delete file/dir, and delete the node itself
//
// Parameter:
//    szFullPath: (IN) Full path to file/directory to be deleted
//
// Return: True if successful, False otherwise.
//
// NOTE: DO NOT USE This method with ReadFirst/Next
//
BOOL CTestFileList::Delete(LPCTSTR szFullPath)
{
    BOOL fRet = FALSE;
    TEST_FILE_NODE* pCurrent = m_pHead;
    TEST_FILE_NODE* pPrevious = m_pHead;
    BOOL fFound = FALSE;

    EnterCriticalSection(&m_csTestFileList);

    // Go through each node and search for matching path
    while (pCurrent)
    {
        if (0 == _tcsncmp(szFullPath, pCurrent->tfi.szFullPath, MAX_PATH))
        {
            if (pCurrent->tfi.fDirectory)
            {
                //NKDbgPrintfW(L"CTestFileList::Delete : RemoveDirectory %s", pCurrent->tfi.szFullPath));
                if (!RemoveDirectory(pCurrent->tfi.szFullPath))    
                {
                    NKDbgPrintfW(L"CTestFileList::Delete : RemoveDirectory on %s failed. Err=%u\r\n", pCurrent->tfi.szFullPath, GetLastError());
                    fRet = FALSE;
                }
            }
            else
            {
                //NKDbgPrintfW(L"CTestFileList::Delete : DeleteFile %s", pCurrent->tfi.szFullPath));
                if (!DeleteFile(pCurrent->tfi.szFullPath))
                {
                    NKDbgPrintfW(L"CTestFileList::Delete : DeleteFile on %s failed. Err=%u\r\n", pCurrent->tfi.szFullPath, GetLastError());
                    fRet = FALSE;
                }
            }

            if (pCurrent == m_pHead)
            {
                m_pHead = pCurrent->pNext;                
            }
            else
            {
                pPrevious->pNext = pCurrent->pNext;
            }

            if (pCurrent == m_pTail) 
            {
                if (!m_pHead)
                    m_pTail = NULL;
                else
                    m_pTail = pPrevious;
            }
            
            delete pCurrent;
            fFound = TRUE;
            break;
        }

        pPrevious = pCurrent;
        pCurrent = pCurrent->pNext;
    }

    LeaveCriticalSection(&m_csTestFileList);

    if (!fFound)
    {
        NKDbgPrintfW(L"CTestFileList::Delete : Could not find %s in the file list\r\n", szFullPath);
        goto done;
    }

    fRet = TRUE;
done:
    return fRet;
}


//
// Delete all nodes and delete all files/directories
// This method is accessible from public
//
BOOL CTestFileList::DeleteAll()
{
    NKDbgPrintfW(L"CTestFileList: Delete all test files/directories. This might take some time...\r\n");
    return Destroy();
}


//
// Create
//    Create a new node, create a new file/directory as specified by the parameter
//
// Parameter:
//    fDirectory: (IN) Specify to TRUE to create directory
//    szFullPath: (IN) Full path to file/directory to be created
//    dwAttrib: (IN) File attribute to be created
//    dwSize: (IN) Size of file in bytes. Will be ignored when creating a directory
//
// Return: True if successful, False otherwise.
//
// Note: This method only supports writing a file < 4GB
//
BOOL CTestFileList::Create(BOOL fDirectory, LPCTSTR szFullPath, DWORD dwAttrib, DWORD dwSize)
{
    BOOL fRet = FALSE;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    BYTE* pbPattern = NULL;
    size_t cbPattern = 0;
    DWORD dwBytes = 0;
    DWORD dwNumWrites = 0;
    DWORD dwRemainder = 0;
    DWORD dwCheckSum = 0;
    BOOL fFileCreated = FALSE;
    
    EnterCriticalSection(&m_csTestFileList);

    // Create the test file/directory first, make sure it succeed before creating a new node to keep track
    if (fDirectory)
    {
        //NKDbgPrintfW(L"CTestFileList: CreateDirectory %s", szFullPath));
        if (!CreateDirectory(szFullPath, NULL))
        {
            NKDbgPrintfW(L"CTestFileList: CreateDirectory on %s failed. Err=%u\r\n", szFullPath, GetLastError());
            goto done;
        }
    }
    else
    {
        // Create a new file. Fail if existing file with same name already exists
        //NKDbgPrintfW(L"CTestFileList: CreateFile %s", szFullPath));
        hFile = CreateFile(szFullPath, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                        CREATE_NEW, dwAttrib, NULL);

        if (hFile == INVALID_HANDLE_VALUE)
        {
            NKDbgPrintfW(L"CTestFileList: CreateFile on %s failed. Err=%u\r\n", szFullPath, GetLastError());
            goto done;
        }
        fFileCreated = TRUE;

        // Write some data to the file        
        cbPattern = TEST_WRITE_BUFFER_SIZE;

        // Generate random buffer for the pattern
        pbPattern = new BYTE[cbPattern];

        // Check for the result, bail out if we're out of memory
        if (!pbPattern)
        {
            NKDbgPrintfW(L"CTestFileList::Create ERROR allocating memory\r\n");
            goto done;
        }

        StoreHlp::GenerateRandomPattern(pbPattern, cbPattern);

        // Find out how many times we should write the pattern to the file
        dwNumWrites = dwSize / cbPattern;
        dwRemainder = dwSize % cbPattern;                

        // Write the pattern to the file. The idea is to always try to write with biggest buffer for performance reason
        // We don't want to do too much 1-byte writing at a time
        do 
        {
            for (DWORD i = 0; i < dwNumWrites; i++)
            {
                if (!WriteFile(hFile, pbPattern, cbPattern, &dwBytes, NULL) || (dwBytes != cbPattern)) 
                {
                    NKDbgPrintfW(L"ERROR: CTestFileList: WriteFile on %s failed. Err=%u\r\n", szFullPath, GetLastError());
                    goto done;
                }
            }

            if (cbPattern <= 32)
                break;

            // Write buffer at half chunk of the original buffer size for perf reason
            cbPattern = cbPattern / 2;
            dwNumWrites = dwRemainder / cbPattern;
            dwRemainder = dwRemainder % cbPattern;

            if (dwNumWrites)
                StoreHlp::GenerateRandomPattern(pbPattern, cbPattern);

        } while (cbPattern >= 32); 

        // Write the remainder, 1 byte at a time
        for (DWORD i = 0; i < dwRemainder; i++)
        {
            if (!WriteFile(hFile, &(pbPattern[i]), 1, &dwBytes, NULL) || (dwBytes != 1)) 
            {
                NKDbgPrintfW(L"ERROR: CTestFileList: WriteFile on %s failed (on remainder). Err=%u\r\n", szFullPath, GetLastError());
                goto done;
            }
        }

        // Mark end of file
        if (!SetEndOfFile(hFile))
        {
            NKDbgPrintfW(L"ERROR: CTestFileList: SetEndOfFile on %s failed. Err=%u\r\n", szFullPath, GetLastError());
            goto done;
        }
        FlushFileBuffers(hFile);
        CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;

        // Compute the checksum of the file        
        if (!StoreHlp::GetFileChecksum(szFullPath, &dwCheckSum))
        {
            NKDbgPrintfW(L"CTestFileList::Create ERROR getting file check sum on %s\r\n", szFullPath);
            goto done;
        }
        
    }

    // If we make it this far, create a new node 
    TEST_FILE_NODE* pTempNode = new TEST_FILE_NODE;

    if (!pTempNode)
    {
        NKDbgPrintfW(L"CTestFileList::Create ERROR allocating memory\r\n");
        goto done;
    }

    StringCchCopy(pTempNode->tfi.szFullPath, MAX_PATH, szFullPath);
    pTempNode->tfi.fDirectory = fDirectory;
    pTempNode->tfi.dwAttribute = GetFileAttributes(pTempNode->tfi.szFullPath);
    if (fDirectory)
    {
        pTempNode->tfi.dwSize = 0;
        pTempNode->tfi.dwCheckSum = 0;
    }
    else
    {
        pTempNode->tfi.dwSize = dwSize;
        pTempNode->tfi.dwCheckSum = dwCheckSum;
    }
    
    pTempNode->pNext = NULL;

    // Check if the list is empty
    if (!m_pHead)
        m_pHead = pTempNode;

    if (!m_pTail)
        m_pTail = m_pHead;
    else
    {
        m_pTail->pNext = pTempNode;
        m_pTail = pTempNode;
    }

    fRet = TRUE;

done:
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    if(pbPattern) delete [] pbPattern;
    if (!fRet && !fDirectory && fFileCreated) DeleteFile(szFullPath);    // Delete the file if something's wrong in file creation
    LeaveCriticalSection(&m_csTestFileList);
    return fRet;
}


//
// Resize
//    Find a node that matches the path parameter, resize the file
//
// Parameter:
//    szFullPath: (IN) Full path to file/directory to be resized
//    dwSize: (IN) Size in bytes
//
// Return: True if successful, False otherwise.
//
// Note: This method only supports resizing file < 4GB in size
//
BOOL CTestFileList::Resize(LPCTSTR szFullPath, DWORD dwSize)
{
    BOOL fRet = TRUE;
    TEST_FILE_NODE* pCurrent = m_pHead;
    BOOL fFound = FALSE;
    HANDLE hFile = INVALID_HANDLE_VALUE;

    EnterCriticalSection(&m_csTestFileList);

    // Go through each node and search for matching path
    while (pCurrent)
    {
        if (0 == _tcsncmp(szFullPath, pCurrent->tfi.szFullPath, MAX_PATH))
        {
            if (pCurrent->tfi.fDirectory)
            {
                NKDbgPrintfW(L"CTestFileList::Resize : Cannot resize directory %s\r\n", pCurrent->tfi.szFullPath);
                fRet = FALSE;
                goto done;
            }
            else
            {
                //NKDbgPrintfW(L"CTestFileList::Resize : Resize file %s to %u bytes", pCurrent->tfi.szFullPath, dwSize));

                // First, get the file handle
                hFile = CreateFile(pCurrent->tfi.szFullPath, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

                if (hFile == INVALID_HANDLE_VALUE)
                {
                    NKDbgPrintfW(L"CTestFileList:Resize : CreateFile on %s failed. Err=%u\r\n", szFullPath, GetLastError());
                    fRet = FALSE;
                    goto done;
                }

                if (0xFFFFFFFF == SetFilePointer(hFile, dwSize, NULL, FILE_BEGIN) && GetLastError() != NO_ERROR)
                {
                    NKDbgPrintfW(L"CTestFileList::Resize : SetFilePointer on %s failed. Err=%u\r\n", pCurrent->tfi.szFullPath, GetLastError());
                    fRet = FALSE;
                }

                if (!SetEndOfFile(hFile))
                {
                    NKDbgPrintfW(L"CTestFileList::Resize : SetEndOfFile on %s failed. Err=%u\r\n", pCurrent->tfi.szFullPath, GetLastError());
                    fRet = FALSE;
                }
                FlushFileBuffers(hFile);
                CloseHandle(hFile);
                hFile = INVALID_HANDLE_VALUE;

                // Update size and checksum information
                if (!StoreHlp::GetFileChecksum(pCurrent->tfi.szFullPath, &(pCurrent->tfi.dwCheckSum)))
                {
                    NKDbgPrintfW(L"CTestFileList::Resize ERROR getting file check sum on %s\r\n", pCurrent->tfi.szFullPath);
                    fRet = FALSE;
                }

                if (fRet)
                    pCurrent->tfi.dwSize = dwSize;
            }

            fFound = TRUE;
            break;
        }

        pCurrent = pCurrent->pNext;
    }

    LeaveCriticalSection(&m_csTestFileList);

    if (!fFound)
    {
        NKDbgPrintfW(L"CTestFileList::Resize : Could not find %s in the file list\r\n", szFullPath);
        fRet = FALSE;
    }

done:
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    return fRet;
}


//
// Append
//    Find a node that matches the path parameter, append/write some data at the end of the file
//
// Parameter:
//    szFullPath: (IN) Full path to file/directory to be resized
//    dwAppendSize: (IN) Append size in bytes
//
// Return: True if successful, False otherwise.
//
//
BOOL CTestFileList::Append(LPCTSTR szFullPath, DWORD dwAppendSize)
{
    BOOL fRet = TRUE;
    TEST_FILE_NODE* pCurrent = m_pHead;
    BOOL fFound = FALSE;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    BYTE* pbPattern = NULL;
    size_t cbPattern = 0;
    DWORD dwBytes = 0;
    DWORD dwNumWrites = 0;
    DWORD dwRemainder = 0;
    DWORD dwCheckSum = 0;

    EnterCriticalSection(&m_csTestFileList);

    // Go through each node and search for matching path
    while (pCurrent)
    {
        if (0 == _tcsncmp(szFullPath, pCurrent->tfi.szFullPath, MAX_PATH))
        {
            if (pCurrent->tfi.fDirectory)
            {
                NKDbgPrintfW(L"CTestFileList::Append : Cannot append directory %s\r\n", pCurrent->tfi.szFullPath);
                fRet = FALSE;
                goto done;
            }
            else
            {
                //NKDbgPrintfW(L"CTestFileList::Append : Append file %s to %u bytes", pCurrent->tfi.szFullPath, dwSize));

                // First, get the file handle
                hFile = CreateFile(pCurrent->tfi.szFullPath, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

                if (hFile == INVALID_HANDLE_VALUE)
                {
                    NKDbgPrintfW(L"CTestFileList:Append : CreateFile on %s failed. Err=%u\r\n", szFullPath, GetLastError());
                    fRet = FALSE;
                    goto done;
                }

                // Set the file pointer to the end of the file
                if (0xFFFFFFFF == SetFilePointer(hFile, 0, NULL, FILE_END) && GetLastError() != NO_ERROR)
                {
                    NKDbgPrintfW(L"CTestFileList::Append : SetFilePointer on %s failed. Err=%u\r\n", pCurrent->tfi.szFullPath, GetLastError());
                    fRet = FALSE;
                }


                // Write some data to the file        
                cbPattern = TEST_WRITE_BUFFER_SIZE;

                // Generate random buffer for the pattern
                pbPattern = new BYTE[cbPattern];

                // Check for the result, bail out if we're out of memory
                if (!pbPattern)
                {
                    NKDbgPrintfW(L"CTestFileList::Append ERROR allocating memory\r\n");
                    goto done;
                }

                StoreHlp::GenerateRandomPattern(pbPattern, cbPattern);

                // Find out how many times we should write the pattern to the file
                dwNumWrites = dwAppendSize / cbPattern;
                dwRemainder = dwAppendSize % cbPattern;                

                // Write the pattern to the file. The idea is to always try to write with biggest buffer for performance reason
                // We don't want to do too much 1-byte writing at a time
                do 
                {
                    for (DWORD i = 0; i < dwNumWrites; i++)
                    {
                        if (!WriteFile(hFile, pbPattern, cbPattern, &dwBytes, NULL) || (dwBytes != cbPattern)) 
                        {
                            NKDbgPrintfW(L"ERROR: CTestFileList::Append: WriteFile on %s failed. Err=%u\r\n", szFullPath, GetLastError());
                            goto done;
                        }
                    }

                    if (cbPattern <= 32)
                        break;

                    // Write buffer at half chunk of the original buffer size for perf reason
                    cbPattern = cbPattern / 2;
                    dwNumWrites = dwRemainder / cbPattern;
                    dwRemainder = dwRemainder % cbPattern;

                    if (dwNumWrites)
                        StoreHlp::GenerateRandomPattern(pbPattern, cbPattern);

                } while (cbPattern >= 32); 

                // Write the remainder, 1 byte at a time
                for (DWORD i = 0; i < dwRemainder; i++)
                {
                    if (!WriteFile(hFile, &(pbPattern[i]), 1, &dwBytes, NULL) || (dwBytes != 1)) 
                    {
                        NKDbgPrintfW(L"ERROR: CTestFileList: WriteFile on %s failed (on remainder). Err=%u\r\n", szFullPath, GetLastError());
                        goto done;
                    }
                }

                if (!SetEndOfFile(hFile))
                {
                    NKDbgPrintfW(L"CTestFileList::Append : SetEndOfFile on %s failed. Err=%u\r\n", pCurrent->tfi.szFullPath, GetLastError());
                    fRet = FALSE;
                }
                FlushFileBuffers(hFile);
                CloseHandle(hFile);
                hFile = INVALID_HANDLE_VALUE;

                // Update size and checksum information
                if (!StoreHlp::GetFileChecksum(pCurrent->tfi.szFullPath, &(pCurrent->tfi.dwCheckSum)))
                {
                    NKDbgPrintfW(L"CTestFileList::Append ERROR getting file check sum on %s\r\n", pCurrent->tfi.szFullPath);
                    fRet = FALSE;
                }

                if (fRet)
                    pCurrent->tfi.dwSize += dwAppendSize;
            }

            fFound = TRUE;
            break;
        }

        pCurrent = pCurrent->pNext;
    }

    LeaveCriticalSection(&m_csTestFileList);

    if (!fFound)
    {
        NKDbgPrintfW(L"CTestFileList::Append : Could not find %s in the file list\r\n", szFullPath);
        fRet = FALSE;
    }

done:
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    if(pbPattern) delete [] pbPattern;
    return fRet;
}


//
// ReadFirst
//    Read the head of the list
//
// Parameter:
//    lpFileInfo: (OUT) pointer to TEST_FILE_INFO
//
// Return: True if successful, False otherwise.
//
BOOL CTestFileList::ReadFirst(TEST_FILE_INFO* lpFileInfo)
{
    BOOL fRet = FALSE;

    EnterCriticalSection(&m_csTestFileList);

    m_pCurrent = m_pHead;

    if (!m_pCurrent)
    {
        NKDbgPrintfW(L"CTestFileList::ReadFirst ERROR: List is empty\r\n");
        goto done;
    }
    else
    {
        memcpy(lpFileInfo, &(m_pCurrent->tfi), sizeof(TEST_FILE_INFO));
    }

    fRet = TRUE;
done:
    LeaveCriticalSection(&m_csTestFileList);
    return fRet;
}


//
// ReadNext
//    Advance to the next node and read the content
//
// Parameter:
//    lpFileInfo: (OUT) pointer to TEST_FILE_INFO
//
// Return: True if successful, False otherwise.
//
BOOL CTestFileList::ReadNext(TEST_FILE_INFO* lpFileInfo)
{
    BOOL fRet = FALSE;

    EnterCriticalSection(&m_csTestFileList);

    if (!m_pCurrent)
    {
        NKDbgPrintfW(L"CTestFileList::ReadNext ERROR: List is empty\r\n");
        goto done;
    }

    m_pCurrent = m_pCurrent->pNext;

    if (!m_pCurrent)
    {
        //NKDbgPrintfW(L"CTestFileList::ReadNext Next node was not found\r\n");
        fRet = FALSE;
        goto done;
    }

    memcpy(lpFileInfo, &(m_pCurrent->tfi), sizeof(TEST_FILE_INFO));

    fRet = TRUE;
done:
    LeaveCriticalSection(&m_csTestFileList);
    return fRet;
}


//
// ReadClose
//    Reset m_pCurrent pointer to the head
//
void CTestFileList::ReadClose()
{
    m_pCurrent = m_pHead;
}


//
// VerifyTFI
//    Verify Test File Info in parameter
//
BOOL CTestFileList::VerifyTFI(TEST_FILE_INFO& tfi)
{
    BOOL fRet = FALSE;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    DWORD dwFileSize = 0;
    DWORD dwAttrib = 0;
    DWORD dwCheckSum = 0;

    //NKDbgPrintfW(L"CTestFileList::VerifyTFI : Verifying %s..."), tfi.szFullPath));

    if (!tfi.fDirectory)
    {
        // Make sure file exists
        // Open the file
        hFile = CreateFile(
            tfi.szFullPath,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

        if (hFile == INVALID_HANDLE_VALUE)
        {
            NKDbgPrintfW(L"Error : CTestFileList::VerifyTFI : CreateFile %s failed. Err=%u\r\n", tfi.szFullPath, GetLastError());
            goto done;
        }

        // Check file size
        dwFileSize = GetFileSize(hFile, NULL);
        if (dwFileSize != tfi.dwSize)
        {
            NKDbgPrintfW(L"Error : CTestFileList::VerifyTFI : File size doesn't match on %s. Stored[%u] Returned[%u]\r\n", tfi.szFullPath,  tfi.dwSize, dwFileSize);
            goto done;
        }

        CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;

        // Verify checksum
        if (!StoreHlp::GetFileChecksum(tfi.szFullPath, &dwCheckSum))
        {
            NKDbgPrintfW(L"CTestFileList::VerifyTFI ERROR getting file check sum on %s\r\n", tfi.szFullPath);
            goto done;
        }

        if (dwCheckSum != tfi.dwCheckSum)
        {
            NKDbgPrintfW(L"Error : CTestFileList::VerifyTFI : File checksum doesn't match on %s. Stored[%u] Returned[%u]\r\n", tfi.szFullPath,  tfi.dwCheckSum, dwCheckSum);
            goto done;
        }
    }

    // Check file attribute
    dwAttrib = GetFileAttributes(tfi.szFullPath);
    if (dwAttrib != tfi.dwAttribute)
    {
        NKDbgPrintfW(L"Error : CTestFileList::VerifyTFI : File attribute doesn't match on %s. Stored[%u] Returned[%u]\r\n", tfi.szFullPath,  tfi.dwAttribute, dwAttrib);
        goto done;
    }

    fRet = TRUE;
done:
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    return fRet;
}


//
// Verify
//    Verify each file size and checksum
//
// Return: True if successful, False otherwise.
//
BOOL CTestFileList::Verify()
{
    BOOL fRet = FALSE;
    TEST_FILE_INFO tfi;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    DWORD dwFileSize = 0;
    DWORD dwAttrib = 0;
    DWORD dwCheckSum = 0;

    EnterCriticalSection(&m_csTestFileList);

    if (ReadFirst(&tfi))
    {
        fRet = VerifyTFI(tfi);
        if (!fRet) goto done;
        
        while (ReadNext(&tfi))
        {
            fRet = VerifyTFI(tfi);
            if (!fRet) goto done;            
        }
    }
    else
        goto done;

    fRet = TRUE;
done:
    ReadClose();
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    LeaveCriticalSection(&m_csTestFileList);
    return fRet;
}


//
// PrintList
//    Print the content of the list
//
void CTestFileList::PrintList()
{
    TEST_FILE_NODE* pTempNode = m_pHead;
    UINT nNode = 0;

    while (pTempNode)
    {
        nNode++;
        // Print each node
        NKDbgPrintfW(L"\r\n");
        NKDbgPrintfW(L"Node #%u:\r\n", nNode);
        NKDbgPrintfW(L"-------------------\r\n", nNode);
        NKDbgPrintfW(L"%s name = %s\r\n", pTempNode->tfi.fDirectory ? L"Dir" : L"File", pTempNode->tfi.szFullPath);
        NKDbgPrintfW(L"Attribute = %.08x\r\n", pTempNode->tfi.dwAttribute);
        NKDbgPrintfW(L"Size = %u\r\n", pTempNode->tfi.dwSize);
        NKDbgPrintfW(L"Checksum = %u\r\n", pTempNode->tfi.dwCheckSum);
        
        pTempNode = pTempNode->pNext;
    }
    NKDbgPrintfW(L"\r\n");
}


//
// Count
//    # of items in the list
//

DWORD CTestFileList::Count()
{
    TEST_FILE_NODE* pTempNode = m_pHead;
    DWORD dwCount = 0;

    while (pTempNode)
    {
        dwCount++;
        pTempNode = pTempNode->pNext;
    }
    return dwCount;
}
