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
*fullpath.c -
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose: contains the function _fullpath which makes an absolute path out
*       of a relative path. i.e.  ..\pop\..\main.c => c:\src\main.c if the
*       current directory is c:\src\src
*
*Revision History:
*       12-21-87  WAJ   Initial version
*       01-08-88  WAJ   now treats / as an \
*       06-22-88  WAJ   now handles network paths  ie \\sl\users
*       01-31-89  SKS/JCR Renamed _canonic to _fullpath
*       04-03-89  WAJ   Now returns "d:\dir" for "."
*       05-09-89  SKS   Do not change the case of arguments
*       11-30-89  JCR   Preserve errno setting from _getdcwd() call on errors
*       03-07-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h>, removed #include <register.h> and fixed
*                       the copyright.
*       04-25-90  JCR   Fixed an incorrect errno setting
*       06-14-90  SBM   Fixed bugs in which case of user provided drive letter
*                       was not always preserved, and c:\folder\\name did not
*                       generate an error
*       08-10-90  SBM   Compiles cleanly with -W3
*       08-28-90  SBM   Fixed bug in which UNC names were being rejected
*       09-27-90  GJF   New-style function declarator.
*       01-18-91  GJF   ANSI naming.
*       11-30-92  KRS   Ported _MBCS code from 16-bit tree.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-26-93  SKS   Add check for drive validity
*       08-03-93  KRS   Change to use _ismbstrail instead of isdbcscode.
*       09-27-93  CFW   Avoid cast bug.
*       12-07-93  CFW   Wide char enable.
*       01-26-94  CFW   Remove unused isdbcscode function.
*       11-08-94  GJF   Revised to use GetFullPathName.
*       02-08-95  JWM   Spliced _WIN32 & Mac versions.
*       03-28-96  GJF   Free malloc-ed buffer if GetFullPathName fails. 
*                       Detab-ed. Also, cleaned up the ill-formatted Mac
*                       version a bit and renamed isdbcscode to __isdbcscode.
*       07-01-96  GJF   Replaced defined(_WIN32) by !defined(_MAC).
*       12-15-98  GJF   Changes for 64-bit size_t.
*       05-17-99  PML   Remove all Macintosh support.
*       09-11-03  SJ    Secure CRT Work - Assertions & Validations
*       01-09-04  AJS   Fix regression when source and destination are same (VSWhidbey#215520)
*       03-04-04  AC    Use maxlen when allocating the buffer.
*       04-01-05  MSL   Integer overflow fix during allocation 
*
*******************************************************************************/

#include <cruntime.h>
#include <stdio.h>
#include <direct.h>
#include <errno.h>
#include <stdlib.h>
#include <internal.h>
#include <tchar.h>
#include <windows.h>


/***
*_TSCHAR *_fullpath( _TSCHAR *buf, const _TSCHAR *path, maxlen );
*
*Purpose:
*
*       _fullpath - combines the current directory with path to form
*       an absolute path. i.e. _fullpath takes care of .\ and ..\
*       in the path.
*
*       The result is placed in buf. If the length of the result
*       is greater than maxlen NULL is returned, otherwise
*       the address of buf is returned.
*
*       If buf is NULL then a buffer is malloc'ed and maxlen is
*       ignored. If there are no errors then the address of this
*       buffer is returned.
*
*       If path specifies a drive, the curent directory of this
*       drive is combined with path. If the drive is not valid
*       and _fullpath needs the current directory of this drive
*       then NULL is returned.  If the current directory of this
*       non existant drive is not needed then a proper value is
*       returned.
*       For example:  path = "z:\\pop" does not need z:'s current
*       directory but path = "z:pop" does.
*
*
*
*Entry:
*       _TSCHAR *buf  - pointer to a buffer maintained by the user;
*       _TSCHAR *path - path to "add" to the current directory
*       int maxlen - length of the buffer pointed to by buf
*
*Exit:
*       Returns pointer to the buffer containing the absolute path
*       (same as buf if non-NULL; otherwise, malloc is
*       used to allocate a buffer)
*
*Exceptions:
*
*******************************************************************************/

#ifdef _DEBUG

_TSCHAR * __cdecl _tfullpath (
        _TSCHAR *UserBuf,
        const _TSCHAR *path,
        size_t maxlen
        )
{
    return _tfullpath_dbg(UserBuf, path, maxlen, _NORMAL_BLOCK, NULL, 0);
}

_TSCHAR * __cdecl _tfullpath_dbg (
        _TSCHAR *UserBuf,
        const _TSCHAR *path,
        size_t maxlen,
        int nBlockUse,
        const char * szFileName,
        int nLine
        )

#else

_TSCHAR * __cdecl _tfullpath (
        _TSCHAR *UserBuf,
        const _TSCHAR *path,
        size_t maxlen
        )

#endif
{
#ifdef _WIN32_WCE
        _dosmaperr(ERROR_NOT_SUPPORTED);
        return( NULL );  
#else
        _TSCHAR *buf;
        _TSCHAR *pfname;
        unsigned long count;

        if ( !path || !*path )  /* no work to do */
            return( _tgetcwd( UserBuf, (int)maxlen ) );

        /* allocate buffer if necessary */

        if ( !UserBuf ) {
        
            count = GetFullPathName(path,0,NULL,NULL);
        
            if(count == 0) {
                _dosmaperr( GetLastError() );
                return( NULL );
            }

            maxlen = __max(maxlen, count);

            // overflow check

            if (maxlen > SIZE_MAX / sizeof(_TSCHAR)) {
               errno = EINVAL;
               return( NULL );
            }

#ifdef _DEBUG
            if ( !(buf = _calloc_dbg(maxlen, sizeof(_TSCHAR), nBlockUse, szFileName, nLine)) ) {
#else            
            if ( !(buf = calloc(maxlen, sizeof(_TSCHAR))) ) {
#endif            
                errno = ENOMEM;
                return( NULL );
            }
        }
        else
        {
            _VALIDATE_RETURN( (maxlen > 0), EINVAL, NULL);
            buf = UserBuf;
        }            

        
        count = GetFullPathName ( path,
                                  (int)maxlen,
                                  buf,
                                  &pfname );

        if ( count >= maxlen ) {
            if ( !UserBuf )
                free(buf);
            errno = ERANGE;
            return( NULL );
        }
        else if ( count == 0 ) {
            if ( !UserBuf )
                free(buf);
            _dosmaperr( GetLastError() );
            return( NULL );
        }

        return( buf );
#endif // _WIN32_WCE
}
