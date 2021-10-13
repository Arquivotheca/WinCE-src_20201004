//
// Copyright ( c ) Microsoft Corporation.  All rights reserved.
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

#ifndef UNDER_CE

#include "storeincludes.hpp"

extern BOOL CONV_DevCloseFileHandle(StoreDisk_t **ppStore);
extern BOOL EnumSnapshotDelete ( PDLIST pItem, LPVOID pEnumData );
extern HANDLE g_hStoreDiskApi;

StoreDisk_t::StoreDisk_t ( ):
        m_dwStoreId ( 0 ),
        m_pPartDriver ( NULL ),
        m_dwPartCount ( 0 ),
        m_dwMountCount ( 0 ),
        m_hActivityEvent ( NULL ),
        m_dwDefaultMountFlags ( 0 ),
        m_hDisk ( INVALID_HANDLE_VALUE ),
        m_hStoreDisk ( INVALID_HANDLE_VALUE ),
        m_hStore ( INVALID_HANDLE_VALUE ),
        m_pStorageId ( NULL ),
        m_pBlockDevice ( NULL ),
        m_dwPowerCount ( 0 ),
        m_dwFlags ( 0 ),
        m_dwState ( STORE_STATE_DETACHED )
{
    InitializeCriticalSection ( &m_cs );
    //Init the StoreList 
    InitDList ( &m_link );
    m_RefCount = 1;
    DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "Store %x ( Ref= %d ) Created \n" ),this,m_RefCount ) );
    m_ThisLogicalDiskType = StoreDisk_Type;
}

        StoreDisk_t::StoreDisk_t ( const WCHAR *szDeviceName, const GUID* pDeviceGuid ) :
        m_dwStoreId ( 0 ),
        m_pPartDriver ( NULL ),
        m_dwPartCount ( 0 ),
        m_dwMountCount ( 0 ),
        m_hActivityEvent ( NULL ),
        m_dwDefaultMountFlags ( 0 ),
        m_hDisk ( INVALID_HANDLE_VALUE ),
        m_hStoreDisk ( INVALID_HANDLE_VALUE ),
        m_hStore ( INVALID_HANDLE_VALUE ),
        m_pStorageId ( NULL ),
        m_pBlockDevice ( NULL ),
        m_dwPowerCount ( 0 ),
        m_dwFlags ( 0 ),
        m_dwState ( STORE_STATE_DETACHED )
{
    memset ( &m_si, 0, sizeof ( PD_STOREINFO ) );
    memset ( &m_sdi, 0, sizeof ( STORAGEDEVICEINFO ) );
    memcpy ( &m_DeviceGuid, pDeviceGuid, sizeof ( GUID ) );
    m_szPartDriver[0] = '\0';
    m_szPartDriverName[0] = '\0';
    m_szRootRegKey[0] = '\0';
    m_szActivityName[0] = '\0';
    VERIFY ( SUCCEEDED ( StringCchCopy ( m_szDeviceName, _countof ( m_szDeviceName ), szDeviceName ) ) );
    VERIFY ( SUCCEEDED ( StringCchCopy ( m_szStoreName, STORENAMESIZE, g_szDEFAULT_STORAGE_NAME ) ) );
    VERIFY ( SUCCEEDED ( StringCchCopy ( m_szDefaultFileSystem, MAX_PATH, g_szDEFAULT_FILESYSTEM ) ) );
    VERIFY ( SUCCEEDED ( StringCchCopy ( m_szFolderName, FOLDERNAMESIZE, g_szDEFAULT_FOLDER_NAME ) ) );



    InitializeCriticalSection ( &m_cs );
    //Init the StoreList 
    InitDList ( &m_link );
    m_RefCount = 1;
    DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "Store %x ( Ref= %d ) Created \n" ),this,m_RefCount ) );
    m_ThisLogicalDiskType = StoreDisk_Type;
}

void StoreDisk_t::DeletePartDriver() 
{
    // Delete the Partition Driver
    if ( m_pPartDriver ) {
        if ( m_dwStoreId )
            m_pPartDriver->CloseStore ( m_dwStoreId );
        delete m_pPartDriver;
        m_pPartDriver = NULL;
    }
}

StoreDisk_t::~StoreDisk_t ( )
{
    ASSERT (!OWN_CS (&m_cs));
    ASSERT (m_RefCount == 0);
    ASSERT (m_hStoreDisk == INVALID_HANDLE_VALUE);
    ASSERT (m_hDisk == INVALID_HANDLE_VALUE);

    DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!StoreDisk_t::~StoreDisk_t: deleting store ( %p ) \n", this ) );

    // Delete the Partition Driver
    DeletePartDriver();

    if ( m_pStorageId ) {
        delete [] m_pStorageId;
    }

    if ( m_pBlockDevice ) {
        delete m_pBlockDevice;
        m_pBlockDevice = NULL;
    }

    if ( m_hActivityEvent ) {
#ifdef UNDER_CE
        ::CloseHandle (m_hActivityEvent);
#else
        ::NotifyCloseEvent (m_hActivityEvent);
#endif
    }

    ::DeleteCriticalSection ( &m_cs );

    DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "Store %x ( Ref= %d ) Destroyed \n" ),this,m_RefCount ) );
}


void StoreDisk_t::PowerOff ( )
{
    Lock ( );
    if ( ++m_dwPowerCount ==m_dwMountCount ) {
        if ( m_pBlockDevice ) {
            m_pBlockDevice->PowerOff ( );
        }
    }
    Unlock ( );
}

void StoreDisk_t::PowerOn ( )
{
    Lock ( );
    if ( m_dwPowerCount == m_dwMountCount ) {
        if ( m_pBlockDevice ) {
            m_pBlockDevice->PowerOn ( );
        }
    }
    m_dwPowerCount--;
    Unlock ( );
}

BOOL StoreDisk_t::SetBlockDevice ( const TCHAR *szDriverName )
{
    m_pBlockDevice = new BlockDevice_t ( szDriverName );
    return ( NULL != m_pBlockDevice );
}

BOOL StoreDisk_t::GetStoreInfo ( STOREINFO *pInfo )
{
    ASSERT ( !IsDetached(  ) );

    __try {
        pInfo->dwAttributes = m_si.dwAttributes;
        pInfo->snBiggestPartCreatable = m_si.snBiggestPartCreatable;
        pInfo->dwBytesPerSector = m_si.dwBytesPerSector;
        pInfo->snFreeSectors = m_si.snFreeSectors;
        pInfo->snNumSectors = m_si.snNumSectors;
        pInfo->dwPartitionCount = m_dwPartCount;
        pInfo->dwMountCount = m_dwMountCount;
        pInfo->dwDeviceClass = m_sdi.dwDeviceClass;
        pInfo->dwDeviceFlags = m_sdi.dwDeviceFlags;
        pInfo->dwDeviceType = m_sdi.dwDeviceType;
        VERIFY ( GetShortDeviceName ( m_szDeviceName, pInfo->szDeviceName, DEVICENAMESIZE ) );
        VERIFY ( SUCCEEDED ( StringCchCopy ( pInfo->szStoreName, STORENAMESIZE, m_szStoreName ) ) );
        memcpy ( &pInfo->ftCreated, &m_si.ftCreated, sizeof ( FILETIME ) );
        memcpy ( &pInfo->ftLastModified, &m_si.ftLastModified, sizeof ( FILETIME ) );
        memcpy ( &pInfo->sdi, &m_sdi, sizeof ( STORAGEDEVICEINFO ) );
    } __except ( EXCEPTION_EXECUTE_HANDLER ) {
        return FALSE;
    }
    return TRUE;
}

BOOL StoreDisk_t::GetPartitionDriver ( HKEY hKeyStorage, HKEY hKeyProfile )
{
    BOOL fValidDriver = FALSE, fValidName = FALSE;

    if ( hKeyProfile ) {
        // try to read the partition driver name from the profile key
        fValidName = FsdGetRegistryString ( hKeyProfile, g_szPART_DRIVER_NAME_STRING, m_szPartDriverName, sizeof ( m_szPartDriverName )/sizeof ( WCHAR ) );
    }

    if ( !fValidName && hKeyProfile ) {
        // allow the legacy partition driver DLL name specified in the profile
        fValidDriver = FsdGetRegistryString ( hKeyProfile, g_szPART_DRIVER_STRING, m_szPartDriver, sizeof ( m_szPartDriver )/sizeof ( WCHAR ) );
    }

    if ( !fValidName && !fValidDriver && hKeyStorage ) {
        // try to read the partition driver name from the storage key
        fValidName = FsdGetRegistryString ( hKeyStorage, g_szPART_DRIVER_NAME_STRING, m_szPartDriverName, sizeof ( m_szPartDriverName )/sizeof ( WCHAR ) );
    }

    if ( fValidName && !fValidDriver ) {
        // open the partition driver sub key
        HKEY hKeyPartition;
        WCHAR szSubKey[MAX_PATH];

        if ( hKeyProfile ) {
            // try to find the dll name under the profile partition driver sub key
            if ( ERROR_SUCCESS == FsdRegOpenSubKey ( hKeyProfile, m_szPartDriverName, &hKeyPartition ) ) {
                fValidDriver = FsdGetRegistryString ( hKeyPartition, g_szPART_DRIVER_MODULE_STRING, m_szPartDriver, sizeof ( m_szPartDriver )/sizeof ( WCHAR ) );
                FsdRegCloseKey ( hKeyPartition );
            }
        }

        if ( !fValidDriver && hKeyStorage ) {
            // try to find the dll name under the storage root partition driver key
            VERIFY ( SUCCEEDED ( StringCchPrintf ( szSubKey, MAX_PATH, L"%s\\%s", g_szSTORAGE_PATH, m_szPartDriverName ) ) );
            if ( ERROR_SUCCESS == FsdRegOpenKey ( szSubKey, &hKeyPartition ) ) {
                fValidDriver = FsdGetRegistryString ( hKeyPartition, g_szPART_DRIVER_MODULE_STRING, m_szPartDriver, sizeof ( m_szPartDriver )/sizeof ( WCHAR ) );
                FsdRegCloseKey ( hKeyPartition );
            }
        }

    }

    if ( !fValidDriver ) {
        // unable to find a valid driver in the registry
        DEBUGMSG ( 1, ( L"FSDMGR!StoreDisk_t::GetPartitionDriver: Using the default HARDCODED partitioning driver ( %s )!!!\r\n", g_szDEFAULT_PARTITION_DRIVER ) );
        VERIFY ( SUCCEEDED ( StringCchCopy ( m_szPartDriver, MAX_PATH, g_szDEFAULT_PARTITION_DRIVER ) ) );
    }

    if ( !fValidName ) {
        // unable to find a valid partition driver name in the registry,
        // generate the name from the name of the driver
        WCHAR *pszTmp;
        StringCbCopyW ( m_szPartDriverName, sizeof ( m_szPartDriverName ), m_szPartDriver );
        pszTmp = wcsstr ( m_szPartDriverName, L"." );
        if ( pszTmp ) {
            *pszTmp = L'\0';
        }
        DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!StoreDisk_t::GetPartitionDriver: Using the generated partition driver name ( %s )!!!\r\n", m_szPartDriverName ) );
    }

    return TRUE;
}

