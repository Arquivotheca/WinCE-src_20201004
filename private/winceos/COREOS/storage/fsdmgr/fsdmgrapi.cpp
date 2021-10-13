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
#include "storeincludes.hpp"
extern HANDLE g_hStoreDiskApi;

#ifdef UNDER_CE

typedef BOOL ( * PADVERTISEINTERFACE ) ( const GUID *devclass, LPCWSTR name, BOOL fAdd );

EXTERN_C BOOL FSDMGR_AdvertiseInterface ( const GUID *pGuid, LPCWSTR lpszName, BOOL fAdd )
{
    static PADVERTISEINTERFACE pAdvertiseInterface = NULL;
    BOOL bRet = FALSE;

    if ( lpszName && L'\0' == *lpszName ) {
        // AdvertiseInterface does not allow advertising an empty string. Since this
        // typically indicates the root file system, advertise backslash instead.
        lpszName = L"\\";
    }

    if( !pAdvertiseInterface ) {
        
        if ( WAIT_OBJECT_0 == WaitForAPIReady ( SH_FILESYS_APIS, 0 ) ) {
            HMODULE hCoreDll = ( HMODULE )GetModuleHandle( L"coredll.dll" );
            if ( hCoreDll ) {
                pAdvertiseInterface = ( PADVERTISEINTERFACE )FsdGetProcAddress( hCoreDll, L"AdvertiseInterface" );
            }
        }
    }

    if ( pAdvertiseInterface ) {
        bRet = pAdvertiseInterface( pGuid, lpszName, fAdd );
    }
    
    return bRet;
}
#else
// NT stub
EXTERN_C BOOL FSDMGR_AdvertiseInterface ( const GUID *pGuid, LPCWSTR lpszName, BOOL fAdd )
{
    return TRUE;
}
#endif

typedef struct _IHDR {
    BYTE        Revision;
    BYTE        fFlags;
    WORD        cbSize;
    // variable length struct follows
} IHDR;

// Parse the security descriptor and size out of a SECURITY_ATTRIBUTES
// structure passed to FSD_CreateFileW or FSD_CreateDirectoryW. These
// embedded parameters have already been validated before passing to
// the FSD, so no access check is necessary.
EXTERN_C LRESULT FSDMGR_ParseSecurityDescriptor ( 
    __in PSECURITY_ATTRIBUTES pSecurityAttributes,
    __out PSECURITY_DESCRIPTOR* ppSecurityDescriptor,
    __out DWORD* pSecurityDescriptorSize )
{
#ifdef UNDER_CE
    if ( !ppSecurityDescriptor || !pSecurityDescriptorSize ) {
        // FSD did not provide output parameters.
        DEBUGCHK ( 0 );
        return ERROR_INVALID_PARAMETER;
    }

    if ( !pSecurityAttributes ) {
        // No input SECURITY_ATTRIBUTES structure so the security descriptor
        // is empty.
        *ppSecurityDescriptor = NULL;
        *pSecurityDescriptorSize = 0;
        return ERROR_SUCCESS;
    }

    if ( ( sizeof ( SECURITY_ATTRIBUTES ) != pSecurityAttributes->nLength ) ||
       ( FALSE != pSecurityAttributes->bInheritHandle ) ||
        !IsKModeAddr ( ( DWORD )pSecurityAttributes->lpSecurityDescriptor ) ) {
        // Malformed SECURITY_ATTRIBUTES structure. Unexpected because the
        // structure is populated by FSDMGR before passing to the FSD.
        DEBUGCHK ( 0 );
        return ERROR_INVALID_SECURITY_DESCR;
    }

    *pSecurityDescriptorSize = ( ( IHDR * )( pSecurityAttributes->lpSecurityDescriptor ) )->cbSize;
    *ppSecurityDescriptor = pSecurityAttributes->lpSecurityDescriptor;
#else
    *pSecurityDescriptorSize = 0;
    *ppSecurityDescriptor = NULL;
#endif // !UNDER_CE
    return ERROR_SUCCESS;
}

// Used to add a reference to the volume when the FSD is performing
// asynchronous volume access.
EXTERN_C LRESULT FSDMGR_AsyncEnterVolume ( HVOL hVolume, OUT HANDLE* phLock, OUT LPVOID* ppLockData )
{
    // Lock the volume handle to get the volume object.
    MountedVolume_t* pVolume;
    HANDLE hVolumeLock = LockAPIHandle ( hVolumeAPI,
        reinterpret_cast<HANDLE> ( GetCurrentProcessId ( ) ), hVolume,
        reinterpret_cast<LPVOID*> ( &pVolume ) );
    if ( !hVolumeLock ) {
        // We cannot distinguish between an invalid HVOL and a volume that is
        // going away, so indicate the latter.
        return ERROR_DEVICE_REMOVED;
    }

    // Copy the lock handle and lock data to the output parameters.
    if ( !CeSafeCopyMemory ( phLock, &hVolumeLock, sizeof ( HANDLE ) ) ||
        !CeSafeCopyMemory ( ppLockData, &pVolume, sizeof ( LPVOID ) ) ) {
        VERIFY ( UnlockAPIHandle ( hVolumeAPI, hVolumeLock ) );
        return ERROR_INVALID_PARAMETER;
    }

    // Enter the volume.
    LRESULT lResult = pVolume->Enter ( );
    if ( ERROR_SUCCESS != lResult ) {
        // Unable to enter the volume, remove the volume handle lock
        // before returning error code.
        VERIFY ( UnlockAPIHandle ( hVolumeAPI, hVolumeLock ) );
    }

    return lResult;
}

