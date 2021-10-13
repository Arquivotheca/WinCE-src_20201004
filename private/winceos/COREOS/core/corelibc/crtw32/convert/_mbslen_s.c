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
*_mbslen_s.c - Return number of multibyte characters in a multibyte string
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Return number of multibyte characters in a multibyte string
*       excluding the terminal null.  Locale-dependent.
*
*Revision History:
*       10-08-03  AC    Module created.
*       02-03-04  SJ    VSW#233454 - call strlen_s.
*       03-08-04  AC    Change the name to _mbstrnlen.
*       03-26-04  AC    Check the string in the first sizeInBytes bytes (and not chars)
*
*******************************************************************************/

#if     defined(_NTSUBSET_) || defined(_POSIX_)
#include <ntrtl.h>
#include <nturtl.h>
#endif
#include <cruntime.h>
#include <stdlib.h>
#include <ctype.h>
#include <locale.h>
#include <dbgint.h>
#include <internal.h>
#include <mtdll.h>
#include <setlocal.h>

/***
*_mbstrnlen - Return number of multibyte characters in a multibyte string
*
*Purpose:
*       Return number of multibyte characters in a multibyte string
*       excluding the terminal null.  Locale-dependent.
*
*Entry:
*       char *s = string
*       size_t maxsize
*
*Exit:
*       Returns the number of multibyte characters in the string, or
*       (size_t)-1 if the string contains an invalid multibyte character and errno 
*       is set to EILSEQ.
*       Only the first sizeInBytes bytes of the string are inspected: if the null
*       terminator is not found, sizeInBytes is returned.
*       If the string is null terminated in sizeInBytes bytes, the return value
*       will always be less than sizeInBytes.
*       If something goes wrong, (size_t)-1 is returned and errno is set to EINVAL.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

size_t __cdecl _mbstrnlen_l(
        const char *s,
        size_t sizeInBytes,
        _locale_t plocinfo
        )
{
    size_t n, size;

#if     !defined(_NTSUBSET_) && !defined(_POSIX_)

    /* validation section */
    _VALIDATE_RETURN(s != NULL, EINVAL, (size_t)-1);
    _VALIDATE_RETURN(sizeInBytes <= INT_MAX, EINVAL, (size_t)-1);

#endif

    _LocaleUpdate _loc_update(plocinfo);

    _ASSERTE (_loc_update.GetLocaleT()->locinfo->mb_cur_max == 1 || _loc_update.GetLocaleT()->locinfo->mb_cur_max == 2);

    if ( _loc_update.GetLocaleT()->locinfo->mb_cur_max == 1 )
        /* handle single byte character sets */
        return (int)strnlen(s, sizeInBytes);

#if     !defined(_NTSUBSET_) && !defined(_POSIX_)

    /* verify all valid MB chars */
    if ( MultiByteToWideChar( _loc_update.GetLocaleT()->locinfo->lc_codepage,
                              MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
                              s, 
                              (int)sizeInBytes, 
                              NULL, 
                              0 ) == 0 ) 
    {
#ifndef _WCE_BOOTCRT
        /* bad MB char */
        errno = EILSEQ;
#endif
        return (size_t)-1;
    }

    /* count MB chars */
    /* Note that sizeInBytes here is the number of bytes, not mb characters! */
    for (n = 0, size = 0; size < sizeInBytes && *s; n++, s++, size++)
    {
        if ( _isleadbyte_l((unsigned char)*s, _loc_update.GetLocaleT()) )
        {
            size++;
            if (size >= sizeInBytes)
            {
                break;
            }
            if (*++s == '\0')
            {
                break;
            }
        }
    }

#else

    {
        char *s1 = (char *)s;


        while (RtlAnsiCharToUnicodeChar( &s1 ) != UNICODE_NULL)
            ;

        n = s1 - s;
    }

#endif

    return (size >= sizeInBytes ? sizeInBytes : n);
}

size_t __cdecl _mbstrnlen(
        const char *s,
        size_t maxsize
        )
{
    return _mbstrnlen_l(s, maxsize, NULL);
}
