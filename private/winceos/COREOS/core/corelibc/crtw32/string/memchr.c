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

#if !defined(_M_IX86) && !defined(_M_ARM)

#include <string.h>

#pragma warning(disable:4163)
#pragma function(memchr)

/****************************************************************************
memchr

The memchr function looks for the first occurance of cCharacter in the first
iLength bytes of pBuffer and stops after finding cCharacter or after checking
iLength bytes.

Arguments:
    pBuffer: pointer to buffer
    cCharacter: character to look for
    iLength: number of characters

Returns:

If successful, memchr returns a pointer to the first location of cCharacter
in pBuffer.  Otherwise, it returns NULL.

************************************************************MikeMon*********/
void * memchr(const void *pBuffer, int cCharacter, size_t iLength) {
    while ((iLength != 0) && (*((char *)pBuffer) != (char)cCharacter)) {
        ((char *) pBuffer) += 1;
        iLength -= 1;
    }
    return((iLength != 0) ? (void *)pBuffer : (void *)0);
}

#endif

