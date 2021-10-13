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

#ifdef UNDER_CE
#include <psystoken.h>
#endif

LRESULT MountFileSystemDriver ( MountableDisk_t* pDisk, FileSystemDriver_t* pFSD, DWORD MountFlags, BOOL fDoFormat );

DWORD PartitionDriver_t::LoadPartitionDriver ( const WCHAR *szPartDriver )
{
    DWORD dwError = ERROR_SUCCESS;

    m_pOpenStore = ( POPENSTORE )PD_OpenStore;
    m_pCloseStore = ( PCLOSESTORE )PD_CloseStore;
    m_pFormatStore = ( PFORMATSTORE )PD_FormatStore;
    m_pIsStoreFormatted = ( PISSTOREFORMATTED )PD_IsStoreFormatted;
    m_pGetStoreInfo = ( PGETSTOREINFO )PD_GetStoreInfo;
    m_pCreatePartition = ( PCREATEPARTITION )PD_CreatePartition;
    m_pDeletePartition = ( PDELETEPARTITION )PD_DeletePartition;
    m_pRenamePartition = ( PRENAMEPARTITION )PD_RenamePartition;
    m_pSetPartitionAttrs = ( PSETPARTITIONATTRS )PD_SetPartitionAttrs;
    m_pSetPartitionSize = ( PSETPARTITIONSIZE )PD_SetPartitionSize;
    m_pFormatPartition = ( PFORMATPARTITION )PD_FormatPartition;
    m_pGetPartitionInfo = ( PGETPARTITIONINFO )PD_GetPartitionInfo;
    m_pFindPartitionStart = ( PFINDPARTITIONSTART )PD_FindPartitionStart;
    m_pFindPartitionNext = ( PFINDPARTITIONNEXT )PD_FindPartitionNext;
    m_pFindPartitionClose = ( PFINDPARTITIONCLOSE )PD_FindPartitionClose;
    m_pOpenPartition = ( POPENPARTITION )PD_OpenPartition;
    m_pClosePartition = ( PCLOSEPARTITION )PD_ClosePartition;
    m_pDeviceIoControl = ( PPD_DEVICEIOCONTROL )PD_DeviceIoControl;
    m_pMediaChangeEvent = ( PMEDIACHANGEEVENT )PD_MediaChangeEvent;
    m_pStoreIoControl = ( PSTOREIOCONTROL )NULL;

    if ( szPartDriver[0] ) {
        m_hPartDriver = ::LoadDriver ( szPartDriver );
        if ( m_hPartDriver ) {
            DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!LoadPartitionDriver: Loading partition driver %s hModule=%08X\r\n", szPartDriver, m_hPartDriver ) );
            m_pOpenStore = ( POPENSTORE )FsdGetProcAddress ( m_hPartDriver, L"PD_OpenStore" );
            m_pCloseStore = ( PCLOSESTORE )FsdGetProcAddress ( m_hPartDriver, L"PD_CloseStore" );
            m_pFormatStore = ( PFORMATSTORE )FsdGetProcAddress ( m_hPartDriver, L"PD_FormatStore" );
            m_pIsStoreFormatted = ( PISSTOREFORMATTED )FsdGetProcAddress ( m_hPartDriver, L"PD_IsStoreFormatted" );
            m_pGetStoreInfo = ( PGETSTOREINFO )FsdGetProcAddress ( m_hPartDriver, L"PD_GetStoreInfo" );
            m_pCreatePartition = ( PCREATEPARTITION )FsdGetProcAddress ( m_hPartDriver, L"PD_CreatePartition" );
            m_pDeletePartition = ( PDELETEPARTITION )FsdGetProcAddress ( m_hPartDriver, L"PD_DeletePartition" );
            m_pRenamePartition = ( PRENAMEPARTITION )FsdGetProcAddress ( m_hPartDriver, L"PD_RenamePartition" );
            m_pSetPartitionAttrs = ( PSETPARTITIONATTRS )FsdGetProcAddress ( m_hPartDriver, L"PD_SetPartitionAttrs" );
            m_pSetPartitionSize = ( PSETPARTITIONSIZE )FsdGetProcAddress ( m_hPartDriver, L"PD_SetPartitionSize" );
            if ( !m_pSetPartitionSize ) m_pSetPartitionSize = ( PSETPARTITIONSIZE )PD_SetPartitionSize;
            m_pFormatPartition = ( PFORMATPARTITION )FsdGetProcAddress ( m_hPartDriver, L"PD_FormatPartition" );
            m_pGetPartitionInfo = ( PGETPARTITIONINFO )FsdGetProcAddress ( m_hPartDriver, L"PD_GetPartitionInfo" );
            m_pFindPartitionStart = ( PFINDPARTITIONSTART )FsdGetProcAddress ( m_hPartDriver, L"PD_FindPartitionStart" );
            m_pFindPartitionNext = ( PFINDPARTITIONNEXT )FsdGetProcAddress ( m_hPartDriver, L"PD_FindPartitionNext" );
            m_pFindPartitionClose = ( PFINDPARTITIONCLOSE )FsdGetProcAddress ( m_hPartDriver, L"PD_FindPartitionClose" );
            m_pOpenPartition = ( POPENPARTITION )FsdGetProcAddress ( m_hPartDriver, L"PD_OpenPartition" );
            m_pClosePartition = ( PCLOSEPARTITION )FsdGetProcAddress ( m_hPartDriver, L"PD_ClosePartition" );
            m_pDeviceIoControl = ( PPD_DEVICEIOCONTROL )FsdGetProcAddress ( m_hPartDriver, L"PD_DeviceIoControl" );
            m_pMediaChangeEvent = ( PMEDIACHANGEEVENT )FsdGetProcAddress ( m_hPartDriver, L"PD_MediaChangeEvent" );
            m_pStoreIoControl = ( PSTOREIOCONTROL )FsdGetProcAddress ( m_hPartDriver, L"PD_StoreIoControl" );

            if ( !m_pOpenStore ||
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
                !m_pDeviceIoControl )
            {
                DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!LoadPartitionDriver: Entry points in the driver not found\r\n" ) );
                dwError = ERROR_BAD_DRIVER;
            }
            DEBUGMSG ( ZONE_VERBOSE, ( L"FSDMGR!LoadPartitionDriver: Driver %s loaded\r\n", szPartDriver ) );
        } else {
            DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!LoadPartitionDriver: Could not find/load partition driver %s\r\n", szPartDriver ) );
            dwError = ERROR_FILE_NOT_FOUND;
        }
    }
    return dwError;
}

