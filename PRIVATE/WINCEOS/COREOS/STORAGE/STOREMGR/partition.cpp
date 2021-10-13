//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <store.h>
#include <fsdmgrp.h>

BOOL PartitionIoControl(CPartition *pPartition, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    DWORD dwError;
    if ((dwIoControlCode == IOCTL_DISK_GETNAME) || (dwIoControlCode == DISK_IOCTL_GETNAME)){
        if (lpOutBuf && (nOutBufSize >= FOLDERNAMESIZE) && wcslen(pPartition->m_szFolderName)) {
            wcscpy( (TCHAR *)lpOutBuf, pPartition->m_szFolderName);
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
            m_pOpenStore = (POPENSTORE)GetProcAddress( m_hPartDriver, L"PD_OpenStore");
            m_pCloseStore = (PCLOSESTORE)GetProcAddress( m_hPartDriver, L"PD_CloseStore");
            m_pFormatStore = (PFORMATSTORE)GetProcAddress( m_hPartDriver, L"PD_FormatStore");
            m_pIsStoreFormatted = (PISSTOREFORMATTED)GetProcAddress( m_hPartDriver, L"PD_IsStoreFormatted");
            m_pGetStoreInfo = (PGETSTOREINFO)GetProcAddress( m_hPartDriver, L"PD_GetStoreInfo");
            m_pCreatePartition = (PCREATEPARTITION)GetProcAddress( m_hPartDriver, L"PD_CreatePartition");
            m_pDeletePartition = (PDELETEPARTITION)GetProcAddress( m_hPartDriver, L"PD_DeletePartition");
            m_pRenamePartition = (PRENAMEPARTITION)GetProcAddress( m_hPartDriver, L"PD_RenamePartition");
            m_pSetPartitionAttrs = (PSETPARTITIONATTRS)GetProcAddress( m_hPartDriver, L"PD_SetPartitionAttrs");
            m_pFormatPartition = (PFORMATPARTITION)GetProcAddress( m_hPartDriver, L"PD_FormatPartition");
            m_pGetPartitionInfo = (PGETPARTITIONINFO)GetProcAddress( m_hPartDriver, L"PD_GetPartitionInfo");
            m_pFindPartitionStart = (PFINDPARTITIONSTART)GetProcAddress( m_hPartDriver, L"PD_FindPartitionStart");
            m_pFindPartitionNext = (PFINDPARTITIONNEXT)GetProcAddress( m_hPartDriver, L"PD_FindPartitionNext");
            m_pFindPartitionClose = (PFINDPARTITIONCLOSE)GetProcAddress( m_hPartDriver, L"PD_FindPartitionClose");
            m_pOpenPartition = (POPENPARTITION)GetProcAddress( m_hPartDriver, L"PD_OpenPartition");
            m_pClosePartition = (PCLOSEPARTITION)GetProcAddress( m_hPartDriver, L"PD_ClosePartition");
            m_pDeviceIoControl = (PPD_DEVICEIOCONTROL)GetProcAddress( m_hPartDriver, L"PD_DeviceIoControl");

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
                DEBUGMSG( 1, (L"Entry point sin the driver not found\r\n"));
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
    wsprintf( szRegKey, L"%s\\%s", m_szRootKey, m_szFileSys);
    if (bSuccess = (ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey))) {
        DUMPREGKEY(ZONE_INIT, szRegKey, hKey);
        bSuccess = FsdGetRegistryString(hKey, szValueName, szValue, dwValueSize);
        RegCloseKey( hKey);
    }    
    if (!bSuccess) {
        wsprintf( szRegKey, L"%s\\%s", g_szSTORAGE_PATH, m_szFileSys);
        if (bSuccess = (ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey))) {
            DUMPREGKEY(ZONE_INIT, szRegKey, hKey);
            bSuccess = FsdGetRegistryString(hKey, szValueName, szValue, dwValueSize);
            RegCloseKey( hKey);
        }    
    }
    if (!bSuccess) 
        wcscpy( szValue, L"");
    return bSuccess;
}

