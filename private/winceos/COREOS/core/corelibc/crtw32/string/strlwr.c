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
*strlwr.c - routine to map upper-case characters in a string to lower-case
*
*
*Purpose:
*       Converts all the upper case characters in a string to lower case,
*       in place.
*
*******************************************************************************/

#include <cruntime.h>
#include <string.h>
#include <internal.h>
#include <internal_securecrt.h>

/***
*char *_strlwr(string) - map upper-case characters in a string to lower-case
*
*Purpose:
*       _strlwr() converts upper-case characters in a null-terminated string
*       to their lower-case equivalents.  Conversion is done in place and
*       characters other than upper-case letters are not modified.
*
*       In the C locale, this function modifies only 7-bit ASCII characters
*       in the range 0x41 through 0x5A ('A' through 'Z').
*
*Entry:
*       char *string - string to change to lower case
*
*Exit:
*       input string address
*
*Exceptions:
*       The original string is returned unchanged on any error.
*
*******************************************************************************/
char * _strlwr(char *pstr) {
    /* Performance note:  use unsigned char on RISC machines to avoid sign extension in 4-byte regs. */
    unsigned char *pTrav;
    for (pTrav = (unsigned char *)pstr; *pTrav; pTrav++)
        *pTrav = (unsigned char)_tolower(*pTrav);
    return pstr;
}

/***
*errno_t _strlwr_s(string, size_t) - map upper-case characters in a string to lower-case
*
*Purpose:
*       _strlwr_s() converts upper-case characters in a null-terminated string
*       to their lower-case equivalents.  Conversion is done in place and
*       characters other than upper-case letters are not modified.
*
*       In the C locale, this function modifies only 7-bit ASCII characters
*       in the range 0x41 through 0x5A ('A' through 'Z').
*
*Entry:
*       char *string - string to change to lower case
*       size_t sizeInBytes - size of the destination buffer
*
*Exit:
*       the error code
*
*Exceptions:
*       The original string is returned unchanged on any error.
*
*******************************************************************************/

errno_t __cdecl _strlwr_s (
        char * string,
        size_t sizeInBytes
        )
{
    size_t stringlen;
    unsigned char *pTrav;

    /* validation section */
    _VALIDATE_RETURN_ERRCODE(string != NULL, EINVAL);
    stringlen = strnlen(string, sizeInBytes);
    if (stringlen >= sizeInBytes)
    {
        _RESET_STRING(string, sizeInBytes);
        _RETURN_DEST_NOT_NULL_TERMINATED(string, sizeInBytes);
    }
    _FILL_STRING(string, sizeInBytes, stringlen + 1);

    for (pTrav = (unsigned char *)string; *pTrav; pTrav++)
        *pTrav = (unsigned char)_tolower(*pTrav);

    return 0;
}

