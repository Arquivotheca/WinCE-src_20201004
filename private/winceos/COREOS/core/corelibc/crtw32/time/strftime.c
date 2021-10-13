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
*strftime.c - String Format Time
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*
*Revision History:
*       03-09-89  JCR   Initial version.
*       03-15-89  JCR   Changed day/month strings from all caps to leading cap
*       06-20-89  JCR   Removed _LOAD_DGROUP code
*       03-20-90  GJF   Replaced _LOAD_DS with _CALLTYPE1, added #include
*                       <cruntime.h>, removed #include <register.h> and
*                       removed some leftover 16-bit support. Also, fixed
*                       the copyright.
*       03-23-90  GJF   Made static functions _CALLTYPE4.
*       07-23-90  SBM   Compiles cleanly with -W3 (removed unreferenced
*                       variable)
*       08-13-90  SBM   Compiles cleanly with -W3 with new build of compiler
*       10-04-90  GJF   New-style function declarators.
*       01-22-91  GJF   ANSI naming.
*       08-15-91  MRM   Calls tzset() to set timezone info in case of %z.
*       08-16-91  MRM   Put appropriate header file for tzset().
*       10-10-91  ETC   Locale support under _INTL switch.
*       12-18-91  ETC   Use localized time strings structure.
*       02-10-93  CFW   Ported to Cuda tree, change _CALLTYPE4 to _CRTAPI3.
*       02-16-93  CFW   Massive changes: bug fixes & enhancements.
*       03-08-93  CFW   Changed _expand to _expandtime.
*       03-09-93  CFW   Handle string literals inside format strings.
*       03-09-93  CFW   Alternate form cleanup.
*       03-17-93  CFW   Change *count > 0, to *count != 0, *count is unsigned.
*       03-22-93  CFW   Change "C" locale time format specifier to 24-hour.
*       03-30-93  GJF   Call _tzset instead of __tzset (which no longer
*                       exists).
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-14-93  CFW   Disable _alternate_form for 'X' specifier, fix count bug.
*       04-28-93  CFW   Fix bug in '%c' handling.
*       07-15-93  GJF   Call __tzset() in place of _tzset().
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       04-11-94  GJF   Made definitions of __lc_time_c, _alternate_form and
*                       _no_lead_zeros conditional on ndef DLL_FOR_WIN32S.
*       09-06-94  CFW   Remove _INTL switch.
*       02-13-95  GJF   Appended Mac version of source file (somewhat cleaned
*                       up), with appropriate #ifdef-s.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       02-22-96  JWM   Merge in PlumHall mods.
*       06-17-96  SKS   Enable new Plum-Hall code for _MAC as well as _WIN32
*       07-10-97  GJF   Made __lc_time_c selectany. Also, removed unnecessary
*                       init to 0 for globals, cleaned up the formatting a bit,
*                       added a few __cdecls and detailed old (and no longer
*                       used as of 6/17/96 change) Mac version.
*       08-21-97  GJF   Added support for AM/PM type suffix to time string.
*       09-10-98  GJF   Added support for per-thread locale info.
*       03-04-99  GJF   Added refcount field to __lc_time_c.
*       05-17-99  PML   Remove all Macintosh support.
*       08-30-99  PML   Don't overflow buffer on leadbyte in _store_winword.
*       03-17-00  PML   Corrected _Gettnames to also copy ww_timefmt (VS7#9374)
*       09-07-00  PML   Remove dependency on libcp.lib/xlocinfo.h (vs7#159463)
*       03-25-01  PML   Use GetDateFormat/GetTimeFormat in _store_winword for
*                       calendar types other than the basic type 1, localized
*                       Gregorian (vs7#196892)  Also fix formatting for leading
*                       zero suppression in fields, which was busted for %c,
*                       %x, %X.
*       11-12-01  GB    Added support for new locale implementation.
*       12-11-01  BWT   Replace _getptd with _getptd_noexit - we can return 0/ENOMEM
*                       here instead of exiting.
*       02-25-02  BWT   Early exit expandtime if NULL is passed in as the timeptr
*       05-07-02  GB    Added fix for am:pm in spanish locale
*       05-08-02  MSL   Fixed MBCS leadbyte followed by \0 issues
*                       Added error-return path from internal functions
*                       Fixed one case of multi-threadunsafe isleadbyte
*                       VS7 340068
*       03-06-03  SSM   Assertions and validations
*       03-23-03  SSM   VSWhidbey 173159. Remove dead code.
*       09-22-04  AGH   Assert and set error code when dest buffer is too small
*       09-25-04  JL    Replace usage of _alloca() with _alloca_s() / _freea_s()
*       10-22-04  AGH   In _Strftime_l, fix validations
*       02-16-05  AC    Just return ERANGE if there is not enough space in strftime
*                       VSW#454336
*       04-04-05  JL    Replace _alloca_s and _freea_s with _malloca and _freea
*       01-20-06  AC    Fix the validation for tm_year to allow negative values
*                       for tm_year in some cases.
*                       VSW#494313
*       02-01-07  JKS   Fix DevDiv bug #63971
                        Change validation behavior to meet with MSDN
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>
#include <mtdll.h>
#include <time.h>
#include <locale.h>
#include <setlocal.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <dbgint.h>
#include <malloc.h>
#include <errno.h>

extern "C" size_t __cdecl _Wcsftime_l (
        wchar_t *string,
        size_t maxsize,
        const wchar_t *format,
        const struct tm *timeptr,
        void *lc_time_arg,
        _locale_t plocinfo
        );

/* Prototypes for local routines */
extern "C" size_t __cdecl _Strftime (
        char *string,
        size_t maxsize,
        const char *format,
        const struct tm *timeptr,
        void *lc_time_arg
        );

extern "C" size_t __cdecl _Strftime_l (
        char *string,
        size_t maxsize,
        const char *format,
        const struct tm *timeptr,
        void *lc_time_arg,
        _locale_t plocinfo
        );

#define TIME_SEP        ':'

/*      get a copy of the current day names */
extern "C" char * __cdecl _Getdays_l (
        _locale_t plocinfo
        )
{
    const struct __lc_time_data *pt;
    size_t n, len = 0;
    char *p;
    _LocaleUpdate _loc_update(plocinfo);

    pt = __LC_TIME_CURR(_loc_update.GetLocaleT()->locinfo);

    for (n = 0; n < 7; ++n)
        len += strlen(pt->wday_abbr[n]) + strlen(pt->wday[n]) + 2;
    p = (char *)_malloc_crt(len + 1);

    if (p != 0) {
        char *s = p;

        for (n = 0; n < 7; ++n) {
            *s++ = TIME_SEP;
            _ERRCHECK(strcpy_s(s, (len + 1) - (s - p), pt->wday_abbr[n]));
            s += strlen(s);
            *s++ = TIME_SEP;
            _ERRCHECK(strcpy_s(s, (len + 1) - (s - p), pt->wday[n]));
            s += strlen(s);
        }
        *s++ = '\0';
    }

    return (p);
}
extern "C" char * __cdecl _Getdays (
        void
        )
{
    return _Getdays_l(NULL);
}

/*      get a copy of the current month names */
extern "C" char * __cdecl _Getmonths_l (
        _locale_t plocinfo
        )
{
    const struct __lc_time_data *pt;
    size_t n, len = 0;
    char *p;
    _LocaleUpdate _loc_update(plocinfo);

    pt = __LC_TIME_CURR(_loc_update.GetLocaleT()->locinfo);

    for (n = 0; n < 12; ++n)
        len += strlen(pt->month_abbr[n]) + strlen(pt->month[n]) + 2;
    p = (char *)_malloc_crt(len + 1);

    if (p != 0) {
        char *s = p;

        for (n = 0; n < 12; ++n) {
            *s++ = TIME_SEP;
            _ERRCHECK(strcpy_s(s, (len + 1) - (s - p), pt->month_abbr[n]));
            s += strlen(s);
            *s++ = TIME_SEP;
            _ERRCHECK(strcpy_s(s, (len + 1) - (s - p), pt->month[n]));
            s += strlen(s);
        }
        *s++ = '\0';
    }

    return (p);
}
extern "C" char * __cdecl _Getmonths (
        void
        )
{
    return _Getmonths_l(NULL);
}

/*      get a copy of the current time locale information */
extern "C" void * __cdecl _W_Gettnames_l (
        _locale_t plocinfo
        );
extern "C" void * __cdecl _Gettnames (
        void
        )
{
    return _W_Gettnames_l(NULL);
}


/***
*size_t strftime(string, maxsize, format, timeptr) - Format a time string
*
*Purpose:
*       Place characters into the user's output buffer expanding time
*       format directives as described in the user's control string.
*       Use the supplied 'tm' structure for time data when expanding
*       the format directives.
*       [ANSI]
*
*Entry:
*       char *string = pointer to output string
*       size_t maxsize = max length of string
*       const char *format = format control string
*       const struct tm *timeptr = pointer to tb data structure
*
*Exit:
*       !0 = If the total number of resulting characters including the
*       terminating null is not more than 'maxsize', then return the
*       number of chars placed in the 'string' array (not including the
*       null terminator).
*
*       0 = Otherwise, return 0 and the contents of the string are
*       indeterminate.
*
*Exceptions:
*
*******************************************************************************/

extern "C" size_t __cdecl _strftime_l (
        char *string,
        size_t maxsize,
        const char *format,
        const struct tm *timeptr,
        _locale_t plocinfo
        )
{
        return (_Strftime_l(string, maxsize, format, timeptr, 0, plocinfo));
}
extern "C" size_t __cdecl strftime (
        char *string,
        size_t maxsize,
        const char *format,
        const struct tm *timeptr
        )
{
        return (_Strftime_l(string, maxsize, format, timeptr, 0, NULL));
}

/***
*size_t _Strftime(string, maxsize, format,
*       timeptr, lc_time) - Format a time string for a given locale
*
*Purpose:
*       Place characters into the user's output buffer expanding time
*       format directives as described in the user's control string.
*       Use the supplied 'tm' structure for time data when expanding
*       the format directives. use the locale information at lc_time.
*       [ANSI]
*
*Entry:
*       char *string = pointer to output string
*       size_t maxsize = max length of string
*       const char *format = format control string
*       const struct tm *timeptr = pointer to tb data structure
*               struct __lc_time_data *lc_time = pointer to locale-specific info
*                       (passed as void * to avoid type mismatch with C++)
*
*Exit:
*       !0 = If the total number of resulting characters including the
*       terminating null is not more than 'maxsize', then return the
*       number of chars placed in the 'string' array (not including the
*       null terminator).
*
*       0 = Otherwise, return 0 and the contents of the string are
*       indeterminate.
*
*Exceptions:
*
*******************************************************************************/

extern "C" size_t __cdecl _Strftime (
        char *string,
        size_t maxsize,
        const char *format,
        const struct tm *timeptr,
        void *lc_time_arg
        )
{
    return _Strftime_l(string, maxsize, format, timeptr,
                        lc_time_arg, NULL);
}

extern "C" size_t __cdecl _Strftime_l (
        char *string,
        size_t maxsize,
        const char *format,
        const struct tm *timeptr,
        void *lc_time_arg,
        _locale_t plocinfo
        )
{
        size_t ret = 0;
        wchar_t *wformat = NULL;
        wchar_t *wstring = NULL;
        int cch_wformat;
        _LocaleUpdate _loc_update(plocinfo);

        _VALIDATE_RETURN( ( string != NULL ), EINVAL, 0)
        _VALIDATE_RETURN( ( maxsize != 0 ), EINVAL, 0)
        *string = '\0';

        _VALIDATE_RETURN( ( format != NULL ), EINVAL, 0)
        _VALIDATE_RETURN( ( timeptr != NULL ), EINVAL, 0)

        /* get the number of characters, including terminating null, needed for conversion */
        if  ( (cch_wformat = MultiByteToWideChar(_loc_update.GetLocaleT()->locinfo->lc_time_cp, 0 /* Use default flags */, format, -1, NULL, 0) ) == 0 )
        {
            _dosmaperr(GetLastError());
            goto error_cleanup;
        }

        /* allocate enough space for wide char format string */
        if ( (wformat = (wchar_t*)_malloc_crt( cch_wformat * sizeof(wchar_t) ) ) == NULL )
        {
            /* malloc should set the errno, if any */
            goto error_cleanup;
        }

        /* Copy format to a wide-char string */
        if ( MultiByteToWideChar(_loc_update.GetLocaleT()->locinfo->lc_time_cp, 0 /* Use default flags */, format, -1, wformat, cch_wformat) == 0 )
        {
            _dosmaperr(GetLastError());
            goto error_cleanup;
        }

        /* Allocate a new wide-char output string with the same size as the char* string one passed in as argument */
        if ((wstring = (wchar_t*)_malloc_crt(maxsize * sizeof(wchar_t))) == 0)
        {
            goto error_cleanup;
        }
                
        if (0 != (ret = _Wcsftime_l(wstring, maxsize, wformat, timeptr, lc_time_arg, plocinfo)))
        {
            /* Copy output from wide char string */
            if (!WideCharToMultiByte(_loc_update.GetLocaleT()->locinfo->lc_time_cp, 0, wstring, -1, string, (int)maxsize, NULL, NULL))
            {
                /* If conversion doesn't succeed */
                _dosmaperr(GetLastError());
                ret = 0; /* reset return status to indicate error */
            }
        }

error_cleanup:
        /* Cleanup */
        _free_crt(wstring);
        _free_crt(wformat);         

        return ret;
}
