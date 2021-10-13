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
#include "stgmarshal.hpp"

extern StoreDisk_t g_Stores;

//---------------------- Store DList Enum Helper functions --------------------------//

static BOOL EnumMatchStoreShortName ( PDLIST pItem, LPVOID pEnumData )
{
    StoreDisk_t* pStore = ( StoreDisk_t* ) GetStoreFromDList( pItem );
    WCHAR* pShortDeviceName = ( WCHAR* ) pEnumData;
    ASSERT( pShortDeviceName );

    WCHAR szStoreShortName[DEVICENAMESIZE];
    VERIFY( GetShortDeviceName( pStore->GetDeviceName( ),szStoreShortName,DEVICENAMESIZE ) );
    return ( _wcsicmp( pShortDeviceName, szStoreShortName ) == 0 );
}


//Delete the DList Item; Used for deleting the whole snapshot
BOOL EnumSnapshotDelete ( PDLIST pItem, LPVOID pEnumData )
{
    PartStoreInfo  *pStorePartInfo = ( PartStoreInfo * ) pItem;
    delete pStorePartInfo;
    return FALSE;
}

//---------------------- Store DList Enum Helper functions --------------------------//



#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

HANDLE WINAPI STG_FindFirstStore( STOREINFO *pInfo, HANDLE hProc )
{
    HANDLE hFindStore = INVALID_HANDLE_VALUE;
    StoreDisk_t *pStore = NULL;
    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_FindFirstStore( )\r\n" ) );
    DWORD dwSize=0;

    // NOTE: pInfo was already mapped by filesys
    __try {
        dwSize = pInfo->cbSize;
    } __except( EXCEPTION_EXECUTE_HANDLER ) {
    }

    if ( dwSize == sizeof( STOREINFO ) ) {

        SEARCHSTORE *pSearch = new StoreHandle_t( hProc,0,STOREHANDLE_TYPE_SEARCH,0 );
        if( pSearch ) {
            LockStoreMgr( );
            DLIST* ptrav=NULL;
            for ( ptrav = g_Stores.m_link.pFwd; &g_Stores.m_link != ptrav; ptrav = ptrav->pFwd ) {
                pStore = GetStoreFromDList( ptrav );

                if ( !pStore->IsDetached( ) ) {
                    PartStoreInfo  *pStorePartInfo = new PartStoreInfo;
                    if ( pStorePartInfo ) {
                        pStore->GetStoreInfo( &pStorePartInfo->u.storeInfo );
                        pStorePartInfo->u.storeInfo.cbSize = sizeof( STOREINFO );
                        AddToDListTail( &pSearch->m_PartStoreInfos.m_link,&pStorePartInfo->m_link );
                    }
                }
            }
            UnlockStoreMgr( );

            if( !IsDListEmpty( &pSearch->m_PartStoreInfos.m_link ) ) {
                PartStoreInfo  *pStorePartInfo = ( PartStoreInfo * ) pSearch->m_PartStoreInfos.m_link.pFwd;
                RemoveDList( &pStorePartInfo->m_link );
                if( CeSafeCopyMemory( pInfo, &pStorePartInfo->u.storeInfo, sizeof( STOREINFO ) ) ) {
                    hFindStore = CreateAPIHandle( g_hFindStoreApi, pSearch );
                    if( !hFindStore ) {
                        DestroyDList( &pSearch->m_PartStoreInfos.m_link, EnumSnapshotDelete, NULL );
                        delete pSearch;
                        pSearch = NULL;
                        hFindStore = INVALID_HANDLE_VALUE;
                        SetLastError( ERROR_GEN_FAILURE );
                    }
                } else { //copy failed
                    delete pSearch;
                    pSearch = NULL;
                    SetLastError( ERROR_BAD_ARGUMENTS );
                }

                delete pStorePartInfo;

            } else { //list empty
                delete pSearch;
                pSearch = NULL;
                SetLastError( ERROR_NO_MORE_ITEMS );
            }
        } else { //!psearch
            SetLastError( ERROR_OUTOFMEMORY );
        }

    } else { //size mismatch
        SetLastError( ERROR_BAD_ARGUMENTS );
    }


    return hFindStore;
}


