//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <fsdmgrp.h>
#include <storemain.h>
#include <strsafe.h>

void Disk_PowerOn(HANDLE hDsk)
{
    CPartition *pPartition = (CPartition *)hDsk;
    __try {
        if (pPartition && pPartition->m_pBlockDevice) {
            pPartition->m_pBlockDevice->PowerOn();
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        ASSERT(0);
    }
}

void Disk_PowerOff(HANDLE hDsk)
{
    CPartition *pPartition = (CPartition *)hDsk;
    __try {
        if (pPartition && pPartition->m_pBlockDevice) {
            pPartition->m_pBlockDevice->PowerOff();
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        ASSERT(0);
    }
}

BOOL PartitionIoControl(CPartition *pPartition, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    DWORD dwError;
    if ((dwIoControlCode == IOCTL_DISK_GETNAME) || (dwIoControlCode == DISK_IOCTL_GETNAME)){
        if (lpOutBuf && (nOutBufSize >= FOLDERNAMESIZE) && wcslen(pPartition->m_szFolderName)) {
            VERIFY(SUCCEEDED(StringCchCopy((TCHAR *)lpOutBuf, nOutBufSize, pPartition->m_szFolderName)));
            return TRUE;
        }   
    }
    SetLastError(ERROR_SUCCESS);
    dwError = pPartition->m_pPartDriver->DeviceIoControl( pPartition->m_dwPartitionId, 
                                                      dwIoControlCode, 
                                                      lpInBuf, 
                                                      nInBufSize, 
                                                      lpOutBuf, 
                                                      nOutBufSize, 
                                                      lpBytesReturned);
    if ((dwError != ERROR_SUCCESS) &&
        ((dwIoControlCode == IOCTL_DISK_READ) || 
         (dwIoControlCode == IOCTL_DISK_WRITE) || 
         (dwIoControlCode == DISK_IOCTL_WRITE) ||
         (dwIoControlCode == DISK_IOCTL_READ))) 
    {
        DEBUGMSG( ZONE_ERRORS, (L"PartitionIoControl failed with the following error %ld\r\n", dwError));        
    }
    if (dwError != ERROR_SUCCESS) {
        SetLastError(dwError);
    }
    return (ERROR_SUCCESS == dwError);
}


DWORD CPartDriver::LoadPartitionDriver(TCHAR *szPartDriver)
{
    DWORD dwError = ERROR_SUCCESS;

    m_pOpenStore = (POPENSTORE)PD_OpenStore;
    m_pCloseStore = (PCLOSESTORE)PD_CloseStore;
    m_pFormatStore = (PFORMATSTORE)PD_FormatStore;
    m_pIsStoreFormatted = (PISSTOREFORMATTED)PD_IsStoreFormatted;
    m_pGetStoreInfo = (PGETSTOREINFO)PD_GetStoreInfo;
    m_pCreatePartition = (PCREATEPARTITION)PD_CreatePartition;
    m_pDeletePartition = (PDELETEPARTITION)PD_DeletePartition;
    m_pRenamePartition = (PRENAMEPARTITION)PD_RenamePartition;
    m_pSetPartitionAttrs = (PSETPARTITIONATTRS)PD_SetPartitionAttrs;
    m_pSetPartitionSize = (PSETPARTITIONSIZE)PD_SetPartitionSize;
    m_pFormatPartition = (PFORMATPARTITION)PD_FormatPartition;
    m_pGetPartitionInfo = (PGETPARTITIONINFO)PD_GetPartitionInfo;
    m_pFindPartitionStart = (PFINDPARTITIONSTART)PD_FindPartitionStart;
    m_pFindPartitionNext = (PFINDPARTITIONNEXT)PD_FindPartitionNext;
    m_pFindPartitionClose = (PFINDPARTITIONCLOSE)PD_FindPartitionClose;
    m_pOpenPartition = (POPENPARTITION)PD_OpenPartition;
    m_pClosePartition = (PCLOSEPARTITION)PD_ClosePartition;
    m_pDeviceIoControl = (PPD_DEVICEIOCONTROL)PD_DeviceIoControl;

    if (wcslen(szPartDriver)) {
        m_hPartDriver = LoadDriver( szPartDriver);
        if (m_hPartDriver) {
            DEBUGMSG( 1, (L"Loading partition driver %s hModule=%08X\r\n", szPartDriver, m_hPartDriver));
            m_pOpenStore = (POPENSTORE)FsdGetProcAddress( m_hPartDriver, L"PD_OpenStore");
            m_pCloseStore = (PCLOSESTORE)FsdGetProcAddress( m_hPartDriver, L"PD_CloseStore");
            m_pFormatStore = (PFORMATSTORE)FsdGetProcAddress( m_hPartDriver, L"PD_FormatStore");
            m_pIsStoreFormatted = (PISSTOREFORMATTED)FsdGetProcAddress( m_hPartDriver, L"PD_IsStoreFormatted");
            m_pGetStoreInfo = (PGETSTOREINFO)FsdGetProcAddress( m_hPartDriver, L"PD_GetStoreInfo");
            m_pCreatePartition = (PCREATEPARTITION)FsdGetProcAddress( m_hPartDriver, L"PD_CreatePartition");
            m_pDeletePartition = (PDELETEPARTITION)FsdGetProcAddress( m_hPartDriver, L"PD_DeletePartition");
            m_pRenamePartition = (PRENAMEPARTITION)FsdGetProcAddress( m_hPartDriver, L"PD_RenamePartition");
            m_pSetPartitionAttrs = (PSETPARTITIONATTRS)FsdGetProcAddress( m_hPartDriver, L"PD_SetPartitionAttrs");
            m_pSetPartitionSize = (PSETPARTITIONSIZE)FsdGetProcAddress( m_hPartDriver, L"PD_SetPartitionSize");
            if (!m_pSetPartitionSize) m_pSetPartitionSize = (PSETPARTITIONSIZE)PD_SetPartitionSize;
            m_pFormatPartition = (PFORMATPARTITION)FsdGetProcAddress( m_hPartDriver, L"PD_FormatPartition");
            m_pGetPartitionInfo = (PGETPARTITIONINFO)FsdGetProcAddress( m_hPartDriver, L"PD_GetPartitionInfo");
            m_pFindPartitionStart = (PFINDPARTITIONSTART)FsdGetProcAddress( m_hPartDriver, L"PD_FindPartitionStart");
            m_pFindPartitionNext = (PFINDPARTITIONNEXT)FsdGetProcAddress( m_hPartDriver, L"PD_FindPartitionNext");
            m_pFindPartitionClose = (PFINDPARTITIONCLOSE)FsdGetProcAddress( m_hPartDriver, L"PD_FindPartitionClose");
            m_pOpenPartition = (POPENPARTITION)FsdGetProcAddress( m_hPartDriver, L"PD_OpenPartition");
            m_pClosePartition = (PCLOSEPARTITION)FsdGetProcAddress( m_hPartDriver, L"PD_ClosePartition");
            m_pDeviceIoControl = (PPD_DEVICEIOCONTROL)FsdGetProcAddress( m_hPartDriver, L"PD_DeviceIoControl");

            if (!m_pOpenStore ||
                !m_pCloseStore ||
                !m_pFormatStore ||
                !m_pIsStoreFormatted ||
                !m_pGetStoreInfo ||
                !m_pCreatePartition ||
                !m_pDeletePartition ||
                !m_pRenamePartition ||
                !m_pSetPartitionAttrs ||
                !m_pFormatPartition ||
                !m_pGetPartitionInfo ||
                !m_pFindPartitionStart ||
                !m_pFindPartitionNext ||
                !m_pFindPartitionClose ||
                !m_pOpenPartition ||
                !m_pClosePartition ||
                !m_pDeviceIoControl)
            {    
                DEBUGMSG( 1, (L"Entry points in the driver not found\r\n"));
                dwError = ERROR_BAD_DRIVER;
            }
            DEBUGMSG( 1, (L"Driver %s loaded\r\n", szPartDriver));
        } else {
            DEBUGMSG( 1, (L"Could not find/load partition driver %s\r\n", szPartDriver));
            dwError = ERROR_FILE_NOT_FOUND;
        }    
    } 
    return dwError;
}

BOOL CPartition::GetFSDString( const TCHAR *szValueName, TCHAR *szValue, DWORD dwValueSize)
{
    extern const TCHAR *g_szSTORAGE_PATH;
    TCHAR szRegKey[MAX_PATH];
    HKEY hKey;
    BOOL bSuccess = FALSE;
    
    VERIFY(SUCCEEDED(StringCchPrintf(szRegKey, MAX_PATH, L"%s\\%s", m_szRootKey, m_szFileSys)));
    if (bSuccess = (ERROR_SUCCESS == FsdRegOpenKey( szRegKey, &hKey))) {
        DUMPREGKEY(ZONE_INIT, szRegKey, hKey);
        bSuccess = FsdGetRegistryString(hKey, szValueName, szValue, dwValueSize);
        FsdRegCloseKey( hKey);
    }    
    if (!bSuccess) {
        VERIFY(SUCCEEDED(StringCchPrintf(szRegKey, MAX_PATH, L"%s\\%s", g_szSTORAGE_PATH, m_szFileSys)));
        if (bSuccess = (ERROR_SUCCESS == FsdRegOpenKey( szRegKey, &hKey))) {
            DUMPREGKEY(ZONE_INIT, szRegKey, hKey);
            bSuccess = FsdGetRegistryString(hKey, szValueName, szValue, dwValueSize);
            FsdRegCloseKey( hKey);
        }    
    }
    if (!bSuccess) 
        VERIFY(SUCCEEDED(StringCbCopy( szValue, dwValueSize, L"")));
    return bSuccess;
}

BOOL CPartition::GetFSDValue( const TCHAR *szValueName, LPDWORD pdwValue)
{
    extern const TCHAR *g_szSTORAGE_PATH;
    TCHAR szRegKey[MAX_PATH];
    HKEY hKey;
    BOOL bSuccess = FALSE;
    
    VERIFY(SUCCEEDED(StringCchPrintf(szRegKey, MAX_PATH, L"%s\\%s", m_szRootKey, m_szFileSys)));
    if (bSuccess = (ERROR_SUCCESS == FsdRegOpenKey( szRegKey, &hKey))) {
        DUMPREGKEY(ZONE_INIT, szRegKey, hKey);
        bSuccess = FsdGetRegistryValue(hKey, szValueName, pdwValue);
        FsdRegCloseKey( hKey);
    }    
    if (!bSuccess) {
        VERIFY(SUCCEEDED(StringCchPrintf(szRegKey, MAX_PATH, L"%s\\%s", g_szSTORAGE_PATH, m_szFileSys)));
        if (bSuccess = (ERROR_SUCCESS == FsdRegOpenKey( szRegKey, &hKey))) {
            DUMPREGKEY(ZONE_INIT, szRegKey, hKey);
            bSuccess = FsdGetRegistryValue(hKey, szValueName, pdwValue);
            FsdRegCloseKey( hKey);
        }    
    }
    if (!bSuccess)
        *pdwValue = 0;
    return bSuccess;
}


BOOL CPartition::GetPartitionInfo(PARTINFO *pInfo)
{
    __try {
        pInfo->bPartType = m_pi.bPartType;
        pInfo->dwAttributes = m_dwFlags;
        memcpy( &pInfo->ftCreated, &m_pi.ftCreated, sizeof(FILETIME));
        memcpy( &pInfo->ftLastModified, &m_pi.ftLastModified, sizeof(FILETIME));
        pInfo->snNumSectors = m_pi.snNumSectors;
        VERIFY(SUCCEEDED(StringCchCopy( pInfo->szPartitionName, PARTITIONNAMESIZE, m_szPartitionName)));
        if (wcslen(m_szFriendlyName)) {
            wcsncpy( pInfo->szFileSys, m_szFriendlyName, FILESYSNAMESIZE-1);
        } else {    
            wcsncpy( pInfo->szFileSys, m_szFileSys, FILESYSNAMESIZE-1);
        }    
        pInfo->szFileSys[FILESYSNAMESIZE-1] = 0;
        if (m_hFSD && m_pDsk && m_pDsk->pVol) {
            VERIFY(SUCCEEDED(StringCchCopy(pInfo->szVolumeName, VOLUMENAMESIZE, m_pDsk->pVol->wsVol)));
        } else {
            VERIFY(SUCCEEDED(StringCchCopy(pInfo->szVolumeName, VOLUMENAMESIZE, L"")));
        }   
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }
    return TRUE;
}


BOOL CPartition::LoadPartition(LPCTSTR szPartitionName)
{
    wcsncpy( m_szPartitionName, szPartitionName, PARTITIONNAMESIZE-1);
    m_szPartitionName[PARTITIONNAMESIZE-1] = 0;
    m_pPartDriver->GetPartitionInfo(m_dwStoreId, szPartitionName, &m_pi);
    return TRUE;
}

BOOL CPartition::MountPartition(HANDLE hPartition, LPCTSTR szRootRegKey, LPCTSTR szDefaultFileSystem, DWORD dwMountFlags,  HANDLE hActivityEvent, BOOL bFormat)
{
    TCHAR szRegKey[MAX_PATH];
    TCHAR szPartId[10];
    HKEY hKey = NULL;
    m_hPartition = hPartition;
    BOOL bRet = TRUE;
    DWORD dwTmpFlags;
    DWORD dwCheckForFormat = 0;

    VERIFY(SUCCEEDED(StringCchCopy(m_szRootKey, MAX_PATH, szRootRegKey)));

    FsdRegOpenKey( m_szRootKey, &hKey);
    if (hKey) {
        FsdGetRegistryValue(hKey, g_szCHECKFORFORMAT,  &dwCheckForFormat);
        FsdRegCloseKey( hKey);
    }

    VERIFY(SUCCEEDED(StringCchPrintf(szRegKey, MAX_PATH, L"%s\\%s", m_szRootKey, g_szPART_TABLE_STRING)));
    
    VERIFY(SUCCEEDED(StringCchCopy(m_szFileSys, FILESYSNAMESIZE, szDefaultFileSystem)));
    if (ERROR_SUCCESS != FsdRegOpenKey( szRegKey, &hKey)) {
        VERIFY(SUCCEEDED(StringCchPrintf(szRegKey, MAX_PATH, L"%s\\%s", g_szSTORAGE_PATH, g_szPART_TABLE_STRING)));
        FsdRegOpenKey( szRegKey, &hKey);
    }    
    if (hKey) {
        TCHAR* szTempId = szPartId;
        DUMPREGKEY(ZONE_INIT, szRegKey, hKey);

        // Convert m_pi.bPartType into a string in %02x format.
        if (m_pi.bPartType < 0x10)
            *szTempId++ = L'0';
        _itow (m_pi.bPartType, szTempId, 16);
        
        if (!FsdGetRegistryString(hKey, szPartId, m_szFileSys, sizeof(m_szFileSys)/sizeof(WCHAR))) {
            VERIFY(SUCCEEDED(StringCchCopy(m_szFileSys, FILESYSNAMESIZE, szDefaultFileSystem)));
        }    
        FsdRegCloseKey(hKey);
    }    

    VERIFY(SUCCEEDED(StringCchPrintf(szRegKey, MAX_PATH, L"%s\\%s", m_szRootKey, m_szFileSys)));
    if (ERROR_SUCCESS == FsdRegOpenKey( szRegKey, &hKey)) {
        TCHAR szFolderName[FOLDERNAMESIZE];
        GetMountSettings(hKey, &dwMountFlags);
        if (FsdGetRegistryValue(hKey,  g_szCHECKFORFORMAT, &dwTmpFlags)) {
            dwCheckForFormat = dwTmpFlags;
        }
        if (FsdGetRegistryString(hKey, g_szFOLDER_NAME_STRING, szFolderName, sizeof(szFolderName)/sizeof(WCHAR))) {
            VERIFY(SUCCEEDED(StringCchCopy(m_szFolderName, FOLDERNAMESIZE, szFolderName)));
        }
        FsdRegCloseKey(hKey);
    }    

    VERIFY(SUCCEEDED(StringCchPrintf(szRegKey, MAX_PATH, L"%s\\%s", m_szRootKey, m_szPartitionName)));
    if (ERROR_SUCCESS == FsdRegOpenKey( szRegKey, &hKey)) {
        TCHAR szFolderName[FOLDERNAMESIZE];
        GetMountSettings(hKey, &dwMountFlags);
        if (FsdGetRegistryString(hKey, g_szFOLDER_NAME_STRING, szFolderName, sizeof(szFolderName)/sizeof(WCHAR))) {
            VERIFY(SUCCEEDED(StringCchCopy(m_szFolderName, FOLDERNAMESIZE, szFolderName)));
        }
        FsdRegCloseKey( hKey);
    }
    
    m_dwMountFlags = dwMountFlags;
#ifdef UNDER_CE    
    if (dwCheckForFormat == 1) {
        STORAGECONTEXT sc;
        BOOL fFormat = 0;
        memset(&sc, 0, sizeof(sc));
        sc.cbSize = sizeof(sc);
        m_pStore->GetStoreInfo( &sc.StoreInfo);
        GetPartitionInfo(&sc.PartInfo);
        sc.dwFlags = m_dwMountFlags;
        if (KernelIoControl(IOCTL_HAL_QUERY_FORMAT_PARTITION, &sc, sizeof(sc), (LPBYTE)&fFormat, sizeof(BOOL), NULL) && fFormat) {
            FormatPartition(m_pi.bPartType, FALSE);
            bFormat = TRUE;
        }
    }
#endif // UNDER_CE    
    if (wcslen(m_szFileSys)) {
        GetFSDString( g_szFILE_SYSTEM_MODULE_STRING, m_szModuleName, sizeof(m_szModuleName)/sizeof(WCHAR));
        GetFSDString( g_szFILE_SYSTEM_MODULE_STRING, m_szFriendlyName, sizeof(m_szFriendlyName)/sizeof(WCHAR));
        if (m_szModuleName) {
            m_hFSD = LoadDriver( m_szModuleName);
            if (m_hFSD) {
                FSDINITDATA fid;
                fid.pIoControl = (PDEVICEIOCONTROL)PartitionIoControl;
                fid.hDsk = (HANDLE)this;
                fid.hPartition = hPartition;
                fid.dwFlags = m_dwMountFlags;
                fid.hActivityEvent = hActivityEvent;
                fid.pPartition = this;
                fid.pStore = m_pStore;

                VERIFY(SUCCEEDED(StringCchCopy(fid.szFileSysName, FILESYSNAMESIZE, m_szFileSys)));
                VERIFY(SUCCEEDED(StringCchCopy(fid.szRegKey, MAX_PATH, szRootRegKey)));
                VERIFY(SUCCEEDED(StringCchCopy(fid.szDiskName, MAX_PATH, m_szPartitionName)));
                fid.hFSD = m_hFSD;
                fid.pNextDisk = NULL;
                fid.bFormat = bFormat;
                m_pDsk = InitEx( &fid);
            }    
        }    
    }    
    if (m_pDsk) {
        m_dwFlags |= PARTITION_ATTRIBUTE_MOUNTED;
    } else {
        if (m_hFSD) {
            FreeLibrary( m_hFSD);
            m_hFSD = NULL;
        }
        bRet = FALSE;
    }    
    return bRet;
}

BOOL CPartition::UnmountPartition()
{
    if (m_dwMountFlags & MOUNTFLAGS_TYPE_NODISMOUNT)
        return FALSE;
    if (m_pDsk) {
        if (!DeinitEx(m_pDsk)) {
            return FALSE;
        }    
        m_pDsk = NULL;
    }    
    if (m_hFSD) {
        FreeLibrary( m_hFSD);
        m_hFSD = NULL;
    }    
    m_dwFlags &= ~PARTITION_ATTRIBUTE_MOUNTED;
#ifdef UNDER_CE
    CloseHandle( m_hPartition);
#endif
    return TRUE;
}

BOOL CPartition::RenamePartition(LPCTSTR szNewName)
{
    DWORD dwError = ERROR_SUCCESS;
    if (ERROR_SUCCESS == (dwError = m_pPartDriver->RenamePartition(m_dwStoreId, m_szPartitionName, szNewName))) {
        VERIFY(SUCCEEDED(StringCchCopy(m_szPartitionName, PARTITIONNAMESIZE, szNewName)));
        return TRUE;
    }
    SetLastError(dwError);
    return FALSE;
}    

BOOL CPartition::FormatPartition(BYTE bPartType, BOOL bAuto)
{
    DWORD dwError = ERROR_SUCCESS;
    if (ERROR_SUCCESS == (dwError = m_pPartDriver->FormatPartition(m_dwStoreId, m_szPartitionName, bPartType, bAuto))) {
        m_pPartDriver->GetPartitionInfo(m_dwStoreId, m_szPartitionName, &m_pi);
        return TRUE;
    }
    SetLastError(dwError);
    return FALSE;
}

