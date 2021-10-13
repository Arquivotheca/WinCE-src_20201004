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
*fleni64.c - find length of a file
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _filelengthi64() - find the length of a file
*
*Revision History:
*       11-18-94  GJF   Created. Adapted from flength.c
*       06-27-95  GJF   Added check that the file handle is open.
*       12-19-97  GJF   Exception-safe locking.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       11-13-03  SJ    File Handle checks made consistent - VSW#199372
*       10-31-04  AC    Prefast fixes
*                       VSW#373224
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <errno.h>
#include <io.h>
#include <internal.h>
#include <msdos.h>
#include <mtdll.h>
#include <stddef.h>
#include <stdlib.h>

/***
*__int64 _filelengthi64(filedes) - find length of a file
*
*Purpose:
*       Returns the length in bytes of the specified file.
*
*Entry:
*       int filedes - handle referring to file to find length of
*
*Exit:
*       returns length of file in bytes
*       returns -1i64 if fails
*
*Exceptions:
*
*******************************************************************************/

__int64 __cdecl _filelengthi64 (
        int filedes
        )
{
        __int64 length;
        __int64 here;

        _CHECK_FH_CLEAR_OSSERR_RETURN( filedes, EBADF, -1i64 );
        _CHECK_IO_INIT(-1i64);
        _VALIDATE_CLEAR_OSSERR_RETURN((filedes >= 0 && (unsigned)filedes < (unsigned)_nhandle), EBADF, -1i64);
        _VALIDATE_CLEAR_OSSERR_RETURN((_osfile(filedes) & FOPEN), EBADF, -1i64);

        _lock_fh(filedes);
        __try {
            if ( _osfile(filedes) & FOPEN ) {
				/* Seek to end (and back) to get the file length. */

				if ( (here = _lseeki64_nolock( filedes, 0i64, SEEK_CUR )) == -1i64 )
					length = -1i64;     /* return error */
				else {
					length = _lseeki64_nolock( filedes, 0i64, SEEK_END );
					if ( here != length )
						_lseeki64_nolock( filedes, here, SEEK_SET );
				}
            }
            else {
                    errno = EBADF;
                    _doserrno = 0L;
                    length = -1i64;
                    _ASSERTE(("Invalid file descriptor. File possibly closed by a different thread",0));
            }
        }
        __finally {
                _unlock_fh(filedes);
        }

        return( length );
}
