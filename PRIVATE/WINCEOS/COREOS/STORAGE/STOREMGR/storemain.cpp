//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <msgqueue.h>
#include <pnp.h>
#include <store.h>
#include <partdrv.h>
#include <fsdmgrp.h>
#include <storemain.h>

CRITICAL_SECTION g_csStoreMgr;
CStore *g_pStoreRoot = NULL;
PSTOREHANDLE g_pRootHandle = NULL;

HANDLE g_hFindStoreApi = NULL;
HANDLE g_hFindPartApi  = NULL;
HANDLE g_hStoreApi     = NULL;
HANDLE g_hSTRMGRApi    = NULL;
HANDLE g_hBlockDevApi  = NULL;

HANDLE g_hPNPQueue = NULL;
HANDLE g_hPNPThread = NULL;
HANDLE g_hPNPUpdateEvent = NULL;
DWORD  g_dwUpdateState = 0;

DWORD  g_dwWaitIODelay = 0;

HANDLE g_hAutoLoadEvent = NULL;

HANDLE STRMGR_CreateFileW(DWORD dwData, HANDLE hProc, PCWSTR pwsFileName, DWORD dwAccess, DWORD dwShareMode, PSECURITY_ATTRIBUTES pSecurityAttributes, DWORD dwCreate, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
BOOL   STRMGR_RefreshStore(DWORD dwData, PWSTR pwsFileName, PCWSTR szReserved);
HANDLE STRMGR_FindFirstFileW(DWORD dwData, HANDLE hProc, PCWSTR pwsFileSpec, PWIN32_FIND_DATAW pfd);
BOOL   STRMGR_CloseAllHandles(DWORD dwData, HANDLE hProc);
DWORD  STRMGR_StubFunction(){   return 0;}

typedef HANDLE (* PREQUESTDEVICENOT)(const GUID *devclass, HANDLE hMsgQ, BOOL fAll);
typedef BOOL (* PSTOPDEVICENOT)(HANDLE h);

extern const TCHAR *g_szAUTOLOAD_PATH;
extern const TCHAR *g_szFILE_SYSTEM_MODULE_STRING;
extern const TCHAR *g_szMOUNT_FLAGS_STRING;
extern const TCHAR *g_szACTIVITY_TIMER_STRING;
extern const TCHAR *g_szDEFAULT_ACTIVITY_NAME;
extern const TCHAR *g_szFILE_SYSTEM_DRIVER_STRING;
extern const TCHAR *g_szFSD_BOOTPHASE_STRING;
extern const TCHAR *g_szSTORAGE_PATH; // = L"System\\StorageManager";
extern const TCHAR *g_szReloadTimeOut; // = L"PNPUnloadDelay";
extern const TCHAR *g_szWaitDelay; // = L"PNPWaitIODelay";
extern const TCHAR *g_szPNPThreadPrio; // =L"PNPThreadPrio256";

CONST PFNAPI apfnFindStoreAPIs[NUM_FIND_APIS] = {
        (PFNAPI)STG_FindCloseStore,
        (PFNAPI)NULL,
        (PFNAPI)STG_FindNextStore,
};

CONST DWORD asigFindStoreAPIs[NUM_FIND_APIS] = {
        FNSIG1(DW),                                     // FindClose
        FNSIG0(),                                       //
        FNSIG2(DW, PTR)                                 // FindNextFileW
};

CONST PFNAPI apfnFindPartAPIs[NUM_FIND_APIS] = {
        (PFNAPI)STG_FindClosePartition,
        (PFNAPI)NULL,
        (PFNAPI)STG_FindNextPartition,
};

CONST DWORD asigFindPartAPIs[NUM_FIND_APIS] = {
        FNSIG1(DW),                                     // FindClose
        FNSIG0(),                                       //
        FNSIG2(DW, PTR)                                 // FindNextFileW
};

#define NUM_STG_APIS 15

CONST PFNAPI apfnSTGAPIs[NUM_STG_APIS] = {
        (PFNAPI)STG_CloseHandle,
        (PFNAPI)STG_GetStoreInfo,
        (PFNAPI)STG_DismountStore,
        (PFNAPI)STG_FormatStore,
        (PFNAPI)STG_CreatePartition,
        (PFNAPI)STG_DeletePartition,
        (PFNAPI)STG_OpenPartition, 
        (PFNAPI)STG_MountPartition,
        (PFNAPI)STG_DismountPartition,
        (PFNAPI)STG_RenamePartition,
        (PFNAPI)STG_SetPartitionAttributes,
        (PFNAPI)STG_DeviceIoControl,
        (PFNAPI)STG_GetPartitionInfo,
        (PFNAPI)STG_FormatPartition,
        (PFNAPI)STG_FindFirstPartition
};

CONST DWORD asigSTGAPIs[NUM_STG_APIS] = {
        FNSIG1(DW),                     // CloseHandle
        FNSIG2(DW, PTR),                // GetStoreInfo
        FNSIG1(DW),                     // DismountStore
        FNSIG1(DW),                     // FormatStore
        FNSIG5(DW, PTR, DW, DW, DW),    // CreatePartition
        FNSIG2(DW, PTR),                // DeletePartition
        FNSIG2(DW, PTR),                // OpenPartition
        FNSIG1(DW),                     // MountPartition,
        FNSIG1(DW),                     // DismountPartition
        FNSIG2(DW, PTR),                // RenamePartition
        FNSIG2(DW, DW),                 // SetPartitionAttributes    
        FNSIG8(DW, DW,  PTR, DW,  PTR, DW, PTR, PTR),   // DeviceIoControl
        FNSIG2(DW, PTR),                // GetPartitionInfo    
        FNSIG3(DW, DW, DW),             // FormatPartition    
        FNSIG2(DW, PTR),                // FindFirstPartition    
};

CONST PFNAPI apfnSTGMGRAPIs[NUM_AFS_APIS] = {
        (PFNAPI)STRMGR_StubFunction,
        (PFNAPI)NULL,
        (PFNAPI)STRMGR_StubFunction,
        (PFNAPI)STRMGR_StubFunction,
        (PFNAPI)STRMGR_StubFunction,
        (PFNAPI)STRMGR_StubFunction,
        (PFNAPI)STRMGR_CreateFileW,
        (PFNAPI)STRMGR_StubFunction,
        (PFNAPI)STRMGR_RefreshStore,
        (PFNAPI)STRMGR_FindFirstFileW,
        (PFNAPI)STRMGR_StubFunction,
        (PFNAPI)STRMGR_StubFunction,
        (PFNAPI)STRMGR_StubFunction,
        (PFNAPI)STRMGR_CloseAllHandles,
        (PFNAPI)STRMGR_StubFunction,
        (PFNAPI)STRMGR_StubFunction,
        (PFNAPI)STRMGR_StubFunction,
        (PFNAPI)STRMGR_StubFunction,
};

CONST DWORD asigSTGMGRAPIs[NUM_AFS_APIS] = {
        FNSIG1(DW),                                     // CloseVolume
        FNSIG0(),                                       //
        FNSIG3(DW, PTR, PTR),                           // CreateDirectoryW
        FNSIG2(DW, PTR),                                // RemoveDirectoryW
        FNSIG2(DW, PTR),                                // GetFileAttributesW
        FNSIG3(DW, PTR, DW),                            // SetFileAttributesW
        FNSIG9(DW, DW,  PTR, DW, DW, PTR, DW, DW, DW),  // CreateFileW
        FNSIG2(DW, PTR),                                // DeleteFileW
        FNSIG3(DW, PTR, PTR),                           // MoveFileW
        FNSIG4(DW, DW,  PTR, PTR),                      // FindFirstFileW
        FNSIG2(DW, DW),                                 // CeRegisterFileSystemNotification
        FNSIG3(DW, DW,  PTR),                           // CeOidGetInfo
        FNSIG3(DW, PTR, PTR),                           // PrestoChangoFileName
        FNSIG2(DW, DW),                                 // CloseAllFiles
        FNSIG6(DW, PTR, PTR, PTR, PTR, PTR),            // GetDiskFreeSpace
        FNSIG2(DW, DW),                                 // Notify
        FNSIG2(DW, DW),                                 // CeRegisterFileSystemFunction
        FNSIG4(DW, PTR, DW, DW),
};

const PFNVOID DevFileApiMethods[] = {
    (PFNVOID)FS_DevCloseFileHandle,
    (PFNVOID)0,
    (PFNVOID)FS_DevReadFile,
    (PFNVOID)FS_DevWriteFile,
    (PFNVOID)FS_DevGetFileSize,
    (PFNVOID)FS_DevSetFilePointer,
    (PFNVOID)FS_DevGetFileInformationByHandle,
    (PFNVOID)FS_DevFlushFileBuffers,
    (PFNVOID)FS_DevGetFileTime,
    (PFNVOID)FS_DevSetFileTime,
    (PFNVOID)FS_DevSetEndOfFile,
    (PFNVOID)FS_DevDeviceIoControl,
};

#define NUM_FAPIS (sizeof(DevFileApiMethods)/sizeof(DevFileApiMethods[0]))

const DWORD DevFileApiSigs[NUM_FAPIS] = {
    FNSIG1(DW),                 // CloseFileHandle
    FNSIG0(),
    FNSIG5(DW,PTR,DW,PTR,PTR),  // ReadFile
    FNSIG5(DW,PTR,DW,PTR,PTR),  // WriteFile
    FNSIG2(DW,PTR),             // GetFileSize
    FNSIG4(DW,DW,PTR,DW),       // SetFilePointer
    FNSIG2(DW,PTR),             // GetFileInformationByHandle
    FNSIG1(DW),                 // FlushFileBuffers
    FNSIG4(DW,PTR,PTR,PTR),     // GetFileTime
    FNSIG4(DW,PTR,PTR,PTR),     // SetFileTime
    FNSIG1(DW),                 // SetEndOfFile,
    FNSIG8(DW, DW, PTR, DW, PTR, DW, PTR, PTR), // DeviceIoControl
};


BOOL STRMGR_CloseAllHandles(DWORD dwData, HANDLE hProc)
{
    return TRUE;
}


BOOL IsValidStore(CStore *pStore)
{
    if (!pStore || (pStore == INVALID_STORE))
        return FALSE;
    LockStoreMgr();
    CStore *pTemp = g_pStoreRoot;
    while( pTemp) {
        if (pTemp == pStore) {
            break;
        }    
        pTemp = pTemp->m_pNextStore;
    }
    UnlockStoreMgr();
    return pTemp != NULL;
}

void AddStore(CStore *pStore)
{
// TODO: Add in the order of Disk Index ???
    if (g_pStoreRoot) {
        CStore *pTemp = g_pStoreRoot;
        while(pTemp->m_pNextStore) {
            pTemp = pTemp->m_pNextStore;
        }
        pTemp->m_pNextStore = pStore;
    } else {
        g_pStoreRoot = pStore;
    }    
}

void DeleteStore(CStore *pStore)
{
    CStore *pTemp = g_pStoreRoot;
    if (g_pStoreRoot == pStore) {
        g_pStoreRoot = pStore->m_pNextStore;
    } else {    
        while(pTemp) {
            if (pTemp->m_pNextStore && (pTemp->m_pNextStore == pStore)) {
                break;
            }
            pTemp = pTemp->m_pNextStore;
        }    
        if (pTemp) {
            pTemp->m_pNextStore = pStore->m_pNextStore;
        }
    }    
    delete pStore;
}

CStore *FindStore(TCHAR *szDeviceName)
{
    CStore *pTemp = g_pStoreRoot;
    while(pTemp) {
        if (wcsicmp(szDeviceName, pTemp->m_szDeviceName) == 0) {
            break;
        }
        pTemp = pTemp->m_pNextStore;
    }    
    return pTemp;
}

BOOL CompareStore(CStore *pStore1, CStore *pStore2)
{
    BOOL bRet = FALSE;
    if (pStore1->m_pStorageId && pStore2->m_pStorageId) {
         DWORD dwSize1 = ((STORAGE_IDENTIFICATION *)(pStore1->m_pStorageId))->dwSize;
         DWORD dwSize2 = ((STORAGE_IDENTIFICATION *)(pStore2->m_pStorageId))->dwSize;
         if (dwSize1 != dwSize2) 
            goto exit;
         if (memcmp( pStore1->m_pStorageId, pStore2->m_pStorageId, dwSize1) != 0) 
            goto exit;
    } else {
        if (pStore1->m_pStorageId && !pStore2->m_pStorageId) 
            goto exit;
        if (!pStore1->m_pStorageId && pStore2->m_pStorageId) 
            goto exit;
    }    
    if (memcmp( &pStore1->m_si, &pStore2->m_si, sizeof(PD_STOREINFO)) != 0) 
        goto exit;
    if (memcmp( &pStore1->m_di, &pStore2->m_di, sizeof(DISK_INFO)) != 0) 
        goto exit;
    if (memcmp( &pStore1->m_sdi, &pStore2->m_sdi, sizeof(STORAGEDEVICEINFO)) != 0) 
        goto exit;
    if (pStore1->m_dwPartCount != pStore2->m_dwPartCount)
        goto exit;
    bRet = TRUE;            
exit:
    return bRet;
}

BOOL   WINAPI MountStore(TCHAR *szDeviceName, GUID DeviceGuid)
{
    DWORD dwError = ERROR_SUCCESS;
    LockStoreMgr();
    CStore *pStore = FindStore( szDeviceName);
    if (!pStore || (pStore->m_dwFlags & STORE_FLAG_DETACHED)) {
        CStore *pStoreTemp = g_pStoreRoot;
        while( pStoreTemp) {
            if (pStoreTemp->m_dwFlags & STORE_FLAG_DETACHED) {
                break;
            }   
            pStoreTemp = pStoreTemp->m_pNextStore;
        }   
        pStore = new CStore(szDeviceName, DeviceGuid);
        if (!pStore) {
            UnlockStoreMgr();
            return FALSE;
        }    
        if (ERROR_SUCCESS != (dwError = pStore->MountStore(pStoreTemp ? FALSE : TRUE))) {
            DEBUGMSG( ZONE_INIT | ZONE_ERRORS, (L"STOREMGR: Store %s failed to mount initially\r\n", szDeviceName));
            delete pStore;    
            pStore = NULL;
        } else {
            if (pStoreTemp) {
                pStoreTemp = g_pStoreRoot;
                while(pStoreTemp) {
                    pStoreTemp->Lock();
                    if (CompareStore( pStore, pStoreTemp)) {
                        CPartition *pPartition1 = pStoreTemp->m_pPartitionList;
                        CPartition *pPartition2 = pStore->m_pPartitionList;
                        while(pPartition1 && pPartition2) {
                            if (memcmp( &pPartition1->m_pi, &pPartition2->m_pi, sizeof(PD_PARTINFO)) != 0) {
                                break;
                            }
                            pPartition1 = pPartition1->m_pNextPartition;
                            pPartition2 = pPartition2->m_pNextPartition;
                        }
                        if (!pPartition1 && !pPartition2) {
                            pStoreTemp->Unlock();
                            break;
                        }   
                    }   
                    pStoreTemp->Unlock();
                    pStoreTemp = pStoreTemp->m_pNextStore;                  
                }
                if (pStoreTemp) {
                    pStoreTemp->m_pPartDriver->CloseStore(pStoreTemp->m_dwStoreId);
                    CloseHandle(pStoreTemp->m_hDisk);
                    pStoreTemp->m_hDisk = pStore->m_hDisk;
                    pStoreTemp->m_dwStoreId = pStore->m_dwStoreId;
                    pStoreTemp->Lock();
                    pStoreTemp->m_dwFlags &= ~STORE_FLAG_DETACHED;
                    wcscpy( pStoreTemp->m_szDeviceName, szDeviceName);
                    wcscpy( pStoreTemp->m_szOldDeviceName, L"");
                    CPartition *pPartition1 = pStoreTemp->m_pPartitionList;
                    CPartition *pPartition2 = pStore->m_pPartitionList;
                    while(pPartition1) {
                        if (pPartition1->m_pDsk && pPartition1->m_pDsk->pVol) {
                            pPartition1->m_pDsk->pVol->dwFlags &= ~VOL_DETACHED;
                        }
                        pPartition1->m_dwPartitionId = pPartition2->m_dwPartitionId;
                        pPartition1->m_dwStoreId = pPartition2->m_dwStoreId;
                        pPartition2->m_dwPartitionId = 0;
                        pPartition1 = pPartition1->m_pNextPartition;
                        pPartition2 = pPartition2->m_pNextPartition;
                    }
                    pStore->m_dwStoreId = 0;
                    pStore->m_hDisk = NULL;
                    pStoreTemp->Unlock();
                    pStore->UnmountStore(TRUE);
                    delete pStore;
                    pStore = NULL;
                } else {
                    DetachStores(STORE_FLAG_DETACHED);
                    CPartition *pPartition = pStore->m_pPartitionList;
                    while(pPartition) {
                        pStore->MountPartition(pPartition);
                        pPartition = pPartition->m_pNextPartition;
                    }
                    AddStore(pStore);
                }
            } else {
                AddStore( pStore);
            }   
        }
    } else {
        DEBUGMSG( ZONEID_ERRORS, (L"STOREMGR: Store %s already exists ...skipping\r\n", szDeviceName));
    }   
    UnlockStoreMgr();
    if (pStore && (dwError == ERROR_SUCCESS)) {
        FSDMGR_AdvertiseInterface( &STORE_MOUNT_GUID, pStore->m_szDeviceName, TRUE);
    }
    return TRUE;
}

void WINAPI DetachStores(DWORD dwFlags)
{
    LockStoreMgr();
    CStore *pDetachedStore;
    CStore *pStore = g_pStoreRoot;
    while( pStore) {
        if (pStore->m_dwFlags & dwFlags) {
            pDetachedStore = pStore;
            pStore = pStore->m_pNextStore;
            pDetachedStore->Lock();
            DEBUGMSG( ZONE_INIT, (L"Delayed detach of store %s is happenning !!!\r\n", pDetachedStore->m_szOldDeviceName));
            pDetachedStore->UnmountStore();
            UpdateHandleFromList(pDetachedStore->m_pRootHandle, pDetachedStore, NULL);
            pDetachedStore->Unlock();
            UpdateHandleFromList(g_pRootHandle, pDetachedStore, NULL);
            if (dwFlags & STORE_FLAG_DETACHED) 
                FSDMGR_AdvertiseInterface( &STORE_MOUNT_GUID, pDetachedStore->m_szOldDeviceName, FALSE);
            else    
                FSDMGR_AdvertiseInterface( &STORE_MOUNT_GUID, pDetachedStore->m_szDeviceName, FALSE);
            DeleteStore( pDetachedStore);
        } else {
            pStore = pStore->m_pNextStore;
        }
    }   
    UnlockStoreMgr();
}

BOOL WINAPI UnmountStore(TCHAR *szDeviceName)
{
    LockStoreMgr();
    CStore *pStore = FindStore( szDeviceName);
    UnlockStoreMgr();
    if (pStore) {
        pStore->Lock();
        pStore->m_dwFlags |= STORE_FLAG_DETACHED;
        wcscpy( pStore->m_szOldDeviceName , pStore->m_szDeviceName);
        wcscpy( pStore->m_szDeviceName, L"");
        CPartition *pPartition = pStore->m_pPartitionList;
        while(pPartition) {
            if (pPartition->m_pDsk && pPartition->m_pDsk->pVol) {
                pPartition->m_pDsk->pVol->dwFlags |= VOL_DETACHED;
            }
            pPartition = pPartition->m_pNextPartition;
        }
        pStore->Unlock();
        LockStoreMgr();
        g_dwUpdateState |= STOREMGR_EVENT_UPDATETIMEOUT;
        SetEvent( g_hPNPUpdateEvent);
        UnlockStoreMgr();
        return TRUE;
    } 
    return FALSE;
}

BOOL AddHandleToList(PSTOREHANDLE *ppRoot, PSTOREHANDLE pHandle)
{
    DEBUGMSG(ZONE_INIT, (L"AddHandleToList root=%08X pHandle=%08X\r\n", *ppRoot, pHandle));
    pHandle->pNext = *ppRoot;
    *ppRoot = pHandle;
    return TRUE;
}

BOOL DeleteHandleFromList(PSTOREHANDLE *ppRoot, PSTOREHANDLE pHandle)
{
    PSTOREHANDLE pTemp = *ppRoot;
    if (*ppRoot && pHandle) {
        if (*ppRoot == pHandle) {
            DEBUGMSG(ZONE_INIT, (L"DeleteHandleFromList deleting with root=%08X pHandle=%08X\r\n", *ppRoot, pHandle));
            *ppRoot = pHandle->pNext;
            return TRUE;
        } 
        while( pTemp) {
            if (pTemp->pNext == pHandle) {
                pTemp->pNext = pHandle->pNext;
                DEBUGMSG(ZONE_INIT, (L"DeleteHandleFromList Root=%08X pHandle=%08X <---Deleted\r\n", *ppRoot, pHandle));
                return TRUE;
            }
            DEBUGMSG(1, (L"DeleteHandleFromList pHandle=%08X\r\n", pTemp));
            pTemp = pTemp->pNext;
        }
    } 
    DEBUGMSG( ZONE_INIT, (L"DeleteHandleFromList handle not found Root=%08X pHandle=%08X\r\n", *ppRoot, pHandle));
    return FALSE;
}

BOOL UpdateHandleFromList( PSTOREHANDLE pHandle, CStore *pStore, CPartition *pPartition)
{
    while(pHandle) {
        DEBUGMSG(ZONE_INIT, (L"UpdateHandleFromList pHandle=%08X\r\n", pHandle));
        if (pHandle->pPartition == pPartition && pPartition) {
            if (pHandle->dwFlags & STOREHANDLE_TYPE_SEARCH) {
                pHandle->pPartition = pPartition->m_pNextPartition;
                pHandle->dwFlags |= STOREHANDLE_TYPE_CURRENT;
            } else {
                pHandle->pPartition = INVALID_PARTITION;
            }
        }   
        if (pHandle->pStore == pStore && pStore) {
            if (pHandle->dwFlags & STOREHANDLE_TYPE_SEARCH) {
                pHandle->pStore = pStore->m_pNextStore;
                pHandle->dwFlags |= STOREHANDLE_TYPE_CURRENT;
            } else {
                pHandle->pStore =  INVALID_STORE;
            }   
        }   
        pHandle = pHandle->pNext;
    }
    return TRUE;
}

// Device Io Control for filesystems that do not have a block driver associated with it.
BOOL DeviceIoControlStub(HANDLE hDsk, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    SetLastError( ERROR_NOT_SUPPORTED);
    return FALSE;
}



void AutoLoadFileSystems(DWORD dwCurPhase, DWORD dwFlags)
{
    HKEY  hKeyAutoLoad;
    DWORD dwBootPhase;
    PFSDLOADLIST pFSDLoadList = NULL, pTemp=NULL;
    TCHAR szActivityName[MAX_PATH];
    if (ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, g_szAUTOLOAD_PATH, 0, 0, &hKeyAutoLoad)) {
        pFSDLoadList = LoadFSDList( hKeyAutoLoad, dwFlags);
        while(pFSDLoadList) {
            HKEY hKeyFS;
            if (ERROR_SUCCESS == RegOpenKeyEx( hKeyAutoLoad, pFSDLoadList->szName, 0, 0, &hKeyFS)) {
                if (!FsdGetRegistryValue( hKeyFS, g_szFSD_BOOTPHASE_STRING, &dwBootPhase)) {
                    if (dwCurPhase != 0) {
                        dwBootPhase = dwCurPhase;
                    } else {
                        dwBootPhase = 1;
                    }   
                    RegSetValueExW( hKeyFS, g_szFSD_BOOTPHASE_STRING, 0, REG_DWORD, (LPBYTE)&dwBootPhase, sizeof(DWORD));
                }
                TCHAR szFSName[MAX_PATH];
                if (dwBootPhase == dwCurPhase) {
                    if (FsdGetRegistryString(hKeyFS, g_szFILE_SYSTEM_MODULE_STRING, szFSName, sizeof(szFSName)/sizeof(WCHAR))) {
                        HMODULE hFSD = LoadDriver( szFSName);
                        if (hFSD) {
                            FSDINITDATA fid;
                            fid.pIoControl = (PDEVICEIOCONTROL)DeviceIoControlStub;
                            fid.hDsk = NULL;
                            fid.hPartition = NULL;
                            if (!FsdGetRegistryValue(hKeyFS, g_szMOUNT_FLAGS_STRING, &fid.dwFlags)) {
                                fid.dwFlags = 0;
                            }   
                            if (!FsdGetRegistryString(hKeyFS, g_szACTIVITY_TIMER_STRING, szActivityName, sizeof(szActivityName)/sizeof(WCHAR))) {
                                wcscpy( szActivityName, g_szDEFAULT_ACTIVITY_NAME);
                            } 
                            if (wcslen(szActivityName)) {
                                fid.hActivityEvent = CreateEvent( NULL, FALSE, FALSE, szActivityName);
                            } else {
                                fid.hActivityEvent = NULL;
                            }    
                            wcsncpy( fid.szFileSysName, pFSDLoadList->szName, FILESYSNAMESIZE-1);
                            wcscpy( fid.szRegKey, g_szAUTOLOAD_PATH);
                            wcscpy( fid.szDiskName, L"AutoLoad");
                            fid.hFSD = hFSD;
                            fid.pNextDisk = NULL;;
                            InitEx( &fid);
                        }    
                    } else {
                        if (FsdGetRegistryString(hKeyFS, g_szFILE_SYSTEM_DRIVER_STRING, szFSName, sizeof(szFSName)/sizeof(WCHAR))) {
                            CStore *pStore = new CStore(pFSDLoadList->szName, BLOCK_DRIVER_GUID);
                            pStore->SetBlockDevice(szFSName);
                            if (ERROR_SUCCESS == pStore->MountStore(TRUE)) {
                                AddStore(pStore);   
                            } else {
                                delete pStore;  
                            }
                        }
                    }
                }    
                RegCloseKey( hKeyFS);
            }
            pTemp = pFSDLoadList;
            pFSDLoadList = pFSDLoadList->pNext;
            delete pTemp;
        }
        RegCloseKey( hKeyAutoLoad);
    }
    if ((dwCurPhase == 2) && (dwFlags & LOAD_FLAG_SYNC)) {
        SetEvent(g_hAutoLoadEvent);
    }   
}