BOOL WINAPI STG_FindNextStore( SEARCHSTORE *pSearch, STOREINFO *pInfo, DWORD dwSizeOfInfo )
{
#ifdef UNDER_CE
    if ( sizeof ( STOREINFO ) != dwSizeOfInfo ) {
        DEBUGCHK ( 0 ); // FindNextFileW_Trap macro was called directly w/out proper size.
        SetLastError ( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
#else
    //TO DO- Check whether NT provides the size correctly.
    dwSizeOfInfo = sizeof( STOREINFO );
#endif

    BOOL bRet = FALSE;
    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_FindNextStore( 0x%08X )\r\n", pSearch ) );

    if ( dwSizeOfInfo >= sizeof( STOREINFO ) ) {
        if( !IsDListEmpty( &pSearch->m_PartStoreInfos.m_link ) ) {
            PartStoreInfo *pStorePartInfo = ( PartStoreInfo* ) pSearch->m_PartStoreInfos.m_link.pFwd;
            RemoveDList( &pStorePartInfo->m_link );

            //success of FindNextStore routine depends on success of this copy
            bRet = CeSafeCopyMemory( pInfo, &pStorePartInfo->u.storeInfo, sizeof( STOREINFO ) );
            delete pStorePartInfo;

        } else {
            SetLastError( ERROR_NO_MORE_ITEMS );
        }
    } else {//size mismatch
        SetLastError( ERROR_BAD_ARGUMENTS );
    }

    return bRet;
}

BOOL WINAPI STG_FindCloseStore( SEARCHSTORE *pSearch )
{
    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_FindCloseStore( 0x%08X )\r\n", pSearch ) );

    DestroyDList( &pSearch->m_PartStoreInfos.m_link, EnumSnapshotDelete, NULL );
    delete pSearch;

    return TRUE;
}

// This is stub function to support internal OpenStore calls; cannot be called
// externally except in the NT FSDMGR build.
HANDLE WINAPI STG_OpenStore( LPCTSTR szDeviceName, HANDLE hProc )
{
    return STG_OpenStoreEx( szDeviceName, hProc, FILE_ALL_ACCESS );
}

// access that is actually granted to the caller is passed to this function as
// dwAccess because the xxx_OpenStore thunk passes the MAXIMUM_ACCESS bit to
// CreateFile.
HANDLE WINAPI STG_OpenStoreEx( LPCTSTR szDeviceName, HANDLE hProc, DWORD dwAccess )
{
    HANDLE hStore = INVALID_HANDLE_VALUE;
    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_OpenStore( %s )\r\n", szDeviceName ) );
    WCHAR szShortDeviceName[DEVICENAMESIZE] = { 0 };

    //GetShortDeviceName takes care of invalid device name inputs
    if( GetShortDeviceName( szDeviceName, szShortDeviceName, DEVICENAMESIZE ) ) {
        StoreDisk_t* pStore= NULL;
        // NOTE: szDeviceName pointer was already mapped by CreateFile
        LockStoreMgr( );
        DLIST* pdl= EnumerateDList ( &g_Stores.m_link, EnumMatchStoreShortName, ( LPVOID ) szShortDeviceName );
        if( pdl ) {
            pStore = GetStoreFromDList( pdl );
            pStore->AddRef( );
        }
        UnlockStoreMgr( );

        if ( pStore ) {
            hStore = pStore->CreateHandle( hProc,dwAccess );
            pStore->Release( );
        } else {
            SetLastError( ERROR_DEVICE_NOT_AVAILABLE );
        }
    }

    return hStore;
}

// Close a store or partition handle.
BOOL WINAPI STG_CloseHandle( PSTOREHANDLE pStoreHandle )
{
    DEBUGMSG ( ZONE_STOREAPI, ( L"FSDMGR!STG_CloseHandle( 0x%08X )\r\n", pStoreHandle ) );
    pStoreHandle->Close( );
    delete pStoreHandle;
    return TRUE;
}

BOOL WINAPI STG_GetStoreInfo( PSTOREHANDLE pStoreHandle, PSTOREINFO pInfo )
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_GetStoreInfo( 0x%08X )\r\n", pStoreHandle ) );

    StoreDisk_t *pStore = pStoreHandle->m_pStore;
    pStore->Lock( );
    if ( pStore->IsDetached( ) ) {
       dwError = ERROR_DEVICE_NOT_AVAILABLE;
    } else {
         __try {
            if ( pInfo->cbSize == sizeof( STOREINFO ) ) {
                if ( !pStore->GetStoreInfo( pInfo ) ) {
                    dwError = ERROR_GEN_FAILURE;
                }
            } else {
                dwError = ERROR_BAD_ARGUMENTS;
            }
         } __except( EXCEPTION_EXECUTE_HANDLER ) {
            dwError = ERROR_BAD_ARGUMENTS;
         }
    }
    pStore->Unlock( );

    if ( dwError != ERROR_SUCCESS ) {
        SetLastError( dwError );
    }
    return ( dwError == ERROR_SUCCESS );
}

BOOL WINAPI STG_DismountStore( PSTOREHANDLE pStoreHandle )
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_DismountStore( 0x%08X )\r\n", pStoreHandle ) );

    StoreDisk_t *pStore = pStoreHandle->m_pStore;
    pStore->Lock( );

    if ( pStore->IsDetached( ) ) {
       DEBUGMSG ( ZONE_VERBOSE,( TEXT( "STG_DismountStore Store %x ( Ref= %d ) SKIPPED Already Detached \n" ),pStore,pStore->RefCount( ) ) );
    } else {
        ASSERT( SYNCHRONIZE == ( SYNCHRONIZE & pStoreHandle->GetAccess( ) ) ); // kernel enforced
        if ( !pStore->UnmountPartitions( FALSE ) ) { 
            dwError = ERROR_GEN_FAILURE;
        }
    }

    pStore->Unlock( );
    if ( dwError != ERROR_SUCCESS ) {
        SetLastError( dwError );
    }
    return ( dwError == ERROR_SUCCESS );
}

