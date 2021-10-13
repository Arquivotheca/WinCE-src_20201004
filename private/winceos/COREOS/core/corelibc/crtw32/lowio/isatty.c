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
*isatty.c - check if file handle refers to a device
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _isatty() - check if file handle refers to a device
*
*Revision History:
*       06-08-89  PHG   Module created
*       03-12-90  GJF   Made calling type _CALLTYPE1, added #include
*                       <cruntime.h> and fixed the copyright. Also, cleaned
*                       up the formatting a bit.
*       08-14-90  SBM   Compiles cleanly with -W3, minor optimization
*       09-28-90  GJF   New-style function declarator.
*       12-04-90  GJF   Appended Win32 version onto the source with #ifdef-s.
*                       It's not worth it to try to merge the versions more
*                       closely.
*       12-06-90  SRW   Changed to use _osfile and _osfhnd instead of _osfinfo
*       01-16-91  GJF   ANSI naming.
*       02-13-92  GJF   Replaced _nfile by _nhandle for Win32.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       01-04-95  GJF   _WIN32_ -> _WIN32
*       06-06-95  GJF   Replaced _osfile[] with _osfile() (macro referencing
*                       field in ioinfo struct).
*       07-08-96  GJF   Replaced defined(_WIN32) with !defined(_MAC). Also,
*                       detab-ed.
*       05-17-99  PML   Remove all Macintosh support.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       11-13-03  SJ    File Handle checks made consistent - VSW#199372
*       10-31-04  AC    Prefast fixes
*                       VSW#373224
*       11-11-04  MR    Add support for special handles in C# - VSW#272079
*
*******************************************************************************/

#include <cruntime.h>
#include <msdos.h>
#include <internal.h>
#include <io.h>

/***
*int _isatty(handle) - check if handle is a device
*
*Purpose:
*       Checks if the given handle is associated with a character device
*       (terminal, console, printer, serial port)
*
*Entry:
*       int handle - handle of file to be tested
*
*Exit:
*       returns non-0 if handle refers to character device,
*       returns 0 otherwise
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _isatty (
        int fh
        )
{
        /* see if file handle is valid, otherwise return FALSE */
        _CHECK_FH_RETURN(fh, EBADF, 0);
        _CHECK_IO_INIT(-1);
        _VALIDATE_RETURN((fh >= 0 && (unsigned)fh < (unsigned)_nhandle), EBADF, 0);
        
        /* check file handle database to see if device bit set */
        return (int)(_osfile(fh) & FDEV);
}
