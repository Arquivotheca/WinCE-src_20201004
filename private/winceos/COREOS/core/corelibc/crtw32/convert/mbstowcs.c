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
*mbstowcs.c - Convert multibyte char string to wide char string.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Convert a multibyte char string into the equivalent wide char string.
*
*Revision History:
*       08-24-90  KRS   Module created.
*       03-20-91  KRS   Ported from 16-bit tree.
*       10-16-91  ETC   Locale support under _INTL switch.
*       12-09-91  ETC   Updated nlsapi; added multithread.
*       08-20-92  KRS   Activated NLSAPI support.
*       08-31-92  SRW   Allow INTL definition to be conditional for building ntcrt.lib
*       09-02-92  SRW   Get _INTL definition via ..\crt32.def
*       02-09-93  CFW   Always stuff WC 0 at end of output string of room (non _INTL).
*       04-06-93  SKS   Replace _CRTAPI* with _cdecl
*       05-03-93  CFW   Return pointer == NULL, return size, plus massive cleanup.
*       06-01-93  CFW   Minor optimization and beautify.
*       06-02-93  SRW   ignore _INTL if _NTSUBSET_ defined.
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       09-21-93  CFW   Avoid cast bug.
*       09-27-93  GJF   Merged NT SDK and Cuda.
*       10-22-93  CFW   Test for invalid MB chars using global preset flag.
*       01-14-94  SRW   if _NTSUBSET_ defined call Rtl functions
*       02-03-94  GJF   Merged in Steve Wood's latest change (affects
*                       _NTSUBSET_ build only).
*       02-07-94  CFW   POSIXify.
*       08-03-94  CFW   Bug #15300; fix MBToWC workaround for small buffer.
*       09-06-94  CFW   Remove _INTL switch.
*       10-18-94  BWT   Fix build warning for call to RtlMultiByteToUnicodeN
*       12-21-94  CFW   Remove invalid MB chars NT 3.1 hack.
*       01-07-95  CFW   Mac merge cleanup.
*       02-06-95  CFW   assert -> _ASSERTE.
*       04-19-95  CFW   Rearrange & fix non-Win32 version.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       04-01-96  BWT   POSIX work.
*       05-26-96  BWT   Return the word count, not the byte count for 
*                       _NTSUBSET_/POSIX case.
*       06-25-96  GJF   Removed DLL_FOR_WIN32S. Replaced defined(_WIN32) with
*                       !defined(_MAC). Polished the format a bit.
*       07-27-98  GJF   Revised multithread support based on threadlocinfo
*                       struct.
*       12-15-98  GJF   Changes for 64-bit size_t.
*       05-17-99  PML   Remove all Macintosh support.
*       11-12-01  GB    Added support for new locale implementation.
*       05-09-02  MSL   Fixed bug when converting dud string with Leadbyte, EOS
*                       VS7 340656 
*       10-10-03  AC    Added secure version.
*       03-08-04  MSL   Fixed mbstowcs family validation 
*                       VSW 232132
*       03-10-04  AC    Return ERANGE when buffer is too small
*       03-11-04  AC    Fixed validation.
*                       VSW#239396
*       09-08-04  AC    Added truncate behavior.
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

#include <internal.h>
#include <internal_securecrt.h>
#include <locale.h>
#include <errno.h>
#include <cruntime.h>
#include <stdlib.h>
#include <string.h>
#include <dbgint.h>
#include <stdio.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*size_t mbstowcs() - Convert multibyte char string to wide char string.
*
*Purpose:
*       Convert a multi-byte char string into the equivalent wide char string,
*       according to the LC_CTYPE category of the current locale.
*       [ANSI].
*
*Entry:
*       wchar_t *pwcs = pointer to destination wide character string buffer
*       const char *s = pointer to source multibyte character string
*       size_t      n = maximum number of wide characters to store
*
*Exit:
*       If pwcs != NULL returns the number of words modified (<=n): note that
*       if the return value == n, then no destination string is not 0 terminated.
*       If pwcs == NULL returns the length (not size) needed for the destination buffer.
*
*Exceptions:
*       Returns (size_t)-1 if s is NULL or invalid mbcs character encountered
*       and errno is set to EILSEQ.
*
*******************************************************************************/

/* Helper shared by secure and non-secure functions */

