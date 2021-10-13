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

#include "udf.h"

// -----------------------------------------------------------------------------
// CVolume
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
// Note that for now we are assuming a 2048 sector size.  This is required for
// all current CDs and DVDs.  If we wish to support hard disks, we would have
// to configure this in the Initialize method.
//
CVolume::CVolume()
: m_hDsk( NULL ),
  m_hVol( NULL ),
  m_hHeap( NULL ),
  m_pMedia( NULL ),
  m_dwSectorSize( CD_SECTOR_SIZE ),
  m_dwShiftCount( 11 ),
  m_dwMediaType( MED_TYPE_UNKNOWN ),
  m_dwFirstSector( 0 ),
  m_dwLastSector( 0 ),
  m_dwFlags( 0 ),
  m_NSRVer( 0 ),
  m_pPartition( NULL ),
  m_pRoot( NULL ),
  m_fUnAdvertise( FALSE ),
  m_dwAllocSegmentsPerChunk( DEFAULT_ALLOC_SEGMENTS_PER_CHUNK )
{
    ZeroMemory( m_strDiskName, sizeof(m_strDiskName) );
    ZeroMemory( m_strVolumeName, sizeof(m_strVolumeName) );
    ZeroMemory( m_PartitionInfo, sizeof(m_PartitionInfo) );
    ZeroMemory( &m_FsdStart, sizeof(m_FsdStart) );
    ZeroMemory( &m_RootDir, sizeof(m_RootDir) );

    InitializeCriticalSection( &m_cs );
}

// /////////////////////////////////////////////////////////////////////////////
// Destructor
//
CVolume::~CVolume()
{
    if( m_pMedia )
    {
        delete m_pMedia;
    }

    for( int x = 0; x < 2; x++ )
    {
        if( m_PartitionInfo[x].pPhysicalPartition )
        {
            delete m_PartitionInfo[x].pPhysicalPartition;
        }

        if( m_PartitionInfo[x].pVirtualPartition )
        {
            delete m_PartitionInfo[x].pVirtualPartition;
        }
    }

    DeleteCriticalSection( &m_cs );
}

// /////////////////////////////////////////////////////////////////////////////
// Initialize
//
// Syncronization: Not needed at this point.  The volume should not be
// accessed by any thread until after it is initialized.
//
// Arguments:
//     hDsk - This is the storage driver below us.
//
// This method is used to
// a) Detect a UDF volume.
// b) Read in the data necessary to mount the volume.
//
BOOL CVolume::Initialize( HDSK hDsk )
{
    ASSERT( hDsk != NULL );

    DWORD dwReturned  = 0;
    DWORD dwValue     = 0;
    DWORD dwThreadId  = 0;
    DWORD dwThreadPri = 0;

    BOOL fResult     = TRUE;
    BOOL fRegistered = FALSE;

    LRESULT lResult = ERROR_SUCCESS;

    m_hDsk = hDsk;

    if( !ValidateAndInitMedia() )
    {
        fResult = FALSE;
        goto exit_initialize;
    }

    if( !m_FileManager.Initialize( this ) )
    {
        fResult = FALSE;
        goto exit_initialize;
    }

    if( !m_HandleManager.Initialize( this ) )
    {
        fResult = FALSE;
        goto exit_initialize;
    }

    //
    // Since sector sizes will be a multipe of 2, we can skip division and
    // just shift the values later on.
    //
    if( m_dwSectorSize != CD_SECTOR_SIZE )
    {
        DWORD dwSize = 1;
        m_dwShiftCount = 0;

        while( dwSize < m_dwSectorSize )
        {
            dwSize = dwSize << 1;
            m_dwShiftCount++;
        }
    }

    fResult = ValidateUDFVolume();
    if( !fResult )
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!Initialize: The volume does not appear to be UDF.\n")));

        fResult = FALSE;
        goto exit_initialize;
    }

    //
    // Now that we know the VRS and VDS are viable, read in the FSD and locate
    // the root directory and volume label.
    //
    fResult = ReadFSD( &m_RootDir );
    if( !fResult )
    {
        fResult = FALSE;
        goto exit_initialize;
    }

    //
    // We've got the root directory location, so read it in and add it to the
    // file manager.
    //
    m_pRoot = new (UDF_TAG_DIRECTORY) CDirectory();
    if( !m_pRoot )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        fResult = FALSE;
        goto exit_initialize;
    }

    lResult = m_pRoot->Initialize( this, m_RootDir.Start );
    if( lResult == ERROR_SUCCESS )
    {
        CFile* pBetterNotExist = NULL;

        lResult = m_FileManager.AddFile( m_pRoot, &pBetterNotExist );
        if( lResult != ERROR_SUCCESS )
        {
            SetLastError( lResult );
            fResult = FALSE;
            goto exit_initialize;
        }
    }
    else
    {
        delete m_pRoot;
        m_pRoot = NULL;
        SetLastError( lResult );
        fResult = FALSE;
        goto exit_initialize;
    }

    //
    // From what I can gather about this registry value:
    // If MountLabel == 1 Then Get The Mount String from the hDsk
    // else Get The Mount String using DISK_IOCTL_GETNAME which
    //      returns a static "CDROM DRIVE" or similar.
    //

    //
    // TODO::Come back and look at this later to make sure it's correct.
    //
    if( !FSDMGR_GetRegistryValue( m_hDsk,
                                  L"MountLabel",
                                  (LPDWORD)&dwValue ) )
    {
        dwValue = 0;
    }

    if( dwValue )
    {
        fResult = ::FSDMGR_GetDiskName( hDsk, m_strDiskName );
    }
    else
    {
        fResult = UDFSDeviceIoControl( DISK_IOCTL_GETNAME,
                                       NULL,
                                       0,
                                       (LPVOID)m_strDiskName,
                                       sizeof(m_strDiskName),
                                       &dwReturned,
                                       NULL );
    }


    if( fResult )
    {
        m_hVol = ::FSDMGR_RegisterVolume( m_hDsk,
                                          m_strDiskName,
                                          (PVOLUME)this );

        if( !m_hVol )
        {
            goto exit_initialize;
        }

        fRegistered = TRUE;

        if( FSDMGR_GetVolumeName( m_hVol, m_strVolumeName, MAX_PATH ) )
        {
            m_fUnAdvertise = FSDMGR_AdvertiseInterface( &UDFS_MOUNT_GUID,
                                                        m_strVolumeName,
                                                        TRUE);
        }

    }
    else
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!Initialize: FSDMGR_RegisterVolume failed (%d)\n"), GetLastError()));
        fRegistered = FALSE;
    }



exit_initialize:
    if( !fResult )
    {
        if( fRegistered )
        {
            ::FSDMGR_DeregisterVolume( m_hVol );
            m_hVol = NULL;
        }
    }

    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT(" Initialize Done\r\n")));

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// Deregister
//
BOOL CVolume::Deregister()
{
    return FSDMGR_AdvertiseInterface( &UDFS_MOUNT_GUID,
                                      m_strVolumeName,
                                      FALSE );
}

