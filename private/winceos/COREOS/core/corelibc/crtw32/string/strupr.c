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
*strupr.c - routine to map lower-case characters in a string to upper-case
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Converts all the lower case characters in a string to upper case,
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
*       08-19-92  KRS   Activated NLS Support.
*       08-22-92  SRW   Allow INTL definition to be conditional for building
*                       ntcrt.lib
*       09-02-92  SRW   Get _INTL definition via ..\crt32.def
*       03-10-93  CFW   Remove UNDONE comment.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       06-01-93  CFW   Simplify "C" locale test.
*       06-02-93  SRW   ignore _INTL if _NTSUBSET_ defined.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       09-16-93  GJF   Merged NT SDK and Cuda versions.
*       09-22-93  CFW   Use __crtxxx internal NLS API wrapper.
*       10-07-93  CFW   Fix macro name.
*       11-09-93  CFW   Add code page for __crtxxx().
*       09-06-94  CFW   Remove _INTL switch.
*       10-24-94  GJF   Sped up C locale, multi-thread case.
*       12-29-94  CFW   Merge non-Win32.
*       01-10-95  CFW   Debug CRT allocs.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       03-17-97  RDK   Added error flag to __crtLCMapStringA.
*       08-12-98  GJF   Revised multithread support based on threadlocinfo
*                       struct. Also, use _alloca instead of _malloc_crt.
*       05-17-99  PML   Remove all Macintosh support.
*       12-10-99  GB    Added support for recovery from stack overflow around
*                       _alloca().
*       05-01-00  BWT   Fix Posix.
*       03-13-01  PML   Pass per-thread cp to __crtLCMapStringA (vs7#224974).
*       11-12-01  GB    Added support for new locale implementation.
*       10-09-03  AC    Added secure version.
*       03-10-04  AC    Fixed validation.
*       03-11-04  AC    Return ERANGE when buffer is too small
*       03-24-04  MSL   Fixed c locale case
*                       VSW#248040
*                       VSW#248033
*                       VSW#231824
*       09-25-04  JL    Replace usage of _alloca() with _alloca_s() / _freea_s()
*       10-08-04  AGH   Added validations to _strupr
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
*char *_strupr(string) - map lower-case characters in a string to upper-case
*
*Purpose:
*       _strupr() converts lower-case characters in a null-terminated string
*       to their upper-case equivalents.  Conversion is done in place and
*       characters other than lower-case letters are not modified.
*
*       In the C locale, this function modifies only 7-bit ASCII characters
*       in the range 0x61 through 0x7A ('a' through 'z').
*
*       If the locale is not the 'C' locale, LCMapString() is used to do
*       the work.  Assumes enough space in the string to hold result.
*
*Entry:
*       char *string - string to change to upper case
*
*Exit:
*       input string address
*
*Exceptions:
*       The original string is returned unchanged on any error, and errno is set.
*
*******************************************************************************/

extern "C" char * __cdecl _strupr_l (
        char * string,
        _locale_t plocinfo
        )
{
    _strupr_s_l(string, (size_t)(-1), plocinfo);
    return (string);
}

extern "C" char * __cdecl _strupr (
        char * string
        )
{
    if (__locale_changed == 0)
    {
        /* validation section */
        _VALIDATE_RETURN(string != NULL, EINVAL, NULL);

        char *cp;       /* traverses string for C locale conversion */

        for ( cp = string ; *cp ; ++cp )
            if ( ('a' <= *cp) && (*cp <= 'z') )
                *cp -= 'a' - 'A';

        return(string);
    }
    else
    {
        _strupr_s_l(string, (size_t)(-1), NULL);
        return (string);
    }
}

/***
*errno_t _strupr_s(string, size_t) - map lower-case characters in a string to upper-case
*
*Purpose:
*       _strupr() converts lower-case characters in a null-terminated string
*       to their upper-case equivalents.  Conversion is done in place and
*       characters other than lower-case letters are not modified.
*
*       In the C locale, this function modifies only 7-bit ASCII characters
*       in the range 0x61 through 0x7A ('a' through 'z').
*
*       If the locale is not the 'C' locale, LCMapString() is used to do
*       the work.  Assumes enough space in the string to hold result.
*
*Entry:
*       char *string - string to change to upper case
*       size_t sizeInBytes - size of the destination buffer
*
*Exit:
*       the error code
*
*Exceptions:
*       The original string is returned unchanged on any error, and errno is set.
*
*******************************************************************************/

static errno_t __cdecl _strupr_s_l_stat (
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

    if ( plocinfo->locinfo->locale_name[LC_CTYPE] == NULL ) 
    {
        char *cp=string;       /* traverses string for C locale conversion */

        for ( ; *cp ; ++cp )
        {
            if ( ('a' <= *cp) && (*cp <= 'z') )
            {
                *cp -= 'a' - 'A';
            }
        }

        return 0;
    }   /* C locale */

    /* Inquire size of dst string */
    if ( 0 == (dstsize = __crtLCMapStringA(
                    plocinfo,
                    plocinfo->locinfo->locale_name[LC_CTYPE],
                    LCMAP_UPPERCASE,
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
                LCMAP_UPPERCASE,
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

extern "C" errno_t __cdecl _strupr_s_l (
        char * string,
        size_t sizeInBytes,
        _locale_t plocinfo
        )
{
    _LocaleUpdate _loc_update(plocinfo);

    return _strupr_s_l_stat(string, sizeInBytes, _loc_update.GetLocaleT());
}

extern "C" errno_t __cdecl _strupr_s (
        char * string,
        size_t sizeInBytes
        )
{
    return _strupr_s_l(string, sizeInBytes, NULL);
}
