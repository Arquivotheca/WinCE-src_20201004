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
*dup.c - duplicate file handles
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines _dup() - duplicate file handles
*
*Revision History:
*       06-09-89  PHG   Module created, based on asm version
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
*       02-18-91  SRW   Changed to call _free_osfhnd [_WIN32_]
*       02-25-91  SRW   Renamed _get_free_osfhnd to be _alloc_osfhnd [_WIN32_]
*       02-13-92  GJF   Replaced _nfile by _nhandle for Win32.
*       09-03-92  GJF   Added explicit check for unopened handles [_WIN32_].
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-06-94  CFW   Remove Cruiser support.
*       12-03-94  SKS   Clean up OS/2 references
*       01-04-95  GJF   _WIN32_ -> _WIN32
*       02-15-95  GJF   Appended Mac version of source file (somewhat cleaned
*                       up), with appropriate #ifdef-s.
*       06-11-95  GJF   Replaced _osfile[] with _osfile() (macro referencing
*                       field in ioinfo struct).
*       05-16-96  GJF   Clear FNOINHERIT (new) bit on _osfile. Also, detab-ed.
*       07-08-96  GJF   Replaced defined(_WIN32) with !defined(_MAC), and
*                       defined(_M_M68K) || defined(_M_MPPC) with 
*                       defined(_MAC). Also, cleaned up the format a bit.
*       12-17-97  GJF   Exception-safe locking.
*       02-07-98  GJF   Changes for Win64: use intptr_t for anything holding 
*                       a HANDLE value.
*       05-17-99  PML   Remove all Macintosh support.
*       04-29-02  GB    Added try-finally arounds lock-unlock.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       11-13-03  SJ    File Handle checks made consistent - VSW#199372
*       03-13-04  MSL   Avoid returning from within __try for prefast
*       10-31-04  AC    Prefast fixes
*                       VSW#373224
*       05-10-05  PML   _alloc_osfhnd now sets FOPEN (VSW#443926)
*
*******************************************************************************/

#include <cruntime.h>
#include <oscalls.h>
#include <errno.h>
#include <mtdll.h>
#include <io.h>
#include <msdos.h>
#include <internal.h>
#include <stdlib.h>

static int __cdecl _dup_nolock(int);

/***
*int _dup(fh) - duplicate a file handle
*
*Purpose:
*       Assigns another file handle to the file associated with the
*       handle fh.  The next available file handle is assigned.
*
*       Multi-thread: Be sure not to hold two file handle locks
*       at the same time!
*
*Entry:
*       int fh - file handle to duplicate
*
*Exit:
*       returns new file handle if successful
*       returns -1 (and sets errno) if fails
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _dup (
        int fh
        )
{
        int newfh =-1;                      /* variable for new file handle */

        /* validate file handle */
        _CHECK_FH_CLEAR_OSSERR_RETURN( fh, EBADF, -1 );
        _CHECK_IO_INIT(-1);
        _VALIDATE_CLEAR_OSSERR_RETURN((fh >= 0 && (unsigned)fh < (unsigned)_nhandle), EBADF, -1);
        _VALIDATE_CLEAR_OSSERR_RETURN((_osfile(fh) & FOPEN), EBADF, -1);
    
        _lock_fh(fh);                   /* lock file handle */
#ifdef _WIN32_WCE
        if (fh < 3 && _osfhnd(fh) == (intptr_t)INVALID_HANDLE_VALUE) {
            BOOL is_dev = TRUE;
            _osfhnd(fh) = (intptr_t)OpenStdConsole(fh, &is_dev);
            if (!is_dev)
                _osfile(fh) &= (char)~FDEV;
        }
#endif    

        __TRY
                if ( _osfile(fh) & FOPEN )
                        newfh = _dup_nolock(fh);
                else {
                        errno = EBADF;
                        _doserrno = 0;
                        newfh = -1;
                        _ASSERTE(("Invalid file descriptor. File possibly closed by a different thread",0));
                }
        __FINALLY
                _unlock_fh(fh);
        __END_TRY_FINALLY

        return newfh;
}

static int __cdecl _dup_nolock(
        int fh
        )
{
        int newfh;                      /* variable for new file handle */
        ULONG dosretval;                /* o.s. return value */
        char fileinfo;                  /* _osfile info for file */
        intptr_t new_osfhandle;
        int success = FALSE;

        fileinfo = _osfile(fh);         /* get file info for file */

        if ( !(_osfile(fh) & FOPEN) )
                return -1;

        /* create duplicate handle */

        if ( (newfh = _alloc_osfhnd()) == -1 ) 
        {
                errno = EMFILE;         /* too many files error */
                _doserrno = 0L;         /* not an OS error */
                return -1;              /* return error to caller */
        }
        __TRY

            /*
             * duplicate the file handle
             */
            if ( !(DuplicateHandle(GetCurrentProcess(),
                                   (HANDLE)_get_osfhandle(fh),
                                   GetCurrentProcess(),
                                   (PHANDLE)&new_osfhandle,
                                   0L,
                                   TRUE,
                                   DUPLICATE_SAME_ACCESS)) )
            {
                    dosretval = GetLastError();
            }
            else {
                    _set_osfhnd(newfh, new_osfhandle);
                    dosretval = 0;
            }

            if (dosretval) 
                        {
                    /* o.s. error -- map errpr and release handle */
                    _dosmaperr(dosretval);
            }
            else
            {
                    /* 
                     * copy the _osfile value, with the FNOINHERIT bit cleared 
                     */
                    _osfile(newfh) = fileinfo & ~FNOINHERIT;
                    _textmode(newfh) = _textmode(fh);
                    _tm_unicode(newfh) = _tm_unicode(fh);
                    success = TRUE;
            }

        __FINALLY
            if (!success)
            {
                _osfile(newfh) &= ~FOPEN;
            }
            _unlock_fh(newfh);
        __END_TRY_FINALLY

            return success ? newfh : -1;
}