BOOL StoreDisk_t::InitStoreSettings ( )
{
    HKEY hKeyStorage=NULL, hKeyProfile = NULL;
    BOOL bRet = FALSE;
    if ( ERROR_SUCCESS == FsdRegOpenKey ( g_szPROFILE_PATH, &hKeyStorage ) ) {
        DUMPREGKEY ( ZONE_INIT, g_szPROFILE_PATH, hKeyStorage );
        VERIFY ( SUCCEEDED ( StringCchPrintf ( m_szRootRegKey, MAX_PATH, L"%s\\%s", g_szPROFILE_PATH, m_sdi.szProfile ) ) );
        if ( ERROR_SUCCESS != FsdRegOpenKey ( m_szRootRegKey, &hKeyProfile ) ) {
            hKeyProfile = NULL;
        } else {
            DUMPREGKEY ( ZONE_INIT, m_sdi.szProfile, hKeyProfile );
        }
        // Read settings from the storage profile and the base profile:
        //
        // Storage Profile ( hKeyProfile ):
        //
        //     HKLM\System\StorageManager\Profiles\<ProfileName>
        //
        // Base Profile ( hKeyStorage ):
        //
        //     HKLM\System\StorageManager\Profiles
        //
        // Load the AutoMount setting.
        if ( !hKeyProfile || !FsdLoadFlag ( hKeyProfile, g_szAUTO_MOUNT_STRING, &m_dwFlags, STORE_ATTRIBUTE_AUTOMOUNT ) ) {
            if ( !FsdLoadFlag ( hKeyStorage, g_szAUTO_MOUNT_STRING, &m_dwFlags, STORE_ATTRIBUTE_AUTOMOUNT ) ) {
                // AutoMount=TRUE is the default if it was not specified in either profile.
                m_dwFlags |= STORE_ATTRIBUTE_AUTOMOUNT;
            }
        }
        // Load the AutoFormat setting.
        if ( !hKeyProfile || !FsdLoadFlag ( hKeyProfile, g_szAUTO_FORMAT_STRING, &m_dwFlags, STORE_ATTRIBUTE_AUTOFORMAT ) ) {
            FsdLoadFlag ( hKeyStorage, g_szAUTO_FORMAT_STRING, &m_dwFlags, STORE_ATTRIBUTE_AUTOFORMAT );
        }
        // Load the AutoPart setting.
        if ( !hKeyProfile || !FsdLoadFlag ( hKeyProfile, g_szAUTO_PART_STRING, &m_dwFlags, STORE_ATTRIBUTE_AUTOPART ) ) {
            FsdLoadFlag ( hKeyStorage, g_szAUTO_PART_STRING, &m_dwFlags, STORE_ATTRIBUTE_AUTOPART );
        }
        // Load the DefaultFileSystem setting.
        if ( !hKeyProfile || !FsdGetRegistryString ( hKeyProfile, g_szFILE_SYSTEM_STRING, m_szDefaultFileSystem, sizeof ( m_szDefaultFileSystem )/sizeof ( WCHAR ) ) ) {
            if ( !FsdGetRegistryString ( hKeyStorage, g_szFILE_SYSTEM_STRING, m_szDefaultFileSystem, sizeof ( m_szDefaultFileSystem )/sizeof ( WCHAR ) ) ) {
                // "FATFS" is the default file system if none was specified in either profile.
                VERIFY ( SUCCEEDED ( StringCchCopy ( m_szDefaultFileSystem, MAX_PATH, g_szDEFAULT_FILESYSTEM ) ) );
            }
        }
        // Load the Folder setting.
        if ( !hKeyProfile || !FsdGetRegistryString ( hKeyProfile, g_szFOLDER_NAME_STRING, m_szFolderName, sizeof ( m_szFolderName )/sizeof ( WCHAR ) ) ) {
            if ( !FsdGetRegistryString ( hKeyStorage, g_szFOLDER_NAME_STRING, m_szFolderName, sizeof ( m_szFolderName )/sizeof ( WCHAR ) ) ) {
                // "Mounted Volume" is the default folder name if none was specified in either profile.
                VERIFY ( SUCCEEDED ( StringCchCopy ( m_szFolderName, FOLDERNAMESIZE, g_szDEFAULT_FOLDER_NAME ) ) );
            }
        }
        // Load the Name setting; this is really the friendly name for th estore.
        if ( !hKeyProfile || !FsdGetRegistryString ( hKeyProfile, g_szSTORE_NAME_STRING, m_szStoreName, sizeof ( m_szStoreName )/sizeof ( WCHAR ) ) ) {
            if ( !FsdGetRegistryString ( hKeyStorage, g_szSTORE_NAME_STRING, m_szStoreName, sizeof ( m_szStoreName )/sizeof ( WCHAR ) ) ) {
                // "External Storage" is the default name if none was specified in either profile.
                VERIFY ( SUCCEEDED ( StringCchCopy ( m_szStoreName, STORENAMESIZE, g_szDEFAULT_STORAGE_NAME ) ) );
            }
        }
        // By default activity timer is enabled.  The Event Strings will specify the name of the activity event.
        if ( !hKeyProfile || !FsdLoadFlag ( hKeyProfile, g_szACTIVITY_TIMER_ENABLE_STRING, &m_dwFlags, STORE_FLAG_ACTIVITYTIMER ) ) {
            if ( !FsdLoadFlag ( hKeyStorage, g_szACTIVITY_TIMER_ENABLE_STRING, &m_dwFlags, STORE_FLAG_ACTIVITYTIMER ) ) {
                m_dwFlags |= STORE_FLAG_ACTIVITYTIMER;
            }
        }
        // Load the ActivityEvent setting.
        if ( !hKeyProfile || !FsdGetRegistryString ( hKeyProfile, g_szACTIVITY_TIMER_STRING, m_szActivityName, sizeof ( m_szActivityName )/sizeof ( WCHAR ) ) ) {
            if ( !FsdGetRegistryString ( hKeyStorage, g_szACTIVITY_TIMER_STRING, m_szActivityName, sizeof ( m_szActivityName )/sizeof ( WCHAR ) ) ) {
                // By default, there is no activity event name and hence no activity event.
                VERIFY ( SUCCEEDED ( StringCchCopy ( m_szActivityName, MAX_PATH, g_szDEFAULT_ACTIVITY_NAME ) ) );
            }
        }

        // Load the DisableOnSuspend setting.
        if ( !hKeyProfile || !FsdLoadFlag ( hKeyProfile, g_szDISABLE_ON_SUSPEND, &m_dwFlags, STORE_FLAG_DISABLE_ON_SUSPEND ) ) {
            FsdLoadFlag ( hKeyStorage, g_szDISABLE_ON_SUSPEND, &m_dwFlags, STORE_FLAG_DISABLE_ON_SUSPEND );
        }

        // Load the MountAsXXX and MountFlags values from the profile key.
        g_pMountTable->GetMountSettings ( hKeyStorage, &m_dwDefaultMountFlags ); // First read from base profile.
        g_pMountTable->GetMountSettings ( hKeyProfile, &m_dwDefaultMountFlags ); // Override values using storage profile.

        // Load the Attrib settings.
        DWORD dwAttribs = 0;
        if ( !hKeyProfile || !FsdGetRegistryValue ( hKeyProfile, g_szATTRIB_STRING, &dwAttribs ) ) {
            FsdGetRegistryValue ( hKeyStorage, g_szATTRIB_STRING, &dwAttribs );
        }

        // We currently only honor the read-only attribute of the store.
        if ( dwAttribs & STORE_ATTRIBUTE_READONLY ) {
            m_dwFlags |= STORE_ATTRIBUTE_READONLY;
        }

    }

    // Determine the name of the partition driver and partition driver dll to use
    // for this storage device.
    if ( !GetPartitionDriver ( hKeyStorage, hKeyProfile ) ) {
        goto ExitFalse;
    }

    // Set-up the LogicalDisk_t registry values so that the partition driver can read
    // its own settings using FSDMGR_GetRegistryXXX functions.
    if ( ERROR_SUCCESS != AddRootRegKey ( m_szRootRegKey ) ||
        ERROR_SUCCESS != SetRegSubKey ( m_szPartDriverName ) ) {
        goto ExitFalse;
    }

    if ( m_sdi.dwDeviceType & STORAGE_DEVICE_TYPE_REMOVABLE_DRIVE ||
        m_sdi.dwDeviceType & STORAGE_DEVICE_TYPE_REMOVABLE_MEDIA ) {
        m_dwFlags |= STORE_ATTRIBUTE_REMOVABLE;
    }

    bRet = TRUE;
ExitFalse:
    if ( hKeyStorage ) {
        FsdRegCloseKey ( hKeyStorage );
    }
    if ( hKeyProfile ) {
        FsdRegCloseKey ( hKeyProfile );
    }
    return bRet;
}


