//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

/***
*seccgen.c - generate a value for the global buffer overrun security cookie
*
*Purpose:
*       Define __security_gen_cookie and __security_error_handler, which
*       are routines required to support the compiler's runtime security error
*       checks, enabled with the /GS flag.  These routines are coredll-specific.
*       Modules linking with fulllibc will need to implement both of these.
*
*Entrypoints:
*       __security_gen_cookie
*       __security_error_handler
*
*******************************************************************************/

#include <corecrt.h>
#include <windows.h>
#include <stdlib.h>


/***
*__security_gen_cookie() - init buffer overrun security cookie.
*
*Purpose:
*       Generate a value for the global buffer overrun security cookie, which
*       is used by the /GS compile switch to detect overwrites to local array
*       variables the potentially corrupt the return address.  This routine is
*       called at EXE/DLL startup by the static __security_init_cookie.
*
*Entry:
*
*Exit:
*       returns the generated value
*
*Exceptions:
*
*******************************************************************************/

DWORD_PTR __cdecl __security_gen_cookie(void)
{
    DWORD_PTR cookie;
    FILETIME ft;
    LARGE_INTEGER perfctr;

    /*
     * Initialize the global cookie with an unpredictable value which is
     * different for each module in a process.  Combine a number of sources
     * of randomness.
     */

    GetCurrentFT(&ft);

    cookie  = ft.dwLowDateTime;
    cookie ^= ft.dwHighDateTime;

    cookie ^= GetCurrentProcessId();
    cookie ^= GetCurrentThreadId();
    cookie ^= GetTickCount();

    QueryPerformanceCounter(&perfctr);
    cookie ^= perfctr.LowPart;
    cookie ^= perfctr.HighPart;

    return cookie;
}


/***
*__security_error_handler() - Report security error.
*
*Purpose:
*       A /GS security error has been detected.  Bring up a default
*       message box describing the problem.
*
*Entry:
*       int code - security failure code
*       void *data - code-specific data
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

void __cdecl __security_error_handler
(
    int code,
    void* data
)
{
    UINT idMsg;
    DWORD dwExitCode = (DWORD)-1;

    BOOL IsAPIReady(DWORD hAPI);

    switch (code) {
    default:
        /*
         * Unknown failure code, which probably means an old CRT is
         * being used with a new compiler.
         */
        idMsg = 3; 
        break;
    case _SECERR_BUFFER_OVERRUN:
        /*
         * Buffer overrun detected which may have overwritten a return
         * address.
         */
        idMsg = 4;
        dwExitCode = STATUS_STACK_BUFFER_OVERRUN;
        break;
    }

    if (!IsAPIReady(SH_WMGR))
    {
        RETAILMSG(1,(L"Security check (%d) failed in proc %8.8lx, WMGR not on line!\r\n", code, GetCurrentProcessId()));
    }
    else
    {
        extern HINSTANCE hInstCoreDll;

        LPCWSTR pstr = (LPCWSTR)LoadString(hInstCoreDll, idMsg, 0, 0);

        if (pstr)
        {
            LPCWSTR pname = GetProcName();
            WCHAR bufx[512];
            _snwprintf(bufx, sizeof(bufx)/sizeof(bufx[0]) - 1, pstr, pname);
            bufx[sizeof(bufx)/sizeof(bufx[0]) - 1] = 0;
            pstr = (LPCWSTR)LoadString(hInstCoreDll, 2, 0, 0);
            if (pstr)
            {
                MessageBox(0, bufx, pstr, MB_OK|MB_ICONEXCLAMATION|MB_TOPMOST);
            }
        }
        if (!pstr)
        {
            RETAILMSG (1, (L"Security check failed in proc %8.8lx, unable to load strings!\r\n", GetCurrentProcessId()));
        }
    }

    ExitProcess(dwExitCode);
}

