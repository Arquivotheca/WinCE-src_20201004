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

const TCHAR *g_szSTORAGE_PATH = L"System\\StorageManager";
const TCHAR *g_szPROFILE_PATH = L"System\\StorageManager\\Profiles";
const TCHAR *g_szAUTOLOAD_PATH = L"System\\StorageManager\\AutoLoad";

const TCHAR *g_szReloadTimeOut= L"PNPUnloadDelay";
const TCHAR *g_szWaitDelay=L"PNPWaitIODelay";
const TCHAR *g_szPNPThreadPrio=L"PNPThreadPrio256";

const TCHAR *g_szAUTO_PART_STRING = L"AutoPart";
const TCHAR *g_szAUTO_MOUNT_STRING = L"AutoMount";
const TCHAR *g_szAUTO_FORMAT_STRING = L"AutoFormat";
const TCHAR *g_szPART_DRIVER_STRING = L"PartitionDriver";
const TCHAR *g_szPART_TABLE_STRING = L"PartitionTable";
const TCHAR *g_szFILE_SYSTEM_STRING = L"DefaultFileSystem";
const TCHAR *g_szFILE_SYSTEM_MODULE_STRING = L"Dll";
const TCHAR *g_szFSD_BOOTPHASE_STRING = L"BootPhase";
const TCHAR *g_szFILE_SYSTEM_DRIVER_STRING = L"DriverPath";
const TCHAR *g_szFSD_ORDER_STRING = L"Order";
const TCHAR *g_szFSD_LOADFLAG_STRING = L"LoadFlags";
const TCHAR *g_szSTORE_NAME_STRING =L"Name";
const TCHAR *g_szFOLDER_NAME_STRING =L"Folder";
const TCHAR *g_szMOUNT_FLAGS_STRING =L"MountFlags";
const TCHAR *g_szATTRIB_STRING =L"Attrib";
const TCHAR *g_szACTIVITY_TIMER_STRING = L"ActivityEvent";
const TCHAR *g_szACTIVITY_TIMER_ENABLE_STRING = L"EnableActivityEvent";

const TCHAR *g_szDEFAULT_PARTITION_DRIVER = L"mspart.dll";
const TCHAR *g_szDEFAULT_FILESYSTEM     = L"FATFS";
const TCHAR *g_szDEFAULT_STORAGE_NAME = L"External Storage";
const TCHAR *g_szDEFAULT_FOLDER_NAME = L"Mounted Volume";
const TCHAR *g_szDEFAULT_ACTIVITY_NAME = L""; // No activity timer

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
        m_dwRefCount(0)
{
    memset( &m_si, 0, sizeof(PD_STOREINFO));
    memset( &m_sdi, 0, sizeof(STORAGEDEVICEINFO));
    memcpy( &m_DeviceGuid, &DeviceGuid, sizeof(GUID));
    wcscpy( m_szPartDriver, L"");
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
    BOOL fSuccess = FALSE;
// Step1: Read Device2 Registry Key
    if (hKeyProfile) {
        fSuccess = FsdGetRegistryString(hKeyProfile, g_szPART_DRIVER_STRING, m_szPartDriver, sizeof(m_szPartDriver)/sizeof(WCHAR));
    }    
// TODO: Step2: Query the Device
// TODO: Step3: Enum through Partition Drivers to see if any of them recogize it
// Step4: If all else fails then get the default partition Driver
    if (!fSuccess && hKeyStorage) {
        fSuccess = FsdGetRegistryString( hKeyStorage, g_szPART_DRIVER_STRING, m_szPartDriver, sizeof(m_szPartDriver)/sizeof(WCHAR));
    }
// Step5: If all else fails then setup the hardcoded partition driver
    if (!fSuccess) {
        DEBUGMSG( ZONE_INIT, (L"Using the default HARDCODED partitioning driver !!!\r\n"));
        wcscpy( m_szPartDriver, L"mspart.dll");
    }    
    return TRUE;
}

