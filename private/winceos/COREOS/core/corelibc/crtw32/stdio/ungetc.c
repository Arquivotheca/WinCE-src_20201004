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
*ungetc.c - unget a character from a stream
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines ungetc() - pushes a character back onto an input stream
*
*Revision History:
*       09-02-83  RN    initial version
*       04-16-87  JCR   added support for _IOUNGETC flag
*       08-04-87  JCR   (1) Added _IOSTRG check before setting _IOUNGETC flag.
*                       (2) Allow an ugnetc() before a read has occurred (get a
*                       buffer (ANSI).  [MSC only]
*       09-28-87  JCR   Corrected _iob2 indexing (now uses _iob_index() macro).
*       11-04-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-25-88  JCR   Allow an ungetc() before read for file opened "r+".
*       05-31-88  PHG   Merged DLL and normal versions
*       06-06-88  JCR   Optimized _iob2 references
*       06-15-88  JCR   Near reference to _iob[] entries; improve REG variables
*       08-25-88  GJF   Don't use FP_OFF() macro for the 386
*       04-11-89  JCR   Removed _IOUNGETC flag, fseek() no longer needs it
*       08-17-89  GJF   Clean up, now specific to OS/2 2.0 (i.e., 386 flat
*                       model). Also fixed copyright and indents.
*       02-16-90  GJF   Fixed copyright
*       03-20-90  GJF   Made calling type _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       07-23-90  SBM   Replaced <assertm.h> by <assert.h>
*       08-13-90  SBM   Compiles cleanly with -W3
*       10-03-90  GJF   New-style function declarators.
*       11-07-92  SRW   Dont modify buffer if stream opened by sscanf
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-26-93  CFW   Wide char enable.
*       04-30-93  CFW   Remove wide char support to ungetwc.c.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       03-02-98  GJF   Exception-safe locking.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       12-02-03  SJ    Reroute Unicode I/O
*       07-19-04  AC    Added _SAFECRT_IMPL for safecrt.lib
*       07-30-04  AC    Removed _SAFECRT_IMPL #if and moved the ungetw_nolock code
*                       into ungetc_nolock.inl
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <file2.h>
#include <dbgint.h>
#include <internal.h>
#include <mtdll.h>

/***
*int ungetc(ch, stream) - put a character back onto a stream
*
*Purpose:
*       Guaranteed one char pushback on a stream as long as open for reading.
*       More than one char pushback in a row is not guaranteed, and will fail
*       if it follows an ungetc which pushed the first char in buffer. Failure
*       causes return of EOF.
*
*Entry:
*       char ch - character to push back
*       FILE *stream - stream to push character onto
*
*Exit:
*       returns ch
*       returns EOF if tried to push EOF, stream not opened for reading or
*       or if we have already ungetc'd back to beginning of buffer.
*
*Exceptions:
*
*******************************************************************************/

int __cdecl ungetc (
        int ch,
        FILE *stream
        )
{
        int retval;

        _VALIDATE_RETURN( (stream != NULL), EINVAL, EOF);
        _CHECK_IO_INIT(EOF);

        _lock_str(stream);

        __try {
                retval = _ungetc_nolock (ch, stream);
        }
        __finally {
                _unlock_str(stream);
        }

        return(retval);
}

#include <ungetc_nolock.inl>
