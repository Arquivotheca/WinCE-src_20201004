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
*getcwd.c - get current working directory
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*
*       contains functions _getcwd, _getdcwd and _getcdrv for getting the
*       current working directory.  getcwd gets the c.w.d. for the default disk
*       drive, whereas _getdcwd allows one to get the c.w.d. for whatever disk
*       drive is specified. _getcdrv gets the current drive.
*
*Revision History:
*       09-09-83  RKW   created
*       05-??-84  DCW   added conditional compilation to handle case of library
*                       where SS != DS (can't take address of a stack variable).
*       09-??-84  DCW   changed comparison of path length to maxlen to take the
*                       terminating null character into account.
*       11-28-84  DCW   changed to return errno values compatibly with the
*                       System 3 version.
*       05-19-86  SKS   adapted for OS/2
*       11-19-86  SKS   if pnbuf==NULL, maxlen is ignored;
*                       eliminated use of intermediate buffer "buf[]"; added
*                       entry point "_getdcwd()" which takes a drive number.
*       12-03-86  SKS   if pnbuf==NULL, maxlen is the minimum allocation size
*       02-05-87  BCM   fixed comparison in _getdcwd,
*                       (unsigned)(len+3) > (int)(maxlen), to handle maxlen < 0,
*                       since implicit cast to (unsigned) was occurring.
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       12-21-87  WAJ   Added _getcdrv()
*       06-22-88  WAJ   _getcdrv() is now made for all OS/2 libs
*       10-03-88  JCR   386: Change DOS calls to SYS calls
*       10-04-88  JCR   386: Removed 'far' keyword
*       10-10-88  GJF   Made API names match DOSCALLS.H
*       01-31-89  JCR   Remove _getcdrv(), which has been renamed _getdrive()
*       04-12-89  JCR   Use new OS/2 system calls
*       05-25-89  JCR   386 OS/2 calls use '_syscall' calling convention
*       11-27-89  JCR   Corrected ERRNO values
*       12-12-89  JCR   Fixed bogus syscall introduced in previous fix (oops)
*       03-07-90  GJF   Replaced _LOAD_DS by _CALLTYPE1, added #include
*                       <cruntime.h>, removed #include <register.h>, removed
*                       some leftover 16-bit support and fixed the copyright.
*                       Also, cleaned up the formatting a bit.
*       07-24-90  SBM   Compiles cleanly with -W3 (removed unreferenced
*                       variable), removed redundant includes, removed
*                       '32' from API names
*       08-10-90  SBM   Compiles cleanly with -W3 with new build of compiler
*       09-27-90  GJF   New-style function declarator.
*       12-04-90  SRW   Changed to include <oscalls.h> instead of <doscalls.h>
*       12-06-90  SRW   Added _CRUISER_ and _WIN32 conditionals.
*       01-16-91  GJF   ANSI naming.
*       08-21-91  JCR   Test DOSQUERYCURRENTDIR call for error return (update)
*       04-23-92  GJF   Fixed initialization of DriveVar[].
*       04-28-92  GJF   Revised Win32 version.
*       12-09-92  PLM   Removed _getdcwd (Mac version only)
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*                       Change _ValidDrive to _validdrive
*       04-19-93  SKS   Move _validdrive to this module
*       04-26-93  SKS   Set _doserrno on invalid drive
*       05-26-93  SKS   Change _getdcwd to call GetFullPathName() instead of
*                       reading a current directory environment variable.
*       09-30-93  GJF   Removed #include <error.h> (thereby getting rid of a
*                       bunch of compiler warnings). Also, MTHREAD -> _MT.
*       11-01-93  CFW   Enable Unicode variant.
*       12-21-93  CFW   Fix API failure error handling.
*       01-04-94  CFW   Fix API failure error handling correctly.
*       08-11-94  GJF   Revised _validdrive() to use GetDriveType (suggestion
*                       from Richard Shupak).
*       08-18-94  GJF   Revised _validdrive() logic slightly per suggestion
*                       of Richard Shupak.
*       02-08-95  JWM   Spliced _WIN32 & Mac versions.
*       07-01-96  GJF   Replaced defined(_WIN32) with !defined(_MAC). Also,
*                       detab-ed and cleaned up the format a bit.
*       12-17-97  GJF   Exception-safe locking.
*       05-17-99  PML   Remove all Macintosh support.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       10-18-04  MSL   Fix getdrive validation for network drives
*       04-01-05  MSL   Integer overflow fix during allocation 
*
*******************************************************************************/