BOOL CStore::GetStoreSettings()
{   
    HKEY hKeyStorage=NULL, hKeyProfile = NULL;
    BOOL bRet = FALSE;
    if (ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, g_szPROFILE_PATH, 0, 0, &hKeyStorage)) {
        DUMPREGKEY(ZONE_INIT, g_szPROFILE_PATH, hKeyStorage);
        wsprintf( m_szRootRegKey, L"%s\\%s", g_szPROFILE_PATH, m_sdi.szProfile);
        if (ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE, m_szRootRegKey, 0, 0, &hKeyProfile)) {
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
                
        if (!hKeyProfile || !FsdGetRegistryValue(hKeyProfile, g_szMOUNT_FLAGS_STRING, &m_dwMountFlags)) 
            if (!FsdGetRegistryValue(hKeyStorage, g_szMOUNT_FLAGS_STRING, &m_dwMountFlags)) 
                m_dwMountFlags = 0;

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
        RegCloseKey( hKeyStorage);
    }    
    if (hKeyProfile) {
        RegCloseKey( hKeyProfile);
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
        if (!::DeviceIoControl(m_hDisk, DISK_IOCTL_GETINFO, &m_di, sizeof(DISK_INFO), NULL, 0, &dwRet, NULL)) {
            dwError = GetLastError();
            if ((dwError == ERROR_BAD_COMMAND) || (dwError == ERROR_NOT_SUPPORTED)){
                memset( &m_di, 0, sizeof(DISK_INFO));   
                dwError = ERROR_SUCCESS;
            }
        }
        if (dwError == ERROR_SUCCESS) {
            if (!::DeviceIoControl( m_hDisk, IOCTL_DISK_DEVICE_INFO, &m_sdi, sizeof(STORAGEDEVICEINFO), NULL, 0, &dwRet, NULL)) {
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
        ::DeviceIoControl(m_hDisk, IOCTL_DISK_GET_STORAGEID, NULL, 0, &storageid, sizeof(STORAGE_IDENTIFICATION), &dwRet, NULL);
        if (ERROR_INSUFFICIENT_BUFFER == GetLastError()) {
            m_pStorageId = new BYTE[storageid.dwSize];
            ::DeviceIoControl(m_hDisk, IOCTL_DISK_GET_STORAGEID, NULL, 0, m_pStorageId, storageid.dwSize, &dwRet, NULL);
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



BOOL CStore::LoadPartition(LPCTSTR szPartitionName, BOOL bMount)
{
    DWORD dwPartitionId;
    HANDLE hPartition;
    if (ERROR_SUCCESS == m_pPartDriver->OpenPartition(m_dwStoreId, szPartitionName, &dwPartitionId)) {
        CPartition *pPartition = new CPartition(m_dwStoreId, dwPartitionId, m_pPartDriver, m_szFolderName);
        if (!pPartition) {
            m_pPartDriver->ClosePartition(dwPartitionId);
            return FALSE;
        }
        AddPartition( pPartition);
        pPartition->LoadPartition(szPartitionName);
        if (bMount && (m_dwFlags & STORE_ATTRIBUTE_AUTOMOUNT)) {
            OpenPartition( szPartitionName, &hPartition, GetCurrentProcess());
            if (pPartition->MountPartition(hPartition, m_szRootRegKey, m_szDefaultFileSystem, m_dwMountFlags, m_hActivityEvent)) {
                m_dwMountCount++;
            } 
        }
        return TRUE;
    }
    return FALSE;
}

void CStore::LoadExistingPartitions(BOOL bMount)
{
    DWORD dwSearchId;
    if (ERROR_SUCCESS == m_pPartDriver->FindPartitionStart(m_dwStoreId, &dwSearchId)) {
        PD_PARTINFO pi;
        while( ERROR_SUCCESS == m_pPartDriver->FindPartitionNext(dwSearchId, &pi)) {
            DEBUGMSG( 1, (L"Partition %s  NumSectors=%ld\r\n", pi.szPartitionName,  (DWORD)pi.snNumSectors));
            LoadPartition(pi.szPartitionName, bMount);
        }
        m_pPartDriver->FindPartitionClose(dwSearchId);
    }
}


DWORD CStore::MountStore(BOOL bMount)
{
    DWORD dwError = ERROR_GEN_FAILURE;
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
                    m_pPartDriver->LoadPartitionDriver(m_szPartDriver);
                    dwError = m_pPartDriver->OpenStore( m_hDisk, &m_dwStoreId );
                }
                if (ERROR_SUCCESS == dwError) {
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
                                }
                            }
                        } 
                        LoadExistingPartitions(bMount);
                    }    
                    // Activity Timer Setup
                    if ((STORE_FLAG_ACTIVITYTIMER & m_dwFlags) && wcslen( m_szActivityName)) {
                        m_hActivityEvent = CreateEvent( NULL, FALSE, FALSE, m_szActivityName);
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
        LoadPartition(szPartitionName, TRUE);
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
            if (pPartition->MountPartition(hPartition, m_szRootRegKey, m_szDefaultFileSystem, m_dwMountFlags, m_hActivityEvent)) {
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
    if (pPartition) {
        if (IsValidPartition(pPartition)) {
            dwError = pPartition->DeviceIoControl(dwIoControlCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned);
        } else {
            dwError = ERROR_DEVICE_NOT_AVAILABLE;
        }
    } else {
        if (!::DeviceIoControl( m_hDisk, dwIoControlCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, NULL)) {
            GetDriverError(&dwError);
        }
    }
    return dwError;
 }

