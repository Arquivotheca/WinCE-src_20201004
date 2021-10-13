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

//
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <stdlib.h>

#pragma warning(disable:4163)
#pragma function(wcscpy)

/***
*wchar_t *wcscpy(dst, src) - copy one wchar_t string over another
*
*Purpose:
*   Copies the wchar_t string src into the spot specified by
*   dest; assumes enough room.
*
*Entry:
*   wchar_t * dst - wchar_t string over which "src" is to be copied
*   const wchar_t * src - wchar_t string to be copied over "dst"
*
*Exit:
*   The address of "dst"
*
*Exceptions:
*******************************************************************************/

wchar_t * wcscpy(wchar_t *dst, const wchar_t *src) {
    wchar_t * cp = dst;
    while((*cp++ = *src++) != 0)
        ;       /* Copy src over dst */
    return( dst );
}

