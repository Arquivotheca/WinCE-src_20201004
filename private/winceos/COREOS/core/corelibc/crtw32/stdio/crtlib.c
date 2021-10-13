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
/***
*crtlib.c - CRT DLL initialization and termination routine (Win32, Dosx32)
*
*
*Purpose:
*       Init code for all the STDIO functionality in CE
*
*******************************************************************************/

#include <corecrt.h>
#include <internal.h>
#include <coredll.h>
#include <crt_ordinals.h>

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

BOOL g_fCrtLegacyInputValidation;

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
    if (dwReason == DLL_PROCESS_ATTACH)
        {
        _cinit(); // call static initializers
        InitializeCriticalSection(&csStdioInitLock);
        DEBUGMSG (DBGSTDIO, (TEXT("Stdio: DllInit attach\r\n")));
        fStdioInited = FALSE;
        pfnInitStdio = NULL;
        pfnTermStdio = NULL;
        }
    else if (dwReason == DLL_PROCESS_DETACH)
        {
        DEBUGMSG (DBGSTDIO, (TEXT("Stdio: DllInit dettach\r\n")));
        if(fStdioInited && pfnTermStdio)
            {
            DEBUGMSG (DBGSTDIO, (TEXT("Stdio: Calling STDIO Deinit\r\n")));
            pfnTermStdio();
            }
        DeleteCriticalSection(&csStdioInitLock);
        // don't call static terminators (_cexit) to avoid a reference to
        // TerminateProcess inside COREDLL.  These aren't used inside COREDLL
        // anyway.
        }

    return TRUE;
}

int __cdecl InitStdio(void)
{
    BOOL fRet = TRUE;

    EnterCriticalSection(&csStdioInitLock);

    if(fStdioInited) goto done;

    // try to get the real Init function
    if(!pfnInitStdio && hInstCoreDll)
        {
        pfnInitStdio = GetProcAddressA(hInstCoreDll, (LPCSTR)_ORDINAL__InitStdioLib);
        }

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

