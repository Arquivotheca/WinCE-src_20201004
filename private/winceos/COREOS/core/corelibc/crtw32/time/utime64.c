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
*utime64.c - set modification time for a file
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Sets the access/modification times for a file.
*
*Revision History:
*       05-28-98  GJF   Created.
*       03-06-03  SSM   Assertions and validations
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       11-13-03  SJ    File Handle checks made consistent - VSW#199372
*       12-09-03  SJ    VSW#187077 - errno in time functions doesn't match those
*                       in lowio for bad file handles
*       09-28-04  AC    Modified call to _sopen_s.
*       10-31-04  AC    Prefast fixes
*                       VSW#373224
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <sys/types.h>
#include <sys/utime.h>
#include <msdos.h>
#include <time.h>
#include <fcntl.h>
#include <io.h>
#include <dos.h>
#include <oscalls.h>
#include <errno.h>
#include <stddef.h>
#include <share.h>
#include <internal.h>

#include <stdio.h>
#include <tchar.h>

/***
*int _utime64(pathname, time) - set modification time for file
*
*Purpose:
*       Sets the modification time for the file specified by pathname.
*       Only the modification time from the __utimbuf64 structure is used
*       under MS-DOS.
*
*Entry:
*       struct __utimbuf64 *time - new modification date
*
*Exit:
*       returns 0 if successful
*       returns -1 and sets errno if fails
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _tutime64 (
        const _TSCHAR *fname,
        struct __utimbuf64 *times
        )
{
        int fh;
        int retval;
        int errno_local;

        _VALIDATE_RETURN( ( fname != NULL ), EINVAL, -1 )

        /* open file, fname, since filedate system call needs a handle.  Note
         * _utime definition says you must have write permission for the file
         * to change its time, so open file for write only.  Also, must force
         * it to open in binary mode so we dont remove ^Z's from binary files.
         */


        if (_tsopen_s(&fh, fname, _O_RDWR | _O_BINARY, _SH_DENYNO, 0) != 0)
                return(-1);

        retval = _futime64(fh, times);

        if ( retval == -1 )
        {
            errno_local = errno;
        }

        _close(fh);

        if ( retval == -1 )
        {
            errno = errno_local;
        }

        return(retval);
}

#ifndef _UNICODE

/***
*int _futime64(fh, time) - set modification time for open file
*
*Purpose:
*       Sets the modification time for the open file specified by fh.
*       Only the modification time from the __utimbuf64 structure is used
*       under MS-DOS.
*
*Entry:
*       struct __utimbuf64 *time - new modification date
*
*Exit:
*       returns 0 if successful
*       returns -1 and sets errno if fails
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _futime64 (
        int fh,
        struct __utimbuf64 *times
        )
{
        struct tm tmb;

        SYSTEMTIME SystemTime;
        FILETIME LocalFileTime;
        FILETIME LastWriteTime;
        FILETIME LastAccessTime;
        struct __utimbuf64 deftimes;

        _CHECK_FH_RETURN( fh, EBADF, -1 );
        _CHECK_IO_INIT(-1);
        _VALIDATE_RETURN((fh >= 0 && (unsigned)fh < (unsigned)_nhandle), EBADF, -1);
        _VALIDATE_RETURN((_osfile(fh) & FOPEN), EBADF, -1);

        if (times == NULL) {
                _time64(&deftimes.modtime);
                deftimes.actime = deftimes.modtime;
                times = &deftimes;
        }

        if (_localtime64_s(&tmb, &times->modtime) != 0) {
                errno = EINVAL;
                return(-1);
        }

        SystemTime.wYear   = (WORD)(tmb.tm_year + 1900);
        SystemTime.wMonth  = (WORD)(tmb.tm_mon + 1);
        SystemTime.wDay    = (WORD)(tmb.tm_mday);
        SystemTime.wHour   = (WORD)(tmb.tm_hour);
        SystemTime.wMinute = (WORD)(tmb.tm_min);
        SystemTime.wSecond = (WORD)(tmb.tm_sec);
        SystemTime.wMilliseconds = 0;

        if ( !SystemTimeToFileTime( &SystemTime, &LocalFileTime ) ||
             !LocalFileTimeToFileTime( &LocalFileTime, &LastWriteTime ) )
        {
                errno = EINVAL;
                return(-1);
        }

        if (_localtime64_s(&tmb, &times->actime) != 0) {
                errno = EINVAL;
                return(-1);
        }

        SystemTime.wYear   = (WORD)(tmb.tm_year + 1900);
        SystemTime.wMonth  = (WORD)(tmb.tm_mon + 1);
        SystemTime.wDay    = (WORD)(tmb.tm_mday);
        SystemTime.wHour   = (WORD)(tmb.tm_hour);
        SystemTime.wMinute = (WORD)(tmb.tm_min);
        SystemTime.wSecond = (WORD)(tmb.tm_sec);
        SystemTime.wMilliseconds = 0;

        if ( !SystemTimeToFileTime( &SystemTime, &LocalFileTime ) ||
             !LocalFileTimeToFileTime( &LocalFileTime, &LastAccessTime ) )
        {
                errno = EINVAL;
                return(-1);
        }

        /* set the date via the filedate system call and return. failing
         * this call implies the new file times are not supported by the
         * underlying file system.
         */

        if (!SetFileTime((HANDLE)_get_osfhandle(fh),
                                NULL,
                                &LastAccessTime,
                                &LastWriteTime
                               ))
        {
                errno = EINVAL;
                return(-1);
        }

        return(0);
}

#endif  /* _UNICODE */

#endif  /* _POSIX_ */
