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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "kernel.h"


static FARPROC pfnLocalALloc;
static FARPROC pfnLocalReAlloc;
static FARPROC pfnLocalSize;
static FARPROC pfnLocalFree;

static HLOCAL SanitizeReturn (HLOCAL hRet, HLOCAL hErrRtn)
{
    if ((DWORD) hRet >= VM_SHARED_HEAP_BASE) {
        // user tampered with LocalXXX to return a kernel address.
        DEBUGCHK (0);
        SetLastError (ERROR_INVALID_PARAMETER);
        hRet = hErrRtn;
    }
    return hRet;
}

HLOCAL NKRemoteLocalAlloc(UINT uFlags, UINT uBytes)
{
    HLOCAL hRet = NULL;
    
    if (pfnLocalALloc && (pVMProc != g_pprcNK)) {
        CALLBACKINFO cbi = {
            (HANDLE) pVMProc->dwId,
            pfnLocalALloc,
            (LPVOID) uFlags,
            0,
        };

        __try {
            hRet = SanitizeReturn ((HLOCAL) NKPerformCallBack (&cbi, uBytes), NULL);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_INVALID_PARAMETER);
        }
    }
    return hRet;
}


HLOCAL NKRemoteLocalReAlloc(HLOCAL hMem, UINT uBytes, UINT uFlags)
{
    HLOCAL hRet = NULL;
    
    if (pfnLocalReAlloc && (pVMProc != g_pprcNK)) {
        CALLBACKINFO cbi = {
            (HANDLE) pVMProc->dwId,
            pfnLocalReAlloc,
            hMem,
            0,
        };
        __try {
            hRet = SanitizeReturn ((HLOCAL) NKPerformCallBack (&cbi, uBytes, uFlags), NULL);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_INVALID_PARAMETER);
        }
    }
    return hRet;
}

UINT NKRemoteLocalSize(HLOCAL hMem)
{
    UINT uRet = 0;

    if (pfnLocalSize && (pVMProc != g_pprcNK)) {
        CALLBACKINFO cbi = {
            (HANDLE) pVMProc->dwId,
            pfnLocalSize,
            hMem,
            0,
        };
        __try {
            uRet = NKPerformCallBack (&cbi);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_INVALID_PARAMETER);
        }
    }

    return uRet;
}

HLOCAL NKRemoteLocalFree(HLOCAL hMem)
{
    HLOCAL hRet = hMem;
    
    if (pfnLocalFree && (pVMProc != g_pprcNK)) {
        CALLBACKINFO cbi = {
            (HANDLE) pVMProc->dwId,
            pfnLocalFree,
            hMem,
            (DWORD) hMem,
        };
        __try {
            hRet = SanitizeReturn ((HLOCAL) NKPerformCallBack (&cbi), hMem);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            SetLastError (ERROR_INVALID_PARAMETER);
        }
    }
    return hRet;
}

BOOL RemoteUserHeapInit (HANDLE hCoreDll)
{
    pfnLocalALloc   = GetProcAddressA (hCoreDll, (LPCSTR) 33);
    pfnLocalReAlloc = GetProcAddressA (hCoreDll, (LPCSTR) 34);
    pfnLocalSize    = GetProcAddressA (hCoreDll, (LPCSTR) 35);
    pfnLocalFree    = GetProcAddressA (hCoreDll, (LPCSTR) 36);

    return pfnLocalALloc && pfnLocalReAlloc && pfnLocalSize && pfnLocalFree;
}
