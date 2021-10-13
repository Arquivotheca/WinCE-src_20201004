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
*strupr.c - routine to map lower-case characters in a string to upper-case
*
*
*Purpose:
*       Converts all the lower case characters in a string to upper case,
*       in place.
*
*******************************************************************************/

#include <cruntime.h>
#include <string.h>
#include <internal.h>
#include <internal_securecrt.h>

/***
*char *_strupr(string) - map lower-case characters in a string to upper-case
*
*Purpose:
*       _strupr() converts lower-case characters in a null-terminated string
*       to their upper-case equivalents.  Conversion is done in place and
*       characters other than lower-case letters are not modified.
*
*       In the C locale, this function modifies only 7-bit ASCII characters
*       in the range 0x61 through 0x7A ('a' through 'z').
*
*Entry:
*       char *string - string to change to upper case
*
*Exit:
*       input string address
*
*Exceptions:
*       The original string is returned unchanged on any error.
*
*******************************************************************************/
char * _strupr(char *pstr) {
    /* Performance note:  use unsigned char on RISC machines to avoid sign extension in 4-byte regs. */
    unsigned char *pTrav;
    for (pTrav = (unsigned char *)pstr; *pTrav; pTrav++)
        *pTrav = (unsigned char)_toupper(*pTrav);
    return pstr;
}

/***
*errno_t _strupr_s(string, size_t) - map lower-case characters in a string to upper-case
*
*Purpose:
*       _strupr_s() converts lower-case characters in a null-terminated string
*       to their upper-case equivalents.  Conversion is done in place and
*       characters other than lower-case letters are not modified.
*
*       In the C locale, this function modifies only 7-bit ASCII characters
*       in the range 0x61 through 0x7A ('a' through 'z').
*
*Entry:
*       char *string - string to change to upper case
*       size_t sizeInBytes - size of the destination buffer
*
*Exit:
*       the error code
*
*Exceptions:
*       The original string is returned unchanged on any error.
*
*******************************************************************************/

errno_t __cdecl _strupr_s (
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
        *pTrav = (unsigned char)_toupper(*pTrav);

    return 0;
}

