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
*eof.c - test a handle for end of file
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _eof() - determine if a file is at eof
*
*Revision History:
*       09-07-83  RN    initial version
*       10-28-87  JCR   Multi-thread support
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-25-88  PHG   DLL replaces normal version
*       07-11-88  JCR   Added REG allocation to declarations
*       03-12-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h>, removed #include <register.h> and fixed
*                       the copyright. Also, cleaned up the formatting a bit.
*       09-28-90  GJF   New-style function declarator.
*       12-04-90  GJF   Improved range check of file handle.
*       01-16-91  GJF   ANSI naming.
*       02-13-92  GJF   Replaced _nfile by _nhandle for Win32.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       01-04-95  GJF   _WIN32_ -> _WIN32
*       02-15-95  GJF   Appended Mac version of source file (somewhat cleaned
*                       up), with appropriate #ifdef-s.
*       06-27-95  GJF   Added check that the file handle is open.
*       07-08-96  GJF   Replaced defined(_WIN32) with !defined(_MAC), and
*                       defined(_M_M68K) || defined(_M_MPPC) with 
*                       defined(_MAC). Removed obsolete REG* macros. Also, 
*                       detab-ed and cleaned up the format a bit.
*       12-17-97  GJF   Exception-safe locking.
*       09-23-98  GJF   Use __lseeki64_lk so _eof works on BIG files
*       05-17-99  PML   Remove all Macintosh support.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       11-13-03  SJ    File Handle checks made consistent - VSW#199372
*       10-31-04  AC    Prefast fixes
*                       VSW#373224
*
*******************************************************************************/

#include <cruntime.h>
#include <io.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <internal.h>
#include <msdos.h>
#include <mtdll.h>

/***
*int _eof(filedes) - test a file for eof
*
*Purpose:
*       see if the file length is the same as the present position. if so, return
*       1. if not, return 0. if an error occurs, return -1
*
*Entry:
*       int filedes - handle of file to test
*
*Exit:
*       returns 1 if at eof
*       returns 0 if not at eof
*       returns -1 and sets errno if fails
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _eof (
        int filedes
        )
{
        __int64 here;
        __int64 end;
        int retval;

        _CHECK_FH_CLEAR_OSSERR_RETURN( filedes, EBADF, -1 );
        _CHECK_IO_INIT(-1);
        _VALIDATE_CLEAR_OSSERR_RETURN((filedes >= 0 && (unsigned)filedes < (unsigned)_nhandle), EBADF, -1);
        _VALIDATE_CLEAR_OSSERR_RETURN((_osfile(filedes) & FOPEN), EBADF, -1);

        /* Lock the file */
        _lock_fh(filedes);
        __try {
            if ( _osfile(filedes) & FOPEN ) {
				/* See if the current position equals the end of the file. */

				if ( ((here = _lseeki64_nolock(filedes, 0i64, SEEK_CUR)) == -1i64) || 
					 ((end = _lseeki64_nolock(filedes, 0i64, SEEK_END)) == -1i64) )
						retval = -1;
				else if ( here == end )
						retval = 1;
				else {
						_lseeki64_nolock(filedes, here, SEEK_SET);
						retval = 0;
				}
            }
            else {
                    errno = EBADF;
                    _doserrno = 0;
                    retval = -1;
                    _ASSERTE(("Invalid file descriptor. File possibly closed by a different thread",0));
            }
        }
        __finally {
                /* Unlock the file */
                _unlock_fh(filedes);
        }

        /* Done */
        return(retval);
}