extern "C" size_t __cdecl _mbstowcs_l_helper (
        wchar_t  *pwcs,
        const char *s,
        size_t n,
        _locale_t plocinfo
        )
{
    size_t count = 0;

    if (pwcs && n == 0)
        /* dest string exists, but 0 bytes converted */
        return (size_t) 0;

    if (pwcs && n > 0)
    {
        *pwcs = '\0';
    }

    /* validation section */
    _VALIDATE_RETURN(s != NULL, EINVAL, (size_t)-1);

#if     !defined(_NTSUBSET_) && !defined(_POSIX_)

    _LocaleUpdate _loc_update(plocinfo);
    /* if destination string exists, fill it in */
    if (pwcs)
    {
        if (_loc_update.GetLocaleT()->locinfo->locale_name[LC_CTYPE] == NULL)
        {
            /* C locale: easy and fast */
            while (count < n)
            {
                *pwcs = (wchar_t) ((unsigned char)s[count]);
                if (!s[count])
                    return count;
                count++;
                pwcs++;
            }
            return count;

        } else {
            
            /* Assume that the buffer is large enough */
            if ( (count = MultiByteToWideChar( _loc_update.GetLocaleT()->locinfo->lc_codepage,
                                               MB_PRECOMPOSED | 
                                                MB_ERR_INVALID_CHARS,
                                               s, 
                                               -1, 
                                               pwcs, 
                                               (int)n )) != 0 )
                return count - 1; /* don't count NUL */
#ifdef _WCE_BOOTCRT
            // BOOTCRT fails early if MultiByteToWideChar fails.
            // This is because BOOTCRT cannot support GetLastError and
            // hence cannot judge the error returned correctly.
            return (size_t)-1;
#else
            int bytecnt, charcnt;
            unsigned char *p;

            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            {
                errno = EILSEQ;
                *pwcs = '\0';
                return (size_t)-1;
            }

            /* User-supplied buffer not large enough. */

            /* How many bytes are in n characters of the string? */
            charcnt = (int)n;
            for (p = (unsigned char *)s; (charcnt-- && *p); p++)
            {
                if (
                    _isleadbyte_l(*p, _loc_update.GetLocaleT())
                    )
                {
                    if(p[1]=='\0')
                    {
                        /*  this is a leadbyte followed by EOS -- a dud MBCS string
                            We choose not to assert here because this
                            function is defined to deal with dud strings on
                            input and return a known value
                        */
                        errno = EILSEQ;
                        *pwcs = '\0';
                        return (size_t)-1;
                    }
                    else
                    {
                        p++;
                    }
                }
            }
            bytecnt = ((int) ((char *)p - (char *)s));

            if ( (count = MultiByteToWideChar( _loc_update.GetLocaleT()->locinfo->lc_codepage, 
                                               MB_PRECOMPOSED,
                                               s, 
                                               bytecnt, 
                                               pwcs, 
                                               (int)n )) == 0 )
            {
                errno = EILSEQ;
                *pwcs = '\0';
                return (size_t)-1;
            }

            return count; /* no NUL in string */
#endif /* _WCE_BOOTCRT */
        }
    }
    else { /* pwcs == NULL, get size only, s must be NUL-terminated */
        if (_loc_update.GetLocaleT()->locinfo->locale_name[LC_CTYPE] == NULL) {
            return strlen(s);
        } else if ( (count = MultiByteToWideChar( _loc_update.GetLocaleT()->locinfo->lc_codepage,
                                                  MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
                                                  s,
                                                  -1,
                                                  NULL, 
                                                  0 )) == 0 ) {
            errno = EILSEQ;
            return (size_t)-1;
        } else {
            return count - 1;
        }
    }

#else /* _NTSUBSET_/_POSIX_ */

    if (pwcs) {

        NTSTATUS Status;
        int size;

        size = _mbstrlen(s);
        Status = RtlMultiByteToUnicodeN(pwcs,
                                        (ULONG) ( n * sizeof( *pwcs ) ),
                                        (PULONG)&size,
                                        (char *)s,
                                        size+1 );
        if (!NT_SUCCESS(Status)) {
            errno = EILSEQ;
            *pwcs = '\0';
            size = -1;
        } else {
            size = size / sizeof(*pwcs);
            if (pwcs[size-1] == L'\0') {
                size -= 1;
            }
        }
        return size;

    } else { /* pwcs == NULL, get size only, s must be NUL-terminated */
        return strlen(s);
    }

#endif  /* _NTSUBSET_/_POSIX_ */
}