BYTE g_pPNPBuf[sizeof(DEVDETAIL) + 200];

#define DEFAULT_TIMEOUT_RESET 5000
#define DEFAULT_WAITIO_MULTIPLIER 3

DWORD PNPThread(LPVOID lParam)
{
    DWORD dwFlags, dwSize;
//    HKEY hDevKey;
    HANDLE hReg;
    DEVDETAIL * pd = (DEVDETAIL *)g_pPNPBuf;
    GUID guid = {0};
    HANDLE pHandles[3];
    TCHAR szGuid[MAX_PATH];
    HMODULE hCoreDll;
    DWORD dwTimeOut = INFINITE, dwTimeOutReset = DEFAULT_TIMEOUT_RESET;
    PSTOPDEVICENOT pStopDeviceNotification = NULL;
    PREQUESTDEVICENOT pRequestDeviceNotification = NULL;

    pHandles[0] = g_hPNPQueue;
    pHandles[1] = g_hPNPUpdateEvent;
    pHandles[2] = g_hAutoLoadEvent;

    HKEY hKey;
    if (ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, g_szSTORAGE_PATH, 0, 0, &hKey)) {
        DWORD dwPriority;
        if (!FsdGetRegistryValue(hKey, g_szReloadTimeOut, &dwTimeOutReset)) {
            dwTimeOutReset = DEFAULT_TIMEOUT_RESET;
        }
        if (!FsdGetRegistryValue( hKey, g_szWaitDelay, &g_dwWaitIODelay)) {
            g_dwWaitIODelay = dwTimeOutReset * DEFAULT_WAITIO_MULTIPLIER;
        }
        if (FsdGetRegistryValue( hKey, g_szPNPThreadPrio, &dwPriority)) {
            CeSetThreadPriority(GetCurrentThread(), dwPriority); 
        }
        RegCloseKey( hKey);
    }
    DEBUGMSG( ZONE_INIT, (L"STOREMGR: Using PNP unload delay of %ld\r\n", dwTimeOutReset));

    hCoreDll = (HMODULE)LoadLibrary(L"coredll.dll");
    if (hCoreDll) {
        pRequestDeviceNotification = (PREQUESTDEVICENOT)GetProcAddress( hCoreDll, L"RequestDeviceNotifications");
        pStopDeviceNotification = (PSTOPDEVICENOT)GetProcAddress( hCoreDll, L"StopDeviceNotifications");
    }
    FreeLibrary( hCoreDll); // This is okay since we should already have a reference to coredll

    if (pRequestDeviceNotification) {
        DEBUGMSG( ZONE_INIT, (L"STOREMGR: PNPThread Created\r\n"));
        while(!IsAPIReady(SH_DEVMGR_APIS)) {
            Sleep(1000);
        }
        hReg = pRequestDeviceNotification(&BLOCK_DRIVER_GUID, g_hPNPQueue, TRUE);
    }   
    
    while(TRUE) {
        DWORD dwWaitCode;
        dwWaitCode = WaitForMultipleObjects( 3, pHandles, FALSE, dwTimeOut);
        if (dwWaitCode == WAIT_TIMEOUT) {
            DEBUGMSG( ZONE_INIT, (L"STOREMGR: Scavenging stores\r\n"));
            LockStoreMgr();
            dwTimeOut = INFINITE;
            g_dwUpdateState &= ~STOREMGR_EVENT_UPDATETIMEOUT;
            UnlockStoreMgr();
            DetachStores(STORE_FLAG_DETACHED);                  
        } else {
            DWORD dwEvent = dwWaitCode - WAIT_OBJECT_0;
            switch(dwEvent) {
                case 0: {
                    if (ReadMsgQueue(g_hPNPQueue, pd, sizeof(g_pPNPBuf), &dwSize, INFINITE, &dwFlags)) {
                        FsdStringFromGuid(&pd->guidDevClass, szGuid);
                        DEBUGMSG( ZONE_INIT, (L"STOREMGR: Got a plug and play event %s Class(%s) Attached=%s!!!\r\n", pd->szName, szGuid, pd->fAttached ? L"TRUE":L"FALSE"));
                        if (memcmp( &pd->guidDevClass, &BLOCK_DRIVER_GUID, sizeof(GUID)) == 0) {
                            if (pd->fAttached) {
                                MountStore( pd->szName, pd->guidDevClass);
                            } else {
                                UnmountStore(pd->szName);
                            }    
                        }    
                    }             
                    break;
                }
                case 1: {
                    if (g_dwUpdateState & STOREMGR_EVENT_UPDATETIMEOUT) {
                        dwTimeOut = dwTimeOutReset;
                    }
                    if (g_dwUpdateState & STOREMGR_EVENT_REFRESHSTORE) {
                        LockStoreMgr();
                        BOOL fRet;
                        CStore *pStore = g_pStoreRoot;
                        TCHAR szName[MAX_PATH];
                        while(pStore) {
                            if (pStore->m_dwFlags & STORE_FLAG_REFRESHED) {
                                CStore *pStoreTemp = pStore;
                                wcscpy( szName, pStore->m_szDeviceName);
                                pStoreTemp->Lock();
                                DEBUGMSG( ZONE_INIT, (L"Refreshing store %s is happenning !!!\r\n", pStore->m_szOldDeviceName));
                                fRet = pStoreTemp->UnmountStore();
                                pStoreTemp->Unlock();
                                pStore = pStoreTemp->m_pNextStore;
                                if (fRet) {
                                    UpdateHandleFromList(g_pRootHandle, pStoreTemp, NULL);
                                    FSDMGR_AdvertiseInterface( &STORE_MOUNT_GUID, pStoreTemp->m_szDeviceName, FALSE);
                                    DeleteStore( pStoreTemp);
                                }
                            } else {
                                pStore = pStore->m_pNextStore;
                            }    
                        }
                        UnlockStoreMgr();
                    }
                    break;
                }
                case 2:
                    ResetEvent( g_hAutoLoadEvent);
                    AutoLoadFileSystems( 2, LOAD_FLAG_ASYNC);
                    break;
                default:
                    break;
            }   
        }    
    }    
    // SHould never get here !!!
    if (pStopDeviceNotification)
        pStopDeviceNotification(hReg);
    return 0;
}

 
BOOL InitStorageManager(DWORD dwBootPhase)
{
    if (!g_hSTRMGRApi) {
        DWORD dwThreadId=0;
        int iAFS = INVALID_AFS;
        MSGQUEUEOPTIONS msgopts;
        DWORD dwOrder = -1;

        iAFS = RegisterAFSName(L"StoreMgr");
        if (iAFS != INVALID_AFS && GetLastError() == 0) {
            g_hSTRMGRApi = CreateAPISet("PFSD", ARRAY_SIZE(apfnSTGMGRAPIs), (const PFNVOID *)apfnSTGMGRAPIs, asigSTGMGRAPIs);
            if (!RegisterAFSEx(iAFS, g_hSTRMGRApi, (DWORD)1, AFS_VERSION, AFS_FLAG_HIDDEN)) {
                DEBUGMSGW(ZONE_INIT,(DBGTEXTW("STOREMGR: InitStoreMgr failed registering secondary volume\r\n")));
                DeregisterAFSName(iAFS);
                goto Fail;
            }
        } else {
            goto Fail;
        }
        
        g_hFindStoreApi = CreateAPISet("FSTG", ARRAY_SIZE(apfnFindStoreAPIs), (const PFNVOID * )apfnFindStoreAPIs, asigFindStoreAPIs);
        g_hFindPartApi = CreateAPISet("FPRT", ARRAY_SIZE(apfnFindPartAPIs), (const PFNVOID * )apfnFindPartAPIs, asigFindPartAPIs);
        g_hStoreApi  = CreateAPISet("STRG", ARRAY_SIZE(apfnSTGAPIs), (const PFNVOID *)apfnSTGAPIs, asigSTGAPIs);
        g_hBlockDevApi = CreateAPISet("BDEV", ARRAY_SIZE(DevFileApiMethods), (const PFNVOID *)DevFileApiMethods, DevFileApiSigs);
        g_hPNPUpdateEvent = CreateEvent( NULL, FALSE, FALSE, NULL);

        msgopts.dwSize = sizeof(MSGQUEUEOPTIONS);
        msgopts.dwFlags = 0;
        msgopts.dwMaxMessages = 0; //?
        msgopts.cbMaxMessage = sizeof(g_pPNPBuf);
        msgopts.bReadAccess = TRUE;
        
        g_hPNPQueue = CreateMsgQueue(NULL, &msgopts);
        
        if (!g_hFindPartApi || !g_hFindStoreApi || !g_hStoreApi || !g_hPNPUpdateEvent || !g_hBlockDevApi || !g_hPNPQueue)
            goto Fail;
        
        RegisterAPISet(g_hFindStoreApi, HT_FIND | REGISTER_APISET_TYPE);
        RegisterAPISet(g_hFindPartApi, HT_FIND | REGISTER_APISET_TYPE);
        RegisterAPISet(g_hStoreApi, HT_FILE | REGISTER_APISET_TYPE);
        RegisterAPISet(g_hBlockDevApi, HT_FILE | REGISTER_APISET_TYPE);

        g_hAutoLoadEvent = CreateEvent( NULL, TRUE, FALSE, NULL);
        if (!g_hAutoLoadEvent)
            goto Fail;

        InitializeCriticalSection(&g_csStoreMgr);

        if (g_hPNPThread = CreateThread( NULL, 0, PNPThread, NULL, 0, &dwThreadId)) {
        } else {
            goto Fail;
        }    
    }     

    AutoLoadFileSystems(dwBootPhase, LOAD_FLAG_SYNC);

    return TRUE;
Fail:
    return FALSE;
}


