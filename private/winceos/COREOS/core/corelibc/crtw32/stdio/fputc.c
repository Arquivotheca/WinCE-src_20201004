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
*fputc.c - write a character to an output stream
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines fputc() - writes a character to a stream
*       defines fputwc() - writes a wide character to a stream
*
*Revision History:
*       09-01-83  RN    initial version
*       11-09-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-27-88  PHG   Merged DLL and normal versions
*       06-14-88  JCR   Near reference to _iob[] entries; improve REG variables
*       08-25-88  GJF   Don't use FP_OFF() macro for the 386
*       06-21-89  PHG   Added putc() function
*       08-28-89  JCR   Removed _NEAR_ for 386
*       02-15-90  GJF   Fixed copyright and indents.
*       03-19-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>. Also,
*                       removed some leftover 16-bit support.
*       07-24-90  SBM   Replaced <assertm.h> by <assert.h>
*       10-02-90  GJF   New-style function declarators.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-26-93  CFW   Wide char enable.
*       04-30-93  CFW   Remove wide char support to fputwc.c.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       02-20-97  GJF   Removed unnecessary local from fputc(). Made putc() 
*                       identical to fputc(). Also, detab-ed.
*       02-27-98  GJF   Exception-safe locking.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       12-02-03  SJ    Reroute Unicode I/O
*       03-13-04  MSL   Avoid returning from __try for prefast
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <file2.h>
#include <internal.h>
#include <mtdll.h>

/***
*int fputc(ch, stream) - write a character to a stream
*
*Purpose:
*       Writes a character to a stream.  Function version of putc().
*
*Entry:
*       int ch - character to write
*       FILE *stream - stream to write to
*
*Exit:
*       returns the character if successful
*       returns EOF if fails
*
*Exceptions:
*
*******************************************************************************/

int __cdecl fputc (
        int ch,
        FILE *str
        )
{
    int retval=0;

    _VALIDATE_RETURN((str != NULL), EINVAL, EOF);
    _CHECK_IO_INIT(EOF);

    _lock_str(str);
    __try {
        _VALIDATE_STREAM_ANSI_SETRET(str, EINVAL, retval, EOF);

        if (retval==0)
        {
            retval = _putc_nolock(ch,str);
        }
    }
    __finally {
        _unlock_str(str);
    }

    return(retval);
}

#undef putc

int __cdecl putc (
        int ch,
        FILE *str
        )
{
    int retval=0;

    _VALIDATE_RETURN((str != NULL), EINVAL, EOF);
    _CHECK_IO_INIT(EOF);

    _lock_str(str);
    __try {
        _VALIDATE_STREAM_ANSI_SETRET(("Invalid ANSI I/O on unicode stream", str), EINVAL, retval, EOF);

        if (retval==0)
        {
            retval = _putc_nolock(ch,str);
        }
    }
    __finally {
        _unlock_str(str);
    }

    return(retval);
}
