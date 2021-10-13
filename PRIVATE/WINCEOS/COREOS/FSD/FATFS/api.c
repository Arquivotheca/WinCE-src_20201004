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

    api.c

Abstract:

    This file contains the file system API sets for the FAT file system.

Revision History:

--*/

#include "fatfs.h"
#include <storemgr.h>

#ifdef UNDER_CE
#ifdef DEBUG
DBGPARAM dpCurSettings = {
        TEXTW("FATFS"), {
            TEXTW("Init"),              // 0x0001
            TEXTW("Errors"),            // 0x0002
            TEXTW("Shell Msgs"),        // 0x0004
            TEXTW("TFAT"),                  // 0x0008
            TEXTW("Memory"),            // 0x0010
            TEXTW("APIs"),              // 0x0020
            TEXTW("Messages"),          // 0x0040
            TEXTW("Streams"),           // 0x0080
            TEXTW("Buffers"),           // 0x0100
            TEXTW("Clusters"),          // 0x0200
            TEXTW("FAT I/O"),           // 0x0400
            TEXTW("Disk I/O"),          // 0x0800
            TEXTW("Log I/O"),           // 0x1000
            TEXTW("Read Verify"),       // 0x2000
            TEXTW("Write Verify"),      // 0x4000
            TEXTW("Prompts"),           // 0x8000
        },
        ZONEMASK_DEFAULT
};
#endif
#endif


CONST WCHAR awcFATFS[] = TEXTW("SYSTEM\\StorageManager\\FATFS");
CONST WCHAR awcCompVolID[] = TEXTW("CompVolID");
CONST WCHAR awcFlags[] = TEXTW("Flags");                // NOTE: "Flags" supercedes "UpdateAccess", but
CONST WCHAR awcCodePage[] = TEXTW("CodePage");
CONST WCHAR awcUpdateAccess[] = TEXTW("UpdateAccess");  // UpdateAccess was supported on v1.0, so we'll keep both -JTP
CONST WCHAR awcPathCacheEntries[] = TEXTW("PathCacheEntries");  
CONST WCHAR awcFormatTfat[] = TEXTW("FormatTfat");  
CONST WCHAR awcSecureWipe[] = TEXTW("SecureWipe");  
#ifdef DEBUG
CONST WCHAR awcDebugZones[] = TEXTW("DebugZones");
#endif


/*  Globals
 */

int         cLoads;             // count of DLL_PROCESS_ATTACH's
#ifdef SHELL_MESSAGE_NOTIFICATION
HWND        hwndShellNotify;    // from FAT_RegisterFileSystemNotification
#endif
#ifdef SHELL_CALLBACK_NOTIFICATION
SHELLFILECHANGEFUNC_t pfnShell; // from FAT_RegisterFileSystemFunction
#endif


HINSTANCE   hFATFS;


DSK_DLINK   dlDisks;            // keeps track of every open FAT device
DWORD       cFATThreads;
HANDLE      hevStartup;
HANDLE      hevShutdown;
CRITICAL_SECTION csFATFS;
HANDLE      hHeap;

#ifdef DEBUG
int         cbAlloc;            // total bytes allocated
CRITICAL_SECTION csAlloc;
DWORD       csecWrite = 0, creqWrite = 0;
#endif

/*  FAT_CloseAllFiles
 *
 *  Walk the open stream list, and for each open stream, walk the open
 *  handle list, and for each open handle, close it.  Each CloseFileHandle
 *  becomes CloseHandle, which becomes FAT_CloseFile, which issues a
 *  CloseStream for the associated stream, and if that's the stream's last
 *  reference, CloseStream frees it.
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      hProc - handle of terminating process;
 *              or NULL to close all file handles;
 *              or INVALID_HANDLE_VALUE to *commit* all file handles
 *
 *  EXIT
 *      TRUE if successful, FALSE if not.
 */