extern "C" size_t __cdecl _mbstowcs_l (
        wchar_t  *pwcs,
        const char *s,
        size_t n,
        _locale_t plocinfo
        )
{
    /* Call a non-deprecated helper to do the work. */

    return _mbstowcs_l_helper(pwcs, s, n, plocinfo);
}

extern "C" size_t __cdecl mbstowcs
(
        wchar_t  *pwcs,
        const char *s,
        size_t n
        )
{
    _BEGIN_SECURE_CRT_DEPRECATION_DISABLE
    if (__locale_changed == 0)
    {
        return _mbstowcs_l(pwcs, s, n, &__initiallocalestructinfo);
    }
    else
    {
        return _mbstowcs_l(pwcs, s, n, NULL);
    }
    _END_SECURE_CRT_DEPRECATION_DISABLE
}

/***
*errno_t mbstowcs_s() - Convert multibyte char string to wide char string.
*
*Purpose:
*       Convert a multi-byte char string into the equivalent wide char string,
*       according to the LC_CTYPE category of the current locale.
*       Same as mbstowcs(), but the destination is ensured to be null terminated.
*       If there's not enough space, we return EINVAL.
*
*Entry:
*       size_t *pConvertedChars = Number of bytes modified including the terminating NULL
*                                 This pointer can be NULL.
*       wchar_t *pwcs = pointer to destination wide character string buffer
*       size_t sizeInWords = size of the destination buffer
*       const char *s = pointer to source multibyte character string
*       size_t n = maximum number of wide characters to store (not including the terminating NULL)
*
*Exit:
*       The error code.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" errno_t __cdecl _mbstowcs_s_l (
        size_t *pConvertedChars,
        wchar_t  *pwcs,
        size_t sizeInWords,
        const char *s,
        size_t n,
        _locale_t plocinfo
        )
{
    size_t retsize;
    errno_t retvalue = 0;

    /* validation section */
    _VALIDATE_RETURN_ERRCODE((pwcs == NULL && sizeInWords == 0) || (pwcs != NULL && sizeInWords > 0), EINVAL);

    if (pwcs != NULL)
    {
        _RESET_STRING(pwcs, sizeInWords);
    }

    if (pConvertedChars != NULL)
    {
        *pConvertedChars = 0;
    }

    _LocaleUpdate _loc_update(plocinfo);
    
    size_t bufferSize = n > sizeInWords ? sizeInWords : n;
    /* n must fit into an int for MultiByteToWideChar */
    _VALIDATE_RETURN_ERRCODE(bufferSize <= INT_MAX, EINVAL);

    /* Call a non-deprecated helper to do the work. */

    retsize = _mbstowcs_l_helper(pwcs, s, bufferSize, _loc_update.GetLocaleT());

    if (retsize == (size_t)-1)
    {
        if (pwcs != NULL)
        {
            _RESET_STRING(pwcs, sizeInWords);
        }
#ifndef _WCE_BOOTCRT
        return errno;
#else
        return EILSEQ;
#endif
    }

    /* count the null terminator */
    retsize++;

    if (pwcs != NULL)
    {
        /* return error if the string does not fit, unless n == _TRUNCATE */
        if (retsize > sizeInWords)
        {
            if (n != _TRUNCATE)
            {
                _RESET_STRING(pwcs, sizeInWords);
                _VALIDATE_RETURN_ERRCODE(retsize <= sizeInWords, ERANGE);
            }
            retsize = sizeInWords;
            retvalue = STRUNCATE;
        }

        /* ensure the string is null terminated */
        pwcs[retsize - 1] = '\0';
    }

    if (pConvertedChars != NULL)
    {
        *pConvertedChars = retsize;
    }

    return retvalue;
}

extern "C" errno_t __cdecl mbstowcs_s (
        size_t *pConvertedChars,
        wchar_t  *pwcs,
        size_t sizeInWords,
        const char *s,
        size_t n
)
{
    return _mbstowcs_s_l(pConvertedChars, pwcs, sizeInWords, s, n, NULL);
}
