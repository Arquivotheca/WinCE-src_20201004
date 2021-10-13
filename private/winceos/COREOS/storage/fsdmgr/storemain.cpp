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

#ifdef UNDER_CE

#include "storeincludes.hpp"
#include <map>
#include "fsdmapincludes.h"

#ifdef UNDER_CE
#include <msgqueue.h>
#include <pnp.h>
#include <windev.h>
#endif

using namespace std;

CRITICAL_SECTION g_csStoreMgr;

static map <TCHAR*, StoreDisk_t*, lt_compare> * s_pStoreMap = NULL;

HANDLE g_hFindStoreApi  = NULL;
HANDLE g_hFindPartApi   = NULL;
HANDLE g_hStoreApi      = NULL;
HANDLE g_hSTRMGRApi     = NULL;
HANDLE g_hStoreDiskApi  = NULL;
HANDLE g_hPNPQueue      = NULL;
HANDLE g_hPNPThread     = NULL;
HANDLE g_hPNPUpdateEvent = NULL;
DWORD  g_dwUpdateState  = 0;

DWORD  g_dwWaitIODelay = 0;

HANDLE g_hAutoLoadEvent = NULL;

static BOOL s_bFreshMap = FALSE;

HANDLE STRMGR_CreateFileW ( DWORD dwData, HANDLE hProc, PCWSTR pwsFileName, DWORD dwAccess, DWORD dwShareMode,
                          PSECURITY_ATTRIBUTES pSecurityAttributes, DWORD dwCreate, DWORD dwFlagsAndAttributes,
                          HANDLE hTemplateFile, PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD SecurityDescriptorSize );
BOOL   STRMGR_RefreshStore ( DWORD dwData, const WCHAR* pFileName, const WCHAR*  pReserved );
HANDLE STRMGR_FindFirstFileW ( DWORD dwData, HANDLE hProc, PCWSTR pwsFileSpec, PWIN32_FIND_DATAW pfd, DWORD SizeOfFindData );
BOOL   STRMGR_CloseAllHandles ( DWORD dwData, HANDLE hProc );
DWORD  STRMGR_StubFunction ( ){   return 0;}

#ifdef UNDER_CE
//Conversion Functions to forward the request to the m_hDisk associated with the Store.
BOOL CONV_DevCloseFileHandle (StoreDisk_t **ppStore);
BOOL CONV_DevDeviceIoControl (StoreDisk_t **ppStore, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) ;
#endif

typedef HANDLE ( * PREQUESTDEVICENOT ) ( const GUID *devclass, HANDLE hMsgQ, BOOL fAll );
typedef BOOL ( * PSTOPDEVICENOT ) ( HANDLE h );

#ifdef UNDER_CE

// NOTE: The store and partition enumeration API sets really just map to
// the standard file enumeration API set ( HT_FIND ).
CONST PFNAPI apfnFindStoreAPIs[NUM_FIND_APIS] = {
        ( PFNAPI )STG_FindCloseStore,                     // FindClose
        ( PFNAPI )NULL,
        ( PFNAPI )STG_FindNextStore,                      // FindNextFileW
};

CONST ULONGLONG asigFindStoreAPIs[NUM_FIND_APIS] = {
        FNSIG1 ( DW ),                                     // FindClose
        FNSIG0 ( ),                                       //
        FNSIG3 ( DW,IO_PTR,DW )                            // FindNextFileW
};

CONST PFNAPI apfnFindPartAPIs[NUM_FIND_APIS] = {
        ( PFNAPI )STG_FindClosePartition,
        ( PFNAPI )NULL,
        ( PFNAPI )STG_FindNextPartition,
};

CONST ULONGLONG asigFindPartAPIs[NUM_FIND_APIS] = {
        FNSIG1 ( DW ),                                     // FindClose
        FNSIG0 ( ),                                       //
        FNSIG3 ( DW,IO_PTR,DW )                            // FindNextFileW
};

// NOTE: The store-handle API set really just maps to the standard handle-
// based file API set ( HT_FILE ). Since invalid handle values are caught by
// the kernel before ever passing to these functions, OpenPartition and
// FindFirstPartition map to GetFileSize and SetFilePointer, which the
// kernel knows to return -1 ( aka INVALID_HANDLE_VALUE ) given a bad handle.
// The other functions map to file APIs which return FALSE given a bad handle.
CONST PFNAPI apfnSTGExternalAPIs[] = {
        ( PFNAPI )STG_CloseHandle,                        // CloseHandle
        ( PFNAPI )NULL,                                   // <pre-close>
        ( PFNAPI )STG_DismountStore,                      // ReadFile
        ( PFNAPI )STG_FormatStore,                        // WriteFile
        ( PFNAPI )STGEXT_OpenPartition,                   // GetFileSize
        ( PFNAPI )STGEXT_FindFirstPartition,              // SetFilePointer
        ( PFNAPI )STG_CreatePartition,                    // GetFileInformationByHandle
        ( PFNAPI )STG_MountPartition,                     // FlushFileBuffers
        ( PFNAPI )STG_DismountPartition,                  // GetFileTime
        ( PFNAPI )STG_RenamePartition,                    // SetFileTime
        ( PFNAPI )STG_SetPartitionAttributes,             // SetEndOfFile
        ( PFNAPI )STG_DeviceIoControl,                    // DeviceIoControl
        ( PFNAPI )STG_GetPartitionInfo,                   // ReadFileWithSeek
        ( PFNAPI )STG_FormatPartition,                    // WriteFileWithSeek
        ( PFNAPI )STG_DeletePartition,                    // LockFileEx
        ( PFNAPI )STG_GetStoreInfo                        // UnlockFileEx
};

CONST PFNAPI apfnSTGInternalAPIs[] = {
        ( PFNAPI )STG_CloseHandle,
        ( PFNAPI )NULL,
        ( PFNAPI )STG_DismountStore,
        ( PFNAPI )STG_FormatStore,
        ( PFNAPI )STGINT_OpenPartition,
        ( PFNAPI )STGINT_FindFirstPartition,
        ( PFNAPI )STG_CreatePartition,
        ( PFNAPI )STG_MountPartition,
        ( PFNAPI )STG_DismountPartition,
        ( PFNAPI )STG_RenamePartition,
        ( PFNAPI )STG_SetPartitionAttributes,
        ( PFNAPI )STG_DeviceIoControl,
        ( PFNAPI )STG_GetPartitionInfo,
        ( PFNAPI )STG_FormatPartition,
        ( PFNAPI )STG_DeletePartition,
        ( PFNAPI )STG_GetStoreInfo
};

// Kernel will enforce the access rights on store and partition handles.
ACCESS_MASK aamSTGAccessRequired[] = {
        0,                      // CloseHandle
        0,                      // <pre-close>
        SYNCHRONIZE,            // DismountStore
        FILE_GENERIC_WRITE,     // FormatStore
        READ_CONTROL,           // OpenPartition
        READ_CONTROL,           // FindFirstPartition
        FILE_GENERIC_WRITE,     // CreatePartition
        SYNCHRONIZE,            // MountPartition
        SYNCHRONIZE,            // DismountPartition
        FILE_GENERIC_WRITE,     // RenamePartition
        FILE_GENERIC_WRITE,     // SetPartitionAttributes
        0,                      // DeviceIoControl ( NOTE: access enforced in API implementation )
        READ_CONTROL,           // GetPartitionInfo
        FILE_GENERIC_WRITE,     // FormatPartition
        FILE_GENERIC_WRITE,     // DeletePartition
        READ_CONTROL            // GetStoreInfo
};