PartitionDisk_t::PartitionDisk_t ( StoreDisk_t *pStore, DWORD dwStoreId, DWORD dwPartitionId, PartitionDriver_t *pPartDriver, const WCHAR *szFolderName, BlockDevice_t* /* pBlockDevice */ ) :
        MountableDisk_t ( TRUE ),
        m_dwStoreId ( dwStoreId ),
        m_dwPartitionId ( dwPartitionId ),
        m_pPartDriver ( pPartDriver ),
        m_hPartition ( INVALID_HANDLE_VALUE ),
        m_dwAttrs ( 0 ),
        m_dwState ( 0 ),
        m_pStore ( pStore ),
        m_fFormatOnMount ( FALSE )
{
    ::memset ( &m_pi, 0, sizeof ( PD_PARTINFO ) );
    m_szPartitionName[0] = '\0';
    m_szFileSys[0] = '\0';
    m_szFriendlyName[0] = '\0';
    VERIFY ( SUCCEEDED (::StringCbCopyW ( m_szFolderName, sizeof ( m_szFolderName ), szFolderName ) ) );

    //Init the PartitionList 
    InitDList ( &m_link );
    m_RefCount = 1;
    if ( m_pStore ) {
        m_pStore->AddRef ( );
    }
    m_ThisMountableDiskType = PartitionDisk_Type;

    DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "Partition %x ( Ref= %d ) Created on Store %x ( Ref= %d )\n" ),this, m_RefCount, m_pStore, m_pStore->RefCount ( ) ) );
}

PartitionDisk_t::PartitionDisk_t ( ):
    MountableDisk_t ( FALSE ),
    m_dwStoreId ( 0 ),
    m_dwPartitionId ( 0 ),
    m_pPartDriver ( NULL ),
    m_hPartition ( INVALID_HANDLE_VALUE ),
    m_dwAttrs ( 0 ),
    m_dwState ( 0 ),
    m_pStore ( NULL ),
    m_fFormatOnMount ( FALSE )
{
    m_szPartitionName[0] = '\0';
    m_szFileSys[0] = '\0';
    m_szFriendlyName[0] = '\0';

    //Init the PartitionList 
    InitDList ( &m_link );
    m_RefCount = 1;
    m_ThisMountableDiskType = PartitionDisk_Type;
}

PartitionDisk_t::~PartitionDisk_t ( )
{
    ASSERT ( m_hPartition == INVALID_HANDLE_VALUE );
    DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!PartitionDisk_t::~PartitionDisk_t: deleting partition ( %p )", this ) );

    if ( m_pStore ) {
        m_pStore->Release ( );
        DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "Partition %x ( Ref= %d ) Destroyed on Store %x ( Ref= %d )\n" ),this, m_RefCount, m_pStore, m_pStore->RefCount ( ) ) );
    }
}


