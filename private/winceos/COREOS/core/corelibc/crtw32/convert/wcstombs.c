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
*wcstombs.c - Convert wide char string to multibyte char string.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Convert a wide char string into the equivalent multibyte char string.
*
*Revision History:
*       08-24-90  KRS   Module created.
*       01-14-91  KRS   Added _WINSTATIC for Windows DLL.  Fix wctomb() call.
*       03-18-91  KRS   Fix check for NUL.
*       03-20-91  KRS   Ported from 16-bit tree.
*       10-16-91  ETC   Locale support under _INTL switch.
*       12-09-91  ETC   Updated nlsapi; added multithread.
*       08-20-92  KRS   Activated NLSAPI support.
*       08-22-92  SRW   Allow INTL definition to be conditional for building ntcrt.lib
*       09-02-92  SRW   Get _INTL definition via ..\crt32.def
*       01-06-93  CFW   Added (count < n) to outer loop - avoid bad wctomb calls
*       01-07-93  KRS   Major code cleanup.  Fix error return, comments.
*       04-06-93  SKS   Replace _CRTAPI* with _cdecl
*       05-03-93  CFW   Return pointer == NULL, return size, plus massive cleanup.
*       06-01-93  CFW   Minor optimization and beautify.
*       06-02-93  SRW   ignore _INTL if _NTSUBSET_ defined.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       09-28-93  GJF   Merged NT SDK and Cuda versions.
*       01-14-94  SRW   if _NTSUBSET_ defined call Rtl functions
*       02-07-94  CFW   POSIXify.
*       08-03-94  CFW   Optimize for SBCS.
*       09-06-94  CFW   Remove _INTL switch.
*       11-22-94  CFW   WideCharToMultiByte will compare past NULL.
*       01-07-95  CFW   Mac merge cleanup.
*       02-06-95  CFW   assert -> _ASSERTE.
*       03-13-95  CFW   Fix wcsncnt counting.
*       04-19-95  CFW   Rearrange & fix non-Win32 version.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       12-07-95  SKS   Fix misspelling of _NTSUBSET_ (final _ was missing)
*       04-01-96  BWT   POSIX work.
*       06-25-96  GJF   Removed DLL_FOR_WIN32S. Replaced defined(_WIN32) with
*                       !defined(_MAC). Polished the format a bit.
*       02-27-98  RKP   Added 64 bit support.
*       06-23-98  GJF   Revised multithread support based on threadlocinfo
*                       struct.
*       08-27-98  GJF   Introduced __wcstombs_mt.
*       12-15-98  GJF   Changes for 64-bit size_t.
*       04-28-99  GJF   Changed dwFlags arg value to 0 in WideCharToMultiByte
*                       calls to avoid problems with codepage 1258 on NT 5.0.
*       05-17-99  PML   Remove all Macintosh support.
*       11-12-01  GB    Added support for new locale implementation.
*       10-14-03  AC    Added secure version.
*       03-04-04  AC    Fixed validation.
*       09-08-04  AC    Added truncate behavior.
*       10-31-04  AC    Prefast fixes
*                       VSW#373224
*       03-23-05  MSL   Review comment clean up - moved to product studio
*       04-12-05  PAL   Remove calls to deprecated/insecure API's.
*       04-25-05  AC    Added debug filling
*                       VSW#2459
*
*******************************************************************************/

#if     defined(_NTSUBSET_) || defined(_POSIX_)
#include <ntrtl.h>
#include <nturtl.h>
#endif

#include <cruntime.h>
#include <stdlib.h>
#include <limits.h>
#include <internal.h>
#include <internal_securecrt.h>
#include <dbgint.h>
#include <errno.h>
#include <locale.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*size_t __cdecl wcsncnt - count wide characters in a string, up to n.
*
*Purpose:
*       Internal local support function. Counts characters in string including NULL.
*       If NULL not found in n chars, then return n.
*
*Entry:
*       const wchar_t *string   - start of string
*       size_t n                - character count
*
*Exit:
*       returns number of wide characters from start of string to
*       NULL (inclusive), up to n.
*
*Exceptions:
*
*******************************************************************************/

static size_t __cdecl wcsncnt (
        const wchar_t *string,
        size_t cnt
        )
{
        size_t n = cnt+1;
        wchar_t *cp = (wchar_t *)string;

        while (--n && *cp)
            cp++;

        if (n && !*cp)
            return cp - string + 1;
        return cnt;
}

