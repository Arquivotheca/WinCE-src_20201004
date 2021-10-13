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
*wctomb.c - Convert wide character to multibyte character.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Convert a wide character into the equivalent multibyte character.
*
*Revision History:
*       03-19-90  KRS   Module created.
*       12-20-90  KRS   Include ctype.h.
*       01-14-91  KRS   Fix argument error: wchar is pass-by-value.
*       03-20-91  KRS   Ported from 16-bit tree.
*       07-23-91  KRS   Hard-coded for "C" locale to avoid bogus interim #'s.
*       10-15-91  ETC   Locale support under _INTL (finally!).
*       12-09-91  ETC   Updated nlsapi; added multithread.
*       08-20-92  KRS   Activated NLSAPI support.
*       08-22-92  SRW   Allow INTL definition to be conditional for building ntcrt.lib
*       09-02-92  SRW   Get _INTL definition via ..\crt32.def
*       04-06-93  SKS   Replace _CRTAPI* with _cdecl
*       05-04-93  CFW   Kinder, gentler error handling.
*       06-01-93  CFW   Minor optimization and beautify.
*       06-02-93  SRW   ignore _INTL if _NTSUBSET_ defined.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       09-28-93  GJF   Merged NT SDK and Cuda versions.
*       01-14-94  SRW   if _NTSUBSET_ defined call Rtl functions
*       02-07-94  CFW   POSIXify.
*       09-06-94  CFW   Remove _INTL switch.
*       01-07-95  CFW   Mac merge cleanup.
*       04-19-95  CFW   Rearrange & fix non-Win32 version.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       12-07-95  SKS   Fix misspelling of _NTSUBSET_ (final _ was missing)
*       04-01-96  BWT   POSIX work.
*       06-25-96  GJF   Removed DLL_FOR_WIN32S. Replaced defined(_WIN32) with
*                       !defined(_MAC). Polished the format a bit.
*       07-22-98  GJF   Revised multithread support based on threadlocinfo
*                       struct.
*       04-28-99  GJF   Changed dwFlags arg value to 0 in WideCharToMultiByte
*                       calls to avoid problems with codepage 1258 on NT 5.0.
*       05-17-99  PML   Remove all Macintosh support.
*       11-12-01  GB    Added support for new locale implementation.
*       03-04-04  AC    Fixed validation.
*       03-29-04  AC    Changed EINVAL to ERANGE.
*       04-07-04  AC    Fixed the way we check the error returned by WideCharToMultiByte.
*                       VSW#246776
*
*******************************************************************************/

#if     defined(_NTSUBSET_) || defined(_POSIX_)
#include <ntrtl.h>
#include <nturtl.h>
#endif