CONST ULONGLONG asigSTGAPIs[] = {
        FNSIG1 ( DW ),                     // CloseHandle
        FNSIG0 ( ),                       //
        FNSIG1 ( DW ),                     // DismountStore
        FNSIG1 ( DW ),                     // FormatStore
        FNSIG2 ( DW,I_WSTR ),              // OpenPartition
        FNSIG2 ( DW,IO_PDW ),              // FindFirstPartition
        FNSIG6 ( DW,I_WSTR,DW,DW,DW,DW ),  // CreatePartition
        FNSIG1 ( DW ),                     // MountPartition,
        FNSIG1 ( DW ),                     // DismountPartition
        FNSIG2 ( DW,I_WSTR ),              // RenamePartition
        FNSIG2 ( DW,DW ),                  // SetPartitionAttributes
        FNSIG8 ( DW,DW,IO_PTR,DW,IO_PTR,DW,O_PDW,IO_PDW ), // DeviceIoControl
        FNSIG2 ( DW,IO_PDW ),              // GetPartitionInfo
        FNSIG3 ( DW,DW,DW ),               // FormatPartition
        FNSIG2 ( DW,I_WSTR ),              // DeletePartition
        FNSIG2 ( DW,IO_PDW )               // GetStoreInfo
};

EXTERN_C DWORD StorageAPIErrorHandler ( DWORD dwIndex )
{
    DWORD dwReturn = 0;
    switch ( dwIndex )
    {
        // APIs that return FALSE on failure
        //
        case  0: __fallthrough;         // CloseHandle
        case  1: __fallthrough;         // Pre-Close
        case  2: __fallthrough;         // DismountStore
        case  3: __fallthrough;         // FormatStore
        case  6: __fallthrough;         // CreatePartition
        case  7: __fallthrough;         // MountPartition
        case  8: __fallthrough;         // DismountPartition
        case  9: __fallthrough;         // RenamePartition
        case 10: __fallthrough;         // SetPartitionAttributes
        case 11: __fallthrough;         // DeviceIoControl
        case 12: __fallthrough;         // GetPartitionInfo
        case 13: __fallthrough;         // FormatPartition
        case 14: __fallthrough;         // DeletePartition
        case 15:                        // DeletePartition
            dwReturn = FALSE;
            break;

        // APIs that return INVALID_HANDLE_VALUE on failure
        //
        case  4: __fallthrough;         // OpenPartition
        case  5:                        // FindFirstPartition
            dwReturn = ( DWORD )INVALID_HANDLE_VALUE;
            break;

        default:
            DEBUGCHK ( 0 ); // unexpectd API ID
            dwReturn = 0;
            break;
    }

    return dwReturn;
}

C_ASSERT ( _countof ( asigSTGAPIs ) == _countof ( aamSTGAccessRequired ) );
C_ASSERT ( _countof ( aamSTGAccessRequired ) == _countof ( apfnSTGInternalAPIs ) );
C_ASSERT ( _countof ( apfnSTGInternalAPIs ) == _countof ( apfnSTGExternalAPIs ) );

CONST PFNAPI apfnSTGMGRAPIs[NUM_AFS_APIS] = {
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )NULL,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_CreateFileW,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_RefreshStore,
        ( PFNAPI )STRMGR_FindFirstFileW,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_CloseAllHandles,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_StubFunction,
        ( PFNAPI )STRMGR_StubFunction
};

CONST ULONGLONG asigSTGMGRAPIs[NUM_AFS_APIS] = {
        FNSIG0 ( ),                                       // CloseVolume
        FNSIG0 ( ),                                       //
        FNSIG0 ( ),                                       // CreateDirectoryW
        FNSIG0 ( ),                                       // RemoveDirectoryW
        FNSIG0 ( ),                                       // GetFileAttributesW
        FNSIG0 ( ),                                       // SetFileAttributesW
        FNSIG9 ( DW,DW,I_WSTR,DW,DW,IO_PDW,DW,DW,DW ),     // CreateFileW
        FNSIG0 ( ),                                       // DeleteFileW
        FNSIG3 ( DW,I_WSTR,I_WSTR ),                       // MoveFileW
        FNSIG5 ( DW,DW,I_WSTR,IO_PTR,DW ),                 // FindFirstFileW
        FNSIG0 ( ),                                       // CeRegisterFileSystemNotification
        FNSIG0 ( ),                                       // CeOidGetInfo
        FNSIG0 ( ),                                       // PrestoChangoFileName
        FNSIG2 ( DW,DW ),                                  // CloseAllFiles
        FNSIG0 ( ),                                       // GetDiskFreeSpace
        FNSIG0 ( ),                                       // Notify
        FNSIG0 ( ),                                       // CeRegisterFileSystemFunction
        FNSIG0 ( ),                                       // FindFirstChangeNotification
        FNSIG0 ( ),                                       // FindNextChangeNotification
        FNSIG0 ( ),                                       // FindCloseChangeNotification
        FNSIG0 ( ),                                       // CeGetFileNotificaitonInfo
        FNSIG0 ( ),                                       // FsIoControl
        FNSIG0 ( ),                                       // SetFileSecurityW
        FNSIG0 ( ),                                       // GetFileSecurityW
};

const PFNVOID apfnDevFileDirectApiMethods[NUM_FILE_APIS] = {
    (PFNVOID)CONV_DevCloseFileHandle,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)CONV_DevDeviceIoControl,
};

const PFNVOID apfnDevFileApiMethods[NUM_FILE_APIS] = {
    (PFNVOID)CONV_DevCloseFileHandle,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)CONV_DevDeviceIoControl,
};


const ULONGLONG apfnDevFileApiSigs[NUM_FILE_APIS] = {
    FNSIG1(DW),                                 // CloseFileHandle
    FNSIG0(),
    FNSIG5(DW,O_PTR,DW,O_PDW,IO_PDW),           // ReadFile
    FNSIG5(DW,I_PTR,DW,O_PDW,IO_PDW),           // WriteFile
    FNSIG2(DW,O_PDW),                           // GetFileSize
    FNSIG4(DW,DW,IO_PDW,DW),                    // SetFilePointer
    FNSIG2(DW,IO_PDW),                          // GetFileInformationByHandle
    FNSIG1(DW),                                 // FlushFileBuffers
    FNSIG4(DW,O_PI64,O_PI64,O_PI64),            // GetFileTime
    FNSIG4(DW,IO_PI64,IO_PI64,IO_PI64),         // SetFileTime
    FNSIG1(DW),                                 // SetEndOfFile,
    FNSIG8(DW,DW,IO_PTR,DW,IO_PTR,DW,O_PDW,IO_PDW), // DeviceIoControl
    FNSIG7(DW,O_PTR,DW,O_PDW,IO_PDW,DW,DW),     // ReadFileWithSeek
    FNSIG7(DW,I_PTR,DW,O_PDW,IO_PDW,DW,DW),     // WriteFileWithSeek
    FNSIG6(DW,DW,DW,DW,DW,IO_PDW),              // LockFileEx
    FNSIG5(DW,DW,DW,DW,IO_PDW),                 // UnlockFileEx
};

#endif // UNDER_CE

//---------------------- Store DList Enum Helper functions --------------------------//

enum StoreState { DETACHED, MOUNTED };
struct StoreFindInfo
{
    TCHAR      szStoreName[STORENAMESIZE];
    StoreState findByState;
};