EXTERN_C LRESULT FSDMGR_AsyncExitVolume ( HANDLE hLock, IN LPVOID pLockData )
{
    // pLockData must be the value returned in ppLockData on the corresponding
    // call to FSDMGR_EnterVolumeAsyncrhonous.
    MountedVolume_t* pVolume = reinterpret_cast<MountedVolume_t*> ( pLockData );

    __try {
        // Exit the volume. We are trusting that pLockData was passed correctly
        // by the FSD or FILTER or we cannot exit it properly. Use SEH here in
        // case we were passed a bad volume object.
        pVolume->Exit ( );
    }
    __except ( EXCEPTION_EXECUTE_HANDLER ) {
        return ERROR_INVALID_PARAMETER;
    }

    // Release the caller's lock on the volume handle.
    if ( !UnlockAPIHandle ( hVolumeAPI, hLock ) ) {
        DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!FSDMGR_ExitVolumeAsynchronous: invalid lock HANDLE ( 0x%08x )!\r\n", hLock ) );
        DEBUGCHK ( 0 );
        return ERROR_INVALID_PARAMETER;
    }

    return ERROR_SUCCESS;
}

// FSDMGR_GetVolumeName
//
// FSD helper used to retrieve the name of a mounted volume.
//
EXTERN_C int FSDMGR_GetVolumeName( HVOL hVolume,
    __out_ecount( MountNameChars ) WCHAR* pMountName, int MountNameChars )
{
    // Lock the volume handle to get the volume object.
    MountedVolume_t* pVolume;
    HANDLE hVolumeLock = LockAPIHandle ( hVolumeAPI,
        reinterpret_cast<HANDLE> ( GetCurrentProcessId ( ) ), hVolume,
        reinterpret_cast<LPVOID*> ( &pVolume ) );
    if ( !hVolumeLock ) {
        // LockAPIHandle will fail if the HVOL is not a valid volume handle.
        SetLastError ( ERROR_INVALID_PARAMETER );
        return 0;
    }

    int Index = pVolume->GetMountIndex ( );
    if ( !MountTable_t::IsValidIndex ( Index ) ) {
        // This should never occur if we successfully locked the volume handle.
        DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!FSDMGR_GetVolumeName: the volume is not registered!" ) );
        DEBUGCHK ( 0 );
        VERIFY ( UnlockAPIHandle ( hVolumeAPI, hVolumeLock ) );
        SetLastError ( ERROR_PATH_NOT_FOUND );
        return 0;
    }

    LRESULT lResult = ERROR_SUCCESS;

    __try {
        lResult = g_pMountTable->GetMountName ( Index, pMountName,
            MountNameChars );
    } __except ( EXCEPTION_EXECUTE_HANDLER ) {
        lResult = ERROR_INVALID_PARAMETER;
    }

    if ( ERROR_SUCCESS != lResult ) {
        DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!FSDMGR_GetVolumeName: unable to query volume name; error=%u",
            lResult ) );
        VERIFY ( UnlockAPIHandle ( hVolumeAPI, hVolumeLock ) );
        SetLastError ( lResult );
        return 0;
    }

    size_t NameChars = 0;
    HRESULT hr = E_FAIL;

    __try {
        hr = StringCchLengthW ( pMountName, MountNameChars, &NameChars );
    } __except ( EXCEPTION_EXECUTE_HANDLER ) {
        hr = E_FAIL;
    }

    if ( FAILED ( hr ) ) {
        // Don't expect this unless the buffer has been tampered with
        // asynchronously since we called GetMountName.
        VERIFY ( UnlockAPIHandle ( hVolumeAPI, hVolumeLock ) );
        SetLastError ( ERROR_INVALID_PARAMETER );
        return 0;
    }

    VERIFY ( UnlockAPIHandle ( hVolumeAPI, hVolumeLock ) );

    DEBUGMSG ( ZONE_APIS, ( L"FSDMGR!FSDMGR_GetVolumeName: success! returning \"%\", lenth=%u",
        pMountName, NameChars ) );

    return NameChars;
}


BOOL IsHandle( DWORD pIn )
{
#ifdef UNDER_CE
    return ( ( pIn&0x3 )==0x03 );
#else
    //For NT the hStore is actually the pStore Hence no special processing required
    return FALSE;
#endif
}

// GetDisk
//
// helper used to retrieve the real disk for handle input. It doesnt do any processing
// in the input *ppDisk is not handle.
//
BOOL GetDisk(LogicalDisk_t** ppDisk,HANDLE *phLock)
{
    BOOL bRet = TRUE;
    *phLock = NULL;
    if( IsHandle( ( DWORD ) *ppDisk ) ) {
        LogicalDisk_t** ppDiskOut=NULL;
        *phLock = LockAPIHandle( g_hStoreDiskApi, 
                            reinterpret_cast<HANDLE>( GetCurrentProcessId( ) ),
                            reinterpret_cast<HANDLE> ( *ppDisk ), reinterpret_cast<LPVOID*>( &ppDiskOut ) );
        if( !*phLock ) {
            //Error couldn’t get Store Obj from the Handle.
            bRet = FALSE;
        } else {
            *ppDisk = *ppDiskOut;
        }

    }
    return bRet;
}

