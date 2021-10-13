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
*mbsnset.c - Sets first n charcaters of string to given character (MBCS)
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Sets first n charcaters of string to given character (MBCS)
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       08-20-93  CFW   Change short params to int for 32-bit tree.
*       10-05-93  GJF   Replaced _CRTAPI1 with __cdecl.
*       04-15-93  CFW   Add _MB_CP_LOCK.
*       05-09-94  CFW   Optimize for SBCS.
*       05-19-94  CFW   Enable non-Win32.
*       09-11-97  GJF   Replaced __mbcodepage == 0 with _ISNOTMBCP.
*       04-17-98  GJF   Revised multithread support based on threadmbcinfo
*                       structs
*       05-09-02  MSL   Fixed to validate val to ensure it is not an invalid
*                       MBCS pair (leadbyte, 0) VS7 340545
*       10-06-03  AC    Added validation.
*       03-23-05  MSL   Review comment cleanup
*
*******************************************************************************/

#ifdef  _MBCS

#include <mtdll.h>
#include <cruntime.h>
#include <string.h>
#include <mbdata.h>
#include <mbctype.h>
#include <mbstring.h>
#include <crtdbg.h>
#include <internal.h>
#include <locale.h>
#include <setlocal.h>


/***
* _mbsnset - Sets first n charcaters of string to given character (MBCS)
*
*Purpose:
*       Sets the first n characters of string to the supplied
*       character value.  If the length of string is less than n,
*       the length of string is used in place of n.  Handles
*       MBCS chars correctly.
*
*       There are several factors that make this routine complicated:
*               (1) The fill value may be 1 or 2 bytes long.
*               (2) The fill operation may end by hitting the count value
*               or by hitting the end of the string.
*               (3) A null terminating char is NOT placed at the end of
*               the string.
*
*       Cases to be careful of (both of these can occur at once):
*               (1) Leaving an "orphaned" trail byte in the string (e.g.,
*               overwriting a lead byte but not the corresponding trail byte).
*               (2) Writing only the 1st byte of a 2-byte fill value because the
*               end of string was encountered.
*
*Entry:
*       unsigned char *string = string to modify
*       unsigned int val = value to fill string with
*       size_t count = count of characters to fill
*
*
*Exit:
*       Returns string = now filled with char val
*
*Uses:
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" unsigned char * __cdecl _mbsnset_l(
        unsigned char *string,
        unsigned int val,
        size_t count,
        _locale_t plocinfo
        )
{
        unsigned char  *start = string;
        unsigned int leadbyte = 0;
        unsigned char highval, lowval;
        _LocaleUpdate _loc_update(plocinfo);

        /* validation section */
        _VALIDATE_RETURN(string != NULL || count == 0, EINVAL, NULL);

        /*
         * leadbyte flag indicates if the last byte we overwrote was
         * a lead byte or not.
         */
_BEGIN_SECURE_CRT_DEPRECATION_DISABLE
        if (_loc_update.GetLocaleT()->mbcinfo->ismbcodepage == 0)
            return (unsigned char *)_strnset((char *)string, val, count);
_END_SECURE_CRT_DEPRECATION_DISABLE

        if (highval = (unsigned char)(val>>8)) {

            /* double byte value */

            lowval = (unsigned char)(val & 0x00ff);

            if(lowval=='\0')
            {
                _ASSERTE(("invalid MBCS pair passed to mbsnset",0));

                /* Ideally we would return NULL here and signal an error 
                    condition. But since this function has no other
                    error modes, there would be a good chance of crashing
                    the caller. So instead we fill the string with spaces
                    to ensure that no information leaks through
                    unexpectedly. Anyway, we do set errno to EINVAL.
                */
                errno = EINVAL;
                lowval=highval=' ';
            }

            while (count-- && *string) {
                leadbyte = _ismbbtruelead_l(leadbyte, *string, _loc_update.GetLocaleT());
                *string++ = highval;

                if (*string) {
                    leadbyte = _ismbbtruelead_l(leadbyte, *string, _loc_update.GetLocaleT());
                    *string++ = lowval;
                }
                else
                    /* overwrite orphaned highval byte */
                    *(string-1) = ' ';
            }
        }

        else {
            /* single byte value */

            while (count-- && *string) {
                leadbyte = _ismbbtruelead_l(leadbyte, *string, _loc_update.GetLocaleT());
                *string++ = (unsigned char)val;
            }
        }

        /* overwrite orphaned trailing byte, if necessary */
        if(leadbyte && *string)
            *string = ' ';

        return( start );
}

unsigned char * (__cdecl _mbsnset)(
        unsigned char *string,
        unsigned int val,
        size_t count
        )
{
_BEGIN_SECURE_CRT_DEPRECATION_DISABLE
    return _mbsnset_l(string, val, count, NULL);
_END_SECURE_CRT_DEPRECATION_DISABLE
}

#endif  /* _MBCS */
