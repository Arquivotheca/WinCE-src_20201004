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


DWORD  g_dwBlockDevCount = 0;


CStore::CStore(TCHAR *szDeviceName, GUID DeviceGuid) :
        m_pNextStore(NULL),
        m_dwStoreId(NULL),
        m_dwFlags(0),
        m_pPartDriver(NULL),
        m_pPartitionList(NULL),
        m_dwPartCount(0),
        m_dwMountCount(0),
        m_pRootHandle(NULL),
        m_hActivityEvent(NULL),
        m_dwMountFlags(0),
        m_hDisk(NULL),
        m_pStorageId(NULL),
        m_pBlockDevice(NULL),
        m_dwRefCount(0),
        m_pDskStore(NULL)
{
    memset( &m_si, 0, sizeof(PD_STOREINFO));
    memset( &m_sdi, 0, sizeof(STORAGEDEVICEINFO));
    memcpy( &m_DeviceGuid, &DeviceGuid, sizeof(GUID));
    wcscpy( m_szPartDriver, L"");
    wcscpy( m_szPartDriverName, L"");
    wcsncpy( m_szDeviceName, szDeviceName, DEVICENAMESIZE-1);
    m_szDeviceName[DEVICENAMESIZE-1] = 0;
    wcscpy( m_szStoreName, g_szDEFAULT_STORAGE_NAME);
    wcscpy( m_szDefaultFileSystem, g_szDEFAULT_FILESYSTEM);
    wcscpy( m_szRootRegKey, L"");
    wcscpy( m_szOldDeviceName, L"");
    wcscpy( m_szFolderName, g_szDEFAULT_FOLDER_NAME);
    wcscpy( m_szActivityName, L"");
    InitializeCriticalSection( &m_cs);
   
// TODO: Read this from the registry
    m_dwFlags = 0;
}


CStore::~CStore()
{
   DEBUGMSG( 1, (L"CStore Destructor(%08X)\r\n", this));
// Delete the Partitions
// Delete the Partition Driver
    if (m_pPartDriver) {
        if (m_dwStoreId)
            m_pPartDriver->CloseStore( m_dwStoreId);
        delete m_pPartDriver;
    }   
    if (m_pStorageId) {
        delete [] m_pStorageId;
    }    
    if (m_hDisk) {
        CloseHandle( m_hDisk);
    }    
    if (m_pBlockDevice) {
        delete m_pBlockDevice;
    }   
    if (m_hActivityEvent) {
        CloseHandle( m_hActivityEvent);
    }   
    if (m_pDskStore) {
        delete m_pDskStore;
    }
    DeleteCriticalSection(&m_cs);
}


BOOL CStore::GetStoreInfo(STOREINFO *pInfo)
{
    __try {
        pInfo->dwAttributes = m_si.dwAttributes;
        pInfo->snBiggestPartCreatable = m_si.snBiggestPartCreatable;
        pInfo->dwBytesPerSector = m_si.dwBytesPerSector;
        pInfo->snFreeSectors = m_si.snFreeSectors;
        pInfo->snNumSectors = m_si.snNumSectors;
        pInfo->dwPartitionCount = m_dwPartCount;
        pInfo->dwMountCount = m_dwMountCount;
        wcscpy( pInfo->szDeviceName, m_szDeviceName);
        wcscpy( pInfo->szStoreName, m_szStoreName);
        memcpy( &pInfo->ftCreated, &m_si.ftCreated, sizeof(FILETIME));
        memcpy( &pInfo->ftLastModified, &m_si.ftLastModified, sizeof(FILETIME));
        memcpy( &pInfo->sdi, &m_sdi, sizeof(STORAGEDEVICEINFO));
    } __except( EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }
    return TRUE;
}

