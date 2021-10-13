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
*mbsinc.c - Move MBCS string pointer ahead one charcter.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Move MBCS string pointer ahead one character.
*
*Revision History:
*       11-19-92  KRS   Ported from 16-bit sources.
*       08-03-93  KRS   Fix prototypes.
*       08-20-93  CFW   Remove test for NULL string, use new function parameters.
*       10-05-93  GJF   Replaced _CRTAPI1 with __cdecl.
*       04-28-98  GJF   No more _ISLEADBYTE macro.
*       05-08-02  MSL   Fixed leadbyte followed by EOS VS7 340535
*       10-02-03  AC    Added validation.
*
*******************************************************************************/

#ifdef  _MBCS

#include <mtdll.h>
#include <cruntime.h>
#include <mbdata.h>
#include <mbstring.h>
#include <mbctype.h>
#include <stddef.h>
#include <internal.h>

/*** 
*_mbsinc - Move MBCS string pointer ahead one charcter.
*
*Purpose:
*       Move the supplied string pointer ahead by one
*       character.  MBCS characters are handled correctly.
*
*Entry:
*       const unsigned char *current = current char pointer (legal MBCS boundary)
*
*Exit:
*       Returns pointer after moving it.
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function.
*
*******************************************************************************/

unsigned char * __cdecl _mbsinc_l(
        const unsigned char *current,
        _locale_t plocinfo
        )
{
        if ( (_ismbblead_l)(*(current++),plocinfo))
        {
            /* don't move forward two if we get leadbyte, EOS
               also don't assert here as we are too low level
            */
            if(*current!='\0')
            {
                current++;
            }
        }

        return (unsigned char *)current;
}

unsigned char * (__cdecl _mbsinc)(
        const unsigned char *current
        )
{
        /* validation section */
        _VALIDATE_RETURN(current != NULL, EINVAL, NULL);

        if ( _ismbblead(*(current++)))
        {
            /* don't move forward two if we get leadbyte, EOS
               also don't assert here as we are too low level
            */
            if(*current!='\0')
            {
                current++;
            }
        }

        return (unsigned char *)current;
}

#endif  /* _MBCS */