DWORD StoreDisk_t::OpenDisk ( )
{
    DWORD dwError = ERROR_SUCCESS;
    STORAGE_IDENTIFICATION storageid;

    if ( m_pBlockDevice ) {

        m_hDisk = m_pBlockDevice->OpenBlockDevice ( );
        if ( INVALID_HANDLE_VALUE == m_hDisk ) {
            dwError = ::FsdGetLastError ( );
        }

    } else {
        m_hDisk = ::CreateFileW ( m_szDeviceName,
                           GENERIC_READ | GENERIC_WRITE,
                           0,
                           NULL, OPEN_EXISTING, 0, NULL );

        if ( m_hDisk == INVALID_HANDLE_VALUE ) {
            dwError = ::FsdGetLastError ( );
            if ( dwError == ERROR_ACCESS_DENIED ) {
                dwError = ERROR_SUCCESS;
                m_hDisk = ::CreateFileW ( m_szDeviceName,
                                   GENERIC_READ,
                                   FILE_SHARE_READ,
                                   NULL, OPEN_EXISTING, 0, NULL );
                if ( m_hDisk != INVALID_HANDLE_VALUE ) {
                    m_dwFlags |= STORE_ATTRIBUTE_READONLY;
                } else {
                    dwError = ::FsdGetLastError ( );
                }
            }
        }
    }
    if ( INVALID_HANDLE_VALUE != m_hDisk ) {

        DWORD dwRet;
        dwError = DiskIoControl ( DISK_IOCTL_GETINFO, &m_di, sizeof ( DISK_INFO ), NULL, 0, &dwRet, NULL );
        if ( ( dwError == ERROR_BAD_COMMAND ) || ( dwError == ERROR_NOT_SUPPORTED ) ){
            ::memset ( &m_di, 0, sizeof ( DISK_INFO ) );
            dwError = ERROR_SUCCESS;
        }

        if ( dwError == ERROR_SUCCESS ) {

            if ( ERROR_SUCCESS != DiskIoControl ( IOCTL_DISK_DEVICE_INFO, &m_sdi, sizeof ( STORAGEDEVICEINFO ), NULL, 0, &dwRet, NULL ) ) {
                DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!StoreDisk_t::OpenDisk ( 0x%08X ) call to IOCTL_DISK_DEVICE_INFO failed..filling info\r\n", this ) );
                m_sdi.dwDeviceClass = STORAGE_DEVICE_CLASS_BLOCK;
                m_sdi.dwDeviceFlags = STORAGE_DEVICE_FLAG_READWRITE;
                m_sdi.dwDeviceType = STORAGE_DEVICE_TYPE_UNKNOWN;

                // In the case that a disk does not respond to IOCTL_DISK_DEVICE_INFO, use
                // the profile name "Default"
                VERIFY ( SUCCEEDED ( ::StringCchCopy ( m_sdi.szProfile, PROFILENAMESIZE, L"Default" ) ) );
            }
        }

        DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!StoreDisk_t::OpenDisk ( 0x%08X ) DeviceInfo Class ( 0x%08X ) Flags ( 0x%08X ) Type ( 0x%08X ) Profile ( %s )\r\n",
            this,
            m_sdi.dwDeviceClass,
            m_sdi.dwDeviceFlags,
            m_sdi.dwDeviceType,
            m_sdi.szProfile ) );

        SetLastError ( ERROR_SUCCESS );
        storageid.dwSize = 0;
        // Get the storage ID.  The returned error should be ERROR_INSUFFICIENT_BUFFER.
        // Allocate enough space for the ID.  If storageid.dwSize is 0,
        // then the call failed and should not be attempted a second time.
        if ( ( ERROR_INSUFFICIENT_BUFFER == DiskIoControl ( IOCTL_DISK_GET_STORAGEID, NULL, 0, &storageid, sizeof ( STORAGE_IDENTIFICATION ), &dwRet, NULL ) )&& ( storageid.dwSize != 0 ) ){
            m_pStorageId = new BYTE[storageid.dwSize];
            if ( m_pStorageId ) {
                DiskIoControl ( IOCTL_DISK_GET_STORAGEID, NULL, 0, m_pStorageId, storageid.dwSize, &dwRet, NULL );
            }
        }

        if ( ERROR_SUCCESS == dwError ) {
            //hStore -> hDisk conversion handle
            StoreDisk_t **ppStore = new StoreDisk_t*;
            if ( ppStore ) {
                *ppStore = this;
                m_hStoreDisk = CreateAPIHandle ( g_hStoreDiskApi, ( LPVOID ) ppStore ); 
                if ( m_hStoreDisk ) {
                    AddRef ( );
                }else {
                    m_hStoreDisk = INVALID_HANDLE_VALUE;
                }
            } else { //Cant proceed without hStoreDisk handle.
                dwError = ERROR_NOT_ENOUGH_MEMORY;
                SetLastError ( dwError );
            }
        }
    }
    return dwError;
}

HANDLE StoreDisk_t::CreateHandle ( HANDLE hProc, DWORD dwAccess )
{
    HANDLE hStore = INVALID_HANDLE_VALUE;
    STOREHANDLE *pStoreHandle = new StoreHandle_t ( hProc,dwAccess,0,STORE_HANDLE_SIG );

    if ( pStoreHandle ) {
        pStoreHandle->m_pStore = this;
        hStore = CreateAPIHandleWithAccess ( g_hStoreApi, pStoreHandle, dwAccess, GetCurrentProcess ( ) );

        if ( hStore ) {
            AddRef ( );
        }else {
            hStore = INVALID_HANDLE_VALUE;
            delete pStoreHandle;
            pStoreHandle = NULL;
        }
    }
    return hStore;
}


void StoreDisk_t::AddPartition ( PartitionDisk_t *pPartition )
{
    DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "AddPartition: Partition %x ( Ref= %d ) Added to Store %x ( Ref= %d ) \n" ),pPartition,pPartition->RefCount ( ), this,m_RefCount ) );
    AddToDListTail ( &m_Partitions.m_link,&pPartition->m_link );
}

DWORD StoreDisk_t::DeletePartition ( PartitionDisk_t * pPartition )
{
    ASSERT (OWN_CS (&m_cs));
    DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "DeletePartition: Partition %x ( Ref= %d ) deleted from Store %x ( Ref= %d ) \n" ),pPartition,pPartition->RefCount ( ), this,m_RefCount ) );
    RemoveDList ( &pPartition->m_link );
    pPartition->Release ( );
    return ERROR_SUCCESS;
}

static BOOL EnumMatchPartitionName ( PDLIST pItem, LPVOID pEnumData )
{
    PartitionDisk_t* pPartition = GetPartitionFromDList ( pItem );
    WCHAR* pPartitionName = ( WCHAR* ) pEnumData;
    ASSERT (pPartitionName);
    return ( _wcsicmp ( pPartition->GetName ( ), pPartitionName ) == 0 );
}

PartitionDisk_t *StoreDisk_t::FindPartition ( LPCTSTR szPartitionName )
{
    ASSERT (OWN_CS (&m_cs));
    PartitionDisk_t *pPartition = NULL;
    DLIST* pdl=EnumerateDList ( &m_Partitions.m_link, EnumMatchPartitionName, ( LPVOID ) szPartitionName );
    if ( pdl ) {
        pPartition = GetPartitionFromDList ( pdl );
    }

    return pPartition;
}

void StoreDisk_t::GetPartitionCount ( )
{
    DWORD dwSearchId;
    m_dwPartCount = 0;
    if ( ERROR_SUCCESS == m_pPartDriver->FindPartitionStart ( m_dwStoreId, &dwSearchId ) ) {
        PD_PARTINFO pi;
        while ( ERROR_SUCCESS == m_pPartDriver->FindPartitionNext ( dwSearchId, &pi ) ) {
            m_dwPartCount++;
        }
        m_pPartDriver->FindPartitionClose ( dwSearchId );
    }
}



