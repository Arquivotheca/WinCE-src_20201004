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
*strlwr.c - routine to map upper-case characters in a string to lower-case
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Converts all the upper case characters in a string to lower case,
*       in place.
*
*Revision History:
*       05-31-89  JCR   C version created.
*       02-27-90  GJF   Fixed calling type, #include <cruntime.h>, fixed
*                       copyright.
*       10-02-90  GJF   New-style function declarator.
*       01-18-91  GJF   ANSI naming.
*       09-18-91  ETC   Locale support under _INTL switch.
*       12-08-91  ETC   Updated nlsapi; added multithread.
*       08-19-92  KRS   Activated NLS support.
*       08-22-92  SRW   Allow INTL definition to be conditional for building
*                       ntcrt.lib
*       09-02-92  SRW   Get _INTL definition via ..\crt32.def
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       06-01-93  CFW   Simplify "C" locale test.
*       06-02-93  SRW   ignore _INTL if _NTSUBSET_ defined.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       09-16-93  GJF   Merged NT SDK and Cuda versions.
*       09-22-93  CFW   Use __crtxxx internal NLS API wrapper.
*       11-09-93  CFW   Add code page for __crtxxx().
*       09-06-94  CFW   Remove _INTL switch.
*       10-24-94  GJF   Sped up C locale, multi-thread case.
*       12-29-94  CFW   Merge non-Win32.
*       01-10-95  CFW   Debug CRT allocs.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       03-17-97  RDK   Added error flag to __crtLCMapStringA.
*       08-11-98  GJF   Revised multithread support based on threadlocinfo
*                       struct. Also, use _alloca instead of _malloc_crt.
*       05-17-99  PML   Remove all Macintosh support.
*       12-10-99  GB    Added support for recovery from stack overflow around
*                       _alloca().
*       05-01-00  BWT   Fix Posix.
*       03-13-01  PML   Fix memory leak if _alloca failed (vs7#224860)
*       11-12-01  GB    Added support for new locale implementation.
*       10-09-03  AC    Added secure version.
*       03-10-04  AC    Fixed validation.
*       03-11-04  AC    Return ERANGE when buffer is too small
*       09-25-04  JL    Replace usage of _alloca() with _alloca_s() / _freea_s()
*       10-08-04  AGH   Added validations to strlwr
*       04-04-05  JL    Replace _alloca_s and _freea_s with _malloca and _freea
*       04-01-05  MSL   Integer overflow protection
*       04-25-05  AC    Added debug filling
*                       VSW#2459
*
*******************************************************************************/

#include <cruntime.h>
#include <string.h>
#include <malloc.h>
#include <locale.h>
#include <limits.h>     /* for INT_MAX */
#include <awint.h>
#include <dbgint.h>
#include <internal.h>
#include <internal_securecrt.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*char *_strlwr(string) - map upper-case characters in a string to lower-case
*
*Purpose:
*       _strlwr() converts upper-case characters in a null-terminated string
*       to their lower-case equivalents.  Conversion is done in place and
*       characters other than upper-case letters are not modified.
*
*       In the C locale, this function modifies only 7-bit ASCII characters
*       in the range 0x41 through 0x5A ('A' through 'Z').
*
*       If the locale is not the 'C' locale, LCMapString() is used to do
*       the work.  Assumes enough space in the string to hold result.
*
*Entry:
*       char *string - string to change to lower case
*
*Exit:
*       input string address
*
*Exceptions:
*       The original string is returned unchanged on any error, and errno is set.
*
*******************************************************************************/

extern "C" char * __cdecl _strlwr_l (
        char * string,
        _locale_t plocinfo
        )
{
    _strlwr_s_l(string, (size_t)(-1), plocinfo);
    return string;
}

extern "C" char * __cdecl _strlwr (
        char * string
        )
{
#if     !defined(_NTSUBSET_) && !defined(_POSIX_)
    if (__locale_changed == 0)
    {
        char * cp;

        /* validation section */
        _VALIDATE_RETURN(string != NULL, EINVAL, NULL);

        for (cp=string; *cp; ++cp)
        {
            if ('A' <= *cp && *cp <= 'Z')
                *cp += 'a' - 'A';
        }

        return(string);
    }
    else
    {
        _strlwr_s_l(string, (size_t)(-1), NULL);
        return string;
    }
#else

    char * cp;

    for (cp=string; *cp; ++cp)
    {
        if ('A' <= *cp && *cp <= 'Z')
            *cp += 'a' - 'A';
    }

    return(string);

#endif
}

/***
*errno_t _strlwr_s(string, size_t) - map upper-case characters in a string to lower-case
*
*Purpose:
*       _strlwr_s() converts upper-case characters in a null-terminated string
*       to their lower-case equivalents.  Conversion is done in place and
*       characters other than upper-case letters are not modified.
*
*       In the C locale, this function modifies only 7-bit ASCII characters
*       in the range 0x41 through 0x5A ('A' through 'Z').
*
*       If the locale is not the 'C' locale, LCMapString() is used to do
*       the work.  Assumes enough space in the string to hold result.
*
*Entry:
*       char *string - string to change to lower case
*       size_t sizeInBytes - size of the destination buffer
*
*Exit:
*       the error code
*
*Exceptions:
*       The original string is returned unchanged on any error, and errno is set.
*
*******************************************************************************/

errno_t __cdecl _strlwr_s_l_stat (
        char * string,
        size_t sizeInBytes,
        _locale_t plocinfo
        )
{

    int dstsize;                /* size of dst string buffer (include null)  */
    unsigned char *dst;         /* destination string */
    errno_t e = 0;
    size_t stringlen;

    /* validation section */
    _VALIDATE_RETURN_ERRCODE(string != NULL, EINVAL);
    stringlen = strnlen(string, sizeInBytes);
    if (stringlen >= sizeInBytes)
    {
        _RESET_STRING(string, sizeInBytes);
        _RETURN_DEST_NOT_NULL_TERMINATED(string, sizeInBytes);
    }
    _FILL_STRING(string, sizeInBytes, stringlen + 1);

    if ( plocinfo->locinfo->locale_name[LC_CTYPE] == NULL ) {
        char *cp;       /* traverses string for C locale conversion */

        for ( cp = string ; *cp ; ++cp )
            if ( ('A' <= *cp) && (*cp <= 'Z') )
                *cp -= 'A' - 'a';

        return 0;
    }   /* C locale */

    /* Inquire size of dst string */
    if ( 0 == (dstsize = __crtLCMapStringA(
                    plocinfo,
                    plocinfo->locinfo->locale_name[LC_CTYPE],
                    LCMAP_LOWERCASE,
                    string,
                    -1,
                    NULL,
                    0,
                    plocinfo->locinfo->lc_codepage,
                    TRUE )) )
    {
        errno = EILSEQ;
        return errno;
    }

    if (sizeInBytes < (size_t)dstsize)
    {
        _RESET_STRING(string, sizeInBytes);
        _RETURN_BUFFER_TOO_SMALL(string, sizeInBytes);
    }

    /* Allocate space for dst */
    dst = (unsigned char *)_calloca(dstsize, sizeof(unsigned char));
    if (dst == NULL)
    {
        errno = ENOMEM;
        return errno;
    }

    /* Map src string to dst string in alternate case */
    if (__crtLCMapStringA(
                plocinfo,
                plocinfo->locinfo->locale_name[LC_CTYPE],
                LCMAP_LOWERCASE,
                string,
                -1,
                (LPSTR)dst,
                dstsize,
                plocinfo->locinfo->lc_codepage,
                TRUE ) != 0)
    {
        /* copy dst string to return string */
        e = strcpy_s(string, sizeInBytes, (const char *)dst);
    }
    else
    {
        e = errno = EILSEQ;
    }

    _freea(dst);

    return e;
}

extern "C" errno_t __cdecl _strlwr_s_l (
        char * string,
        size_t sizeInBytes,
        _locale_t plocinfo
        )
{

    _LocaleUpdate _loc_update(plocinfo);

    return _strlwr_s_l_stat(string, sizeInBytes, _loc_update.GetLocaleT());
}

extern "C" errno_t __cdecl _strlwr_s(
        char * string,
        size_t sizeInBytes
        )
{
    return _strlwr_s_l(string, sizeInBytes, NULL);
}