BOOL CStore::GetPartitionDriver(HKEY hKeyStorage, HKEY hKeyProfile)
{
    BOOL fValidDriver = FALSE, fValidName = FALSE;

    if (hKeyProfile) {
        // try to read the partition driver name from the profile key
        fValidName = FsdGetRegistryString(hKeyProfile, g_szPART_DRIVER_NAME_STRING, m_szPartDriverName, sizeof(m_szPartDriverName)/sizeof(WCHAR)); 
    }

    if (!fValidName && hKeyProfile) {
        // allow the legacy partition driver DLL name specified in the profile
        fValidDriver = FsdGetRegistryString(hKeyProfile, g_szPART_DRIVER_STRING, m_szPartDriver, sizeof(m_szPartDriver)/sizeof(WCHAR));        
    }

    if (!fValidName && !fValidDriver && hKeyStorage) {
        // try to read the partition driver name from the storage key
        fValidName = FsdGetRegistryString(hKeyStorage, g_szPART_DRIVER_NAME_STRING, m_szPartDriverName, sizeof(m_szPartDriverName)/sizeof(WCHAR));
    }

    if (fValidName && !fValidDriver) {
        // open the partition driver sub key
        HKEY hKeyPartition;
        TCHAR szSubKey[MAX_PATH];

        if (hKeyProfile) {
            // try to find the dll name under the profile partition driver sub key
            if (ERROR_SUCCESS == FsdRegOpenSubKey(hKeyProfile, m_szPartDriverName, &hKeyPartition)) {
                fValidDriver = FsdGetRegistryString(hKeyPartition, g_szPART_DRIVER_MODULE_STRING, m_szPartDriver, sizeof(m_szPartDriver)/sizeof(WCHAR));
                FsdRegCloseKey(hKeyPartition);
            }
        }

        if (!fValidDriver && hKeyStorage) {
            // try to find the dll name under the storage root partition driver key
            VERIFY(SUCCEEDED(StringCchPrintf(szSubKey, MAX_PATH, L"%s\\%s", g_szSTORAGE_PATH, m_szPartDriverName)));
            if (ERROR_SUCCESS == FsdRegOpenKey(szSubKey, &hKeyPartition)) {
                fValidDriver = FsdGetRegistryString(hKeyPartition, g_szPART_DRIVER_MODULE_STRING, m_szPartDriver, sizeof(m_szPartDriver)/sizeof(WCHAR));
                FsdRegCloseKey(hKeyPartition);
            }        
        }

    }

    if (!fValidDriver) {
        // unable to find a valid driver in the registry
        DEBUGMSG( ZONE_INIT, (L"Using the default HARDCODED partitioning driver (%s)!!!\r\n", g_szDEFAULT_PARTITION_DRIVER));
        wcscpy( m_szPartDriver, g_szDEFAULT_PARTITION_DRIVER);
    }

    if (!fValidName) {
        // unable to find a valid partition driver name in the registry,
        // generate the name from the name of the driver
        TCHAR *pszTmp;
        wcsncpy( m_szPartDriverName, m_szPartDriver, sizeof(m_szPartDriverName)/sizeof(WCHAR));
        if (pszTmp = wcsstr( m_szPartDriverName, L".")) {
            *pszTmp = L'\0';
        }
        DEBUGMSG( ZONE_INIT, (L"Using the generated partition driver name (%s)!!!\r\n", m_szPartDriverName));
    }
    
    return TRUE;
}