BOOL WINAPI STG_FormatStore( PSTOREHANDLE pStoreHandle )
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_FormatStore( 0x%08X )\r\n", pStoreHandle ) );

    ASSERT( FILE_GENERIC_WRITE == ( FILE_GENERIC_WRITE & pStoreHandle->GetAccess( ) ) ); // kernel enforced
    dwError = pStoreHandle->m_pStore->Format( );

    if ( dwError != ERROR_SUCCESS ) {
        SetLastError( dwError );
    }
    return ( dwError == ERROR_SUCCESS );

}

BOOL CopyPartitionName( LPTSTR pszPartitionNameOut, size_t PartitionNameSize, LPCTSTR pszPartitionName )
{
    BOOL bRet = FALSE;
    __try {
        if ( SUCCEEDED ( StringCchCopy(pszPartitionNameOut, PartitionNameSize, pszPartitionName ) ) ) {
            if ( pszPartitionNameOut [0] != '\0' ) {
                bRet = TRUE;
            }
        }
    } __except ( EXCEPTION_EXECUTE_HANDLER ) {
    }

    return bRet;
}


BOOL WINAPI STG_CreatePartition( PSTOREHANDLE pStoreHandle, LPCTSTR pszPartitionName, DWORD dwPartType, DWORD dwHighSec, DWORD dwLowSec, BOOL bAuto )
{
    DWORD dwError = ERROR_SUCCESS;
    SECTORNUM snNumSectors = ( ( SECTORNUM )dwHighSec << 32 ) | dwLowSec;
    WCHAR szPartitionNameLocal[PARTITIONNAMESIZE];

    if ( CopyPartitionName(szPartitionNameLocal, PARTITIONNAMESIZE, pszPartitionName ) ) {
        pszPartitionName = ( LPCTSTR ) szPartitionNameLocal;
    } else {
        pszPartitionName = NULL;
        dwError = ERROR_BAD_ARGUMENTS;
    }

    if ( pszPartitionName ) {
        DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_CreatePartition( 0x%08X ) %s PartType=%ld %ld( 0x%08X%08X ) Auto=%s\r\n",
                  pStoreHandle,
                  pszPartitionName,
                  dwPartType,
                  dwLowSec,
                  ( DWORD )( snNumSectors >> 32 ),
                  ( DWORD )snNumSectors,
                  bAuto ? L"TRUE" : L"FALSE" ) );

        ASSERT( FILE_GENERIC_WRITE == ( FILE_GENERIC_WRITE & pStoreHandle->GetAccess( ) ) ); // kernel enforced
        dwError = pStoreHandle->m_pStore->CreatePartition( pszPartitionName, ( BYTE )dwPartType, snNumSectors, bAuto );
    }

    if ( dwError != ERROR_SUCCESS ) {
        SetLastError( dwError );
    }
    return ( dwError == ERROR_SUCCESS );
}

