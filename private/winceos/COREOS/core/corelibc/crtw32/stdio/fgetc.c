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
*fgetc.c - get a character from a stream
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines fgetc() and getc() - read  a character from a stream
*
*Revision History:
*       09-01-83  RN    initial version
*       11-09-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-31-88  PHG   Merged DLL and normal versions
*       06-21-89  PHG   Added getc() function
*       02-15-90  GJF   Fixed copyright and indents
*       03-16-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       03-16-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*       07-24-90  SBM   Replaced <assertm.h> by <assert.h>
*       10-02-90  GJF   New-style function declarators.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-26-93  CFW   Wide char enable.
*       04-30-93  CFW   Remove wide char support to fgetwc.c.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       07-20-97  GJF   Made getc() identical to fgetc(). Also, detab-ed.
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
*int fgetc(stream), getc(stream) - read a character from a stream
*
*Purpose:
*       reads a character from the given stream
*
*Entry:
*       FILE *stream - stream to read character from
*
*Exit:
*       returns the character read
*       returns EOF if at end of file or error occurred
*
*Exceptions:
*
*******************************************************************************/

int __cdecl fgetc (
        FILE *stream
        )
{
    int retval=0;

    _VALIDATE_RETURN( (stream != NULL), EINVAL, EOF);
    _CHECK_IO_INIT(EOF);

    _lock_str(stream);
    __try {
        _VALIDATE_STREAM_ANSI_SETRET(stream, EINVAL, retval, EOF);

        if(retval==0)
        {
            retval = _getc_nolock(stream);
        }
    }
    __finally {
        _unlock_str(stream);
    }

    return(retval);
}

#undef getc

int __cdecl getc (
        FILE *stream
        )
{
    int retval=0;

    _VALIDATE_RETURN( (stream != NULL), EINVAL, EOF);
    _CHECK_IO_INIT(EOF);

    _lock_str(stream);
    __try {
        _VALIDATE_STREAM_ANSI_SETRET(stream, EINVAL, retval, EOF);

        if(retval==0)
        {
            retval = _getc_nolock(stream);
        }
    }
    __finally {
        _unlock_str(stream);
    }

    return(retval);
}


_CRTIMP int (__cdecl _getc_nolock)(
        FILE *stream
        )
{
    return _getc_nolock(stream);
}
