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
*initcon.c - direct console I/O initialization and termination for Win32
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines __initconin() and _initconout() and __termcon() routines.
*       The first two are called on demand to initialize _coninpfh and
*       _confh, and the third is called indirectly by CRTL termination.
*
*       NOTE:   The __termcon() routine is called indirectly by the C/C++
*               Run-Time Library termination code.
*
*Revision History:
*       07-26-91  GJF   Module created. Based on the original code by Stevewo
*                       (which was distributed across several sources).
*       03-12-92  SKS   Split out initializer
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       10-28-93  GJF   Define entries for initialization and termination
*                       sections (used to be i386\cinitcon.asm).
*       04-12-94  GJF   Made _initcon() and _termcon() into empty functions
*                       for the Win32s version of msvcrt*.dll.
*       12-08-95  SKS   Replaced __initcon() with __initconin()/__initconout().
*                       _confh and _coninfh are no longer initialized during
*                       CRTL start-up but rather on demand in _getch(),
*                       _putch(), _cgets(), _cputs(), and _kbhit().
*       07-08-96  GJF   Removed Win32s support.Also, detab-ed.
*       02-07-98  GJF   Changes for Win64: _coninph and _confh are now an 
*                       intptr_t.
*       04-28-99  PML   Wrap __declspec(allocate()) in _CRTALLOC macro.
*       02-11-05  AC    Remove redundant #pragma section
*                       VSW#445138
*
*******************************************************************************/

#ifdef _WIN32_WCE
#ifndef _UNICODE   /* CRT flag */
#define _UNICODE 1
#endif

#ifndef UNICODE	   /* OS flag */
#define UNICODE 1
#endif
#endif // _WIN32_WCE

#include <sect_attribs.h>
#include <cruntime.h>
#include <internal.h>
#include <oscalls.h>

void __cdecl __termconout(void);

_CRTALLOC(".CRT$XPX") static  _PVFV pterm = __termconout;

/*
 * define console handles. these definitions cause this file to be linked
 * in if one of the direct console I/O functions is referenced.
 * The value (-2) is used to indicate the un-initialized state.
 */
intptr_t _confh = -2;       /* console output */


/***
*void __initconout(void) - open handles for console output
*
*Purpose:
*       Opens handle for console output.
*
*Entry:
*       None.
*
*Exit:
*       No return value. If successful, the handle value is copied into the
*       global variable _confh.  Otherwise _confh is set to -1.
*
*Exceptions:
*
*******************************************************************************/

void __cdecl __initconout (
        void
        )
{
    _confh = (intptr_t) CreateFileW(L"CONOUT$",
                                    GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL,
                                    OPEN_EXISTING,
                                    0,
                                    NULL);
}


/***
*void __termconout(void) - close console output handle
*
*Purpose:
*       Closes _confh.
*
*Entry:
*       None.
*
*Exit:
*       No return value.
*
*Exceptions:
*
*******************************************************************************/

void __cdecl __termconout (
        void
        )
{
    if ((_confh != -1) && (_confh != -2)) {
        CloseHandle((HANDLE) _confh);
    }
}