BOOL WINAPI STG_DeletePartition( PSTOREHANDLE pStoreHandle, LPCTSTR pszPartitionName )
{
    DWORD dwError = ERROR_SUCCESS;
    WCHAR szPartitionNameLocal[PARTITIONNAMESIZE];

    if ( CopyPartitionName(szPartitionNameLocal, PARTITIONNAMESIZE, pszPartitionName ) ) {
        pszPartitionName = ( LPCTSTR ) szPartitionNameLocal;
    } else {
        pszPartitionName = NULL;
        dwError = ERROR_BAD_ARGUMENTS;
    }

    if ( pszPartitionName ) {
        DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_DeletePartition( 0x%08X ) Name=%s\r\n", pStoreHandle,pszPartitionName ) );

        ASSERT( FILE_GENERIC_WRITE == ( FILE_GENERIC_WRITE & pStoreHandle->GetAccess( ) ) ); // kernel enforced
        dwError = pStoreHandle->m_pStore->DeletePartition( pszPartitionName );
    }

    if ( dwError != ERROR_SUCCESS ) {
        SetLastError( dwError );
    }
    return ( dwError == ERROR_SUCCESS );
}

HANDLE WINAPI STGINT_OpenPartition( PSTOREHANDLE pStoreHandle, LPCTSTR pszPartitionName )
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_OpenPartition( 0x%08X ) Name=%s\r\n", pStoreHandle,pszPartitionName ) );
    HANDLE hPartition = INVALID_HANDLE_VALUE;

    ASSERT( READ_CONTROL == ( READ_CONTROL & pStoreHandle->GetAccess( ) ) ); // kernel enforced
    dwError = pStoreHandle->m_pStore->OpenPartitionEx( pszPartitionName, &hPartition, pStoreHandle->GetProc( ), pStoreHandle->GetAccess( ) );

    if ( dwError != ERROR_SUCCESS ) {
        SetLastError( dwError );
    }
    return hPartition;
}

#ifdef UNDER_CE
HANDLE WINAPI STGEXT_OpenPartition( PSTOREHANDLE pStoreHandle, LPCTSTR pszPartitionName )
{
    DWORD dwError = ERROR_SUCCESS;
    HANDLE hPartition = INVALID_HANDLE_VALUE;
    WCHAR szLocalName[PARTITIONNAMESIZE];

    if ( CopyPartitionName(szLocalName, PARTITIONNAMESIZE, pszPartitionName ) ) {
        pszPartitionName = ( LPCTSTR ) szLocalName;
    } else {
        pszPartitionName = NULL;
        dwError = ERROR_BAD_ARGUMENTS;
    }

    if ( pszPartitionName ) {
        // Invoke internal OpenPartition API with copied parameter.
        hPartition = STGINT_OpenPartition ( pStoreHandle, pszPartitionName );
        if ( INVALID_HANDLE_VALUE != hPartition ) {
            if ( !FsdDuplicateHandle( ( HANDLE )GetCurrentProcessId( ), hPartition,
                                reinterpret_cast<HANDLE> ( GetCallerVMProcessId ( ) ),
                                &hPartition, 0, FALSE,
                                DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE ) ) {
                dwError = FsdGetLastError ( ERROR_OUTOFMEMORY );
                DEBUGCHK( 0 );
            }
        }
    }

    if ( dwError != ERROR_SUCCESS ) {
        SetLastError( dwError );
    }
    return hPartition;
}
#endif