BOOL FAT_CloseAllFiles(PVOLUME pvol, HANDLE hProc)
{
    PFHANDLE pfh, pfhEnd;
    PDSTREAM pstm, pstmEnd;

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_CloseAllFiles(0x%x,0x%x)\r\n"), pvol, hProc));

    // We can't hold the volume's stream list critical section indefinitely (like
    // while attempting to enter an arbitrary stream's critical section), so we
    // have to let it go after selecting a stream to visit.  So each stream has a
    // VISITED bit that helps us remember which streams we have/have not visited.

    // To protect the integrity of the VISITED bits, all functions that
    // manipulate them must take the volume's critical section, too.

    EnterCriticalSection(&pvol->v_cs);

    // First, make sure the VISITED bit is clear in every stream currently
    // open on this volume.

    EnterCriticalSection(&pvol->v_csStms);

    pstm = pvol->v_dlOpenStreams.pstmNext;
    pstmEnd = (PDSTREAM)&pvol->v_dlOpenStreams;

    while (pstm != pstmEnd) {

        pstm->s_flags &= ~STF_VISITED;

        // If all we're doing is committing, then we don't need to visit volume-based streams
        // (just file-based streams).

        if (hProc == INVALID_HANDLE_VALUE && (pstm->s_flags & STF_VOLUME))
            pstm->s_flags |= STF_VISITED;

        pstm = pstm->s_dlOpenStreams.pstmNext;
    }

    // Now find the next unvisited stream.  Note that every iteration of the
    // loop starts with the volume's "stream list" critical section held.

  restart_streams_on_volume:
    pstm = pvol->v_dlOpenStreams.pstmNext;
    while (pstm != pstmEnd) {

        if (pstm->s_flags & STF_VISITED) {
            pstm = pstm->s_dlOpenStreams.pstmNext;
            continue;
        }

        pstm->s_flags |= STF_VISITED;

        // Add a ref to ensure that the stream can't go away once we
        // let go of the volume's critical section.

        pstm->s_refs++;
        LeaveCriticalSection(&pvol->v_csStms);
        EnterCriticalSection(&pstm->s_cs);

      restart_files_on_stream:
        pfh = pstm->s_dlOpenHandles.pfhNext;
        pfhEnd = (PFHANDLE)&pstm->s_dlOpenHandles;

        while (pfh != pfhEnd) {

            if (pfh->fh_hProc == hProc || hProc == NULL || hProc == INVALID_HANDLE_VALUE) {

                DEBUGMSGW(ZONE_ERRORS,(DBGTEXTW("FATFS!FAT_CloseAllFiles: file %.11hs still open by 0x%08x\r\n"), pstm->s_achOEM, pfh->fh_hProc));

                if (hProc != INVALID_HANDLE_VALUE) {
                    DEBUGMSGW(ZONE_ERRORS,(DBGTEXTW("FATFS!FAT_CloseAllFiles: closing file now (0x%08x -> 0x%08x)\r\n"), pfh, pfh->fh_h));
                    SetHandleOwner(pfh->fh_h, GetCurrentProcess());
                    VERIFYTRUE(CloseFileHandle(pfh));
                    goto restart_files_on_stream;
                }
                else {
                    DEBUGMSGW(ZONE_ERRORS,(DBGTEXTW("FATFS!FAT_CloseAllFiles: committing file now (0x%08x -> 0x%08x)\r\n"), pfh, pfh->fh_h));
                    FAT_FlushFileBuffers(pfh);
                }
            }
            pfh = pfh->fh_dlOpenHandles.pfhNext;
        }

        CloseStream(pstm);
        EnterCriticalSection(&pvol->v_csStms);
        goto restart_streams_on_volume;
    }

    LeaveCriticalSection(&pvol->v_csStms);
    LeaveCriticalSection(&pvol->v_cs);

    DEBUGMSG(ZONE_APIS,(DBGTEXT("FATFS!FAT_CloseAllFiles returned TRUE\r\n")));
    return TRUE;
}


/*  FAT_Notify - Power off/device on notification handler
 *
 *  ENTRY
 *      pvol - pointer to VOLUME (NULL if none)
 *      dwFlags - notification flags (see FSNOTIFY_*)
 *
 *  EXIT
 *      None
 *
 *  NOTES
 *      On FSNOTIFY_POWER_OFF notification, we want to make sure that all
 *      threads currently in FATFS that could, however briefly, put the
 *      disk into an inconsistent state, complete before we return from the
 *      power-off notification.  This is accomplished by key FATFS APIs
 *      calling FATEnter and FATExit on entry and exit, so that we can (1)
 *      prevent new threads from entering, and (2) wake ourselves up when all
 *      old threads have left.
 *
 *      On FSNOTIFY_POWER_ON notification, we simply set the volume's RETAIN
 *      flag, so that the next (imminent) FSD_Deinit will not cause the
 *      PC Card folder to be deregistered.  After the FSNOTIFY_DEVICES_ON
 *      notification, DEVICE.EXE will call FSD_Deinit *again* with a special
 *      value (NULL) requesting us to completely close any volumes we no longer
 *      need to retain.
 *
 *      On FSNOTIFY_DEVICES_ON notification, we simply clear our shutdown
 *      bit and allow any threads blocked in FATEnter to proceed.  All driver(s)
 *      should once again be completely functional at that time.
 */

void FAT_Notify(PVOLUME pvol, DWORD dwFlags)
{
}


