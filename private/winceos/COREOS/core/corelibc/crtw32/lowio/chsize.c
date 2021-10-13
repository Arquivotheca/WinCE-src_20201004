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
*chsize.c - change size of a file
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       contains the _chsize() function - changes the size of a file.
*
*Revision History:
*       03-13-84  RN    initial version
*       05-17-86  SKS   ported to OS/2
*       07-07-87  JCR   Added (_doserrno == 5) check that is in DOS 3.2 version
*       10-29-87  JCR   Multi-thread support; also, re-wrote for efficiency
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-25-88  PHG   Merged DLL and normal versions
*       10-03-88  GJF   Changed DOSNEWSIZE to SYSNEWSIZE
*       10-10-88  GJF   Made API names match DOSCALLS.H
*       04-13-89  JCR   New syscall interface
*       05-25-89  JCR   386 OS/2 calls use '_syscall' calling convention
*       03-12-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h>, removed #include <register.h> and fixed
*                       the copyright. Also, cleaned up the formatting a bit.
*       04-04-90  GJF   Added #include <string.h>, removed #include <dos.h>.
*       05-21-90  GJF   Fixed stack checking pragma syntax.
*       07-24-90  SBM   Replaced <assertm.h> by <assert.h>, removed '32'
*                       from API names
*       09-28-90  GJF   New-style function declarator.
*       12-03-90  GJF   Appended Win32 version of the function. It is based
*                       on the Cruiser version and probably could be merged
*                       in later (much later).
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Changed to use _osfile and _osfhnd instead of _osfinfo
*       12-28-90  SRW   Added _CRUISER_ conditional around check_stack pragma
*       01-16-91  GJF   ANSI naming. Also, fixed __chsize_lk parameter decls.
*       02-07-91  SRW   Changed to call _get_osfhandle [_WIN32_]
*       04-09-91  PNT   Added _MAC_ conditional
*       02-13-92  GJF   Replaced _nfile by _nhandle for Win32.
*       05-01-92  GJF   Fixed embarrassing bug (didn't work for Win32)!
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       05-11-93  GJF   Replaced BUFSIZ with _INTERNAL_BUFSIZ.
*       09-06-94  CFW   Remove Cruiser support.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       01-07-95  CFW   Mac merge.
*       02-06-95  CFW   assert -> _ASSERTE.
*       06-27-95  GJF   Added check that the file handle is open.
*       07-03-96  GJF   Replaced defined(_WIN32) with !defined(_MAC), and
*                       defined(_M_M68K) || defined(_M_MPPC) with 
*                       defined(_MAC). Also, detab-ed and cleaned up the 
*                       format a bit.
*       12-17-97  GJF   Exception-safe locking.
*       05-17-99  PML   Remove all Macintosh support.
*       06-25-01  BWT   Alloc blank buffer off the heap instead of the stack (ntbug: 423988)
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-20-03  SJ    errno shouldn't be set to 0 on success
*       11-13-03  SJ    File Handle checks made consistent - VSW#199372
*       10-31-04  AC    Prefast fixes
*                       VSW#373224
*
*******************************************************************************/

#include <cruntime.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <dbgint.h>
#include <fcntl.h>
#include <msdos.h>
#include <io.h>
#include <string.h>
#include <windows.h>
#include <internal.h>
#include <mtdll.h>

/***
*erccode _chsize_s(filedes, size) - change size of a file
*
*Purpose:
*       Change file size. Assume file is open for writing, or we can't do it.
*       The DOS way to do this is to go to the right spot and write 0 bytes. The
*       Xenix way to do this is to make a system call. We write '\0' bytes because
*       DOS won't do this for you if you lseek beyond eof, though Xenix will.
*
*Entry:
*       int filedes - file handle to change size of
*       __int64 size - new size of file
*
*Exit:
*       return 0 if successful
*       returns errno_t on failure
*
*Exceptions:
*
*******************************************************************************/

/* define normal version that locks/unlocks, validates fh */

errno_t __cdecl _chsize_s (
        int filedes,
        __int64 size
        )
{
        int r;                          /* return value */

        _CHECK_FH_CLEAR_OSSERR_RETURN_ERRCODE( filedes, EBADF );
        _CHECK_IO_INIT(EBADF);
        _VALIDATE_CLEAR_OSSERR_RETURN_ERRCODE((filedes >= 0 && (unsigned)filedes < (unsigned)_nhandle), EBADF);
        _VALIDATE_CLEAR_OSSERR_RETURN_ERRCODE((_osfile(filedes) & FOPEN), EBADF);
        _VALIDATE_CLEAR_OSSERR_RETURN_ERRCODE((size >= 0), EINVAL);
        
        _lock_fh(filedes);

        __try {
                if ( _osfile(filedes) & FOPEN )
                        r = _chsize_nolock(filedes,size);
                else {
                        _ASSERTE(("Invalid file descriptor. File possibly closed by a different thread",0));
                        r = errno = EBADF;
                }
        }
        __finally {
                _unlock_fh(filedes);
        }

        return r;
}

/* now define version that doesn't lock/unlock, validate fh */
errno_t __cdecl _chsize_nolock (
        int filedes,
        __int64 size
        )
{
        __int64 filend;
        __int64 extend;
        __int64 place;
        int cnt;
        int oldmode;
        __int64 retval = 0; /* assume good return */
        errno_t err = 0;

		/* Get current file position and seek to end */
        if ( ((place = _lseeki64_nolock(filedes, 0i64, SEEK_CUR)) == -1i64) ||
             ((filend = _lseeki64_nolock(filedes, 0i64, SEEK_END)) == -1i64) )
            return errno;

        extend = size - filend;

        /* Grow or shrink the file as necessary */

        if (extend > 0i64) {

            /* extending the file */
            char *bl = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, _INTERNAL_BUFSIZ);

            if (!bl) {
                err = errno = ENOMEM;
                retval= -1i64;
            } else {
                oldmode = _setmode_nolock(filedes, _O_BINARY);

                /* pad out with nulls */
                do  {
                    cnt = (extend >= (__int64)_INTERNAL_BUFSIZ ) ?
                          _INTERNAL_BUFSIZ : (int)extend;
                    if ( (cnt = _write_nolock( filedes,
                                           bl,
                                           (extend >= (__int64)_INTERNAL_BUFSIZ) ?
                                                _INTERNAL_BUFSIZ : (int)extend ))
                         == -1 )
                    {
                        /* Error on write */
                        if (_doserrno == ERROR_ACCESS_DENIED)
                            err = errno = EACCES;

                        retval = cnt;
                        break;  /* leave write loop */
                    }
                }
                while ((extend -= (__int64)cnt) > 0i64);

                _setmode_nolock(filedes, oldmode);

                HeapFree(GetProcessHeap(), 0, bl);
            }

            /* retval set correctly */
        }

        else  if ( extend < 0i64 ) {
            /* shortening the file */

            /*
             * Set file pointer to new eof...and truncate it there.
             */
            retval = _lseeki64_nolock(filedes, size, SEEK_SET);
            
            
            if(retval != -1i64)
            {
                if ( (retval = SetEndOfFile((HANDLE)_get_osfhandle(filedes)) ?
                     0 : -1) == -1 )
                {
                    err = errno = EACCES;
                    _doserrno = GetLastError();
                }
            }
        }

        /* else */
        /* no file change needed */
        /* retval = 0; */


/* Common return code */

        if(retval == -1 || (_lseeki64_nolock(filedes, place, SEEK_SET) == -1i64))
        {
            return errno;
        }

        return 0;
}

/***
*int _chsize(filedes, size) - change size of a file
*
*Purpose:
*       Change file size. Assume file is open for writing, or we can't do it.
*       The DOS way to do this is to go to the right spot and write 0 bytes. The
*       Xenix way to do this is to make a system call. We write '\0' bytes because
*       DOS won't do this for you if you lseek beyond eof, though Xenix will.
*
*Entry:
*       int filedes - file handle to change size of
*       long size - new size of file
*
*Exit:
*       return 0 if successful
*       returns -1 and sets errno if fails
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _chsize (
        int filedes,
        long size
        )
{
    errno_t e;
    e = _chsize_s(filedes, (__int64)size) ;
    
    return e == 0 ? 0 : -1 ;
}
