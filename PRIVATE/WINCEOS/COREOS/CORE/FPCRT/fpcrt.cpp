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

#include <windows.h>

extern "C" {

BOOL
WINAPI
DllMain
    (
    HANDLE hdll,
    DWORD fdwReason,
    LPVOID
    )
{
    switch (fdwReason)
        {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls((HMODULE)hdll);
            break;

        case DLL_PROCESS_DETACH:
            break;

        default:
            break;
        }
    return TRUE;
}

/*
 * References to RaiseException need to be redirected to COREDLL
 */
VOID
__crtRaiseException(
    DWORD dwExceptionCode,
    DWORD dwExceptionFlags,
    DWORD nNumberOfArguments,
    CONST DWORD *lpArguments
    )
{
    RaiseException(dwExceptionCode, dwExceptionFlags, nNumberOfArguments, lpArguments);
}

} // extern "C"