BOOL StoreDisk_t::LoadPartition ( LPCTSTR szPartitionName, BOOL bMount, BOOL bFormat )
{
    DWORD dwPartitionId;
    BOOL bRet=FALSE;

    if ( ERROR_SUCCESS == m_pPartDriver->OpenPartition ( m_dwStoreId, szPartitionName, &dwPartitionId ) ) {

        PartitionDisk_t *pPartition = new PartitionDisk_t ( this, m_dwStoreId, dwPartitionId, m_pPartDriver, m_szFolderName, m_pBlockDevice );
        if ( !pPartition ) {
            m_pPartDriver->ClosePartition ( dwPartitionId );
        }else {
            if ( !pPartition->Init ( szPartitionName, m_szRootRegKey ) ) {
                m_pPartDriver->ClosePartition ( dwPartitionId );
                pPartition->Release ( );
                pPartition = NULL;
            }else {

                //Preserve the "To be Formated" information in the partition
                //This is required since otherwise we will miss formating if the parition is 
                //not being mounting now (i.e when bMount == FALSE).
                if ( bFormat ) {
                   pPartition->SetFormatOnMount ( TRUE );
                }

                // Attempt to mount the partition if the store is being mounted ( bMount == TRUE )
                // and auto-mount is set in the registry profile.
                if ( bMount && ( m_dwFlags & STORE_ATTRIBUTE_AUTOMOUNT ) ) {

                    HANDLE hPartition = INVALID_HANDLE_VALUE;
                    // Open a private handle to the partition and mount it.
                    HANDLE hProc = reinterpret_cast<HANDLE> ( GetCurrentProcessId ( ) );

                    //Open a Local Partition Handle
                    if ( ERROR_SUCCESS == pPartition->CreateHandle ( &hPartition, hProc,FILE_ALL_ACCESS ) ) {

                        DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!StoreDisk_t::LoadPartition: mounting partition "
                            L"\"%s\" on store \"%s\"", pPartition->GetName ( ), m_szDeviceName ) ); 

                        // Try to mount the partition. If this fails for some reason, we
                        // intentionally still succeed the LoadPartition call.
                        if ( pPartition->Mount ( hPartition ) ) {
                            m_dwMountCount++;
                        } else {
        #ifdef UNDER_CE
                            ::CloseHandle ( hPartition );
        #else
                            ::STG_CloseHandle ( reinterpret_cast<PSTOREHANDLE> ( hPartition ) );
        #endif
                        }
                    } 
                }
                //Add Parition to the list; Partition in the list need not be in mounted state.
                AddPartition ( pPartition );
                bRet = TRUE;

            }//Partition->Init
        } //partition
    }//Open Partition
    return bRet;
}

void StoreDisk_t::LoadExistingPartitions ( BOOL bMount, BOOL bFormat )
{
    DWORD dwSearchId;
    if ( ERROR_SUCCESS == m_pPartDriver->FindPartitionStart ( m_dwStoreId, &dwSearchId ) ) {
        PD_PARTINFO pi;
        while ( ERROR_SUCCESS == m_pPartDriver->FindPartitionNext ( dwSearchId, &pi ) ) {
            DEBUGMSG ( ZONE_INIT, ( L"Partition %s  NumSectors=%ld\r\n", pi.szPartitionName,  ( DWORD )pi.snNumSectors ) );
            LoadPartition ( pi.szPartitionName, bMount, bFormat );
        }
        m_pPartDriver->FindPartitionClose ( dwSearchId );
    }
}


DWORD StoreDisk_t::MountPartitions ( HANDLE hStore, BOOL bMount )
{
    DWORD dwError = ERROR_GEN_FAILURE;
    BOOL bFormat = FALSE;
    ASSERT ( !m_pPartDriver );

    m_pPartDriver = new PartitionDriver_t ( );
    if ( !m_pPartDriver ) {
        return ERROR_OUTOFMEMORY;
    }

    dwError = m_pPartDriver->LoadPartitionDriver ( m_szPartDriver );
    if ( ERROR_SUCCESS == dwError ) {
#ifdef UNDER_CE
        //For CE Pass hStore -> hDisk handle to the Partition Driver
        dwError = m_pPartDriver->OpenStore ( m_hStoreDisk, &m_dwStoreId );
#else
        // Pass the store handle rather than the disk handle to the partition driver
        // so it will trap back into STG_DeviceIoControl when calling DeviceIoControl.
        dwError = m_pPartDriver->OpenStore ( hStore, &m_dwStoreId );
#endif
        if ( ERROR_DEVICE_NOT_PARTITIONED == dwError ) {
            DeletePartDriver();
            m_szPartDriver[0] = '\0';
            m_pPartDriver = new PartitionDriver_t ( );
            if ( !m_pPartDriver ) {
                return ERROR_OUTOFMEMORY;
            }
            dwError = m_pPartDriver->LoadPartitionDriver ( m_szPartDriver );
            if ( ERROR_SUCCESS == dwError ) {
                //For CE Pass hStore -> hDisk handle to the Partition Driver
                dwError = m_pPartDriver->OpenStore ( m_hStoreDisk, &m_dwStoreId );
            }
        }
        if ( ERROR_SUCCESS == dwError ) {
            if ( NULL == m_hActivityEvent ) {
                // Activity Timer Setup
                if ( ( STORE_FLAG_ACTIVITYTIMER & m_dwFlags ) && m_szActivityName[0] ) {
                    m_hActivityEvent = CreateEvent ( NULL, FALSE, FALSE, m_szActivityName );
                    // there is really nothing we can do if event creation fails
                    DEBUGCHK ( m_hActivityEvent );
                }
            }
            DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!StoreDisk_t::Mount: Opened the store \"%s\" hStore=0x%08X\r\n",
                m_szDeviceName, m_dwStoreId ) );
            m_si.cbSize = sizeof ( PD_STOREINFO );
            m_pPartDriver->GetStoreInfo ( m_dwStoreId, &m_si );
            DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!StoreDisk_t::Mount: Geometry: NumSec=%ld BytesPerSec=%ld FreeSec=%ld BiggestCreatable=%ld\r\n",
                ( DWORD )m_si.snNumSectors, ( DWORD )m_si.dwBytesPerSector, ( DWORD )m_si.snFreeSectors,
                ( DWORD )m_si.snBiggestPartCreatable ) );
            if ( ERROR_SUCCESS != m_pPartDriver->IsStoreFormatted ( m_dwStoreId ) ) {
                if ( m_dwFlags & STORE_ATTRIBUTE_AUTOFORMAT ) {
                    m_pPartDriver->FormatStore ( m_dwStoreId );
                    m_pPartDriver->GetStoreInfo ( m_dwStoreId, &m_si );
                }
            }
            if ( ERROR_SUCCESS == m_pPartDriver->IsStoreFormatted ( m_dwStoreId ) ) { // Retry again if we formatted it
                GetPartitionCount ( );
                if ( !m_dwPartCount ) {
                    if ( m_dwFlags & STORE_ATTRIBUTE_AUTOPART ) {
                        PD_STOREINFO si;
                        si.cbSize = sizeof ( PD_STOREINFO );
                        m_pPartDriver->GetStoreInfo ( m_dwStoreId, &si );
                        if ( ( ERROR_SUCCESS == m_pPartDriver->CreatePartition ( m_dwStoreId, L"PART00", 0, si.snBiggestPartCreatable, TRUE ) ) &&
                             ( ERROR_SUCCESS == m_pPartDriver->FormatPartition ( m_dwStoreId, L"PART00", 0, TRUE ) ) ) {
                                m_pPartDriver->GetStoreInfo ( m_dwStoreId, &m_si );
                                DEBUGMSG ( 1, ( L"FSDMGR!StoreDisk_t::Mount: Geometry: NumSec=%ld BytesPerSec=%ld FreeSec=%ld BiggestCreatable=%ld\r\n",
                                    ( DWORD )m_si.snNumSectors, ( DWORD )m_si.dwBytesPerSector,
                                    ( DWORD )m_si.snFreeSectors, ( DWORD )m_si.snBiggestPartCreatable ) );
                                m_dwPartCount = 1;
                                bFormat = TRUE;
                        }
                    }
                }
                LoadExistingPartitions ( bMount, bFormat );
            }
        }
    }

    return dwError;
}
DWORD StoreDisk_t::Mount ( HANDLE hStore, BOOL bMount )
{
    DWORD dwError = ERROR_GEN_FAILURE;

    if ( InitStoreSettings ( ) ) {

        dwError= MountPartitions ( hStore,bMount );
        if ( ERROR_SUCCESS == dwError ) {
             if ( m_dwMountCount && m_pBlockDevice ) {
                TCHAR strShortDeviceName[DEVICENAMESIZE] = {0};

                VERIFY ( GetShortDeviceName ( m_szDeviceName, strShortDeviceName, _countof ( strShortDeviceName ) ) );
                m_pBlockDevice->AdvertiseInterface ( strShortDeviceName, TRUE );
            }

            // Preserve the handle to this store.
            m_hStore = hStore;

            //Mount is Completed;
            SetMounted ( );
        }
    }
    return dwError;
}

void StoreDisk_t::Disable ( )
{
    ASSERT (OWN_CS (&m_cs));

    DLIST* ptrav=NULL;
    PartitionDisk_t * pPartition = NULL;

    for ( ptrav = m_Partitions.m_link.pFwd; &m_Partitions.m_link != ptrav; ptrav = ptrav->pFwd ) {
        pPartition = GetPartitionFromDList ( ptrav );
        // Disable the volume associate with this partition.
        // It will be reenabled if the disk comes back online.
        pPartition->Disable ( );
    }
}

void StoreDisk_t::Enable ( )
{
    ASSERT (OWN_CS (&m_cs));

    DLIST* ptrav=NULL;
    PartitionDisk_t * pPartition = NULL;

    for ( ptrav = m_Partitions.m_link.pFwd; &m_Partitions.m_link != ptrav; ptrav = ptrav->pFwd ) {
        pPartition = GetPartitionFromDList ( ptrav );
        // Enable the volume associated with this partition.
        pPartition->Enable ( );
    }
}