EXTERN_C BOOL FSDMGR_GetRegistryValue ( LogicalDisk_t* pDisk, const WCHAR* pValueName, DWORD* pValue )
{
    LogicalDisk_t* pDiskLocal = pDisk;
    HANDLE hLock = NULL;
    LRESULT lResult = ERROR_GEN_FAILURE;

    if ( GetDisk( &pDiskLocal, &hLock ) ) {
       __try {
            lResult = pDiskLocal->GetRegistryValue ( pValueName, pValue );
        }_except ( EXCEPTION_EXECUTE_HANDLER ) {
        }
    }

    //With LockAPIHandle, There is a possibility to get hLock != NULL but still pDiskLocal == NULL
    if( hLock ) {
       VERIFY( UnlockAPIHandle( g_hStoreDiskApi, hLock ) );
    }

    SetLastError ( lResult );
    return ( ERROR_SUCCESS == lResult );
}

EXTERN_C BOOL FSDMGR_GetRegistryString ( LogicalDisk_t* pDisk, const WCHAR* pValueName,
    __out_ecount( ValueSize ) WCHAR* pValue, DWORD ValueSize )
{

    LogicalDisk_t* pDiskLocal = pDisk;
    HANDLE hLock = NULL;
    LRESULT lResult = ERROR_GEN_FAILURE;

    if ( GetDisk( &pDiskLocal, &hLock ) ) {
        __try {
            lResult = pDiskLocal->GetRegistryString ( pValueName, pValue, ValueSize );
        }_except ( EXCEPTION_EXECUTE_HANDLER ) {
        }
    }

    //With LockAPIHandle, There is a possibility to get hLock != NULL but still pDiskLocal == NULL
    if( hLock ) {
       VERIFY( UnlockAPIHandle( g_hStoreDiskApi, hLock ) );
    }


    SetLastError ( lResult );
    return ( ERROR_SUCCESS == lResult );
}

EXTERN_C BOOL FSDMGR_GetRegistryFlag ( LogicalDisk_t* pDisk, const WCHAR* pValueName, DWORD* pFlag, DWORD SetBit )
{
    LogicalDisk_t* pDiskLocal = pDisk;
    HANDLE hLock = NULL;
    LRESULT lResult = ERROR_GEN_FAILURE;

    if ( GetDisk( &pDiskLocal, &hLock ) ) {
        __try {
            lResult = pDiskLocal->GetRegistryFlag ( pValueName, pFlag, SetBit );
        }_except ( EXCEPTION_EXECUTE_HANDLER ) {
        }
    }

    //With LockAPIHandle, There is a possibility to get hLock != NULL but still pDiskLocal == NULL
    if( hLock ) {
       VERIFY( UnlockAPIHandle( g_hStoreDiskApi, hLock ) );
    }


    SetLastError ( lResult );
    return ( ERROR_SUCCESS == lResult );
}

typedef DWORD ( *PFN_UTILAPI )( HANDLE, LPVOID );
EXTERN_C DWORD CallUtilApi ( LogicalDisk_t* pDisk, LPVOID pParams, const WCHAR* pApiName )
{
    HINSTANCE hUtilDll;
    TCHAR szUtilDll[MAX_PATH];
    PFN_UTILAPI pfnUtilApi = NULL;

    LogicalDisk_t* pDiskLocal = pDisk;
    HANDLE hLock = NULL;
    LRESULT lResult = ERROR_GEN_FAILURE;

    if ( GetDisk( &pDiskLocal, &hLock ) ) {
        __try {
            if ( !FSDMGR_GetRegistryString( pDiskLocal, L"Util", szUtilDll, MAX_PATH ) ) {
                DEBUGMSG( ZONE_ERRORS, ( L"FSDMGR!CallUtilApi: No utility DLL specified in registry\r\n" ) );
                lResult = ERROR_FILE_NOT_FOUND;
            }else {

                hUtilDll = LoadLibrary ( szUtilDll );
                if ( hUtilDll != NULL )
                {
                    pfnUtilApi = ( PFN_UTILAPI )FsdGetProcAddress( hUtilDll, pApiName );
                    if ( !pfnUtilApi ) {
                        DEBUGMSG( ZONE_ERRORS, ( L"FSDMGR!CallUtilApi: GetProcAddress failed on %s.\r\n", pApiName ) );
                        FreeLibrary( hUtilDll );
                        lResult = ERROR_NOT_SUPPORTED;
                    } else {
                        DEBUGMSG( ZONE_HELPER, ( L"FSDMGR!CallUtilApi: Calling %s on disk %p with utility dll \"%s\"",
                            pApiName, pDiskLocal, szUtilDll ) );
                        lResult = ERROR_GEN_FAILURE;
                        __try {
                            HANDLE hPartition = pDiskLocal->GetDeviceHandle ( );
                            lResult = pfnUtilApi( hPartition, pParams );
                        } __except( ReportFault( GetExceptionInformation( ), 0 ), EXCEPTION_EXECUTE_HANDLER ) {
                            lResult = ERROR_EXCEPTION_IN_SERVICE;
                        }
                        FreeLibrary( hUtilDll );
                    }
                } else {
                    DEBUGMSG( ZONE_ERRORS, ( L"FSDMGR!CallUtilApi: LoadLibrary failed on %s \r\n", szUtilDll ) );
                    lResult = ERROR_FILE_NOT_FOUND;
                }
            }
        }_except ( EXCEPTION_EXECUTE_HANDLER ) {
        }
    }

    //With LockAPIHandle, There is a possibility to get hLock != NULL but still pDiskLocal == NULL
    if( hLock ) {
       VERIFY( UnlockAPIHandle( g_hStoreDiskApi, hLock ) );
    }
    return lResult;
}