LRESULT InitializeStoreAPI ( )
{
    DEBUGMSG ( ZONE_INIT || ZONE_VERBOSE, ( L"FSDMGR!InitializeStoreAPI\r\n" ) );

    InitializeCriticalSection ( &g_csStoreMgr );

#ifdef UNDER_CE
    // Create the store enumeration API set ( this is an HT_FIND set ).
    g_hFindStoreApi = CreateAPISet ( const_cast<CHAR*> ( "FSTG" ), sizeof ( apfnFindStoreAPIs )/sizeof ( apfnFindStoreAPIs[0] ),
        reinterpret_cast<const PFNVOID *> ( apfnFindStoreAPIs ), asigFindStoreAPIs );
    if ( !g_hFindStoreApi ) {
        return FsdGetLastError ( ERROR_GEN_FAILURE );
    }
    // Create the partition enumeration API set ( this is an HT_FIND set ).
    g_hFindPartApi = CreateAPISet ( const_cast<CHAR*> ( "FPRT" ), sizeof ( apfnFindPartAPIs )/sizeof ( apfnFindPartAPIs[0] ),
        reinterpret_cast<const PFNVOID *> ( apfnFindPartAPIs ), asigFindPartAPIs );
    if ( !g_hFindPartApi ) {
        return FsdGetLastError ( ERROR_GEN_FAILURE );
    }
    // Create the store/partition I/O APi set ( this is an HT_FILE set ).
    g_hStoreApi  = CreateAPISet ( const_cast<CHAR*> ( "STRG" ), sizeof ( apfnSTGExternalAPIs )/sizeof ( apfnSTGExternalAPIs[0] ),
        reinterpret_cast<const PFNVOID *> ( apfnSTGExternalAPIs ), asigSTGAPIs );
    if ( !g_hStoreApi ) {
        return FsdGetLastError ( ERROR_GEN_FAILURE );
    }

    // Register the previously created API sets.
    VERIFY ( RegisterAPISet ( g_hFindStoreApi, HT_FIND | REGISTER_APISET_TYPE ) );
    VERIFY ( RegisterAPISet ( g_hFindPartApi, HT_FIND | REGISTER_APISET_TYPE ) );
    VERIFY ( RegisterAPISet ( g_hStoreApi, HT_FILE | REGISTER_APISET_TYPE ) );
    VERIFY ( RegisterDirectMethods ( g_hStoreApi, ( const PFNVOID* )apfnSTGInternalAPIs ) );
    VERIFY ( SetAPIErrorHandler ( g_hStoreApi, StorageAPIErrorHandler ) );
    VERIFY ( CeRegisterAccessMask ( g_hStoreApi, aamSTGAccessRequired, _countof ( aamSTGAccessRequired ) ) );

    // Register the storage manager AFS mount name. This hidden mount point will
    // be used when performing storage manager API calls.
    int iAFS = INVALID_MOUNT_INDEX;
    iAFS = RegisterAFSName ( L"StoreMgr" );
    if ( ( iAFS == INVALID_MOUNT_INDEX ) || ( ERROR_SUCCESS != GetLastError ( ) ) ) {
        DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!InitializeStoreAPI: failed registering StoreMgr volume name\r\n" ) );
        return FsdGetLastError ( ERROR_GEN_FAILURE );
    }

    LRESULT lResult = 0;

    // Create the storage manager AFS API set and associate it with the newly
    // registered mount point.
    g_hSTRMGRApi = CreateAPISet ( const_cast<CHAR*> ( "PFSD" ), sizeof ( apfnSTGMGRAPIs )/sizeof ( apfnSTGMGRAPIs[0] ),
        reinterpret_cast<const PFNVOID *> ( apfnSTGMGRAPIs ), asigSTGMGRAPIs );
    VERIFY ( RegisterAPISet ( g_hSTRMGRApi, HT_AFSVOLUME | REGISTER_APISET_TYPE ) );
    if ( !g_hSTRMGRApi ) {
        lResult = FsdGetLastError ( ERROR_GEN_FAILURE );
        DeregisterAFSName ( iAFS );
        return lResult;
    }
    if ( !RegisterAFSEx ( iAFS, g_hSTRMGRApi, ( DWORD )1, AFS_VERSION, AFS_FLAG_HIDDEN ) ) {
        DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!InitializeStoreAPI: failed registering StoreMgr volume\r\n" ) );
        lResult = FsdGetLastError ( ERROR_GEN_FAILURE );
        DeregisterAFSName ( iAFS );
        return lResult;
    }

    // Initialize the handle-based file API for forwarding Disk based operations.
    g_hStoreDiskApi = CreateAPISet (const_cast<CHAR*> ("STDK"), NUM_FILE_APIS, apfnDevFileApiMethods, apfnDevFileApiSigs);
    RegisterAPISet (g_hStoreDiskApi, HT_FILE | REGISTER_APISET_TYPE);
    RegisterDirectMethods(g_hStoreDiskApi, apfnDevFileDirectApiMethods);

#endif // UNDER_CE

    return ERROR_SUCCESS;
}

BOOL STRMGR_CloseAllHandles ( DWORD /* dwData */, HANDLE /* hProc */ )
{
    return TRUE;
}

BOOL AddStore ( StoreDisk_t *pStore )
{
    BOOL bRet = FALSE;

    ASSERT (OWN_CS (&g_csStoreMgr));

    DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "Store %x ( Ref= %d ) Device= %s , Added To List \n" ),pStore,pStore->RefCount ( ),pStore->GetDeviceName ( ) ) );

    if (NULL == s_pStoreMap)
    {
        //First time through, create map
        s_pStoreMap = new map <TCHAR*, StoreDisk_t*, lt_compare>();
    }

    if (NULL != s_pStoreMap) {
        TCHAR *szMapDeviceKeyName = new(TCHAR[MAX_PATH]);
        if (NULL != szMapDeviceKeyName) {
            wcscpy_s(szMapDeviceKeyName, MAX_PATH, pStore->GetDeviceName());
            s_pStoreMap->insert(std::pair<TCHAR*, StoreDisk_t*>(&(*szMapDeviceKeyName), &(*pStore)));
            bRet = TRUE;
        }
    }
    return bRet;
}

StoreDisk_t *GetStoreFromMap(LPWSTR pDeviceName)
{
    StoreDisk_t *pStore = NULL;
    if (NULL != s_pStoreMap) {
        map <TCHAR*, StoreDisk_t*, lt_compare>::iterator iter = s_pStoreMap->begin();
        while (iter != s_pStoreMap->end()) {
            // Handle API calls with device name length <= 7.  MSDN reference:
            // <http://msdn.microsoft.com/en-us/library/ee482448(v=winembedded.70).aspx>
            if ( _wcsnicmp(pDeviceName, iter->first, DEVICENAMESIZE-1) == 0) {
                pStore = iter->second;
                break;
            }
            iter++;
        }
    }
    return pStore;
}

BOOL DeleteStoreFromMap(StoreDisk_t *pStoreDisk)
{
    BOOL bRet = FALSE;
    WCHAR deviceName[MAX_PATH] = L"";

    StringCchCopyW (deviceName, MAX_PATH, pStoreDisk->GetDeviceName());

    if (NULL != s_pStoreMap)
    {
        map <TCHAR*, StoreDisk_t*, lt_compare>::iterator iter = 
            s_pStoreMap->find(deviceName);

        if (iter != s_pStoreMap->end())
        {
            StoreDisk_t *pStoreDelete = iter->second;

            if (!pStoreDelete->IsDetached ()) {
                pStoreDelete->SetDetached ();
                pStoreDelete->Disable ();
            }

            if (pStoreDelete->IsMounted()) {
                pStoreDelete->Unmount();
            }

            //If the store handle passed in does not match the one found
            //in the map, let caller deallocate temp media handle.
            if (pStoreDisk = pStoreDelete) {
                bRet = TRUE;
            }

            //RefCnt == 0 implies store object is deleted
            DWORD RefCnt = pStoreDelete->RefCount();
            while (RefCnt > 0) {
                pStoreDelete->Release();
                RefCnt--;
            }

            //Release key resource and erase from map
            delete[] iter->first;
            s_pStoreMap->erase(iter);

            pStoreDelete = NULL;
        }

    }
    return bRet;
}