BOOL StoreDisk_t::Unmount ( )
{
    BOOL bRet = FALSE;
    TCHAR strShortDeviceName[DEVICENAMESIZE] = {0};

    Lock ( );
    VERIFY ( GetShortDeviceName ( m_szDeviceName, strShortDeviceName, _countof ( strShortDeviceName ) ) );

    //After successful unmount the store no longer exists in the list
    bRet = UnmountPartitions ( TRUE );
    if ( bRet ) {
        FSDMGR_AdvertiseInterface ( &STORE_MOUNT_GUID, strShortDeviceName, FALSE );

        if ( m_pBlockDevice ) {
#ifndef UNDER_CE
            if ( ( INVALID_HANDLE_VALUE != m_hDisk ) && m_hDisk ) {
                ::FS_DevCloseFileHandle ( reinterpret_cast<BlockDevice_t*> ( m_hDisk ) );
                m_hDisk = INVALID_HANDLE_VALUE;
            }
#endif
        }

        if ( INVALID_HANDLE_VALUE != m_hDisk ) {
            ::CloseHandle ( m_hDisk );
            m_hDisk = INVALID_HANDLE_VALUE;
        }

        //Close hStoreDisk
        if ( INVALID_HANDLE_VALUE != m_hStoreDisk ) {
#ifdef UNDER_CE
            ::CloseHandle ( m_hStoreDisk );
#else
            CONV_DevCloseFileHandle (reinterpret_cast<PSTORE*> ( m_hStoreDisk ) );
#endif
            m_hStoreDisk = INVALID_HANDLE_VALUE;
        }

        if ( INVALID_HANDLE_VALUE != m_hStore ) {
#ifdef UNDER_CE
            ::CloseHandle ( m_hStore );
#else
            ::STG_CloseHandle ( reinterpret_cast<PSTOREHANDLE> ( m_hStore ) );
#endif
            m_hStore = INVALID_HANDLE_VALUE;
        }

        //Mark this Store as unmounted;
        SetDetachCompleted ( );
    }

    Unlock ( );
    return bRet;
}

BOOL StoreDisk_t::UnmountPartitions ( BOOL bDelete )
{
    ASSERT (OWN_CS (&m_cs));
    DLIST* ptrav=NULL;
    PartitionDisk_t * pPartition = NULL;

    for ( ptrav = m_Partitions.m_link.pFwd; &m_Partitions.m_link != ptrav; ) {
        pPartition = GetPartitionFromDList ( ptrav );

        //Advance to the next element ( before the element is potentially deleted below )
        ptrav = ptrav->pFwd;

        if ( pPartition->IsMounted ( ) ) {
            if ( pPartition->Unmount ( ) ) {
                m_dwMountCount--;
            } else {
                continue;
            }
        } 
        if ( bDelete ) {
            VERIFY ( DeletePartition ( pPartition ) == ERROR_SUCCESS );
        }
    }

    if ( !m_dwMountCount && m_pBlockDevice ) {
        TCHAR strShortDeviceName[DEVICENAMESIZE] = {0};

        VERIFY ( GetShortDeviceName ( m_szDeviceName, strShortDeviceName, _countof ( strShortDeviceName ) ) );
        m_pBlockDevice->AdvertiseInterface ( strShortDeviceName, FALSE );
    }

    return ( m_dwMountCount == 0 );
}

DWORD StoreDisk_t::Format ( )
{
    DWORD dwError = ERROR_SUCCESS;
    Lock ( );
    if ( IsDetached ( ) ) {
       dwError = ERROR_DEVICE_NOT_AVAILABLE;
    }else {
        if ( m_dwMountCount ) {
            dwError = ERROR_DEVICE_IN_USE;
        } else if ( m_dwFlags & STORE_ATTRIBUTE_READONLY ) {
            dwError = ERROR_ACCESS_DENIED;
        } else {
            dwError = m_pPartDriver->FormatStore ( m_dwStoreId );
            if ( ERROR_SUCCESS == dwError ) {
                if ( IsMounted ( ) ) {
                    UnmountPartitions ( TRUE );
                }

                m_si.cbSize = sizeof ( PD_STOREINFO );
                m_pPartDriver->GetStoreInfo ( m_dwStoreId, &m_si );
                
                GetPartitionCount ( );

                if ( m_dwPartCount > 0 ) {
                    // m_dwPartCount > 0 after a Format Store implies this is a 
                    // super floppy store which is not partitionable and managed by Nullpart
                    // Nullpart always returns partition count as 1, so load it now so that the
                    // caller and enumerate and mount it as needed.
                    LoadExistingPartitions ( FALSE, FALSE );
                    
                    // stgui will know Format Store has failed to remove all the partitions
                    // and will display an appropriate error msg
                    dwError = ERROR_NOT_SUPPORTED;
                }
            }
        }
    }

    Unlock ( );

    return dwError;
}

DWORD StoreDisk_t::CreatePartition ( LPCTSTR szPartitionName, BYTE bPartType, SECTORNUM snNumSectors, BOOL bAuto )
{
    DWORD dwError = ERROR_SUCCESS;
    Lock ( );

    if ( IsDetached ( ) ) {
       dwError = ERROR_DEVICE_NOT_AVAILABLE;
    }else {
        if ( snNumSectors == 0 ) {
            snNumSectors = m_si.snBiggestPartCreatable;
        }
        dwError = m_pPartDriver->CreatePartition ( m_dwStoreId, szPartitionName, bPartType, snNumSectors, bAuto );
        if ( ERROR_SUCCESS == dwError ) {
            m_pPartDriver->GetStoreInfo ( m_dwStoreId, &m_si );
            // Load up the new partition
            LoadPartition ( szPartitionName, TRUE, TRUE );
            m_dwPartCount++;
        }
    }

    Unlock ( );
    return dwError;
}

DWORD StoreDisk_t::DeletePartition ( LPCTSTR szPartitionName )
{
    DWORD dwError = ERROR_SUCCESS;

    Lock ( );

    if ( IsDetached ( ) ) {
       dwError = ERROR_DEVICE_NOT_AVAILABLE;
    }else {
        PartitionDisk_t *pPartition = FindPartition ( szPartitionName );
        if ( pPartition ) {
            if ( pPartition->IsMounted ( ) ) {
               dwError = ERROR_DEVICE_IN_USE;
            } else {
               dwError = m_pPartDriver->DeletePartition ( m_dwStoreId, szPartitionName );
               if ( ERROR_SUCCESS == dwError ) {
                   dwError = DeletePartition ( pPartition );
                   m_pPartDriver->GetStoreInfo ( m_dwStoreId, &m_si );
                   m_dwPartCount--;
               }
            }
        }
    }

    Unlock ( );
    return dwError;
}

DWORD StoreDisk_t::OpenPartitionEx ( LPCTSTR szPartitionName, HANDLE *phHandle, HANDLE hProc, DWORD dwAccess )
{
    DWORD dwError = ERROR_FILE_NOT_FOUND;
    *phHandle = INVALID_HANDLE_VALUE;
    Lock ( );

    if ( IsDetached ( ) ) {
       dwError = ERROR_DEVICE_NOT_AVAILABLE;
    }else {

        PartitionDisk_t * pPartition = FindPartition ( szPartitionName );
        if ( pPartition ) {
            dwError = pPartition->CreateHandle ( phHandle, hProc, dwAccess );
        }
    }

    Unlock ( );
    return dwError;
}

DWORD StoreDisk_t::MountPartition ( PartitionDisk_t * pPartition )
{
    HANDLE hPartition;
    DWORD dwError = ERROR_SUCCESS;

    if ( pPartition->IsMounted ( ) ) {
        dwError = ERROR_ALREADY_EXISTS;
    } else {

        // Open a private handle to the partition.
        HANDLE hProc = reinterpret_cast<HANDLE> ( GetCurrentProcessId ( ) );
        dwError = pPartition->CreateHandle ( &hPartition, hProc, FILE_ALL_ACCESS );
        if ( ERROR_SUCCESS == dwError ) {

            DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!StoreDisk_t::MountPartition: mounting partition "
                L"\"%s\" on store \"%s\"", pPartition->GetName ( ), m_szDeviceName ) );

            // Mount the partition.
            if ( pPartition->Mount ( hPartition ) ) {
                m_dwMountCount++;
            } else {
                dwError = ::FsdGetLastError ( );
#ifdef UNDER_CE
                ::CloseHandle ( hPartition ); 
#else
                ::STG_CloseHandle ( reinterpret_cast<PSTOREHANDLE> ( hPartition ) );
#endif
            }
        }
    }

    return dwError;
}

DWORD StoreDisk_t::UnmountPartition ( PartitionDisk_t * pPartition )
{
    DWORD dwError = ERROR_SUCCESS;
    Lock ( );
    if ( !pPartition->IsMounted ( ) ) {
        dwError = ERROR_FILE_NOT_FOUND;
    } else {
        DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!StoreDisk_t::UnmountPartition: unmounting partition "
            L"\"%s\" on store \"%s\"", pPartition->GetName ( ), m_szDeviceName ) );

        if ( pPartition->Unmount ( ) ) {
            m_dwMountCount--;
        } else {
            dwError = ERROR_GEN_FAILURE;
        }
    }
    Unlock ( );
    return dwError;
}