BOOL WINAPI STG_MountPartition( PSTOREHANDLE pStoreHandle )
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_MountPartition( 0x%08X )\r\n", pStoreHandle ) );

    StoreDisk_t *pStore = pStoreHandle->m_pStore;
    pStore->Lock( );

    if ( pStore->IsDetached( ) ) {
       dwError = ERROR_DEVICE_NOT_AVAILABLE;
    } else {
        ASSERT( SYNCHRONIZE == ( SYNCHRONIZE & pStoreHandle->GetAccess( ) ) ); // kernel enforced
        dwError = pStore->MountPartition( pStoreHandle->m_pPartition );
    }

    pStore->Unlock( );

    if ( dwError != ERROR_SUCCESS ) {
        SetLastError( dwError );
    }

    return ( dwError == ERROR_SUCCESS );
}


BOOL WINAPI STG_DismountPartition( PSTOREHANDLE pStoreHandle )
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_DismountPartition( 0x%08X )\r\n", pStoreHandle ) );

    ASSERT( SYNCHRONIZE == ( SYNCHRONIZE & pStoreHandle->GetAccess( ) ) ); // kernel enforced
    dwError = pStoreHandle->m_pStore->UnmountPartition( pStoreHandle->m_pPartition );

    if ( dwError != ERROR_SUCCESS ) {
        SetLastError( dwError );
    }
    return ( dwError == ERROR_SUCCESS );
}


BOOL WINAPI STG_RenamePartition( PSTOREHANDLE pStoreHandle, LPCTSTR pszNewPartitionName )
{
    DWORD dwError = ERROR_SUCCESS;
    WCHAR szPartitionNameLocal[PARTITIONNAMESIZE];
    if ( CopyPartitionName(szPartitionNameLocal, PARTITIONNAMESIZE, pszNewPartitionName ) ) {
        pszNewPartitionName = ( LPCTSTR ) szPartitionNameLocal;
    } else {
        pszNewPartitionName = NULL;
        dwError = ERROR_BAD_ARGUMENTS;
    }

    if ( pszNewPartitionName ) {
        DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_RenamePartition( 0x%08X ) Name=%s\r\n", pStoreHandle, pszNewPartitionName ) );
        ASSERT( FILE_GENERIC_WRITE == ( FILE_GENERIC_WRITE & pStoreHandle->GetAccess( ) ) ); // kernel enforced
        dwError = pStoreHandle->m_pStore->RenamePartition( pStoreHandle->m_pPartition, pszNewPartitionName );
    }

    if ( dwError != ERROR_SUCCESS ) {
        SetLastError( dwError );
    }
    return ( dwError == ERROR_SUCCESS );
}


BOOL WINAPI STG_SetPartitionAttributes( PSTOREHANDLE pStoreHandle, DWORD dwAttrs )
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_SetPartitionAttributes( 0x%08X ) Attrs=%08X\r\n", pStoreHandle, dwAttrs ) );

    ASSERT( FILE_GENERIC_WRITE == ( FILE_GENERIC_WRITE & pStoreHandle->GetAccess( ) ) ); // kernel enforced
    dwError = pStoreHandle->m_pStore->SetPartitionAttributes( pStoreHandle->m_pPartition, dwAttrs );

    if ( dwError != ERROR_SUCCESS ) {
        SetLastError( dwError );
    }
    return ( dwError == ERROR_SUCCESS );
}

