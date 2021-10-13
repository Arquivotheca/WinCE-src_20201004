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
*utime.c - set modification time for a file
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Sets the access/modification times for a file.
*
*Revision History:
*       03-??-84  RLB   initial version
*       05-17-86  SKS   ported to OS/2
*       08-21-87  JCR   error return if localtime() returns NULL.
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       10-03-88  JCR   386: Change DOS calls to SYS calls
*       10-04-88  JCR   386: Removed 'far' keyword
*       10-10-88  GJF   Made API names match DOSCALLS.H
*       10-11-88  GJF   Made API arg types match DOSCALLS.H
*       04-12-89  JCR   New syscall interface
*       05-01-89  JCR   Corrected OS/2 time/date interpretation
*       05-25-89  JCR   386 OS/2 calls use '_syscall' calling convention
*       08-16-89  PHG   moved date validation above open() so file isn't left
*                       open if the date is invalid
*       03-20-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h>, removed #include <register.h>, removed
*                       some leftover 16-bit support and fixed the copyright.
*                       Also, cleaned up the formatting a bit.
*       07-25-90  SBM   Compiles cleanly with -W3 (added include, removed
*                       unreferenced variable), removed '32' from API names
*       10-04-90  GJF   New-style function declarator.
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Added _CRUISER_ and _WIN32 conditionals.
*       01-18-91  GJF   ANSI naming.
*       02-14-91  SRW   Fix Mips compile error (_WIN32_)
*       02-26-91  SRW   Fix SetFileTime parameter ordering (_WIN32_)
*       08-21-91  BWM   Add _futime to set time on open file
*       08-26-91  BWM   Change _utime to call _futime
*       05-19-92  DJM   ifndef for POSIX build.
*       08-18-92  SKS   SystemTimeToFileTime now takes UTC/GMT, not local time.
*                       Remove _CRUISER_ conditional
*       04-02-93  GJF   Changed interpretation of error on SetFileTime call.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-09-93  GJF   Have Win32 convert from a local file time value to a
*                       (system) file time value. This is symmetric with
*                       _stat() and a better work-around for the Windows NT
*                       bug in converting file times on FAT (applies DST
*                       offset based on current time rather than the file's
*                       time stamp).
*       11-01-93  CFW   Enable Unicode variant, rip out Cruiser.
*       02-09-95  GJF   Replaced WPRFLAG with _UNICODE.
*       02-13-95  GJF   Appended Mac version of source file (somewhat cleaned
*                       up), with appropriate #ifdef-s.
*       05-17-99  PML   Remove all Macintosh support.
*       10-27-99  GB    Remove #inlcude <dostypes.h>
*       02-03-03  EAN   changed to _utime32 as part of 64-bit time_t change 
*                       (VSWhidbey 6851)
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
*int _utime32(pathname, time) - set modification time for file
*
*Purpose:
*       Sets the modification time for the file specified by pathname.
*       Only the modification time from the __utimbuf32 structure is used
*       under MS-DOS.
*
*Entry:
*       struct __utimbuf32 *time - new modification date
*
*Exit:
*       returns 0 if successful
*       returns -1 and sets errno if fails
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _tutime32 (
        const _TSCHAR *fname,
        struct __utimbuf32 *times
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

        retval = _futime32(fh, times);

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
*int _futime32(fh, time) - set modification time for open file
*
*Purpose:
*       Sets the modification time for the open file specified by fh.
*       Only the modification time from the __utimbuf32 structure is used
*       under MS-DOS.
*
*Entry:
*       struct __utimbuf32 *time - new modification date
*
*Exit:
*       returns 0 if successful
*       returns -1 and sets errno if fails
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _futime32 (
        int fh,
        struct __utimbuf32 *times
        )
{
        struct tm tmb;

        SYSTEMTIME SystemTime;
        FILETIME LocalFileTime;
        FILETIME LastWriteTime;
        FILETIME LastAccessTime;
        struct __utimbuf32 deftimes;

        _CHECK_FH_RETURN( fh, EBADF, -1 );
        _CHECK_IO_INIT(-1);
        _VALIDATE_RETURN((fh >= 0 && (unsigned)fh < (unsigned)_nhandle), EBADF, -1);
        _VALIDATE_RETURN((_osfile(fh) & FOPEN), EBADF, -1);

        if (times == NULL) {
                _time32(&deftimes.modtime);
                deftimes.actime = deftimes.modtime;
                times = &deftimes;
        }

        if (_localtime32_s(&tmb, &times->modtime) != 0) {
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

        if (_localtime32_s(&tmb, &times->actime) != 0) {
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
