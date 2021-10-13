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
#include <corecrt.h>

BOOL WINAPI DllEntry(HANDLE hDllHandle, DWORD dwReason, LPVOID lpReserved);

BOOL WINAPI _DllEntryCRTStartup(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved)
{
    BOOL retcode = TRUE;

    if (dwReason == DLL_PROCESS_ATTACH)
    {
        __security_init_cookie();
        _cinit();
    }

    retcode = DllEntry(hDllHandle, dwReason, lpreserved);

    if (dwReason == DLL_PROCESS_DETACH)
    {
        _cexit();
    }

    return retcode;
}