/***
*size_t wcstombs() - Convert wide char string to multibyte char string.
*
*Purpose:
*       Convert a wide char string into the equivalent multibyte char string,
*       according to the LC_CTYPE category of the current locale.
*       [ANSI].
*
*       The destination string is null terminated only if the null terminator
*       is copied from the source string.
*
*Entry:
*       char *s            = pointer to destination multibyte char string
*       const wchar_t *pwc = pointer to source wide character string
*       size_t           n = maximum number of bytes to store in s
*
*Exit:
*       If s != NULL, returns    (size_t)-1 (if a wchar cannot be converted)
*       Otherwise:       Number of bytes modified (<=n), not including
*                    the terminating NUL, if any.
*
*Exceptions:
*       Returns (size_t)-1 if an error is encountered.
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" size_t __cdecl _wcstombs_l_helper (
        char * s,
        const wchar_t * pwcs,
        size_t n,
        _locale_t plocinfo
        )
{
    size_t count = 0;
    BOOL defused = 0;

    if (s && n == 0)
        /* dest string exists, but 0 bytes converted */
        return 0;

    /* validation section */
    _VALIDATE_RETURN(pwcs != NULL, EINVAL, (size_t)-1);

#if     !defined( _NTSUBSET_ ) && !defined(_POSIX_)

    /* if destination string exists, fill it in */

    _LocaleUpdate _loc_update(plocinfo);

    if (s)
    {
        if ( _loc_update.GetLocaleT()->locinfo->locale_name[LC_CTYPE] == NULL )
        {
            /* C locale: easy and fast */
            /* Actually, there are such wchar_t characters which are > 255, 
             * but they can be transformed to a valid single byte char
             * (i.e. a char in the C locale case). Like, for example, 
             * alternative digits in unicode like Arabic-Indic U+0660..U+0669.
             * The problem is that WideCharToMultiByte() does not translate those
             * wchar_t unless we pass the correct codepage (1256, Arabic).
             * See bug VSW:192653.
             */
            while(count < n)
            {
                if (*pwcs > 255)  /* validate high byte */
                {
                    errno = EILSEQ;
                    return (size_t)-1;  /* error */
                }
                s[count] = (char) *pwcs;
                if (*pwcs++ == L'\0')
                    return count;
                count++;
            }
            return count;
        } else {

            if (1 == _loc_update.GetLocaleT()->locinfo->mb_cur_max)
            {
                /* If SBCS, one wchar_t maps to one char */

                /* WideCharToMultiByte will compare past NULL - reset n */
                if (n > 0)
                    n = wcsncnt(pwcs, n);
                if ( ((count = WideCharToMultiByte( _loc_update.GetLocaleT()->locinfo->lc_codepage,
                                                    0,
                                                    pwcs, 
                                                    (int)n, 
                                                    s,
                                                    (int)n, 
                                                    NULL, 
                                                    &defused )) != 0) &&
                     (!defused) )
                {
                    if (*(s + count - 1) == '\0')
                        count--; /* don't count NUL */

                    return count;
                }

                errno = EILSEQ;
                return (size_t)-1;
            }
            else {

                /* If MBCS, wchar_t to char mapping unknown */

                /* Assume that usually the buffer is large enough */
                if ( ((count = WideCharToMultiByte( _loc_update.GetLocaleT()->locinfo->lc_codepage,
                                                    0,
                                                    pwcs, 
                                                    -1,
                                                    s, 
                                                    (int)n, 
                                                    NULL, 
                                                    &defused )) != 0) &&
                     (!defused) )
                {
                    return count - 1; /* don't count NUL */
                }
#ifdef _WCE_BOOTCRT
            // BOOTCRT fails early if WideChartoMultibyte fails.
            // This is because BOOTCRT cannot support GetLastError and
            // hence cannot judge the error returned correctly.
            return (size_t)-1;
#else
                int i, retval;
                char buffer[MB_LEN_MAX];

                if (defused || GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                {
                    errno = EILSEQ;
                    return (size_t)-1;
                }

                /* buffer not large enough, must do char by char */
                while (count < n)
                {
                    if ( ((retval = WideCharToMultiByte( _loc_update.GetLocaleT()->locinfo->lc_codepage, 
                                                         0,
                                                         pwcs, 
                                                         1, 
                                                         buffer,
                                                         _loc_update.GetLocaleT()->locinfo->mb_cur_max, 
                                                         NULL, 
                                                         &defused )) == 0)
                         || defused )
                    {
                        errno = EILSEQ;
                        return (size_t)-1;
                    }

                    /* enforce this for prefast */
                    if (retval < 0 ||
                        retval > _countof(buffer))
                    {
                        errno = EILSEQ;
                        return (size_t)-1;
                    }

                    if (count + retval > n)
                        return count;

                    for (i = 0; i < retval; i++, count++) /* store character */
                        if((s[count] = buffer[i])=='\0')
                            return count;

                    pwcs++;
                }

                return count;
#endif
            }
        }
    }
    else { /* s == NULL, get size only, pwcs must be NUL-terminated */
        if ( _loc_update.GetLocaleT()->locinfo->locale_name[LC_CTYPE] == NULL )
        {
            size_t len = 0;
            for (wchar_t *pw = (wchar_t *)pwcs; *pw != 0; pw++)  /* validate high byte */
            {
                if (*pw > 255)  /* validate high byte */
                {
#ifndef _WCE_BOOTCRT
                    errno = EILSEQ;
#endif
                    return (size_t)-1;  /* error */
                }
                ++len;
            }

            return len;
        }
        else {
            if ( ((count = WideCharToMultiByte( _loc_update.GetLocaleT()->locinfo->lc_codepage,
                                                0,
                                                pwcs,
                                                -1,
                                                NULL,
                                                0,
                                                NULL,
                                                &defused )) == 0) ||
                 (defused) )
            {
#ifndef _WCE_BOOTCRT
                errno = EILSEQ;
#endif
                return (size_t)-1;
            }

            return count - 1;
        }
    }

#else /* _NTSUBSET_/_POSIX_ */

    /* if destination string exists, fill it in */
    if (s)
    {
        NTSTATUS Status;

        Status = RtlUnicodeToMultiByteN( s, 
                                         (ULONG) n, 
                                         (PULONG)&count, 
                                         (wchar_t *)pwcs, 
                                         (wcslen(pwcs) + 1) *
                                            sizeof(WCHAR) );

        if (NT_SUCCESS(Status))
        {
            return count - 1; /* don't count NUL */
        } else {
            errno = EILSEQ;
            count = (size_t)-1;
        }
    } else { /* s == NULL, get size only, pwcs must be NUL-terminated */
        NTSTATUS Status;

        Status = RtlUnicodeToMultiByteSize( (PULONG)&count, 
                                            (wchar_t *)pwcs, 
                                            (wcslen(pwcs) + 1) * 
                                                sizeof(WCHAR) );

        if (NT_SUCCESS(Status))
        {
            return count - 1; /* don't count NUL */
        } else {
            errno = EILSEQ;
            count = (size_t)-1;
        }
    }
    return count;

#endif  /* _NTSUBSET_/_POSIX_ */
}