/*  FATEnter - Gates threads entering FATFS
 *
 *  ENTRY
 *      pvol - pointer to VOLUME
 *      idLog - one of the LOGID_* equates
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (SetLastError is already set)
 *
 *
 *  QUALIFICATIONS
 *      This is currently called only on every FATFS API entry point that
 *      could possibly generate I/O (with the exception of commit/close).  If
 *      a thread slips into GetFileTime, for example, who cares?  This could be
 *      restricted even further, and called only on entry points that could
 *      possibly generate WRITES, but since we ALSO want to ensure that shutdowns
 *      are timely (as well as safe), preventing as much I/O as possible seems
 *      worthwhile.
 *
 *      Commit/close functions are exempted simply to allow us to block all
 *      threads and cleanly flush/unmount a volume without deadlocking.
 */

BOOL FATEnter(PVOLUME pvol, BYTE idLog)
{
    if (cLoads == 0) {
        SetLastError(ERROR_NOT_READY);
        return FALSE;
    }
    InterlockedIncrement(&cFATThreads);

    if (!BufEnter(pvol, FALSE)) {
        DEBUGMSGW(ZONE_ERRORS, (DBGTEXTW("FATFS!FATEnter: BufEnter failed, failing API request!\r\n")));
        InterlockedDecrement(&cFATThreads);
        return FALSE;
    }

    return TRUE;
}

void FATEnterQuick(void)
{
    InterlockedIncrement(&cFATThreads);
}


/*  FATExit  - Gates threads exiting FATFS
 *
 *  ENTRY
 *      idLog - one of the LOGID_* equates
 *
 *  EXIT
 *      None
 *
 *  NOTES
 *      Called at the end of most FATFS API entry points.  If we are
 *      the last thread, and the shutdown bit is set, then we will signal
 *      the shutdown event.  The shutdown event is initially reset AND
 *      auto-reset.
 *
 *      Also, if there is any risk of code inside here modifying the error code
 *      we set via SetLastError (if any), then that code must always save/restore
 *      the last error code.  I think even if all the APIs it calls succeeds, some 
 *      of those APIs may do something like "SetLastError(0)" when they initialize, 
 *      so we have to guard against that.
 *
 *  QUALIFICATIONS
 *      This is currently called only on every FATFS API entry point that
 *      could possibly generate I/O (with the exception of commit/close).  If
 *      a thread slips into GetFileTime, for example, who cares?  This could be
 *      restricted even further, and called only on entry points that could
 *      possibly generate WRITES, but since we ALSO want to ensure that shutdowns
 *      are timely (as well as safe), preventing as much I/O as possible seems
 *      worthwhile.
 *
 *      Commit/close functions are exempted simply to allow us to block all
 *      threads and cleanly flush/unmount a volume without deadlocking.
 */

void FATExit(PVOLUME pvol, BYTE idLog)
{
    BufExit(pvol);

    if (InterlockedDecrement(&cFATThreads) == 0) {
    }
}

void FATExitQuick(void)
{
    InterlockedDecrement(&cFATThreads);
}




/*  FATAttach - DLL_PROCESS_ATTACH handler
 *
 *  ENTRY
 *      None
 *
 *  EXIT
 *      TRUE if successful, FALSE if not.  Most failures can probably
 *      be attributed to insufficient memory.
 *
 *  NOTES
 *      This is assumed to be protected by a critical section.  Since it is
 *      currently called only by FATMain, we are protected by the loader's
 *      critical section.
 */

BOOL FATAttach()
{
    BOOL fInit = TRUE;

    if (cLoads++ == 0) {
        DEBUGREGISTER(hFATFS);
        
        InitList((PDLINK)&dlDisks);

#ifdef DEBUG
        InitializeCriticalSection(&csAlloc);
        DEBUGALLOC(DEBUGALLOC_CS);
#endif
        InitializeCriticalSection(&csFATFS);
        DEBUGALLOC(DEBUGALLOC_CS);

        hevStartup  = CreateEvent(NULL, TRUE, TRUE, NULL);
        DEBUGALLOC(DEBUGALLOC_EVENT);
        hevShutdown = CreateEvent(NULL, FALSE, FALSE, NULL);
        DEBUGALLOC(DEBUGALLOC_EVENT);

        hHeap = HeapCreate (0, 0x1000, 0);
    }
    return hevStartup && hevShutdown && hHeap && fInit;
}


/*  FATDetach - DLL_PROCESS_DETACH handler
 *
 *  ENTRY
 *      None
 *
 *  EXIT
 *      TRUE if successful, FALSE if not (currently, it always returns TRUE)
 *
 *  NOTES
 *      This is assumed to be protected by a critical section.  Since it is
 *      currently called only by FATMain, we are protected by the loader's
 *      critical section.
 */