StoreDisk_t *FindStore (const WCHAR *szDeviceName, StoreState storeSearchType=MOUNTED)
{
    ASSERT (OWN_CS (&g_csStoreMgr)); 

    StoreDisk_t     *pStore                      = NULL;
    TCHAR           szMapDeviceKeyName[MAX_PATH] = {0};

    if (NULL != s_pStoreMap) {
        StringCchCopy(szMapDeviceKeyName, MAX_PATH, szDeviceName);
        map <TCHAR*, StoreDisk_t*, lt_compare>::iterator iter = 
            s_pStoreMap->find(szMapDeviceKeyName);

        if (iter != s_pStoreMap->end()) {
            pStore = iter->second;
        }
    }
    return pStore;
}

void DeleteStore (StoreDisk_t *pStore)
{
    if (NULL != pStore) {
        if (!DeleteStoreFromMap(pStore)) {
            //Not an existing store, delete temp store
            DWORD RefCnt = pStore->RefCount();
            while (RefCnt > 0) {
                pStore->Release();
                RefCnt--;
            }
        }
    }
}

//
//
// MountStore Functions
//
//
void RollBack (StoreDisk_t **ppStoreDisk)
{
    BOOL bRollback = TRUE;      //Always

    // Unmount and delete the store to rollback the mount
    if (NULL != (*ppStoreDisk)) {
        (*ppStoreDisk)->Unmount(bRollback);
        delete (*ppStoreDisk);
        *ppStoreDisk = NULL;
    }
}

LRESULT StoreUpdate (StoreDisk_t **ppNewStore, const WCHAR *pDevName, StoreDisk_t **ppStore)
{
    LRESULT     lResult = ERROR_SUCCESS;
    STOREINFO   Info = {0};
    BOOL        bRollBack = FALSE;
    INT         nStore = 0;
    StoreDisk_t *pStoreExisting = NULL;

    // Handle store updates under these conditions:
    //      1 New store with new name, nothing to do, continue to finish mounting
    //      2.Old store with new name
    //      3 New store with reused name
    //      4 Old store with reused name
    if (NULL != s_pStoreMap) {
        (*ppNewStore)->GetStoreInfo(&Info);
        pStoreExisting = FindStore(Info.szDeviceName);
        if (NULL != pStoreExisting) {
            // Name exist in map
            if ( !pStoreExisting->IsDetached ( ) ) {
                DEBUGMSG (ZONE_VERBOSE, ( TEXT ( "MountStore %s Aborting since Store %x ( Ref= %d ) Device= %s exists \n"),
                        pDevName, pStoreExisting, pStoreExisting->RefCount ( ), pStoreExisting->GetDeviceName()));

                DEBUGMSG (ZONE_ERRORS, (L"FSDMGR: Store \"%s\" already exists...skipping\r\n", pDevName));
                lResult = ERROR_ALREADY_EXISTS;

                bRollBack = TRUE;
            } else {
                // Compare with existing to determine swap or replace
                if (pStoreExisting->Compare(*ppNewStore)) {
                    // Old store with reused name
                    // Swap contents from the new store into the existing store.
                    if (pStoreExisting->Swap(*ppNewStore)) {
                        // Remove the detached flag from the existing store as it is being
                        // re-attached.
                        pStoreExisting->ClearDetached ( );

                        // Re-enable the existing store.
                        pStoreExisting->Enable ( );

                        bRollBack = TRUE;
                    }
                } else {
                    // New store with reused name,delete old and continue to mount new
                    DeleteStore (pStoreExisting);
                }
            }
        } else {
            // Name doesn't exist, iterate through the map looking for detached stores
            // to see if it is an old store reinserted with a new name assigned
            if (!s_pStoreMap->empty()) {
                map <TCHAR*, StoreDisk_t*, lt_compare>::iterator iter = 
                    s_pStoreMap->begin();
                while (iter != s_pStoreMap->end()) {
                    pStoreExisting = iter->second;
                    iter++;
                    if (pStoreExisting->IsDetached()) {
                        if (pStoreExisting->Compare(*ppNewStore)) {
                            //Old store with new name, remove and continue to mount new
                            DeleteStore (pStoreExisting);
                            break;
                        }
                    }
                }
            }
        }
    }

    // Clean-up/free the new store object if store already exist. The existing 
    // store object will remain in place with updated partition information
    // from the new store object. The new store pointer will be nulled in Rollback()
    //to stop further mounting.
    if (bRollBack) {
        RollBack(ppNewStore);
    }
    pStoreExisting = NULL;

    return lResult;
}

LRESULT CheckForDuplicateStore (const WCHAR *pDevName)
{
    LRESULT lResult = ERROR_SUCCESS;

    StoreDisk_t* pNewStore = FindStore(pDevName);
    if (pNewStore) {
        if (!pNewStore->IsDetached()) {
            DEBUGMSG(ZONE_VERBOSE, (TEXT("MountStore %s SKIPPING since Store %x (Ref= %d) Device= %s exists \n" ),
                pDevName, pNewStore, pNewStore->RefCount(), pNewStore->GetDeviceName()));

            // There is an attached store with the same name as the new one
            // so we are unable to mount it ( we can't have name conflicts ).
            DEBUGMSG (ZONE_ERRORS, (L"FSDMGR: Store \"%s\" already exists...skipping\r\n", pDevName));
            lResult   = ERROR_ALREADY_EXISTS;
            pNewStore = NULL;
        }
    }

    return lResult;
}

LRESULT CreateStore(const WCHAR *pDevName, const GUID *pDeviceGuid, StoreDisk_t **ppStoreDisk)
{
    LRESULT lResult = ERROR_SUCCESS;

    // Allocate a new store; RefCount = 1 if successful
    StoreDisk_t *pNewStore = new StoreDisk_t(pDevName, pDeviceGuid);
    if (!pNewStore) {
        lResult = ERROR_NOT_ENOUGH_MEMORY;
    } else {
        *ppStoreDisk = pNewStore;
        pNewStore = NULL;
    }
    return lResult;
}

LRESULT CreateBlockDevice(const WCHAR *pDrvPath, StoreDisk_t *pStoreDisk)
{
    LRESULT lResult = ERROR_SUCCESS;

    if (pDrvPath) {
        // Auto-load stores will have a block device name specified.
        // Associate the specified block device with the store object.
        if (!pStoreDisk->SetBlockDevice(pDrvPath)) {
            lResult = FsdGetLastError(ERROR_GEN_FAILURE);
        }
    }
    return lResult;
}

LRESULT CreateDisk (StoreDisk_t *pStoreDisk)
{
    LRESULT lResult = pStoreDisk->OpenDisk();

    if (lResult != ERROR_SUCCESS) {
        DEBUGMSG(ZONE_ERRORS, (L"FSDMGR!MountStore: Failed opening store; error=%u\r\n",
            lResult));
    }
    return lResult;
}

LRESULT GetPartitionHandle (const WCHAR* pDevName, HANDLE &hStore, StoreDisk_t *pStoreDisk)
{
    LRESULT lResult = ERROR_SUCCESS;

    // Open a store handle to the new store to pass to the partition driver. It will
    // be ready for I/O once we've opened the disk handle in MountStore.
    HANDLE hProc = reinterpret_cast<HANDLE>(GetCurrentProcessId());
    hStore       = pStoreDisk->CreateHandle(hProc, FILE_ALL_ACCESS);	//RefCount++ if successful
    if (INVALID_HANDLE_VALUE == hStore) {
        DEBUGMSG (ZONE_ERRORS, (L"FSDMGR!MountStore: Failed opening new store \"%s\"; error=%u\r\n",
            pDevName, GetLastError()));
        lResult = FsdGetLastError(ERROR_GEN_FAILURE);
    } 
    return lResult;
}

