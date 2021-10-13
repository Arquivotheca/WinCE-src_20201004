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
*swab.c - block copy, while swapping even/odd bytes
*
*   Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*   This module contains the routine _swab() which swaps the odd/even
*   bytes of words during a block copy.
*
*Revision History:
*   06-02-89  PHG   module created, based on asm version
*   03-06-90  GJF   Fixed calling type, added #include <cruntime.h> and
*           fixed copyright. Also, cleaned up the formatting a
*           bit.
*   09-27-90  GJF   New-style function declarators.
*   01-21-91  GJF   ANSI naming.
*   04-06-93  SKS   Replace _CRTAPI* with _cdecl
*       03-08-04  MSL   Added validation to _swab
*           VSW# 228709
*
*******************************************************************************/

#include <cruntime.h>
#include <stdlib.h>
#include <internal.h>

/***
*void _swab(srcptr, dstptr, nbytes) - swap ODD/EVEN bytes during word move
*
*Purpose:
*   This routine copys a block of words and swaps the odd and even
*   bytes.  nbytes must be > 0, otherwise nothing is copied.  If
*   nbytes is odd, then only (nbytes-1) bytes are copied.
*
*Entry:
*   srcptr = pointer to the source block
*   dstptr = pointer to the destination block
*   nbytes = number of bytes to swap
*
*Returns:
*   None.
*
*Exceptions:
*
*******************************************************************************/

void __cdecl _swab 
(
    char *src,
    char *dest,
    int nbytes
)
{
    char b1='\0';
    char b2='\0';

    _VALIDATE_RETURN_VOID(src!=NULL, EINVAL);
    _VALIDATE_RETURN_VOID(dest!=NULL, EINVAL);
    _VALIDATE_RETURN_VOID(nbytes>=0, EINVAL);

    while (nbytes > 1) {
        b1 = *src++;
        b2 = *src++;
        *dest++ = b2;
        *dest++ = b1;
        nbytes -= 2;
    }
}
