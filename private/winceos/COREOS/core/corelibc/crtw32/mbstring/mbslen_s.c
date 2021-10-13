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
*mbslen_s.c - Find length of MBCS string
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Find length of MBCS string
*
*Revision History:
*       10-08-03  AC    Module created.
*       03-08-04  AC    Change the name to _mbsnlen.
*       03-26-04  AC    Check the string in the first sizeInBytes bytes (and not chars)
*
*******************************************************************************/

#ifdef  _MBCS

#include <mtdll.h>
#include <cruntime.h>
#include <string.h>
#include <mbdata.h>
#include <mbctype.h>
#include <mbstring.h>
#include <internal.h>
#include <locale.h>
#include <setlocal.h>

/***
* _mbsnlen - Find length of MBCS string
*
*Purpose:
*       Find the length of the MBCS string (in characters).
*
*Entry:
*       unsigned char *s = string
*       size_t maxsize
*
*Exit:
*       Returns the number of MBCS chars in the string.
*       Only the first sizeInBytes bytes of the string are inspected: if the null
*       terminator is not found, sizeInBytes is returned.
*       If the string is null terminated in sizeInBytes bytes, the return value
*       will always be less than sizeInBytes.
*       Returns (size_t)-1 if something went wrong.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

size_t __cdecl _mbsnlen_l(
        const unsigned char *s,
        size_t sizeInBytes,
        _locale_t plocinfo
        )
{
        size_t n, size;
        _LocaleUpdate _loc_update(plocinfo);

        if (_loc_update.GetLocaleT()->mbcinfo->ismbcodepage == 0)
            return strnlen((const char *)s, sizeInBytes);

        /* Note that we do not check if s == NULL, because we do not
         * return errno_t...
         */

        /* Note that sizeInBytes here is the number of bytes, not mb characters! */
        for (n = 0, size = 0; size < sizeInBytes && *s; n++, s++, size++)
        {
            if ( _ismbblead_l(*s, _loc_update.GetLocaleT()) )
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

		return (size >= sizeInBytes ? sizeInBytes : n);
}

size_t __cdecl _mbsnlen(
        const unsigned char *s,
        size_t maxsize
        )
{
    return _mbsnlen_l(s,maxsize, NULL);
}

#endif  /* _MBCS */