#include <cruntime.h>
#include <mtdll.h>
#include <msdos.h>
#include <errno.h>
#include <malloc.h>
#include <oscalls.h>
#include <stdlib.h>
#include <internal.h>
#include <direct.h>
#include <tchar.h>

/***
*_TSCHAR *_getcwd(pnbuf, maxlen) - get current working directory of default drive
*
*Purpose:
*       _getcwd gets the current working directory for the user,
*       placing it in the buffer pointed to by pnbuf.  It returns
*       the length of the string put in the buffer.  If the length
*       of the string exceeds the length of the buffer, maxlen,
*       then NULL is returned.  If pnbuf = NULL, maxlen is ignored.
*       An entry point "_getdcwd()" is defined with takes the above
*       parameters, plus a drive number.  "_getcwd()" is implemented
*       as a call to "_getcwd()" with the default drive (0).
*
*       If pnbuf = NULL, maxlen is ignored, and a buffer is automatically
*       allocated using malloc() -- a pointer to which is returned by
*       _getcwd().
*
*       side effects: no global data is used or affected
*
*Entry:
*       _TSCHAR *pnbuf = pointer to a buffer maintained by the user;
*       int maxlen = length of the buffer pointed to by pnbuf;
*
*Exit:
*       Returns pointer to the buffer containing the c.w.d. name
*       (same as pnbuf if non-NULL; otherwise, malloc is
*       used to allocate a buffer)
*
*Exceptions:
*
*******************************************************************************/

_TSCHAR * __cdecl _tgetcwd (
        _TSCHAR *pnbuf,
        int maxlen
        )
{
    _TSCHAR *retval;

    retval = _tgetdcwd(0, pnbuf, maxlen);

    return retval;
}

#ifdef _DEBUG

_TSCHAR * __cdecl _tgetcwd_dbg (
        _TSCHAR *pnbuf,
        int maxlen,
        int nBlockUse,
        const char * szFileName,
        int nLine
        )
{
    _TSCHAR *retval;

	retval = _tgetdcwd_dbg(0, pnbuf, maxlen, nBlockUse, szFileName, nLine);

    return retval;
}

#endif

#ifdef _DEBUG

/***
*_TSCHAR *_getdcwd(drive, pnbuf, maxlen) - get c.w.d. for given drive
*
*Purpose:
*       _getdcwd gets the current working directory for the user,
*       placing it in the buffer pointed to by pnbuf.  It returns
*       the length of the string put in the buffer.  If the length
*       of the string exceeds the length of the buffer, maxlen,
*       then NULL is returned.  If pnbuf = NULL, maxlen is ignored,
*       and a buffer is automatically allocated using malloc() --
*       a pointer to which is returned by _getdcwd().
*
*       side effects: no global data is used or affected
*
*Entry:
*       int drive   - number of the drive being inquired about
*                     0 = default, 1 = 'a:', 2 = 'b:', etc.
*       _TSCHAR *pnbuf - pointer to a buffer maintained by the user;
*       int maxlen  - length of the buffer pointed to by pnbuf;
*
*Exit:
*       Returns pointer to the buffer containing the c.w.d. name
*       (same as pnbuf if non-NULL; otherwise, malloc is
*       used to allocate a buffer)
*
*Exceptions:
*
*******************************************************************************/

_TSCHAR * __cdecl _tgetdcwd (
        int drive,
        _TSCHAR *pnbuf,
        int maxlen
        )
{
    return _tgetdcwd_dbg(drive, pnbuf, maxlen, _NORMAL_BLOCK, NULL, 0);
}

_TSCHAR * __cdecl _tgetdcwd_dbg (
        int drive,
        _TSCHAR *pnbuf,
        int maxlen,
        int nBlockUse,
        const char * szFileName,
        int nLine
        )

#else   /* ndef _DEBUG */