BOOL FATDetach()
{
        // Now make sure every file is closed and every volume is freed,
        // so that we don't leak any memory.  We have deliberately omitted
        // FATEnter/FATExit from FAT_CloseAllFiles and FAT_CloseFile
        // to avoid deadlocking if any files *do* need to be closed.

        UnmountAllDisks(FALSE);

        DEBUGFREE(DEBUGALLOC_EVENT);
        CloseHandle(hevShutdown);
        DEBUGFREE(DEBUGALLOC_EVENT);
        CloseHandle(hevStartup);

        DEBUGFREE(DEBUGALLOC_CS);
        DeleteCriticalSection(&csFATFS);
#ifdef DEBUG
        DEBUGFREE(DEBUGALLOC_CS);
        DeleteCriticalSection(&csAlloc);
#endif

        HeapDestroy (hHeap);
    
        DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!FATDetach complete (%d sectors written in %d requests)\r\n"), csecWrite, creqWrite)); 
        return TRUE;
}


BOOL FSD_MountDisk(HDSK hDsk)
{
    DWORD flVol;
    PDSK pdsk = NULL;
    TCHAR szName[MAX_PATH];
    STORAGEDEVICEINFO sdi;
    DWORD dwRet;

    wsprintf(szName, L"%08X", hDsk);

    DEBUGMSGW(ZONE_INIT || ZONE_APIS,(DBGTEXTW("FSD_MountDisk: mounting volumes for hDsk=%08X\r\n"), hDsk));

    flVol = VOLF_NONE;
    
    memset (&sdi, 0, sizeof(STORAGEDEVICEINFO));
    sdi.cbSize = sizeof(STORAGEDEVICEINFO);
    
    if (FSDMGR_DiskIoControl(hDsk, IOCTL_DISK_DEVICE_INFO, &sdi, sizeof(STORAGEDEVICEINFO), NULL, 0, &dwRet, NULL)) {
        if (sdi.dwDeviceFlags & STORAGE_DEVICE_FLAG_READONLY) {
            flVol = VOLF_READONLY;
        }
    }


    DEBUGALLOC(DEBUGALLOC_HANDLE);

    pdsk = MountDisk((HANDLE)hDsk, szName, flVol);

    if (pdsk) {
        pdsk->d_flags |= flVol;     // VOLF_READONLY maps to DSKF_READONLY

        if (pdsk->d_flags & (DSKF_REMOUNTED | DSKF_RECYCLED)) {
            // Make sure the REMOUNT bit in the VOLUME pointer is set
            (DWORD)pdsk |= 0x1;
        }
    }
    
    DEBUGMSG(ZONE_APIS,(DBGTEXT("FSD_Init returned 0x%x\r\n"), pdsk));
    return (pdsk != NULL);
}



BOOL FSD_UnmountDisk(HDSK hDsk)
{
    BOOL fSuccess = TRUE;
    PDSK pdsk = NULL;

    DEBUGMSG(ZONE_INIT || ZONE_APIS,(DBGTEXT("FSD_UnmountDisk(0x%x): unmounting...\r\n"), hDsk));

    EnterCriticalSection(&csFATFS);
    if (!hDsk) {
        // Unmount all volumes still marked frozen on all disks
        fSuccess = UnmountAllDisks(TRUE);
    }
    else if (pdsk = FindDisk((HANDLE)hDsk, NULL, NULL)) {
        // Make sure the REMOUNT bit in the VOLUME pointer is clear
        (DWORD)pdsk &= ~1;
        fSuccess = UnmountDisk(pdsk, FALSE);
    }
    LeaveCriticalSection( &csFATFS);
    DEBUGMSG(ZONE_APIS,(DBGTEXT("FSD_Deinit(0x%x) returned %d\r\n"), pdsk, fSuccess));
    return fSuccess;
}


/*  FATMain - FATFS.DLL initialization entry point
 *
 *  ENTRY
 *      DllInstance - DLL module handle
 *      Reason - DLL_* initialization message
 *      Reserved - reserved
 *
 *  EXIT
 *      TRUE if successful, FALSE if not.  Most failures can probably
 *      be attributed to insufficient memory.
 */

BOOL WINAPI DllMain(HANDLE DllInstance, DWORD Reason, LPVOID Reserved)
{
    switch(Reason) {

    case DLL_PROCESS_ATTACH:

        // DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!FATMain: DLL_PROCESS_ATTACH\r\n")));

		
        DisableThreadLibraryCalls( (HMODULE)DllInstance);
        hFATFS = (HINSTANCE)DllInstance;
        return FATAttach();

    case DLL_PROCESS_DETACH:
      //DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!FATMain: DLL_PROCESS_DETACH\r\n")));
        return FATDetach();

    default:
      //DEBUGMSG(ZONE_INIT,(DBGTEXT("FATFS!FATMain: Reason #%d ignored\r\n"), Reason));
        break;
    }
    return TRUE;
}
