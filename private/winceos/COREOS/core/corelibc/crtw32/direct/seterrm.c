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
*seterrm.c - Set mode for handling critical errors
*
*   Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*   Defines signal() and raise().
*
*Revision History:
*   08-21-92  BWM   Wrote for Win32.
*   09-29-93  GJF   Resurrected for compatibility with NT SDK (which had
*           the function). Replaced _CALLTYPE1 with __cdecl and
*           removed Cruiser support.
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>

/***
*void _seterrormode(mode) - set the critical error mode
*
*Purpose:
*
*Entry:
*   int mode - error mode:
*
*       0 means system displays a prompt asking user how to
*       respond to the error. Choices differ depending on the
*       error but may include Abort, Retry, Ignore, and Fail.
*
*       1 means the call system call causing the error will fail
*       and return an error indicating the cause.
*
*Exit:
*   none
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _seterrormode(int mode)
{
#ifdef _WIN32_WCE // CE doesn't support SetErrorMode
        return;
#else       
    SetErrorMode(mode);
#endif // _WIN32_WCE
}