BOOL CStore::GetStoreSettings()
{   
    HKEY hKeyStorage=NULL, hKeyProfile = NULL;
    BOOL bRet = FALSE;
    if (ERROR_SUCCESS == FsdRegOpenKey( g_szPROFILE_PATH, &hKeyStorage)) {
        DUMPREGKEY(ZONE_INIT, g_szPROFILE_PATH, hKeyStorage);
        VERIFY(SUCCEEDED(StringCchPrintf(m_szRootRegKey, MAX_PATH, L"%s\\%s", g_szPROFILE_PATH, m_sdi.szProfile)));
        if (ERROR_SUCCESS != FsdRegOpenKey( m_szRootRegKey, &hKeyProfile)) {
            hKeyProfile = NULL;
        } else {
            DUMPREGKEY(ZONE_INIT, m_sdi.szProfile, hKeyProfile);
        }   
        if (!hKeyProfile || !FsdLoadFlag(hKeyProfile, g_szAUTO_MOUNT_STRING, &m_dwFlags, STORE_ATTRIBUTE_AUTOMOUNT))
            if (!FsdLoadFlag(hKeyStorage, g_szAUTO_MOUNT_STRING, &m_dwFlags, STORE_ATTRIBUTE_AUTOMOUNT))
                        m_dwFlags |= STORE_ATTRIBUTE_AUTOMOUNT;
        if (!hKeyProfile || !FsdLoadFlag(hKeyProfile, g_szAUTO_FORMAT_STRING, &m_dwFlags, STORE_ATTRIBUTE_AUTOFORMAT))
            FsdLoadFlag(hKeyStorage, g_szAUTO_FORMAT_STRING, &m_dwFlags, STORE_ATTRIBUTE_AUTOFORMAT);
        if (!hKeyProfile || !FsdLoadFlag(hKeyProfile, g_szAUTO_PART_STRING, &m_dwFlags, STORE_ATTRIBUTE_AUTOPART))
            FsdLoadFlag(hKeyStorage, g_szAUTO_PART_STRING, &m_dwFlags, STORE_ATTRIBUTE_AUTOPART);
        if (!hKeyProfile || !FsdGetRegistryString(hKeyProfile, g_szFILE_SYSTEM_STRING, m_szDefaultFileSystem, sizeof(m_szDefaultFileSystem)/sizeof(WCHAR)))
            if (!FsdGetRegistryString(hKeyStorage, g_szFILE_SYSTEM_STRING, m_szDefaultFileSystem, sizeof(m_szDefaultFileSystem)/sizeof(WCHAR)))
                wcscpy( m_szDefaultFileSystem, g_szDEFAULT_FILESYSTEM);
        if (!hKeyProfile || !FsdGetRegistryString(hKeyProfile, g_szFOLDER_NAME_STRING, m_szFolderName, sizeof(m_szFolderName)/sizeof(WCHAR)))
            if (!FsdGetRegistryString(hKeyStorage, g_szFOLDER_NAME_STRING, m_szFolderName, sizeof(m_szFolderName)/sizeof(WCHAR)))
                wcscpy( m_szFolderName, g_szDEFAULT_FOLDER_NAME);
        if (!hKeyProfile || !FsdGetRegistryString(hKeyProfile, g_szSTORE_NAME_STRING, m_szStoreName, sizeof(m_szStoreName)/sizeof(WCHAR)))
            if (!FsdGetRegistryString(hKeyStorage, g_szSTORE_NAME_STRING, m_szStoreName, sizeof(m_szStoreName)/sizeof(WCHAR)))
                wcscpy( m_szStoreName, g_szDEFAULT_STORAGE_NAME);
        // By default activity timer is enabled.  The Evnet Strings will specify the name.
        if (!hKeyProfile || !FsdLoadFlag(hKeyProfile, g_szACTIVITY_TIMER_ENABLE_STRING, &m_dwFlags, STORE_FLAG_ACTIVITYTIMER)) 
            if (!FsdLoadFlag(hKeyStorage, g_szACTIVITY_TIMER_ENABLE_STRING, &m_dwFlags, STORE_FLAG_ACTIVITYTIMER))
                m_dwFlags |= STORE_FLAG_ACTIVITYTIMER; 
        if (!hKeyProfile || !FsdGetRegistryString( hKeyProfile, g_szACTIVITY_TIMER_STRING, m_szActivityName, sizeof( m_szActivityName)/sizeof(WCHAR)))
            if (!FsdGetRegistryString(hKeyStorage, g_szACTIVITY_TIMER_STRING, m_szActivityName, sizeof(m_szActivityName)/sizeof(WCHAR)))
                wcscpy( m_szActivityName, g_szDEFAULT_ACTIVITY_NAME); 

        GetMountSettings(hKeyStorage, &m_dwMountFlags); // First storage manager
        GetMountSettings(hKeyProfile, &m_dwMountFlags); // Override in profile
        
        DWORD dwAttribs = 0;
        if (!hKeyProfile || !FsdGetRegistryValue(hKeyProfile, g_szATTRIB_STRING, &dwAttribs)) 
            FsdGetRegistryValue(hKeyStorage, g_szATTRIB_STRING, &dwAttribs);
        if (dwAttribs & STORE_ATTRIBUTE_READONLY)
                m_dwFlags |= STORE_ATTRIBUTE_READONLY;
            
    }
    if (!GetPartitionDriver(hKeyStorage, hKeyProfile))
        goto ExitFalse;
    bRet = TRUE;
ExitFalse:
    if (hKeyStorage) {
        FsdRegCloseKey( hKeyStorage);
    }    
    if (hKeyProfile) {
        FsdRegCloseKey( hKeyProfile);
    }
    return bRet;
}

BOOL CStore::IsValidPartition(CPartition * pPartition)
{
    CPartition *pTemp = m_pPartitionList;
    if (!pPartition || (pPartition == INVALID_PARTITION))
        return FALSE;
    while(pTemp) {
        if (pTemp == pPartition) {
            break;
        }
        pTemp = pTemp->m_pNextPartition;
    }
    return pTemp != NULL;
}