static BOOL IoctlAccessCheck( PSTOREHANDLE pStoreHandle, DWORD dwIoControlCode )
{
    DWORD dwAccess = 0;

    switch( dwIoControlCode )
    {
        // Basic query IOCTLs require READ_CONTROL.
        case IOCTL_DISK_GETINFO:
        case DISK_IOCTL_GETINFO:
        case IOCTL_DISK_DEVICE_INFO:
            dwAccess = READ_CONTROL;
            break;

        // Retrieving the storage ID requires more than query access alone.
        case IOCTL_DISK_GET_STORAGEID:
            dwAccess = FILE_GENERIC_READ;
            break;

        // Any other IOCTLs require full access by default.
        default:
            dwAccess = FILE_ALL_ACCESS;
            break;
    }

    return ( dwAccess == ( pStoreHandle->GetAccess( ) & dwAccess ) );
}

BOOL WINAPI STG_DeviceIoControl( PSTOREHANDLE pStoreHandle, DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned, OVERLAPPED* /* pOverlapped */ )
{
    PVOID pInBufOrig = pInBuf;
    PBYTE PtrBackup[MAX_SG_BUF];
    LRESULT lResult;

    BOOL fTrustedCaller = ( GetDirectCallerProcessId( ) == GetCurrentProcessId( ) );

    if ( !IoctlAccessCheck( pStoreHandle, dwIoControlCode ) ) {
        // Handle opened with insufficient rights to perform the specified IOCTL.
        lResult = ERROR_ACCESS_DENIED;
        goto exit_ioctl;
    }

    if ( !fTrustedCaller ) {
        // For untrusted callers, copy and validate access to the embedded pointers in
        // SG_REQ and CDROM_READ structures.
        //
        // SECURITY NOTE: Any IOCTLs with embedded pointers that we're not aware of and
        // don't validate here need to be marshalled by the partition driver.
        //
        switch ( dwIoControlCode ) {
            case IOCTL_DISK_WRITE:
            case DISK_IOCTL_WRITE:
                lResult = CeOpenSGRequest ( ( PSG_REQ* )&pInBuf, ( SG_REQ* )pInBufOrig, nInBufSize, ARG_I_PTR, PtrBackup, MAX_SG_BUF, pStoreHandle->m_pStore->m_di.di_bytes_per_sect );
                if ( ERROR_SUCCESS != lResult ) {
                    goto exit_ioctl;
                }
                break;

            case IOCTL_DISK_READ:
            case DISK_IOCTL_READ:
                lResult = CeOpenSGRequest ( ( PSG_REQ* )&pInBuf, ( SG_REQ* )pInBufOrig, nInBufSize, ARG_O_PTR, PtrBackup, MAX_SG_BUF, pStoreHandle->m_pStore->m_di.di_bytes_per_sect );
                if ( ERROR_SUCCESS != lResult ) {
                    goto exit_ioctl;
                }
                break;

            case IOCTL_CDROM_READ_SG:
                lResult = CeOpenSGXRequest ( ( PCDROM_READ* )&pInBuf, ( CDROM_READ* )pInBufOrig, nInBufSize, ARG_O_PTR, PtrBackup, MAX_SG_BUF );
                if ( ERROR_SUCCESS != lResult ) {
                    goto exit_ioctl;
                }
                break;
        }
    }

    __try {
        lResult = pStoreHandle->m_pStore->DeviceIoControl ( pStoreHandle->m_pPartition, dwIoControlCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize, pBytesReturned );
    }__except( EXCEPTION_EXECUTE_HANDLER ) {
        lResult = ERROR_INVALID_PARAMETER;
    }

    if ( !fTrustedCaller ) {
        switch ( dwIoControlCode ) {
            LRESULT TempResult;
            case IOCTL_DISK_WRITE:
            case DISK_IOCTL_WRITE:
                TempResult = CeCloseSGRequest ( ( SG_REQ* )pInBuf, ( SG_REQ* )pInBufOrig, nInBufSize, ARG_I_PTR, PtrBackup, MAX_SG_BUF );
                DEBUGCHK ( ERROR_SUCCESS == TempResult );
                break;

            case IOCTL_DISK_READ:
            case DISK_IOCTL_READ:
                TempResult = CeCloseSGRequest ( ( SG_REQ* )pInBuf, ( SG_REQ* )pInBufOrig, nInBufSize, ARG_O_PTR, PtrBackup, MAX_SG_BUF );
                DEBUGCHK ( ERROR_SUCCESS == TempResult );
                break;

            case IOCTL_CDROM_READ_SG:
                TempResult = CeCloseSGXRequest ( ( CDROM_READ* )pInBuf, ( CDROM_READ* )pInBufOrig, nInBufSize, ARG_O_PTR, PtrBackup, MAX_SG_BUF );
                DEBUGCHK ( ERROR_SUCCESS == TempResult );
                break;
        }
    }

exit_ioctl:

    if ( ERROR_SUCCESS != lResult ) {
        SetLastError ( lResult );
    }
    return ( ERROR_SUCCESS == lResult );
}