EXTERN_C DWORD FSDMGR_ScanVolume ( LogicalDisk_t* pDisk, LPVOID pParams )
{
    return CallUtilApi( pDisk, pParams, TEXT( "ScanVolumeEx" ) );
}

EXTERN_C DWORD FSDMGR_FormatVolume ( LogicalDisk_t* pDisk, LPVOID pParams )
{
    return CallUtilApi( pDisk, pParams, TEXT( "FormatVolumeEx" ) );
}

EXTERN_C DWORD FSDMGR_CleanVolume ( LogicalDisk_t* pDisk, LPVOID pParams )
{
    return CallUtilApi( pDisk, pParams, TEXT( "CleanVolume" ) );
}

// NOTENOTE: This is a bad API. We must assume that pDiskName is at least
// MAX_PATH chars because no output buffer length is specified.
EXTERN_C BOOL FSDMGR_GetDiskName( LogicalDisk_t* pDisk, WCHAR *pDiskName )
{
    LogicalDisk_t* pDiskLocal = pDisk;
    HANDLE hLock = NULL;
    LRESULT lResult = ERROR_GEN_FAILURE;

    if ( GetDisk( &pDiskLocal, &hLock ) ) {
        __try {
            lResult = pDiskLocal->GetName ( pDiskName, MAX_PATH );
        }_except ( EXCEPTION_EXECUTE_HANDLER ) {
        }
    }

    //With LockAPIHandle, There is a possibility to get hLock != NULL but still pDiskLocal == NULL
    if( hLock ) {
       VERIFY( UnlockAPIHandle( g_hStoreDiskApi, hLock ) );
    }

    ::SetLastError ( lResult );
    return ( ERROR_SUCCESS == lResult );
}

// FSDMGR_GetMountFlags
//
// FSD helper used to retrieve the mount flags associated with an HVOL.
//
EXTERN_C LRESULT FSDMGR_GetMountFlags( HVOL hVolume, DWORD* pMountFlags )
{
    // Lock the volume handle to get the volume object.
    MountedVolume_t* pVolume;
    HANDLE hVolumeLock = LockAPIHandle ( hVolumeAPI,
        reinterpret_cast<HANDLE> ( GetCurrentProcessId ( ) ), hVolume,
        reinterpret_cast<LPVOID*> ( &pVolume ) );
    if ( !hVolumeLock ) {
        // LockAPIHandle will fail if the HVOL is not a valid volume handle.
        DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!FSDMGR_GetMountFlags: invalid HVOL ( 0x%08x )!\r\n", hVolume ) );
        DEBUGCHK ( 0 );
        return ERROR_INVALID_PARAMETER;
    }

    LRESULT lResult = ERROR_SUCCESS;
    __try {
        *pMountFlags = pVolume->GetMountFlags ( );
    } __except ( EXCEPTION_EXECUTE_HANDLER ) {
        lResult = ERROR_INVALID_PARAMETER;
    }

    VERIFY ( UnlockAPIHandle ( hVolumeAPI, hVolumeLock ) );

    return lResult;
}

/*  FSDMGR_RegisterVolume - Called by an FSD to register a volume
 *
 *  ENTRY
 *      pDisk -> MountableDisk_t passed to FSD_MountDisk
 *      pMountName -> desired volume name, NULL for default name ( eg, "Storage Card" )
 *      VolumeContext -> FSD-defined volume-specific data
 *
 *  EXIT
 *      Volume identifier, NULL if none
 */

