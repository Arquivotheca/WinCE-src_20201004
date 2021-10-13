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
*strxfrm.c - Transform a string using locale information
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Transform a string using the locale information as set by
*       LC_COLLATE.
*
*Revision History:
*       03-21-89  JCR   Module created.
*       06-20-89  JCR   Removed _LOAD_DGROUP code
*       02-27-90  GJF   Fixed calling type, #include <cruntime.h>, fixed
*                       copyright.
*       10-02-90  GJF   New-style function declarator.
*       10-02-91  ETC   Non-C locale support under _INTL switch.
*       12-09-91  ETC   Updated api; added multithread.
*       12-18-91  ETC   Don't convert output of LCMapString.
*       08-18-92  KRS   Activate NLS API.  Fix behavior.
*       09-02-92  SRW   Get _INTL definition via ..\crt32.def
*       12-11-92  SKS   Need to handle count=0 in non-INTL code
*       12-15-92  KRS   Handle return value according to ANSI.
*       01-18-93  CFW   Removed unreferenced variable "dummy".
*       03-10-93  CFW   Remove UNDONE comment.
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       09-15-93  CFW   Use ANSI conformant "__" names.
*       09-22-93  CFW   Use __crtxxx internal NLS API wrapper.
*       11-09-93  CFW   Use LC_COLLATE code page for __crtxxx() conversion.
*       09-06-94  CFW   Remove _INTL switch.
*       10-24-94  GJF   Sped up C locale, multi-thread case.
*       12-29-94  CFW   Merge non-Win32.
*       09-26-95  GJF   New locking macro, and scheme, for functions which
*                       reference the locale.
*       10-11-95  BWT   Fix NTSUBSET
*       03-17-97  RDK   Added error flag to __crtLCMapStringA.
*       11-24-97  GJF   Removed bogus codepage determination.
*       01-12-98  GJF   Use _lc_collate_cp codepage.
*       07-16-98  GJF   Revised multithread support based on threadlocinfo
*                       struct.
*       01-04-99  GJF   Changes for 64-bit size_t.
*       04-30-99  PML   Minor cleanup as part of 64-bit merge.
*       05-17-99  PML   Remove all Macintosh support.
*       11-12-01  GB    Added support for new locale implementation.
*       10-09-03  AC    Added validation.
*       03-04-04  AC    The return value is the string length (without the terminating 0)
*       03-10-04  AC    Return ERANGE when buffer is too small
*       01-21-05  MSL   Add out init to pacify prefix
*       11-30-05  AC    Allow destination buffer to be null or count to be 0
*                       VSW#565363
*
*******************************************************************************/

#include <cruntime.h>
#include <string.h>
#include <windows.h>
#include <stdlib.h>
#include <limits.h>
#include <malloc.h>
#include <locale.h>
#include <awint.h>
#include <internal.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*size_t strxfrm() - Transform a string using locale information
*
*Purpose:
*       Transform the string pointed to by _string2 and place the
*       resulting string into the array pointed to by _string1.
*       No more than _count characters are place into the
*       resulting string (including the null).
*
*       The transformation is such that if strcmp() is applied to
*       the two transformed strings, the return value is equal to
*       the result of strcoll() applied to the two original strings.
*       Thus, the conversion must take the locale LC_COLLATE info
*       into account.
*       [ANSI]
*
*       The value of the following expression is the size of the array
*       needed to hold the transformation of the source string:
*
*           1 + strxfrm(NULL,string,0)
*
*Entry:
*       char *_string1       = result string
*       const char *_string2 = source string
*       size_t _count        = max chars to move
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

extern "C" size_t __cdecl _strxfrm_l (
        char *_string1,
        const char *_string2,
        size_t _count,
        _locale_t plocinfo
        )
{
    int dstlen;
    size_t retval = INT_MAX;   /* NON-ANSI: default if OM or API error */
    _LocaleUpdate _loc_update(plocinfo);

    /* validation section */
    _VALIDATE_RETURN(_count <= INT_MAX, EINVAL, INT_MAX);
    _VALIDATE_RETURN(_string1 != NULL || _count == 0, EINVAL, INT_MAX);
    _VALIDATE_RETURN(_string2 != NULL, EINVAL, INT_MAX);

    /* pre-init output in case of error */
    if(_string1!=NULL && _count>0)
    {
        *_string1='\0';
    }

    if ( (_loc_update.GetLocaleT()->locinfo->locale_name[LC_COLLATE] == NULL) &&
            (_loc_update.GetLocaleT()->locinfo->lc_collate_cp == _CLOCALECP) )
    {
_BEGIN_SECURE_CRT_DEPRECATION_DISABLE
        strncpy(_string1, _string2, _count);
_END_SECURE_CRT_DEPRECATION_DISABLE
        return strlen(_string2);
    }

    /* Inquire size of dst string in BYTES */
    if ( 0 == (dstlen = __crtLCMapStringA(
                    _loc_update.GetLocaleT(),
                    _loc_update.GetLocaleT()->locinfo->locale_name[LC_COLLATE],
                    LCMAP_SORTKEY,
                    _string2,
                    -1,
                    NULL,
                    0,
                    _loc_update.GetLocaleT()->locinfo->lc_collate_cp,
                    TRUE )) )
    {
        errno = EILSEQ;
        return INT_MAX;
    }

    retval = (size_t)dstlen;

    /* if not enough room, return amount needed */
    if ( retval > _count )
    {
        if (_string1 != NULL && _count > 0)
        {
            *_string1 = '\0';
            errno = ERANGE;
        }
        /* the return value is the string length (without the terminating 0) */
        retval--;
        return retval;
    }

    /* Map src string to dst string */
    if ( 0 == __crtLCMapStringA(
                _loc_update.GetLocaleT(),
                _loc_update.GetLocaleT()->locinfo->locale_name[LC_COLLATE],
                LCMAP_SORTKEY,
                _string2,
                -1,
                _string1,
                (int)_count,
                _loc_update.GetLocaleT()->locinfo->lc_collate_cp,
                TRUE ) )
    {
        errno = EILSEQ;
        return INT_MAX;
    }
    /* the return value is the string length (without the terminating 0) */
    retval--;

    return retval;
}

extern "C" size_t __cdecl strxfrm (
        char *_string1,
        const char *_string2,
        size_t _count
        )
{
#ifdef  _NTSUBSET_
    if (_string1)
    {
        strncpy(_string1, _string2, _count);
    }
    return strlen(_string2);
#else

    return _strxfrm_l(_string1, _string2, _count, NULL);

#endif  /* _NTSUBSET_ */
}
