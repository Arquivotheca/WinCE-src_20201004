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

BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpReserved);

// Because it is not explicly initialized, this variable will be initialized to
// zero if it is not defined anywhere else.  If it is defined, the other
// definition will supercede it.
BOOL (WINAPI *_pRawDllMain)(HANDLE, DWORD, LPVOID);

/***
*BOOL WINAPI _DllMainCRTStartup(hDllHandle, dwReason, lpreserved) -
*       C Run-Time initialization for a DLL linked with a C run-time library.
*
*Purpose:
*       This routine does the C run-time initialization or termination
*       and then calls the user code notification handler "DllMain".
*       For the multi-threaded run-time library, it also cleans up the
*       multi-threading locks on DLL termination.
*
*Entry:
*
*Exit:
*
*NOTES:
*       This routine is the preferred entry point of a DLL.
*
*******************************************************************************/

__declspec(noinline)
static
BOOL __cdecl
_DllMainCRTStartupHelper
    (
    HANDLE  hDllHandle,
    DWORD   dwReason,
    LPVOID  lpreserved
    )
    {
    BOOL retcode = TRUE;

    __try
        {
        if (dwReason == DLL_PROCESS_ATTACH)
            {
            if (_pRawDllMain)
                {
                retcode = (*_pRawDllMain)(hDllHandle, dwReason, lpreserved);
                }

            if (retcode)
                {
                _cinit();
                }
            }

        if (retcode)
            {
            retcode = DllMain(hDllHandle, dwReason, lpreserved);
            }

        if (dwReason == DLL_PROCESS_DETACH)
            {
            _cexit();

            if (retcode && _pRawDllMain)
                {
                retcode = (*_pRawDllMain)(hDllHandle, dwReason, lpreserved);
                }
            }
        }
    __except (_XcptFilter(GetExceptionCode(), GetExceptionInformation()))
        {
        retcode = FALSE;
        }

    return retcode;
}


BOOL WINAPI
_DllMainCRTStartup(
        HANDLE  hDllHandle,
        DWORD   dwReason,
        LPVOID  lpreserved
        )
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        /*
         * The /GS security cookie must be initialized before any exception
         * handling targeting the current image is registered.  No function
         * using exception handling can be called in the current image until
         * after __security_init_cookie has been called.
         */
        __security_init_cookie();
    }

    return _DllMainCRTStartupHelper(hDllHandle, dwReason, lpreserved);
}