BOOL PartitionDisk_t::GetPartitionInfo ( PARTINFO *pInfo )
{
    __try {
        pInfo->bPartType = m_pi.bPartType;
        pInfo->dwAttributes = m_dwAttrs;
        if ( IsMounted ( ) ) {
           pInfo->dwAttributes |= PARTITION_ATTRIBUTE_MOUNTED;
        }
        memcpy ( &pInfo->ftCreated, &m_pi.ftCreated, sizeof ( FILETIME ) );
        memcpy ( &pInfo->ftLastModified, &m_pi.ftLastModified, sizeof ( FILETIME ) );
        pInfo->snNumSectors = m_pi.snNumSectors;
        VERIFY ( SUCCEEDED ( StringCchCopy ( pInfo->szPartitionName, PARTITIONNAMESIZE, m_szPartitionName ) ) );
        if ( m_szFriendlyName[0] ) {
            StringCchCopyW ( pInfo->szFileSys, FILESYSNAMESIZE, m_szFriendlyName );
        } else {
            StringCchCopyW ( pInfo->szFileSys, FILESYSNAMESIZE, m_szFileSys );
        }
        VERIFY ( SUCCEEDED ( StringCchCopy ( pInfo->szVolumeName, VOLUMENAMESIZE, L"" ) ) );
        MountedVolume_t* pVolume = GetVolume ( );
        if ( pVolume ) {
            FSDMGR_GetVolumeName ( pVolume->GetVolumeHandle ( ), pInfo->szVolumeName, VOLUMENAMESIZE );
        }
    } __except ( EXCEPTION_EXECUTE_HANDLER ) {
        return FALSE;
    }
    return TRUE;
}

BOOL PartitionDisk_t::GetStoreInfo ( STOREINFO *pInfo )
{
    return m_pStore->GetStoreInfo ( pInfo );
}

void PartitionDisk_t::SignalActivity ( )
{
    // Simply signal activity on the parent store.
    m_pStore->SignalActivity ( );
}

LRESULT PartitionDisk_t::GetName ( WCHAR* pDiskName, DWORD NameChars )
{
    if ( FAILED ( ::StringCchCopyW ( pDiskName, NameChars, m_szPartitionName ) ) ) {
        return ERROR_INSUFFICIENT_BUFFER;
    } else {
        return ERROR_SUCCESS;
    }
}

LRESULT PartitionDisk_t::DiskIoControl ( DWORD IoControlCode, void* pInBuf,
        DWORD InBufBytes, void* pOutBuf, DWORD OutBufBytes,
        DWORD* pBytesReturned, OVERLAPPED* /* pOverlapped */ )
{
    DWORD lResult = ERROR_SUCCESS;

    if ( ( IOCTL_DISK_GETNAME == IoControlCode ) ||
        ( DISK_IOCTL_GETNAME == IoControlCode ) ) {

        size_t FolderNameChars = 0;
        VERIFY ( SUCCEEDED ( ::StringCchLengthW ( m_szFolderName, FOLDERNAMESIZE, &FolderNameChars ) ) );

        if ( !pOutBuf || ( OutBufBytes < ( ( FolderNameChars+1 ) * sizeof ( WCHAR ) ) ) ) {
            return ERROR_INSUFFICIENT_BUFFER;
        }

        VERIFY ( SUCCEEDED ( ::StringCbCopyW ( ( WCHAR* )pOutBuf, OutBufBytes, m_szFolderName ) ) );
        if ( pBytesReturned ) {
            *pBytesReturned = ( FolderNameChars+1 ) * sizeof ( WCHAR );
        }

        return ERROR_SUCCESS;
    }

    lResult = m_pPartDriver->DeviceIoControl ( m_dwPartitionId,
                                              IoControlCode,
                                              pInBuf,
                                              InBufBytes,
                                              pOutBuf,
                                              OutBufBytes,
                                              pBytesReturned );

    if ( ERROR_SUCCESS == lResult )
    {
        if ( IoControlCode == IOCTL_DISK_WRITE )
        {
            SetFormatOnMount ( FALSE );
        }
    }

    return lResult;
}

LRESULT PartitionDisk_t::GetVolumeInfo ( CE_VOLUME_INFO* pInfo )
{
    VERIFY ( SUCCEEDED ( ::StringCchCopyW ( pInfo->szPartitionName, PARTITIONNAMESIZE, m_szPartitionName ) ) );
    VERIFY ( GetStoreNameFromDeviceName ( m_pStore->GetDeviceName ( ), pInfo->szStoreName, STORENAMESIZE ) );

    pInfo->dwFlags |= CE_VOLUME_FLAG_STORE;

    if ( ( m_pStore->IsReadOnly ( ) ) ||
        ( STORE_ATTRIBUTE_READONLY & m_pStore->m_si.dwAttributes ) ||
        ( PARTITION_ATTRIBUTE_READONLY & m_pi.dwAttributes ) ) {
        pInfo->dwAttributes |= CE_VOLUME_ATTRIBUTE_READONLY;
    }

    if ( ( m_pStore->IsRemovable ( ) ) ||
        ( STORE_ATTRIBUTE_REMOVABLE & m_pStore->m_si.dwAttributes ) ) {
        pInfo->dwAttributes |= CE_VOLUME_ATTRIBUTE_REMOVABLE;
    }

    return ERROR_SUCCESS;
}

HANDLE PartitionDisk_t::GetDeviceHandle ( )
{
    return m_hPartition;
}

LRESULT PartitionDisk_t::GetSecurityClassName ( WCHAR* pSecurityClassName, DWORD NameChars )
{
    if ( FAILED ( StringCchCopyW ( pSecurityClassName, NameChars, m_pStore->m_sdi.szProfile ) ) ) {
        return ERROR_INSUFFICIENT_BUFFER;
    } else {
        return ERROR_SUCCESS;
    }
}

