//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "relfsd.h"
#include "cefs.h"

#ifdef DEBUG

DBGPARAM dpCurSettings =
{
    TEXT("ReleaseFSD"),
    {
        TEXT("Init"),
        TEXT("Api"),
        TEXT("Error"),
        TEXT("Create"),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT(""),
        TEXT("")
    },
#if 1
    ZONEMASK_ERROR | ZONEMASK_CREATE
#else
    0xFFFF
#endif
    
};

#endif


VolumeState *g_pvs;

DWORD RELFSD_GetFileAttributesW(PVOLUME pvol, PCWSTR pwsFileName);

#define DBG_LIST    L"dbglist.txt"

__inline BOOL IsSeparator (char ch)
{
	return (' ' == ch) || ('\t' == ch) || ('\r' == ch) || ('\n' == ch) || (',' == ch) || (';' == ch);
}

BOOL UpdateDbgList (char *pTxt, int nSize)
{
    int     idx = 0, nChars = 0;
    LPWSTR  pDbgList;

    // allocate the new list (+2 for max of 2 additional trailing 0)
    if (!(pDbgList = (LPWSTR) LocalAlloc (LMEM_FIXED, (nSize + 2) * sizeof (WCHAR)))) {
        return FALSE;
    }
    
    // construct the new list
    do {

        // skip leading white spaces
        while ((idx < nSize) && IsSeparator (pTxt[idx])) {
            idx ++;
        }

        // copy the name (non-whitespace chars)
        while ((idx < nSize) && (!IsSeparator (pTxt[idx]))) {
            pDbgList[nChars ++] = (WCHAR) pTxt[idx ++];
        }

        // null terminate the string
        pDbgList[nChars ++] = 0;
        

    } while (idx < nSize);


    // add trailing 0
    pDbgList[nChars ++] = 0;

    // notify nk for non-empty list
    if (nChars > 2) {
        KernelLibIoControl ((HANDLE)KMOD_CORE, IOCTL_KLIB_SETDBGLIST, pDbgList, nChars*sizeof(WCHAR), NULL, 0, NULL);
    }

    LocalFree (pDbgList);

    return TRUE;
}

BOOL LoadDgbList (void)
{
    int     fd = ropen (DBG_LIST, _O_RDONLY);
    BOOL    fRet = FALSE;

    DEBUGMSG (ZONE_APIS, (L"RefreshDgbList: Opening '%s', fd = %8.8lx\r\n", DBG_LIST, fd));

    if (-1 != fd) {
        int nSize = rlseek (fd, 0, SEEK_END);     // get the size of the file
        
        if ((-1 != nSize) && (rlseek (fd, 0, SEEK_SET) != -1)) {        // move back to beginning of file
        
            char *pTmp = (char *) LocalAlloc (LMEM_FIXED, nSize);       // temp storage

            DEBUGMSG (1, (L"size of '%s' = %8.8lx\r\n", DBG_LIST, nSize));

            if (pTmp && (rread (fd, pTmp, nSize) == nSize)) {

                fRet = UpdateDbgList (pTmp, nSize);
            }
            LocalFree (pTmp);
        }
        
        rclose (fd);
    }

    return fRet;
}


DWORD RelfsdMountThread(LPVOID lParam) 
{
    BOOL fSuccess = FALSE;
    HANDLE hEvent;
    HDSK hDsk = (HDSK)lParam;

    DEBUGMSG(ZONE_INIT, (L"ReleaseFSD: FSD_MountDisk\n"));

    EnterCriticalSectionMM(g_pcsMain);
    do {
        if (PPSHConnect()) {
            g_pvs = LocalAlloc(0, sizeof(VolumeState));
            if (g_pvs) {
                g_pvs->vs_Handle = FSDMGR_RegisterVolume(hDsk, L"Release", (DWORD)g_pvs);
                if (g_pvs->vs_Handle) {
                    DEBUGMSG (ZONE_ERROR, (L"Mounted ReleaseFSD volume '\\Release'\n"));
                }
            }
            break;
        } else {
            Sleep(5000);
        }    
    } while(TRUE);    
    LeaveCriticalSectionMM(g_pcsMain);

    // Set event API
    hEvent = CreateEvent(NULL, TRUE, FALSE, L"ReleaseFSD"); 
    SetEvent (hEvent);

    // Used for registry functions for PPFS in kernel
    CreateEvent(NULL, TRUE, FALSE, L"WAIT_RELFSD2"); 

    // load debug list
    LoadDgbList();

    return 0;
}

BOOL RELFSD_MountDisk(HDSK hDsk)
{
    // if KITL has already started, connect directly, otherwise create a thread to handle the work
    if (KernelIoControl (IOCTL_EDBG_IS_STARTED, NULL, 0, NULL, 0, NULL)) {
        RelfsdMountThread ((LPVOID)hDsk);
    } else {
        DWORD dwId;
        HANDLE hThread = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)RelfsdMountThread, (LPVOID)hDsk, 0, &dwId);
        CloseHandle( hThread);
    }
    return TRUE;
}


/*  RELFSD_UnmountDisk - Deinitialization service called by FSDMGR.DLL
 *
 *  ENTRY
 *      hdsk == FSDMGR disk handle, or NULL to deinit all
 *      frozen volumes on *any* disk that no longer has any open
 *      files or dirty buffers.
 *
 *  EXIT
 *      0 if failure, or the number of devices that were successfully
 *      unmounted.
 */

BOOL RELFSD_UnmountDisk(HDSK hdsk)
{
    FSDMGR_DeregisterVolume(g_pvs->vs_Handle);
    return TRUE;
}

CRITICAL_SECTION g_csMain;
LPCRITICAL_SECTION g_pcsMain;

BOOL WINAPI DllMain(HINSTANCE DllInstance, DWORD dwReason, LPVOID Reserved)
{
    switch(dwReason) {

    case DLL_PROCESS_ATTACH:
        {
            DisableThreadLibraryCalls( (HMODULE)DllInstance);
            DEBUGREGISTER(DllInstance);
            DEBUGMSG(ZONE_INIT,(TEXT("RELFSD!DllMain: DLL_PROCESS_ATTACH\n")));
            g_pcsMain = MapPtrToProcess( &g_csMain, GetCurrentProcess());
            InitializeCriticalSection(g_pcsMain);
        }            
        break;
    case DLL_PROCESS_DETACH:
        {
            DEBUGBREAK (1);
            DeleteCriticalSection(g_pcsMain);
            DEBUGMSG(ZONE_INIT,(TEXT("RELFSD!DllMain: DLL_PROCESS_DETACH\n")));
        }
        break;
    default:
        break;
    }
    return TRUE;
}
