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
*clearerr.c - clear error and eof flags
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines clearerr() - clear error and eof flags from a stream
*
*Revision History:
*       11-30-83  RN    initial version
*       11-02-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-31-88  PHG   Merged DLL and normal versions
*       06-01-88  JCR   Clear lowio flags as well as stdio flags
*       02-15-90  GJF   Fixed copyright and indents
*       03-16-90  GJF   Replaced _LOAD_DS with _CALLTYPE1 and added #include
*                       <cruntime.h>.
*       07-23-90  SBM   Replaced <assertm.h> by <assert.h>
*       10-02-90  GJF   New-style function declarator.
*       01-22-91  GJF   ANSI naming.
*       03-27-92  DJM   POSIX support
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       02-06-94  CFW   assert -> _ASSERTE.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       06-12-95  GJF   Replaced _osfile[] with _osfile() (macro referencing
*                       field in ioinfo struct).
*       07-28-95  GJF   Replaced _osfile() with _osfile_safe().
*       02-25-98  GJF   Exception-safe locking.
*       05-17-99  PML   Remove all Macintosh support.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-20-03  SJ    errno shouldn't be set to 0 on success
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <dbgint.h>
#include <file2.h>
#include <mtdll.h>
#include <internal.h>
#ifndef _POSIX_
#include <msdos.h>
#endif

/***
*errno_t clearerr(stream) - clear error and eof flags on a stream
*
*Purpose:
*       Resets the error and eof indicators for a stream to 0
*
*Entry:
*       FILE *stream - stream to set indicators on
*
*Exit:
*       0 on success, Otherwise error code
*
*Exceptions:
*
*******************************************************************************/

errno_t __cdecl clearerr_s (FILE * stream)        
{
        _VALIDATE_RETURN_ERRCODE((stream != NULL), EINVAL);
        _CHECK_IO_INIT(EINVAL);

        _lock_str(stream);
        __try {
            /* Clear stdio level flags */
            stream->_flag &= ~(_IOERR|_IOEOF);

            /* Clear lowio level flags */

#ifndef _POSIX_
            _osfile_safe(_fileno(stream)) &= ~(FEOFLAG);
#endif
        }
        __finally {
            _unlock_str(stream);
        }

    return 0;
}

/***
*void clearerr(stream) - clear error and eof flags on a stream
*
*Purpose:
*       Resets the error and eof indicators for a stream to 0
*
*Entry:
*       FILE *stream - stream to set indicators on
*
*Exit:
*       No return value.
*       changes the _flag field of the FILE struct.
*
*Exceptions:
*
*******************************************************************************/

void __cdecl clearerr (
        FILE *stream
        )
{
    clearerr_s(stream);
}
