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
*locking.c - file locking function
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defined the _locking() function - file locking and unlocking
*
*Revision History:
*       06-09-89  PHG   Module created, based on asm version
*       08-10-89  JCR   Changed DOS32FILELOCKS to DOS32SETFILELOCKS
*       03-12-90  GJF   Made calling type _CALLTYPE2 (for now), added #include
*                       <cruntime.h> and fixed the copyright. Also, cleaned up
*                       the formatting a bit.
*       04-03-90  GJF   Now _CALLTYPE1.
*       07-24-90  SBM   Removed '32' from API names
*       08-14-90  SBM   Compiles cleanly with -W3
*       09-28-90  GJF   New-style function declarator.
*       12-04-90  GJF   Appended Win32 version onto the source with #ifdef-s.
*                       It is enough different that there is little point in
*                       trying to more closely merge the two versions.
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Changed to use _osfile and _osfhnd instead of _osfinfo
*       01-16-91  GJF   ANSI naming.
*       02-07-91  SRW   Changed to call _get_osfhandle [_WIN32_]
*       12-05-91  GJF   Fixed usage of [Un]LockFile APIs [_WIN32_].
*       02-13-92  GJF   Replaced _nfile by _nhandle for Win32.
*       05-06-92  SRW   WIN32 LockFile API changed. [_WIN32_].
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-06-94  CFW   Remove Cruiser support.
*       12-03-94  SKS   Clean up OS/2 references
*       01-04-95  GJF   _WIN32_ -> _WIN32
*       02-15-95  GJF   Appended Mac version of source file (somewhat cleaned
*                       up), with appropriate #ifdef-s.
*       06-27-95  GJF   Added check that the file handle is open.
*       07-08-96  GJF   Replaced defined(_WIN32) with !defined(_MAC), and
*                       defined(_M_M68K) || defined(_M_MPPC) with 
*                       defined(_MAC). Also, detab-ed and cleaned up the 
*                       format a bit.
*       12-19-97  GJF   Exception-safe locking.
*       05-17-99  PML   Remove all Macintosh support.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       11-13-03  SJ    File Handle checks made consistent - VSW#199372
*       10-31-04  AC    Prefast fixes
*                       VSW#373224
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <errno.h>
#include <sys\locking.h>
#include <io.h>
#include <stdlib.h>
#include <internal.h>
#include <msdos.h>
#include <mtdll.h>

static int __cdecl _locking_nolock(int, int, long);

/***
*int _locking(fh,lmode,nbytes) - file record locking function
*
*Purpose:
*       Locks or unlocks nbytes of a specified file
*
*       Multi-thread - Must lock/unlock the file handle to prevent
*       other threads from working on the file at the same time as us.
*       [NOTE: We do NOT release the lock during the 1 second delays
*       since some other thread could get in and do something to the
*       file.  The DOSFILELOCK call locks out other processes, not
*       threads, so there is no multi-thread deadlock at the DOS file
*       locking level.]
*
*Entry:
*       int fh -        file handle
*       int lmode -     locking mode:
*                           _LK_LOCK/_LK_RLCK -> lock, retry 10 times
*                           _LK_NBLCK/_LK_N_BRLCK -> lock, don't retry
*                           _LK_UNLCK -> unlock
*       long nbytes -   number of bytes to lock/unlock
*
*Exit:
*       returns 0 if successful
*       returns -1 and sets errno if unsuccessful
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _locking (
        int fh,
        int lmode,
        long nbytes
        )
{
        int retval;

        /* validate file handle */
        _CHECK_FH_CLEAR_OSSERR_RETURN( fh, EBADF, -1 );
        _CHECK_IO_INIT(-1);
        _VALIDATE_CLEAR_OSSERR_RETURN((fh >= 0 && (unsigned)fh < (unsigned)_nhandle), EBADF, -1);
        _VALIDATE_CLEAR_OSSERR_RETURN((_osfile(fh) & FOPEN), EBADF, -1);
        _VALIDATE_CLEAR_OSSERR_RETURN((nbytes >= 0), EINVAL, -1);

        _lock_fh(fh);                   /* acquire file handle lock */

        __try {
                if ( _osfile(fh) & FOPEN )
                        retval = _locking_nolock(fh, lmode, nbytes);
                else {
                        errno = EBADF;
                        _doserrno = 0;  /* not an o.s. error */
                        retval = -1;
                        _ASSERTE(("Invalid file descriptor. File possibly closed by a different thread",0));                        
                }
        }
        __finally {
                _unlock_fh(fh);
        }

        return retval;
}

 static int __cdecl _locking_nolock (
        int fh,
        int lmode,
        long nbytes
        )
{
        ULONG dosretval;                /* o.s. return code */
        __int64 lockoffset;
        int retry;                      /* retry count */
        OVERLAPPED overlapped = {0};    /* used by Lock/UnlockFileEx */

        /* obtain current position in file by calling _lseeki64 */
        /* Use _lseeki64_nolock as we already own lock */
        lockoffset = _lseeki64_nolock(fh, 0L, 1);
        if (lockoffset == -1)
                return -1;

        /* set the offset */
        overlapped.Offset = (DWORD)lockoffset;
        overlapped.OffsetHigh = (DWORD)(((lockoffset) >> 32) & 0xffffffff);

        /* set retry count based on mode */
        if (lmode == _LK_LOCK || lmode == _LK_RLCK)
                retry = 9;              /* retry 9 times */
        else
                retry = 0;              /* don't retry */

        /* ask o.s. to lock the file until success or retry count finished */
        /* note that the only error possible is a locking violation, since */
        /* an invalid handle would have already failed above */
        for (;;) {

                dosretval = 0;
                if (lmode == _LK_UNLCK) {
                    if ( !(UnlockFileEx(
                                      (HANDLE)_get_osfhandle(fh),
                                      0L, /* reserved */
                                      nbytes,
                                      0L,
                                      &overlapped))
                       )
                        dosretval = GetLastError();

                } else {
                    if ( !(LockFileEx(
                                    (HANDLE)_get_osfhandle(fh),
                                    /* ensure exclusive lock access + return immediately if unable to acquire lock */
                                    LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                                    0L, /* reserved */
                                    nbytes,
                                    0L,
                                    &overlapped))
                       )
                        dosretval = GetLastError();
                }

                if (retry <= 0 || dosretval == 0)
                        break;  /* exit loop on success or retry exhausted */

                Sleep(1000L);

                --retry;
        }

        if (dosretval != 0) {
                /* o.s. error occurred -- file was already locked; if a
                   blocking call, then return EDEADLOCK, otherwise map
                   error normally */
                if (lmode == _LK_LOCK || lmode == _LK_RLCK) {
                        errno = EDEADLOCK;
                        _doserrno = dosretval;
                }
                else {
                        _dosmaperr(dosretval);
                }
                return -1;
        }
        else
                return 0;
}