DWORD CStore::OpenDisk()
{
    DWORD dwError = ERROR_SUCCESS;
    STORAGE_IDENTIFICATION storageid;

    if (m_pBlockDevice) {
        m_hDisk = m_pBlockDevice->OpenBlockDevice();
    } else {
        m_hDisk = CreateFileW(m_szDeviceName,
                           GENERIC_READ | GENERIC_WRITE,
                           0,
                           NULL, OPEN_EXISTING, 0, NULL);
    
        if (m_hDisk == INVALID_HANDLE_VALUE) {
            dwError = GetLastError();
            if (dwError == ERROR_ACCESS_DENIED) {
                dwError = ERROR_SUCCESS;
                m_hDisk = CreateFileW(m_szDeviceName,
                                   GENERIC_READ,
                                   FILE_SHARE_READ,
                                   NULL, OPEN_EXISTING, 0, NULL);
                if (m_hDisk != INVALID_HANDLE_VALUE) {
                    m_dwFlags |= STORE_ATTRIBUTE_READONLY;
                } else {
                    dwError = GetLastError();
                }    
            }    
        }
    }   
    if (m_hDisk != INVALID_HANDLE_VALUE) {
        DWORD dwRet;
        dwError = DeviceIoControl( NULL, DISK_IOCTL_GETINFO, &m_di, sizeof(DISK_INFO), NULL, 0, &dwRet);
        if ((dwError == ERROR_BAD_COMMAND) || (dwError == ERROR_NOT_SUPPORTED)){
            memset( &m_di, 0, sizeof(DISK_INFO));   
            dwError = ERROR_SUCCESS;
        }
        if (dwError == ERROR_SUCCESS) {
            if (ERROR_SUCCESS != DeviceIoControl( NULL, IOCTL_DISK_DEVICE_INFO, &m_sdi, sizeof(STORAGEDEVICEINFO), NULL, 0, &dwRet)) {
                DEBUGMSG( ZONE_INIT, (L"FSDMGR!CStore::OpenDisk(0x%08X) call to IOCTL_DISK_DEVICE_INFO failed..filling info\r\n", this));
                m_sdi.dwDeviceClass = STORAGE_DEVICE_CLASS_BLOCK;
                m_sdi.dwDeviceFlags = STORAGE_DEVICE_FLAG_READWRITE;
                m_sdi.dwDeviceType = STORAGE_DEVICE_TYPE_UNKNOWN;
                wcscpy( m_sdi.szProfile, L"Default");
            }
        }    
        DEBUGMSG( ZONE_INIT, (L"FSDMGR!CStore::OpenDisk(0x%08X) DeviceInfo Class(0x%08X) Flags(0x%08X) Type(0x%08X) Profile(%s)\r\n", 
            this,
            m_sdi.dwDeviceClass,
            m_sdi.dwDeviceFlags,
            m_sdi.dwDeviceType,
            m_sdi.szProfile));
        SetLastError( ERROR_SUCCESS);    
        if (ERROR_INSUFFICIENT_BUFFER == DeviceIoControl( NULL, IOCTL_DISK_GET_STORAGEID, NULL, 0, &storageid, sizeof(STORAGE_IDENTIFICATION), &dwRet)) {
            m_pStorageId = new BYTE[storageid.dwSize];
            DeviceIoControl( NULL, IOCTL_DISK_GET_STORAGEID, NULL, 0, m_pStorageId, storageid.dwSize, &dwRet);
        }
        
    }
    return dwError;
}

void CStore::AddPartition(CPartition *pPartition)
{
    if (m_pPartitionList) {
        CPartition *pTemp = m_pPartitionList;
        while(pTemp->m_pNextPartition) {
            pTemp = pTemp->m_pNextPartition;
        }
        pTemp->m_pNextPartition = pPartition;
    } else {
        m_pPartitionList = pPartition;
    }    
}

void CStore::DeletePartition(CPartition * pPartition)
{
    CPartition *pTemp = m_pPartitionList;
    if (m_pPartitionList == pPartition) {
        m_pPartitionList = pPartition->m_pNextPartition;
    } else {    
        while(pTemp) {
            if (pTemp->m_pNextPartition && (pTemp->m_pNextPartition == pPartition)) {
                break;
            }
            pTemp = pTemp->m_pNextPartition;
        }    
        if (pTemp) {
            pTemp->m_pNextPartition = pPartition->m_pNextPartition;
        }
    }    
    UpdateHandleFromList(m_pRootHandle, NULL, pPartition);
    delete pPartition;
}

CPartition *CStore::FindPartition(LPCTSTR szPartitionName)
{
    CPartition *pPartition = m_pPartitionList;
    while( pPartition) {
        if (wcsicmp(pPartition->m_szPartitionName, szPartitionName) == 0) {
            break;
        }    
        pPartition = pPartition->m_pNextPartition;
    }
    return pPartition;
}

void CStore::GetPartitionCount()
{
    DWORD dwSearchId;
    m_dwPartCount = 0;
    if (ERROR_SUCCESS == m_pPartDriver->FindPartitionStart(m_dwStoreId, &dwSearchId)) {
        PD_PARTINFO pi;
        while( ERROR_SUCCESS == m_pPartDriver->FindPartitionNext(dwSearchId, &pi)) {
            m_dwPartCount++;
        }
        m_pPartDriver->FindPartitionClose(dwSearchId);
    }
}



