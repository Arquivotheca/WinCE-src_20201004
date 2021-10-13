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
*wcsupr.c - routine to map lower-case characters in a wchar_t string
*       to upper-case
*
*
*Purpose:
*       Converts all the lower case characters in a wchar_t string
*       to upper case, in place.
*
*******************************************************************************/

#include <cruntime.h>
#include <string.h>
#include <internal.h>
#include <internal_securecrt.h>

/***
*wchar_t *_wcsupr(string) - map lower-case characters in a string to upper-case
*
*Purpose:
*       wcsupr converts lower-case characters in a null-terminated wchar_t
*       string to their upper-case equivalents.  The result may be longer or
*       shorter than the original string.  Assumes enough space in string
*       to hold the result.
*
*Entry:
*       wchar_t *wsrc - wchar_t string to change to upper case
*
*Exit:
*       input string address
*
*Exceptions:
*       on an error, the original string is unaltered, and errno is set
*
*******************************************************************************/

wchar_t * __cdecl _wcsupr (
    wchar_t * wsrc
    )
{
    /* validation section */
    _VALIDATE_RETURN(wsrc != NULL, EINVAL, NULL);

    return CharUpperW(wsrc);
}


/***
*errno_t _wcsupr_s(string, size_t) - map lower-case characters in a string to upper-case
*
*Purpose:
*       _wcsupr_s converts lower-case characters in a null-terminated wchar_t 
*       string to their upper-case equivalents.  The result may be longer or
*       shorter than the original string.
*
*Entry:
*       wchar_t *wsrc - wchar_t string to change to upper case
*       size_t sizeInWords - size of the destination buffer
*
*Exit:
*       the error code
*
*Exceptions:
*       on an error, the original string is unaltered
*
*******************************************************************************/

errno_t __cdecl _wcsupr_s (
        wchar_t * wsrc,
        size_t sizeInWords
        )
{
    size_t stringlen;

    /* validation section */
    _VALIDATE_RETURN_ERRCODE(wsrc != NULL, EINVAL);
    stringlen = wcsnlen(wsrc, sizeInWords);
    if (stringlen >= sizeInWords)
    {
        _RESET_STRING(wsrc, sizeInWords);
        _RETURN_DEST_NOT_NULL_TERMINATED(wsrc, sizeInWords);
    }
    _FILL_STRING(wsrc, sizeInWords, stringlen + 1);

    CharUpperW(wsrc);

    return 0;
}

