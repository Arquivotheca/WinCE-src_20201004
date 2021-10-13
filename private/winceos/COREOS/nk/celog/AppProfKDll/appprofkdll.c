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

#include <windows.h>
#include <celog.h>
#include <pkfuncs.h>
#include "AppProfKDll.h"


// Inputs from kernel
CeLogImportTable    g_PubImports;
FARPROC             g_pfnKernelLibIoControl;

HANDLE g_hPrevThread;                 // Previous running thread, NULL for idle
HANDLE g_hCurThread;                  // Current running thread, NULL for idle

#define MODNAME TEXT("AppProfilerKDll")


//------------------------------------------------------------------------------
// CeLogQueryZones: Called by CeLogGetZones to retrieve the current zone
// settings for this DLL.
//------------------------------------------------------------------------------
BOOL
QueryZones( 
    LPDWORD lpdwZoneUser,
    LPDWORD lpdwZoneCE,
    LPDWORD lpdwZoneProcess
    )
{
    // The zones for this DLL never change.
    if (lpdwZoneUser)
        *lpdwZoneUser = (DWORD)-1;
    if (lpdwZoneCE)
        *lpdwZoneCE = CELZONE_RESCHEDULE;
    if (lpdwZoneProcess)
        *lpdwZoneProcess = (DWORD)-1;

    return TRUE;
}


//------------------------------------------------------------------------------
// CeLogData: Called by the kernel whenever CeLog events occur, or when an
// application calls the CeLogData API.  This function receives events from the
// zones that are enabled for this DLL, and potentially also receives events
// from other zones.
//------------------------------------------------------------------------------
VOID
LogData( 
    BOOL  fTimeStamp,
    WORD  wID,
    VOID* pData,
    WORD  wLen,
    DWORD dwZoneUser,
    DWORD dwZoneCE,
    WORD  wFlag,
    BOOL  fFlagged
    )
{
    if (wID == CELID_THREAD_SWITCH) {
        CEL_THREAD_SWITCH* pcel = (CEL_THREAD_SWITCH*) pData;
        if (g_hCurThread != pcel->hThread) {
            g_hPrevThread = g_hCurThread;
            g_hCurThread = pcel->hThread;
        }
    }
}


//------------------------------------------------------------------------------
// IOControl: Invoked when an app calls KernelLibIoControl with the handle of
// this DLL.
//------------------------------------------------------------------------------
BOOL
IOControl( 
    DWORD   dwInstData,
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{
    switch (dwIoControlCode) {

    case IOCTL_KDLL_GET_PREV_THREAD:
        // Query the ID of the thread that ran most recently before the current
        // thread.  If the CPU was idle before the current thread, returns 0.
        // Logging must be enabled.
        //
        // lpInBuf:     ignored
        // nInBufSize:  ignored
        // lpOutBuf:    Buffer to receive a DWORD with the thread ID.
        // nOutBufSize: Must be at least sizeof(DWORD).

        if (lpBytesReturned) {
            *lpBytesReturned = sizeof(DWORD);
        }
        if (lpOutBuf && (nOutBufSize >= sizeof(DWORD))) {
            *((DWORD*) lpOutBuf) = (DWORD) g_hPrevThread;
            return TRUE;
        }
        g_PubImports.pSetLastError (ERROR_INSUFFICIENT_BUFFER);
        return FALSE;

    default:
        ;
    }
    
    return FALSE;
}


//------------------------------------------------------------------------------
// Hook up function pointers and initialize logging
//------------------------------------------------------------------------------
static BOOL
InitLibrary ()
{
    CeLogExportTable exports;

    //
    // KernelLibIoControl provides the back doors we need to obtain kernel
    // function pointers and register logging functions.
    //
    
    // Get public imports from kernel
    g_PubImports.dwVersion = 4;
    if (!g_pfnKernelLibIoControl ((HANDLE)KMOD_CELOG, IOCTL_CELOG_IMPORT, &g_PubImports,
                                  sizeof(CeLogImportTable), NULL, 0, NULL)) {
        // Can't DEBUGMSG or anything here b/c we need the import table to do that
        return FALSE;
    }

    g_PubImports.pNKDbgPrintfW(MODNAME TEXT(": init\r\n"));

    // Register logging functions with the kernel
    exports.dwVersion             = 2;
    exports.pfnCeLogData          = LogData;
    exports.pfnCeLogInterrupt     = NULL;
    exports.pfnCeLogSetZones      = NULL;  // Not supported, zones are static
    exports.pfnCeLogQueryZones    = QueryZones;
    exports.dwCeLogTimerFrequency = 0;
    if (!g_pfnKernelLibIoControl ((HANDLE)KMOD_CELOG, IOCTL_CELOG_REGISTER, &exports,
                                  sizeof(CeLogExportTable), NULL, 0, NULL)) {
        g_PubImports.pNKDbgPrintfW (MODNAME TEXT(": Unable to register logging functions with kernel\r\n"));
        return FALSE;
    }

    return TRUE;
}


//------------------------------------------------------------------------------
// Shut down logging and unhook function pointers.  Note that this is not really
// threadsafe because other threads could be in the middle of a logging call --
// there is no way to force them out.
//------------------------------------------------------------------------------
BOOL static
CleanupLibrary ()
{
    CeLogExportTable exports;

    // Deregister logging functions with the kernel
    exports.dwVersion          = 2;
    exports.pfnCeLogData       = LogData;
    exports.pfnCeLogInterrupt  = NULL;
    exports.pfnCeLogSetZones   = NULL;
    exports.pfnCeLogQueryZones = QueryZones;
    exports.dwCeLogTimerFrequency = 0;
    if (!g_pfnKernelLibIoControl ((HANDLE)KMOD_CELOG, IOCTL_CELOG_DEREGISTER, &exports,
                                  sizeof(CeLogExportTable), NULL, 0, NULL)) {
        g_PubImports.pNKDbgPrintfW (MODNAME TEXT(": Unable to deregister logging functions with kernel\r\n"));
        return FALSE;
    }
    
    return TRUE;
}


//------------------------------------------------------------------------------
// DLL entry
//------------------------------------------------------------------------------
BOOL WINAPI
KernelDLLEntry (HINSTANCE DllInstance, INT Reason, LPVOID Reserved)
{
    static BOOL s_fInit = FALSE;

    switch (Reason) {
    case DLL_PROCESS_ATTACH:
        if (!s_fInit) {
            if (Reserved) {
                // Reserved parameter is a pointer to KernelLibIoControl function
#pragma warning(disable:4055) // typecast from data pointer LPVOID to function pointer FARPROC
                g_pfnKernelLibIoControl = (FARPROC) Reserved;
#pragma warning(default:4055)
                s_fInit = InitLibrary ();
                return s_fInit;
            }
            // Loaded via LoadLibrary instead of LoadKernelLibrary?
            return FALSE;
        }
        break;

    case DLL_PROCESS_DETACH:
        if (s_fInit) {
            s_fInit = FALSE;
            return CleanupLibrary ();
        }
        break;
    }
    
    return TRUE;
}