void StoreDisk_t::MediaDetachFromPartition ( PartitionDisk_t* pPartition )
{
    DEBUGCHK ( !pPartition->IsMediaDetached ( ) );
    ASSERT (OWN_CS (&m_cs));

    MountedVolume_t* pVolume = pPartition->GetVolume ( );
    if ( pVolume ) {
        // Notify the volume that the media has been detached. The FSD will
        // likely just ignore this message, and we don't care how it responds.
        pVolume->MediaChangeEvent ( 
            StorageMediaChangeEventDetached,
            NULL
            );
    }

    // Add the "media detached" partition flag so we don't try to detach
    // again.
    pPartition->SetMediaDetached ( );
}

void StoreDisk_t::MediaDetachFromStore ( )
{
    ASSERT (OWN_CS (&m_cs));

    if ( IsMediaDetached ( ) ) {
        DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "MediaDetachFromStore: Skipping Media Detach On Store %x ( Ref= %d ). Already Detached \n" ),this ,RefCount ( ) ) );
        // Already detached.
        return;
    }

    // Disable the store so that it cannot be accessed any more.
    Disable ( );

    // Notify the volume that the media has been detached. The partition driver
    // will likely just ignore this message, and we don't care how it responds.
    m_pPartDriver->MediaChangeEvent ( 
            m_dwStoreId,
            StorageMediaChangeEventDetached,
            NULL );

    DLIST* ptrav=NULL;
    PartitionDisk_t * pPartition = NULL;

    for ( ptrav = m_Partitions.m_link.pFwd; &m_Partitions.m_link != ptrav; ptrav = ptrav->pFwd ) {
        pPartition = GetPartitionFromDList ( ptrav );
        // Disable the volume associate with this partition.
        // It will be reenabled if the disk comes back online.
        MediaDetachFromPartition ( pPartition );
    }

    // Mark the store with the "media detached" flag.
    SetMediaDetached ( );
}

void StoreDisk_t::MediaAttachToPartition ( PartitionDisk_t* pPartition )
{
    ASSERT (OWN_CS (&m_cs));

    MountedVolume_t* pVolume = pPartition->GetVolume ( );
    if ( pVolume ) {

#ifdef DEBUG
        WCHAR VolumeName[32] = {0};
        FSDMGR_GetVolumeName ( pVolume->GetVolumeHandle ( ), VolumeName, 32 );
        DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!StoreDisk_t::MediaAttachToPartition: attaching media to \"%s\"\r\n", VolumeName ) );
#endif
        // The FSD should report back either "changed" or "unchanged."
        STORAGE_MEDIA_ATTACH_RESULT Result = StorageMediaAttachResultUnchanged;

        // Notify the volume that the media has been attached. The FSD will
        // report back status on whether or not it recognizes the media as
        // the same media that was previously detached.
        if ( ( ERROR_SUCCESS == pVolume->MediaChangeEvent ( 
                StorageMediaChangeEventAttached, &Result ) ) &&
            ( StorageMediaAttachResultChanged == Result ) ) {
#ifdef DEBUG
            DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!StoreDisk_t::MediaAttachToPartition: media at \"%s\" changed! refreshing...\r\n", VolumeName ) );
#endif
            // The FSD has indicated that the volume has changed, so we
            // need to refresh it. We do this by simply unmounting and re-
            // mounting this partition, which will force the FSD to unload
            // and re-load.
            DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "MediaAttachToStore: Volume::Media Changed. UnMount-Mount Partition %x ( Ref= %d ) \n" ),pPartition,pPartition->RefCount ( ) ) );
            if ( ERROR_SUCCESS == UnmountPartition ( pPartition ) ) {
                // This may fail.
                MountPartition ( pPartition );
            }
        }
        // else, the FSD did not recognise the event or indicated that the media
        // has not changed. In this case, just keep the volume mounted.
    } else { // !pVolume
        // There is currently no volume associated with this partition, so try
        // to mount it.
        DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "MediaAttachToStore: Mounting Partition %x ( Ref= %d ) \n" ),pPartition,pPartition->RefCount ( ) ) );
        MountPartition ( pPartition );
    }

    // Clear the "media detached" partition flag if it is set.
    pPartition->SetMediaAttached ( );
}

void StoreDisk_t::MediaAttachToStore ( )
{
    ASSERT (OWN_CS (&m_cs));

    // The partition driver should report back either "changed" or "unchanged."
    STORAGE_MEDIA_ATTACH_RESULT Result = StorageMediaAttachResultUnchanged;

    // Query the partition driver to determine if its view of the underlying
    // media has changed ( e.g. geometry, partition count, etc ). It is possible
    // the same media was just reinserted, and, if so, we don't need to bother
    // tearing down and reconstructing partition information.
    if ( NO_ERROR == m_pPartDriver->MediaChangeEvent ( 
            m_dwStoreId,
            StorageMediaChangeEventAttached,
            &Result
            ) &&
        ( StorageMediaAttachResultUnchanged == Result ) ) {
        // The partition driver indicates that its view of the media has not
        // changed, so enumerate and re-attach each partition individually.
        DLIST* ptrav=NULL;
        PartitionDisk_t * pPartition = NULL;

        DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "MediaAttachToStore: PD:: No Change in Media %x ( Ref= %d ) \n" ),this,m_RefCount ) );

        for ( ptrav = m_Partitions.m_link.pFwd; &m_Partitions.m_link != ptrav; ptrav = ptrav->pFwd ) {
            pPartition = GetPartitionFromDList ( ptrav );
            MediaAttachToPartition ( pPartition );
        }

        // Re-enable the store. Partitions that were refreshed are already
        // enabled and will be skipped.
        Enable ( );
    } else {
        // If the partition driver fails the call or indicates that the media
        // has changed, then refresh the store. This will re-load the partition
        // driver and re-enumerate and mount all partitions. The store remains
        // disabled until it is refreshed.
        DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "MediaAttachToStore: PD::Change in Media %x ( Ref= %d ). REFRESHING Partitions \n" ),this,m_RefCount ) );
        
        if ( UnmountPartitions (TRUE) ) {

            // Delete the partition driver;
            // MountPartition would load the correct partition driver.
            DeletePartDriver();

            MountPartitions (m_hStore,TRUE);
            Enable ( );
        } else {
            DEBUGMSG ( ZONE_ERRORS, ( TEXT ( "MediaAttachToStore: PD::Change in Media %x ( Ref= %d ) REFRESHING Partition failed  \n" ),this,m_RefCount ) );
        }
    }

    // Clear the "media detached" flag ( s ) if they're set.
    ClearMediaDetached ( );
    ClearMediaDetachCompleted ( );

}

DWORD StoreDisk_t::RenamePartition ( PartitionDisk_t * pPartition, LPCTSTR szNewName )
{
    DWORD dwError = ERROR_SUCCESS;
    Lock ( );
    if ( IsDetached ( ) ) 
    {
       dwError = ERROR_DEVICE_NOT_AVAILABLE;
    } else {
        if ( !pPartition->RenamePartition ( szNewName ) ) {
            GetDriverError ( &dwError );
        }
    }
    Unlock ( );
    return dwError;
}



DWORD StoreDisk_t::SetPartitionAttributes ( PartitionDisk_t * pPartition, DWORD dwAttrs )
{
    DWORD dwError = ERROR_SUCCESS;

    Lock ( );

    if ( IsDetached ( ) ) {
       dwError = ERROR_DEVICE_NOT_AVAILABLE;
    } else {
        if ( !pPartition->SetPartitionAttributes ( dwAttrs ) ) {
            GetDriverError ( &dwError );
        }
    }

    Unlock ( );

    return dwError;
}

DWORD StoreDisk_t::SetPartitionSize ( PartitionDisk_t *pPartition, SECTORNUM snNumSectors )
{
    ASSERT (OWN_CS (&m_cs));
    DWORD dwError = ERROR_SUCCESS;

    if ( !pPartition->SetPartitionSize ( snNumSectors ) ) {
        GetDriverError ( &dwError );
    } else {
        m_pPartDriver->GetStoreInfo ( m_dwStoreId, &m_si );
    }

    return dwError;
}

DWORD StoreDisk_t::GetPartitionInfo ( PartitionDisk_t * pPartition, PPARTINFO pInfo )
{
    DWORD dwError = ERROR_SUCCESS;

    if ( !pPartition->GetPartitionInfo ( pInfo ) ) {
        dwError = ERROR_BAD_ARGUMENTS;
    }

    return dwError;
}


DWORD StoreDisk_t::FormatPartition ( PartitionDisk_t *pPartition, BYTE bPartType, BOOL bAuto )
{
    DWORD dwError = ERROR_SUCCESS;

    Lock ( );

    if ( IsDetached ( ) )  {
       dwError = ERROR_DEVICE_NOT_AVAILABLE;
    } else {
        if ( !pPartition->FormatPartition ( bPartType, bAuto ) ) {
            GetDriverError ( &dwError );
        }
    }

    Unlock ( );
    return dwError;
}