BOOL CStore::LoadPartition(LPCTSTR szPartitionName, BOOL bMount, BOOL bFormat)
{
    DWORD dwPartitionId;
    HANDLE hPartition;
    if (ERROR_SUCCESS == m_pPartDriver->OpenPartition(m_dwStoreId, szPartitionName, &dwPartitionId)) {
        CPartition *pPartition = new CPartition(this, m_dwStoreId, dwPartitionId, m_pPartDriver, m_szFolderName, m_pBlockDevice);
        if (!pPartition) {
            m_pPartDriver->ClosePartition(dwPartitionId);
            return FALSE;
        }
        AddPartition( pPartition);
        pPartition->LoadPartition(szPartitionName);
        if (bMount && (m_dwFlags & STORE_ATTRIBUTE_AUTOMOUNT)) {
            OpenPartition( szPartitionName, &hPartition, GetCurrentProcess());
            if (pPartition->MountPartition(hPartition, m_szRootRegKey, m_szDefaultFileSystem, m_dwMountFlags, m_hActivityEvent, bFormat)) {
                m_dwMountCount++;
            } 
        }
        return TRUE;
    }
    return FALSE;
}

void CStore::LoadExistingPartitions(BOOL bMount, BOOL bFormat)
{
    DWORD dwSearchId;
    if (ERROR_SUCCESS == m_pPartDriver->FindPartitionStart(m_dwStoreId, &dwSearchId)) {
        PD_PARTINFO pi;
        while( ERROR_SUCCESS == m_pPartDriver->FindPartitionNext(dwSearchId, &pi)) {
            DEBUGMSG( 1, (L"Partition %s  NumSectors=%ld\r\n", pi.szPartitionName,  (DWORD)pi.snNumSectors));
            LoadPartition(pi.szPartitionName, bMount, bFormat);
        }
        m_pPartDriver->FindPartitionClose(dwSearchId);
    }
}


DWORD CStore::MountStore(BOOL bMount)
{
    DWORD dwError = ERROR_GEN_FAILURE;
    BOOL bFormat = FALSE;
    
    if(ERROR_SUCCESS == (dwError = OpenDisk())) {
        if (GetStoreSettings()) {
            m_pPartDriver = new CPartDriver();
            if (!m_pPartDriver) {
                return ERROR_OUTOFMEMORY;
            }    
            if (ERROR_SUCCESS == (dwError = m_pPartDriver->LoadPartitionDriver(m_szPartDriver))) {
                dwError = m_pPartDriver->OpenStore( m_hDisk, &m_dwStoreId );
                if (ERROR_DEVICE_NOT_PARTITIONED == dwError) {
                    if (m_dwStoreId)
                        m_pPartDriver->CloseStore( m_dwStoreId);
                    delete m_pPartDriver;
                    wcscpy( m_szPartDriver, L"");
                    m_pPartDriver = new CPartDriver();
                    if (!m_pPartDriver) {
                        return ERROR_OUTOFMEMORY;
                    }
                    m_pPartDriver->LoadPartitionDriver(m_szPartDriver);
                    dwError = m_pPartDriver->OpenStore( m_hDisk, &m_dwStoreId );
                }
                if (ERROR_SUCCESS == dwError) {                    
                    // Activity Timer Setup
                    if ((STORE_FLAG_ACTIVITYTIMER & m_dwFlags) && wcslen( m_szActivityName)) {
                        m_hActivityEvent = CreateEvent( NULL, FALSE, FALSE, m_szActivityName);
                        // there is really nothing we can do if event creation fails
                        DEBUGCHK(m_hActivityEvent);
                    }
                    DEBUGMSG( 1, (L"Opened the store hStore=%08X\r\n", m_dwStoreId));
                    m_si.cbSize = sizeof(PD_STOREINFO);
                    m_pPartDriver->GetStoreInfo( m_dwStoreId, &m_si);
                    DEBUGMSG( 1, (L"NumSec=%ld BytesPerSec=%ld FreeSec=%ld BiggestCreatable=%ld\r\n", (DWORD)m_si.snNumSectors, (DWORD)m_si.dwBytesPerSector, (DWORD)m_si.snFreeSectors, (DWORD)m_si.snBiggestPartCreatable));
                    if (ERROR_SUCCESS != m_pPartDriver->IsStoreFormatted(m_dwStoreId)) { 
                        if (m_dwFlags & STORE_ATTRIBUTE_AUTOFORMAT) {
                            // TODO: Check for failure
                            m_pPartDriver->FormatStore(m_dwStoreId);
                            m_pPartDriver->GetStoreInfo( m_dwStoreId, &m_si);
                        }    
                    }
                    if (ERROR_SUCCESS == m_pPartDriver->IsStoreFormatted(m_dwStoreId)) { // Retry again if we formatted it 
                        GetPartitionCount();
                        if (!m_dwPartCount) {
                            if (m_dwFlags & STORE_ATTRIBUTE_AUTOPART) {
                                PD_STOREINFO si;
                                si.cbSize = sizeof(PD_STOREINFO);
                                m_pPartDriver->GetStoreInfo(m_dwStoreId, &si);
                                if ( (ERROR_SUCCESS == m_pPartDriver->CreatePartition(m_dwStoreId, L"PART00", 0, si.snBiggestPartCreatable, TRUE)) &&
                                     (ERROR_SUCCESS == m_pPartDriver->FormatPartition(m_dwStoreId, L"PART00", 0, TRUE))) {
                                        m_pPartDriver->GetStoreInfo( m_dwStoreId, &m_si);
                                        DEBUGMSG( 1, (L"NumSec=%ld BytesPerSec=%ld FreeSec=%ld BiggestCreatable=%ld\r\n", (DWORD)m_si.snNumSectors, (DWORD)m_si.dwBytesPerSector, (DWORD)m_si.snFreeSectors, (DWORD)m_si.snBiggestPartCreatable));
                                        m_dwPartCount = 1;
                                        bFormat = TRUE;
                                }
                            }
                        } 
                        LoadExistingPartitions(bMount, bFormat);
                    }    
                    m_dwFlags |= STORE_FLAG_MOUNTED;
                }
            }    
        }   
    }    
    return dwError;
}