// External API's
HANDLE STRMGR_CreateFileW(DWORD dwData, HANDLE hProc, PCWSTR pwsFileName, DWORD dwAccess, DWORD dwShareMode, PSECURITY_ATTRIBUTES pSecurityAttributes, DWORD dwCreate, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    if (*pwsFileName == L'\\') 
        pwsFileName++;
    return STG_OpenStore( pwsFileName, hProc);
}

HANDLE STRMGR_FindFirstFileW(DWORD dwData, HANDLE hProc, PCWSTR pwsFileSpec, PWIN32_FIND_DATAW pfd)
{
    return STG_FindFirstStore((STOREINFO * )pfd, hProc);
}

BOOL   STRMGR_RefreshStore(DWORD dwData, PWSTR pwsFileName, PCWSTR szReserved)
{
    if (*pwsFileName == L'\\') 
        pwsFileName++;
    LockStoreMgr();
    CStore *pStore = FindStore( pwsFileName);
    if (pStore) {
        pStore->Lock();
        pStore->m_dwFlags |= STORE_FLAG_REFRESHED;
        pStore->Unlock();
        g_dwUpdateState |= STOREMGR_EVENT_REFRESHSTORE;
        SetEvent( g_hPNPUpdateEvent);
    } else {
        MountStore( pwsFileName, BLOCK_DRIVER_GUID);
    }
    UnlockStoreMgr();
    return TRUE;
}

