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
*fdopen.c - open a file descriptor as stream
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _fdopen() - opens a file descriptor as a stream, thus allowing
*       buffering, etc.
*
*Revision History:
*       09-02-83  RN    initial version
*       03-02-87  JCR   added support for 'b' and 't' embedded in mode strings
*       09-28-87  JCR   Corrected _iob2 indexing (now uses _iob_index() macro).
*       11-03-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-31-88  PHG   Merged DLL and normal versions
*       06-06-88  JCR   Optimized _iob2 references
*       11-20-89  GJF   Fixed copyright, indents. Added const to type of mode.
*       02-15-90  GJF   _iob[], _iob2[] merge.
*       03-16-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       07-23-90  SBM   Replaced <assertm.h> by <assert.h>
*       08-24-90  SBM   Added support for 'c' and 'n' flags
*       10-02-90  GJF   New-style function declarator.
*       01-21-91  GJF   ANSI naming.
*       02-14-92  GJF   Replaced _nfile with _nhandle for Win32.
*       05-01-92  DJM   Replaced _nfile with OPEN_MAX for POSIX.
*       08-03-92  GJF   Function name must be "fdopen" for POSIX.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       12-07-93  CFW   Wide char enable.
*       04-05-94  GJF   #ifdef-ed out _cflush reference for msvcrt*.dll, it
*                       is unnecessary.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       01-04-95  GJF   _WIN32_ -> _WIN32.
*       02-06-94  CFW   assert -> _ASSERTE.
*       02-17-95  GJF   Merged in Mac version.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       10-20-95  GJF   Added checks to passed supplied handle is open (for
*                       Win32 and Mac builds) (Olympus0 10153).
*       09-26-97  BWT   Fix POSIX
*       02-26-98  GJF   Exception-safe locking.
*       05-17-99  PML   Remove all Macintosh support.
*       10-06-99  PML   Set errno EMFILE when out of streams.
*       04-26-02  PML   Fix AV on bogus mode param (VS7#526205)
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       11-13-03  SJ    File Handle checks made consistent - VSW#199372
*       09-03-04  AC    Tolerate spaces in mode string
*                       VSW#330884
*       10-31-04  AC    Prefast fixes
*                       VSW#373224
*
*******************************************************************************/

#include <cruntime.h>
#include <msdos.h>
#include <stdio.h>
#include <file2.h>
#include <dbgint.h>
#include <internal.h>
#include <mtdll.h>
#ifdef  _POSIX_
#include <limits.h>
#endif
#include <tchar.h>
#include <errno.h>

/***
*FILE *_fdopen(filedes, mode) - open a file descriptor as a stream
*
*Purpose:
*       associates a stream with a file handle, thus allowing buffering, etc.
*       The mode must be specified and must be compatible with the mode
*       the file was opened with in the low level open.
*
*Entry:
*       int filedes - handle referring to open file
*       _TSCHAR *mode - file mode to use ("r", "w", "a", etc.)
*
*Exit:
*       returns stream pointer and sets FILE struct fields if successful
*       returns NULL if fails
*
*Exceptions:
*
*******************************************************************************/

FILE * __cdecl _tfdopen (
        int filedes,
        const _TSCHAR *mode
        )
{
        FILE *stream;
        int whileflag, tbflag, cnflag;
        int fileflag;

        _VALIDATE_RETURN(mode != NULL, EINVAL, NULL);

#if     !defined(_POSIX_)

        _CHECK_FH_RETURN( filedes, EBADF, NULL );
        _CHECK_IO_INIT(NULL);
        _VALIDATE_RETURN((filedes >= 0 && (unsigned)filedes < (unsigned)_nhandle), EBADF, NULL);
        _VALIDATE_RETURN((_osfile(filedes) & FOPEN), EBADF, NULL);

#else

        _ASSERTE((unsigned)filedes < OPEN_MAX);

        if ((unsigned)filedes >= OPEN_MAX)
            return(NULL);

#endif  /* !_POSIX_ */

        /* Skip leading spaces */
        while (*mode == _T(' '))
        {
            ++mode;
        }

        /* First character must be 'r', 'w', or 'a'. */
        switch (*mode) {
            case _T('r'):
                fileflag = _IOREAD;
                break;
            case _T('w'):
            case _T('a'):
                fileflag = _IOWRT;
                break;
            default:
                _VALIDATE_RETURN(("Invalid file open mode",0), EINVAL, NULL);
        }

        /* There can be up to three more optional characters:
           (1) A single '+' character,
           (2) One of 'b' and 't' and
           (3) One of 'c' and 'n'.

           Note that currently, the 't' and 'b' flags are syntax checked
           but ignored.  'c' and 'n', however, are correctly supported.
        */

        whileflag=1;
        tbflag=cnflag=0;
        fileflag |= _commode;

        while(*++mode && whileflag)
            switch(*mode) {

                case _T(' '):
                    /* skip spaces */
                    break;

                case _T('+'):
                    if (fileflag & _IORW)
                        whileflag=0;
                    else {
                        fileflag |= _IORW;
                        fileflag &= ~(_IOREAD | _IOWRT);
                    }
                    break;

                case _T('b'):
                case _T('t'):
                    if (tbflag)
                        whileflag=0;
                    else
                        tbflag=1;
                    break;

                case _T('c'):
                    if (cnflag)
                        whileflag = 0;
                    else {
                        cnflag = 1;
                        fileflag |= _IOCOMMIT;
                    }
                    break;

                case _T('n'):
                    if (cnflag)
                        whileflag = 0;
                    else {
                        cnflag = 1;
                        fileflag &= ~_IOCOMMIT;
                    }
                    break;

                default:
                    _VALIDATE_RETURN(("Invalid file open mode",0), EINVAL, NULL);                            
            }

        /* Skip trailing spaces */
        while (*mode == _T(' '))
        {
            ++mode;
        }

        _VALIDATE_RETURN( (*mode == _T('\0')), EINVAL, NULL);

        /* Find a free stream; stream is returned 'locked'. */

        if ((stream = _getstream()) == NULL) {
            errno = EMFILE;
            return(NULL);
        }

        __try {
#ifndef CRTDLL
            _cflush++;  /* force library pre-termination procedure */
#endif  /* CRTDLL */

            stream->_flag = fileflag;
            stream->_file = filedes;
        }
        __finally {
            _unlock_str(stream);
        }

        return(stream);
}
