//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/***
*crtlib.c - CRT DLL initialization and termination routine (Win32, Dosx32)
*
*
*Purpose:
*       Init code for all the STDIO functionality in CE
*
*******************************************************************************/

#include <cruntime.h>
#include <crtmisc.h>
#include <internal.h>
#include <malloc.h>
#include <mtdll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <crttchar.h>
#include <dbgint.h>
#include <msdos.h>
#include <coredll.h>

// Global static data
// CESYSGEN: These should be pulled in always
// This is the ONLY lock inited in DllMain. It is used to protect 
// initialization of all the other globals below (incl the other locks)
// The fStdioInited flag tells us if init has been done
CRITICAL_SECTION csStdioInitLock;
int fStdioInited;
// Pointer to init/term fns. This aids us in componentization.
FARPROC pfnInitStdio;
FARPROC pfnTermStdio;

// Function ptrs used to componentize Str-only from Stdio
PFNFILBUF _pfnfilbuf2;
PFNFLSBUF _pfnflsbuf2;

/***
*BOOL _CRTDLL_INIT(hDllHandle, dwReason, lpreserved) - C DLL initialization.
*   The only thing we do here is init ONE critsec
*******************************************************************************/

BOOL WINAPI _CRTDLL_INIT(
        HANDLE  hDllHandle,
        DWORD   dwReason,
        LPVOID  lpreserved
        )
{
    // Only get PROCESS_ATTACH and PROCESS_DETACH since CoreDllInit in
    // coredll.c calls DisableThreadLibraryCalls().
    if(dwReason == DLL_PROCESS_ATTACH) 
    {
        InitializeCriticalSection(&csStdioInitLock);
        DEBUGMSG (DBGSTDIO, (TEXT("Stdio: DllInit attach\r\n")));
        fStdioInited = FALSE;
        pfnInitStdio = NULL;
        pfnTermStdio = NULL;
    }
    else if ( dwReason == DLL_PROCESS_DETACH ) 
    {
        DEBUGMSG (DBGSTDIO, (TEXT("Stdio: DllInit dettach\r\n")));
        if(fStdioInited && pfnTermStdio)
        {
            DEBUGMSG (DBGSTDIO, (TEXT("Stdio: Calling STDIO Deinit\r\n")));
            pfnTermStdio();
        }
        DeleteCriticalSection(&csStdioInitLock);
    }
    return TRUE;
}

int __cdecl InitStdio(void)
{
    BOOL fRet = TRUE;
    EnterCriticalSection(&csStdioInitLock);
    
    if(fStdioInited) goto done;

    // try to get the real Init function
    if(!pfnInitStdio)
        pfnInitStdio = GetProcAddress(hInstCoreDll, (LPWSTR)1151); // ordinal must match COREDLL.DEF

    // call it if it exists
    if(pfnInitStdio)
    {
        fRet = pfnInitStdio();
    }
    else
    {
        DEBUGMSG (DBGSTDIO, (TEXT("Stdio: being INITED for STRING fns only\r\n")));
    }
    fStdioInited = fRet; // inited to TRUE for str-only case too

done:
    LeaveCriticalSection(&csStdioInitLock);
    return fRet;
}