BOOL PartitionDisk_t::Init ( LPCTSTR szPartitionName, LPCTSTR RootRegKey )
{
    StringCchCopyW ( m_szPartitionName, PARTITIONNAMESIZE, szPartitionName );
    m_pPartDriver->GetPartitionInfo ( m_dwStoreId, szPartitionName, &m_pi );

    LRESULT lResult;

    lResult = AddRootRegKey ( g_szSTORAGE_PATH );
    if ( ERROR_SUCCESS != lResult ) {
        ::SetLastError ( lResult );
        return FALSE;
    }

    lResult = AddRootRegKey ( RootRegKey );
    if ( ERROR_SUCCESS != lResult ) {
        ::SetLastError ( lResult );
        return FALSE;
    }

    return TRUE;
}

BOOL PartitionDisk_t::Mount ( HANDLE hPartition )
{
    LRESULT lResult = ERROR_UNRECOGNIZED_VOLUME;
    BOOL bRet = FALSE;

    DWORD dwMountFlags = m_pStore->GetDefaultMountFlags ( );

    const WCHAR* const RootRegKey = m_pStore->GetRootRegKey ( );
    const WCHAR* const DefaultFileSystem = m_pStore->GetDefaultFSName ( );

    // Look for "CheckForFormat" under the profile key.

    HKEY hKey = NULL;
    DWORD CheckForFormat = 0;
    ::FsdRegOpenKey ( RootRegKey, &hKey );
    if ( hKey ) {
        ::FsdGetRegistryValue ( hKey, g_szCHECKFORFORMAT,  &CheckForFormat );
        ::FsdRegCloseKey ( hKey );
        hKey = NULL;
    }

    // Determine the name of the file system.

    // Try to open a profile specific partition table key.
    //
    //      HKLM\System\StorageManager\Profiles\<ProfileName>\PartitionTable
    //

    WCHAR RegKey[MAX_PATH];
    VERIFY ( SUCCEEDED ( ::StringCchPrintfW ( RegKey, MAX_PATH, L"%s\\%s", RootRegKey, g_szPART_TABLE_STRING ) ) );
    if ( ERROR_SUCCESS != ::FsdRegOpenKey ( RegKey, &hKey ) ) {

        // There is no profile specific partition table key, use the default partition table key.
        //
        //      HKLM\System\StorageManager\PartitionTable
        //

        VERIFY ( SUCCEEDED ( ::StringCchPrintfW ( RegKey, MAX_PATH, L"%s\\%s", g_szSTORAGE_PATH, g_szPART_TABLE_STRING ) ) );
        ::FsdRegOpenKey ( RegKey, &hKey );
    }


    // Convert partition type into a string in %02x format.
    WCHAR PartId[10];
    if ( hKey ) {

        // NOTE: We use _itow here instead of wsprintf because the latter requires the locale to be
        // initialized. Since this code must execute in boot-phase zero, we cannot be dependent on
        // any locale-sensitive functions.

        WCHAR* pTempId = PartId;
        DWORD TempIdChars = sizeof ( PartId ) / sizeof ( PartId[0] );
        // Convert m_pi.bPartType into a string in %02x format.
        if ( m_pi.bPartType < 0x10 ) {
            *pTempId++ = L'0';
            TempIdChars -= 1;
        }

        VERIFY ( 0 == ::_itow_s ( m_pi.bPartType, pTempId, TempIdChars, 16 ) );

        // Use the partition mapping table to map the partition type hex string to the name of the
        // file system to mount on this partition.
        if ( !::FsdGetRegistryString ( hKey, PartId, m_szFileSys, sizeof ( m_szFileSys )/sizeof ( WCHAR ) ) ) {
            // This partition type has no entry in the table, so use the default file system.
            VERIFY ( SUCCEEDED ( ::StringCchCopyW ( m_szFileSys, FILESYSNAMESIZE, DefaultFileSystem ) ) );
        }
        ::FsdRegCloseKey ( hKey );
        hKey = NULL;

    } else {

        // No partition table anywhere? Use the default file system.
        VERIFY ( SUCCEEDED ( ::StringCchCopy ( m_szFileSys, FILESYSNAMESIZE, DefaultFileSystem ) ) );
    }

    DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!ParitionDisk::Mount: Partition Type 0x%s --> \"%s\"\r\n",
        PartId, m_szFileSys ) );

    if ( !m_szFileSys[0] ) {
        // No file system name.
        ::SetLastError ( ERROR_FILE_NOT_FOUND );
        goto Error;
    }

    // Set the file system name as the sub-key for this LogicalDisk_t object.
    lResult = SetRegSubKey ( m_szFileSys, FILESYSNAMESIZE );
    if ( ERROR_SUCCESS != lResult ) {
        ::SetLastError ( lResult );
        goto Error;
    }

    // Look for the module name under the profile or the root file system key.
    //
    //      HKLM\System\StorageManager\Profiles\<ProfileName>\<FileSystemName>
    //
    //          or
    //
    //      HKLM\System\StorageManager\<FileSystemName>
    //

    WCHAR ModuleName[MAX_PATH] = L"";

    GetRegistryString ( g_szFILE_SYSTEM_MODULE_STRING, ModuleName, sizeof ( ModuleName )/sizeof ( WCHAR ) );
    GetRegistryString ( g_szFILE_SYSTEM_MODULE_STRING, m_szFriendlyName, sizeof ( m_szFriendlyName )/sizeof ( WCHAR ) );

    //Preserve the Partition Handle as the DetectFilesystem needs to get back hPartition
    m_hPartition = hPartition;

    if ( !ModuleName[0] ) {

        // No module name found using the determined file system name, so
        // attempt to run FSD detectors. If successful, this will replace
        // the file system name.
        lResult = DetectFileSystem ( m_szFileSys, FILESYSNAMESIZE );
        if ( ERROR_UNRECOGNIZED_VOLUME == lResult ) {
            // None of the detectors recognized the volume, so use the
            // default file system, if one is specified.
            lResult = GetRegistryString ( g_szFILE_SYSTEM_STRING, m_szFileSys, FILESYSNAMESIZE/sizeof ( WCHAR ) );
        }

        if ( ERROR_SUCCESS != lResult ) {
            DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!ParitionDisk::Mount: DetectFileSystem failed; error=%u\r\n", lResult ) );
            ::SetLastError ( lResult );
            m_hPartition = INVALID_HANDLE_VALUE;    //hPartition is closed by the caller for FAILED cases
            goto Error;
        }

        DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!ParitionDisk::Mount: DetectFileSystem remapped 0x%s --> \"%s\"\r\n",
            PartId, m_szFileSys ) );

        // Replace our reg subkey with the new name determined by running
        // the FSD detector ( s ).
        lResult = SetRegSubKey ( m_szFileSys );
        if ( ERROR_SUCCESS != lResult ) {
            ::SetLastError ( lResult );
            m_hPartition = INVALID_HANDLE_VALUE;    //hPartition is closed by the caller for FAILED cases
            goto Error;
        }

        // Try again now that we've loaded a different file system name
        // based on the results of DetectFileSystem.
        GetRegistryString ( g_szFILE_SYSTEM_MODULE_STRING, ModuleName, sizeof ( ModuleName )/sizeof ( WCHAR ) );
        GetRegistryString ( g_szFILE_SYSTEM_MODULE_STRING, m_szFriendlyName, sizeof ( m_szFriendlyName )/sizeof ( WCHAR ) );
    }

    DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!ParitionDisk::Mount: \"%s\" --> %s\r\n", m_szFileSys, ModuleName ) );

    if ( !ModuleName[0] ) {
        // Could not determine the name of the module to use for this file system.
        ::SetLastError ( ERROR_MOD_NOT_FOUND );
        m_hPartition = INVALID_HANDLE_VALUE;    //hPartition is closed by the caller for FAILED cases
        goto Error;
    }

    // Instantiate a file system object using the specified dll name.
    FileSystemDriver_t* pFileSystem = new FileSystemDriver_t ( this, ModuleName );
    if ( !pFileSystem ) {
        ::SetLastError ( ERROR_NOT_ENOUGH_MEMORY );
        m_hPartition = INVALID_HANDLE_VALUE;    //hPartition is closed by the caller for FAILED cases
        goto Error;
    }

    WCHAR FolderName[FOLDERNAMESIZE];

    // Look for MountAsXXX ( MountFlags ), CheckForFormat, and Folder values under the
    // file system key.
    //
    //      HKLM\System\StorageManager\Profiles\<ProfileName>\<FileSystemName>
    //

    VERIFY ( SUCCEEDED ( ::StringCchPrintfW ( RegKey, MAX_PATH, L"%s\\%s", RootRegKey, m_szFileSys ) ) );
    if ( ERROR_SUCCESS == ::FsdRegOpenKey ( RegKey, &hKey ) ) {
        g_pMountTable->GetMountSettings ( hKey, &dwMountFlags );
        DWORD TmpFlags;
        if ( ::FsdGetRegistryValue ( hKey, g_szCHECKFORFORMAT, &TmpFlags ) ) {
            CheckForFormat = TmpFlags;
        }
        if ( ::FsdGetRegistryString ( hKey, g_szFOLDER_NAME_STRING, FolderName, sizeof ( FolderName )/sizeof ( WCHAR ) ) ) {
            VERIFY ( SUCCEEDED ( ::StringCchCopyW ( m_szFolderName, FOLDERNAMESIZE, FolderName ) ) );
        }
        ::FsdRegCloseKey ( hKey );
        hKey = NULL;
    }

    // Look for MountAsXXX ( MountFlags ) and Folder values under the partition key.
    //
    //      HKLM\System\StorageManager\Profiles\<ProfileName>\<PartitionName>
    //

    VERIFY ( SUCCEEDED ( ::StringCchPrintfW ( RegKey, MAX_PATH, L"%s\\%s", RootRegKey, m_szPartitionName ) ) );
    if ( ERROR_SUCCESS == ::FsdRegOpenKey ( RegKey, &hKey ) ) {
        g_pMountTable->GetMountSettings ( hKey, &dwMountFlags );
        if ( ::FsdGetRegistryString ( hKey, g_szFOLDER_NAME_STRING, FolderName, sizeof ( FolderName )/sizeof ( WCHAR ) ) ) {
            VERIFY ( SUCCEEDED ( ::StringCchCopyW ( m_szFolderName, FOLDERNAMESIZE, FolderName ) ) );
        }
        ::FsdRegCloseKey ( hKey );
        hKey = NULL;
    }

    // Enable the partition before mounting a volume on it.
    Enable ( );