LRESULT PerformAutoMount (const WCHAR* pDevName, StoreDisk_t **ppStoreDisk)
{
    LRESULT lResult = ERROR_SUCCESS;

    DEBUGMSG (ZONE_VERBOSE, (L"FSDMGR!MountStore: No Swappable Store; Mounting Partitions \"%s\"", pDevName));

    if ((*ppStoreDisk)->IsAutoMount()) {
        // Auto-mount the partitions on the new store.
        PartitionDisk_t *pPartition = NULL;

        map <TCHAR*, PartitionDisk_t*, lt_compare>::iterator iter = (*ppStoreDisk)->m_pPartitionMap->begin();
        while (iter != (*ppStoreDisk)->m_pPartitionMap->end()) {
            pPartition = iter->second;
            iter++;
            lResult = (*ppStoreDisk)->MountPartition(pPartition);
            if (ERROR_DEV_NOT_EXIST == lResult) {
                DEBUGMSG (ZONE_VERBOSE, (L"FSDMGR!MountStore: Error Mounting Store; Disk detached; Destroying Store  \"%s\"", pDevName));
                //Device no longer exists, no need to keep the store
                RollBack(ppStoreDisk);
                break;
            }
        }
    }
    return lResult;
}

void CleanUp (const WCHAR* pDevName, HANDLE hStore, StoreDisk_t *pStoreDisk)
{
    DEBUGMSG (ZONE_ERRORS, (L"FSDMGR!MountStore: Failed mounting store \"%s\"\r\n",
        pDevName));

#ifdef UNDER_CE
        ::CloseHandle (hStore);
#else
        ::STG_CloseHandle (reinterpret_cast<PSTOREHANDLE> (hStore));
#endif
    DeleteStore(pStoreDisk);
}

void UpdateStoreListing (StoreDisk_t **ppNewStore)
{
    if (AddStore(*ppNewStore)) {
        TCHAR strShortDeviceName[DEVICENAMESIZE] = {0};
        VERIFY (GetShortDeviceName((*ppNewStore)->GetDeviceName(), strShortDeviceName, _countof(strShortDeviceName)));
        FSDMGR_AdvertiseInterface(&STORE_MOUNT_GUID, strShortDeviceName, TRUE);
    }
}

BOOL MountStore (const WCHAR* pDeviceName, const GUID* pDeviceGuid, const WCHAR* pDriverPath, __out StoreDisk_t** ppStore)
{
    LRESULT     lResult     = ERROR_SUCCESS;
    BOOL        bErrorExit  = FALSE;
    StoreDisk_t *pStoreNew  = NULL;
    HANDLE      hStore      = NULL;

    LockStoreMgr ();

   if (ERROR_SUCCESS != (lResult = CheckForDuplicateStore(pDeviceName))) {
        bErrorExit = TRUE;
    }
    else if (ERROR_SUCCESS != (lResult = CreateStore(pDeviceName, pDeviceGuid, &pStoreNew))) {
        bErrorExit = TRUE;
    }
    else if (ERROR_SUCCESS != (lResult = CreateBlockDevice(pDriverPath, pStoreNew))) {
        bErrorExit = TRUE;
    }
    else if (ERROR_SUCCESS != (lResult = CreateDisk(pStoreNew))) {
        bErrorExit = TRUE;
    }
   else if (ERROR_SUCCESS != (lResult = GetPartitionHandle(pDeviceName, hStore, pStoreNew)))	{
        bErrorExit = TRUE;
    }

    if (bErrorExit) {
        // unsuccessful setup, clean up allocated memories if any
        if (pStoreNew) {
            CleanUp(pDeviceName, hStore, pStoreNew);
            pStoreNew = NULL;
        }
    } else {
        lResult = pStoreNew->Mount(hStore, s_bFreshMap);

        if ((ERROR_SUCCESS != lResult) && (ERROR_DEVICE_NOT_PARTITIONED != lResult)) {
            //Mount failed, clean up
            CleanUp(pDeviceName, hStore, pStoreNew);
            pStoreNew = NULL;
            lResult = FsdGetLastError(ERROR_GEN_FAILURE);
        }
        //Check if store is unique then complete mounting
        else if (ERROR_SUCCESS == (lResult = StoreUpdate(&pStoreNew, pDeviceName, ppStore))) {
            // If this store does not match an existing, or detached storage device (pStoreNew
            // is valid), then we'll mount all existing partitions if the store is AUTOMOUNT.
            if (pStoreNew && 
                (ERROR_DEV_NOT_EXIST != (lResult = PerformAutoMount(pDeviceName, &pStoreNew)))) {
                //Before completing the mounting ensure that there is no duplicate store with the same name
                if (ERROR_SUCCESS != (lResult = CheckForDuplicateStore(pDeviceName))) {
                    ///Unmount and delete the store to rollback the mount.
                    RollBack(&pStoreNew);
                    DEBUGMSG (ZONE_ERRORS, (L"FSDMGR: Store \"%s\" already exists...skipping\r\n", pDeviceName));
                    lResult = ERROR_ALREADY_EXISTS;
                } else {
                    //Add store to the list
                    UpdateStoreListing(&pStoreNew);
                }
            }
        }
    }

    UnlockStoreMgr ();

    SetLastError(lResult);
    return (ERROR_SUCCESS == lResult);
}

//Refresh a Store
void RefreshStore ( StoreDisk_t* pStore )
{
    pStore->Lock ( );
    DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "RefreshStore Store %x ( Ref= %d ) Device = %s \n" ),pStore,pStore->RefCount ( ),pStore->GetDeviceName ( ) ) );

    WCHAR BlockDeviceName[MAX_PATH];
    const WCHAR* pBlockDeviceName = NULL;
    WCHAR szName[MAX_PATH];
    TCHAR strShortDeviceName[DEVICENAMESIZE];
    VERIFY ( GetShortDeviceName ( pStore->GetDeviceName ( ), strShortDeviceName, _countof ( strShortDeviceName ) ) );
    StringCchCopyW ( szName, MAX_PATH, strShortDeviceName );

    pStore->ClearRefreshNeeded ( );

    DEBUGMSG ( ZONE_EVENTS, ( L"FSDMGR!RefreshStore: Refreshing store \"%s\"\r\n", szName ) );
    if ( pStore->m_pBlockDevice ) {
        // This is a block driver autoloaded by storage manager.
        VERIFY ( SUCCEEDED ( StringCchCopyW ( BlockDeviceName, MAX_PATH, pStore->m_pBlockDevice->GetDeviceName ( ) ) ) );
        pBlockDeviceName = BlockDeviceName;
    }

    if ( pStore->Unmount ( ) ) {
        DEBUGMSG ( ZONE_EVENTS, ( L"FSDMGR!RefreshStore: store Unmounted \"%s\"\r\n", szName ) );
        pStore->Unlock ( );

        //Remove Store
        DeleteStore ( pStore );
        pStore = NULL;
        DEBUGMSG ( ZONE_EVENTS, ( L"FSDMGR!RefreshStore: store removed from List \"%s\"\r\n", szName ) );

        // Re-mount the store.
        if ( MountStore ( szName, &BLOCK_DRIVER_GUID, pBlockDeviceName ) ) {
            DEBUGMSG ( ZONE_EVENTS, ( L"FSDMGR!RefreshStore: store Re-mounted \"%s\"\r\n", szName ) );
        }
    } else {
        DEBUGMSG ( ZONE_EVENTS, ( L"FSDMGR!RefreshStore: store Unmounted Failed \"%s\"\r\n", szName ) );
    }

    if ( pStore ) {
        pStore->Unlock ( );
    }
}


