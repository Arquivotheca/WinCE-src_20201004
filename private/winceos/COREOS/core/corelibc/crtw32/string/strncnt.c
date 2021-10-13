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
*strncnt.c - contains __strncnt() routine
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	__strncnt returns the count characters in a string, up to n. 
*   (used by _strncnt)
*
*Revision History:
*	10-22-02  PK	Created from strncnt.c.
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <string.h>

/***
*size_t __cdecl __strncnt - count characters in a string, up to n.
*
*Purpose:
*       Internal local support function. Counts characters in string before NULL.
*       If NULL not found in n chars, then return n.
*
*Entry:
*       const char *string   - start of string
*       int n                - byte count
*
*Exit:
*       returns number of bytes from start of string to
*       NULL (exclusive), up to n.
*
*Exceptions:
*
*******************************************************************************/

size_t __cdecl __strncnt (
        const char *string,
        size_t cnt
        )
{
        size_t n = cnt;
        char *cp = (char *)string;

        while (n-- && *cp)
            cp++;

        return cnt - n - 1;
}

#endif /* _POSIX_ */
