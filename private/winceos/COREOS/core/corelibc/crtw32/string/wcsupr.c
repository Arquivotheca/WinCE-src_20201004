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
*wcsupr.c - routine to map lower-case characters in a wchar_t string
*       to upper-case
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Converts all the lower case characters in a wchar_t string
*       to upper case, in place.
*
*Revision History:
*       09-09-91  ETC   Created from strupr.c and wcslwr.c
*       04-06-92  KRS   Make work without _INTL also.
*       08-19-92  KRS   Activate NLS support.
*       08-22-92  SRW   Allow INTL definition to be conditional for building ntcrt.lib
*       09-02-92  SRW   Get _INTL definition via ..\crt32.def
*       02-16-93  CFW   Optimize test for lowercase in "C" locale.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       06-02-93  SRW   ignore _INTL if _NTSUBSET_ defined.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       09-16-93  GJF   Merged NT SDK and Cuda versions.
*       09-22-93  CFW   Use __crtxxx internal NLS API wrapper.
*       11-09-93  CFW   Add code page for __crtxxx().
*       02-07-94  CFW   POSIXify.
*       09-06-94  CFW   Remove _INTL switch.
*       10-25-94  GJF   Sped up C locale, multi-thread case.
*       01-10-95  CFW   Debug CRT allocs.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       08-17-98  GJF   Revised multithread support based on threadlocinfo
*                       struct. Also, use _alloca instead of _malloc_crt if
*                       possible.
*       10-19-98  GJF   Major typo, = should have been ==
*       12-10-99  GB    Added support for recovery from stack overflow around 
*                       _alloca().
*       11-12-01  GB    Added support for new locale implementation.
*       10-09-03  AC    Added secure version.
*       03-10-04  AC    Fixed validation.
*       03-11-04  AC    Return ERANGE when buffer is too small
*       03-24-04  MSL   Add missing validation
*                       VSW#231762
*       09-25-04  JL    Replace usage of _alloca() with _alloca_s() / _freea_s()
*       10-08-04  AGH   Added validations to _wcsupr
*       04-04-05  JL    Replace _alloca_s and _freea_s with _malloca and _freea
*       04-01-05  MSL   Integer overflow protection
*       04-25-05  AC    Added debug filling
*                       VSW#2459
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <string.h>
#include <malloc.h>
#include <locale.h>
#include <ctype.h>
#include <awint.h>
#include <dbgint.h>
#include <internal.h>
#include <internal_securecrt.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*wchar_t *_wcsupr(string) - map lower-case characters in a string to upper-case
*
*Purpose:
*       wcsupr converts lower-case characters in a null-terminated wchar_t 
*       string to their upper-case equivalents.  The result may be longer or
*       shorter than the original string.  Assumes enough space in string
*       to hold the result.
*
*Entry:
*       wchar_t *wsrc - wchar_t string to change to upper case
*
*Exit:
*       input string address
*
*Exceptions:
*       on an error, the original string is unaltered, and errno is set
*
*******************************************************************************/

extern "C" wchar_t * __cdecl _wcsupr_l (
        wchar_t * wsrc,
        _locale_t plocinfo
        )
{
    _wcsupr_s_l(wsrc, (size_t)(-1), plocinfo);
    return wsrc;
}

extern "C" wchar_t * __cdecl _wcsupr (
        wchar_t * wsrc
        )
{
#ifndef _NTSUBSET_
    if (__locale_changed == 0)
    {
#endif
    wchar_t * p;

#ifndef _NTSUBSET_
    /* validation section */
    _VALIDATE_RETURN(wsrc != NULL, EINVAL, NULL);
#endif

    for (p=wsrc; *p; ++p)
    {
            if (L'a' <= *p && *p <= L'z')
                    *p += (wchar_t)(L'A' - L'a');
    }

        return wsrc;
#ifndef _NTSUBSET_
    }
    else
    {
        _wcsupr_s_l(wsrc, (size_t)-1, NULL);
        return wsrc;
    }
#endif
}

/***
*errno_t _wcsupr_s(string, size_t) - map lower-case characters in a string to upper-case
*
*Purpose:
*       wcsupr converts lower-case characters in a null-terminated wchar_t 
*       string to their upper-case equivalents.  The result may be longer or
*       shorter than the original string.
*
*Entry:
*       wchar_t *wsrc - wchar_t string to change to upper case
*       size_t sizeInWords - size of the destination buffer
*
*Exit:
*       the error code
*
*Exceptions:
*       on an error, the original string is unaltered, and errno is set
*
*******************************************************************************/

static errno_t __cdecl _wcsupr_s_l_stat (
        wchar_t * wsrc,
        size_t sizeInWords,
        _locale_t plocinfo
        )
{

    wchar_t *p;             /* traverses string for C locale conversion */
    wchar_t *wdst;          /* wide version of string in alternate case */
    int dstsize;            /* size in wide chars of wdst string buffer (include null) */
    errno_t e = 0;
    size_t stringlen;

    /* validation section */
    _VALIDATE_RETURN_ERRCODE(wsrc != NULL, EINVAL);
    stringlen = wcsnlen(wsrc, sizeInWords);
    if (stringlen >= sizeInWords)
    {
        _RESET_STRING(wsrc, sizeInWords);
        _RETURN_DEST_NOT_NULL_TERMINATED(wsrc, sizeInWords);
    }
    _FILL_STRING(wsrc, sizeInWords, stringlen + 1);

    if ( plocinfo->locinfo->locale_name[LC_CTYPE] == NULL )
    {
        for ( p = wsrc ; *p ; p++ )
        {
            if ( (*p >= (wchar_t)L'a') && (*p <= (wchar_t)L'z') )
                *p = *p - (L'a' - L'A');
        }
        return 0;
    }   /* C locale */


    /* Inquire size of wdst string */
    if ( (dstsize = __crtLCMapStringW(
                    plocinfo->locinfo->locale_name[LC_CTYPE],
                    LCMAP_UPPERCASE,
                    wsrc,
                    -1,
                    NULL,
                    0 )) == 0 )
    {
        errno = EILSEQ;
        return errno;
    }

    if (sizeInWords < (size_t)dstsize)
    {
        _RESET_STRING(wsrc, sizeInWords);
        _RETURN_BUFFER_TOO_SMALL(wsrc, sizeInWords);
    }

    /* Allocate space for wdst */
    wdst = (wchar_t *)_calloca(dstsize, sizeof(wchar_t));
    if (wdst == NULL)
    {
        errno = ENOMEM;
        return errno;
    }

    /* Map wrc string to wide-character wdst string in alternate case */
    if (__crtLCMapStringW(
                plocinfo->locinfo->locale_name[LC_CTYPE],
                LCMAP_UPPERCASE,
                wsrc,
                -1, 
                wdst, 
                dstsize ) != 0)
    {
        /* Copy wdst string to user string */
        e = wcscpy_s(wsrc, sizeInWords, wdst);
    }
    else
    {
        e = errno = EILSEQ;
    }

    _freea(wdst);

    return e;
}

extern "C" errno_t __cdecl _wcsupr_s_l (
        wchar_t * wsrc,
        size_t sizeInWords,
        _locale_t plocinfo
        )
{
    _LocaleUpdate _loc_update(plocinfo);
    
    return _wcsupr_s_l_stat(wsrc, sizeInWords, _loc_update.GetLocaleT());
}


extern "C" errno_t __cdecl _wcsupr_s (
        wchar_t * wsrc,
        size_t sizeInWords
        )
{
    return _wcsupr_s_l(wsrc, sizeInWords, NULL);
}
#endif /* _POSIX_ */
