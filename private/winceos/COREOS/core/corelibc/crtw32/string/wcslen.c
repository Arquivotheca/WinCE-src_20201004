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

#include <stdlib.h>

#pragma warning(disable:4163)
#pragma function(wcslen)

/***
*wcslen - return the length of a null-terminated wide-character string
*
*Purpose:
*   Finds the length in wchar_t's of the given string, not including
*   the final null wchar_t (wide-characters).
*
*Entry:
*   const wchar_t * wcs - string whose length is to be computed
*
*Exit:
*   length of the string "wcs", exclusive of the final null wchar_t
*
*Exceptions:
*
*******************************************************************************/

size_t wcslen (const wchar_t * wcs) {
    const wchar_t *eos = wcs;
    while( *eos++ )
        ;
    return( (size_t)(eos - wcs - 1) );
}

