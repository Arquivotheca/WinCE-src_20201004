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
*strlen.c - contains strlen() routine
*
*Purpose:
*   strlen returns the length of a null-terminated string,
*   not including the null byte itself.
*
*******************************************************************************/

#include <corecrt.h>
#include <string.h>

#ifdef  _MSC_VER
#pragma function(strlen)
#endif

/***
*strlen - return the length of a null-terminated string
*
*Purpose:
*   Finds the length in bytes of the given string, not including
*   the final null character.
*
*Entry:
*   const char * str - string whose length is to be computed
*
*Exit:
*   length of the string "str", exclusive of the final null byte
*
*Exceptions:
*
*******************************************************************************/

size_t __cdecl strlen (
    const char * str
    )
{
    const unsigned char *eos = (const unsigned char*)str;

    while( *eos++ ) ;

    return( (int)(eos - str - 1) );
}
