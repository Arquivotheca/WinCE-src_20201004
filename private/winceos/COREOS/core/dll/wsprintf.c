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
*wsprintf.c - (Win32) print formatted data into a string
*
*Purpose:
*   defines wsprintfW() and wvsprintfW() - print formatted output to
*   a string, get the data from an argument ptr instead of explicit
*   arguments.
*
*******************************************************************************/

#include "windows.h"

/*++

Routine description:

    For the Win32 versions, reuse C Runtime code to save space even though it
    won't match the desktop.

Notes:

    As of WinXP SP2, the desktop version of this function will still
    null-terminate at the 1025th byte, so we are not compatible in
    that respect either.

--*/
#define MAX_W32_LEN 1023

#pragma warning(disable:4995) // disable deprecation warning for banned API
#pragma warning(disable:4996) // disable deprecation warning for _vsnwprintf

int __cdecl wvsprintfW(LPWSTR string, LPCWSTR format, va_list ap)
{
    int len = _vsnwprintf(string, MAX_W32_LEN, format, ap);

    // _vsnwprintf returns a negative number on error
    if (len < 0) {
        // it returns -1 if the buffer isn't big enough
        if (-1 == len) {
            len = MAX_W32_LEN;
        } else {
            len = 0;
        }
    }

    // null terminate the string
    string[len] = 0;

    return len;
}

int __cdecl wsprintfW(LPWSTR string, LPCWSTR format, ...)
{
    int len;
    va_list arglist;
    va_start(arglist, format);

    len = wvsprintfW(string, format, arglist);

    va_end(arglist);

    return len;
}