_TSCHAR * __cdecl _tgetdcwd (
        int drive,
        _TSCHAR *pnbuf,
        int maxlen
        )

#endif  /* _DEBUG */
{

#ifdef _WIN32_WCE
    _dosmaperr(ERROR_NOT_SUPPORTED);
    return NULL;
#else
    _TSCHAR *p;
    _TSCHAR drvstr[4];
    int ret, count;
    _TSCHAR *pname; /* only used as argument to GetFullPathName */

    if ( drive != 0 ) {
        /*
         * Not the default drive - make sure it's valid.
         */
        if ( !_validdrive(drive) ) {
            _doserrno = ERROR_INVALID_DRIVE;        
            _VALIDATE_RETURN(("Invalid Drive",0), EACCES, NULL);
        }
    } else {
        /* Get the drive index of the default drive */
        drive = _getdrive();

    }

    /* If pnbuf is NULL, we pass 0 in both the 2nd & 3rd parameter
    to find required length to allocate */

    if(pnbuf)
    {
        _VALIDATE_RETURN( (maxlen > 0), EINVAL, NULL);
        count = maxlen;
        pnbuf[0] = 0;
    }
    else
        count = 0;
        
    /*
     * Get the current directory string on that drive and its length
     */
    if(drive!=0)
    {
        drvstr[0] = _T('A') - 1 + drive;
        drvstr[1] = _T(':');
        drvstr[2] = _T('.');
        drvstr[3] = _T('\0');
    }
    else
    {
        drvstr[0] = _T('.');
        drvstr[1] = _T('\0');
    }
   
    ret = GetFullPathName( drvstr, count, pnbuf, &pname );
                                   
    if(ret == 0) {
        _dosmaperr( GetLastError() );
        return NULL;
    }
    
    if( pnbuf != NULL ) {
        if(ret < count) {
            /* GetFullPathName always returns a value less than that
            passed in if successful i.e. we got the required dir */
            return pnbuf;
        }
        
        errno = ERANGE;
        *pnbuf = 0;
        return NULL;
    }
    
    /* The code comes here only if pnbuf was NULL
    Allocate the required memory & call GetFullPathName again*/
    
    maxlen = __max(ret, maxlen) ;
#ifdef _DEBUG        
    if( (p = (_TSCHAR *)_calloc_dbg( maxlen, sizeof(_TSCHAR), nBlockUse, szFileName, nLine)) == NULL) {
#else        
    if( (p = (_TSCHAR *)calloc( maxlen, sizeof(_TSCHAR))) == NULL) {
#endif        
        errno = ENOMEM;
        _doserrno = E_nomem;
        return NULL;
    }
    
    
    ret = GetFullPathName( drvstr, maxlen, p, &pname );
    
    if(ret == 0 || ret >= maxlen) {
        _dosmaperr( GetLastError() );
        return NULL;
    }
    
    return p;
#endif // _WIN32_WCE
}

#ifndef WPRFLAG

/***
*int _validdrive( unsigned drive ) -
*
*Purpose: returns non zero if drive is a valid drive number.
*
*Entry: drive = 0 => default drive, 1 => a:, 2 => b: ...
*
*Exit:  0 => drive does not exist.
*
*Exceptions:
*
*******************************************************************************/

int __cdecl _validdrive (
    unsigned drive
    )
{
#ifdef _WIN32_WCE
       _doserrno = ERROR_INVALID_FUNCTION;
        return 0;
#else
        unsigned retcode;
        wchar_t drvstr[4];

        if (drive > 26) {
            _doserrno = ERROR_INVALID_DRIVE;
            _VALIDATE_RETURN(("Invalid Drive Index",0), EACCES, 0);
        }

        if ( drive == 0 )
            return 1;

        drvstr[0] = L'A' + drive - 1;
        drvstr[1] = L':';
        drvstr[2] = L'\\';
        drvstr[3] = L'\0';

        if ( ((retcode = GetDriveTypeW( drvstr )) == DRIVE_UNKNOWN) ||
             (retcode == DRIVE_NO_ROOT_DIR) )
            return 0;

        return 1;
#endif // _WIN32_WCE
}

#endif  /* WPRFLAG */