EXTERN_C HVOL FSDMGR_RegisterVolume ( LogicalDisk_t* pDisk, const WCHAR* pMountName,
        PVOLUME VolumeContext )
{
    //pDisk should a mountable Disk; Can not be a Handle
    ASSERT ( FALSE == IsHandle( ( DWORD ) pDisk ) );

    // NOTE: This should only ever be called by an FSD on a MountableDisk_t object.
    DEBUGCHK ( LogicalDisk_t::MountableDisk_Type == pDisk->m_ThisLogicalDiskType );
    MountableDisk_t* pMountableDisk = reinterpret_cast<MountableDisk_t*> ( pDisk );

    MountedVolume_t* pVolume = pMountableDisk->GetVolume ( );
    DEBUGCHK ( pVolume );

    // Skip preceding slash. Sometimes FSDs will provide this, but it will
    // cause RegisterAFSName to fail.
    if ( pMountName[0] == L'\\' || pMountName[0] == L'/' ) {
        pMountName ++;
    }

    // Check the volume and make sure it is not already mounted.
    if ( INVALID_MOUNT_INDEX != pVolume->GetMountIndex ( ) ) {
        SetLastError ( ERROR_ALREADY_EXISTS );
        return NULL;
    }

    // Make a local copy of the requested name.
    WCHAR FolderName[MAX_PATH];
    size_t FolderLength;
    if ( FAILED ( StringCchCopy ( FolderName, MAX_PATH, pMountName ) ) ||
        FAILED ( StringCchLength ( FolderName, MAX_PATH-1, &FolderLength ) ) ) {
        DEBUGCHK ( 0 ); // Invalid name passed by caller?
        SetLastError ( ERROR_INVALID_PARAMETER );
        return NULL;
    }

    DWORD MountFlags = pVolume->GetMountFlags ( );

    int index = INVALID_MOUNT_INDEX;
    if ( AFS_FLAG_NETWORK & MountFlags ) {

        index = AFS_ROOTNUM_NETWORK;

    } else {


        // Loop through all possible permutations of pMount name, appending suffixes
        // 2 through 9 as necessary.
        for ( int suffix = 1; suffix <= 9; suffix ++ ) {

            if ( suffix > 1 ) {
                FolderName[FolderLength] = L'0' + ( WCHAR )suffix;
                FolderName[FolderLength+1] = L'\0';
            }

            // Register the volume with the current process as the owner because
            // FSDs can not be hosted in user processes.
            index = RegisterAFSName ( FolderName );

            // Loop until we successfully register or exhaust all allowed suffixes.
            if ( ( INVALID_MOUNT_INDEX != index ) &&
               ( ERROR_SUCCESS == ::GetLastError ( ) ) ) {
                // Successfully registered a new name.
                break;
            }

            index = INVALID_MOUNT_INDEX;
        }

        if ( INVALID_MOUNT_INDEX == index ) {
            // Unable to register the volume with any permutation of the requested
            // mount folder name.
            DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!FSDMGR_RegisterVolume: unable to register volume with name \"%s\"!",
                FolderName ) );
            SetLastError ( ERROR_OUT_OF_STRUCTURES );
            return NULL;
        }
    }


    // Because FSDMGR_RegisterVolume is only ever called from within an FSD, we
    // know the FileSystem_t object associated with this MountedVolume_t object is
    // always a FileSystemDriver_t object ( filters are not installed yet ).
    FileSystemDriver_t* pFSD =
        reinterpret_cast<FileSystemDriver_t*> ( pVolume->GetFileSystem ( ) );

    DEBUGCHK ( pFSD );

    // Associate the caller-supplied volume context with the file system.
    pFSD->RegisterContext ( ( DWORD )VolumeContext );

    // Register the volume with the current process as the owner.
    if ( !RegisterAFSEx ( index, hVolumeAPI, ( DWORD )pVolume, AFS_VERSION, MountFlags ) ) {
        // This should never fail because we registered the name and it would
        // be very unexpected for someone to steal the index we just reserved
        // because it would have to be someone else in this process.
        DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!FSDMGR_RegisterVolume: failed to register volume; error=%u",
            ::FsdGetLastError ( ) ) );
        DEBUGCHK ( 0 );
        VERIFY ( DeregisterAFSName ( index ) );
        return NULL;
    }

    // Register the volume at the specified index.
    LRESULT lResult = pVolume->Register ( index );
    if ( ERROR_SUCCESS != lResult ) {
        DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!FSDMGR_RegisterVolume: failed to register volume; error=%u",
            lResult ) );
        DEBUGCHK ( 0 );
        VERIFY ( DeregisterAFS ( index ) );
        VERIFY ( DeregisterAFSName ( index ) );
        SetLastError ( lResult );
        return NULL;
    }

    // Build the initial filter chain. The chain can later been prepended
    // with new filters with subsequent calls to InstallFilters.
    lResult = pVolume->InstallFilters ( );
    if ( ERROR_SUCCESS != lResult ) {
        // We don't expect this to fail even if an individual filter fails.
        DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!FSDMGR_RegisterVolume: failed installing filter drivers; error=%u",
            lResult ) );
        DEBUGCHK ( 0 );
        VERIFY ( DeregisterAFS ( index ) );
        VERIFY ( DeregisterAFSName ( index ) );
        SetLastError ( lResult );
        return NULL;
    }

    // Attach the volume; it is now available for threads to access.
    lResult = pVolume->Attach ( );
    if ( ERROR_SUCCESS != lResult ) {
        DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!FSDMGR_RegisterVolume: failed to attach volume; error=%u",
            lResult ) );
        DEBUGCHK ( 0 );
        VERIFY ( DeregisterAFS ( index ) );
        VERIFY ( DeregisterAFSName ( index ) );
        SetLastError ( lResult );
        return NULL;
    }

    // Process mount finalization if this call is being made outside the context
    // of the FSD_MountDisk call.
    if ( pVolume->DelayedFinalize ( ) ) {
        pVolume->FinalizeMount ( );
    }

    DEBUGMSG ( ZONE_APIS, ( L"FSDMGR!FSDMGR_RegisterVolume: success! returning %p", pVolume ) );

    return pVolume->GetVolumeHandle ( );
}