// Called by PNPThread when after STRMGR_RefreshStore is called.;
//Refresh All Stores which are marked for REFRESH
void WINAPI RefreshStores ( )
{
    StoreDisk_t  *pStore = NULL;

    DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "** RefreshAllStore BEGIN \n" ) ) );

    if (NULL != s_pStoreMap)
    {
        map <TCHAR*, StoreDisk_t*, lt_compare>::iterator iter = s_pStoreMap->begin();
        while (iter != s_pStoreMap->end()) {
            //Refresh Store would skip stores not marked for refresh
            pStore = iter->second;
            iter++;
            if (!pStore->IsDetached ())
            {
                //Consider only stores marked for refresh
                if (pStore->IsRefreshNeeded())
                {
                    RefreshStore (pStore);
                    //Reset iterator after a delete in refreshstore
                    iter = s_pStoreMap->begin();
                }
            } 
            else
            {
                pStore->ClearRefreshNeeded ();
            }

        }
    }

    DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "** RefreshAllStore END \n" ) ) );
}


void WINAPI DetachStores ( DWORD DetachMask )
{
    StoreDisk_t  *pStore      = NULL;
    BOOL         bNeedRefresh = FALSE;

    if (NULL != s_pStoreMap) {
        map <TCHAR*, StoreDisk_t*, lt_compare>::iterator iter = s_pStoreMap->begin();

        while (iter != s_pStoreMap->end()) {
            pStore = iter->second;
            iter++;

            if (( STORE_STATE_DETACHED & DetachMask) &&
                (pStore->IsDetached()) &&
                !( pStore->IsDetachCompleted())) {
                //Don't unmount if PNP Mount Thread already unmounted this.
                
                DEBUGMSG (ZONE_INIT, ( L"FSDMGR!DetachStores: Completing Store detach \"%s\"\r\n", pStore->GetDeviceName()));
                pStore->Unmount();

                //Remove Store from the map; reset iterator after erase
                DeleteStore (pStore);
                iter = s_pStoreMap->begin();
            }
            else if ((STORE_STATE_MEDIA_DETACHED & DetachMask) &&
                (pStore->IsMediaDetached()) &&
                !(pStore->IsMediaDetachCompleted())) {
                // Unmount Partitions on the store; and not delete them
                DEBUGMSG (ZONE_INIT, (L"FSDMGR!DetachStores: Completing media detach from store \"%s\"\r\n", pStore->GetDeviceName()));
                pStore->UnmountPartitions (FALSE);

                //Prevent to detach again.
                pStore->SetMediaDetachCompleted();

                //Refresh this store to update the partition information.
                pStore->SetRefreshNeeded();

                bNeedRefresh=TRUE;
            }
        }

        if (bNeedRefresh) {
            RefreshStores();
        }
    }
}

void SuspendStores ( )
{
    StoreDisk_t  *pStore = NULL;

    if (NULL != s_pStoreMap)
    {
        map <TCHAR*, StoreDisk_t*, lt_compare>::iterator iter = s_pStoreMap->begin();
        for (iter = s_pStoreMap->begin(); iter != s_pStoreMap->end(); iter++)
        {
            pStore = iter->second;
            pStore->Lock();
            if (pStore->m_pBlockDevice) {

                // For auto-loaded block drivers, we must call the PowerOn and
                // PowerOff exports on suspend/resume because device manager is
                // not aware of auto-loaded drivers.
                pStore->m_pBlockDevice->PowerOff ();
            }

            if (pStore->IsDisableOnSuspend()) {

                // On suspend, we need to permanently disable any devices that will be
                // de-activated and re-activated by their bus driver on resume. By doing
                // this, we make sure no threads will enter the volume from the time that
                // we suspend until the time that the device has been re-activated.

                DEBUGMSG ( ZONE_POWER, ( L"FSDMGR!SuspendStores: Disabling store \"%s\"\r\n", pStore->GetDeviceName ( ) ) );

                pStore->Disable();
            }
            pStore->Unlock();
        }
    }
}

void ResumeStores ( )
{
    StoreDisk_t  *pStore = NULL;

    if (NULL != s_pStoreMap)
    {
        map <TCHAR*, StoreDisk_t*, lt_compare>::iterator iter = NULL;
        for (iter = s_pStoreMap->begin(); iter != s_pStoreMap->end(); iter++)
        {
            pStore = iter->second;
            pStore->Lock();
            if (pStore->m_pBlockDevice) {

                // For auto-loaded block drivers, we must call the PowerOn and
                // PowerOff exports on suspend/resume because device manager is
                // not aware of auto-loaded drivers.

                pStore->m_pBlockDevice->PowerOn ( );
            }
            pStore->Enable();
            pStore->Unlock();
        }
    }
}

BOOL WINAPI UnmountStore ( const WCHAR* pDeviceName )
{
    BOOL bRet           = FALSE;
    StoreDisk_t *pStore = FindStore (pDeviceName);

    if ( pStore ) {
        if ( !pStore->IsDetached ( ) )
        {
            DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "UnmountStore: Setting Detached for Store (%s) for %s \n" ),pStore->GetDeviceName(), pDeviceName ) );

            pStore->SetDetached ( );

            // Disable the store so that it cannot be used.
            pStore->Disable ();
            DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!DetachStores: Completing Store detach \"%s\"\r\n", pStore->GetDeviceName ( ) ) );

            g_dwUpdateState |= STOREMGR_EVENT_UPDATETIMEOUT;
            SetEvent ( g_hPNPUpdateEvent );
        }

        pStore = NULL;

        bRet = TRUE;
    }

    if (s_bFreshMap) {
        s_bFreshMap = FALSE;
    }

    return bRet;
}

BOOL DetachMedia ( const WCHAR* pDeviceName )
{
    LRESULT lResult = ERROR_SUCCESS;

    // Find the store object associated with this device.
    StoreDisk_t* pStore = FindStore ( pDeviceName );
    if ( pStore ) {
        pStore->Lock();	
        if ( pStore->IsDetached ( ) ) {
            DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "MediaDetach: Skipping. Store already Detached \n" ) ) );
            lResult = ERROR_DEV_NOT_EXIST;
        }else {
            // Detach the media from the storage device.
            pStore->MediaDetachFromStore ( );
        }
        pStore->Unlock();
    }else {
        DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "MediaDetach: Skipping. Store not Found \n" ) ) );
        lResult = ERROR_DEV_NOT_EXIST;
    }

    if ( ERROR_SUCCESS == lResult ) {
        // Reset storage manager timeout. After timeout, the store will be
        // completely (truly) dismounted.
        g_dwUpdateState |= STOREMGR_EVENT_UPDATETIMEOUT;
        SetEvent ( g_hPNPUpdateEvent );
    }

    return ( ERROR_SUCCESS == lResult );
}

BOOL AttachMedia ( const WCHAR* pDeviceName )
{
    LRESULT lResult = ERROR_SUCCESS;

    // Find the store object associated with this device.
    StoreDisk_t* pStore = FindStore ( pDeviceName );
    if ( pStore ) {
        pStore->Lock();
        if ( pStore->IsDetached ( ) ) {
            lResult = ERROR_DEV_NOT_EXIST;
        }else {
            // Attach media to the store.
            pStore->MediaAttachToStore ( );
        }
        pStore->Unlock();
    }else {
        lResult = ERROR_DEV_NOT_EXIST;
        DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "MediaAttach: Skipping. Store ( %s ) Not mounted yet \n" ),pDeviceName ) );
    }

    return ( ERROR_SUCCESS == lResult );
}

#ifdef UNDER_CE

#define DEFAULT_TIMEOUT_RESET 5000
#define DEFAULT_WAITIO_MULTIPLIER 3

struct StoreMountInfo
{
    TCHAR   szDeviceName[MAX_PATH];
    GUID    DeviceGuid;
    BOOL    bSetThreadPriority;
    DWORD   dwPriority;
};

