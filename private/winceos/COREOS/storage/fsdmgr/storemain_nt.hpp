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

#ifndef UNDER_CE

#ifndef _STOREMAIN_H_
#define _STOREMAIN_H_

class StoreDisk_t;
class PartitionDisk_t;
class StoreHandle_t;
class MountableDisk_t;
class FileSystemDriver_t;

typedef StoreHandle_t STOREHANDLE, *PSTOREHANDLE, SEARCHSTORE, *PSEARCHSTORE, SEARCHPARTITION, *PSEARCHPARTITION;

LRESULT InitializeStoreAPI ();
LRESULT AutoLoadFileSystems (DWORD CurrentBootPhase, DWORD LoadFlags);
LRESULT MountFileSystemDriver (MountableDisk_t* pDisk, FileSystemDriver_t* pFSD, DWORD MountFlags, BOOL fDoFormat);
LRESULT UnmountFileSystemDriver (MountableDisk_t* pDisk);
LRESULT InitializeStorageManager (DWORD BootPhase);

BOOL MountStore (const WCHAR* pDeviceName, const GUID* pDeviceGuid, const WCHAR* pDriverPath = NULL, StoreDisk_t** ppStore = NULL);
BOOL UnmountStore(StoreDisk_t* pStore);
void WINAPI DetachStores (DWORD DetachMask);

#ifdef UNDER_CE
#define OWN_CS(pcs) (GetCurrentThreadId ( ) == (DWORD)(pcs)->OwnerThread)
#else
#define OWN_CS(pcs) (TRUE)
#endif

extern DWORD  g_dwWaitIODelay;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

HANDLE WINAPI STG_OpenStore(LPCTSTR szDeviceName, HANDLE hProc);
HANDLE WINAPI STG_OpenStoreEx(LPCTSTR szDeviceName, HANDLE hProc, DWORD dwAccess);
BOOL   WINAPI STG_GetStoreInfo(PSTOREHANDLE pStoreHandle, PSTOREINFO pInfo);
BOOL   WINAPI STG_DismountStore(PSTOREHANDLE pStoreHandle);
BOOL   WINAPI STG_FormatStore(PSTOREHANDLE pStoreHandle);
HANDLE WINAPI STG_FindFirstStore(STOREINFO *pInfo, HANDLE hProc);
BOOL   WINAPI STG_FindNextStore(PSEARCHSTORE pSearch, STOREINFO *pInfo, DWORD SizeOfInfo);
BOOL   WINAPI STG_FindCloseStore(PSEARCHSTORE pSearch);
BOOL   WINAPI STG_CreatePartition(PSTOREHANDLE pStoreHandle, LPCTSTR szPartitionName, DWORD dwPartType, DWORD dwHighSec, DWORD dwLowSec, BOOL bAuto);
BOOL   WINAPI STG_DeletePartition(PSTOREHANDLE pStoreHandle, LPCTSTR szPartitionName);
HANDLE WINAPI STGINT_OpenPartition(PSTOREHANDLE pStoreHandle, LPCTSTR szPartitionName);
HANDLE WINAPI STGEXT_OpenPartition(PSTOREHANDLE pStoreHandle, LPCTSTR szPartitionName);
BOOL   WINAPI STG_MountPartition(PSTOREHANDLE pStoreHandle);
BOOL   WINAPI STG_DismountPartition(PSTOREHANDLE pStoreHandle);
BOOL   WINAPI STG_RenamePartition(PSTOREHANDLE pStoreHandle, LPCTSTR szNewName);
BOOL   WINAPI STG_SetPartitionAttributes(PSTOREHANDLE pStoreHandle, DWORD dwAttrs);
BOOL   WINAPI STG_DeviceIoControl(PSTOREHANDLE pStoreHandle, DWORD dwIoControlCode, PVOID pInBuf, DWORD nInBufSize, PVOID pOutBuf, DWORD nOutBufSize, PDWORD pBytesReturned, OVERLAPPED *pOverlapped);
BOOL   WINAPI STG_GetPartitionInfo(PSTOREHANDLE pStoreHandle, PPARTINFO pInfo);
BOOL   WINAPI STG_FormatPartition(PSTOREHANDLE pStoreHandle, DWORD bPartType, BOOL bAuto);
HANDLE WINAPI STGINT_FindFirstPartition(PSTOREHANDLE pStoreHandle, PPARTINFO pInfo);
HANDLE WINAPI STGEXT_FindFirstPartition(PSTOREHANDLE pStoreHandle, PPARTINFO pInfo);
BOOL   WINAPI STG_FindNextPartition(PSEARCHPARTITION pSearch, PPARTINFO pInfo, DWORD SizeOfInfo);
BOOL   WINAPI STG_FindClosePartition(PSEARCHPARTITION pSearch);
BOOL   WINAPI STG_CloseHandle(PSTOREHANDLE pHandle);


#ifdef __cplusplus
}
#endif /* __cplusplus */

extern CRITICAL_SECTION g_csStoreMgr;
extern HANDLE g_hFindStoreApi;
extern HANDLE g_hFindPartApi;
extern HANDLE g_hStoreApi;
extern HANDLE g_hPNPUpdateEvent;
extern DWORD g_dwUpdateState;

#define STOREMGR_EVENT_UPDATETIMEOUT 0x00000001
#define STOREMGR_EVENT_REFRESHSTORE  0x00000002

void DeleteStore ( StoreDisk_t *pStore );
void WINAPI DetachStores(DWORD dwFlags);
void SuspendStores ();
void ResumeStores ();

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

#define INVALID_STORE ((StoreDisk_t *)INVALID_HANDLE_VALUE)
#define INVALID_PARTITION ((PartitionDisk_t *)INVALID_HANDLE_VALUE)

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
extern const TCHAR *g_szDISABLE_ON_SUSPEND;
extern const TCHAR *g_szROMFS_STRING;
extern const TCHAR *g_szROMFS_DLL_STRING;

extern const TCHAR *g_szMOUNT_FLAG_MOUNTASROOT_STRING;
extern const TCHAR *g_szMOUNT_FLAG_MOUNTHIDDEN_STRING;
extern const TCHAR *g_szMOUNT_FLAG_MOUNTASBOOT_STRING;
extern const TCHAR *g_szMOUNT_FLAG_MOUNTASROM_STRING;
extern const TCHAR *g_szMOUNT_FLAG_MOUNTSYSTEM_STRING;
extern const TCHAR *g_szMOUNT_FLAG_MOUNTPERMANENT_STRING;
extern const TCHAR *g_szMOUNT_FLAG_MOUNTASNETWORK_STRING;


extern const TCHAR *g_szDEFAULT_PARTITION_DRIVER;
extern const TCHAR *g_szDEFAULT_FILESYSTEM;
extern const TCHAR *g_szDEFAULT_STORAGE_NAME;
extern const TCHAR *g_szDEFAULT_FOLDER_NAME;
extern const TCHAR *g_szDEFAULT_ACTIVITY_NAME; 


#endif //_STOREMAIN_H_

#endif /* !UNDER_CE */