DWORD StoreDisk_t::FindFirstPartition ( PPARTINFO pInfo, HANDLE * pHandle, HANDLE hProc )
{
    DWORD dwError = ERROR_SUCCESS;
    *pHandle = INVALID_HANDLE_VALUE;

    Lock ( );
    if ( IsDetached ( ) ) {
       dwError = ERROR_DEVICE_NOT_AVAILABLE;
    } else {
        SEARCHPARTITION*pSearch = new StoreHandle_t ( hProc,0,STOREHANDLE_TYPE_SEARCH,0 );
        if ( pSearch ) {

            DLIST* ptrav=NULL;
            PartitionDisk_t * pPartition = NULL;

            for ( ptrav = m_Partitions.m_link.pFwd; &m_Partitions.m_link != ptrav; ptrav = ptrav->pFwd ) {
                pPartition = GetPartitionFromDList ( ptrav );
                PartStoreInfo  *pStorePartInfo = new PartStoreInfo;
                if ( pStorePartInfo ) {
                    pPartition->GetPartitionInfo ( &pStorePartInfo->u.partInfo );
                    pStorePartInfo->u.partInfo.cbSize = sizeof ( PARTINFO );
                    AddToDListTail ( &pSearch->m_PartStoreInfos.m_link,&pStorePartInfo->m_link );
                }
            }

            if ( !IsDListEmpty ( &pSearch->m_PartStoreInfos.m_link ) ) {

                PartStoreInfo  *pStorePartInfo = ( PartStoreInfo * ) pSearch->m_PartStoreInfos.m_link.pFwd;
                RemoveDList ( &pStorePartInfo->m_link );
                if ( CeSafeCopyMemory ( pInfo, &pStorePartInfo->u.partInfo, sizeof ( PARTINFO ) ) ) {
                   *pHandle = CreateAPIHandle ( g_hFindPartApi, pSearch );
                    if ( !(*pHandle) ) {
                        DestroyDList ( &pSearch->m_PartStoreInfos.m_link, EnumSnapshotDelete, NULL );
                        delete pSearch;
                        pSearch = NULL;
                        *pHandle = INVALID_HANDLE_VALUE;
                        dwError = ERROR_GEN_FAILURE;
                    }
                } else {
                    delete pSearch;
                    pSearch = NULL;
                    dwError = ERROR_BAD_ARGUMENTS;
                }

                delete pStorePartInfo;
            } else {
                delete pSearch;
                pSearch = NULL;
                dwError = ERROR_NO_MORE_ITEMS;
            }
        } else { //!search
            dwError = ERROR_OUTOFMEMORY;
        }
    }

    Unlock ( );
    return dwError;
}

DWORD StoreDisk_t::GetStorageId ( PVOID pOutBuf, DWORD nOutBufSize, DWORD *pBytesReturned )
{
    DWORD dwError = ERROR_SUCCESS;
    DWORD dwSize;
    if ( NULL == m_pStorageId ) {
        dwError = ERROR_NOT_SUPPORTED;
    } else {
        dwSize = ( ( STORAGE_IDENTIFICATION * )m_pStorageId )->dwSize;
        if ( nOutBufSize >= dwSize ) {
            if ( !CeSafeCopyMemory ( pOutBuf, m_pStorageId, dwSize ) ) {
                dwError = ERROR_INVALID_PARAMETER;
            }else if ( pBytesReturned && !CeSafeCopyMemory ( pBytesReturned, &dwSize, sizeof ( DWORD ) ) ) {
                dwError = ERROR_INVALID_PARAMETER;
            }
        } else {
            dwError = ERROR_INSUFFICIENT_BUFFER;
             // copy the size of the ID
            if ( !CeSafeCopyMemory ( & ( ( STORAGE_IDENTIFICATION * )pOutBuf )->dwSize, &dwSize, sizeof ( DWORD ) ) ) {
                dwError = ERROR_INVALID_PARAMETER;
            }
        }
    }
    return dwError;
}

DWORD StoreDisk_t::GetDeviceInfo ( STORAGEDEVICEINFO *pDeviceInfo, DWORD *pBytesReturned )
{
    DWORD dwError = ERROR_SUCCESS;
    DWORD dwSize = sizeof ( STORAGEDEVICEINFO );

    if ( !CeSafeCopyMemory ( pDeviceInfo, &m_sdi, dwSize ) ||
        ( pBytesReturned && !CeSafeCopyMemory ( pBytesReturned, &dwSize, sizeof ( DWORD ) ) ) ) {
        dwError = ERROR_INVALID_PARAMETER;
    }

    return dwError;
}

// SECURITY NOTE: Invoked by 
// 1. STG_DeviceIoControl which is responsible for verifying that the caller has permission to perform the IOCTL operation.
// 2. Short circuit route if Partition is not Mounted in response to FSDMGR_DiskIoControl invoked by FSD drivers.
DWORD StoreDisk_t::DeviceIoControl ( PartitionDisk_t *pPartition, DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned ) 
{
    LRESULT lResult = ERROR_SUCCESS;

    // Allow a few IOCTLs that retrieve only cached info and behave the same regardless of
    // handle type ( store or partition ).
    if ( IOCTL_DISK_DEVICE_INFO == dwIoControlCode ) {
        // handle IOCTL_DISK_DEVICE_INFO without calling into the store or partition IOCTL handler
        if ( !pInBuf || ( nInBufSize < sizeof( STORAGEDEVICEINFO ) ) ) {
            lResult = ERROR_INVALID_PARAMETER;
        } else {
            __try {
                lResult = GetDeviceInfo( ( STORAGEDEVICEINFO* )pInBuf, pBytesReturned );
            }__except( EXCEPTION_EXECUTE_HANDLER ) {
                lResult = ERROR_INVALID_PARAMETER;
            }
        }
    } else if ( IOCTL_DISK_GET_STORAGEID == dwIoControlCode ) {
        // handle IOCTL_DISK_GET_STORAGEID without calling into the store or partition IOCTL handler
        if ( !pOutBuf || ( nOutBufSize < sizeof( STORAGE_IDENTIFICATION ) ) ) {
            lResult = ERROR_INVALID_PARAMETER;
        } else {
            __try {
                // initialize the size to zero to indicate that the operation
                // should not be attempted a second time if it fails.
                ( ( STORAGE_IDENTIFICATION* )pOutBuf )->dwSize = 0;
                lResult = GetStorageId( pOutBuf, nOutBufSize, pBytesReturned );
            }__except( EXCEPTION_EXECUTE_HANDLER ) {
                lResult = ERROR_INVALID_PARAMETER;
            }
        }
    } else {
        __try {
            lResult = StoreIoControl( pPartition, dwIoControlCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned );
        }__except( EXCEPTION_EXECUTE_HANDLER ) {
            lResult = ERROR_INVALID_PARAMETER;
        }
    }

    return lResult;
}

DWORD StoreDisk_t::StoreIoControl ( PartitionDisk_t *pPartition, DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned )
{
    LRESULT lResult = ERROR_SUCCESS;

    if ( NULL != pPartition ) {
        // perform a partition IOCTL
        lResult = pPartition->DiskIoControl ( dwIoControlCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, NULL );
    }
    else {
        // perform a store IOCTL on this LogicalDisk_t object.
        lResult = DiskIoControl ( dwIoControlCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned, NULL );
    }

    return lResult;
}

void StoreDisk_t::SignalActivity ( )
{
    if ( m_hActivityEvent ) {
        ::SetEvent ( m_hActivityEvent );
    }
}

LRESULT StoreDisk_t::GetName ( WCHAR* pDiskName, DWORD NameChars )
{
    if ( !GetShortDeviceName ( m_szDeviceName, pDiskName, NameChars ) ) {
        return ERROR_INSUFFICIENT_BUFFER;
    } else {
        return ERROR_SUCCESS;
    }
}

HANDLE StoreDisk_t::GetDeviceHandle ( )
{
    return m_hStore;
}


/*  direct access to i/o control for a store. intended for use by the partition driver to
    directly access the store. this is the implementation for the abstract LogicalDisk_t class
 */

#ifdef UNDER_CE

LRESULT StoreDisk_t::DiskIoControl ( DWORD IoControlCode, void* pInBuf, DWORD InBufBytes,
    void* pOutBuf, DWORD nOutBufBytes,  DWORD* pBytesReturned, OVERLAPPED* pOverlapped )
{
    BOOL fRet;
    if ( m_pPartDriver ) {
        // If we have a partition driver, let it handle the store IOCTL first.
        fRet = m_pPartDriver->StoreIoControl ( m_dwStoreId, m_hDisk, IoControlCode,
            pInBuf, InBufBytes, pOutBuf, nOutBufBytes, pBytesReturned, pOverlapped );
    } else {
        // Pass the IOCTL directly to the disk.
        fRet = ::DeviceIoControl ( m_hDisk, IoControlCode, pInBuf, InBufBytes,
            pOutBuf, nOutBufBytes, pBytesReturned, pOverlapped );
    }
    return fRet ? ERROR_SUCCESS : FsdGetLastError ( ERROR_GEN_FAILURE );
}

#else // UNDER_CE

LRESULT StoreDisk_t::DiskIoControl ( DWORD IoControlCode, void* pInBuf, DWORD InBufBytes,
    void* pOutBuf, DWORD nOutBufBytes,  DWORD* pBytesReturned, OVERLAPPED* pOverlapped )
{
    BOOL fRet;
    // This is not really a handle under NT but a BlockDevice_t object.
    fRet = ::FS_DevDeviceIoControl ( reinterpret_cast<BlockDevice_t*> ( m_hDisk ), IoControlCode, pInBuf, InBufBytes,
        pOutBuf, nOutBufBytes, pBytesReturned, pOverlapped );

    return fRet ? ERROR_SUCCESS : FsdGetLastError ( ERROR_GEN_FAILURE );
}

#endif // UNDER_CE


void StoreHandle_t::Close ( )
{
    switch ( m_dwSig )
    {
    case PART_HANDLE_SIG:

        DEBUGMSG ( ZONE_VERBOSE, ( L"StoreHandle( 0x%08X ) Close  Part %x ( Ref= %d )\r\n", this, m_pPartition, m_pPartition->RefCount( ) ) );
        m_pPartition->Release ( );
        m_pStore = NULL;
        m_pPartition = NULL;

        break;

    case STORE_HANDLE_SIG:

        DEBUGMSG ( ZONE_VERBOSE, ( L"StoreHandle( 0x%08X ) Close Store %x ( Ref= %d )\r\n", this, m_pStore,m_pStore->RefCount( )) );
        m_pStore->Release ( );
        m_pStore = NULL;

        break;
    };
}

