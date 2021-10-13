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

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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

#ifdef __cplusplus
}
#endif /* __cplusplus */

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

void  GetMountSettings(HKEY hKey, LPDWORD dwMountFlags);

extern const TCHAR *g_szSTORAGE_PATH;
extern const TCHAR *g_szPROFILE_PATH;
extern const TCHAR *g_szAUTOLOAD_PATH;
extern const TCHAR *g_szReloadTimeOut;
extern const TCHAR *g_szWaitDelay;
extern const TCHAR *g_szPNPThreadPrio;
extern const TCHAR *g_szAUTO_PART_STRING;
extern const TCHAR *g_szAUTO_MOUNT_STRING;
extern const TCHAR *g_szAUTO_FORMAT_STRING;
extern const TCHAR *g_szPART_DRIVER_STRING;
extern const TCHAR *g_szPART_DRIVER_MODULE_STRING;
extern const TCHAR *g_szPART_DRIVER_NAME_STRING;
extern const TCHAR *g_szPART_TABLE_STRING;
extern const TCHAR *g_szFILE_SYSTEM_STRING;
extern const TCHAR *g_szFILE_SYSTEM_MODULE_STRING;
extern const TCHAR *g_szFSD_BOOTPHASE_STRING;
extern const TCHAR *g_szFILE_SYSTEM_DRIVER_STRING;
extern const TCHAR *g_szFSD_ORDER_STRING;
extern const TCHAR *g_szFSD_LOADFLAG_STRING;
extern const TCHAR *g_szSTORE_NAME_STRING;
extern const TCHAR *g_szFOLDER_NAME_STRING;
extern const TCHAR *g_szMOUNT_FLAGS_STRING;
extern const TCHAR *g_szATTRIB_STRING;
extern const TCHAR *g_szACTIVITY_TIMER_STRING;
extern const TCHAR *g_szACTIVITY_TIMER_ENABLE_STRING;
extern const TCHAR *g_szCHECKFORFORMAT;

extern const TCHAR *g_szMOUNT_FLAG_MOUNTASROOT_STRING;
extern const TCHAR *g_szMOUNT_FLAG_MOUNTHIDDEN_STRING;
extern const TCHAR *g_szMOUNT_FLAG_MOUNTASBOOT_STRING;
extern const TCHAR *g_szMOUNT_FLAG_MOUNTASROM_STRING;
extern const TCHAR *g_szMOUNT_FLAG_MOUNTSYSTEM_STRING;
extern const TCHAR *g_szMOUNT_FLAG_MOUNTPERMANENT_STRING;

extern const TCHAR *g_szDEFAULT_PARTITION_DRIVER;
extern const TCHAR *g_szDEFAULT_FILESYSTEM;
extern const TCHAR *g_szDEFAULT_STORAGE_NAME;
extern const TCHAR *g_szDEFAULT_FOLDER_NAME;
extern const TCHAR *g_szDEFAULT_ACTIVITY_NAME; 


#endif //_STOREMAIN_H_