DWORD PNPMountThread ( LPVOID lParam )
{
    StoreMountInfo *  pStoreMountInfo = NULL;

    ASSERT ( lParam );

    if (lParam) {
        pStoreMountInfo = ( StoreMountInfo * ) lParam;
        MountStore ( pStoreMountInfo->szDeviceName, &pStoreMountInfo->DeviceGuid );
        delete pStoreMountInfo;
    }

    return 0;
}


DWORD PNPThread ( LPVOID /* lParam */ )
{
    static BYTE             pPNPBuf[sizeof ( DEVDETAIL ) + 200] = {0};
    MSGQUEUEOPTIONS         msgopts                             = {0};
    DWORD                   dwFlags                             = 0;
    DWORD                   dwSize                              = 0;
    DWORD                   dwThreadPriority                    = THREAD_BASE_PRIORITY_MIN;
    BOOL                    bSetThreadPriority                  = FALSE;
    HANDLE                  hMediaNotify                        = NULL;
    HANDLE                  hBlockDeviceNotify                  = NULL;
    DEVDETAIL *             pd                                  = ( DEVDETAIL * )pPNPBuf;
    HANDLE                  pHandles[3]                         = {NULL, NULL, NULL};
    HMODULE                 hCoreDll                            = NULL;
    DWORD                   dwTimeOut                           = INFINITE; 
    DWORD                   dwTimeOutReset                      = DEFAULT_TIMEOUT_RESET;
    PSTOPDEVICENOT          pStopDeviceNotification             = NULL;
    PREQUESTDEVICENOT       pRequestDeviceNotification          = NULL;

    msgopts.dwSize = sizeof ( MSGQUEUEOPTIONS );
    msgopts.dwFlags = 0;
    msgopts.dwMaxMessages = 0; //?
    msgopts.cbMaxMessage = sizeof ( pPNPBuf );
    msgopts.bReadAccess = TRUE;

    g_hPNPQueue = CreateMsgQueue ( NULL, &msgopts );

    if ( !g_hPNPQueue ) {
        DEBUGMSG ( ZONE_ERRORS, ( TEXT ( "FSDMGR!PNPThread: Unable to create PNP Message Queue...aborting PNPThread\r\n" ) ) );
        return FALSE;
    }

    pHandles[0] = g_hPNPQueue;
    pHandles[1] = g_hPNPUpdateEvent;
    pHandles[2] = g_hAutoLoadEvent;

    HKEY hKey;
    if ( ERROR_SUCCESS == FsdRegOpenKey ( g_szSTORAGE_PATH, &hKey ) ) {
        if ( !FsdGetRegistryValue ( hKey, g_szReloadTimeOut, &dwTimeOutReset ) ) {
            dwTimeOutReset = DEFAULT_TIMEOUT_RESET;
        }
        if ( FsdGetRegistryValue ( hKey, g_szPNPThreadPrio, &dwThreadPriority ) ) {
            CeSetThreadPriority ( GetCurrentThread ( ), dwThreadPriority );
            bSetThreadPriority = TRUE;
        }
        FsdRegCloseKey ( hKey );
    }
    DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!PNPThread: Using PNPUnloadDelay of %ld\r\n", dwTimeOutReset ) );
    hCoreDll = ( HMODULE )LoadLibrary ( L"coredll.dll" );
    if ( hCoreDll ) {
        pRequestDeviceNotification = ( PREQUESTDEVICENOT )FsdGetProcAddress ( hCoreDll, L"RequestDeviceNotifications" );
        pStopDeviceNotification = ( PSTOPDEVICENOT )FsdGetProcAddress ( hCoreDll, L"StopDeviceNotifications" );
    }
    FreeLibrary ( hCoreDll ); // This is okay since we should already have a reference to coredll

    if ( pRequestDeviceNotification ) {
        DEBUGMSG ( ZONE_INIT, ( L"FSDMGR!PNPThread: PNPThread starting!\r\n" ) );
        hBlockDeviceNotify = pRequestDeviceNotification ( &BLOCK_DRIVER_GUID, g_hPNPQueue, TRUE );
        DEBUGCHK ( hBlockDeviceNotify );
        hMediaNotify = pRequestDeviceNotification ( &STORAGE_MEDIA_GUID, g_hPNPQueue, TRUE );
        DEBUGCHK ( hMediaNotify );
    }

    if (NULL == s_pStoreMap) {
        s_bFreshMap = TRUE;
    }

    for ( ;; )
    {
        DWORD dwWaitCode;
        dwWaitCode = WaitForMultipleObjects ( 3, pHandles, FALSE, dwTimeOut );
        if ( dwWaitCode == WAIT_TIMEOUT ) {
            DEBUGMSG ( ZONE_EVENTS, ( L"FSDMGR!PNPThread: Scavenging stores\r\n" ) );
            LockStoreMgr ();
            dwTimeOut = INFINITE;
            g_dwUpdateState &= ~STOREMGR_EVENT_UPDATETIMEOUT;
            DetachStores ( STORE_STATE_DETACHED | STORE_STATE_MEDIA_DETACHED );
            UnlockStoreMgr ();
        } else {
            DWORD dwEvent = dwWaitCode - WAIT_OBJECT_0;
            switch ( dwEvent ) {
                case 0: {
                    if ( ReadMsgQueue ( g_hPNPQueue, pd, sizeof ( pPNPBuf ), &dwSize, INFINITE, &dwFlags ) ) {
                        if ( 0 == memcmp ( &pd->guidDevClass, &BLOCK_DRIVER_GUID, sizeof ( GUID ) ) ) {
                            if ( pd->fAttached ) {
                                DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "FSDMGR!PNPThread Mount %s !\r\n" ),pd->szName ) );
                                StoreMountInfo * pStoreMountInfo = new StoreMountInfo;
                                if ( pStoreMountInfo ) {
                                    memcpy ( &pStoreMountInfo->DeviceGuid, &pd->guidDevClass, sizeof ( GUID ) );
                                    VERIFY ( SUCCEEDED ( StringCchCopy ( pStoreMountInfo->szDeviceName, _countof ( pStoreMountInfo->szDeviceName ), pd->szName ) ) );
                                    pStoreMountInfo->dwPriority = dwThreadPriority;
                                    pStoreMountInfo->bSetThreadPriority = bSetThreadPriority;
                                    HANDLE hMountThread = CreateThread ( NULL, 0, PNPMountThread, ( LPVOID ) pStoreMountInfo, 0, NULL );
                                    if ( hMountThread == INVALID_HANDLE_VALUE ) {
                                        DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!PNPThread Mount Thread failed to start!\r\n" ) );
                                        //Try mounting directly.
                                        MountStore ( pStoreMountInfo->szDeviceName, &pStoreMountInfo->DeviceGuid );
                                        delete pStoreMountInfo;
                                    } else {
                                        CeSetThreadPriority ( hMountThread, CeGetThreadPriority(GetCurrentThread()));
                                        CloseHandle ( hMountThread );
                                    }
                                } else {
                                    DEBUGMSG ( ZONE_ERRORS, ( TEXT ( "FSDMGR!PNPThread Mount %s Not Enough Memory. Skipping Mount! \r\n" ),pd->szName ) );
                                }
                            } else {
                                DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "FSDMGR!PNPThread Unmount %s !\r\n" ),pd->szName ) );
                                LockStoreMgr ();
                                UnmountStore ( pd->szName );
                                UnlockStoreMgr ();
                            }

                        } else if ( 0 == memcmp ( &pd->guidDevClass, &STORAGE_MEDIA_GUID, sizeof ( GUID ) ) ) {
                            if ( pd->fAttached ) {
                                DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "FSDMGR!PNPThread Media Attach %s !\r\n" ),pd->szName ) );
                                LockStoreMgr ();
                                AttachMedia ( pd->szName );
                                UnlockStoreMgr ();
                            } else {
                                DEBUGMSG ( ZONE_VERBOSE, ( TEXT ( "FSDMGR!PNPThread Media Detach %s !\r\n" ),pd->szName ) );
                                LockStoreMgr ();
                                DetachMedia ( pd->szName );
                                UnlockStoreMgr ();
                            }
                        }
                    }
                    break;
                }
                case 1: {
                    if ( g_dwUpdateState & STOREMGR_EVENT_UPDATETIMEOUT ) {
                        dwTimeOut = dwTimeOutReset;
                    }
                    if ( g_dwUpdateState & STOREMGR_EVENT_REFRESHSTORE ) {
                        LockStoreMgr();
                        RefreshStores();
                        UnlockStoreMgr();
                    }
                    break;
                }
                case 2:
                    ResetEvent ( g_hAutoLoadEvent );
                    AutoLoadFileSystems ( 2, LOAD_FLAG_ASYNC );
                    break;
                default:
                    break;
            }
        }
    }

    // Should never get here.
}