/*  FSDMGR_CreateSearchHandle - Called by an FSD to create 'search' ( aka 'find' ) handles
 *
 *  ENTRY
 *      pVolume -> MountedVolume_t structure returned from FSDMGR_RegisterVolume
 *      hProc == originating process handle
 *      pSearch == FSD-defined search-specific data for the new handle
 *
 *  EXIT
 *      A 'search' handle associated with the originating process, or INVALID_HANDLE_VALUE
 */

EXTERN_C HANDLE FSDMGR_CreateSearchHandle( HVOL /* hVolume */, HANDLE /* hProc */, PSEARCH pSearch )
{
    return reinterpret_cast<HANDLE> ( pSearch );
}

/*  FSDMGR_CreateFileHandle - Called by an FSD to create 'file' handles
 *
 *  ENTRY
 *      pVolume -> MountedVolume_t structure returned from FSDMGR_RegisterVolume
 *      hProc == originating process handle
 *      pFile == FSD-defined file-specific data for the new handle
 *
 *  EXIT
 *      A 'file' handle associated with the originating process, or INVALID_HANDLE_VALUE
 */

EXTERN_C HANDLE FSDMGR_CreateFileHandle( HVOL /* hVolume */, HANDLE /* hProc */, PFILE pFile )
{
    return reinterpret_cast<HANDLE> ( pFile );
}

/*  FSDMGR_DeregisterVolume - Called by an FSD to deregister a volume
 *
 *  ENTRY
 *      pVolume -> MountedVolume_t structure returned from FSDMGR_RegisterVolume
 *
 *  EXIT
 *      None
 */

EXTERN_C void FSDMGR_DeregisterVolume( HVOL /* hVol */ )
{
    // NO-OP
}

/*  FSDMGR_GetVolumeHandle - Called by an FSD to convert a PDSK to a PVOL
 *
 *  ENTRY
 *      pDisk -> MountableDisk_t passed to FSD_MountDisk
 *
 *  EXIT
 *      Volume identifier, NULL if none
 */

EXTERN_C HVOL FSDMGR_GetVolumeHandle( LogicalDisk_t* pDisk )
{
    //pDisk should a mountable Disk; Can not be a Handle
    ASSERT ( FALSE == IsHandle( ( DWORD ) pDisk ) );

    // NOTE: This should only ever be called by an FSD on a MountableDisk_t object.
    DEBUGCHK ( LogicalDisk_t::MountableDisk_Type == pDisk->m_ThisLogicalDiskType );
    MountableDisk_t* pMountableDisk = reinterpret_cast<MountableDisk_t*> ( pDisk );
    MountedVolume_t* pVolume = pMountableDisk->GetVolume ( );
    return pVolume->GetVolumeHandle ( );
}

/*  FSDMGR_DiskIoControl - Called by an FSD to access DeviceIoControl functions exported by the Block Driver*/

#ifdef UNDER_CE

EXTERN_C BOOL FSDMGR_DiskIoControl( LogicalDisk_t* pDisk, DWORD IoControlCode, void* pInBuf, DWORD InBufSize,
    void* pOutBuf, DWORD OutBufSize, DWORD* pBytesReturned, OVERLAPPED* pOverlapped )
{
    BOOL bRet = FALSE;
    __try {
        ASSERT ( FALSE == IsHandle( ( DWORD ) pDisk ) );
        bRet = pDisk->DoIOControl(IoControlCode, pInBuf, InBufSize, 
                    pOutBuf, OutBufSize, pBytesReturned, pOverlapped );
    }__except( EXCEPTION_EXECUTE_HANDLER ) {
    }

    return bRet;
}

#else

EXTERN_C BOOL FSDMGR_DiskIoControl( LogicalDisk_t* pDisk, DWORD IoControlCode, void* pInBuf, DWORD InBufSize,
    void* pOutBuf, DWORD OutBufSize, DWORD* pBytesReturned, OVERLAPPED* pOverlapped )
{
    LRESULT lResult = pDisk->DiskIoControl ( IoControlCode, pInBuf, InBufSize, pOutBuf,
        OutBufSize, pBytesReturned, pOverlapped );
    SetLastError ( lResult );
    return ( ERROR_SUCCESS == lResult );
}

#endif // !UNDER_CE


/*  FSDMGR_GetDiskInfo - Called by an FSD to query disk info
 *
 *  ENTRY
 *      pDisk -> LogicalDisk_t passed to FSD_MountDisk
 *      pfdi -> FSD_DISK_INFO structure to be filled in
 *
 *  EXIT
 *      ERROR_SUCCESS if structure successfully filled in, or an error code.
 */