BOOL WINAPI STG_GetPartitionInfo( PSTOREHANDLE pStoreHandle, PPARTINFO pInfo )
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_GetPartitionInfo( 0x%08X )\r\n", pStoreHandle ) );

    StoreDisk_t *pStore = pStoreHandle->m_pStore;
    pStore->Lock( );

    if ( pStore->IsDetached( ) ) {
       dwError = ERROR_DEVICE_NOT_AVAILABLE;
    }else {
        __try {
            ASSERT( READ_CONTROL == ( READ_CONTROL & pStoreHandle->GetAccess( ) ) ); // kernel enforced
            if ( pInfo->cbSize == sizeof( PARTINFO ) ) {
                dwError = pStore->GetPartitionInfo( pStoreHandle->m_pPartition, pInfo );
            } else {
                dwError = ERROR_BAD_ARGUMENTS;
            }
        } __except( EXCEPTION_EXECUTE_HANDLER ) {
            dwError = ERROR_BAD_ARGUMENTS;
        }
    }

    pStore->Unlock( );

    if ( dwError != ERROR_SUCCESS ) {
        SetLastError( dwError );
    }
    return ( dwError == ERROR_SUCCESS );
}


BOOL WINAPI STG_FormatPartition( PSTOREHANDLE pStoreHandle, DWORD bPartType, BOOL bAuto )
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_FormatPartition( 0x%08X ) PartType=%ld Auto=%s\r\n", pStoreHandle, bPartType, bAuto ? L"TRUE" : L"FALSE" ) );

    ASSERT( FILE_GENERIC_WRITE == ( FILE_GENERIC_WRITE & pStoreHandle->GetAccess( ) ) ); // kernel enforced
    dwError = pStoreHandle->m_pStore->FormatPartition( pStoreHandle->m_pPartition, ( BYTE )bPartType, bAuto );

    if ( dwError != ERROR_SUCCESS ) {
        SetLastError( dwError );
    }
    return ( dwError == ERROR_SUCCESS );
}

HANDLE WINAPI STGINT_FindFirstPartition( PSTOREHANDLE pStoreHandle, PPARTINFO pInfo )
{
    DWORD dwError = ERROR_SUCCESS;
    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_FindFirstPartition( 0x%08X )\r\n", pStoreHandle ) );
    HANDLE hSearch = INVALID_HANDLE_VALUE;

    __try {
        ASSERT( READ_CONTROL == ( READ_CONTROL & pStoreHandle->GetAccess( ) ) ); // kernel enforced
        if ( pInfo->cbSize == sizeof( PARTINFO ) ) {
            dwError = pStoreHandle->m_pStore->FindFirstPartition( pInfo, &hSearch, pStoreHandle->GetProc( ) );
        } else {
            dwError = ERROR_BAD_ARGUMENTS;
        }
    } __except( EXCEPTION_EXECUTE_HANDLER ) {
        dwError = ERROR_BAD_ARGUMENTS;
    }

    if ( dwError != ERROR_SUCCESS ) {
        SetLastError( dwError );
    }
    return hSearch;
}