#ifdef UNDER_CE
    if ( CheckForFormat ) {
        // Query the kernel with STORAGECONTEXT information to determine whether or
        // not this partition should be formatted before it is mounted.
        BOOL fTemp = 0;
        STORAGECONTEXT sc = {0};
        sc.cbSize = sizeof ( sc );
        m_pStore->GetStoreInfo ( &sc.StoreInfo );
        GetPartitionInfo ( &sc.PartInfo );
        sc.dwFlags = dwMountFlags;
        if ( ::KernelIoControl ( IOCTL_HAL_QUERY_FORMAT_PARTITION, &sc, sizeof ( sc ), ( LPBYTE )&fTemp, sizeof ( BOOL ), NULL ) ) {
            if ( fTemp ) {
                FormatPartition ( m_pi.bPartType, FALSE );
            }
        }
    }
#endif // UNDER_CE

    // Mount a new volume.
    lResult = MountFileSystemDriver ( this, pFileSystem, dwMountFlags, m_fFormatOnMount );
    if ( ERROR_SUCCESS != lResult ) {
        delete pFileSystem;
        ::SetLastError ( lResult );
        m_hPartition = INVALID_HANDLE_VALUE;    //hPartition is closed by the caller for FAILED cases
        goto Error;
    }

    // The partition is now mounted.
    SetMounted ( );
    m_fFormatOnMount = FALSE;
    bRet = TRUE;