EXTERN_C DWORD FSDMGR_GetDiskInfo( LogicalDisk_t* pDiskIn, FSD_DISK_INFO *pfdi )
{
    // NOTE: the FSD_DISK_INFO structure is identical to the DISK_INFO structure
    // except that the members have different names.

    LogicalDisk_t* pDisk = pDiskIn;
    HANDLE hLock = NULL;
    LRESULT lResult = ERROR_SUCCESS;

    if ( GetDisk( &pDisk, &hLock ) ) {
        __try {

            DWORD BytesReturned;

            if ( FSDMGR_DiskIoControl ( pDisk, IOCTL_DISK_GETINFO, NULL, 0, pfdi,
                sizeof( *pfdi ), &BytesReturned, NULL ) ) {

                // Properly supports the IOCTL.
                pDisk->m_DiskFlags |= LogicalDisk_t::DSK_NEW_IOCTLS;

            } else if ( FSDMGR_DiskIoControl ( pDisk, DISK_IOCTL_GETINFO, pfdi, sizeof ( *pfdi ),
                    NULL, 0, &BytesReturned, NULL ) ) {

                // Supports the old DISK_IOCTL style codes, restrict it to these only.
                pDisk->m_DiskFlags &= ~LogicalDisk_t::DSK_NEW_IOCTLS;

            } else if ( FSDMGR_DiskIoControl ( pDisk, IOCTL_DISK_GETINFO, pfdi, sizeof ( *pfdi ),
                    NULL, 0, &BytesReturned, NULL ) ) {

                // Supports the new IOCT, but only with the parameters reversed
                pDisk->m_DiskFlags |= LogicalDisk_t::DSK_NEW_IOCTLS;

            } else {

                pDisk->m_DiskFlags |= LogicalDisk_t::DSK_NO_IOCTLS;
                DEBUGMSG( ZONE_ERRORS, ( L"FSDMGR!FSDMGR_GetDiskInfo: IOCTL_DISK_GETINFO / DISK_IOCTL_GETINFO failed ( %d )\n", FsdGetLastError ( ) ) );
                lResult = FsdGetLastError ( ); 

            }

            if ( lResult == ERROR_SUCCESS ) {
                DEBUGMSG( ZONE_DISKIO, ( L"FSDMGR!FSDMGR_GetDiskInfo: DISK_IOCTL_GETINFO returned %d bytes\n", BytesReturned ) );

                DEBUGMSG( ZONE_DISKIO, ( L"FSDMGR!FSDMGR_GetDiskInfo: %d sectors, BPS=%d, CHS=%d:%d:%d\n",
                          pfdi->cSectors, pfdi->cSectors, pfdi->cbSector, pfdi->cCylinders, pfdi->cHeadsPerCylinder,
                          pfdi->cSectorsPerTrack ) );
            }

        }_except ( EXCEPTION_EXECUTE_HANDLER ) {
            lResult = ERROR_GEN_FAILURE; 
        }
    }else {
        lResult = ERROR_GEN_FAILURE; 
    }

    if( hLock ) {
        VERIFY( UnlockAPIHandle( g_hStoreDiskApi, hLock ) );
    }

    return lResult ;
}

/*  FSDMGR_GetPartInfo - Called by an FSD to query partition info
 *
 *  ENTRY
 *      pDisk -> LogicalDisk_t passed to FSD_MountDisk
 *      pPartInfo -> PARTINFO structure to be filled in
 *
 *  EXIT
 *      ERROR_SUCCESS if structure successfully filled in, or an error code.
 */

EXTERN_C DWORD FSDMGR_GetPartInfo( LogicalDisk_t* pDisk, PARTINFO *pPartInfo )
{

    LRESULT lResult = ERROR_SUCCESS;

    ASSERT ( FALSE == IsHandle( ( DWORD ) pDisk ) );

    DEBUGCHK ( LogicalDisk_t::MountableDisk_Type == pDisk->m_ThisLogicalDiskType );
    MountableDisk_t* pMountableDisk = reinterpret_cast<MountableDisk_t*> ( pDisk );

    if ( !pMountableDisk->GetPartitionInfo( pPartInfo ) ) {
        lResult = FsdGetLastError( );
    }

    DEBUGMSG( ZONE_DISKIO, ( L"FSDMGR!FSDMGR_GetDiskInfo: DISK_IOCTL_GETINFO returned %d\n", lResult ) );

    return lResult;
}

/*  FSDMGR_GetPartInfo - Called by an FSD to query partition info
 *
 *  ENTRY
 *      pDisk -> LogicalDisk_t passed to FSD_MountDisk
 *      pPartInfo -> PARTINFO structure to be filled in
 *
 *  EXIT
 *      ERROR_SUCCESS if structure successfully filled in, or an error code.
 */

EXTERN_C DWORD FSDMGR_GetStoreInfo( LogicalDisk_t* pDisk, STOREINFO *pStoreInfo )
{
    LRESULT lResult = ERROR_SUCCESS;

    ASSERT ( FALSE == IsHandle( ( DWORD ) pDisk ) );

    DEBUGCHK ( LogicalDisk_t::MountableDisk_Type == pDisk->m_ThisLogicalDiskType );
    MountableDisk_t* pMountableDisk = reinterpret_cast<MountableDisk_t*> ( pDisk );

    if ( !pMountableDisk->GetStoreInfo( pStoreInfo ) ) {
        lResult = FsdGetLastError( );
    }

    DEBUGMSG( ZONE_DISKIO, ( L"FSDMGR!FSDMGR_GetDiskInfo: DISK_IOCTL_GETINFO returned %d\n", lResult ) );

    return lResult;
}

/*  ReadWriteDiskEx - Read/write sectors from/to a disk to/from discontiguous buffers
 *
 *  ENTRY
 *      pfsgi -> FSD_SCATTER_GATHER_INFO
 *      pfsgr -> FSD_SCATTER_GATHER_RESULTS
 *      ioctl -> I/O op to perform
 *
 *  EXIT
 *      ERROR_SUCCESS if successful, or an error code
 */