BOOL CPartition::GetFSDValue( const TCHAR *szValueName, LPDWORD pdwValue)
{
    extern const TCHAR *g_szSTORAGE_PATH;
    TCHAR szRegKey[MAX_PATH];
    HKEY hKey;
    BOOL bSuccess = FALSE;
    wsprintf( szRegKey, L"%s\\%s", m_szRootKey, m_szFileSys);
    if (bSuccess = (ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey))) {
        DUMPREGKEY(ZONE_INIT, szRegKey, hKey);
        bSuccess = FsdGetRegistryValue(hKey, szValueName, pdwValue);
        RegCloseKey( hKey);
    }    
    if (!bSuccess) {
        wsprintf( szRegKey, L"%s\\%s", g_szSTORAGE_PATH, m_szFileSys);
        if (bSuccess = (ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey))) {
            DUMPREGKEY(ZONE_INIT, szRegKey, hKey);
            bSuccess = FsdGetRegistryValue(hKey, szValueName, pdwValue);
            RegCloseKey( hKey);
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
        wcscpy( pInfo->szPartitionName, m_szPartitionName);
        if (wcslen(m_szFriendlyName)) {
            wcsncpy( pInfo->szFileSys, m_szFriendlyName, FILESYSNAMESIZE-1);
        } else {    
            wcsncpy( pInfo->szFileSys, m_szFileSys, FILESYSNAMESIZE-1);
        }    
        pInfo->szFileSys[FILESYSNAMESIZE-1] = 0;
        if (m_hFSD && m_pDsk && m_pDsk->pVol) {
            wcsncpy( pInfo->szVolumeName, m_pDsk->pVol->wsVol, VOLUMENAMESIZE-1);
            pInfo->szVolumeName[VOLUMENAMESIZE-1] = 0;
        } else {
            wcscpy( pInfo->szVolumeName, L"");
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

BOOL CPartition::MountPartition(HANDLE hPartition, LPCTSTR szRootRegKey, LPCTSTR szDefaultFileSystem, DWORD dwMountFlags, HANDLE hActivityEvent)
{
    extern const TCHAR *g_szPART_TABLE_STRING;
    extern const TCHAR *g_szFILE_SYSTEM_MODULE_STRING;
    extern const TCHAR *g_szMOUNT_FLAGS_STRING;
    extern const TCHAR *g_szSTORAGE_PATH;
    extern const TCHAR *g_szMOUNT_FLAGS_STRING;
    TCHAR szRegKey[2*MAX_PATH];
    TCHAR szPartId[10];
    HKEY hKey = NULL;
    m_hPartition = hPartition;
    BOOL bRet = TRUE;
    DWORD dwTmpFlags;

    wcsncpy( m_szRootKey, szRootRegKey, MAX_PATH-1);
    m_szRootKey[MAX_PATH-1] = 0;

    wsprintf( szRegKey, L"%s\\%s", m_szRootKey, g_szPART_TABLE_STRING);

    wcsncpy( m_szFileSys, szDefaultFileSystem, FILESYSNAMESIZE-1); 
    m_szFileSys[FILESYSNAMESIZE-1] = 0;
    if (ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey)) {
        wcscpy( szRegKey, g_szSTORAGE_PATH);
        wcscat( szRegKey, L"\\");
        wcscat( szRegKey, g_szPART_TABLE_STRING);
        RegOpenKeyEx( HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey);
    }    
    if (hKey) {
        TCHAR* szTempId = szPartId;
        DUMPREGKEY(ZONE_INIT, szRegKey, hKey);

        // Convert m_pi.bPartType into a string in %02x format.
        if (m_pi.bPartType < 0x10)
            *szTempId++ = L'0';
        _itow (m_pi.bPartType, szTempId, 16);
        
        if (!FsdGetRegistryString(hKey, szPartId, m_szFileSys, sizeof(m_szFileSys)/sizeof(WCHAR))) {
            wcscpy( m_szFileSys, szDefaultFileSystem); 
        }    
        RegCloseKey(hKey);
    }    
    wcsncpy( szRegKey, m_szRootKey, MAX_PATH-1);
    szRegKey[MAX_PATH-1] = 0;
    wcscat( szRegKey, L"\\");
    wcscat( szRegKey, m_szFileSys);
    if (ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, szRegKey, 0, 0, &hKey)) {
        if (FsdGetRegistryValue(hKey, g_szMOUNT_FLAGS_STRING, &dwTmpFlags)) {
                dwMountFlags = dwTmpFlags;
        }
        RegCloseKey(hKey);
    }    
    m_dwMountFlags = dwMountFlags;
    if (wcslen(m_szFileSys)) {
        GetFSDString( g_szFILE_SYSTEM_MODULE_STRING, m_szModuleName, sizeof(m_szModuleName));
        GetFSDString( g_szFILE_SYSTEM_MODULE_STRING, m_szFriendlyName, sizeof(m_szFriendlyName));
        if (m_szModuleName) {
            m_hFSD = LoadDriver( m_szModuleName);
            if (m_hFSD) {
                FSDINITDATA fid;
                fid.pIoControl = (PDEVICEIOCONTROL)PartitionIoControl;
                fid.hDsk = (HANDLE)this;
                fid.hPartition = hPartition;
                fid.dwFlags = m_dwMountFlags;
                fid.hActivityEvent = hActivityEvent;
                wcsncpy( fid.szFileSysName, m_szFileSys,FILESYSNAMESIZE-1);
                fid.szFileSysName[FILESYSNAMESIZE-1] = 0;
                wcscpy( fid.szRegKey, szRootRegKey);
                wcscpy( fid.szDiskName, m_szPartitionName);
                fid.hFSD = m_hFSD;
                fid.pNextDisk = NULL;
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
        if (!FSDMGR_DeinitEx(m_pDsk)) {
            return FALSE;
        }    
        m_pDsk = NULL;
    }    
    if (m_hFSD) {
        FreeLibrary( m_hFSD);
        m_hFSD = NULL;
    }    
    m_dwFlags &= ~PARTITION_ATTRIBUTE_MOUNTED;
    CloseHandle( m_hPartition);
    return TRUE;
}

BOOL CPartition::RenamePartition(LPCTSTR szNewName)
{
    DWORD dwError = ERROR_SUCCESS;
    if (ERROR_SUCCESS == (dwError = m_pPartDriver->RenamePartition(m_dwStoreId, m_szPartitionName, szNewName))) {
        wcscpy( m_szPartitionName, szNewName);
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

