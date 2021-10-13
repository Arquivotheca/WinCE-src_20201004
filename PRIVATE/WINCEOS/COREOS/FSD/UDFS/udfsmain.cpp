//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+-------------------------------------------------------------------------
//
//
//  File:       udfsmain.cpp
//
//  Contents:
//
//  Classes:
//
//  Functions:
//
//--------------------------------------------------------------------------

#include "udfs.h"


#define ARRAYSIZE(a)            (sizeof(a)/sizeof(a[0]))
#define ERRFALSE(exp)           extern char __ERRXX[(exp)!=0]
#define REGSTR_PATH_UDFS        L"System\\StorageManager\\UDFS"

PUDFSDRIVERLIST g_pHeadFSD = NULL;          
CRITICAL_SECTION g_csMain;
HANDLE g_hHeap;
const SIZE_T g_uHeapInitSize = 20;    // Initial size of the heap in KB.
SIZE_T g_uHeapMaxSize = 0;    // Maximum size of the heap in KB. The default is unbounded.
BOOL g_bDestroyOnUnmount = FALSE;


//+-------------------------------------------------------------------------
//
//  Function:   InitGlobals
//
//  Synopsis:
//
//  Arguments:  None
//
//  Returns: void
//
//
//  Notes:
//
//--------------------------------------------------------------------------

void InitGlobals(void)
{
    DWORD dwError;
    DWORD dwSize;
    DWORD dwType;
    DWORD dwData;
    HKEY hKey;

    dwError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, REGSTR_PATH_UDFS, 0, 0, &hKey);

    if(dwError == ERROR_SUCCESS)
    {
        dwSize = sizeof(DWORD);
        dwError = RegQueryValueEx(hKey, L"HeapMaxSize", NULL, &dwType, (LPBYTE)&dwData, &dwSize);

        if(dwError == ERROR_SUCCESS && dwType == REG_DWORD)
        {
            // Reserve at least as many pages as we initially commit.
            g_uHeapMaxSize = dwData == 0 ? 0 : max(dwData, g_uHeapInitSize);
        }

        dwSize = sizeof(DWORD);
        dwError = RegQueryValueEx(hKey, L"DestroyHeapOnUnmount", NULL, &dwType, (LPBYTE)&dwData, &dwSize);

        if(dwError == ERROR_SUCCESS && dwType == REG_DWORD)
        {
            g_bDestroyOnUnmount = dwData == 1 ? TRUE : FALSE;
        }

        RegCloseKey(hKey);
    }

    return;
}


//+-------------------------------------------------------------------------
//
//  Function:   DllMain
//
//  Synopsis:
//
//  Arguments:  [DllInstance] --
//              [Reason]      --
//              [Reserved]    --
//
//  Returns:
//
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL WINAPI DllMain(HANDLE DllInstance, DWORD Reason, LPVOID Reserved)
{
    switch(Reason) {
        case DLL_PROCESS_ATTACH:

            DisableThreadLibraryCalls( (HMODULE)DllInstance);
            DEBUGREGISTER((HINSTANCE) DllInstance);

#if DEBUG
            if (ZONE_INITBREAK) {
                DebugBreak();
            }
#endif
            
            InitGlobals();

            if(!g_bDestroyOnUnmount) {
                g_hHeap = HeapCreate(0, 1024 * g_uHeapInitSize, 1024 * g_uHeapMaxSize);
            }

            InitializeCriticalSection(&g_csMain);

            DEBUGMSG(ZONE_INIT,(TEXT("UDFSFS!UDFSMain: DLL_PROCESS_ATTACH\n")));
            return TRUE;

        case DLL_PROCESS_DETACH:
            
            if(!g_bDestroyOnUnmount) {
                HeapDestroy( g_hHeap);
            }

            DeleteCriticalSection(&g_csMain); 

            DEBUGMSG(ZONE_INIT,(TEXT("UDFSFS!UDFSMain: DLL_PROCESS_DETACH\n")));
            return TRUE;

        default:
            DEBUGMSG(ZONE_INIT,(TEXT("UDFSFS!UDFSMain: Reason #%d ignored\n"), Reason));
            return TRUE;
    }

    DEBUGMSG(ZONE_INIT,(TEXT("UDFSFS!UDFSMain: Reason #%d failed\n"), Reason));
    
    return FALSE;
}


//---------------------------------------------------------------------------
//
//  Function:   FSD_MountDisk
//
//  Synopsis:   Initialization service called by FSDMGR.DLL
//
//  Arguments:  
//      hdsk == FSDMGR disk handle, or NULL to deinit all
//      frozen volumes on *any* disk that no longer has any open
//      files or dirty buffers.
//
//  Returns:    BOOL   
//
//  Notes:
//
//---------------------------------------------------------------------------

BOOL FSD_MountDisk(HDSK hDsk)
{
    PUDFSDRIVER pUDFS;
    BOOL fRet;

    DEBUGMSG( ZONEID_INIT, (TEXT("UDFS!MountDisk hDsk=%08X\r\n"), hDsk));
    //
    //  Create the volume class
    //

    pUDFS = new CReadOnlyFileSystemDriver();

    if (pUDFS == NULL) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    
    fRet = pUDFS->Initialize(hDsk);

    if (fRet) {
        EnterCriticalSection( &g_csMain);
        UDFSDRIVERLIST *pUDFSEntry = new UDFSDRIVERLIST;
        if (pUDFSEntry) {
            pUDFSEntry->hDsk = hDsk;
            pUDFSEntry->pUDFS = pUDFS;
            pUDFSEntry->pUDFSNext  = g_pHeadFSD;
            g_pHeadFSD = pUDFSEntry;
        } else {
            DEBUGCHK(0);
        }    
        LeaveCriticalSection( &g_csMain);
    } else {
        delete pUDFS;
    }
    return fRet;
}


//---------------------------------------------------------------------------
//
//  Function:   FSD_UnmountDisk
//
//  Synopsis:   Deinitialization service called by FSDMGR.DLL
//
//  Arguments:  
//      hdsk == FSDMGR disk handle, or NULL to deinit all
//      frozen volumes on *any* disk that no longer has any open
//      files or dirty buffers.
//
//  Returns:    BOOL   
//
//  Notes:
//
//---------------------------------------------------------------------------

BOOL FSD_UnmountDisk(HDSK hDsk)
{
    DEBUGMSG( ZONEID_INIT, (TEXT("UDFS!UnmountDisk hDsk=%08X\r\n"), hDsk));
    EnterCriticalSection(&g_csMain);
    PUDFSDRIVERLIST pUDFSEntry = g_pHeadFSD;
    PUDFSDRIVERLIST pUDFSPrev = NULL;
    while( pUDFSEntry) {
        if (pUDFSEntry->hDsk == hDsk) {
            if (pUDFSPrev) {
                pUDFSPrev->pUDFSNext = pUDFSEntry->pUDFSNext;
            } else {
                g_pHeadFSD = pUDFSEntry->pUDFSNext;
            }
            break;
        }
        pUDFSPrev = pUDFSEntry;
        pUDFSEntry = pUDFSEntry->pUDFSNext;
    }
    LeaveCriticalSection(&g_csMain);
    if (pUDFSEntry) {
        if(pUDFSEntry->pUDFS) {
            pUDFSEntry->pUDFS->DeregisterVolume(pUDFSEntry->pUDFS);
            delete pUDFSEntry->pUDFS;
            pUDFSEntry->pUDFS = NULL;
        }
        delete pUDFSEntry;
        return TRUE;
    }

    return FALSE;
}