static DWORD ReadWriteDiskEx( FSD_SCATTER_GATHER_INFO *sgi,
                             FSD_SCATTER_GATHER_RESULTS *sgr,
                             DWORD ioctl )
{
    DWORD   result;
    DWORD   i;
    DWORD   dummy;
    BYTE    requestBuffer[sizeof( SG_REQ ) + ( MAX_SG_BUF - 1 ) * sizeof( SG_BUF )];
    SG_REQ* psgreq = ( SG_REQ* )requestBuffer;

    LogicalDisk_t* pDisk = ( LogicalDisk_t* )sgi->idDsk;

    // make sure the caller is not passing inappropriate flags
    if ( sgi->dwFlags ) {
        return ERROR_INVALID_PARAMETER;
    }

    if ( pDisk->m_DiskFlags & LogicalDisk_t::DSK_NEW_IOCTLS ) {

        // Use IOCTL_DISK calls in place of the DISK_IOCTL calls.
        if ( ioctl == DISK_IOCTL_READ ) {
            ioctl = IOCTL_DISK_READ;
        } else if ( ioctl == DISK_IOCTL_WRITE ) {
            ioctl = IOCTL_DISK_WRITE;
        }
    }

    if ( sgi->cfbi > MAX_SG_BUF ) {
        psgreq = ( PSG_REQ )LocalAlloc( LPTR, ( sizeof( SG_REQ ) + sgi->cfbi * sizeof( SG_BUF ) - sizeof( SG_BUF ) ) );
        if ( !psgreq ) {
            return ERROR_OUTOFMEMORY;
        }
    }
    
    psgreq->sr_start    = sgi->dwSector;
    psgreq->sr_num_sec  = sgi->cSectors;
    psgreq->sr_num_sg   = sgi->cfbi;
    psgreq->sr_status   = 0;
    psgreq->sr_callback = NULL;

    for ( i = 0; i < sgi->cfbi; i++ ) {
        psgreq->sr_sglist[i].sb_buf = sgi->pfbi[i].pBuffer;
        psgreq->sr_sglist[i].sb_len = sgi->pfbi[i].cbBuffer;
    }

    if ( !FSDMGR_DiskIoControl( pDisk, ioctl, psgreq, sizeof( SG_REQ ) + sgi->cfbi * sizeof( SG_BUF ) - sizeof( SG_BUF ),
        NULL, 0, &dummy, NULL ) ) {

        result = FsdGetLastError ( );
    } else {

        result = ERROR_SUCCESS;
    }

    if ( ( BYTE* )psgreq != requestBuffer ) {
        LocalFree ( psgreq );
    }

    if ( sgr ) {
        sgr->dwFlags = 0;
        if ( result == ERROR_SUCCESS ) {
            sgr->cSectorsTransferred = sgi->cSectors;
        } else {
            sgr->cSectorsTransferred = 0;
        }
    }

    return result;
}


EXTERN_C DWORD FSDMGR_ReadDisk( PDSK pDsk, DWORD dwSector, DWORD cSectors, PBYTE pBuffer, DWORD cbBuffer )
{

    ASSERT ( FALSE == IsHandle( ( DWORD ) pDsk ) );

    FSD_BUFFER_INFO         bi;
    FSD_SCATTER_GATHER_INFO sgi;

    bi.pBuffer      = pBuffer;
    bi.cbBuffer     = cbBuffer;

    sgi.dwFlags     = 0;
    sgi.idDsk       = ( DWORD )pDsk;
    sgi.dwSector    = dwSector;
    sgi.cSectors    = cSectors;
    sgi.pfdi        = NULL;
    sgi.cfbi        = 1;
    sgi.pfbi        = &bi;
    sgi.pfnCallBack = NULL;

    return ReadWriteDiskEx( &sgi, NULL, DISK_IOCTL_READ );
}

EXTERN_C DWORD FSDMGR_WriteDisk( PDSK pDsk, DWORD dwSector, DWORD cSectors, PBYTE pBuffer, DWORD cbBuffer )
{
    ASSERT ( FALSE == IsHandle( ( DWORD ) pDsk ) );

    FSD_BUFFER_INFO         bi;
    FSD_SCATTER_GATHER_INFO sgi;

    bi.pBuffer      = pBuffer;
    bi.cbBuffer     = cbBuffer;

    sgi.dwFlags     = 0;
    sgi.idDsk       = ( DWORD )pDsk;
    sgi.dwSector    = dwSector;
    sgi.cSectors    = cSectors;
    sgi.pfdi        = NULL;
    sgi.cfbi        = 1;
    sgi.pfbi        = &bi;
    sgi.pfnCallBack = NULL;

    return ReadWriteDiskEx( &sgi, NULL, DISK_IOCTL_WRITE );
}

EXTERN_C DWORD FSDMGR_ReadDiskEx( FSD_SCATTER_GATHER_INFO *pfsgi, FSD_SCATTER_GATHER_RESULTS *pfsgr )
{
    return ReadWriteDiskEx( pfsgi, pfsgr, DISK_IOCTL_READ );
}

EXTERN_C DWORD FSDMGR_WriteDiskEx( FSD_SCATTER_GATHER_INFO *pfsgi, FSD_SCATTER_GATHER_RESULTS *pfsgr )
{
    return ReadWriteDiskEx( pfsgi, pfsgr, DISK_IOCTL_WRITE );
}