BOOL CStore::UnmountStore(BOOL bDelete)
{
    CPartition *pPartition = m_pPartitionList;
    while( pPartition) {
        if (pPartition->IsPartitionMounted()) {
            if (pPartition->UnmountPartition()) {
                m_dwMountCount--;
            } else {
                pPartition = pPartition->m_pNextPartition;
                continue;
            }    
        }    
        if (bDelete) {
            m_pPartitionList = pPartition->m_pNextPartition;
            UpdateHandleFromList(m_pRootHandle, NULL, pPartition);
            delete pPartition;
            pPartition = m_pPartitionList;
        } else {
            pPartition = pPartition->m_pNextPartition;
        }    
    }    
    return (m_dwMountCount == 0);
}

DWORD CStore::FormatStore()
{
    DWORD dwError = ERROR_SUCCESS;
    Lock();
    if (m_dwMountCount) {
        dwError = ERROR_DEVICE_IN_USE;
    } 
    else if (m_dwFlags & STORE_ATTRIBUTE_READONLY) {
        dwError = ERROR_ACCESS_DENIED;
    }
    else if (ERROR_SUCCESS == (dwError = m_pPartDriver->FormatStore( m_dwStoreId))) {
        if (m_dwFlags & STORE_FLAG_MOUNTED) {
            UnmountStore();
        }   
        m_dwPartCount = 0;
        m_si.cbSize = sizeof(PD_STOREINFO);
        m_pPartDriver->GetStoreInfo(m_dwStoreId, &m_si);
        // TODO: Should we auto partition here ???            
    }    
    Unlock();
    return dwError;
}    

DWORD CStore::CreatePartition(LPCTSTR szPartitionName, BYTE bPartType, SECTORNUM snNumSectors, BOOL bAuto)
{
    DWORD dwError = ERROR_SUCCESS;
    
    Lock();
    if (snNumSectors == 0) {
        snNumSectors = m_si.snBiggestPartCreatable;
    }    
    if (ERROR_SUCCESS == (dwError = m_pPartDriver->CreatePartition(m_dwStoreId, szPartitionName, bPartType, snNumSectors, bAuto))) {
        m_pPartDriver->GetStoreInfo(m_dwStoreId, &m_si);
        // Load up the new partition
        LoadPartition(szPartitionName, TRUE, TRUE);
        m_dwPartCount++;
    } 
    Unlock();
    return dwError;
}

DWORD CStore::DeletePartition(LPCTSTR szPartitionName)
{
    DWORD dwError = ERROR_SUCCESS;
    CPartition *pPartition;
       
    Lock();
    if (pPartition = FindPartition( szPartitionName)) {
        if (pPartition->IsPartitionMounted()) {
           dwError = ERROR_DEVICE_IN_USE;
        } else {
           if (ERROR_SUCCESS == (dwError = m_pPartDriver->DeletePartition(m_dwStoreId, szPartitionName))) {
               DeletePartition(pPartition);
               m_pPartDriver->GetStoreInfo(m_dwStoreId, &m_si);
               m_dwPartCount--;
           }    
        }  
    }
     Unlock();
    return dwError;
}

DWORD CStore::OpenPartition(LPCTSTR szPartitionName, HANDLE *pbHandle, HANDLE hProc)
{
    DWORD dwError = ERROR_SUCCESS;
    STOREHANDLE *pStoreHandle = new STOREHANDLE;
    *pbHandle = INVALID_HANDLE_VALUE;
    if (!pStoreHandle) {
        return ERROR_OUTOFMEMORY;
    }    
    Lock();
    if (pStoreHandle->pPartition = FindPartition(szPartitionName)) {
        pStoreHandle->pStore = this;
        pStoreHandle->pNext = NULL;
        pStoreHandle->dwFlags = 0;
        pStoreHandle->dwSig = PART_HANDLE_SIG;
        if (hProc == GetCurrentProcess()) {
            pStoreHandle->dwFlags |= STOREHANDLE_TYPE_INTERNAL;
        }
        pStoreHandle->hProc = hProc;
        AddHandleToList(&m_pRootHandle, pStoreHandle);
    } else {
        delete pStoreHandle;
        pStoreHandle = NULL;
    }    
    Unlock();
    if (pStoreHandle) {
        *pbHandle = CreateAPIHandle( g_hStoreApi, pStoreHandle);
        if (*pbHandle != INVALID_HANDLE_VALUE) {
            SetHandleOwner( *pbHandle, hProc);
        }
    } else {
        dwError = ERROR_FILE_NOT_FOUND;
    }    
    return dwError;    
}

