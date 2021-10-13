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
*ctime.c - convert time argument to a string
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       contains _ctime32() - convert time value to string
*
*Revision History:
*       03-??-84  RLB   initial version
*       05-??-84  DFW   split off into seperate module
*       02-18-87  JCR   put in NULL ptr support
*       04-10-87  JCR   changed long declaration ot time_t and added const.
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       03-20-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h> and fixed the copyright. Also, cleaned
*                       up the formatting a bit.
*       05-21-90  GJF   Fixed compiler warning.
*       10-04-90  GJF   New-style function declarators.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       11-01-93  CFW   Enable Unicode variant, rip out Cruiser.
*       08-30-99  PML   Fix function header comment, detab.
*       02-03-03  EAN   changed to _ctime32 as part of 64-bit time_t change 
*                       (VSWhidbey 6851)
*       03-06-03  SSM   Assertions, validations and secure version
*       04-25-05  AC    Added debug filling
*                       VSW#2459
*
*******************************************************************************/

#include <cruntime.h>
#include <time.h>
#include <internal.h>
#include <internal_securecrt.h>
#include <stddef.h>
#include <tchar.h>

/***
*errno_t _tctime32_s(buffer, sizeInChars, time) - converts a time stored as a __time32_t
*to a string
*
*Purpose:
*       Converts a time stored as a __time32_t to a string of the form:
*              Tue May 01 14:25:03 1984
*
*Entry:
*       __time32_t *time - time value in XENIX format
*
*Exit:
*       errno_t = 0 success
*                 buffer contains time converted to a string
*       errno_t = correct error code
*                 buffer null terminated if it is at least 1 character in size
*
*Exceptions:
*
*******************************************************************************/

errno_t __cdecl _tctime32_s (
        _TSCHAR * buffer,
        size_t sizeInChars,
        const __time32_t *timp
        )
{
        struct tm tmtemp;
        errno_t e;

        _VALIDATE_RETURN_ERRCODE(
            ( ( buffer != NULL ) && ( sizeInChars > 0 ) ),
            EINVAL
        )

        _RESET_STRING(buffer, sizeInChars);

        _VALIDATE_RETURN_ERRCODE( ( timp != NULL ), EINVAL )
        _VALIDATE_RETURN_ERRCODE_NOEXC( ( *timp >= 0 ), EINVAL )

        e = _localtime32_s(&tmtemp, timp);
        if ( e == 0 )
        {
            e = _tasctime_s(buffer, sizeInChars, &tmtemp);
        }
        return e;
}

/***
*_TSCHAR *_tctime32(time) - converts a time stored as a __time32_t to a string
*
*Purpose:
*       Converts a time stored as a __time32_t to a string of the form:
*              Tue May 01 14:25:03 1984
*
*Entry:
*       __time32_t *time - time value in XENIX format
*
*Exit:
*       returns pointer to static string or NULL if time is before
*       Jan 1 1980.
*
*Exceptions:
*
*******************************************************************************/

_TSCHAR * __cdecl _tctime32 (
        const __time32_t *timp
        )
{
        struct tm tmtemp;
        errno_t e;

        _VALIDATE_RETURN( ( timp != NULL ), EINVAL, NULL )
        _VALIDATE_RETURN_NOEXC( ( *timp >= 0 ), EINVAL, NULL )

        e = _localtime32_s(&tmtemp, timp);
        if ( e == 0 )
        {
_BEGIN_SECURE_CRT_DEPRECATION_DISABLE
            return(_tasctime(&tmtemp));
_END_SECURE_CRT_DEPRECATION_DISABLE
        }
        else
        {
            return(NULL);
        }
}
