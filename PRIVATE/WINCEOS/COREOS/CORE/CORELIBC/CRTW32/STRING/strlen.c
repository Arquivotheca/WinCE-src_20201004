//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
