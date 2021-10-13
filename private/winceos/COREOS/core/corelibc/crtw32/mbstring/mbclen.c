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
*mbclen.c - Find length of MBCS character
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Find length of MBCS character
*
*Revision History:
*       04-12-93  KRS   Created.
*       10-05-93  GJF   Replace _CRTAPI1 with __cdecl.
*       04-28-98  GJF   No more _ISLEADBYTE macro.
*       05-08-02  MSL   Fix MBCS Leadbyte, EOS issue
*
*******************************************************************************/

#include <mtdll.h>
#include <cruntime.h>
#include <mbdata.h>
#include <mbctype.h>
#include <mbstring.h>
#include <stddef.h>


/*** 
* _mbclen - Find length of MBCS character
*
*Purpose:
*       Find the length of the MBCS character (in bytes).
*
*Entry:
*       unsigned char *c = MBCS character
*
*Exit:
*       Returns the number of bytes in the MBCS character
*
*Exceptions:
*
*******************************************************************************/

size_t __cdecl _mbclen_l(
        const unsigned char *c,
        _locale_t plocinfo
        )

{
        /*  Don't return two if we have leadbyte, EOS. 
            Don't assert here; too low level 
        */
        return ((_ismbblead_l)(*c, plocinfo) && c[1]!='\0')  ? 2 : 1;
}

size_t (__cdecl _mbclen)(
        const unsigned char *c
        )

{
        /*  Don't return two if we have leadbyte, EOS. 
            Don't assert here; too low level 
        */
        return (_ismbblead(*c) && c[1]!='\0')  ? 2 : 1;
}
