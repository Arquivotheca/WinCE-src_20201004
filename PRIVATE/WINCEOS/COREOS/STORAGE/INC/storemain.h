//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _STOREMAIN_H_
#define _STOREMAIN_H_

#include <storemgr.h>
#include <store.h>
#include <partition.h>
#include <blockdev.h>

BOOL InitStorageManager(DWORD dwBootPhase);
BOOL IsValidStore(CStore *pStore);

HANDLE WINAPI STG_OpenStore(LPCTSTR szDeviceName, HANDLE hProc);
BOOL   WINAPI STG_GetStoreInfo(PSTOREHANDLE pStoreHandle, PSTOREINFO pInfo);
BOOL   WINAPI STG_DismountStore(PSTOREHANDLE pStoreHandle);
BOOL   WINAPI STG_FormatStore(PSTOREHANDLE pStoreHandle);
HANDLE WINAPI STG_FindFirstStore(STOREINFO *pInfo, HANDLE hProc);
BOOL   WINAPI STG_FindNextStore(PSEARCHSTORE pSearch, STOREINFO *pInfo);
BOOL   WINAPI STG_FindCloseStore(PSEARCHSTORE pSearch);
BOOL   WINAPI STG_CreatePartition(PSTOREHANDLE pStoreHandle, LPCTSTR szPartitionName, DWORD dwPartType, DWORD dwHighSec, DWORD dwLowSec, BOOL bAuto);
BOOL   WINAPI STG_DeletePartition(PSTOREHANDLE pStoreHandle, LPCTSTR szPartitionName);
HANDLE WINAPI STG_OpenPartition(PSTOREHANDLE pStoreHandle, LPCTSTR szPartitionName);
BOOL   WINAPI STG_MountPartition(PSTOREHANDLE pStoreHandle);
BOOL   WINAPI STG_DismountPartition(PSTOREHANDLE pStoreHandle);
BOOL   WINAPI STG_RenamePartition(PSTOREHANDLE pStoreHandle, LPCTSTR szNewName);
BOOL   WINAPI STG_SetPartitionAttributes(PSTOREHANDLE pStoreHandle, DWORD dwAttrs);
BOOL   WINAPI STG_DeviceIoControl(PSTOREHANDLE pStoreHandle, DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned, OVERLAPPED *pOverlapped);
BOOL   WINAPI STG_GetPartitionInfo(PSTOREHANDLE pStoreHandle, PPARTINFO pInfo);
BOOL   WINAPI STG_FormatPartition(PSTOREHANDLE pStoreHandle, DWORD bPartType, BOOL bAuto);
HANDLE WINAPI STG_FindFirstPartition(PSTOREHANDLE pStoreHandle, PPARTINFO pInfo);
BOOL   WINAPI STG_FindNextPartition(PSEARCHPARTITION pSearch, PPARTINFO pInfo);
BOOL   WINAPI STG_FindClosePartition(PSEARCHPARTITION pSearch);
BOOL   WINAPI STG_CloseHandle(PSTOREHANDLE pHandle);

extern CRITICAL_SECTION g_csStoreMgr;
extern CStore *g_pStoreRoot;
extern HANDLE g_hFindStoreApi;
extern HANDLE g_hFindPartApi;
extern HANDLE g_hStoreApi;
extern HANDLE g_hBlockDevApi;
extern PSTOREHANDLE g_pRootHandle;
extern HANDLE g_hPNPUpdateEvent;
extern DWORD g_dwUpdateState;

#define STOREMGR_EVENT_UPDATETIMEOUT 0x00000001
#define STOREMGR_EVENT_REFRESHSTORE  0x00000002

BOOL AddHandleToList(PSTOREHANDLE *ppRoot, PSTOREHANDLE pHandle);
BOOL DeleteHandleFromList(PSTOREHANDLE *ppRoot, PSTOREHANDLE pHandle);
BOOL UpdateHandleFromList( PSTOREHANDLE pRoot, CStore *pStore, CPartition *pPartition);
void WINAPI DetachStores(DWORD dwFlags);

// Some inline functions

inline CStore *GetStorageRoot()
{
    return g_pStoreRoot;
}    

inline void LockStoreMgr()
{
    EnterCriticalSection(&g_csStoreMgr);
}

inline void UnlockStoreMgr()
{
    LeaveCriticalSection(&g_csStoreMgr);
}

inline void GetDriverError(DWORD *pdwError)
{
    *pdwError = GetLastError();
    if (*pdwError == ERROR_SUCCESS) {
        *pdwError = ERROR_GEN_FAILURE;
    }    
}

#define INVALID_STORE ((CStore *)INVALID_HANDLE_VALUE)
#define INVALID_PARTITION ((CPartition *)INVALID_HANDLE_VALUE)


#endif //_STOREMAIN_H_
