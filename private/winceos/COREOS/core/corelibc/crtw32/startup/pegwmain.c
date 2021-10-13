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

#ifdef CRT_UNICODE
#define _tWinMain wWinMain
#define _tWinMainCRTStartup wWinMainCRTStartup
#else
#define _tWinMain WinMain
#define _tWinMainCRTStartup WinMainCRTStartup
#endif

BOOL
WINAPI
_tWinMain
    (
    HINSTANCE hInstance,
    HINSTANCE hInstancePrev,
    LPWSTR lpszCmdLine,
    int  nCmdShow
    );

/***
*WinMainCRTStartup(void)
*wWinMainCRTStartup(void)
*
*Purpose:
*       These routines do the C runtime initialization, call the appropriate
*       user entry function, and handle termination cleanup.  For an unmanaged
*       app, they call exit and never return.
*
*       Function:               User entry called:
*       WinMainCRTStartup       WinMain
*       wWinMainCRTStartup      wWinMain
*
*Entry:
*
*Exit:
*       Unmanaged app: never return.
*
*******************************************************************************/

__declspec(noinline)
static
void
WINAPI
WinMainCRTStartupHelper
    (
    HINSTANCE hInstance,
    HINSTANCE hInstancePrev,
    LPWSTR lpszCmdLine,
    int nCmdShow
    )
{
    int retcode = -3;

    __try
    {
        _cinit(); /* Initialize C's data structures */

        retcode = _tWinMain(hInstance, hInstancePrev, lpszCmdLine, nCmdShow);

        exit(retcode);
    }
    __except (_XcptFilter(retcode = GetExceptionCode(), GetExceptionInformation()))
    {
        /*
         * Should never reach here unless UnHandled Exception Filter does
         * not pass the exception to the debugger
         */
        _exit(retcode);
    } /* end of try - except */
}


void
WINAPI
_tWinMainCRTStartup
    (
    HINSTANCE hInstance,
    HINSTANCE hInstancePrev,
    LPWSTR lpszCmdLine,
    int nCmdShow
    )
{
    /*
     * The /GS security cookie must be initialized before any exception
     * handling targeting the current image is registered.  No function
     * using exception handling can be called in the current image until
     * after __security_init_cookie has been called.
     */
    __security_init_cookie();

    WinMainCRTStartupHelper(hInstance, hInstancePrev, lpszCmdLine, nCmdShow);
}

