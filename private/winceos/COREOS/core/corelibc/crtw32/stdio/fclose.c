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
*fclose.c - close a file
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines fclose() - close an open file
*
*Revision History:
*       09-02-83  RN    initial version
*       05-14-87  SKS   error return from fflush must not be clobbered
*       08-10-87  JCR   Added code to support P_tmpdir with or without trailing '\'
*       11-01-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       01-13-88  JCR   Removed unnecessary calls to mthread fileno/feof/ferror
*       05-31-88  PHG   Merged DLL and normal versions
*       06-14-88  JCR   Use near pointer to reference _iob[] entries
*       08-24-88  GJF   Don't use FP_OFF() macro for the 386
*       08-17-89  GJF   Clean up, now specific to OS/2 2.0 (i.e., 386 flat
*                       model). Also fixed copyright and indents.
*       02-15-90  GJF   Fixed copyright
*       03-16-90  GJF   Made calling type _CALLTYPE1, added #include
*                       <cruntime.h> and removed #include <register.h>.
*       05-29-90  SBM   Use _flush, not [_]fflush[_lk]
*       07-25-90  SBM   Replaced <assertm.h> by <assert.h>
*       10-02-90  GJF   New-style function declarator.
*       01-21-91  GJF   ANSI naming.
*       03-11-92  GJF   For Win32, revised temporary file cleanup.
*       03-25-92  DJM   POSIX support.
*       08-26-92  GJF   Include unistd.h for POSIX build.
*       01-13-93  GJF   Don't need/want to remove() tmp files on Windows NT
*                       (file is removed by the OS when handle is closed).
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-06-94  CFW   Remove Cruiser support.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       01-10-95  CFW   Debug CRT allocs.
*       02-06-94  CFW   assert -> _ASSERTE.
*       02-17-95  GJF   Merged in Mac version. Removed some useless #ifdef-s.
*       03-07-95  GJF   _[un]lock_str macros now take FILE * arg.
*       05-12-95  CFW   Paranoia: set _tmpfname field to NULL.
*       02-25-98  GJF   Exception-safe locking.
*       05-17-99  PML   Remove all Macintosh support.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*
*******************************************************************************/

#include <cruntime.h>
#ifdef  _POSIX_
#include <unistd.h>
#endif
#include <stdio.h>
#include <file2.h>
#include <string.h>
#include <io.h>
#include <stdlib.h>
#include <internal.h>
#include <mtdll.h>
#include <dbgint.h>


/***
*int fclose(stream) - close a stream
*
*Purpose:
*       Flushes and closes a stream and frees any buffer associated
*       with that stream, unless it was set with setbuf.
*
*Entry:
*       FILE *stream - stream to close
*
*Exit:
*       returns 0 if OK, EOF if fails (can't _flush, not a FILE, not open, etc.)
*       closes the file -- affecting FILE structure
*
*Exceptions:
*
*******************************************************************************/

int __cdecl fclose (
        FILE *stream
        )
{
    int result = EOF;

    _VALIDATE_RETURN((stream != NULL), EINVAL, EOF);

    /* If stream is a string, simply clear flag and return EOF */
    if (stream->_flag & _IOSTRG)
        stream->_flag = 0;  /* IS THIS REALLY NEEDED ??? */

    /* Stream is a real file. */
    else {
        _lock_str(stream);
        __try {
            result = _fclose_nolock(stream);
        }
        __finally {
            _unlock_str(stream);
        }
    }

    return(result);
}

/***
*int _fclose_nolock() - close a stream (lock already held)
*
*Purpose:
*       Core fclose() routine; assumes caller has stream lock held.
*
*       [See fclose() above for more information.]
*
*Entry:
*
*Exit:
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _fclose_nolock (
        FILE *str
        )
{
        FILE *stream;
        int result = EOF;

        _VALIDATE_RETURN((str != NULL), EINVAL, EOF);
        
        /* Init near stream pointer */
        stream = str;

        if (inuse(stream)) {

                /* Stream is in use:
                       (1) flush stream
                       (2) free the buffer
                       (3) close the file
                       (4) delete the file if temporary
                */

                result = _flush(stream);
                _freebuf(stream);

#ifdef _POSIX_
                if (close(fileno(stream)) <0)
#else
                if (_close(_fileno(stream)) < 0)
#endif
                        result = EOF;

                else if ( stream->_tmpfname != NULL ) {
                        /*
                         * temporary file (i.e., one created by tmpfile()
                         * call). delete, if necessary (don't have to on
                         * Windows NT because it was done by the system when
                         * the handle was closed). also, free up the heap
                         * block holding the pathname.
                         */
#ifdef _POSIX_
                        if ( unlink(stream->_tmpfname) )
                                result = EOF;
#endif

                        _free_crt(stream->_tmpfname);
                stream->_tmpfname = NULL;
                }

        }

        stream->_flag = 0;
        return(result);
}
