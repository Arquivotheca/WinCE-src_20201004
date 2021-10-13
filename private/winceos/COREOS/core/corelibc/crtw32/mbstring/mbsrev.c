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
*mbsrev.c - Reverse a string in place (MBCS)
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Reverse a string in place (MBCS)
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       10-05-93  GJF   Replaced _CRTAPI1 with __cdecl.
*       04-15-93  CFW   Add _MB_CP_LOCK.
*       05-09-94  CFW   Optimize for SBCS.
*       05-19-94  CFW   Enable non-Win32.
*       09-11-97  GJF   Replaced __mbcodepage == 0 with _ISNOTMBCP.
*       04-17-98  GJF   Revised multithread support based on threadmbcinfo
*                       structs
*       05-09-02  MSL   Fixed remove dud leadbyte at end of string before
*                       it gets reversed to beginning and corrupts string
*                       VS7 340546
*       10-06-03  AC    Added validation.
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
* _mbsrev - Reverse a string in place (MBCS)
*
*Purpose:
*       Reverses the order of characters in the string.  The terminating
*       null character remains in place.  The order of MBCS characters
*       is not changed.
*
*Entry:
*       unsigned char *string = string to reverse
*
*Exit:
*       returns string - now with reversed characters
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

extern "C" unsigned char * __cdecl _mbsrev_l(
        unsigned char *string,
        _locale_t plocinfo
        )
{
        unsigned char *start = string;
        unsigned char *left  = string;
        unsigned char c;
        _LocaleUpdate _loc_update(plocinfo);

        /* validation section */
        _VALIDATE_RETURN(string != NULL, EINVAL, NULL);

        if (_loc_update.GetLocaleT()->mbcinfo->ismbcodepage == 0)
            return (unsigned char *)_strrev((char *)string);


        /* first go through and reverse the bytes in MBCS chars */
        while ( *string ) {
            if ( _ismbblead_l(*string++, _loc_update.GetLocaleT()) ) {
                if ( *string ) {
                    c = *string;
                    *string = *(string - 1);
                    *(string - 1) = c;
                    string++;
                }
                else
                {
                    /*  second byte is EOS
                        There is nothing really satisfying to do here. We have a string
                        that ends in leadbyte,'\0'. Reversing this would lead to the leadbyte
                        becoming falsely attached to the character before it:
                        (XL0 -> LX0, X has suddenly become a trailbyte)

                        So what we choose to do is assert and purge the dud byte from within the
                        string. 
                    */
                    errno = EINVAL;
                    _ASSERTE(("Bad MBCS string passed into _mbsrev",0));
                    
                    /* String has at least moved once already, so this is safe */
                    _ASSERTE(string>start);

                    /* move back one to point at the dud leadbyte */
                    --string;

                    /* now truncate the string one byte earlier */
                    *string='\0';

                    break;
                }
            }
        }

        /* now reverse the whole string */
        string--;
        while ( left < string ) {
            c = *left;
            *left++ = *string;
            *string-- = c;
        }

        return ( start );
}

extern "C" unsigned char * (__cdecl _mbsrev)(
        unsigned char *string
        )
{
    return _mbsrev_l(string, NULL);
}

#endif  /* _MBCS */