Error:

    return bRet;
}

void PartitionDisk_t::Disable ( )
{
    if ( IsDisabled ( ) ) {
        return;
    }

    MountedVolume_t* pVolume = GetVolume ( );
    if ( pVolume ) {
        pVolume->Disable ( );
    }

    SetDisabled ( );
}

void PartitionDisk_t::Enable ( )
{
    if ( ! IsDisabled ( ) ) {
        return;
    }

    MountedVolume_t* pVolume = GetVolume ( );
    if ( pVolume ) {
        pVolume->Enable ( );
    }

    SetEnabled ( );
}


BOOL PartitionDisk_t::Unmount ( )
{
    Disable ( );

    LRESULT lResult = UnmountFileSystemDriver ( this );
    if ( ERROR_SUCCESS != lResult ) {
        Enable ( );
        ::SetLastError ( lResult );
        return FALSE;
    }

    SetUnmounted ( );
#ifdef UNDER_CE
    ::CloseHandle ( m_hPartition );
#else
    ::STG_CloseHandle ( reinterpret_cast<PSTOREHANDLE> ( m_hPartition ) );
#endif

    m_hPartition = INVALID_HANDLE_VALUE;

    return TRUE;
}

BOOL PartitionDisk_t::RenamePartition ( LPCTSTR szNewName )
{
    DWORD dwError = m_pPartDriver->RenamePartition ( m_dwStoreId, m_szPartitionName, szNewName );
    if ( ERROR_SUCCESS == dwError ) {
        VERIFY ( SUCCEEDED ( StringCchCopy ( m_szPartitionName, PARTITIONNAMESIZE, szNewName ) ) );
        return TRUE;
    }
    SetLastError ( dwError );
    return FALSE;
}

BOOL PartitionDisk_t::FormatPartition ( BYTE bPartType, BOOL bAuto )
{
    DWORD dwError = m_pPartDriver->FormatPartition ( m_dwStoreId, m_szPartitionName, bPartType, bAuto );
    if ( ERROR_SUCCESS == dwError ) {
        m_pPartDriver->GetPartitionInfo ( m_dwStoreId, m_szPartitionName, &m_pi );
        m_fFormatOnMount = TRUE;
        return TRUE;
    }
    SetLastError ( dwError );
    return FALSE;
}