// /////////////////////////////////////////////////////////////////////////////
// CreateFile
//
LRESULT CVolume::CreateFile( HANDLE hProc,
                             const WCHAR* strFileName,
                             DWORD Access,
                             DWORD ShareMode,
                             LPSECURITY_ATTRIBUTES pSecurityAttributes,
                             DWORD Create,
                             DWORD FlagsAndAttributes,
                             PHANDLE pHandle )
{
    LRESULT lResult = ERROR_SUCCESS;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    CFileName FileName;
    CDirectory* pDir = NULL;
    CFileLink* pLink = NULL;
    CFile* pFile = NULL;
    PFILE_HANDLE pNewHandle = NULL;

    ASSERT( pHandle != NULL );

    //
    // GENERIC_READ and GENERIC_WRITE are currently the only supported options.
    //
    if( Access & ~(GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE) )
    {
        lResult = ERROR_INVALID_PARAMETER;
        goto exit_createfile;
    }

    //
    // FILE_SHARE_READ and FILE_SHARE_WRITE are currently the only supported
    // options.
    //
    if( ShareMode & ~(FILE_SHARE_READ | FILE_SHARE_WRITE) )
    {
        lResult = ERROR_INVALID_PARAMETER;
        goto exit_createfile;
    }

    if( Access & (GENERIC_WRITE | GENERIC_ALL) )
    {
        //
        // OK to ask for write but only in conjunction with GENERIC_ALL
        //
        lResult = ERROR_ACCESS_DENIED;
        goto exit_createfile;
    }

    //
    // Write support is not yet enabled.
    //
    switch( Create )
    {
    case CREATE_NEW:
    case CREATE_ALWAYS:
    case TRUNCATE_EXISTING:
        lResult = ERROR_ACCESS_DENIED;
        goto exit_createfile;

    case OPEN_EXISTING:
        break;

    case OPEN_ALWAYS:
        break;

    default:
        lResult = ERROR_INVALID_PARAMETER;
        goto exit_createfile;
    }

    if( !FileName.Initialize( strFileName ) )
    {
        lResult = GetLastError();
        goto exit_createfile;
    }

    //
    // The child file from this link will come back referenced.
    //
    pLink = ParsePath( m_pRoot, &FileName );
    if( !pLink )
    {
        lResult = GetLastError();
        goto exit_createfile;
    }

    pFile = pLink->GetChild();

    ASSERT( pFile != NULL );

    //
    // We've found a file object, so the most we should have left to do
    // is to find a stream.
    //
    if( FileName.HasStreamName() )
    {
        //
        // TODO::Implement stream support.
        //
        lResult = ERROR_NOT_SUPPORTED;
        goto exit_createfile;
    }

    //
    // If we actually found a file object, not a directory object, then
    // we need to be at the end of the file name, or it's an invalid file
    // name.
    //
    if( !FileName.IsFinished() )
    {
        lResult = ERROR_PATH_NOT_FOUND;
        goto exit_createfile;
    }

    pNewHandle = new(UDF_TAG_FILE_HANDLE) FILE_HANDLE;
    if( !pNewHandle )
    {
        lResult = ERROR_NOT_ENOUGH_MEMORY;
        goto exit_createfile;
    }

    pNewHandle->dwAccess = Access;
    pNewHandle->dwShareMode = ShareMode;
    pNewHandle->fOverlapped = FALSE;
    pNewHandle->pLink = pLink;
    pNewHandle->Position = 0;
    pNewHandle->pStream = pFile->GetMainStream();

    //
    // Now that we know what the file is, make sure that we can actually create
    // it with the requested access.
    //
    lResult = m_HandleManager.AddHandle( pNewHandle );
    if( lResult != ERROR_SUCCESS )
    {
        delete pNewHandle;
        pNewHandle = NULL;
    }

exit_createfile:
    if( lResult != ERROR_SUCCESS && pFile != NULL )
    {
        pFile->Dereference();
    }

    if( pNewHandle )
    {
        hFile = ::FSDMGR_CreateFileHandle( (HVOL)this,
                                           hProc,
                                           (PFILE)pNewHandle );
    }

    //
    // I guess this tells the user that the file was already there.  Not a bad
    // idea really.
    //
    if( (Create == OPEN_ALWAYS) && (lResult == ERROR_SUCCESS) )
    {
        lResult = ERROR_ALREADY_EXISTS;
    }

    *pHandle = hFile;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// CloseFile
//
LRESULT CVolume::CloseFile ( PFILE_HANDLE pHandle, BOOL Flush )
{
    LRESULT lResult = ERROR_SUCCESS;

    if( Flush )
    {
        if( (pHandle->dwAccess & GENERIC_WRITE) == 0 )
        {
            lResult = ERROR_ACCESS_DENIED;
            goto exit_closefile;
        }

        //
        // TODO::Add flush support for write.
        //
        lResult = ERROR_CALL_NOT_IMPLEMENTED;
        goto exit_closefile;
    }

    lResult = m_HandleManager.RemoveHandle( pHandle );

exit_closefile:

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// FsIoControl
//
// This function accepts specific FSCTLs.
//
LRESULT CVolume::FsIoControl( DWORD IoControlCode,
                              LPVOID pInBuf,
                              DWORD InBufSize,
                              LPVOID pOutBuf,
                              DWORD OutBufSize,
                              LPDWORD pBytesReturned,
                              LPOVERLAPPED pOverlapped )
{
    LRESULT Result = ERROR_SUCCESS;

    switch( IoControlCode )
    {
    case FSCTL_STORAGE_MEDIA_CHANGE_EVENT:
    {
        STORAGE_MEDIA_CHANGE_EVENT_TYPE* pEventType = (STORAGE_MEDIA_CHANGE_EVENT_TYPE*)pInBuf;
        STORAGE_MEDIA_ATTACH_RESULT* pResult = (STORAGE_MEDIA_ATTACH_RESULT*)pOutBuf;
        if( !pInBuf || !pOutBuf )
        {
            return ERROR_INVALID_PARAMETER;
        }

        if(  InBufSize < sizeof(STORAGE_MEDIA_CHANGE_EVENT_TYPE) )
        {
            return ERROR_INVALID_PARAMETER;
        }

        if( OutBufSize < sizeof(STORAGE_MEDIA_ATTACH_RESULT) )
        {
            return ERROR_INVALID_PARAMETER;
        }

        if( *pEventType == StorageMediaChangeEventDetached )
        {
        }
        else if( *pEventType == StorageMediaChangeEventAttached )
        {
            if( RemountMedia() )
            {
                __try
                {
                    *pResult = StorageMediaAttachResultUnchanged;
                }
                __except( EXCEPTION_EXECUTE_HANDLER )
                {
                    return ERROR_INVALID_PARAMETER;
                }
            }
            else
            {
                __try
                {
                    //
                    // This disc has changed and all handles are invalid.
                    //
                    *pResult = StorageMediaAttachResultChanged;
                }
                __except( EXCEPTION_EXECUTE_HANDLER )
                {
                    return ERROR_INVALID_PARAMETER;
                }
            }
        }
        else
        {
            return ERROR_INVALID_PARAMETER;
        }

        break;
    }

    default:
        Result = ERROR_NOT_SUPPORTED;
    }

    return Result;
}

// /////////////////////////////////////////////////////////////////////////////
// DeviceIoControl
//
// This function accepts IOCTLs from a client and either handles the
// functionality here or passes the IOCTL to a lower level driver to handle.
//
LRESULT CVolume::DeviceIoControl( DWORD IoControlCode,
                                  LPVOID pInBuf,
                                  DWORD InBufSize,
                                  LPVOID pOutBuf,
                                  DWORD OutBufSize,
                                  LPDWORD pBytesReturned,
                                  LPOVERLAPPED pOverlapped )
{
    LRESULT Result = ERROR_SUCCESS;

//    switch( IoControlCode )
//    {
        //
        // TODO::Are there any specific IOCTLs that we should be adding here?
        //

//    default:

        //
        // Pass the control down to the device and let it handle it.
        //
        if( !UDFSDeviceIoControl( IoControlCode,
                                  pInBuf,
                                  InBufSize,
                                  pOutBuf,
                                  OutBufSize,
                                  pBytesReturned,
                                  NULL ) )
        {
            Result = GetLastError();
        }

//    }

    return Result;
}


// /////////////////////////////////////////////////////////////////////////////
// GetVolumeInfo
//
// The input has been buffered by the outer layer and does not need to be
// checked here as well.
//
LRESULT CVolume::GetVolumeInfo( FSD_VOLUME_INFO *pInfo )
{
    LRESULT Result = ERROR_SUCCESS;

    //
    // TODO:Is the file system name already added by the file system manager?
    //

    pInfo->cbSize = sizeof( FSD_VOLUME_INFO );
    pInfo->dwAttributes = FSD_ATTRIBUTE_READONLY;
    pInfo->dwBlockSize = m_dwSectorSize;
    CopyMemory( pInfo->szFSDDesc, L"UDF", sizeof( L"UDF" ) );

    return Result;
}


// /////////////////////////////////////////////////////////////////////////////
// FindFirstFile
//
LRESULT CVolume::FindFirstFile( HANDLE hProc,
                                PCWSTR FileSpec,
                                PWIN32_FIND_DATAW pFindData,
                                PHANDLE pHandle )
{
    LRESULT lResult = ERROR_SUCCESS;
    CFileName FileName;
    CFileName FoundName;
    CFileLink* pLink = NULL;
    CDirectory* pDir = m_pRoot;
    PSEARCH_HANDLE  pNewHandle = NULL;

    NSR_FID Fid = { 0 };
    CStream* pStream = NULL;
    BY_HANDLE_FILE_INFORMATION HandleInfo = { 0 };

    if( !FileName.Initialize( FileSpec, TRUE ) )
    {
        lResult = GetLastError();
        goto exit_findfirstfile;
    }

    //
    // We don't enumerate streams right now.
    // TODO::We could allow stream enumeration in the future.
    //
    if( FileName.HasStreamName() )
    {
        lResult = ERROR_BAD_PATHNAME;
        goto exit_findfirstfile;
    }

    //
    // When we're parsing the file name, we want to stop at the file name so
    // that the parent directory is returned to us.
    //
    if( !FileName.SetStopIndex( FileName.GetFileTokenIndex() ) )
    {
        ASSERT(FALSE);
        lResult = ERROR_PATH_NOT_FOUND;
        goto exit_findfirstfile;
    }

    //
    // TODO::Reference the volume to keep it alive.
    //

    //
    // We've truncated the filename to stop before the file name.  So this
    // should give us the link to the directory in which we are searching.
    // Note that the child file will come back referenced from this function.
    //
    pLink = ParsePath( m_pRoot, &FileName, TRUE );

    if( !pLink && FileName.GetTokenCount() > 1 )
    {
        lResult = ERROR_FILE_NOT_FOUND;
        goto exit_findfirstfile;
    }

    if( pLink )
    {
        CFile* pTmp = pLink->GetChild();

        if( pTmp->IsDirectory() == FALSE )
        {
            pTmp->Dereference();
            lResult = ERROR_BAD_PATHNAME;
            goto exit_findfirstfile;
        }

        pDir = (CDirectory*)pTmp;
    }
    else
    {
        //
        // We will be using the root directory in the handle, so reference it
        // here.
        //
        pDir->Reference();
    }

    //
    // Now that we have a parent directory, we will want to find all the
    // matching children now. Note that we should currently be at the file
    // index.
    //
    ASSERT( FileName.GetCurrentTokenIndex() == FileName.GetFileTokenIndex() );
    FileName.SetStopIndex( FileName.GetTokenCount() );

    pNewHandle = new(UDF_TAG_SEARCH_HANDLE) SEARCH_HANDLE;
    if( !pNewHandle )
    {
        lResult = ERROR_NOT_ENOUGH_MEMORY;
        goto exit_findfirstfile;
    }

    ZeroMemory( pNewHandle, sizeof(SEARCH_HANDLE) );

    pNewHandle->pFileName = new(UDF_TAG_FILENAME) CFileName;
    if( !pNewHandle->pFileName )
    {
        lResult = ERROR_NOT_ENOUGH_MEMORY;
        goto exit_findfirstfile;
    }

    if( !pNewHandle->pFileName->Initialize( FileName.GetCurrentToken(),
                                            TRUE ) )
    {
        lResult = GetLastError();
        goto exit_findfirstfile;
    }

    //
    // Save the volume and the stream from the directory in which we are
    // enumerating.  It is not necessary to store the volume, and it could
    // be replaced with other information.  There is a check to make sure
    // this is a valid kernel address when calling FindNextFile, so make
    // sure to either change this check or keep a kernel address in here.
    //
    pNewHandle->pStream = pDir->GetMainStream();
    pNewHandle->pVolume = this;

    //
    // Get the the first file name and the location of the ICB.
    //
    lResult = pDir->FindFileName( 0,
                                  &FileName,
                                  pFindData->cFileName,
                                  sizeof(pFindData->cFileName) / sizeof(WCHAR),
                                  &Fid,
                                  &pNewHandle->Position );
    if( lResult != ERROR_SUCCESS )
    {
        goto exit_findfirstfile;
    }

    if( !FoundName.Initialize( pFindData->cFileName ) )
    {
        lResult = GetLastError();
        goto exit_findfirstfile;
    }

    //
    // Open the file and get the attributes.  Remember that this will be
    // referenced and we will need to dereference it.
    //
    pLink = ParsePath( pDir, &FoundName, TRUE );
    if( !pLink )
    {
        lResult = GetLastError();
        goto exit_findfirstfile;
    }

    pStream = pLink->GetChild()->GetMainStream();

    lResult = pStream->GetFileInformation( pLink, &HandleInfo );
    if( lResult != ERROR_SUCCESS )
    {
        goto exit_findfirstfile;
    }

    pFindData->dwFileAttributes = HandleInfo.dwFileAttributes;
    pFindData->ftCreationTime = HandleInfo.ftCreationTime;
    pFindData->ftLastAccessTime = HandleInfo.ftLastAccessTime;
    pFindData->ftLastWriteTime = HandleInfo.ftLastWriteTime;
    pFindData->nFileSizeHigh = HandleInfo.nFileSizeHigh;
    pFindData->nFileSizeLow = HandleInfo.nFileSizeLow;
    pFindData->dwOID = 0;

    pNewHandle->fOverlapped = FALSE;
    pNewHandle->dwAccess = GENERIC_READ;
    pNewHandle->dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;

    lResult = m_HandleManager.AddHandle( pNewHandle );
    if( lResult != ERROR_SUCCESS )
    {
        goto exit_findfirstfile;
    }

    *pHandle = ::FSDMGR_CreateSearchHandle( (HVOL)this,
                                            hProc,
                                            (PSEARCH)pNewHandle );

    pDir = NULL; // NULL the directory pointer so we don't dereference it

exit_findfirstfile:
    if( lResult != ERROR_SUCCESS )
    {
        if( pNewHandle )
        {
            if( pNewHandle->pFileName )
            {
                delete pNewHandle->pFileName;
            }

            delete pNewHandle;
        }
    }

    if( pDir )
    {
        pDir->Dereference();
    }

    if( pStream )
    {
        pStream->GetFile()->Dereference();
    }

    return lResult;
}


// /////////////////////////////////////////////////////////////////////////////
// CreateDirectory
//
LRESULT CVolume::CreateDirectory( PCWSTR PathName, LPSECURITY_ATTRIBUTES pSecurityAttributes )
{
    LRESULT Result = ERROR_ACCESS_DENIED;

    //
    // TODO::Check that we limit the total number of diretories to USHORT_MAX - 1.
    // This is because we have to keep track of the link count to the parent
    // directory, which is limited to a 16-bit value.
    //

    return Result;
}


// /////////////////////////////////////////////////////////////////////////////
// RemoveDirectory
//
LRESULT CVolume::RemoveDirectory( PCWSTR PathName )
{
    LRESULT Result = ERROR_ACCESS_DENIED;

    return Result;
}


// /////////////////////////////////////////////////////////////////////////////
// GetFileAttributes
//
LRESULT CVolume::GetFileAttributes( PCWSTR strFileName, PDWORD pAttributes )
{
    LRESULT lResult = ERROR_SUCCESS;
    CFileName FileName;
    CFileLink* pLink = NULL;
    CStream* pStream = NULL;

    ASSERT( pAttributes );
    ASSERT( strFileName );

    if( !FileName.Initialize( strFileName ) )
    {
        lResult = GetLastError();
        goto exit_getfileattributes;
    }

    //
    //  The link will come back referenced.
    //
    pLink = ParsePath( m_pRoot, &FileName, TRUE );

    if( !pLink )
    {
        lResult = ERROR_FILE_NOT_FOUND;
        goto exit_getfileattributes;
    }

    pStream = pLink->GetChild()->GetMainStream();

    *pAttributes = pStream->GetFileAttributes( pLink );

    pLink->GetChild()->Dereference();

exit_getfileattributes:

    return lResult;
}


// /////////////////////////////////////////////////////////////////////////////
// SetFileAttributes
//
LRESULT CVolume::SetFileAttributes( PCWSTR FileName, DWORD Attributes )
{
    LRESULT Result = ERROR_ACCESS_DENIED;

    return Result;
}


// /////////////////////////////////////////////////////////////////////////////
// DeleteFile
//
LRESULT CVolume::DeleteFile( PCWSTR FileName )
{
    LRESULT Result = ERROR_ACCESS_DENIED;

    return Result;
}


// /////////////////////////////////////////////////////////////////////////////
// MoveFile
//
LRESULT CVolume::MoveFile( PCWSTR OldFileName, PCWSTR NewFileName )
{
    LRESULT Result = ERROR_ACCESS_DENIED;

    return Result;
}


// /////////////////////////////////////////////////////////////////////////////
// DeleteAndRenameFile
//
LRESULT CVolume::DeleteAndRenameFile( PCWSTR OldFile, PCWSTR NewFile )
{
    LRESULT Result = ERROR_ACCESS_DENIED;

    return Result;
}


// /////////////////////////////////////////////////////////////////////////////
// GetDiskFreeSpace
//
LRESULT CVolume::GetDiskFreeSpace( PDWORD pSectorsPerCluster, PDWORD pBytesPerSector, PDWORD pFreeClusters, PDWORD pClusters )
{
    LRESULT Result = ERROR_CALL_NOT_IMPLEMENTED;
    __try
    {
        *pSectorsPerCluster = 1;
        *pBytesPerSector = m_dwSectorSize;
        *pClusters = m_dwLastSector - m_dwFirstSector;

        //
        // TODO: Once write supporte is enabled, this value needs to be non-zero.
        //
        *pFreeClusters = 0;

        Result = ERROR_SUCCESS;
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        Result = ERROR_INVALID_PARAMETER;
    }

    return Result;
}

// /////////////////////////////////////////////////////////////////////////////
// GetPhysicalReference
//
Uint16 CVolume::GetPhysicalReference( CPhysicalPartition* pPart ) const
{
    for( int x = 0; x < 2; x++ )
    {
        if( m_PartitionInfo[x].pPhysicalPartition == pPart )
        {
            return m_PartitionInfo[x].pPhysicalPartition->GetReferenceNumber();
        }
    }

    return INVALID_REFERENCE;
}

// /////////////////////////////////////////////////////////////////////////////
// ValidateUDFVolume
//
// This method checks to see if we're trying to mount a UDF volume or not.
// The checks in place are as follows:
// 1) VRS is correct
//    a) Contains BEA01
//    b) Contains NSR0x ( where x = 1, 2, 3 )
//    c) Contains TEA01
// 2) AVDP is found at either sector 256, 512, N, or N-256 (2.2.3)
// 3) VDS contains PVD, LVD, LVID, PD, IUVD, USD
//
BOOL CVolume::ValidateUDFVolume()
{
    ASSERT(m_hDsk != NULL);

    BOOL  fResult = TRUE;

    //
    // Try to find the first sector and last sector of the session from which
    // we can read.  GetSessionBounds will look for a VRS in each of the
    // tracks in the session.
    //
    fResult = GetSessionBounds( &m_dwFirstSector, &m_dwLastSector );
    if( !fResult )
    {
        goto Exit_ValidateUDFVolume;
    }

    //
    // We've found a VRS indicating a UDF volume.
    //
    fResult = ValidateVDS();
    if( !fResult )
    {
        goto Exit_ValidateUDFVolume;
    }

    //Quick Sanity Check on FSD location
    if( m_FsdStart.Start.Lbn > m_dwLastSector )
    {
       RETAILMSG( 1,( TEXT( "*******Sanity Check FSD Failed!!\r\n" ) ) );
       fResult = FALSE;
       goto Exit_ValidateUDFVolume;
    }

    //
    // At this point we should have the information needed to read in the file
    // set descriptor to get the root directory.
    //

Exit_ValidateUDFVolume:
    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// DetermineMediaType
//
BOOL CVolume::DetermineMediaType( DWORD* pdwMediaType )
{
    ASSERT( pdwMediaType != NULL );
    DWORD dwBytesReturned = 0;
    STORAGEDEVICEINFO DeviceInfo = { 0 };

    *pdwMediaType = MED_TYPE_UNKNOWN;
    DeviceInfo.cbSize = sizeof(STORAGEDEVICEINFO);

    if( !UDFSDeviceIoControl( IOCTL_DISK_DEVICE_INFO,
                              &DeviceInfo,
                              sizeof(DeviceInfo),
                              &DeviceInfo,
                              sizeof(DeviceInfo),
                              &dwBytesReturned,
                              NULL ) )
    {
        return FALSE;
    }

    if( DeviceInfo.dwDeviceClass == STORAGE_DEVICE_CLASS_MULTIMEDIA )
    {
        if( !UDFSDeviceIoControl( IOCTL_CDROM_GET_MEDIA_TYPE,
                                  NULL,
                                  0,
                                  pdwMediaType,
                                  sizeof(*pdwMediaType),
                                  &dwBytesReturned,
                                  NULL ) )
        {
            return FALSE;
        }
    }
    else
    {
        *pdwMediaType = MED_TYPE_DISK;
    }

    return TRUE;
}

// /////////////////////////////////////////////////////////////////////////////
// GetSessionBounds
//
BOOL CVolume::GetSessionBounds( DWORD* pdwFirstSector, DWORD* pdwLastSector )
{
    BOOL fResult = FALSE;
    DISC_INFORMATION DiscInfo = { 0 };
    DWORD dwBytesReturned = 0;
    BOOL fFirstTrackIsEmpty = FALSE;
    DISC_INFO di = DI_SCSI_INFO;

    PCD_PARTITION_INFO pPartInfo = NULL;
    DWORD dwBufferSize = 0;

    STORAGEDEVICEINFO DeviceInfo = { 0 };
    DeviceInfo.cbSize = sizeof(STORAGEDEVICEINFO);
    if( !UDFSDeviceIoControl( IOCTL_DISK_DEVICE_INFO,
                              &DeviceInfo,
                              sizeof(DeviceInfo),
                              &DeviceInfo,
                              sizeof(DeviceInfo),
                              &dwBytesReturned,
                              NULL ) )
    {
        return FALSE;
    }

    if( DeviceInfo.dwDeviceClass != STORAGE_DEVICE_CLASS_MULTIMEDIA )
    {
        PARTINFO PartInfo = { 0 };
        PartInfo.cbSize = sizeof(PartInfo);
        DWORD dwResult = FSDMGR_GetPartInfo( m_hDsk, &PartInfo );
        if( dwResult != ERROR_SUCCESS )
        {
            SetLastError( dwResult );
            return FALSE;
        }

        if( PartInfo.snNumSectors > ULONG_MAX )
        {
            //
            // TODO::We should support ulonglong sectors.
            //
            SetLastError( ERROR_NOT_SUPPORTED );
            return FALSE;
        }

        *pdwFirstSector = 0;
        *pdwLastSector = (DWORD)PartInfo.snNumSectors - 1;

        return TRUE;
    }

    dwBufferSize = sizeof(CD_PARTITION_INFO) +
                   MAXIMUM_NUMBER_TRACKS_LARGE * sizeof(TRACK_INFORMATION2);

    pPartInfo = (PCD_PARTITION_INFO)new(UDF_TAG_BYTE_BUFFER) BYTE[ dwBufferSize ];
    if( !pPartInfo )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return FALSE;
    }

    fResult = UDFSDeviceIoControl( IOCTL_CDROM_GET_PARTITION_INFO,
                                   NULL,
                                   0,
                                   pPartInfo,
                                   dwBufferSize,
                                   &dwBytesReturned,
                                   NULL );
    if( !fResult )
    {
        //
        // The last error should have been set when calling
        // into the file system manager.
        //
        goto exit_getmediabounds;
    }

    *pdwLastSector = 0;

    for( USHORT Track = pPartInfo->NumberOfTracks; Track; Track-- )
    {
        PTRACK_INFORMATION2 pTrackInfo = &pPartInfo->TrackInformation[Track - 1];
        USHORT TrackNumber = pTrackInfo->TrackNumberLsb +
                            (pTrackInfo->TrackNumberMsb << 8);
        ULONG FreeBlocks = 0;

        *pdwFirstSector = (pTrackInfo->TrackStartAddress[0] << 24) +
                          (pTrackInfo->TrackStartAddress[1] << 16) +
                          (pTrackInfo->TrackStartAddress[2] << 8) +
                          (pTrackInfo->TrackStartAddress[3]);

        if( !*pdwLastSector )
        {
            *pdwLastSector = (pTrackInfo->TrackSize[0] << 24) +
                             (pTrackInfo->TrackSize[1] << 16) +
                             (pTrackInfo->TrackSize[2] << 8) +
                             (pTrackInfo->TrackSize[3]);

            //
            // Offset from the beginning of the track;
            //
            *pdwLastSector += *pdwFirstSector;

        }

        FreeBlocks = (pTrackInfo->FreeBlocks[0] << 24) +
                     (pTrackInfo->FreeBlocks[1] << 16) +
                     (pTrackInfo->FreeBlocks[2] << 8) +
                     (pTrackInfo->FreeBlocks[3]);

        //
        // We need to find which (if any) of the tracks contains the VRS.
        //
        fResult = ReadVRS( *pdwFirstSector + (32 * 1024 / m_dwSectorSize),
                           &m_NSRVer );
        if( !fResult )
        {
            continue;
        }

        if( FreeBlocks &&
            FreeBlocks != 0xFFFFFFFF )
        {
            *pdwLastSector -= FreeBlocks;

            //
            // Find the last written address and subtract 7 blocks
            // for run-in/out.  This will only be on opened CDs.
            //
            // Note that this should only happen on open media as
            // this is where we will see the free blocks in a track.
            // I don't think that we will be able to read most of
            // this CD media.  I've experimented with different
            // devices and with Windows Vista and it is not unusual if two
            // devices can't read open media written by the other.
            //
            if( m_dwMediaType & MED_FLAG_CD )
            {
                *pdwLastSector -= 7;
            }
        }

        //
        // Starting at block 0, not 1.
        //
        *pdwLastSector -= 1;

        //
        // We've gone through a lot of trouble to get the last sector on the
        // media I know.  However, some DVD players offer this nice piece of
        // information to let us know the last recorded address.
        //
        if( m_dwMediaType & MED_FLAG_DVD &&
            pTrackInfo->LRA_V )
        {
            *pdwLastSector = (pTrackInfo->LastRecordedAddress[0] << 24) +
                             (pTrackInfo->LastRecordedAddress[1] << 16) +
                             (pTrackInfo->LastRecordedAddress[2] << 8) +
                             (pTrackInfo->LastRecordedAddress[3]);
        }

        fResult = TRUE;
        break;
    }

exit_getmediabounds:

    if( pPartInfo )
    {
        delete [] pPartInfo;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetClosedMediaBounds
//
// This method discovers the bounds for media that has been closed (the last
// session is empty or the disk is closed).
//
BOOL CVolume::GetClosedMediaBounds( __out DWORD* pdwFirstSector,
                                    __out DWORD* pdwLastSector )
{
    ASSERT( pdwFirstSector != NULL );
    ASSERT( pdwLastSector != NULL );

    //
    // The disk is in a closed state.  When this happens, we read the TOC
    // to get the track data.  The last track, specified as track 'AA'
    // is the lead-out area of the last session.  Using this starting address,
    // we can find the last sector of the program area of the last session.
    // NOTE: The other option here is to call the READ CAPACITY command.  However,
    // when doing this, it is important for MRW media to make sure the LBA
    // Space bit of the MRW mode page is zero.
    //
    BOOL fResult = TRUE;
    DWORD dwBytesReturned = 0;

    USHORT BufferSize = 0;
    PSCSI_PASS_THROUGH pPassThrough = NULL;
    CDB* pCDB = NULL;
    CDROM_TOC_LARGE* pToc = NULL;
    TRACK_DATA* pTrackData = NULL;
    USHORT usLength = 0;

    BufferSize = sizeof(SCSI_PASS_THROUGH) +
                 sizeof(CDROM_TOC_LARGE);

    pPassThrough = (PSCSI_PASS_THROUGH)new(UDF_TAG_PASS_THROUGH) BYTE[BufferSize];
    if( !pPassThrough )
    {
        fResult = FALSE;
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!GetMediaBounds unable to allocate TOC buffer")));
        goto CLEANUP_TOC_AND_RETURN;
    }

    ZeroMemory( pPassThrough, BufferSize );

    pPassThrough->CdbLength = 10;
    pPassThrough->DataBufferOffset = sizeof(SCSI_PASS_THROUGH);
    pPassThrough->DataIn = 1;
    pPassThrough->DataTransferLength = sizeof(CDROM_TOC_LARGE);
    pPassThrough->Length = BufferSize;
    pPassThrough->TimeOutValue = 50;

    pCDB = (PCDB)pPassThrough->Cdb;
    pCDB->READ_TOC.OperationCode = SCSIOP_READ_TOC;
    pCDB->READ_TOC.AllocationLength[0] = (UCHAR)(sizeof(CDROM_TOC_LARGE) >> 8);
    pCDB->READ_TOC.AllocationLength[1] = (UCHAR)(sizeof(CDROM_TOC_LARGE) & 0xFF);

    //
    // Note that the Windows driver tries to do MSF addressing if the LBA
    // addressing fails.  Apparently some devices don't support LBA
    // correctly.  At this time I'm not going to try that unless we find
    // devices not working under CE.
    //
    fResult = UDFSDeviceIoControl( IOCTL_SCSI_PASS_THROUGH,
                                   pPassThrough,
                                   BufferSize,
                                   pPassThrough,
                                   BufferSize,
                                   &dwBytesReturned,
                                   NULL );

    if( !fResult )
    {
        goto CLEANUP_TOC_AND_RETURN;
    }

    //
    // TODO::Add some sanity checking on the information returned from the
    // device?
    //


    pToc = (PCDROM_TOC_LARGE)(pPassThrough + 1);

    //
    // If we were able to get the track information and there was at least
    // two tracks returned (a track and a lead out), then find the last
    // sector number.
    //
    usLength = pToc->Length[0] << 8 | pToc->Length[1];
    if( usLength < (2 * sizeof(TRACK_DATA)) )
    {
        fResult = FALSE;
        goto CLEANUP_TOC_AND_RETURN;
    }

    pTrackData = &pToc->TrackData[ pToc->LastTrack - pToc->FirstTrack + 1 ];

    SwapCopyUchar4( pdwLastSector, pTrackData->Address );

    if( *pdwLastSector != 0 )
    {
        *pdwLastSector -= 1;
    }

/*
    //
    // Apparently some DVD drives will always return the value 0x6dd39.
    // This doesn't match the MMC spec, but then few firmwares actually do.
    //
    if( m_dwLastSector == 0x6dd39 )
    {
        //
        // We may want to try the READ CAPACITY command if we're seeing
        // this hit.
        //
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!GetMediaBounds is getting 0x6dd39.  Maybe try READ CAPACITY.")));
        fResult = FALSE;
        goto CLEANUP_TOC_AND_RETURN;
    }
*/

    //
    // Now get the starting address of the last session.
    //
    pCDB->READ_TOC.Format = 1;
    pCDB->READ_TOC.Format2 = 1;

    fResult = UDFSDeviceIoControl( IOCTL_SCSI_PASS_THROUGH,
                                   pPassThrough,
                                   BufferSize,
                                   pPassThrough,
                                   BufferSize,
                                   &dwBytesReturned,
                                   NULL );

    if( !fResult )
    {
        goto CLEANUP_TOC_AND_RETURN;
    }

    usLength = pToc->Length[0] << 8 | pToc->Length[1];
    if( usLength != (2 + sizeof(TRACK_DATA)) )
    {
        fResult = FALSE;
        goto CLEANUP_TOC_AND_RETURN;
    }

    pTrackData = &pToc->TrackData[0];
    SwapCopyUchar4( pdwFirstSector, pTrackData->Address );

    if( pdwLastSector <= pdwFirstSector )
    {
        fResult = FALSE;
    }

CLEANUP_TOC_AND_RETURN:
    if( pPassThrough )
    {
        delete [] pPassThrough;
        pPassThrough = NULL;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetOpenMediaBounds
//
// This method discovers the bounds for media where the disc is not closed and
// the last session
//
BOOL CVolume::GetOpenMediaBounds( __out BOOL* pfFirstTrackIsEmpty,
                                  const DISC_INFORMATION& DiscInfo,
                                  __out DWORD* pdwFirstSector,
                                  __out DWORD* pdwLastSector )
{
    BOOL fResult = FALSE;
    SCSI_PASS_THROUGH_DIRECT PassThroughDirect = { 0 };
    CDB& Cdb = *((PCDB)PassThroughDirect.Cdb);
    BYTE* pBuffer = NULL;
    PTRACK_INFORMATION2 pTrackInfo = NULL;
    DWORD dwBytesReturned = 0;
    USHORT LastTrack = 0;
    USHORT FirstTrack = 0;

    ASSERT(pfFirstTrackIsEmpty != NULL);

    pBuffer = new (UDF_TAG_BYTE_BUFFER) BYTE[ GetSectorSize() ];
    if( !pBuffer )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        fResult = FALSE;
        goto Exit_GetOpenMediaBounds;
    }

    pTrackInfo = (PTRACK_INFORMATION2)pBuffer;

    *pfFirstTrackIsEmpty = FALSE;
    ZeroMemory( &Cdb, sizeof(Cdb) );
    ZeroMemory( pTrackInfo, GetSectorSize() );

    Cdb.READ_TRACK_INFORMATION.OperationCode = SCSIOP_READ_TRACK_INFORMATION;
    Cdb.READ_TRACK_INFORMATION.Track = 1;
    Cdb.READ_TRACK_INFORMATION.BlockAddress[2] = DiscInfo.LastSessionLastTrackMsb;
    Cdb.READ_TRACK_INFORMATION.BlockAddress[3] = DiscInfo.LastSessionLastTrackLsb;
    Cdb.READ_TRACK_INFORMATION.AllocationLength[0] = (UCHAR)((GetSectorSize() >> 8) & 0xFF);
    Cdb.READ_TRACK_INFORMATION.AllocationLength[1] = (UCHAR)(GetSectorSize() & 0xFF);

    PassThroughDirect.CdbLength = 10;
    PassThroughDirect.DataBuffer = pTrackInfo;
    PassThroughDirect.DataIn = 1;
    PassThroughDirect.DataTransferLength = GetSectorSize();
    PassThroughDirect.Length = sizeof(PassThroughDirect);
    PassThroughDirect.TimeOutValue = 50;

    fResult = UDFSDeviceIoControl( IOCTL_SCSI_PASS_THROUGH_DIRECT,
                                   &PassThroughDirect,
                                   sizeof(PassThroughDirect),
                                   &PassThroughDirect,
                                   sizeof(PassThroughDirect),
                                   &dwBytesReturned,
                                   NULL );
    if( !fResult )
    {
        goto Exit_GetOpenMediaBounds;
    }

    //
    // TODO::Repair damaged track.
    //

    LastTrack = DiscInfo.LastSessionLastTrackMsb << 8 | DiscInfo.LastSessionLastTrackLsb;
    FirstTrack = DiscInfo.LastSessionFirstTrackMsb << 8 | DiscInfo.LastSessionFirstTrackLsb;

    //
    // If the only track in the last session is empty, then leave and try the
    // TOC method to determine the disc capacity.
    //
    if( pTrackInfo->Blank )
    {
        if( LastTrack == FirstTrack )
        {
            *pfFirstTrackIsEmpty = TRUE;
            fResult = FALSE;
            goto Exit_GetOpenMediaBounds;
        }
    }

    if( pTrackInfo->Damage || pTrackInfo->FixedPacket || pTrackInfo->ReservedTrack ||
        !(pTrackInfo->LRA_V || pTrackInfo->NWA_V) )
    {
        SetLastError( ERROR_INVALID_MEDIA );
        fResult = FALSE;
        goto Exit_GetOpenMediaBounds;
    }

    if( pTrackInfo->LRA_V && (m_dwMediaType & MED_FLAG_DVD) )
    {
        //
        // Use the last recorded address.  This should only be used for
        // DVD-R/RW according to the MMC 5 specification 6.33.3.17.
        //
        SwapCopyUchar4(pdwLastSector, pTrackInfo->LastRecordedAddress);
    }
    else
    {
        //
        // Use the next writable address - 1.
        //
        SwapCopyUchar4(pdwLastSector, pTrackInfo->NextWritableAddress);
        *pdwLastSector -= 1;
    }

    //
    // Now we need the first track in order to get the starting sector for the
    // volume.
    //
    if( LastTrack != FirstTrack )
    {
        Cdb.READ_TRACK_INFORMATION.BlockAddress[3] = DiscInfo.LastSessionFirstTrackMsb;
        Cdb.READ_TRACK_INFORMATION.BlockAddress[2] = DiscInfo.LastSessionFirstTrackLsb;

        fResult = UDFSDeviceIoControl( IOCTL_SCSI_PASS_THROUGH,
                                       &PassThroughDirect,
                                       sizeof(PassThroughDirect),
                                       &PassThroughDirect,
                                       sizeof(PassThroughDirect),
                                       &dwBytesReturned,
                                       NULL );
        if( !fResult )
        {
            goto Exit_GetOpenMediaBounds;
        }
    }

    SwapCopyUchar4(pdwFirstSector, pTrackInfo->TrackStartAddress);

    if( GetMediaType() & MED_FLAG_CD )
    {
        //
        // CD Media have packets written like this (or in reverse)...
        //
        // ---------------------------------------------------------------------
        // | Link Block | 4 Run-in Blocks | User Data Blocks | 2 Run-out Blocks|
        // ---------------------------------------------------------------------
        //
        // In order to find the last user data block on opened media, we must
        // take into account the 2 run-out blocks, the link block, and the 4
        // run-in blocks for the newly linked block.  I think.  Why this is not
        // abstracted out by this layer, I don't know.  What a shame.  We've
        // already subtracted 1 (valid for DVD), now we need to subtract the
        // other six.
        //
        // Theoretically we could read in the block RAW and determine what type
        // of block it is.
        // Byte 12, bits [7,6,5] would be 000 for user data
        //                                001 for the 4th run-in
        //                                010 for the 3rd run-in
        //                                011 for the 2nd run-in
        //                                100 for the 1st run-in
        //                                101 for the link block
        //                                110 for the 2nd run-out
        //                                111 for the 1st run-out
        //
        if( *pdwLastSector > 10 )
        {
            *pdwLastSector -= 10;
        }

        /*
        if( pBuffer )
        {
            delete [] pBuffer;
        }

        //
        // A RAW sector is is 2352 bytes.
        //
        pBuffer = new BYTE[3000];
        if( !pBuffer )
        {
            goto Exit_GetOpenMediaBounds;
        }

        for( DWORD x = 0; m_dwLastSector && x < 10; x++ )
        {
            CDB Cdb;

            ZeroMemory( &Cdb, sizeof(Cdb) );

            m_dwLastSector -= 1;

            Cdb.READ_CD.OperationCode = SCSIOP_READ_CD;
            Cdb.READ_CD.StartingLBA[0] = (BYTE)(m_dwLastSector >> 24);
            Cdb.READ_CD.StartingLBA[1] = (BYTE)((m_dwLastSector >> 16) & 0xFF);
            Cdb.READ_CD.StartingLBA[2] = (BYTE)((m_dwLastSector >> 8) & 0xFF);
            Cdb.READ_CD.StartingLBA[3] = (BYTE)(m_dwLastSector & 0xFF);
            Cdb.READ_CD.TransferBlocks[2] = 1;
            Cdb.READ_CD.IncludeUserData = 1;
            Cdb.READ_CD.HeaderCode = 0;
            Cdb.READ_CD.IncludeSyncData = 0;

            if( UDFSDeviceIoControl( IOCTL_SCSI_PASS_THROUGH,
                                     &Cdb,
                                     sizeof(Cdb),
                                     pBuffer,
                                     3000,
                                     &dwBytesReturned,
                                     NULL ) )
            {
                DEBUGMSG(ZONE_INIT,(TEXT("UDFS!CVolume::GetOpenMediaBounds - Last block mode: %d\r\n"), pBuffer[12]));
            }

            ZeroMemory( &Cdb, sizeof(Cdb) );
            Cdb.CDB10.OperationCode = SCSIOP_READ;
            Cdb.CDB10.LogicalBlockByte0 = (BYTE)(m_dwLastSector >> 24);
            Cdb.CDB10.LogicalBlockByte1 = (BYTE)((m_dwLastSector >> 16) & 0xFF);
            Cdb.CDB10.LogicalBlockByte2 = (BYTE)((m_dwLastSector >> 8) & 0xFF);
            Cdb.CDB10.LogicalBlockByte3 = (BYTE)(m_dwLastSector & 0xFF);
            Cdb.CDB10.TransferBlocksMsb = 0;
            Cdb.CDB10.TransferBlocksLsb = 1;

            if( UDFSDeviceIoControl( IOCTL_SCSI_PASS_THROUGH,
                                     &Cdb,
                                     sizeof(Cdb),
                                     pBuffer,
                                     3000,
                                     &dwBytesReturned,
                                     NULL ) )
            {
                DEBUGMSG(ZONE_INIT,(TEXT("UDFS!CVolume::GetOpenMediaBounds - ReadCD failed, Read succeeded.\r\n")));
            }

        }
        */
    }

Exit_GetOpenMediaBounds:
    if( pBuffer )
    {
        delete [] pBuffer;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ReadVRS
//
// The volume recognition sequence (VRS) should be found at 32K into the disk.
// We need to find a BEA01 descriptor, an NSRx descriptor, and a TEA01
// descriptor.
//
// Note that this is pretty simple checking.  We could be much more stringent,
// but I'm not sure it would be helpful.  There aren't any other file systems
// that will match these conditions currently.
//
BOOL CVolume::ReadVRS( DWORD dwStartingSector, USHORT* pNSRVersion )
{
    ASSERT( pNSRVersion != NULL );

    const DWORD dwBuffSize = MAX_READ_SIZE;

    BOOL fResult = FALSE;
    BOOL fFoundBEA = FALSE;
    BOOL fFoundTEA = FALSE;
    BOOL fFoundNSR = FALSE;
    BYTE* pVRSBuffer = NULL;
    DWORD dwRead = 0;
    LRESULT lResult = ERROR_SUCCESS;

    pVRSBuffer = new (UDF_TAG_BYTE_BUFFER) BYTE[dwBuffSize];
    if( !pVRSBuffer )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return FALSE;
    }

    lResult = m_pMedia->ReadRange( dwStartingSector,
                                   0,
                                   dwBuffSize,
                                   pVRSBuffer,
                                   &dwRead );
    if( lResult != ERROR_SUCCESS )
    {
        fResult = FALSE;
        SetLastError( lResult );
        goto CLEANUP_BUFFER;
    }

    //
    // Note that we're re-using "dwRead" here to tell us how many
    // sectors we've read through the buffer.  We'll go through the
    // buffer as long as we haven't found the required descriptors
    // for UDF, or until we've hit the end of the buffer.
    //
    // Note that 16K should be enough for any VRS, but there's not really
    // a requirement saying it will be less than 16K.  Or, if a device
    // comes out with large sector sizes, this might not be enough.
    //
    dwRead = 0;
    while( (!fFoundBEA || !fFoundTEA || !fFoundNSR) &&
           (dwRead < dwBuffSize / m_dwSectorSize) )
    {
        PVSD_GENERIC pVRSSector = (PVSD_GENERIC)(&pVRSBuffer[dwRead * m_dwSectorSize]);

        dwRead += 1;

        if( pVRSSector->Type == 0 )
        {
            if( strncmp( (char*)pVRSSector->Ident, "TEA01", 5 ) == 0 )
            {
                fFoundTEA = TRUE;
                continue;
            }

            if( strncmp( (char*)pVRSSector->Ident, "BEA01", 5 ) == 0 )
            {
                fFoundBEA = TRUE;
                continue;
            }

            if( strncmp( (char*)pVRSSector->Ident, "NSR02", 5 ) == 0 )
            {
                *pNSRVersion = 2;
                fFoundNSR = TRUE;
                continue;
            }

            if( strncmp( (char*)pVRSSector->Ident, "NSR03", 5 ) == 0 )
            {
                *pNSRVersion = 3;
                fFoundNSR = TRUE;
                continue;
            }
        }
    }

    fResult = fFoundBEA && fFoundTEA && fFoundNSR;

CLEANUP_BUFFER:
    if( pVRSBuffer )
    {
        delete [] pVRSBuffer;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ValidateVDS
//
//
// TODO::If the main VDS is corrupt or unusable, switch to the reserve
// VDS.
//
BOOL CVolume::ValidateVDS()
{
    BOOL fResult = TRUE;
    BYTE* pLogicalVolume = NULL;

    if( !ReadVDS( &pLogicalVolume, &m_FsdStart, m_PartitionInfo ) )
    {
        fResult = FALSE;
        goto exit_validatevds;
    }

    //
    // We need to ensure that we've found the location of the file set descriptor
    // from the LVD.
    //
    if( m_FsdStart.Length.Length == 0 )
    {
        fResult = FALSE;
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!ReadVDS did not find the FSD pointer.\n")));
        SetLastError( ERROR_DISK_CORRUPT );
        goto exit_validatevds;
    }

    //
    // We need to have found at least one partition descriptor.
    //
    if( m_PartitionInfo[0].Length == 0 )
    {
        fResult = FALSE;
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!ReadVDS did not find a partition descriptor.\n")));
        SetLastError( ERROR_DISK_CORRUPT );
        goto exit_validatevds;
    }

    //
    // We allocated space for the LVD so that we could later check the partition
    // maps in it.  We needed to make sure we had the partition descriptors first,
    // so we put off reading in all of this information until last.  If this isn't
    // here, then we have problems.
    //
    if( !pLogicalVolume )
    {
        fResult = FALSE;
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!ReadVDS did not find any partition maps.\n")));
        SetLastError( ERROR_DISK_CORRUPT );
        goto exit_validatevds;
    }

    //
    // Now that we know we have some partition descriptors, read in the partition
    // maps and check that they make sense.
    //
    if( !ReadPartitionMaps( (PNSR_LVOL)pLogicalVolume ) )
    {
        fResult = FALSE;
        goto exit_validatevds;
    }

exit_validatevds:
    if( pLogicalVolume )
    {
        delete [] pLogicalVolume;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ReadVDS
//
BOOL CVolume::ReadVDS( BYTE** ppLogicalVolume,
                       LONGAD* pFSDAddr,
                       PPARTITION_INFO pPartitionArray )
{
    ASSERT( ppLogicalVolume != NULL );
    ASSERT( pFSDAddr != NULL );

    BOOL fResult = TRUE;
    EXTENTAD MainVDS;
    EXTENTAD ReserveVDS;

    LRESULT lResult = 0;
    BYTE* pVDS = NULL;
    DWORD dwRead = 0;

    Uint32 MapLength = 0;

    PDESTAG pTag = NULL;
    DWORD dwSector = 0;

    BYTE* pLogicalVolume = NULL;

    //
    // Find the locations of the main and reserve VDS.
    //
    if( !GetVDSLocations( &MainVDS, &ReserveVDS ) )
    {
        fResult = FALSE;
        goto exit_readvds;
    }

    //
    // TODO::We should break this down if it is too large.  In most
    // cases this won't be a problem.  A malicious case at worse will
    // force us to allocate a lot of memory temporarily, but it should
    // be fixed.
    //
    pVDS = new (UDF_TAG_BYTE_BUFFER) BYTE[ MainVDS.Len ];
    if( !pVDS )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        fResult = FALSE;
        goto exit_readvds;
    }

    lResult = m_pMedia->ReadRange( MainVDS.Lsn,
                                   0,
                                   MainVDS.Len,
                                   pVDS,
                                   &dwRead );
    if( lResult != ERROR_SUCCESS )
    {
        fResult = FALSE;
        SetLastError( lResult );
        goto exit_readvds;
    }

    pTag = (PDESTAG)pVDS;
    dwSector = MainVDS.Lsn;

    //
    // Now iterate through the sectors that we've read in.  Each sector
    // should contain a different VDS descriptor until we've reached
    // the end of the sequence.
    //
    while( (BYTE*)pTag <= pVDS + MainVDS.Len - m_dwSectorSize &&
           pTag->Ident >= DESTAG_ID_NSR_PVD &&
           pTag->Ident <= DESTAG_ID_NSR_TERM )
    {
        BOOL fAdvance = TRUE;

        switch( pTag->Ident )
        {
        case DESTAG_ID_NSR_VDP:
        {
            //
            // This is the end of this run in the VDS.  We need to read
            // a new extent.
            //
            // TODO::We could keep a count here to ensure that we don't
            // hit some corrupted disk with an infinite loop in it.
            //
            fAdvance = FALSE;

            EXTENTAD NextExtent = ((PNSR_VDP)pTag)->Next;

            delete [] pVDS;
            pVDS = new (UDF_TAG_BYTE_BUFFER) BYTE[ NextExtent.Len ];
            if( !pVDS )
            {
                fResult = FALSE;
                SetLastError( ERROR_NOT_ENOUGH_MEMORY );
                goto exit_readvds;
            }

            lResult = m_pMedia->ReadRange( NextExtent.Lsn,
                                           0,
                                           NextExtent.Len,
                                           pVDS,
                                           &dwRead );
            if( lResult != ERROR_SUCCESS )
            {
                fResult = FALSE;
                SetLastError( lResult );
                goto exit_readvds;
            }

            pTag = (PDESTAG)pVDS;
            dwSector = NextExtent.Lsn;
        }
        break;

        case DESTAG_ID_NSR_PVD:
        {
            //
            // TODO::Is there anything we want from the PVD?
            //
        }
        break;

        case DESTAG_ID_NSR_IMPUSE:
        {
            //
            // TODO::Is there anything we want from the IUVD?
            // We'll pull the volume label from the FSD.
            //
        }
        break;

        case DESTAG_ID_NSR_PART:
        {
            //
            // We will pull the following information through the PD:
            // 1. Partition starting sector.
            // 2. Partition length.
            // 3. Partition access type.
            // 4. Location of a space bitmap descriptor.
            // 5. The partition number.
            //
            PNSR_PART pPD = (PNSR_PART)pTag;
            PNSR_PART_H pPartHeader = (PNSR_PART_H)pPD->ContentsUse;
            PPARTITION_INFO pPartInfo = pPartitionArray[0].Start == 0
                                            ? &pPartitionArray[0]
                                            : &pPartitionArray[1];

            if( !VerifyDescriptor( dwSector,
                                   (BYTE*)pPD,
                                   sizeof(NSR_PART) ) )
            {
                fResult = FALSE;
                DEBUGMSG(ZONE_INIT|ZONE_ERROR, (TEXT("UDFS!ReadVDS has found an invalid PD at 0x%x"),dwSector));
                SetLastError( ERROR_DISK_CORRUPT );
                goto exit_readvds;
            }

            if( pPartInfo->Start != 0 )
            {
                //
                // There can only be two partition descriptors on a UDF volume.
                //
                fResult = FALSE;
                DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!ReadVDS Found more than two partition descriptors!\n")));
                SetLastError( ERROR_DISK_CORRUPT );
                goto exit_readvds;
            }

            pPartInfo->AccessType = pPD->AccessType;
            if( pPD->AccessType == NSR_PART_ACCESS_RO )
            {
                DEBUGMSG(ZONE_INIT,(TEXT("UDFS!ReadVDS The partition is marked as read-only in the PD.\n")));
            }

#if 0 // TODO: Revisit this when we add UDF write support to the driver.
            if( pPD->AccessType == NSR_PART_ACCESS_RW_PRE )
            {
                //
                // We currently do not support this type of media.  Each sector
                // has to be manually erased before it can be used.  These media
                // type are rare in today's world.  Basically it's legacy MO.
                //

                //
                // TODO::Is there a better error code to return?
                //
                fResult = FALSE;
                SetLastError( ERROR_UNSUPPORTED_TYPE );
                DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!ReadVDS Found a partition marked as re-write.  These aren't supported as they need prep.\n")));
                goto exit_readvds;
            }
#endif

            pPartInfo->Length = pPD->Length;
            pPartInfo->Start = pPD->Start;
            pPartInfo->Number = pPD->Number;
            pPartInfo->SBD = pPartHeader->UASBitmap;

            if( pPartInfo->Start > m_dwLastSector )
            {
                fResult = FALSE;
                DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!ReadVDS The partition start sector is invalid.\n")));
                SetLastError( ERROR_DISK_CORRUPT );
                goto exit_readvds;
            }

            if( pPartInfo->Start + pPartInfo->Length > m_dwLastSector )
            {
                if( (m_dwMediaType & MED_FLAG_REWRITE) == MED_FLAG_REWRITE )
                {
                    fResult = FALSE;
                    DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!ReadVDS The partition is too large for the disc.\n")));
                    SetLastError( ERROR_DISK_CORRUPT );
                    goto exit_readvds;
                }
            }

            //
            // TODO::Check for overlapping partitions.  Since I've never seen 2
            // partitions on a disc, I don't think this is a high priority.
            //
        }
        break;

        case DESTAG_ID_NSR_LVOL:
        {
            PNSR_LVOL pLVol = (PNSR_LVOL)pTag;

            if( !VerifyDescriptor( dwSector,
                                   (BYTE*)pLVol,
                                   sizeof(NSR_LVOL) + pLVol->MapTableLength ) )
            {
                fResult = FALSE;
                DEBUGMSG(ZONE_INIT|ZONE_ERROR, (TEXT("UDFS!ReadVDS has found an invalid LVD at 0x%x"),dwSector));
                SetLastError( ERROR_DISK_CORRUPT );
                goto exit_readvds;
            }

            *pFSDAddr = pLVol->FSD;

            //
            // We need to save a copy of the partition maps and then
            // validate them later when we're assured to have read in
            // the partition descriptors.
            //
            MapLength = pLVol->MapTableLength;
            if( MapLength > m_dwSectorSize - sizeof(NSR_LVOL) )
            {
                fResult = FALSE;
                DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFSReadVDS The partition map length is too long.\n")));
                SetLastError( ERROR_DISK_CORRUPT );
                goto exit_readvds;
            }

            pLogicalVolume = new (UDF_TAG_BYTE_BUFFER) BYTE[m_dwSectorSize];
            if( !pLogicalVolume )
            {
                fResult = FALSE;
                SetLastError( ERROR_NOT_ENOUGH_MEMORY );
                goto exit_readvds;
            }

            CopyMemory( pLogicalVolume, pLVol, m_dwSectorSize );

            if( !ReadLVIDSequence( pLVol->Integrity ) )
            {
                fResult = FALSE;
                goto exit_readvds;
            }
        }
        break;

        case DESTAG_ID_NSR_UASD:
        {
            //
            // TODO::This can be helpful if we need to create an LVID or similar.
            // For now though we will just ignore it.
            //
        }
        break;

        case DESTAG_ID_NSR_TERM:
        {
            //
            // This will mark the end of the VDS.  Nothing that we need from it
            // though.
            //
        }
        break;
        }

        //
        //
        //
        if( fAdvance )
        {
            dwSector += 1;
            pTag = (PDESTAG)((BYTE*)pTag + m_dwSectorSize);
        }
    }

exit_readvds:
    *ppLogicalVolume = pLogicalVolume;

    if( pVDS )
    {
        delete [] pVDS;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ReadLVIDSequence
//
BOOL CVolume::ReadLVIDSequence( const EXTENTAD& Start )
{
    BOOL fResult = TRUE;

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ReadPartitionMaps
//
// Assuming that the partition descriptors have already been read.  That means
// we've filled in the partition numbers, the starting sectors, and the lengths
// of the partition(s).
//
BOOL CVolume::ReadPartitionMaps( PNSR_LVOL pLVol )
{
    BOOL fResult = TRUE;
    Uint16 nIndex = 0;
    Uint32 nOffset = 0;
    Uint8* pTable = (Uint8*)&pLVol->MapTable[0];

    CPhysicalPartition* pPhysicalPartition = NULL;
    CSparingPartition* pSparingPartition = NULL;
    CVirtualPartition* pVirtualPartition = NULL;
    CMetadataPartition* pMetadataPartition = NULL;

    if( pLVol->MapTableLength < pLVol->MapTableCount * sizeof(PARTMAP_GENERIC) )
    {
        DEBUGMSG( ZONE_ERROR,(TEXT("UDFS!ReadPartitionMaps The partition table length is invalid.\n")));
        return FALSE;
    }

    //
    // Reading in the partition maps is a two phase process.  In the first phase
    // we'll read in all the phyiscal maps.  This is necessary because we need
    // the physical partitions to be created to instantiate the virtual partitions.
    //
    while( nOffset < pLVol->MapTableLength &&
           nIndex < pLVol->MapTableCount )
    {
        PPARTMAP_GENERIC pGenMap = (PPARTMAP_GENERIC)(&pTable[nOffset]);

        switch( pGenMap->Type )
        {
        case PARTMAP_TYPE_PHYSICAL:
        {
            PPARTMAP_PHYSICAL pPhysical = (PPARTMAP_PHYSICAL)pGenMap;
            nOffset += sizeof(PARTMAP_PHYSICAL);

            if( pPhysical->Length != sizeof(PARTMAP_PHYSICAL) )
            {
                DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!ReadPartitionMaps The map length is incorrect.\n")));
                return FALSE;
            }

            pPhysicalPartition = new (UDF_TAG_PARTITION) CPhysicalPartition();
            if( !pPhysicalPartition )
            {
                SetLastError( ERROR_NOT_ENOUGH_MEMORY );
                return FALSE;
            }

            if( !pPhysicalPartition->Initialize( this, nIndex ) )
            {
                delete pPhysicalPartition;
                return FALSE;
            }

            if( !AddPhysicalPartition( pPhysicalPartition, pPhysical->Partition ) )
            {
                delete pPhysicalPartition;
                return FALSE;
            }

            pPhysicalPartition = NULL;
        }
        break;

        case PARTMAP_TYPE_PROXY:
        {
            PPARTMAP_UDF_GENERIC pGen2Map = (PPARTMAP_UDF_GENERIC)pGenMap;

            nOffset += sizeof(PARTMAP_UDF_GENERIC);

            if( strncmp( (char*)pGen2Map->PartID.Identifier,
                         "*UDF Sparable Partition",
                         strlen( "*UDF Sparable Partition" ) ) == 0 )
            {
                PPARTMAP_SPARABLE pSpareMap = (PPARTMAP_SPARABLE)pGen2Map;

                pSparingPartition = new (UDF_TAG_PARTITION) CSparingPartition();
                if( !pSparingPartition )
                {
                    SetLastError( ERROR_NOT_ENOUGH_MEMORY );
                    return FALSE;
                }

                if( !pSparingPartition->Initialize( this, nIndex ) )
                {
                    delete pSparingPartition;
                    return FALSE;
                }

                if( !pSparingPartition->InitializeSparingInfo( pSpareMap ) )
                {
                    delete pSparingPartition;
                    return FALSE;
                }

                if( !AddPhysicalPartition( pSparingPartition, pSpareMap->Partition ) )
                {
                    delete pSparingPartition;
                    return FALSE;
                }

                pSparingPartition = NULL;
            }
            else if( strncmp( (char*)pGen2Map->PartID.Identifier,
                              "*UDF Virtual Partition",
                              strlen( "*UDF Virtual Partition" ) ) == 0 )
            {
                //
                // This is valid, but we're skipping it in phase one.
                //
                break;
            }
            else if( strncmp( (char*)pGen2Map->PartID.Identifier,
                              "*UDF Metadata Partition",
                              strlen( "*UDF Metadata Partition" ) ) == 0 )
            {
                //
                // This is valid, but we're skipping it in phase one.
                //
                break;
            }
            else
            {
                DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!ReadPartitionMaps Found unknown type 2 partition map.\n")));
                return FALSE;
            }
        }
        break;

        default:
            DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!ReadPartitionMaps There is an invalid partition map type.\n")));
            return FALSE;
        }

        nIndex += 1;
    }

    //
    // This is phase two.  We ignore the physical maps here and just focus on
    // the virtual maps.  We know by now that there are no invalid maps, so no
    // need to check for that again.
    //

    nIndex = 0;
    nOffset = 0;
    while( nOffset < pLVol->MapTableLength &&
           nIndex < pLVol->MapTableCount )
    {
        PPARTMAP_GENERIC pGenMap = (PPARTMAP_GENERIC)(&pTable[nOffset]);

        switch( pGenMap->Type )
        {
        case PARTMAP_TYPE_PHYSICAL:
        {
            PPARTMAP_PHYSICAL pPhysical = (PPARTMAP_PHYSICAL)pGenMap;
            nOffset += sizeof(PARTMAP_PHYSICAL);
        }
        break;

        case PARTMAP_TYPE_PROXY:
        {
            PPARTMAP_UDF_GENERIC pGen2Map = (PPARTMAP_UDF_GENERIC)pGenMap;

            nOffset += sizeof(PARTMAP_UDF_GENERIC);

            if( strncmp( (char*)pGen2Map->PartID.Identifier,
                         "*UDF Virtual Partition",
                         strlen( "*UDF Virtual Partition" ) ) == 0 )
            {
                PPARTMAP_VIRTUAL pVirtualMap = (PPARTMAP_VIRTUAL)pGen2Map;

                pVirtualPartition = new (UDF_TAG_PARTITION) CVirtualPartition();
                if( !pVirtualPartition )
                {
                    SetLastError( ERROR_NOT_ENOUGH_MEMORY );
                    return FALSE;
                }

                if( !AddVirtualPartition( pVirtualPartition,
                                          pVirtualMap->Partition,
                                          nIndex ) )
                {
                    delete pVirtualPartition;
                    return FALSE;
                }

                //
                // Is this necessary?  I don't think so and am removing it for
                // now.
                //
                /*
                if( !pVirtualPartition->InitializeVirtualInfo( pVirtualMap ) )
                {
                    //
                    // Do not delete the virtual partition here!  This has
                    // already been added to the volume, so it will be deleted
                    // when the volume is destroyed.
                    //
                    return FALSE;
                }
                */

                pVirtualPartition = NULL;
            }
            else if( strncmp( (char*)pGen2Map->PartID.Identifier,
                              "*UDF Metadata Partition",
                              strlen( "*UDF Metadata Partition" ) ) == 0 )
            {
                PPARTMAP_METADATA pMetadataMap = (PPARTMAP_METADATA)pGen2Map;

                pMetadataPartition = new (UDF_TAG_PARTITION) CMetadataPartition();
                if( !pMetadataPartition )
                {
                    SetLastError( ERROR_NOT_ENOUGH_MEMORY );
                    return FALSE;
                }

                if( !AddVirtualPartition( pMetadataPartition,
                                          pMetadataMap->Partition,
                                          nIndex ) )
                {
                    delete pMetadataPartition;
                    return FALSE;
                }

                if( !pMetadataPartition->InitializeMetadataInfo( pMetadataMap ) )
                {
                    //
                    // Do not delete the virtual partition here!  This has
                    // already been added to the volume, so it will be deleted
                    // when the volume is destroyed.
                    //
                    return FALSE;
                }

            }
        }
        break;
        }

        nIndex += 1;
    }

    //
    // Now that we've added all our partition maps, we need to check that all
    // the information we have is valid.
    // 1. There is at least one physical map for each partition.
    // 2. There is not a CSparingPartition and a CVirtualPartition together.
    //
    for( int x = 0; x < 2; x++ )
    {
        if( m_PartitionInfo[x].Length )
        {
            if( !m_PartitionInfo[x].pPhysicalPartition )
            {
                DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!ReadPartitionMaps found a partition with no physical partition map: %d\n"),m_PartitionInfo[x].Number));
                return FALSE;
            }

            if( m_PartitionInfo[x].pVirtualPartition )
            {
                if( m_PartitionInfo[x].pVirtualPartition->GetPartitionType() == UDF_PART_VIRTUAL )
                {
                    if( m_PartitionInfo[x].pPhysicalPartition->GetPartitionType() == UDF_PART_SPARING )
                    {
                        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!ReadPartitionMaps found a sparing partition paired with a virtual partition: %d\n"),m_PartitionInfo[x].Number));
                        return FALSE;
                    }
                }
            }
        }
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// AddPhysicalPartition
//
BOOL CVolume::AddPhysicalPartition( CPhysicalPartition* pPartition,
                                    Uint16 Number )
{
    BOOL fResult = FALSE;

    ASSERT(pPartition != NULL);

    for( int x = 0; (x < 2) && !fResult; x++ )
    {
        if( m_PartitionInfo[x].Number == Number &&
            m_PartitionInfo[x].Length != 0 )
        {
            if( m_PartitionInfo[x].pPhysicalPartition != NULL )
            {
                DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!AddPhysicalPartition Multiple phyiscal partition maps were found for partition %d.\n"), Number));
                SetLastError( ERROR_DISK_CORRUPT );
                return FALSE;
            }

            m_pPartition = pPartition;
            m_PartitionInfo[x].pPhysicalPartition = pPartition;
            fResult = TRUE;
        }
    }

    if( !fResult )
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!AddPhysicalPartition A physical partition map has a reference to an unknown partition number: %d.\n"),Number));
        SetLastError( ERROR_DISK_CORRUPT );
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// AddVirtualPartition
//
BOOL CVolume::AddVirtualPartition( CFilePartition* pPartition,
                                   Uint16 Number,
                                   Uint16 Reference )
{
    BOOL fResult = FALSE;
    LRESULT lResult = ERROR_SUCCESS;

    ASSERT(pPartition != NULL);

    for( int x = 0; (x < 2) && !fResult; x++ )
    {
        if( m_PartitionInfo[x].Number == Number &&
            m_PartitionInfo[x].Length != 0 )
        {
            if( m_PartitionInfo[x].pVirtualPartition != NULL )
            {
                DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!AddVirtualPartition Multiple virtual partition maps were found for partition %d.\n"), Number));
                SetLastError( ERROR_DISK_CORRUPT );
                return FALSE;
            }

            if( m_PartitionInfo[x].pPhysicalPartition == NULL )
            {
                DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!AddVirtualPartition A virtual partition map found to a partition without a physical partition: %d.\n"), Number));
                SetLastError( ERROR_DISK_CORRUPT );
                return FALSE;
            }

            lResult = pPartition->Initialize( m_PartitionInfo[x].pPhysicalPartition,
                                              Reference );
            if( lResult != ERROR_SUCCESS )
            {
                SetLastError( lResult );
                return FALSE;
            }

            m_PartitionInfo[x].pVirtualPartition = pPartition;
            m_pPartition = pPartition;
            fResult = TRUE;
        }
    }

    if( !fResult )
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!AddVirtualPartition A virtual partition map has a reference to an unknown partition number: %d.\n"),Number));
        SetLastError( ERROR_DISK_CORRUPT );
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetVDSLocations
//
BOOL CVolume::GetVDSLocations( PEXTENTAD pMain, PEXTENTAD pReserve )
{
    ASSERT( pMain != NULL );
    ASSERT( pReserve != NULL );

    BOOL fResult = FALSE;
    LRESULT lResult = ERROR_SUCCESS;
    BYTE nIndex = 0;
    Uint8* pBuffer = NULL;
    PNSR_ANCHOR pAVDP = NULL;
    DWORD dwRead = 0;
    DWORD dwAVDPSector[] = { m_dwFirstSector + 256,
                             m_dwLastSector - 1,
                             m_dwLastSector - 256,
                             m_dwFirstSector + 512 };

    pBuffer = new (UDF_TAG_BYTE_BUFFER) Uint8[m_dwSectorSize];
    if( !pBuffer )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return FALSE;
    }

    pAVDP = (PNSR_ANCHOR)pBuffer;

    while( !fResult &&
           (nIndex < sizeof(dwAVDPSector) / sizeof(dwAVDPSector[1])) )
    {
        lResult = m_pMedia->ReadRange( dwAVDPSector[nIndex],
                                       0,
                                       m_dwSectorSize,
                                       pBuffer,
                                       &dwRead );
        if( lResult != ERROR_SUCCESS )
        {
            fResult = FALSE;
            SetLastError( lResult );
            goto Exit_GetVDSLocations;
        }

        //
        // Check the self-referencing location, the CRC, and the Checksum.
        //
        if( !VerifyDescriptor( dwAVDPSector[nIndex],
                               pBuffer,
                               sizeof(NSR_ANCHOR) ) )
        {
            DEBUGMSG(ZONE_INIT, (TEXT("UDFS!ReadVDS No AVDP found at 0x%x"), dwAVDPSector[nIndex] - m_dwFirstSector));
            nIndex += 1;
            continue;
        }

        //
        // See if it's actually an AVDP.
        //
        if( pAVDP->Destag.Ident != DESTAG_ID_NSR_ANCHOR )
        {
            DEBUGMSG(ZONE_INIT, (TEXT("UDFS!ReadVDS No AVDP found at 0x%x"), dwAVDPSector[nIndex] - m_dwFirstSector));
            nIndex += 1;
            continue;
        }

        //
        // Everything looks in order, so get the reserve and main VDS locations.
        //
        *pMain = pAVDP->Main;
        *pReserve = pAVDP->Reserve;
        fResult = TRUE;

    }

    if( !fResult )
    {
        DEBUGMSG(ZONE_INIT,(TEXT("UDFS!ReadVDS no AVDP was found.")));
        fResult = FALSE;
        goto Exit_GetVDSLocations;
    }

Exit_GetVDSLocations:
    if( pBuffer )
    {
        delete [] pBuffer;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ReadFSD
//
// We are just validating the FSD and pulling the following from it.
// 1. The volume label.  This is taken from the FSD instead of the multitude
//    of other possible locations becuase the FSD is the only descriptor that
//    can be remapped for incremental media using the VAT.
// 2. The location of the root directory.
//
// NOTE: We aren't using the volume label for anything right now, so just
// ignoring it.
//
BOOL CVolume::ReadFSD( LONGAD* pRootAddr )
{
    BOOL fResult = TRUE;
    LRESULT lResult = ERROR_SUCCESS;

    BYTE* pBuffer = NULL;
    PNSR_FSD pFSD = NULL;
    Uint32 BufferSize = GetSectorSize();

    ASSERT( pRootAddr != NULL );

    if( m_FsdStart.Length.Length > BufferSize )
    {
        BufferSize = m_FsdStart.Length.Length >> m_dwShiftCount;
        BufferSize *= GetSectorSize();
        if( m_FsdStart.Length.Length & (GetSectorSize() - 1) )
        {
            BufferSize += GetSectorSize();
        }
    }

    pBuffer = new (UDF_TAG_BYTE_BUFFER) BYTE[ BufferSize ];
    if( !pBuffer )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        goto Exit_ReadFSD;
    }

    lResult = m_pPartition->Read( m_FsdStart.Start,         // Block & Partition
                                  0,                        // Block Offset
                                  m_FsdStart.Length.Length, // Length in bytes
                                  pBuffer );                // Output buffer
    if( lResult != ERROR_SUCCESS )
    {
        SetLastError( lResult );
        goto Exit_ReadFSD;
    }

    //
    // Validate some basics for the FSD.
    //
    pFSD = (PNSR_FSD)pBuffer;
    if( pFSD->Destag.Ident != DESTAG_ID_NSR_FSD )
    {
        SetLastError( ERROR_DISK_CORRUPT );
        goto Exit_ReadFSD;
    }

    if( !VerifyDescriptor( m_FsdStart.Start.Lbn,
                           (BYTE*)pFSD,
                           sizeof(NSR_FSD) ) )
    {
        SetLastError( ERROR_DISK_CORRUPT );
        goto Exit_ReadFSD;
    }

    //
    // The root directory better be allocated and recorded.
    //
    if( pFSD->IcbRoot.Length.Type != NSRLENGTH_TYPE_RECORDED )
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!ReadFSD The root ICB is not recorded.\n")));
        SetLastError( ERROR_DISK_CORRUPT );
        goto Exit_ReadFSD;
    }

    *pRootAddr = pFSD->IcbRoot;

Exit_ReadFSD:
    if( GetLastError() != ERROR_SUCCESS )
    {
        fResult = FALSE;
    }

    if( pBuffer )
    {
        delete [] pBuffer;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ParsePath
//
// Syncronization: Directories are locked.
//      While searching a directory and opening the child we lock the directory
//      to make sure that the file is not deleted or renamed.
//
CFileLink* CVolume::ParsePath( CDirectory* pDir,
                               CFileName* pFileName,
                               BOOL fReturnDir )
{
    CFile* pFile = NULL;
    CFileLink* pLink = NULL;
    WCHAR strFileName[MAX_PATH];
    Uint64  NextFid = 0;
    LRESULT lResult = ERROR_SUCCESS;
    PFILE_HANDLE pNewHandle = NULL;
    CDirectory* pDerefDir = NULL;
    BOOL fUnlock = FALSE;

    ASSERT( pDir != NULL );
    ASSERT( pFileName != NULL );

    if( pFileName->GetCurrentToken() == NULL )
    {
        //
        // There is nothing to find here.
        //
        return NULL;
    }

    do
    {
        //
        // If we find a link, then the child is referenced and will need to be
        // unreferenced in order for it to be deleted.  We need to lock the
        // file manager around this call so that it will not try to delete
        // the file while we are trying to reference it.
        //
        m_FileManager.Lock();
        pLink = pDir->FindOpenFile( pFileName );
        if( pLink )
        {
            pFile = pLink->GetChild();
        }
        m_FileManager.Unlock();

        if( !pFile )
        {
            CFile* pExistingFile = NULL;
            NSR_FID Fid;

            //
            // We need to lock the directory here so that the file cannot be
            // deleted after we look it up.  Also, we will lock the file
            // manager first to preserve locking order.
            //

            m_FileManager.Lock();
            pDir->Lock();
            fUnlock = TRUE;

            lResult = pDir->FindFileName( 0,           // Offset in dir
                                          pFileName,   // Name to look for (including wilcards)
                                          strFileName, // Where to store the file name
                                          MAX_PATH,    // The maximum size of the name
                                          &Fid,        // The FID representing the file
                                          &NextFid );  // The location of the next FID

            if( lResult != ERROR_SUCCESS )
            {
                goto exit_parsepath;
            }

            //
            // We don't currently have this file open, so try to read it in.
            //
            if( Fid.Flags & NSR_FID_F_DIRECTORY )
            {
                pFile = new (UDF_TAG_DIRECTORY) CDirectory();
            }
            else
            {
                pFile = new (UDF_TAG_FILE) CFile();
            }

            if( !pFile )
            {
                lResult = ERROR_NOT_ENOUGH_MEMORY;
                goto exit_parsepath;
            }

            //
            // Read in the file allocation and ICB for the main stream.
            //
            lResult = pFile->Initialize( this, Fid.Icb.Start );
            if( lResult != ERROR_SUCCESS )
            {
                goto exit_parsepath;
            }

            //
            // Check in the file manager to see if this file already exists and
            // add it if it's not there.
            //

            lResult = m_FileManager.AddFile( pFile, &pExistingFile );
            if( lResult == ERROR_ALREADY_EXISTS )
            {
                lResult = ERROR_SUCCESS;

                delete pFile;

                pFile = pExistingFile;
            }
            else if( lResult != ERROR_SUCCESS )
            {
                goto exit_parsepath;
            }

            //
            // This file is now in the file manager and has been read in.  We just
            // need to link it to this directory now.
            //
            pLink = pDir->AddChildPointer( pFile,
                                           strFileName,
                                           (Fid.Flags & NSR_FID_F_HIDDEN) != 0 );
            if( !pLink )
            {
                lResult = GetLastError();
                if( lResult != ERROR_SUCCESS )
                {
                    goto exit_parsepath;
                }
            }

            pDir->Unlock();
            m_FileManager.Unlock();
            fUnlock = FALSE;
        }

        //
        // If we had a directory referenced so that it wouldn't go away, we can
        // dereference it now because we have a child open under it.  We don't
        // want to keep a reference on each directory we open along the way
        //
        if( pDerefDir )
        {
            pDerefDir->Dereference();
            pDerefDir = NULL;
        }

        //
        // Now we need to check if we found a file or a directory.
        //
        if( pFile->IsDirectory() == FALSE )
        {
            //
            // We've found a file object, so the most we should have left to do
            // is to find a stream.
            //
            if( pFileName->HasStreamName() )
            {
                //
                // TODO::Implement stream support.
                //
                lResult = ERROR_NOT_SUPPORTED;
            }
            else if( pFileName->GetCurrentTokenIndex() != pFileName->GetTokenCount() - 1 )
            {
                //
                // If we actually found a file object, not a directory object, then
                // we need to be at the end of the file name, or it's an invalid file
                // name.
                //
                lResult = ERROR_PATH_NOT_FOUND;
            }

        }
        else
        {
            if( !pFileName->GotoNextToken() )
            {
                if( fReturnDir == FALSE )
                {
                    //
                    // We don't want to end with a directory, it must be a file.
                    //
                    lResult = ERROR_PATH_NOT_FOUND;
                }
            }
            else
            {
                pDir = (CDirectory*)pFile;
                pFile = NULL;
                pLink = NULL;
                pDerefDir = pDir;
            }
        }

    } while( !pLink && lResult == ERROR_SUCCESS );

exit_parsepath:
    if( fUnlock )
    {
        pDir->Unlock();
        m_FileManager.Unlock();
    }

    if( pDerefDir )
    {
        pDerefDir->Dereference();
    }

    if( lResult != ERROR_SUCCESS )
    {
        if( pFile )
        {
            pFile->Dereference();
        }
        pLink = NULL;

        SetLastError( lResult );
    }

    return pLink;
}

// /////////////////////////////////////////////////////////////////////////////
// RemountMedia
//
// Determine if the media looks the same after insertion as it did before
// insertion.  We check the following pieces of data:
//
// * Media type
// * First sector
// * Last sector
// * NSR version
// * Partition location
// * Partition size
// * FSD location
// * Root partition location
//
// NOTE THAT THE ORDER OF THE CHECKS IS IMPORTANT.  In later functions there
// are assumptions that the previous checks have already been done.  This can
// of course be changed, but it will require going through these functions
// and removing all of these assumptions.
//
// TODO::Add the number of files and directories, the last updated time
// from the LVID/VAT, and the sector size.
//
BOOL CVolume::RemountMedia()
{
    BOOL fResult = FALSE;
    DWORD dwMediaType = MED_TYPE_UNKNOWN;
    DWORD dwFirstSector = 0;
    DWORD dwLastSector = 0;
    USHORT NSRVersion = 0;
    BYTE* pLogicalVolume = NULL;
    LONGAD FSDStart = { 0 };
    PARTITION_INFO PartitionArray[2] = { 0 };
    LONGAD RootAddr = { 0 };

    //
    // Checking the media type.
    //
    if( !DetermineMediaType( &dwMediaType ) )
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!RemountMedia: Unable to determine the media type.\n")));
        goto exit_remountmedia;
    }

    if( dwMediaType != m_dwMediaType )
    {
        goto exit_remountmedia;
    }

    //
    // TODO::Add a check for the sector size - assuming identical for now.
    //

    //
    // Checking the first and last sectors.  This needs to be done after the
    // media type is verified or it is possible to get unexpected results.
    //
    if( !GetSessionBounds( &dwFirstSector, &dwLastSector ) )
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!RemountMedia: Unable to determine the media bounds.\n")));
        goto exit_remountmedia;
    }

    if( dwFirstSector != m_dwFirstSector )
    {
        goto exit_remountmedia;
    }

    if( dwLastSector != m_dwLastSector )
    {
        goto exit_remountmedia;
    }

    if( m_NSRVer != NSRVersion )
    {
        goto exit_remountmedia;
    }

    //
    // Read in the VDS and check the information here.
    //
    if( !ReadVDS( &pLogicalVolume, &FSDStart, PartitionArray ) )
    {
        goto exit_remountmedia;
    }

    for( int x = 0; x < 2; x++ )
    {
        if( PartitionArray[x].AccessType != m_PartitionInfo[x].AccessType )
        {
            goto exit_remountmedia;
        }

        if( PartitionArray[x].Length != m_PartitionInfo[x].Length )
        {
            goto exit_remountmedia;
        }

        if( PartitionArray[x].Number != m_PartitionInfo[x].Number )
        {
            goto exit_remountmedia;
        }

        if( PartitionArray[x].Start != m_PartitionInfo[x].Start )
        {
            goto exit_remountmedia;
        }

        if( memcmp( &PartitionArray[x].SBD,
                    &m_PartitionInfo[x].SBD,
                    sizeof(SHORTAD) ) != 0 )
        {
            goto exit_remountmedia;
        }
    }

    if( memcmp( &FSDStart, &m_FsdStart, sizeof(FSDStart) ) != 0 )
    {
        goto exit_remountmedia;
    }

    //
    // Finally we will try reading in the FSD to check the root directory
    // location.
    //
    if( !ReadFSD( &RootAddr ) )
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!RemountMedia: Unable to check the VRS.\n")));
        goto exit_remountmedia;
    }

    if( memcmp( &RootAddr, &m_RootDir, sizeof(RootAddr) ) != 0 )
    {
        goto exit_remountmedia;
    }

    fResult = TRUE;

exit_remountmedia:
    if( pLogicalVolume )
    {
        delete [] pLogicalVolume;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// UDFSDeviceIoControl
//
// This will send an IOCTL to the device below us through the file system
// driver manager.
//
BOOL CVolume::UDFSDeviceIoControl( DWORD dwIoControlCode,
                                   LPVOID lpInBuffer,
                                   DWORD nInBufferSize,
                                   LPVOID lpOutBuffer,
                                   DWORD nOutBufferSize,
                                   LPDWORD lpBytesReturned,
                                   LPOVERLAPPED lpOverlapped )
{
    BOOL        fResult = FALSE;

    //
    // This will automatically forward the device IO control on down if the
    // buffer is a user mode address.
    //
    fResult = ::FSDMGR_DiskIoControl( m_hDsk,
                                      dwIoControlCode,
                                      lpInBuffer,
                                      nInBufferSize,
                                      lpOutBuffer,
                                      nOutBufferSize,
                                      lpBytesReturned,
                                      lpOverlapped );

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetPartitionStart
//
Uint32 CVolume::GetPartitionStart( Uint16 Reference ) const
{
    Uint32 Block = 0;

    for( int x = 0; x < 2; x++ )
    {
        if( m_PartitionInfo[x].pPhysicalPartition &&
            m_PartitionInfo[x].pPhysicalPartition->GetReferenceNumber() == Reference )
        {
            return m_PartitionInfo[x].Start;
        }

        if( m_PartitionInfo[x].pVirtualPartition &&
            m_PartitionInfo[x].pVirtualPartition->GetReferenceNumber() == Reference )
        {
            return m_PartitionInfo[x].Start;
        }
    }

    return Block;
}

// /////////////////////////////////////////////////////////////////////////////
// ValidateUDFVolume
//
BOOL CVolume::ValidateUDFVolume(HDSK hDisk)
{
    BOOL fResult = TRUE;

    m_hDsk = hDisk;

    if (!ValidateAndInitMedia())
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!ValidateUDFVolume: Not a Valid media Type \n")));
        fResult= FALSE;
        goto Exit;
    }

    fResult = ValidateUDFVolume();
    if( !fResult )
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!ValidateUDFVolume: The volume does not appear to be UDF.\n")));
    }

Exit:
    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ValidateAndInitMedia
//
BOOL CVolume::ValidateAndInitMedia()
{
    BOOL fResult = TRUE;
    LRESULT lResult = ERROR_SUCCESS;

    if( !DetermineMediaType( &m_dwMediaType ) )
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!Initialize: Unable to determine the media type.\n")));
        fResult = FALSE;
        goto Exit;
    }

    if( m_dwMediaType == MED_TYPE_UNKNOWN )
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!Initialize: Unable to determine the media type.\n")));
        fResult = FALSE;
        goto Exit;
    }

    if( m_dwMediaType & MED_FLAG_INCREMENTAL )
    {
        m_pMedia = new(UDF_TAG_MEDIA) CWriteOnceMedia();
    }
    else if( (m_dwMediaType & MED_TYPE_DISK) == MED_TYPE_DISK )
    {
        FSD_DISK_INFO DiskInfo = { 0 };
        lResult = ::FSDMGR_GetDiskInfo( m_hDsk, &DiskInfo );
        if( lResult != ERROR_SUCCESS )
        {
            fResult = FALSE;
            goto Exit;
        }

        m_dwSectorSize = DiskInfo.cbSector;

        m_pMedia = new(UDF_TAG_MEDIA) CHardDiskMedia();
    }
    else if( m_dwMediaType & MED_FLAG_REWRITE )
    {
        m_pMedia = new(UDF_TAG_MEDIA) CRewritableMedia();
    }
    else
    {
        //
        // TODO::Could we just have on incremental media with a flag
        // describing its writability?
        //
        m_pMedia = new(UDF_TAG_MEDIA) CReadOnlyMedia();
    }

    if( !m_pMedia )
    {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!Initialize: Unable to allocate media object.\n")));
        fResult = FALSE;
        goto Exit;
    }

    m_pMedia->Initialize( this );
Exit:
    return fResult;
    
}