#ifdef UNDER_CE
HANDLE WINAPI STGEXT_FindFirstPartition( PSTOREHANDLE pStoreHandle, PPARTINFO pInfo )
{
    HANDLE h = STGINT_FindFirstPartition ( pStoreHandle, pInfo );
    if ( INVALID_HANDLE_VALUE != h ) {
        if ( FsdDuplicateHandle( ( HANDLE )GetCurrentProcessId( ), h,
                            reinterpret_cast<HANDLE> ( GetCallerVMProcessId ( ) ),
                            &h, 0, FALSE,
                            DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE ) ) {
            return h;
        }
        DEBUGCHK( 0 );
    }
    return INVALID_HANDLE_VALUE;
}
#endif

//Partition Search Handle
BOOL WINAPI STG_FindNextPartition( SEARCHPARTITION *pSearch, PPARTINFO pInfo, DWORD dwSizeOfInfo )
{
#ifdef UNDER_CE
    if ( sizeof ( PARTINFO ) != dwSizeOfInfo ) {
        DEBUGCHK ( 0 ); // FindNextFileW_Trap macro was called directly w/out proper size.
        SetLastError ( ERROR_INVALID_PARAMETER );
    }
#else
    //TO DO- Check whether NT provides the size correctly.
    dwSizeOfInfo = sizeof( PARTINFO );
#endif

    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_FindNextPartition( 0x%08X )\r\n", pSearch ) );
    DWORD dwError = ERROR_BAD_ARGUMENTS;

    if ( dwSizeOfInfo >= sizeof( PARTINFO ) ) {
        if( !IsDListEmpty( &pSearch->m_PartStoreInfos.m_link ) ) {
            PartStoreInfo* pStorePartInfo = ( PartStoreInfo * ) pSearch->m_PartStoreInfos.m_link.pFwd;
            RemoveDList( &pStorePartInfo->m_link );

            //success of FindNextPartition routine depends on success of this copy
            if( CeSafeCopyMemory( pInfo, &pStorePartInfo->u.partInfo, sizeof( PARTINFO ) ) ) {
               dwError = ERROR_SUCCESS;
            } else {
               dwError = ERROR_BAD_ARGUMENTS;
            }
            delete pStorePartInfo;
        } else {
            dwError = ERROR_NO_MORE_ITEMS;
        }
    }
    if ( dwError != ERROR_SUCCESS ) {
        SetLastError( dwError );
    }
    return ( dwError == ERROR_SUCCESS );
}

BOOL WINAPI STG_FindClosePartition( SEARCHPARTITION *pSearch )
{
    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_FindClosePartition( 0x%08X )\r\n", pSearch ) );
    DestroyDList( &pSearch->m_PartStoreInfos.m_link, EnumSnapshotDelete, NULL );
    delete pSearch;
    return ( TRUE );
}


#ifndef UNDER_CE
BOOL WINAPI STG_SetPartitionSize( PSTOREHANDLE pStoreHandle, DWORD dwHighSec, DWORD dwLowSec )
{
    DWORD dwError = ERROR_SUCCESS;
    SECTORNUM snNumSectors = ( ( SECTORNUM )dwHighSec << 32 ) | dwLowSec;
    DEBUGMSG( ZONE_STOREAPI, ( L"FSDMGR:STG_SetPartitionSize( 0x%08X ) %ld( 0x%08X%08X )\r\n",
        pStoreHandle, dwLowSec, ( DWORD )( snNumSectors >> 32 ), ( DWORD )snNumSectors ) );

        StoreDisk_t *pStore = pStoreHandle->m_pStore;
        pStore->Lock( );

        if ( pStore->IsDetached( ) ) {
           dwError = ERROR_DEVICE_NOT_AVAILABLE;
        } else {
            dwError = pStore->SetPartitionSize( pStoreHandle->m_pPartition, snNumSectors );
        }

        pStore->Unlock( );

    if ( dwError != ERROR_SUCCESS ) {
        SetLastError( dwError );
    }
    return ( dwError == ERROR_SUCCESS );
}
#endif // !UNDER_CE

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !UNDER_CE */
