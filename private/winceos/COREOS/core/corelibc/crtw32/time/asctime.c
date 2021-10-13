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
*asctime.c - convert date/time structure to ASCII string
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*   Contains asctime() - convert a date/time structure to ASCII string.
*
*Revision History:
*       03-??-84  RLB   Module created
*       05-??-84  DCW   Removed use of sprintf, to avoid loading stdio
*                       functions
*       04-13-87  JCR   Added "const" to declarations
*       05-21-87  SKS   Declare the static buffer and helper routines as NEAR
*                       Replace store_year() with in-line code
*       11-24-87  WAJ   allocated a static buffer for each thread.
*       12-11-87  JCR   Added "_LOAD_DS" to declaration
*       05-24-88  PHG   Merged DLL and normal versions; Removed initializers to
*                       save memory
*       06-06-89  JCR   386 mthread support
*       03-20-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h>, removed #include <register.h>, fixed
*                       the copyright and removed some leftover 16-bit support.
*                       Also, cleaned up the formatting a bit.
*       08-16-90  SBM   Compiles cleanly with -W3
*       10-04-90  GJF   New-style function declarators.
*       07-17-91  GJF   Multi-thread support for Win32 [_WIN32_].
*       02-17-93  GJF   Changed for new _getptd().
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       11-01-93  CFW   Enable Unicode variant, rip out Cruiser.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       01-10-95  CFW   Debug CRT allocs.
*       02-09-95  GJF   Replaced WPRFLAG with _UNICODE.
*       12-12-01  BWT   Replace _getptd with _getptd_noexit and deal with error
*       03-06-03  SSM   Secure function, validation and assertion
*       04-01-05  MSL   Integer overflow protection
*       04-25-05  AC    Added debug filling
*                       VSW#2459
*
*******************************************************************************/

#include <cruntime.h>
#include <time.h>
#include <internal.h>
#include <internal_securecrt.h>
#include <mtdll.h>
#include <malloc.h>
#include <stddef.h>
#include <tchar.h>
#include <dbgint.h>

#define _ASCBUFSIZE   26
static _TSCHAR buf[_ASCBUFSIZE];

/*
** This prototype must be local to this file since the procedure is static
*/

static _TSCHAR * __cdecl store_dt(_TSCHAR *, int);

static _TSCHAR * __cdecl store_dt (
    _TSCHAR *p,
    int val
    )
{
    *p++ = (_TSCHAR)(_T('0') + val / 10);
    *p++ = (_TSCHAR)(_T('0') + val % 10);
    return(p);
}


/***
*errno_t asctime_s(buffer, sizeInChars, time) - convert a structure time to 
*ascii string
*
*Purpose:
*   Converts a time stored in a struct tm to a charcater string.
*   The string is always exactly 26 characters of the form
*       Tue May 01 02:34:55 1984\n\0
*
*Entry:
*   struct tm *time - ptr to time structure
*   _TSCHAR *buffer - ptr to output buffer
*   size_t sizeInChars - size of the buffer in characters including sapce for 
*                        NULL terminator
*
*Exit:
*   errno_t = 0 success
*       out buffer with time string.
*   errno_t = correct error code
*       out buffer NULL terminated if it is at least 1 character in size
*
*Exceptions:
*
*******************************************************************************/