#include <cruntime.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <internal.h>
#include <limits.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*errno_t wctomb_s() - Convert wide character to multibyte character.
*
*Purpose:
*       Convert a wide character into the equivalent multi-byte character,
*       according to the LC_CTYPE category of the current locale.
*       [ANSI].
*
*       NOTE:  Currently, the C libraries support the "C" locale only.
*              Non-C locale support now available under _INTL switch.
*Entry:
*       int *retvalue = pointer to a useful return value:
*           if s == NULL && sizeInBytes == 0: number of bytes needed for the conversion
*           if s == NULL && sizeInBytes > 0: the state information
*           if s != NULL : number of bytes used for the conversion
*           The pointer can be null.
*       char *s = pointer to multibyte character
*       size_t sizeInBytes = size of the destination buffer
*       wchar_t wchar = source wide character
*
*Exit:
*       The error code.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" int __cdecl _wctomb_s_l (
        int *pRetValue,
        char *dst,
        size_t sizeInBytes,
        wchar_t wchar,
        _locale_t plocinfo
        )
{
    if (dst == NULL && sizeInBytes > 0)
    {
        /* indicate do not have state-dependent encodings */
        if (pRetValue != NULL)
        {
            *pRetValue = 0;
        }
        return 0;
    }

    if (pRetValue != NULL)
    {
        *pRetValue = -1;
    }

    /* validation section */
    /* we need to cast sizeInBytes to int, so we make sure we are not going to truncate sizeInBytes */
    _VALIDATE_RETURN_ERRCODE(sizeInBytes <= INT_MAX, EINVAL);

#if     defined(_NTSUBSET_) || defined(_POSIX_)
    if (dst == NULL)
    {
        /* sizeInBytes == 0, i.e. we return the size of the buffer */
        if (pRetValue != NULL)
        {
            *pRetValue = MB_CUR_MAX;
        }
        return 0;
    }
    else
    {
        NTSTATUS Status;
        int size;

        Status = RtlUnicodeToMultiByteN( dst, 
                                         (int)sizeInBytes, 
                                         (PULONG)&size, 
                                         &wchar, 
                                         sizeof( wchar )
                                         );

        if (!NT_SUCCESS(Status))
        {
            errno = EILSEQ;
            return errno;
        }
        if (pRetValue != NULL)
        {
            *pRetValue = size;
        }
        return 0;
    }

#else   /* _NTSUBSET_/_POSIX_ */

    _LocaleUpdate _loc_update(plocinfo);

    if ( _loc_update.GetLocaleT()->locinfo->locale_name[LC_CTYPE] == NULL )
    {
        if ( wchar > 255 )  /* validate high byte */
        {
            if (dst != NULL && sizeInBytes > 0)
            {
                memset(dst, 0, sizeInBytes);
            }
#ifndef _WCE_BOOTCRT
            errno = EILSEQ;
#endif
            return EILSEQ;
        }

        if (dst != NULL)
        {
            _VALIDATE_RETURN_ERRCODE(sizeInBytes > 0, ERANGE);
            *dst = (char) wchar;
        }
        if (pRetValue != NULL)
        {
            *pRetValue = 1;
        }
        return 0;
    }
    else
    {
        int size;
        BOOL defused = 0;

        if ( ((size = WideCharToMultiByte( _loc_update.GetLocaleT()->locinfo->lc_codepage,
                                           0,
                                           &wchar,
                                           1,
                                           dst,
                                           (int)sizeInBytes,
                                           NULL,
                                           &defused) ) == 0) || 
             (defused) )
        {
#ifndef _WCE_BOOTCRT
            if (size == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                if (dst != NULL && sizeInBytes > 0)
                {
                    memset(dst, 0, sizeInBytes);
                }
                _VALIDATE_RETURN_ERRCODE(("Buffer too small", 0), ERANGE);
            }
            errno = EILSEQ;
#endif
            return EILSEQ;
        }

        if (pRetValue != NULL)
        {
            *pRetValue = size;
        }
        return 0;
    }

#endif  /* ! _NTSUBSET_/_POSIX_ */
}

extern "C" errno_t __cdecl wctomb_s (
        int *pRetValue,
        char *dst,
        size_t sizeInBytes,
        wchar_t wchar
        )
{
    return _wctomb_s_l(pRetValue, dst, sizeInBytes, wchar, NULL);
}

/***
*int wctomb() - Convert wide character to multibyte character.
*
*Purpose:
*       Convert a wide character into the equivalent multi-byte character,
*       according to the LC_CTYPE category of the current locale.
*       [ANSI].
*
*       NOTE:  Currently, the C libraries support the "C" locale only.
*              Non-C locale support now available under _INTL switch.
*Entry:
*       char *s          = pointer to multibyte character
*       wchar_t wchar        = source wide character
*
*Exit:
*       If s = NULL, returns 0, indicating we only use state-independent
*       character encodings.
*       If s != NULL, returns:
*                   -1 (if error) or number of bytes comprising
*                   converted mbc
*
*Exceptions:
*
*******************************************************************************/

extern "C" int __cdecl _wctomb_l (
        char *s,
        wchar_t wchar,
        _locale_t plocinfo
        )
{
    int retval = -1;
    errno_t e;
    _LocaleUpdate _loc_update(plocinfo);

    e = _wctomb_s_l(&retval, s, _loc_update.GetLocaleT()->locinfo->mb_cur_max, wchar, _loc_update.GetLocaleT());
    return (e == 0 ? retval : -1);
}

extern "C" int __cdecl wctomb (
        char *s,
        wchar_t wchar
        )
{
    int retval = -1;
    errno_t e;

    e = _wctomb_s_l(&retval, s, MB_CUR_MAX, wchar, NULL);
    return (e == 0 ? retval : -1);
}