DWORD CStore::MountPartition(CPartition * pPartition)
{
    HANDLE hPartition;
    DWORD dwError = ERROR_SUCCESS;
    Lock();
    if (IsValidPartition(pPartition)){
        if (pPartition->IsPartitionMounted()){
            dwError = ERROR_ALREADY_EXISTS;
        } else {
            OpenPartition( pPartition->m_szPartitionName, &hPartition, GetCurrentProcess());
            if (pPartition->MountPartition(hPartition, m_szRootRegKey, m_szDefaultFileSystem, m_dwMountFlags, m_hActivityEvent, FALSE)) {
                m_dwMountCount++;
            } else {
                dwError = ERROR_GEN_FAILURE; 
            }
        }    
    }
    Unlock();
    return dwError;
}

DWORD CStore::UnmountPartition(CPartition * pPartition)
{
    DWORD dwError = ERROR_SUCCESS;
    Lock();
    if (IsValidPartition(pPartition)){
        if (!pPartition->IsPartitionMounted()){
            dwError = ERROR_FILE_NOT_FOUND;
        } else {
            if (pPartition->UnmountPartition()) {
                m_dwMountCount--;
            } else {
                dwError = ERROR_GEN_FAILURE; 
            }
        }    
    }
    Unlock();
    return dwError;
}   

DWORD CStore::RenameParttion(CPartition * pPartition, LPCTSTR szNewName)
{
    DWORD dwError = ERROR_SUCCESS;
    Lock();
    if (IsValidPartition(pPartition)){
        if (!pPartition->RenamePartition(szNewName)) {
            GetDriverError(&dwError);
        }    
    }
    Unlock();
    return dwError;
}   

    

DWORD CStore::SetPartitionAttributes(CPartition * pPartition, DWORD dwAttrs)
{
    DWORD dwError = ERROR_SUCCESS;
    Lock();
    if (IsValidPartition(pPartition)){
        if (!pPartition->SetPartitionAttributes(dwAttrs)) {
            GetDriverError(&dwError);
        }    
    }
    Unlock();
    return dwError;
}   

DWORD CStore::SetPartitionSize(CPartition *pPartition, SECTORNUM snNumSectors)
{
    DWORD dwError = ERROR_SUCCESS;
    Lock();
    if (IsValidPartition(pPartition)) {
        if (!pPartition->SetPartitionSize(snNumSectors)) {
            GetDriverError(&dwError);
        } else {
            m_pPartDriver->GetStoreInfo(m_dwStoreId, &m_si);
        }
    }
    Unlock();
    return dwError;
}

DWORD CStore::GetPartitionInfo(CPartition * pPartition, PPARTINFO pInfo)
{
    DWORD dwError = ERROR_SUCCESS;
    Lock();
    if (IsValidPartition(pPartition)){
        if (!pPartition->GetPartitionInfo(pInfo)) {
            dwError = ERROR_BAD_ARGUMENTS;
        }    
    }
    Unlock();
    return dwError;
}   

    
DWORD CStore::FormatPartition(CPartition *pPartition, BYTE bPartType, BOOL bAuto)
{
    DWORD dwError = ERROR_SUCCESS;
    Lock();
    if (IsValidPartition(pPartition)){
        if (!pPartition->FormatPartition(bPartType, bAuto)) {
            GetDriverError( &dwError);
        }    
    }
    Unlock();
    return dwError;
}

DWORD CStore::FindFirstPartition(PPARTINFO pInfo, HANDLE * pHandle, HANDLE hProc)
{
    DWORD dwError = ERROR_SUCCESS;
    *pHandle = INVALID_HANDLE_VALUE;
    SEARCHPARTITION*pSearch = new SEARCHPARTITION;
    if (!pSearch) {
        return ERROR_OUTOFMEMORY;
    }
    Lock();
    if (m_pPartitionList) {
        pSearch->pStore = this;
        pSearch->pPartition = m_pPartitionList;
        pSearch->pPartition->GetPartitionInfo( pInfo);
        pSearch->pNext = NULL;
        pSearch->dwFlags = STOREHANDLE_TYPE_SEARCH;
        pSearch->hProc = hProc;
        pSearch->dwSig = PART_HANDLE_SIG;
        AddHandleToList(&m_pRootHandle, pSearch);
        *pHandle = CreateAPIHandle(g_hFindPartApi, pSearch);
    } else {
        delete pSearch;
        dwError = ERROR_NO_MORE_ITEMS;
    }    
    Unlock();
    return dwError;
}