// RunAllDetectors
//
// Determine the name of the file system to associate with
// a partition by invoking a set of detector dlls.
//
// Returns ERROR_SUCCESS and the file system name in
// pFileSystemName if detection was successful.
//
// Returns non-zero error code on failure:
//      ERROR_UNRECOGNIZED_VOLUME if the no detector succeeds
//      ERROR_INSUFFICIENT_BUFFER if the file system name is
//      longer than FileSystemNameChars characters.
//
LRESULT PartitionDisk_t::RunAllDetectors ( HKEY hKey,
        __out_ecount ( FileSystemNameChars ) WCHAR* pFileSystemName,
        size_t FileSystemNameChars )
{
    LRESULT lResult = ERROR_UNRECOGNIZED_VOLUME;

    // Use LoadFSDList to build a list of detector Dlls.
    FSDLOADLIST* pLoadList = LoadFSDList ( hKey, LOAD_FLAG_ASYNC | LOAD_FLAG_SYNC,
        NULL, NULL, FALSE );

    BYTE* pBootSector = NULL;
    DWORD SectorSize = m_pStore->m_si.dwBytesPerSector;
    FSDLOADLIST* pNextDetect = pLoadList;

    if ( pNextDetect ) {
        // Try to read sector zero from the disk to pass to the detector.
        pBootSector = new BYTE[SectorSize];
        if ( pBootSector &&
            ( ERROR_SUCCESS != FSDMGR_ReadDisk ( this, 0, 1, pBootSector, SectorSize ) ) ) {
            // Unable to read boot sector. Pass NULL to the detector.
            delete[] pBootSector;
            pBootSector = NULL;
        }
    }

    // Iterate over the load list, invoking every detector driver.
    while ( pNextDetect &&
            ( ERROR_UNRECOGNIZED_VOLUME == lResult ) ) {

        // Open the detector key.
        HKEY hSubKey = NULL;
        if ( ERROR_SUCCESS ==
            ::FsdRegOpenSubKey ( hKey, pNextDetect->szName, &hSubKey ) ) {

            GUID FsdGuid;
            WCHAR DetectorDll[MAX_PATH];
            WCHAR DetectorExport[MAX_PATH];

            // Query the GUID, detector dll name, and dll export name from the
            // registry and invoke the detector.
            if ( ::FsdGetRegistryString ( hSubKey, L"Guid", DetectorDll, MAX_PATH ) &&
                ::FsdGuidFromString ( DetectorDll, &FsdGuid ) &&
                ::FsdGetRegistryString ( hSubKey, L"Dll", DetectorDll, MAX_PATH ) &&
                ::FsdGetRegistryString ( hSubKey, L"Export", DetectorExport,
                    MAX_PATH ) ) {

                // Run the detector function.
                lResult = m_DetectorState.RunDetector ( this, DetectorDll, DetectorExport,
                    &FsdGuid, pBootSector, SectorSize );
            }

            ::FsdRegCloseKey ( hSubKey );
            hSubKey = NULL;

            if ( ERROR_SUCCESS == lResult ) {
                // On success, copy the file system name to the output buffer.
                if ( FAILED ( ::StringCchCopy ( pFileSystemName,
                                FileSystemNameChars, pNextDetect->szName ) ) ) {
                    // Failed to copy to output buffer, indicate failure.
                    lResult = ERROR_INSUFFICIENT_BUFFER;
                }
            }
        }

        pNextDetect = pNextDetect->pNext;
    }

    if ( pBootSector ) {
        delete[] pBootSector;
    }

    // Cleanup the list.
    while ( pLoadList ) {
        pNextDetect = pLoadList;
        pLoadList = pLoadList->pNext;
        delete pNextDetect;
    }

    return lResult;
}

LRESULT PartitionDisk_t::DetectFileSystem ( __out_ecount ( FileSystemNameChars ) WCHAR* pFileSystemName, size_t FileSystemNameChars )
{
    LRESULT lResult = ERROR_UNRECOGNIZED_VOLUME;
    WCHAR LocalString[MAX_PATH];
    HKEY hKey = NULL;

    // Enumerate all registry keys.
    RootRegKeyListItem* pItem;

    for ( pItem = m_pRootRegKeyList; pItem != NULL; pItem = pItem->pNext ) {

        if ( FAILED ( ::StringCchPrintfW ( LocalString, MAX_PATH, L"%s\\%s\\Detectors",
            pItem->RootRegKeyName, m_pRegSubKey ) ) ) {
            return ERROR_REGISTRY_IO_FAILED;
        }

        if ( ERROR_SUCCESS == ::FsdRegOpenKey ( LocalString, &hKey ) ) {

            // Run detectors listed under this key.
            lResult = RunAllDetectors ( hKey, pFileSystemName,
                FileSystemNameChars );
            if ( ERROR_UNRECOGNIZED_VOLUME != lResult ) {
                ::FsdRegCloseKey ( hKey );
                hKey = NULL;
                break;
            }

            ::FsdRegCloseKey ( hKey );
            hKey = NULL;
        }
    }

    if ( lResult != ERROR_SUCCESS ) {
        DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!ParitionDisk::DetectFileSystem: No file systems were detected for this partition.\r\n" ) );
    }

    return lResult;

}

DWORD PartitionDisk_t::CreateHandle ( HANDLE *phHandle, HANDLE hProc, DWORD dwAccess )
{
    DWORD dwError = ERROR_GEN_FAILURE;
    HANDLE hPartition = INVALID_HANDLE_VALUE;
    STOREHANDLE *pPartHandle = new StoreHandle_t ( hProc,dwAccess,0,PART_HANDLE_SIG );

    if ( pPartHandle ) {
        pPartHandle->m_pPartition = this;
        pPartHandle->m_pStore = m_pStore;
        // Create a kernel handle for this store handle object.
        hPartition = CreateAPIHandleWithAccess ( g_hStoreApi, pPartHandle, dwAccess, GetCurrentProcess ( ) );
        if ( hPartition ) {
            AddRef ( );
            dwError = ERROR_SUCCESS;
        } else {
            hPartition = INVALID_HANDLE_VALUE;
            dwError = ::FsdGetLastError ( ERROR_GEN_FAILURE );
            delete pPartHandle;
        }
    }
    // Output the handle value, valid or invalid.
    *phHandle = hPartition;
    
    return dwError;
}