errno_t __cdecl _tasctime_s (
    _TSCHAR *buffer,
    size_t sizeInChars,
    const struct tm *tb
    )
{
    _TSCHAR *p = buffer;
    int day, mon;
    int i;

    _VALIDATE_RETURN_ERRCODE(
        ( buffer != NULL ) && ( sizeInChars > 0 ),
        EINVAL
    )

    _RESET_STRING(buffer, sizeInChars);

    _VALIDATE_RETURN_ERRCODE( 
        ( sizeInChars >= _ASCBUFSIZE ),
        EINVAL
    )
    _VALIDATE_RETURN_ERRCODE(
        ( tb != NULL ),
        EINVAL
    )
    _VALIDATE_RETURN_ERRCODE(
        ( tb->tm_year >= 0 ),
        EINVAL
    )
    // month 0 based
    _VALIDATE_RETURN_ERRCODE(
        ( ( tb->tm_mon  >= 0 ) && ( tb->tm_mon  <= 11 ) ),
        EINVAL
    )
    // hour/min/sec 0 based
    _VALIDATE_RETURN_ERRCODE(
        ( ( tb->tm_hour >= 0 ) && ( tb->tm_hour <= 23 ) ),
        EINVAL
    )
    _VALIDATE_RETURN_ERRCODE(
        ( ( tb->tm_min  >= 0 ) && ( tb->tm_min  <= 59 ) ),
        EINVAL
    )
    _VALIDATE_RETURN_ERRCODE(
        ( ( tb->tm_sec  >= 0 ) && ( tb->tm_sec  <= 59 ) ),
        EINVAL
    )
    // day 1 based
    _VALIDATE_RETURN_ERRCODE(
        (
            ( tb->tm_mday >= 1 ) &&
            (
                // Day is in valid range for the month
                ( ( _days[ tb->tm_mon + 1 ] - _days[ tb->tm_mon ] ) >=
                        tb->tm_mday ) ||
                // Special case for Feb in a leap year
                (
                    ( IS_LEAP_YEAR( tb->tm_year + 1900 ) ) &&
                    ( tb->tm_mon == 1 ) &&
                    ( tb->tm_mday <= 29 )
                )
            )
        ),
        EINVAL
    )
    // week day 0 based
    _VALIDATE_RETURN_ERRCODE(
        ( ( tb->tm_wday >= 0 ) && ( tb->tm_wday <= 6 ) ),
        EINVAL
    )

#if 0
    // Review : Validate some field that the function does not access.
    // day in year 0 based

    _ASSERTE( (tb->yday >= 0 ) && 
        ( ( tb->yday <= ( IS_LEAP_YEAR( tb->tm_year ) ) ? 365 : 364 ) ) );
    if ( !( (tb->yday >= 0 ) && 
        ( ( tb->yday <= ( IS_LEAP_YEAR( tb->tm_year ) ) ? 365 : 364 ) ) ) )
    {
        errno = EINVAL;
        return errno;
    }

#endif

    /* copy day and month names into the buffer */

    day = tb->tm_wday * 3;      /* index to correct day string */
    mon = tb->tm_mon * 3;       /* index to correct month string */
    for (i=0; i < 3; i++,p++) {
        *p = *(__dnames + day + i);
        *(p+4) = *(__mnames + mon + i);
    }

    *p = _T(' ');           /* blank between day and month */

    p += 4;

    *p++ = _T(' ');
    p = store_dt(p, tb->tm_mday);   /* day of the month (1-31) */
    *p++ = _T(' ');
    p = store_dt(p, tb->tm_hour);   /* hours (0-23) */
    *p++ = _T(':');
    p = store_dt(p, tb->tm_min);    /* minutes (0-59) */
    *p++ = _T(':');
    p = store_dt(p, tb->tm_sec);    /* seconds (0-59) */
    *p++ = _T(' ');
    p = store_dt(p, 19 + (tb->tm_year/100)); /* year (after 1900) */
    p = store_dt(p, tb->tm_year%100);
    *p++ = _T('\n');
    *p = _T('\0');

#ifdef  _POSIX_
    /* Date should be padded with spaces instead of zeroes. */

    if (_T('0') == buf[8])
        buf[8] = _T(' ');
#endif

    return 0;
}

/***
*char *asctime(time) - convert a structure time to ascii string
*
*Purpose:
*   Converts a time stored in a struct tm to a charcater string.
*   The string is always exactly 26 characters of the form
*       Tue May 01 02:34:55 1984\n\0
*
*Entry:
*   struct tm *time - ptr to time structure
*
*Exit:
*   returns pointer to static string with time string.
*
*Exceptions:
*
*******************************************************************************/

_TSCHAR * __cdecl _tasctime (
    const struct tm *tb
    )
{
    _TSCHAR *p = buf;
    errno_t e = 0;

    _TSCHAR *retval;            /* holds retval pointer */
    _ptiddata ptd = _getptd_noexit();

    /* Use per thread buffer area (malloc space, if necessary) */
    if (ptd) {
#ifdef  _UNICODE
        if ( (ptd->_wasctimebuf != NULL) || ((ptd->_wasctimebuf =
            (wchar_t *)_calloc_crt(_ASCBUFSIZE, sizeof(wchar_t))) != NULL) )
            p = ptd->_wasctimebuf;
#else
        if ( (ptd->_asctimebuf != NULL) || ((ptd->_asctimebuf =
            (char *)_calloc_crt(_ASCBUFSIZE, sizeof(char))) != NULL) )
            p = ptd->_asctimebuf;
#endif
    }

    retval = p;         /* save return value for later */

    e = _tasctime_s( p, _ASCBUFSIZE, tb ); 
    if ( e != 0 )
    {
        return NULL;
    }

    return (retval);
}