DWORD CStore::FindNextPartition(PSEARCHPARTITION pSearch, PARTINFO *pInfo)
{
    DWORD dwError = ERROR_NO_MORE_ITEMS;
    Lock();
    if (IsValidPartition(pSearch->pPartition)) {
        if (pSearch->dwFlags & STOREHANDLE_TYPE_CURRENT) {
            pSearch->dwFlags &= ~STOREHANDLE_TYPE_CURRENT;
        } else {
            pSearch->pPartition= pSearch->pPartition->m_pNextPartition;
        }    
        if (pSearch->pPartition && (pSearch->pPartition != INVALID_PARTITION)) {
            pSearch->pPartition->GetPartitionInfo( pInfo);
            dwError = ERROR_SUCCESS;
        }    
    }
    Unlock();
    return dwError;
}

DWORD CStore::FindClosePartition(PSEARCHPARTITION pSearch)
{
    DWORD dwError = ERROR_SUCCESS;
    Lock();
    if (DeleteHandleFromList(&m_pRootHandle, pSearch)) {
        delete pSearch;
    } else {
        dwError = ERROR_INVALID_OPERATION;
    }   
    Unlock();
    return dwError;
}

 DWORD CStore::DeviceIoControl(CPartition *pPartition, DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned)
 {
    DWORD dwError = ERROR_SUCCESS;
    
    if (IsValidPartition(pPartition)) {

        dwError = pPartition->DeviceIoControl(dwIoControlCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned);

    } else {

        switch(dwIoControlCode) {
        case IOCTL_DISK_WRITE:
        case IOCTL_DISK_READ:
        case DISK_IOCTL_READ:
        case DISK_IOCTL_WRITE:
            if (!MapSgBuffers ((PSG_REQ)pInBuf, nInBufSize))
                return FALSE;
            break;
        case IOCTL_CDROM_READ_SG:
            if (!MapSgBuffers ((PCDROM_READ)pInBuf, nInBufSize))
                return FALSE;
            break;           
        default:
            break;
        }

        if (!InternalStoreIoControl( this, dwIoControlCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, NULL)) {
            GetDriverError(&dwError);
        }
    }
    return dwError;
 }

/*  direct access to i/o control for a store. intended for use by the partition driver to 
    directly access the store.
 */
 
#ifdef UNDER_CE
BOOL CStore::InternalStoreIoControl(PSTORE pStore, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{   
    BOOL fRet = FALSE;
    // TODO: if pStore->m_pBlockDevice is valid, directly call function ptr
    return ::DeviceIoControl(pStore->m_hDisk, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
}
#endif

//
// Helper functions for CStore::DeviceIoControl
//
#ifdef UNDER_CE
BOOL CStore::MapSgBuffers(PSG_REQ pSgReq ,DWORD InBufLen)
{
    if  (pSgReq && InBufLen >= (sizeof(SG_REQ) + sizeof(SG_BUF) * (pSgReq->sr_num_sg - 1))) {
        DWORD dwIndex;
        for (dwIndex=0; dwIndex < pSgReq -> sr_num_sg; dwIndex++) {
            pSgReq->sr_sglist[dwIndex].sb_buf = 
                (PUCHAR)MapCallerPtr((LPVOID)pSgReq->sr_sglist[dwIndex].sb_buf,pSgReq->sr_sglist[dwIndex].sb_len);
        }
    }
    else // Parameter Wrong.
        return FALSE;
    
    return TRUE;
}
BOOL CStore::MapSgBuffers(PCDROM_READ pCdrom ,DWORD InBufLen)
{
    if  (pCdrom && InBufLen >= (sizeof(CDROM_READ) + sizeof(SGX_BUF) * (pCdrom->sgcount - 1))) {
        DWORD dwIndex;
        for (dwIndex=0; dwIndex < pCdrom-> sgcount; dwIndex++) {
            pCdrom->sglist[dwIndex].sb_buf = 
                (PUCHAR)MapCallerPtr((LPVOID)pCdrom->sglist[dwIndex].sb_buf,pCdrom->sglist[dwIndex].sb_len);
        }
    }
    else // Parameter Wrong.
        return FALSE;
    return TRUE;
}
#else
// NT stub
BOOL CStore::MapSgBuffers(PSG_REQ pSgReq ,DWORD InBufLen)
{
    return TRUE;
}

// NT stub
BOOL CStore::MapSgBuffers(PCDROM_READ pCdrom ,DWORD InBufLen)
{
    return TRUE;
}
#endif // UNDER_CE