extern "C" size_t __cdecl _wcstombs_l (
        char * s,
        const wchar_t * pwcs,
        size_t n,
        _locale_t plocinfo
        )
{
    return _wcstombs_l_helper(s, pwcs, n, plocinfo);
}

extern "C" size_t __cdecl wcstombs (
        char * s,
        const wchar_t * pwcs,
        size_t n
        )
{
    return _wcstombs_l_helper(s, pwcs, n, NULL);
}

/***
*errno_t wcstombs_s() - Convert wide char string to multibyte char string.
*
*Purpose:
*       Convert a wide char string into the equivalent multibyte char string,
*       according to the LC_CTYPE category of the current locale.
*
*       The destination string is always null terminated.
*
*Entry:
*       size_t *pConvertedChars = Number of bytes modified including the terminating NULL
*                                 This pointer can be NULL.
*       char *dst = pointer to destination multibyte char string
*       size_t sizeInBytes = size of the destination buffer
*       const wchar_t *src = pointer to source wide character string
*       size_t n = maximum number of bytes to store in s (not including the terminating NULL)
*
*Exit:
*       The error code.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" errno_t __cdecl _wcstombs_s_l (
        size_t *pConvertedChars,
        char * dst,
        size_t sizeInBytes,
        const wchar_t * src,
        size_t n,
        _locale_t plocinfo
        )
{
    size_t retsize;
    errno_t retvalue = 0;

    /* validation section */
    _VALIDATE_RETURN_ERRCODE((dst != NULL && sizeInBytes > 0) || (dst == NULL && sizeInBytes == 0), EINVAL);
    if (dst != NULL)
    {
        _RESET_STRING(dst, sizeInBytes);
    }

    if (pConvertedChars != NULL)
    {
        *pConvertedChars = 0;
    }
    
    size_t bufferSize = n > sizeInBytes ? sizeInBytes : n;
    _VALIDATE_RETURN_ERRCODE(bufferSize <= INT_MAX, EINVAL);

    retsize = _wcstombs_l_helper(dst, src, bufferSize, plocinfo);

    if (retsize == (size_t)-1)
    {
        if (dst != NULL)
        {
            _RESET_STRING(dst, sizeInBytes);
        }
#ifndef _WCE_BOOTCRT
        return EILSEQ;
#else
        return (size_t)-1;
#endif
    }

    /* count the null terminator */
    retsize++;

    if (dst != NULL)
    {
        /* return error if the string does not fit, unless n == _TRUNCATE */
        if (retsize > sizeInBytes)
        {
            if (n != _TRUNCATE)
            {
                _RESET_STRING(dst, sizeInBytes);
                _VALIDATE_RETURN_ERRCODE(sizeInBytes > retsize, ERANGE);
            }
            retsize = sizeInBytes;
            retvalue = STRUNCATE;
        }

        /* ensure the string is null terminated */
        dst[retsize - 1] = '\0';
    }

    if (pConvertedChars != NULL)
    {
        *pConvertedChars = retsize;
    }

    return retvalue;
}

extern "C" errno_t __cdecl wcstombs_s (
        size_t *pConvertedChars,
        char * dst,
        size_t sizeInBytes,
        const wchar_t * src,
        size_t n
        )
{
    return _wcstombs_s_l(pConvertedChars, dst, sizeInBytes, src, n, NULL);
}