LRESULT InitializeStorageManager ( DWORD BootPhase )
{
    // Create PNP thread events, and start the PNP thread.
    if ( !g_hPNPThread ) {
        g_hPNPUpdateEvent = CreateEvent ( NULL, FALSE, FALSE, NULL );
        if ( !g_hPNPUpdateEvent ) {
            return FsdGetLastError ( ERROR_GEN_FAILURE );
        }
        g_hAutoLoadEvent = CreateEvent ( NULL, TRUE, FALSE, L"SYSTEM/AutoLoadFileSystems" );
        if ( !g_hAutoLoadEvent ) {
            return FsdGetLastError ( ERROR_GEN_FAILURE );
        }

        // Load storage manager registry values:
        //
        // [HKLM\System\StorageManager]
        //
        //      PNPUnloadDelay=dword:xxx
        //
        //      PNPWaitIODelay=dword:xxx
        //
        HKEY hKey = NULL;
        if ( ERROR_SUCCESS == FsdRegOpenKey ( g_szSTORAGE_PATH, &hKey ) ) {
            DWORD dwTimeoutReset;
            if ( !FsdGetRegistryValue ( hKey, g_szReloadTimeOut, &dwTimeoutReset ) ) {
                dwTimeoutReset = DEFAULT_TIMEOUT_RESET;
            }
            if ( !FsdGetRegistryValue ( hKey, g_szWaitDelay, &g_dwWaitIODelay ) ) {
                g_dwWaitIODelay = dwTimeoutReset * DEFAULT_WAITIO_MULTIPLIER;
            }
            FsdRegCloseKey ( hKey );
        }

        g_hPNPThread = CreateThread ( NULL, 0, PNPThread, NULL, 0, NULL );
        if ( !g_hPNPThread ) {
            DEBUGMSG ( ZONE_ERRORS, ( L"FSDMGR!InitializeStoreAPI: PNPThread failed to start!\r\n" ) );
            return FsdGetLastError ( ERROR_GEN_FAILURE );
        }
    }

    if ( 2 == BootPhase ) {
        // If this is BootPhase 2, signall the PNP thread to process asynchronous
        // auto-load tasks.
        SetEvent ( g_hAutoLoadEvent );
    }

    return ERROR_SUCCESS;
}

#endif

// External API's
// SECURITY NOTE: Filesys validated, canonicalized, and locally copied "pwsFileName."
// Filesys also validated that the caller has permission to perform the operation.
HANDLE STRMGR_CreateFileW ( DWORD /* dwData */, HANDLE hProc, PCWSTR pwsFileName, DWORD dwAccess, DWORD /* dwShareMode */,
                          PSECURITY_ATTRIBUTES /* pSecurityAttributes */, DWORD /* dwCreate */, DWORD /* dwFlagsAndAttributes */,
                          HANDLE /* hTemplateFile */, PSECURITY_DESCRIPTOR /* pSecurityDescriptor */, DWORD /* SecurityDescriptorSize */ )
{
    if ( *pwsFileName == L'\\' )
        pwsFileName++;
    return STG_OpenStoreEx ( pwsFileName, hProc, dwAccess );
}

// SECURITY NOTE: Filesys validated, canonicalized, and locally copied "pwsFileSpec."
// Filesys also validated that the caller has permission to perform the operation.
HANDLE STRMGR_FindFirstFileW ( DWORD /* dwData */, HANDLE hProc, PCWSTR /* pwsFileSpec */, WIN32_FIND_DATA *pfd, DWORD SizeOfFindData )
{
    HANDLE hRet = NULL;

    LockStoreMgr ();

#ifdef UNDER_CE
    if ( sizeof ( STOREINFO ) != SizeOfFindData ) {
        DEBUGCHK ( 0 ); // AFS_FindFirstFileW_Trap macro was called directly w/out proper size.
        SetLastError ( ERROR_INVALID_PARAMETER );
        hRet = INVALID_HANDLE_VALUE;
    }
#else
    UNREFERENCED_PARAMETER ( SizeOfFindData );
#endif

    if (INVALID_HANDLE_VALUE !=  hRet) {
        STOREINFO *pInfo = (STOREINFO*)pfd;
        hRet = STG_FindFirstStore (pInfo, hProc);
    }

    UnlockStoreMgr ();

    return hRet;
}

// NOTE: This API is called from MoveFile.
// SECURITY NOTE: Filesys validated, canonicalized, and locally copied "pwsFileName."
// Filesys also verified that the caller had FILE_ALL_ACCESS to the store in MoveFile,
// which is sufficient rights to invoke this API.
BOOL STRMGR_RefreshStore ( DWORD /* dwData */, const WCHAR* pFileName, const WCHAR* /* pReserved */ )
{
    if ( *pFileName == L'\\' )
        pFileName++;

    LockStoreMgr ();

    StoreDisk_t *pStore = FindStore ( pFileName );
    if ( pStore ) {
        pStore->SetRefreshNeeded ( );
        g_dwUpdateState |= STOREMGR_EVENT_REFRESHSTORE;
        SetEvent ( g_hPNPUpdateEvent );
    } else {
        //Store is not mounted yet, mount it.
        MountStore (pFileName, &BLOCK_DRIVER_GUID);
    }

    UnlockStoreMgr ();

    return TRUE;
}

BOOL WINAPI STG_CreateSearchStoreMap ( PSEARCHSTORE pSearch )
{
    BOOL         bRet    = FALSE;
    StoreDisk_t  *pStore = NULL;

    if (NULL != s_pStoreMap) {
        map <TCHAR*, StoreDisk_t*, lt_compare>::iterator iter = NULL;
        for (iter = s_pStoreMap->begin(); iter != s_pStoreMap->end(); iter++) {
            pStore = iter->second;

            if (!pStore->IsDetached()) {
                PartStoreInfo  *pPartStoreInfo = new PartStoreInfo;
                if (pPartStoreInfo) {
                    pStore->GetStoreInfo (&pPartStoreInfo->u.storeInfo);
                    pPartStoreInfo->u.storeInfo.cbSize = sizeof (STOREINFO);
                    pSearch->AddToPartStoreInfoMap(pPartStoreInfo);
                }
            }
        }

        if(!pSearch->PartStoreInfoMapIsEmpty()) {
            bRet = TRUE;
        }
    }

    return bRet;
}

EXTERN_C PDSK FSDMGR_DeviceHandleToHDSK ( HANDLE hDevice )
{
    return (PDSK) hDevice;
}


BOOL CONV_DevCloseFileHandle (StoreDisk_t **ppStore)
{
    return TRUE;
}

#ifdef UNDER_CE

BOOL CONV_DevDeviceIoControl (StoreDisk_t **ppStore, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) 
{
    // Pass the IOCTL directly to the disk.
    return ::ForwardDeviceIoControl ( (*ppStore)->m_hDisk,dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped );
}

#endif

#endif /* UNDER_CE */