//Returns TRUE if the stores are comparable; else FALSE
BOOL StoreDisk_t::Compare ( StoreDisk_t *pStore )
{
    BOOL bRet = FALSE;

    if ( m_pStorageId && pStore->m_pStorageId ) {

        DWORD dwSize1 =
            ( ( STORAGE_IDENTIFICATION* )( m_pStorageId ) )->dwSize;
        DWORD dwSize2 =
            ( ( STORAGE_IDENTIFICATION* )( pStore->m_pStorageId ) )->dwSize;

        if ( dwSize1 != dwSize2 ) {
            // Storage IDs are different sizes.
            goto exit;
        }

        if ( 0 != memcmp( m_pStorageId, pStore->m_pStorageId, dwSize1 ) ) {
            // Storage IDs are not the same.
            goto exit;
        }

    } else {

        if ( m_pStorageId && !pStore->m_pStorageId ) {
            // Store 1 has an ID, Store 2 does not.
            goto exit;
        }

        if ( !m_pStorageId && pStore->m_pStorageId ) {
            // Store 2 has an ID, Store 1 does not.
            goto exit;
        }
    }

    if ( 0 != memcmp( &m_di, &pStore->m_di, sizeof( DISK_INFO ) ) ) {
        // Disk geometry is different.
        goto exit;
    }

    if ( 0 != memcmp( &m_si, &pStore->m_si, sizeof( PD_STOREINFO ) ) ) {
        // Store info is different.
        goto exit;
    }

    if ( 0 != memcmp( &m_sdi, &pStore->m_sdi,
                sizeof( STORAGEDEVICEINFO ) ) ) {
        // Storage device info is different.
        goto exit;
    }

    if ( m_dwPartCount != pStore->m_dwPartCount ) {
        // Different number of partitions.
        goto exit;
    }

    // Next, check with the partition driver to see if it detects a change.
    // If so, this is the only check we need to make. If not, then perform
    // more in-depth partition comparisons.
    STORAGE_MEDIA_ATTACH_RESULT Result = StorageMediaAttachResultUnchanged;
    if ( ERROR_SUCCESS == m_pPartDriver->MediaChangeEvent ( m_dwStoreId,
        StorageMediaChangeEventAttached, &Result ) &&
        ( StorageMediaAttachResultChanged == Result ) ) {
        // Partition driver indicates a change, so skip comparing each
        // partition individually.
        goto exit;
    }

    // Store compare as the same, now compare every partition.
    DLIST* ptravThis;
    DLIST* ptravNew;

    PartitionDisk_t *pPartitionThis = NULL;
    PartitionDisk_t *pPartitionNew = NULL;

    for ( ptravThis = m_Partitions.m_link.pFwd,ptravNew = pStore->m_Partitions.m_link.pFwd; 
            ( &m_Partitions.m_link != ptravThis ) && ( &pStore->m_Partitions.m_link !=  ptravNew ); 
            ptravThis = ptravThis->pFwd, ptravNew = ptravNew->pFwd ) {

        pPartitionThis = GetPartitionFromDList ( ptravThis );
        pPartitionNew = GetPartitionFromDList ( ptravNew );

        if ( !pPartitionThis->ComparePartitions ( pPartitionNew ) ) {
            break;
        }
    }

    if ( ( &m_Partitions.m_link != ptravThis ) || ( &pStore->m_Partitions.m_link !=  ptravNew ) ) {
        // If one of the partitions is non-NULL after the loop exits
        // we either hit the break or there is a mismatch in partition
        // count.
        goto exit;
    }

    // If we got here, then the stores and all partitions on them match.
    bRet = TRUE;

exit:
    return bRet;
}

// Perform a swap of the information from pStoreNew into this store. This function
// is used by MountStore when we're re-mounting an existing, detached store.
BOOL StoreDisk_t::Swap ( StoreDisk_t* pStoreNew )
{
    DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!StoreDisk_t::Swap: Re-attaching storage device \"%s\" \n",
        pStoreNew->m_szDeviceName ) );

    BOOL bRet = FALSE;
    StoreDisk_t **ppStore = NULL;
    HANDLE hLockStoreDisk = LockAPIHandle ( g_hStoreDiskApi, 
                            reinterpret_cast<HANDLE> ( GetCurrentProcess( ) ),
                            reinterpret_cast<HANDLE> ( pStoreNew->m_hStoreDisk ), reinterpret_cast<LPVOID*> ( &ppStore ) );

    STOREHANDLE *pStoreHandle = NULL;
    HANDLE hLockStore = LockAPIHandle ( g_hStoreApi, 
                            reinterpret_cast<HANDLE>( GetCurrentProcess( ) ),
                            reinterpret_cast<HANDLE> ( pStoreNew->m_hStore ), reinterpret_cast<LPVOID*> ( &pStoreHandle ) );

    //Cannot swap if we dont have the Handle Objects for the new store's handles
    if ( !( hLockStoreDisk && ppStore ) || !( hLockStore && pStoreHandle ) ) {
        //With LockAPIHandle, There is a possibility to get handle != NULL but still Handle Object == NULL
        if ( hLockStoreDisk ) {
            VERIFY( UnlockAPIHandle( g_hStoreDiskApi, hLockStoreDisk ) );
        }

        if ( hLockStore ) {
            VERIFY( UnlockAPIHandle( g_hStoreApi, hLockStore ) );
        }

    } else {

        // Close resources associated with the existing store.
        m_pPartDriver->CloseStore ( m_dwStoreId );

        if ( m_pBlockDevice ) {
    #ifndef UNDER_CE
            if ( ( INVALID_HANDLE_VALUE != m_hDisk ) && m_hDisk ) {
                ::FS_DevCloseFileHandle ( reinterpret_cast<BlockDevice_t*> ( m_hDisk ) );
                m_hDisk = INVALID_HANDLE_VALUE;
            }
    #endif
        }

        if ( INVALID_HANDLE_VALUE != m_hDisk ) {
            ::CloseHandle ( m_hDisk );
            m_hDisk = INVALID_HANDLE_VALUE;
        }

    #ifdef UNDER_CE
        ::CloseHandle ( m_hStore );
        ::CloseHandle ( m_hStoreDisk );
    #else
        ::STG_CloseHandle       ( reinterpret_cast<PSTOREHANDLE> ( m_hStore ) );
        CONV_DevCloseFileHandle ( reinterpret_cast<PSTORE*> ( m_hStoreDisk ) );
    #endif

        // Replace existing resources with the resources from the new store.

        if ( INVALID_HANDLE_VALUE != pStoreNew->m_hStoreDisk ) {
            m_hStoreDisk = pStoreNew->m_hStoreDisk;
            *ppStore = this;
            AddRef ( );

            pStoreNew->Release ( );
            pStoreNew->m_hStoreDisk     = INVALID_HANDLE_VALUE;
        }
        VERIFY( UnlockAPIHandle ( g_hStoreDiskApi, hLockStoreDisk ) );

        if ( INVALID_HANDLE_VALUE != pStoreNew->m_hStore ) {
            m_hStore = pStoreNew->m_hStore;
            pStoreHandle->m_pStore = this;
            AddRef ( );

            pStoreNew->Release ( );
            pStoreNew->m_hStore     = INVALID_HANDLE_VALUE;
        }
        VERIFY( UnlockAPIHandle( g_hStoreApi, hLockStore ) );

        m_dwStoreId     = pStoreNew->m_dwStoreId;
        m_hDisk         = pStoreNew->m_hDisk;

        // Invalidate the copied values from the new store so that they won't be
        // freed. The old store now refers to them.
        pStoreNew->m_dwStoreId  = 0;
        pStoreNew->m_hDisk      = INVALID_HANDLE_VALUE;

        // Device name could have changed (e.g. DSK1: is now DSK2: though the
        // physical device is the same), so copy the new name over the existing
        // store name.
        VERIFY ( SUCCEEDED ( ::StringCchCopyW ( m_szDeviceName, _countof ( m_szDeviceName ), pStoreNew->m_szDeviceName ) ) );

        // Copy partition information from the new store to the existing store.
        DLIST* ptravThis=NULL;
        DLIST* ptravNew=NULL;
        PartitionDisk_t *pPartitionThis=NULL;
        PartitionDisk_t *pPartitionNew=NULL;

        for ( ptravThis = m_Partitions.m_link.pFwd, ptravNew = pStoreNew->m_Partitions.m_link.pFwd; 
                ( &m_Partitions.m_link != ptravThis ) && ( &pStoreNew->m_Partitions.m_link !=  ptravNew ); 
                ptravThis = ptravThis->pFwd, ptravNew = ptravNew->pFwd ) {

            pPartitionThis = GetPartitionFromDList ( ptravThis );
            pPartitionNew = GetPartitionFromDList ( ptravNew );

            pPartitionThis->m_dwPartitionId = pPartitionNew->m_dwPartitionId;
            pPartitionThis->m_dwStoreId = pPartitionNew->m_dwStoreId;
            pPartitionNew->m_dwPartitionId = 0;
            pPartitionNew->m_dwStoreId = 0;
        }

        bRet = TRUE;
    }

    return bRet;
}

#endif /* !UNDER_CE */
