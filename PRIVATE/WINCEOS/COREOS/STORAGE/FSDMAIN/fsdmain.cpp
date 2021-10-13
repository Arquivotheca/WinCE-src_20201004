//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    init.c

Abstract:

    This file contains the FSDMGR initialization/deinitialization entry
    points that initiate the loading/unloading other installable file
    system drivers (ie, FSDs such as FATFS.DLL).

    DEVICE.EXE still loads v1 and v2 FSDs and communicates with them via
    their FSD_Init and FSD_Deinit exports, but it also supports new FSDs
    indirectly via this new "FSD Manager", named FSDMGR.DLL.

    DEVICE.EXE detects a new FSD by the existence of an FSD_MountDisk export.
    In that case, it loads FSDMGR.DLL (if it hasn't already), and notifies
    FSDMGR_InitEx of (1) the module handle of the new FSD, and (2) the name
    of the new device (eg, "DSK1").

    FSDMGR.DLL interacts with the new FSD (XXX.DLL) through the following
    interfaces, which XXX.DLL must export:

      XXX_MountDisk(PDSK pDsk)

        Mounts the specified disk, returning TRUE if success, FALSE if error.
        The FSD must be able to cope with this call being issued redundantly
        (ie, it must determine from the media itself, not the uniqueness of
        the given DSK pointer, whether or not this disk has already been mounted).
        This is due to the nature of removable media and the fact that media
        may or may not have been removed -- or may have changed slots -- while
        the system was powered off.

        All I/O to the disk must be performed via calls to FSDMGR_ReadDisk and
        FSDMGR_WriteDisk in FSDMGR.DLL.  Similarly, FSDMGR_RegisterVolume should
        be used to register each volume present on the disk with the file system.

        Note that this function must also be exported as "FSD_MountDisk".

      XXX_UnmountDisk(PDSK pDsk)

        Unmounts the specified disk.  Returns TRUE if XXX.DLL successfully
        unmounted the disk, FALSE if not.  A requirement of returning
        TRUE is that all volumes on the disk previously registered with
        FSDMGR_RegisterVolume must first be deregistered with FSDMGR_DeregisterVolume.

        Note that this function must also be exported as "FSD_UnmountDisk".

    These rest of these exports define the functionality of the FSD.  Any
    function not exported is replaced with a stub function in FSDMGR that returns
    failure.

      XXX_CloseVolume
      XXX_CreateDirectoryW
      XXX_RemoveDirectoryW
      XXX_GetFileAttributesW
      XXX_SetFileAttributesW
      XXX_DeleteFileW
      XXX_MoveFileW
      XXX_DeleteAndRenameFileW (formerly known as "PrestoChangoFileName")
      XXX_GetDiskFreeSpaceW (formerly known as "GetDiskFreeSpace")
      XXX_Notify
      XXX_RegisterFileSystemFunction
      XXX_CreateFileW
      XXX_CloseFile
      XXX_ReadFile
      XXX_WriteFile
      XXX_GetFileSize
      XXX_SetFilePointer
      XXX_GetFileInformationByHandle
      XXX_FlushFileBuffers
      XXX_GetFileTime
      XXX_SetFileTime
      XXX_SetEndOfFile
      XXX_DeviceIoControl
      XXX_ReadFileWithSeek
      XXX_WriteFileWithSeek
      XXX_FindFirstFileW
      XXX_FindNextFileW
      XXX_FindClose

    There are also some noteworthy omissions.  We neither look for nor install handlers
    for the following APIs because they were not supported after v1.0:

      XXX_CeRegisterFileSystemNotification

    Similarly, the following APIs were not supported after v2.0:

      XXX_CeOidGetInfo

    And last but not least, XXX_CloseAllFiles is not supported because FSDMGR.DLL can
    enumerate all known handles for a given process and call XXX_CloseFile, eliminating
    the need for a special process termination handler.

--*/

#include "fsdmgrp.h"
#include "storemain.h"
#include "fsnotify.h"


/*  Globals
 */

HINSTANCE   hDLL;               // module handle of this DLL
HANDLE      hAFSAPI;
HANDLE      hFileAPI;
HANDLE      hFindAPI;
FSD_DLINK   dlFSDList;          // global FSD list
VOL_DLINK   dlVOLList;          // global VOL list
CRITICAL_SECTION csFSD;         // global CS for this DLL
HANDLE      g_hResumeEvent;

// AFS mount-as-root file system mount/dismount notification event handle
extern HANDLE g_hRootNotifyHandle;

void UnHookAllFilters(PVOL pVol);

#define AFS_FROM_MOUNT_FLAGS(flags)     (((flags) >> 16) & 0xFFFF)

BOOL InternalDiskIoControl(HANDLE hDsk, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped)
{
    return DeviceIoControl(hDsk, dwIoControlCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, lpBytesReturned, lpOverlapped);
}


PDSK FSDLoad( FSDINITDATA *pInitData)
{
    PFSD pFSD;
    PDSK pDsk;

    DEBUGMSGW(ZONE_INIT, (DBGTEXTW("FSDMGR_InitEx: initializing FSD for %s\n"), pInitData->szDiskName));

    pFSD = AllocFSD((HMODULE)pInitData->hFSD, pInitData->hDsk);
    pDsk = AllocDisk(pFSD, pInitData->szDiskName, pInitData->hDsk, pInitData->pIoControl);

    if (pDsk){
        FSDMGR_GetDiskInfo(pDsk, &pDsk->fdi);
        pDsk->hPartition = pInitData->hPartition;
        pDsk->dwFlags |= pInitData->dwFlags << 16;
        pDsk->hActivityEvent = pInitData->hActivityEvent;
        pDsk->pNextDisk = pInitData->pNextDisk;
        pDsk->pPrevDisk = NULL;
    }    

    if (pFSD) {
        wcscpy( pFSD->szFileSysName, pInitData->szFileSysName);
        wcscpy( pFSD->szRegKey, pInitData->szRegKey);
        if (!FSDMGR_GetRegistryFlag(pDsk, L"Paging", &pFSD->dwFlags, FSD_FLAGS_PAGEABLE)) {
            pFSD->dwFlags |= FSD_FLAGS_PAGEABLE;
        }
        DEBUGMSG( ZONE_INIT, (L"FSDMGR:InitEx pFSD=%08X pDsk=%08X pwsDsk=%s - Paging is %s\r\n", pFSD, pDsk, pInitData->szDiskName, pFSD->dwFlags & FSD_FLAGS_PAGEABLE ? L"Enabled" : L"Disabled"));
    }
    return pDsk;
}

PDSK InitEx( FSDINITDATA *pInitData)
{
    BOOL bSuccess = FALSE;
    PDSK pDsk = FSDLoad( pInitData);
    PDSK pDskNew = NULL;
    PFSD pFSD = NULL;

    if (pDsk) {
        pDskNew = HookFilters( pDsk, pDsk->pFSD);
        pFSD = pDsk->pFSD;
    }   
    if (pFSD && pDsk) {
        if (pFSD->pfnMountDisk) {
            if (pFSD->pfnMountDisk(pDsk)) {
                ASSERT(VALIDSIG(pFSD, FSD_SIG) && VALIDSIG(pDsk, DSK_SIG));
                UnmarkDisk(pDsk, DSK_UNCERTAIN);
                bSuccess = TRUE;
            }   
        } else {
            if (pFSD->pfnHookVolume) { // Alresdy checked in AllocFSD ?
                UnmarkDisk(pDsk, DSK_UNCERTAIN);  // This is just a filter.
                bSuccess = TRUE;
            }   
        }
    }
    if (!bSuccess) {
        if (pDskNew) {
            pDsk = pDskNew;
            // Remove all the filters
            while( pDsk->pNextDisk) {
                HMODULE hFSD;
                pDskNew = pDsk;
                pFSD = pDsk->pFSD;
                pDsk = pDsk->pNextDisk;
                hFSD = pFSD->hFSD;
                DeallocDisk( pDskNew);
                DeallocFSD(pFSD);
                FreeLibrary( hFSD);
            }
            pFSD = pDsk->pFSD;
            DeallocDisk(pDsk);
            pDskNew = NULL;
        }    
        if (pFSD) {
            DeallocFSD(pFSD);
            pFSD = NULL;
        }
    }
    DEBUGMSG(ZONE_INIT, (DBGTEXT("FSDMGR_InitEx returned 0x%x\n"), pDsk));
    return (pDskNew);
}


/*  FSDMGR_InitEx - Entry point for DEVICE.EXE to initialize new FSD
 *
 *  ENTRY
 *      hFSD == handle to new FSD
 *      pwsDsk -> disk name
 *      pReserved == NULL (reserved for future use)
 *
 *  EXIT
 *      Pointer to internal DSK structure if successful, NULL if error
 *
 *  NOTES
 *      When DEVICE.EXE initializes an old-style FSD directly, it keeps
 *      track of how many devices the FSD claims to be managing.
 *
 *      DEVICE.EXE maintains a similar count for devices passed to FSDMGR,
 *      so for each device, we allocate a DSK structure and make sure we
 *      report a NEW (as opposed to REMOUNTED) DSK only once.
 *
 *      This function assumes it is single-thread in virtue of the fact that
 *      DEVICE.EXE is single-threaded when it calls it.
 */

PDSK FSDMGR_InitEx(HMODULE hFSD, PCWSTR pwsDsk, LPVOID pReserved)
{
    FSDINITDATA fid;
    fid.pIoControl = (PDEVICEIOCONTROL)InternalDiskIoControl;
    fid.hDsk = INVALID_HANDLE_VALUE;
    fid.hPartition = NULL;
    fid.dwFlags = 0;
    wcscpy( fid.szFileSysName, L"");
    wcscpy( fid.szRegKey, L"");
    wcsncpy( fid.szDiskName, pwsDsk, MAX_PATH-1);
    fid.szDiskName[MAX_PATH-1] = 0;
    fid.hFSD = hFSD;
    fid.pNextDisk= NULL;
    return InitEx( &fid);
}


/*  FSDMGR_DeinitEx - Entry point for DEVICE.EXE to deinitialize new FSD
 *
 *  ENTRY
 *      pDsk -> DSK structure, or NULL to deinit all disks that are no
 *      longer present (ie, whose UNCERTAIN bit is still set)
 *
 *  EXIT
 *      TRUE if the specified DSK structure has been freed, FALSE if not;
 *      or, if no DSK structure was specified, we return the number of DSKs
 *      we were able to free.
 *
 *  NOTES
 *      This function assumes it is single-thread in virtue of the fact that
 *      DEVICE.EXE is single-threaded when it calls it.
 */

int FSDMGR_DeinitEx(PDSK pDsk)
{
    PFSD pFSD;
    PVOL pVol;
    PDSK pDskTemp;
    ASSERT(VALIDSIG( pDsk, DSK_SIG));
    DEBUGMSGW(ZONE_INIT, (DBGTEXTW("DeinitEx(0x%x): deinit'ing FSD for %s\n"), pDsk, pDsk? ((PDSK)((DWORD)pDsk & ~0x1))->wsDsk : TEXTW("all disks")));

    pVol = pDsk->pVol;
    ASSERT(VALIDSIG(pVol, VOL_SIG));

    // block the dismount of a volume flagged as permanent 
    if (AFS_FLAG_PERMANENT & AFS_FROM_MOUNT_FLAGS(pDsk->dwFlags)) {
        return 0;
    }

    // take the FSD critical section so that no new threads will be allowed
    // to enter the volume (see TryEnterVolume, TryEnterHandle)
    EnterCriticalSection(&csFSD);

    // remove the volume from the list of valid volumes, and mark it for 
    // deinitialization; once this flag is set, no more threads are allowed 
    // to enter the volume (see TryEnterVolume, TryEnterHandle)
    RemoveListItem((PDLINK)&pVol->dlVol);    
    pVol->dwFlags |= VOL_DEINIT;

    // if there are currently threads accessing this FSD, we don't want to
    // deallocate the volume out from underneath it. so, create an event
    // that the final thread to exit the volume can use to signal us when 
    // it has completed accessing the volume
    if (pVol->cThreads) {
        DEBUGCHK(NULL == pVol->hThreadExitEvent);
        // there are curently threads accessing this FSD, create an event
        // that the last thread to exit will use to signal
        pVol->hThreadExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        DEBUGCHK(pVol->hThreadExitEvent);
    }
    
    LeaveCriticalSection(&csFSD);

#ifdef UNDER_CE
    // deregister the AFS volume and name so that the mount-point goes away
    if (pVol->iAFS != INVALID_AFS) {
        VERIFYTRUE(DeregisterAFS(pVol->iAFS));
        if (pVol->iAFS > OID_FIRST_AFS)
            VERIFYTRUE(DeregisterAFSName(pVol->iAFS));
        pVol->iAFS = INVALID_AFS;
        if (g_hRootNotifyHandle && (g_hRootNotifyHandle != pVol->hNotifyHandle)) {
            NotifyPathChange(g_hRootNotifyHandle, pVol->wsVol, TRUE, FILE_ACTION_REMOVED);
        } 
    }
#endif    
    
    // clean-up notification handle for the volume
    if (pVol->hNotifyHandle) {
        if (pVol->hNotifyHandle == g_hRootNotifyHandle) {
            g_hRootNotifyHandle = NULL;
        }
        NotifyDeleteVolume(pVol->hNotifyHandle);
        pVol->hNotifyHandle = NULL;
    }    


    // wait for the final thread to finish accessing the volume before 
    // continuing with clean-up
    if (pVol->hThreadExitEvent) {
        WaitForSingleObject(pVol->hThreadExitEvent, INFINITE);
        VERIFY(CloseHandle(pVol->hThreadExitEvent));
        pVol->hThreadExitEvent = NULL;
    }

    // at this point, all threads in the volume when DeinitEx was called
    // should have left, and no new threads should have been allowed to
    // enter the volume. so, the thread count should be zero, and we can
    // finish cleaning up the volume
    DEBUGCHK(0 == pVol->cThreads);    

    // NULL the ioctl function to prevent the FSD from doing disk i/o 
    // when handles are closed or the disk is unmounted
    pDsk->pDeviceIoControl = NULL;

    // invalidate all handles on the volume, and call into FSD_CloseFile
    // and FSD_FindClose to deallocate them. we have to deallocate the
    // handles before unhooking any filters because handles must be
    // closed through the filter dlls via the filter FSD APIs
    EnterCriticalSection(&csFSD); 
    DeallocVolumeHandles(pVol);    
    LeaveCriticalSection(&csFSD);

    // unhook all the filters (call FILTER_UnhookVolume on all filters)
    UnHookAllFilters(pDsk->pVol);

    // walk down the filter chain to find the real pDsk that the volume 
    // registered so we can pass it to FSD_UnmountDisk
    pDskTemp = pDsk;    
    while( pDsk->pNextDisk) {
        pDsk = pDsk->pNextDisk;
    }
    pFSD = pDsk->pFSD;
    ASSERT(VALIDSIG(pFSD, FSD_SIG));

    // it is too late at this point for the FSD to fail the FSD_UnmountDisk at this point
    // so if the call fails, ignore it and continue with the cleanup
    pFSD->pfnUnmountDisk(pDsk);

    // even if FSD_UnmountDisk fails, continue with the dismount
    
    // deallocate the volume (free the PVOL object)
    EnterCriticalSection(&csFSD); 
    DeallocVolume(pVol);
    LeaveCriticalSection(&csFSD);
        
    pDsk = pDskTemp;
    // Remove all the filters
    while( pDsk->pNextDisk) {
        HMODULE hFSD;
        pDskTemp = pDsk;
        pFSD = pDsk->pFSD;
        pDsk = pDsk->pNextDisk;
        hFSD = pFSD->hFSD;
        DeallocDisk( pDskTemp);
        DeallocFSD(pFSD);
        FreeLibrary( hFSD);
    }
    pFSD = pDsk->pFSD;

    DeallocDisk(pDsk);

    // Similarly, DeallocFSD won't actually deallocate the FSD if it
    // still has disks attached.

    DeallocFSD(pFSD);

    return 1;
}


/*  FSDMGR_Attach - DLL_PROCESS_ATTACH handler
 *
 *  ENTRY
 *      None
 *
 *  EXIT
 *      TRUE if successful, FALSE if not.  Most failures can probably
 *      be attributed to insufficient memory.
 *
 *  NOTES
 *      This is assumed to be protected by a critical section.  Since it's
 *      currently called only by FSDMGR_Main, we are protected by the loader's
 *      critical section.
 */


BOOL FSDMGR_Attach()
{
    DEBUGREGISTER(hDLL);
    InitList((PDLINK)&dlFSDList);
    InitList((PDLINK)&dlVOLList);
    InitializeCriticalSection(&csFSD);

    hAFSAPI = CreateAPISet("PFSD", ARRAY_SIZE(apfnAFSAPIs), (const PFNVOID *)apfnAFSAPIs, asigAFSAPIs);
    hFileAPI = CreateAPISet("HFSD", ARRAY_SIZE(apfnFileAPIs), (const PFNVOID *)apfnFileAPIs, asigFileAPIs);
    RegisterAPISet(hFileAPI, HT_FILE | REGISTER_APISET_TYPE);
    hFindAPI = CreateAPISet("FFSD", ARRAY_SIZE(apfnFindAPIs), (const PFNVOID * )apfnFindAPIs, asigFindAPIs);
    RegisterAPISet(hFindAPI, HT_FIND | REGISTER_APISET_TYPE);
    g_hResumeEvent = CreateEvent( NULL, TRUE, TRUE, NULL);

    return hAFSAPI && hFileAPI && hFindAPI;
}


/*  FSDMGR_Detach - DLL_PROCESS_DETACH handler
 *
 *  ENTRY
 *      None
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (currently, always successful)
 *
 *  NOTES
 *      This is assumed to be protected by a critical section.  Since it's
 *      currently called only by FSDMGR_Main, we are protected by the loader's
 *      critical section.
 */

BOOL FSDMGR_Detach(void)
{
    CloseHandle(hFindAPI);
    CloseHandle(hFileAPI);
    CloseHandle(hAFSAPI);
    DeleteCriticalSection(&csFSD);
    return TRUE;
}


/*  FSDMGR_Main - FSDMGR.DLL initialization entry point
 *
 *  ENTRY
 *      DllInstance == DLL module handle
 *      Reason == DLL_* initialization message
 *      pReserved == NULL (reserved for future use)
 *
 *  EXIT
 *      TRUE if successful, FALSE if not.  Most failures can probably
 *      be attributed to insufficient memory.
 */

BOOL WINAPI DllMain(HANDLE DllInstance, DWORD Reason, LPVOID Reserved)
{
    switch(Reason) {

    case DLL_PROCESS_ATTACH:
        DEBUGMSG(ZONE_INIT, (DBGTEXT("FSDMGR_Main: DLL_PROCESS_ATTACH\n")));
        DisableThreadLibraryCalls((HMODULE) DllInstance);
        hDLL = (HINSTANCE)DllInstance;
        return FSDMGR_Attach();

    case DLL_PROCESS_DETACH:
        DEBUGMSG(ZONE_INIT, (DBGTEXT("FSDMGR_Main: DLL_PROCESS_DETACH\n")));
        return FSDMGR_Detach();

    default:
      //DEBUGMSG(ZONE_INIT, (DBGTEXT("FSDMGR_Main: Reason #%d ignored\n"), Reason));
        break;
    }
    return TRUE;
}
