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
*mbccpy.c - Copy one character  to another (MBCS)
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Copy one MBCS character to another (1 or 2 bytes)
*
*Revision History:
*       04-12-93  KRS   Created.
*       06-03-93  KRS   Change return type to void.
*       10-05-93  GJF   Replace _CRTAPI1 with __cdecl.
*       04-28-98  GJF   No more _ISLEADBYTE macro.
*       05-08-02  MSL   Fix MBCS Leadbyte, EOS issue VS7 340533
*       10-02-03  AC    Added secure version
*       03-10-04  AC    Return ERANGE when buffer is too small
*       08-04-04  AC    Moved secure version to mbccpy_s.inl and mbccpy_s.c
*
*******************************************************************************/

#include <mtdll.h>
#include <cruntime.h>
#include <mbdata.h>
#include <mbctype.h>
#include <mbstring.h>
#include <memory.h>
#include <crtdefs.h>
#include <internal.h>
#include <locale.h>
#include <setlocal.h>

/***
* _mbccpy - Copy one character to another (MBCS)
*
*Purpose:
*       Copies exactly one MBCS character from src to dst.  Copies _mbclen(src)
*       bytes from src to dst.
*
*Entry:
*       unsigned char *dst = destination for copy
*       unsigned char *src = source for copy
*
*Exit:
*
*Exceptions:
*       Input parameters are validated. Refer to the validation section of the function. 
*
*******************************************************************************/

extern "C" void __cdecl _mbccpy_l(
        unsigned char *dst,
        const unsigned char *src,
        _locale_t plocinfo
        )
{
    /* _mbccpy_s_l sets errno */
    _mbccpy_s_l(dst, 2, NULL, src, plocinfo);
}

extern "C" void (__cdecl _mbccpy)(
        unsigned char *dst,
        const unsigned char *src
        )
{
    _mbccpy_s_l(dst, 2, NULL, src, NULL);
}