//Return TRUE if the Partition is comparable; else FALSE
BOOL PartitionDisk_t::ComparePartitions ( PartitionDisk_t *pPartition )
{
    LRESULT lResult = ERROR_UNRECOGNIZED_VOLUME;

    // Proceed further only if Partition information match
    if ( 0 == memcmp ( &m_pi, &pPartition->m_pi, sizeof ( PD_PARTINFO ) ) ) {

        if ( m_DetectorState.IsDetectorPresent ( ) ) {
            // Read the boot sector to pass to the detector.
            DWORD SectorSize = pPartition->m_pStore->m_si.dwBytesPerSector;
            BYTE* pBootSector = new BYTE[SectorSize];

            if ( pBootSector ) {
                FSDMGR_ReadDisk ( pPartition, 0, 1, pBootSector, SectorSize );
                // FSDMGR_ReadDisk will fail at this point for CDROM drives, but that is okay
            }

            // Run the detector which identified this partition on the new
            // partition to see if it also succeeds.
            lResult = m_DetectorState.ReRunDetector ( pPartition, pBootSector, SectorSize );

            if ( pBootSector ) {
                delete[] pBootSector;
                pBootSector = NULL;
            }

        }

        // In the case there there is no detector state, then a detector was
        // not used to detect this volume's file system type and so the file
        // system types are assumed to be the same since the partition info
        // contents were identical.

        if ( ( ERROR_SUCCESS == lResult ) && m_pVolume ) {
            // Now check with the volume to see if its view of the partition
            // is the same.
            STORAGE_MEDIA_ATTACH_RESULT Result = StorageMediaAttachResultUnchanged;
            if ( ERROR_SUCCESS == m_pVolume->MediaChangeEvent ( 
                StorageMediaChangeEventAttached, &Result ) &&
                ( StorageMediaAttachResultChanged == Result ) ) {
                // The volume has indicated that its view of the partition is not
                // the same, so the partitions do not compare.
                lResult = ERROR_UNRECOGNIZED_VOLUME;
            }
        }
    }

    return ( ERROR_SUCCESS == lResult );
}

#ifdef UNDER_CE
//Only for CE
//Perform IOControl, uses either the default or direct short circuit route based on Mount State.
BOOL PartitionDisk_t::DoIOControl( DWORD IoControlCode, void* pInBuf, DWORD InBufSize, 
                void* pOutBuf, DWORD OutBufSize, DWORD* pBytesReturned, OVERLAPPED* pOverlapped )
{
    BOOL bRet = TRUE;

    //If the partition is not mounted, route the call to the store to handle it; else use the route through the kernel.
    if ( FALSE == IsMounted ( ) ){
        DEBUGMSG ( ZONE_APIS, ( L"FSDMGR!FSDMGR_DiskIoControl: Partition %x is not mounted; Using direct method on Store %x \n", this, m_pStore ) );

        BOOL bKernel = CeSetDirectCall(TRUE); // Direct calling from kernel.
        LRESULT lResult = m_pStore->DeviceIoControl( this, IoControlCode, pInBuf, InBufSize, pOutBuf, OutBufSize, pBytesReturned );
        CeSetDirectCall (bKernel);

        if ( ERROR_SUCCESS != lResult ) {
            SetLastError ( lResult );
            bRet = FALSE;
        }
    }else {
        //Let it go through the default route, to the STGAPI
        bRet = LogicalDisk_t::DoIOControl( IoControlCode, pInBuf, InBufSize, 
                    pOutBuf, OutBufSize, pBytesReturned, pOverlapped );
    }

    return bRet;
}
#endif

//Return TRUE if the disk is available; else FALSE
BOOL PartitionDisk_t::IsAvailable() 
{
    BOOL bAvailable = TRUE;

    //Try reading the disk directly. Failure means disk not exist.
    // We will try Disk Info IOCTL to confirm that the disk is indeed available.
    // DISK INFO IOCTLS are guranteed to be supported at store level.
    DISK_INFO di;
    DWORD dwRet = 0;
    memset( &di, 0, sizeof(DISK_INFO));
    if (!DeviceIoControl(m_pStore->m_hDisk, IOCTL_DISK_GETINFO, &di, sizeof(DISK_INFO), &di, sizeof(DISK_INFO), &dwRet, NULL))
    {
        if (!DeviceIoControl(m_pStore->m_hDisk, DISK_IOCTL_GETINFO, &di, sizeof(DISK_INFO), &di, sizeof(DISK_INFO), &dwRet, NULL))
        {
            // failed for DISK INFO IOCTLS means not available.
            bAvailable = FALSE;
        }
    }


    return bAvailable;
}

#endif /* !UNDER_CE */
