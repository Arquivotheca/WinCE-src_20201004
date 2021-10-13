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
#include <storehlp.h>

//
// C++ Interface
//

const WCHAR DEF_ROOT_DIRECTORY[] = L"\\Storage Card";

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
CFSInfo::CFSInfo(LPCWSTR pszRootDirectory)
{
    if(!Stg_GetStoreInfoByPath(pszRootDirectory, &m_storeInfo, &m_partInfo)) {
        ::memset(&m_storeInfo, 0, sizeof(STOREINFO));
        ::memset(&m_partInfo, 0, sizeof(PARTINFO));
    }
    m_hStore = ::OpenStore(m_storeInfo.szDeviceName);
    if(INVALID_HANDLE_VALUE != m_hPart) {
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
const LPWSTR CFSInfo::FindStorageWithProfile(LPCWSTR pszProfile,
                                             __out_ecount(cLen) LPWSTR pszRootDirectory,
                                             UINT cLen)
{
    PPARTDATA pInfo = NULL;

    pInfo = Stg_FindFirstPart(pszProfile);

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
    RETAILMSG(1, (L"refreshing store \"%s\"...\r\n", pszStore));
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
                RETAILMSG(1, (L"unable to refresh store \"%s\"\n", storeInfo.szDeviceName));
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
    IN LPCWSTR pszProfileMatch
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
                    RETAILMSG(1, (L"no storage devices match the profile \"%s\"\n", pInfo->szProfileMatch));
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
            if(NULL == pInfo->hFindPart || INVALID_HANDLE_VALUE == pInfo->hFindPart) {
                // if there wasn't a partition, find the next store and try again
                if(!FindNextStore(pInfo->hFindStore, &pInfo->storeInfo)) {
                    FindCloseStore(pInfo->hFindStore);
                    CloseHandle(pInfo->hStore);
                    RETAILMSG(1, (L"no storage devices match the profile \"%s\"\n", pInfo->szProfileMatch));
                    LocalFree(pInfo);
                    SetLastError(ERROR_NO_MORE_ITEMS);
                    return NULL;
                }
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
    RETAILMSG(1, (L"format volume: %u%% complete\r\n", dwPercent));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL FormatVolumeMessage(LPWSTR pszMessage, LPWSTR pszCaption, BOOL fYesNo)
{
    RETAILMSG(1, (L"format volume: %s : %s %s\r\n", pszMessage, pszCaption, fYesNo));
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
        RETAILMSG(1,(L"LoadLibrary(%s) failed! Error %u\r\n", szLibrary, GetLastError()));
        return FALSE;
    }

    // load the format function
    pfnFormatVolume = (PFN_FORMATVOLUME)GetProcAddress(hUtilLib, FORMAT_EXPORT_NAME);
    if(NULL == pfnFormatVolume) {
        RETAILMSG(1, (L"GetProcAddress(0x%08X, \"%s\")", hUtilLib, FORMAT_EXPORT_NAME));
        goto done;
    }
    
    if (!DeviceIoControl(hPart, DISK_IOCTL_GETINFO, &diskInfo, sizeof(DISK_INFO), 
        &diskInfo, sizeof(DISK_INFO), &dwBytes, NULL)) {
        RETAILMSG(1,(L"DeviceIoControl(DISK_IOCTL_GETINFO) failed! Error %u\r\n", GetLastError()));
        goto done;
    }

    // dismount the partition before formatting
    if(!(pFormatOpts->dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !DismountPartition(hPart)){
        RETAILMSG(1,(L"DismountPartition() failed! Error %u\r\n", GetLastError()));
        goto done;
    }

    if(pFormatOpts) {
        RETAILMSG(1, (L"format volume: tfat=%s full=%s fatver=%u roodirs=%u clusterbytes=%u\r\n", 
            (pFormatOpts->dwFlags & FATUTIL_FORMAT_TFAT) ? L"TRUE" : L"FALSE", 
            (pFormatOpts->dwFlags & FATUTIL_FULL_FORMAT) ? L"TRUE" : L"FALSE", 
            pFormatOpts->dwFatVersion, pFormatOpts->dwRootEntries, pFormatOpts->dwClusSize));
    }

    // format the partition
    dwRet = (pfnFormatVolume)(hPart, &diskInfo, pFormatOpts, FormatVolumeProgress, FormatVolumeMessage);

    // remount the partitoin
    if(!(pFormatOpts->dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !MountPartition(hPart)) {
        RETAILMSG(1,(L"MountPartition() failed! Error %u\r\n", GetLastError()));
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
    DWORD dwBytes = 0;
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
        RETAILMSG(1,(L"LoadLibrary(%s) failed! Error %u\r\n", szLibrary, GetLastError()));
        return FALSE;
    }

    // load the defrag function
    pfnFormatVolumeEx = (PFN_FORMATVOLUMEEX)GetProcAddress(hUtilLib, FORMATEX_EXPORT_NAME);
    if(NULL == pfnFormatVolumeEx) {
        RETAILMSG(1, (L"GetProcAddress(0x%08X, \"%s\")", hUtilLib, FORMATEX_EXPORT_NAME));
        goto done;
    }

    // dismount the partition before formatting
    if(!(pFormatParams->fo.dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !DismountPartition(hPart)){
        RETAILMSG(1,(L"DismountPartition() failed! Error %u\r\n", GetLastError()));
        goto done;
    }

    RETAILMSG(1, (L"format volume: tfat=%s full=%s fatver=%u roodirs=%u clusterbytes=%u\r\n",
        (pFormatParams->fo.dwFlags & FATUTIL_FORMAT_TFAT) ? L"TRUE" : L"FALSE",
        (pFormatParams->fo.dwFlags & FATUTIL_FULL_FORMAT) ? L"TRUE" : L"FALSE",
        pFormatParams->fo.dwFatVersion, pFormatParams->fo.dwRootEntries, pFormatParams->fo.dwClusSize));

    // ScanEx the partition
    dwRet = (pfnFormatVolumeEx)(hPart, pFormatParams);

    if(ERROR_SUCCESS == dwRet) {
        RETAILMSG(1, (L"format volume: total sectors:        %u\r\n", pFormatParams->fr.dwTotalSectors));
        RETAILMSG(1, (L"format volume: sectors per FAT:      %u\r\n", pFormatParams->fr.dwSectorsPerFat));
        RETAILMSG(1, (L"format volume: sectors per cluster:  %u\r\n", pFormatParams->fr.dwSectorsPerCluster));
        RETAILMSG(1, (L"format volume: root sectors:         %u\r\n", pFormatParams->fr.dwRootSectors));
        RETAILMSG(1, (L"format volume: reserved sectors:     %u\r\n", pFormatParams->fr.dwReservedSectors));
        RETAILMSG(1, (L"format volume: number of FATs:       %u\r\n", pFormatParams->fr.dwNumFats));
        RETAILMSG(1, (L"format volume: FAT version:          FAT%u\r\n", pFormatParams->fr.dwFatVersion));
        if(0 != pFormatParams->fr.dwSectorsPerCluster) {
            RETAILMSG(1, (L"format volume: clusters:             %u\r\n", pFormatParams->fr.dwTotalSectors / pFormatParams->fr.dwSectorsPerCluster));
        }
    }

    // remount the partition
    if(!(pFormatParams->fo.dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !MountPartition(hPart)) {
        RETAILMSG(1,(L"MountPartition() failed! Error %u\r\n", GetLastError()));
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
    RETAILMSG(1, (L"scan volume: %u%% complete\r\n", dwPercent));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL ScanVolumeMessage(LPWSTR pszMessage, LPWSTR pszCaption, BOOL fYesNo)
{
    RETAILMSG(1, (L"scan volume: %s : %s %s\r\n", pszMessage, pszCaption, fYesNo));
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
        RETAILMSG(1,(L"LoadLibrary(%s) failed! Error %u\r\n", szLibrary, GetLastError()));
        return FALSE;
    }

    // load the scan function
    pfnScanVolume = (PFN_SCANVOLUME)GetProcAddress(hUtilLib, SCAN_EXPORT_NAME);
    if(NULL == pfnScanVolume) {
        RETAILMSG(1, (L"GetProcAddress(0x%08X, \"%s\")", hUtilLib, SCAN_EXPORT_NAME));
        goto done;
    }

    
    if (!DeviceIoControl(hPart, DISK_IOCTL_GETINFO, &diskInfo, sizeof(DISK_INFO), 
        &diskInfo, sizeof(DISK_INFO), &dwBytes, NULL)) {
        RETAILMSG(1,(L"DeviceIoControl(DISK_IOCTL_GETINFO) failed! Error %u\r\n", GetLastError()));
        goto done;
    }

    // dismount the partition before scanning
    if(!(pScanOpts->dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !DismountPartition(hPart)){
        RETAILMSG(1,(L"DismountPartition() failed! Error %u\r\n", GetLastError()));
        goto done;
    }

    if(pScanOpts) {
        RETAILMSG(1, (L"scan volume: fat=%u verifyfix=%s\r\n", pScanOpts->dwFatToUse, 
            (pScanOpts->dwFlags & FATUTIL_SCAN_VERIFY_FIX) ? L"TRUE" : L"FALSE"));
    }

    // scan the partition
    dwRet = (pfnScanVolume)(hPart, &diskInfo, pScanOpts, ScanVolumeProgress, ScanVolumeMessage);

    // remount the partition
    if(!(pScanOpts->dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !MountPartition(hPart)) {
        RETAILMSG(1,(L"MountPartition() failed! Error %u\r\n", GetLastError()));
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
    DWORD dwBytes = 0;
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
        RETAILMSG(1,(L"LoadLibrary(%s) failed! Error %u\r\n", szLibrary, GetLastError()));
        return FALSE;
    }    

    // load the ScanEx function
    pfnScanVolumeEx = (PFN_SCANVOLUMEEX)GetProcAddress(hUtilLib, SCANEX_EXPORT_NAME);
    if(NULL == pfnScanVolumeEx) {
        RETAILMSG(1, (L"GetProcAddress(0x%08X, \"%s\")", hUtilLib, SCANEX_EXPORT_NAME));
        goto done;
    }

    // dismount the partition before defragmenting
    if(!(pScanParams->so.dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !DismountPartition(hPart)){
        RETAILMSG(1,(L"DismountPartition() failed! Error %u\r\n", GetLastError()));
        goto done;
    }
    
    RETAILMSG(1, (L"scan volume: fat=%u verifyfix=%s\r\n", pScanParams->so.dwFatToUse, 
        (pScanParams->so.dwFlags & FATUTIL_SCAN_VERIFY_FIX) ? L"TRUE" : L"FALSE"));

    // ScanEx the partition
    dwRet = (pfnScanVolumeEx)(hPart, pScanParams);

    if(ERROR_SUCCESS == dwRet) {
        RETAILMSG(1, (L"defrag volume: lost clusters:       %u\r\n", pScanParams->sr.dwLostClusters));
        RETAILMSG(1, (L"defrag volume: invalid clusters:    %u\r\n", pScanParams->sr.dwInvalidClusters));
        RETAILMSG(1, (L"defrag volume: lost chains:         %u\r\n", pScanParams->sr.dwLostChains));
        RETAILMSG(1, (L"defrag volume: invalid directories: %u\r\n", pScanParams->sr.dwInvalidDirs));
        RETAILMSG(1, (L"defrag volume: invalid files:       %u\r\n", pScanParams->sr.dwInvalidFiles));
        RETAILMSG(1, (L"defrag volume: total errors:        %u\r\n", pScanParams->sr.dwTotalErrors));
        RETAILMSG(1, (L"defrag volume: percent fragmented:  %u%%\r\n", pScanParams->sr.dwPercentFrag));
        RETAILMSG(1, (L"defrag volume: all fats consistent: %s\r\n", pScanParams->sr.fConsistentFats ? L"TRUE" : L"FALSE"));
        RETAILMSG(1, (L"defrag volume: errors not fixed:    %s\r\n", pScanParams->sr.fErrorNotFixed ? L"TRUE" : L"FALSE"));
    }

    // remount the partition
    if(!(pScanParams->so.dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !MountPartition(hPart)) {
        RETAILMSG(1,(L"MountPartition() failed! Error %u\r\n", GetLastError()));
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
    RETAILMSG(1, (L"defrag volume: %u%% complete\r\n", dwPercent));
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static BOOL DefragVolumeMessage(LPWSTR pszMessage, LPWSTR pszCaption, BOOL fYesNo)
{
    RETAILMSG(1, (L"defrag volume: %s : %s %s\r\n", pszMessage, pszCaption, fYesNo));
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
        RETAILMSG(1,(L"LoadLibrary(%s) failed! Error %u\r\n", szLibrary, GetLastError()));
        return FALSE;
    }

    // load the defrag function
    pfnDefragVolume = (PFN_UTILITY)GetProcAddress(hUtilLib, DEFRAG_EXPORT_NAME);
    if(NULL == pfnDefragVolume) {
        RETAILMSG(1, (L"GetProcAddress(0x%08X, \"%s\")", hUtilLib, DEFRAG_EXPORT_NAME));
        goto done;
    }
    
    if (!DeviceIoControl(hPart, DISK_IOCTL_GETINFO, &diskInfo, sizeof(DISK_INFO), 
        &diskInfo, sizeof(DISK_INFO), &dwBytes, NULL)) {
        RETAILMSG(1,(L"DeviceIoControl(DISK_IOCTL_GETINFO) failed! Error %u\r\n", GetLastError()));
        goto done;
    }

    // dismount the partition before defragmenting
    if(!(pDefragOpts->dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !DismountPartition(hPart)){
        RETAILMSG(1,(L"DismountPartition() failed! Error %u\r\n", GetLastError()));
        goto done;
    }

    RETAILMSG(1, (L"defrag volume: fat=%u verifyfix=%s\r\n", pDefragOpts->dwFatToUse, 
        (pDefragOpts->dwFlags & FATUTIL_SCAN_VERIFY_FIX) ? L"TRUE" : L"FALSE"));

    // defragment the partition
    dwRet = (pfnDefragVolume)(hPart, &diskInfo, pDefragOpts, DefragVolumeProgress, DefragVolumeMessage);

    // remount the partitoin
    if(!(pDefragOpts->dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !MountPartition(hPart)) {
        RETAILMSG(1,(L"MountPartition() failed! Error %u\r\n", GetLastError()));
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
    DWORD dwBytes = 0;
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
        RETAILMSG(1,(L"LoadLibrary(%s) failed! Error %u\r\n", szLibrary, GetLastError()));
        return FALSE;
    }    

    // load the ScanEx function
    pfnDefragVolumeEx = (PFN_DEFRAGVOLUMEEX)GetProcAddress(hUtilLib, DEFRAGEX_EXPORT_NAME);
    if(NULL == pfnDefragVolumeEx) {
        RETAILMSG(1, (L"GetProcAddress(0x%08X, \"%s\")", hUtilLib, DEFRAGEX_EXPORT_NAME));
        goto done;
    }

    // dismount the partition before defragmenting
    if(!(pDefragParams->dopt.dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !DismountPartition(hPart)){
        RETAILMSG(1,(L"DismountPartition() failed! Error %u\r\n", GetLastError()));
        goto done;
    }

    RETAILMSG(1, (L"defrag volume: fat=%u verifyfix=%s\r\n", pDefragParams->dopt.dwFatToUse, 
        (pDefragParams->dopt.dwFlags & FATUTIL_SCAN_VERIFY_FIX) ? L"TRUE" : L"FALSE"));

    // DefragEx the partition
    dwRet = (pfnDefragVolumeEx)(hPart, pDefragParams);

    if(ERROR_SUCCESS == dwRet) {
        RETAILMSG(1, (L"defrag volume: lost clusters:       %u\r\n", pDefragParams->dr.sr.dwLostClusters));
        RETAILMSG(1, (L"defrag volume: invalid clusters:    %u\r\n", pDefragParams->dr.sr.dwInvalidClusters));
        RETAILMSG(1, (L"defrag volume: lost chains:         %u\r\n", pDefragParams->dr.sr.dwLostChains));
        RETAILMSG(1, (L"defrag volume: invalid directories: %u\r\n", pDefragParams->dr.sr.dwInvalidDirs));
        RETAILMSG(1, (L"defrag volume: invalid files:       %u\r\n", pDefragParams->dr.sr.dwInvalidFiles));
        RETAILMSG(1, (L"defrag volume: total errors:        %u\r\n", pDefragParams->dr.sr.dwTotalErrors));
        RETAILMSG(1, (L"defrag volume: percent fragmented:  %u%%\r\n", pDefragParams->dr.sr.dwPercentFrag));
        RETAILMSG(1, (L"defrag volume: all fats consistent: %s\r\n", pDefragParams->dr.sr.fConsistentFats ? L"TRUE" : L"FALSE"));
        RETAILMSG(1, (L"defrag volume: errors not fixed:    %s\r\n", pDefragParams->dr.sr.fErrorNotFixed ? L"TRUE" : L"FALSE"));
    }

    // remount the partition
    if(!(pDefragParams->dopt.dwFlags & FATUTIL_DISABLE_MOUNT_CHK) && !MountPartition(hPart)) {
        RETAILMSG(1,(L"MountPartition() failed! Error %u\r\n", GetLastError()));
        dwRet = ERROR_GEN_FAILURE;
        goto done;
    }
    
done:    
    if(hUtilLib) FreeLibrary(hUtilLib);
    return (ERROR_SUCCESS == dwRet);
}

