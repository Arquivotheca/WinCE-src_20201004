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
*wcsxfrm.c - Transform a wide-character string using locale information
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Transform a wide-character string using the locale information as set by
*       LC_COLLATE.
*
*Revision History:
*       09-09-91  ETC   Created from strxfrm.c.
*       12-09-91  ETC   Updated api; Added multithread lock.
*       12-18-91  ETC   Changed back LCMAP_SORTKEYA --> LCMAP_SORTKEY.
*       04-06-92  KRS   Fix so it works without _INTL too.
*       08-19-92  KRS   Activate use of NLS API.
*       09-02-92  SRW   Get _INTL definition via ..\crt32.def
*       12-15-92  KRS   Fix return value to match ANSI/ISO Std.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       09-22-93  CFW   Use __crtxxx internal NLS API wrapper.
*       09-23-93  CFW   Complete re-write. Non-C locale totally broken.
*       11-09-93  CFW   Use LC_COLLATE code page for __crtxxx() conversion.
*       02-07-94  CFW   POSIXify.
*       09-06-94  CFW   Remove _INTL switch.
*       10-25-94  GJF   Sped up C locale, multi-thread case.
*       01-10-95  CFW   Debug CRT allocs.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       10-11-95  BWT   Fix NTSUBSET
*       11-24-97  GJF   Removed bogus codepage determination.
*       01-12-98  GJF   Use _lc_collate_cp codepage.
*       07-16-98  GJF   Revised multithread support based on threadlocinfo
*                       struct. Also, use _alloca instead of _malloc_crt if
*                       possible.
*       01-04-99  GJF   Changes for 64-bit size_t.
*       04-30-99  PML   Minor cleanup as part of 64-bit merge.
*       12-10-99  GB    Added support for recovery from stack overflow around 
*                       _alloca().
*       10-12-00  GB    Changed the function to be similar to strxfrm()
*       11-12-01  GB    Added support for new locale implementation.
*       10-09-03  AC    Added validation.
*       03-11-04  AC    Return ERANGE when buffer is too small
*       11-30-05  AC    Allow destination buffer to be null or count to be 0
*                       VSW#565375
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <windows.h>
#include <string.h>
#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <awint.h>
#include <dbgint.h>
#include <malloc.h>
#include <internal.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*size_t wcsxfrm() - Transform a string using locale information
*
*Purpose:
*       Transform the wide string pointed to by _string2 and place the
*       resulting wide string into the array pointed to by _string1.
*       No more than _count wide characters are placed into the
*       resulting string (including the null).
*
*       The transformation is such that if wcscmp() is applied to
*       the two transformed strings, the return value is equal to
*       the result of wcscoll() applied to the two original strings.
*       Thus, the conversion must take the locale LC_COLLATE info
*       into account.
*
*       In the C locale, wcsxfrm() simply resolves to wcsncpy()/wcslen().
*
*Entry:
*       wchar_t *_string1       = result string
*       const wchar_t *_string2 = source string
*       size_t _count           = max wide chars to move
*
*       [If _count is 0, _string1 is permitted to be NULL.]
*
*Exit:
*       Length of the transformed string (not including the terminating
*       null).  If the value returned is >= _count, the contents of the
*       _string1 array are indeterminate.
*
*Exceptions:
*       Non-standard: if OM/API error, return INT_MAX.
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" size_t __cdecl _wcsxfrm_l (
        wchar_t *_string1,
        const wchar_t *_string2,
        size_t _count,
        _locale_t plocinfo
        )
{
    int size = INT_MAX;

    /* validation section */
    _VALIDATE_RETURN(_count <= INT_MAX, EINVAL, INT_MAX);
    _VALIDATE_RETURN(_string1 != NULL || _count == 0, EINVAL, INT_MAX);
    _VALIDATE_RETURN(_string2 != NULL, EINVAL, INT_MAX);

    _LocaleUpdate _loc_update(plocinfo);

    if ( _loc_update.GetLocaleT()->locinfo->locale_name[LC_COLLATE] == NULL )
    {
_BEGIN_SECURE_CRT_DEPRECATION_DISABLE
        wcsncpy(_string1, _string2, _count);
_END_SECURE_CRT_DEPRECATION_DISABLE
        return wcslen(_string2);
    }

    if ( 0 == (size = __crtLCMapStringW(
                    _loc_update.GetLocaleT()->locinfo->locale_name[LC_COLLATE],
                    LCMAP_SORTKEY,
                    _string2,
                    -1,
                    NULL,
                    0 )) )
    {
        errno = EILSEQ;
        size = INT_MAX;
    } else
    {
        if ( size <= (int)_count)
        {
            if ( 0 == (size = __crtLCMapStringW(
                            _loc_update.GetLocaleT()->locinfo->locale_name[LC_COLLATE],
                            LCMAP_SORTKEY,
                            _string2,
                            -1,
                            (wchar_t *)_string1,
                            (int)_count )) )
            {
                errno = EILSEQ;
                size = INT_MAX; /* default error */
            } else
            {
                // Note that the size that LCMapStringW returns for
                // LCMAP_SORTKEY is number of bytes needed. That's why it
                // is safe to convert the buffer to wide char from end.
                _count = size--;
                for (;_count-- > 0;)
                {
                    _string1[_count] = (wchar_t)((unsigned char *)_string1)[_count];
                }
            }
        }
        else
        {
            if (_string1 != NULL && _count > 0)
            {
                *_string1 = '\0';
                errno = ERANGE;
            }
            size--;
        }
    }

    return (size_t)size;
}

extern "C" size_t __cdecl wcsxfrm (
        wchar_t *_string1,
        const wchar_t *_string2,
        size_t _count
        )
{
#ifdef  _NTSUBSET_
    if (_string1)
        wcsncpy(_string1, _string2, _count);
    return wcslen(_string2);
#else

    return _wcsxfrm_l(_string1, _string2, _count, NULL);
#endif
}

#endif  /* _POSIX_ */
